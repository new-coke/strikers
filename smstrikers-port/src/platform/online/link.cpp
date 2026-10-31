// Links between machines: handshake, sealed datagrams, reliable messages, discovery, the poll.

#include "session.h"
#include "stun.h"

#include <cstdio>
#include <cstring>

namespace online
{
namespace
{
const uint64_t kHelloEveryUs = 250000;
const uint64_t kJoinGiveUpUs = 5000000;
const uint64_t kPendingExpiryUs = 10000000;
const uint64_t kLinkDeadUs = 6000000;
const uint64_t kIdleSendUs = 100000;
const uint64_t kMatchSendUs = ONLINE_TICK_MS * 1000;
const uint64_t kReliableSoonUs = 5000;
const uint64_t kMinResendUs = 50000;
const uint64_t kMaxWaitUs = 20000;
const uint64_t kNearbyExpiryUs = 5000000;
const size_t kMaxOutstanding = 32;
const size_t kMaxQueued = 256;
const int kMaxPendingHandshakes = 8;
const int kMaxHellosPerSecond = 20;
const int kMaxAnnouncesPerSecond = 20;
const int kMalformedLimit = 50;
const uint64_t kStunResolveGiveUpUs = 5000000;
const uint64_t kStunEveryUs = 500000;
const int kStunSends = 6;

struct StunServer
{
    const char* host;
    uint16_t port;
};
const StunServer kStunServers[] = { { "stun.cloudflare.com", 3478 },
                                    { "stun.l.google.com", 19302 } };
const int kStunServerCount = (int)(sizeof kStunServers / sizeof kStunServers[0]);

// Order of relSeq within the reliable window, modulo 2^16.
bool seq_before(uint16_t a, uint16_t b) { return (int16_t)(uint16_t)(a - b) < 0; }

uint32_t low32(uint64_t us) { return (uint32_t)us; }
}   // namespace

Session::Link* Session::NewLink(uint64_t now)
{
    Link* l = NULL;
    for (size_t i = 0; i < m_links.size(); i++)
        if (!m_links[i]->used)
        {
            l = m_links[i];
            break;
        }
    if (l == NULL)
    {
        l = new Link;
        m_links.push_back(l);
    }
    l->out.clear();
    l->used = true;
    l->authed = false;
    l->closing = false;
    l->stuck = false;
    l->closeAtUs = 0;
    l->machine = -1;
    std::memset(&l->addr, 0, sizeof l->addr);
    l->home = l->addr;
    l->awayRecv = 0;
    l->awaySent = 0;
    l->linkId = 0;
    std::memset(l->nonce, 0, sizeof l->nonce);
    std::memset(l->guestPublic, 0, sizeof l->guestPublic);
    std::memset(l->hostPublic, 0, sizeof l->hostPublic);
    std::memset(l->sendKey, 0, sizeof l->sendKey);
    std::memset(l->recvKey, 0, sizeof l->recvKey);
    l->sendSeq = 0;
    l->replay = ReplayWindow();
    l->stats.Reset();
    l->createdUs = now;
    l->lastHeardUs = l->createdUs;
    l->lastSentUs = 0;
    l->echoValid = false;
    l->echoUs = 0;
    l->echoAtUs = 0;
    l->timingValid = false;
    l->peerTick = 0;
    l->peerConfirmed = 0;
    l->timingAtUs = 0;
    l->sendRelSeq = 1;
    l->recvNext = 1;
    for (int i = 0; i < kRelWindow; i++)
    {
        l->ahead[i].has = false;
        l->ahead[i].seq = 0;
        l->ahead[i].atUs = 0;
    }
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        l->peerHas[m] = 0;
    l->malformed = 0;
    l->componentsLeft = 0;
    l->matchGapUs = 0;
    l->sendNow = true;
    l->byeReason = -1;
    return l;
}

Session::Link* Session::FindLink(uint32_t linkId)
{
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used && m_links[i]->linkId == linkId)
            return m_links[i];
    return NULL;
}

Session::Link* Session::MachineLink(int machine)
{
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used && m_links[i]->authed && m_links[i]->machine == machine &&
            !m_links[i]->closing)
            return m_links[i];
    return NULL;
}

void Session::CloseLink(Link& link, bool sayBye, uint8_t reason)
{
    if (!link.used)
        return;
    if (sayBye && link.authed && m_game != NULL)
    {
        Farewell f;
        f.pipe = m_game;
        f.addr = link.addr;
        f.linkId = link.linkId;
        std::memcpy(f.key, link.sendKey, sizeof f.key);
        f.seq = link.sendSeq;
        f.reason = reason;
        f.left = 3;
        f.nextUs = m_env->NowUs();
        m_farewells.push_back(f);
    }
    Wipe(link.sendKey, sizeof link.sendKey);
    Wipe(link.recvKey, sizeof link.recvKey);
    link.used = false;
    link.out.clear();
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].link >= 0 && m_room[m].link < (int)m_links.size() &&
            m_links[m_room[m].link] == &link)
            m_room[m].link = -1;
}

void Session::ClosePipe(NetPipe*& pipe)
{
    if (pipe == NULL)
        return;
    // The network thread may be waiting on it; it is deleted by the next Poll.
    m_closedPipes.push_back(pipe);
    pipe = NULL;
}

void Session::SendPlain(NetPipe* pipe, const NetAddr& to, const uint8_t* data, size_t size)
{
    if (pipe != NULL)
        pipe->Send(to, data, size);
}

int Session::Pipes(NetPipe** out, int max) const
{
    int n = 0;
    NetPipe* all[3] = { m_game, m_discovery, m_search };
    for (int i = 0; i < 3 && n < max; i++)
        if (all[i] != NULL)
            out[n++] = all[i];
    return n;
}

void Session::Receive(uint64_t now)
{
    uint8_t buf[kMaxDatagram + 1];
    NetPipe* pipes[3] = { m_game, m_discovery, m_search };
    for (int p = 0; p < 3; p++)
    {
        // A handler may close pipes; stop reading one that is gone.
        for (int budget = 0; budget < 256; budget++)
        {
            NetPipe* pipe = pipes[p];
            if (pipe == NULL || (pipe != m_game && pipe != m_discovery && pipe != m_search))
                break;
            NetAddr from;
            const int n = pipe->Receive(&from, buf, sizeof buf);
            if (n < 0)
                break;
            if (n == 0 || (size_t)n > kMaxDatagram)
                continue;
            OnDatagram(pipe, from, buf, (size_t)n, now);
        }
    }
}

void Session::OnDatagram(NetPipe* pipe, const NetAddr& from, const uint8_t* data, size_t size,
                         uint64_t now)
{
    // A STUN reply comes only from the server's address and is never a session datagram.
    if (pipe == m_game && m_public.stunAddr.family != 0 && AddrEqual(from, m_public.stunAddr))
    {
        NetAddr mapped;
        if (m_public.mapped.family == 0 && DecodeStunResponse(data, size, m_public.txid, &mapped))
        {
            m_public.mapped = mapped;
            // Shown before the router answers; a shared or failed result clears it.
            if (!AddrIsLanSource(mapped))
                std::snprintf(m_public.address, sizeof m_public.address, "%u.%u.%u.%u:%u",
                              mapped.ip[0], mapped.ip[1], mapped.ip[2], mapped.ip[3],
                              (unsigned)m_roomPort);
            SettlePublic();
        }
        return;
    }
    if (data[0] == DG_SEALED)
    {
        if (pipe == m_game)
            OnSealed(from, data, size, now);
        return;
    }
    uint16_t protocol = 0;
    switch (PlainClass(data, size, &protocol))
    {
    case DG_DISCOVER:
    {
        Discover d;
        if (pipe == m_discovery && DecodeDiscover(data, size, &d))
            OnDiscover(pipe, from, d, now);
        return;
    }
    case DG_ANNOUNCE:
    {
        Announce a;
        if (pipe == m_search && DecodeAnnounce(data, size, &a))
            OnAnnounce(from, a, now);
        return;
    }
    case DG_HELLO:
        if (pipe == m_game)
            OnHello(from, data, size, now);
        return;
    case DG_WELCOME:
    {
        Welcome w;
        if (pipe == m_game && DecodeWelcome(data, size, &w))
            OnWelcome(from, w, now);
        return;
    }
    case DG_REFUSE:
    {
        Refuse r;
        if (pipe == m_game && DecodeRefuse(data, size, &r))
            OnRefuse(r);
        return;
    }
    default: return;
    }
}

void Session::OnHello(const NetAddr& from, const uint8_t* data, size_t size, uint64_t now)
{
    if (!IsHost())
        return;
    if (Since(now, m_helloWindowUs) >= 1000000)
    {
        m_helloWindowUs = now;
        m_helloCount = 0;
    }
    if (++m_helloCount > kMaxHellosPerSecond)
        return;

    uint16_t protocol = 0;
    PlainClass(data, size, &protocol);
    const uint8_t* nonce = data + 40;
    uint8_t refuse = 0;
    if (protocol != ONLINE_PROTOCOL_VERSION)
        refuse = ONLINE_ERR_VERSION;
    Hello h;
    if (refuse == 0 && !DecodeHello(data, size, &h))
        return;

    // A repeated HELLO gets the same WELCOME.
    for (size_t i = 0; i < m_links.size(); i++)
    {
        Link& l = *m_links[i];
        if (!l.used || !AddrEqual(l.addr, from) || std::memcmp(l.nonce, nonce, 16) != 0)
            continue;
        if (l.authed)
            return;
        Welcome w;
        std::memcpy(w.publicKey, l.hostPublic, 32);
        std::memcpy(w.nonce, l.nonce, 16);
        w.linkId = l.linkId;
        uint8_t out[kWelcomeSize];
        EncodeWelcome(w, out);
        SendPlain(m_game, from, out, sizeof out);
        return;
    }

    if (refuse == 0)
    {
        int machines = MachineCount();
        if (machines >= ONLINE_MAX_MACHINES || HumanCount() >= ONLINE_MAX_SLOTS)
            refuse = ONLINE_ERR_ROOM_FULL;
        else if ((m_phase != ONLINE_PHASE_ROOM && m_phase != ONLINE_PHASE_HOSTING) || m_starting)
            refuse = ONLINE_ERR_IN_MATCH;
    }
    if (refuse != 0)
    {
        Refuse r;
        r.protocol = ONLINE_PROTOCOL_VERSION;
        std::memcpy(r.nonce, nonce, 16);
        r.reason = refuse;
        uint8_t out[kRefuseSize];
        EncodeRefuse(r, out);
        SendPlain(m_game, from, out, sizeof out);
        return;
    }

    // Authenticated or not, a link that has not joined the room is still pending.
    int pending = 0;
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used && m_links[i]->machine < 0)
            pending++;
    if (pending >= kMaxPendingHandshakes)
        return;

    KeyPair key;
    if (!m_env->Random(key.secret, sizeof key.secret))
    {
        m_env->Log("no system randomness; a join was refused");
        return;
    }
    KeyPairFromSecret(&key);
    uint32_t linkId = 0;
    while (linkId == 0 || FindLink(linkId) != NULL)
        if (!m_env->Random(&linkId, sizeof linkId))
        {
            Wipe(key.secret, sizeof key.secret);
            return;
        }
    uint8_t g2h[32];
    uint8_t h2g[32];
    const bool ok = DeriveLinkKeys(key.secret, h.publicKey, h.publicKey, key.publicKey, h.nonce,
                                   linkId, g2h, h2g);
    Wipe(key.secret, sizeof key.secret);
    if (!ok)
        return;

    Link* l = NewLink(now);
    l->addr = from;
    l->home = from;
    l->linkId = linkId;
    std::memcpy(l->nonce, h.nonce, 16);
    std::memcpy(l->guestPublic, h.publicKey, 32);
    std::memcpy(l->hostPublic, key.publicKey, 32);
    std::memcpy(l->recvKey, g2h, 32);
    std::memcpy(l->sendKey, h2g, 32);
    Wipe(g2h, sizeof g2h);
    Wipe(h2g, sizeof h2g);
    l->sendNow = false;

    Welcome w;
    std::memcpy(w.publicKey, key.publicKey, 32);
    std::memcpy(w.nonce, h.nonce, 16);
    w.linkId = linkId;
    uint8_t out[kWelcomeSize];
    EncodeWelcome(w, out);
    SendPlain(m_game, from, out, sizeof out);
}

void Session::OnWelcome(const NetAddr& from, const Welcome& w, uint64_t now)
{
    // A host bound to every address may answer from another of its own; the nonce names the join.
    if (!m_join.active || m_join.stage != ONLINE_JOIN_CONTACTING ||
        std::memcmp(w.nonce, m_join.nonce, 16) != 0)
        return;
    uint8_t g2h[32];
    uint8_t h2g[32];
    if (!DeriveLinkKeys(m_join.key.secret, w.publicKey, m_join.key.publicKey, w.publicKey,
                        m_join.nonce, w.linkId, g2h, h2g))
        return;
    Wipe(m_join.key.secret, sizeof m_join.key.secret);
    Link* l = NewLink(now);
    l->authed = true;
    l->machine = 0;
    l->addr = from;
    l->home = from;
    l->linkId = w.linkId;
    std::memcpy(l->nonce, m_join.nonce, 16);
    std::memcpy(l->sendKey, g2h, 32);
    std::memcpy(l->recvKey, h2g, 32);
    Wipe(g2h, sizeof g2h);
    Wipe(h2g, sizeof h2g);
    l->lastHeardUs = now;
    m_join.stage = ONLINE_JOIN_HANDSHAKE;

    Message m;
    std::memset(&m, 0, sizeof m);
    MsgJoin& j = m.u.join;
    j.protocol = ONLINE_PROTOCOL_VERSION;
    j.platform = m_id.platform;
    j.binaryFamily = m_id.binaryFamily;
    j.build = m_id.build;
    j.dataFamily = m_id.dataFamily;
    std::memcpy(j.version, m_id.version, sizeof j.version);
    std::memcpy(j.nickname, m_nickname, sizeof j.nickname);
    int humans = 0;
    for (int i = 0; i < ONLINE_MAX_LOCAL_HUMANS; i++)
        if (m_local[i].used)
            std::memcpy(j.human[humans++], m_local[i].name, sizeof j.human[0]);
    j.humanCount = (uint8_t)humans;
    Queue(*l, MSG_JOIN, m, 0);
    SendDatagram(*l, now);
}

void Session::OnRefuse(const Refuse& r)
{
    if (!m_join.active || std::memcmp(r.nonce, m_join.nonce, 16) != 0)
        return;
    switch (r.reason)
    {
    case ONLINE_ERR_ROOM_FULL: Fail(ONLINE_ERR_ROOM_FULL, "That room is full."); break;
    case ONLINE_ERR_IN_MATCH:
        Fail(ONLINE_ERR_IN_MATCH, "That room is playing a match. Join after it ends.");
        break;
    default:
    {
        char text[ONLINE_ERROR_MAX];
        std::snprintf(text, sizeof text,
                      "That room speaks online protocol %u and this build speaks %u.",
                      (unsigned)r.protocol, (unsigned)ONLINE_PROTOCOL_VERSION);
        Fail(ONLINE_ERR_VERSION, text);
        break;
    }
    }
}

void Session::OnDiscover(NetPipe* pipe, const NetAddr& from, const Discover& d, uint64_t now)
{
    if (!IsHost() || !AddrIsLanSource(from))
        return;
    if (Since(now, m_discoverWindowUs) >= 1000000)
    {
        m_discoverWindowUs = now;
        m_discoverCount = 0;
    }
    if (++m_discoverCount > kMaxAnnouncesPerSecond)
        return;
    Announce a;
    std::memset(&a, 0, sizeof a);
    a.queryId = d.queryId;
    a.gamePort = m_roomPort;
    a.platform = m_id.platform;
    a.machines = (uint8_t)MachineCount();
    a.humans = (uint8_t)HumanCount();
    const bool playing = m_phase == ONLINE_PHASE_LOADING || m_phase == ONLINE_PHASE_MATCH ||
                         m_phase == ONLINE_PHASE_POST_MATCH || m_starting;
    const bool full = a.machines >= ONLINE_MAX_MACHINES || a.humans >= ONLINE_MAX_SLOTS;
    a.flags = (uint8_t)((playing ? 1 : 0) | (full ? 2 : 0));
    a.build = m_id.build;
    a.dataFamily = m_id.dataFamily;
    std::memcpy(a.hostName, m_nickname, sizeof a.hostName);
    uint8_t out[kAnnounceSize];
    EncodeAnnounce(a, out);
    SendPlain(pipe, from, out, sizeof out);
}

void Session::OnAnnounce(const NetAddr& from, const Announce& a, uint64_t now)
{
    if (!m_searching || a.queryId != m_queryId)
        return;
    NetAddr game = from;
    game.port = a.gamePort;
    char text[ONLINE_ADDRESS_MAX];
    AddrFormat(game, text, sizeof text);
    Nearby* entry = NULL;
    for (size_t i = 0; i < m_nearby.size(); i++)
        if (std::strcmp(m_nearby[i].room.address, text) == 0)
            entry = &m_nearby[i];
    if (entry == NULL)
    {
        if (m_nearby.size() >= ONLINE_MAX_NEARBY)
            return;
        m_nearby.push_back(Nearby());
        entry = &m_nearby.back();
    }
    std::memset(&entry->room, 0, sizeof entry->room);
    std::memcpy(entry->room.address, text, sizeof text);
    std::memcpy(entry->room.hostName, a.hostName, sizeof a.hostName);
    entry->room.platform = a.platform;
    entry->room.machines = a.machines;
    entry->room.humans = a.humans;
    entry->room.inMatch = (a.flags & 1) != 0;
    entry->room.compatible = a.build == m_id.build && a.dataFamily == m_id.dataFamily;
    entry->from = game;
    entry->heardUs = now;
}

void Session::OnSealed(const NetAddr& from, const uint8_t* data, size_t size, uint64_t now)
{
    SealedHeader h;
    if (!DecodeSealedHeader(data, size, &h))
        return;
    Link* l = FindLink(h.linkId);
    if (l == NULL || !l->replay.Fresh(h.seq))
        return;
    uint8_t plain[kMaxDatagram];
    size_t plainSize = 0;
    if (!OpenDatagram(l->recvKey, data, size, plain, &plainSize))
        return;
    l->replay.Accept(h.seq);
    // An authenticated datagram from a new address means the peer's NAT rebound it.
    if (!AddrEqual(l->addr, from))
    {
        l->addr = from;
        l->awayRecv = 0;
        l->awaySent = 0;
    }
    l->awayRecv += size;
    l->authed = true;
    if (m_phase == ONLINE_PHASE_MATCH && Since(now, l->lastHeardUs) > l->matchGapUs)
        l->matchGapUs = Since(now, l->lastHeardUs);
    l->lastHeardUs = now;
    l->stats.OnSeq(h.seq, now);
    if (!ParsePayload(plain, plainSize, m_payload))
    {
        if (++l->malformed >= kMalformedLimit)
        {
            const int machine = l->machine;
            CloseLink(*l, true, ONLINE_ERR_PROTOCOL);
            if (!IsHost())
                Fail(ONLINE_ERR_PROTOCOL, "The room kept sending data this version can't read.");
            else if (machine > 0)
                LinkGone(machine, ONLINE_ERR_PROTOCOL, true);
        }
        return;
    }
    HandlePayload(*l, *m_payload, now);
}

void Session::Deliver(Link& link, const Message& m, uint64_t now)
{
    link.recvNext++;
    HandleMessage(link, m, now, now);
    while (link.used)
    {
        Held& h = link.ahead[link.recvNext % kRelWindow];
        if (!h.has || h.seq != link.recvNext)
            return;
        h.has = false;
        const Message held = h.m;
        link.recvNext++;
        HandleMessage(link, held, now, h.atUs);
    }
}

uint32_t Session::AheadBits(const Link& link) const
{
    uint32_t bits = 0;
    for (int i = 0; i < kRelWindow; i++)
    {
        const uint16_t seq = (uint16_t)(link.recvNext + 1 + i);
        const Held& h = link.ahead[seq % kRelWindow];
        if (h.has && h.seq == seq)
            bits |= 1u << i;
    }
    return bits;
}

void Session::HandlePayload(Link& link, const Payload& p, uint64_t now)
{
    if (p.hasTiming)
        OnTiming(link, p.timing, now);

    if (p.hasRelAck)
    {
        const uint16_t next = p.relAck.nextRelSeq;
        for (std::deque<Outgoing>::iterator it = link.out.begin(); it != link.out.end();)
        {
            const uint16_t ahead = (uint16_t)(it->relSeq - next);
            bool acked = seq_before(it->relSeq, next);
            if (!acked && ahead >= 1 && ahead <= kRelWindow)
                acked = ((p.relAck.bitmap >> (ahead - 1)) & 1) != 0;
            if (acked && it->lastSentUs != 0)
                it = link.out.erase(it);
            else
                ++it;
        }
    }

    // Reliable messages, delivered in order; a handler may close this link.
    for (int i = 0; i < p.messageCount && link.used; i++)
    {
        const Message& m = p.messages[i];
        link.sendNow = true;
        if (m.relSeq == link.recvNext)
        {
            Deliver(link, m, now);
            continue;
        }
        const uint16_t ahead = (uint16_t)(m.relSeq - link.recvNext);
        if (seq_before(m.relSeq, link.recvNext) || ahead > kRelWindow)
            continue;
        Held& h = link.ahead[m.relSeq % kRelWindow];
        h.m = m;
        h.seq = m.relSeq;
        h.atUs = now;
        h.has = true;
    }
    if (!link.used)
        return;

    if (m_lock.Active())
    {
        for (int i = 0; i < p.inputAckCount; i++)
        {
            const InputAckChunk& a = p.inputAck[i];
            if (a.matchSerial == m_lock.Serial() && a.nextTick > link.peerHas[a.machine])
                link.peerHas[a.machine] = a.nextTick;
        }
        for (int i = 0; i < p.inputCount && link.used; i++)
            OnInput(link, p.input[i]);
        for (int i = 0; i < p.hashCount && link.used; i++)
            OnHashes(link, p.hash[i]);
        for (int i = 0; i < p.componentsCount && link.used; i++)
            OnComponents(link, p.components[i]);
    }

    if (p.hasBye && link.used)
    {
        const int machine = link.machine;
        const int reason = p.byeReason;
        CloseLink(link, false, 0);
        if (machine >= 0)
            LinkGone(machine, reason, true);
        else if (!IsHost() && m_join.active)
        {
            char text[ONLINE_ERROR_MAX];
            std::snprintf(text, sizeof text,
                          "Couldn't reach a room at %s. Check the address, or try again.",
                          m_join.text);
            Fail(ONLINE_ERR_UNREACHABLE, text);
        }
    }
}

void Session::Queue(Link& link, uint8_t kind, const Message& m, uint64_t deadlineUs)
{
    if (link.out.size() >= kMaxQueued)
    {
        // A peer can keep sending while it acknowledges nothing; a full queue drops it.
        link.stuck = true;
        return;
    }
    Outgoing o;
    o.kind = kind;
    o.relSeq = link.sendRelSeq++;
    Message copy = m;
    copy.kind = kind;
    o.size = (uint16_t)EncodeMessageBody(copy, o.body);
    o.lastSentUs = 0;
    o.deadlineUs = deadlineUs;
    if (o.size == 0)
        return;
    link.out.push_back(o);
    link.sendNow = true;
}

void Session::SendDatagram(Link& link, uint64_t now)
{
    if (!link.used || m_game == NULL)
        return;
    uint8_t plain[kMaxPayload];
    ChunkWriter w(plain, sizeof plain);

    w.RelAck(link.recvNext, AheadBits(link));

    TimingChunk t;
    t.sendUs = low32(now);
    t.echoUs = link.echoValid ? link.echoUs : 0;
    t.echoHeldUs = link.echoValid ? low32(Since(now, link.echoAtUs)) : 0;
    const bool playing = m_lock.Active() && link.machine >= 0 && m_lock.InMatch(link.machine);
    t.tick = playing ? m_loopNext : 0;
    const uint32_t confirmed = playing ? m_lock.Confirmed() : 0;
    t.confirmedTick = confirmed == kNoTick ? m_loopNext : confirmed;
    w.Timing(t);

    if (playing)
    {
        const uint32_t serial = m_lock.Serial();
        for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
            if (m != m_self && m_lock.InMatch(m))
                w.InputAck(serial, (uint8_t)m, m_lock.Have(m));
    }

    // Control messages before input, which a backlog would otherwise leave no room for.
    size_t sent = 0;
    for (std::deque<Outgoing>::iterator it = link.out.begin();
         it != link.out.end() && sent < kMaxOutstanding; ++it, sent++)
    {
        Outgoing& o = *it;
        uint64_t resend = kMinResendUs;
        if (link.stats.HasRtt() && link.stats.SrttUs() * 3 / 2 > resend)
            resend = link.stats.SrttUs() * 3 / 2;
        if (o.lastSentUs != 0 && Since(now, o.lastSentUs) <= resend)
            continue;
        if (o.deadlineUs != 0)
        {
            // A resend carries the time that remains.
            const uint32_t left = o.deadlineUs > now ? (uint32_t)(o.deadlineUs - now) : 0;
            Put32(o.body + (o.kind == MSG_GO ? kGoStartOffset : kResumeStartOffset), left);
        }
        if (!w.Reliable(o.kind, o.relSeq, o.body, o.size))
            break;
        o.lastSentUs = now;
    }

    if (playing)
    {
        const uint32_t serial = m_lock.Serial();
        // Hashes first: they are small, and input takes whatever room is left.
        for (int o = 0; o < ONLINE_MAX_MACHINES; o++)
        {
            if (!m_lock.InMatch(o) || o == link.machine || (o != m_self && !IsHost()))
                continue;
            const uint32_t newest = m_lock.NewestHash(o);
            if (newest == kNoTick)
                continue;
            uint64_t hashes[kMaxHashesPerChunk];
            uint32_t first =
                newest + 1 >= (uint32_t)kMaxHashesPerChunk ? newest + 1 - kMaxHashesPerChunk : 0;
            int count = 0;
            // The newest run of held hashes, so a lost datagram costs no comparison.
            for (uint32_t k = first; k <= newest; k++)
            {
                uint64_t h;
                if (!m_lock.HashOf(o, k, &h))
                {
                    count = 0;
                    first = k + 1;
                    continue;
                }
                hashes[count++] = h;
            }
            if (count > 0)
                w.Hash(serial, (uint8_t)o, first, count, hashes);
        }
        // This machine's records, and in a star the host forwards every other guest's.
        for (int o = 0; o < ONLINE_MAX_MACHINES; o++)
        {
            if (!m_lock.InMatch(o) || o == link.machine)
                continue;
            if (o != m_self && !IsHost())
                continue;
            uint32_t from = link.peerHas[o];
            uint32_t to = m_lock.Have(o);
            if (m_lock.Dropped(o) && to > m_lock.DropTick(o))
                to = m_lock.DropTick(o);
            if (from >= to)
                continue;
            uint32_t count = to - from;
            if (count > (uint32_t)kMaxRecordsPerChunk)
                count = kMaxRecordsPerChunk;
            const size_t room = w.Room() > 13 ? (w.Room() - 13) / kRecordSize : 0;
            if (count > room)
                count = (uint32_t)room;
            if (count > 0)
                w.Input(serial, (uint8_t)o, from, (int)count, m_lock.Record(o, from));
        }
    }

    if (playing && link.componentsLeft > 0 && m_componentsCount > 0)
    {
        if (w.Components(m_lock.Serial(), (uint8_t)m_self, m_componentsTick, m_componentsCount,
                         m_componentsHash))
            link.componentsLeft--;
    }

    uint8_t out[kMaxDatagram];
    const size_t n = SealDatagram(link.sendKey, link.linkId, ++link.sendSeq, plain, w.Size(), out);
    m_game->Send(link.addr, out, n);
    link.awaySent += n;
    link.lastSentUs = now;
    link.sendNow = false;
}

bool Session::LinkMuted(const Link& link, uint64_t now) const
{
    // A game whose loop has stopped goes quiet, so the other machines' silence rules reach it.
    if (InMatch() && Since(now, m_frameUs) > kLoopGiveUpUs)
        return true;
    // Until the peer is back at the address its handshake proved, three bytes out per byte in,
    // with room for a whole datagram, so a forged source cannot turn this link on a victim.
    return !AddrEqual(link.addr, link.home) && link.awaySent + kMaxDatagram > 3 * link.awayRecv;
}

bool Session::LinkDue(const Link& link, uint64_t now) const
{
    if (!link.used || !link.authed || LinkMuted(link, now))
        return false;
    if (link.sendNow)
        return true;
    const bool playing = m_lock.Active() && m_phase != ONLINE_PHASE_ROOM;
    if (Since(now, link.lastSentUs) >= (playing ? kMatchSendUs : kIdleSendUs))
        return true;
    uint64_t resend = kMinResendUs;
    if (link.stats.HasRtt() && link.stats.SrttUs() * 3 / 2 > resend)
        resend = link.stats.SrttUs() * 3 / 2;
    size_t n = 0;
    for (std::deque<Outgoing>::const_iterator it = link.out.begin();
         it != link.out.end() && n < kMaxOutstanding; ++it, n++)
    {
        if (it->lastSentUs == 0 && Since(now, link.lastSentUs) >= kReliableSoonUs)
            return true;
        if (it->lastSentUs != 0 && Since(now, it->lastSentUs) > resend)
            return true;
    }
    return false;
}

void Session::SendDue(uint64_t now)
{
    if (IsHost() && m_phase != ONLINE_PHASE_IDLE && m_phase != ONLINE_PHASE_ERROR)
        if (m_roomDirty)
            SendRoomState();
    for (size_t i = 0; i < m_links.size(); i++)
        if (LinkDue(*m_links[i], now))
            SendDatagram(*m_links[i], now);
}

void Session::SendFarewells(uint64_t now)
{
    for (size_t i = 0; i < m_farewells.size();)
    {
        Farewell& f = m_farewells[i];
        if (f.left > 0 && now >= f.nextUs && f.pipe != NULL)
        {
            uint8_t plain[8];
            ChunkWriter w(plain, sizeof plain);
            w.Bye(f.reason);
            uint8_t out[64];
            const size_t n = SealDatagram(f.key, f.linkId, ++f.seq, plain, w.Size(), out);
            f.pipe->Send(f.addr, out, n);
            f.left--;
            f.nextUs = now + 50000;
        }
        if (f.left <= 0 || f.pipe == NULL)
        {
            Wipe(f.key, sizeof f.key);
            m_farewells.erase(m_farewells.begin() + (long)i);
            continue;
        }
        i++;
    }
    for (size_t p = 0; p < m_farewellPipes.size();)
    {
        bool used = false;
        for (size_t i = 0; i < m_farewells.size(); i++)
            if (m_farewells[i].pipe == m_farewellPipes[p])
                used = true;
        if (used)
        {
            p++;
            continue;
        }
        m_closedPipes.push_back(m_farewellPipes[p]);
        m_farewellPipes.erase(m_farewellPipes.begin() + (long)p);
    }
}

void Session::JoinTimers(uint64_t now)
{
    if (!m_join.active)
        return;
    if (m_join.resolving)
    {
        NetAddr a;
        const int r = m_env->PollResolve(&a);
        if (r == 0)
        {
            if (Since(now, m_join.startUs) > 10000000)
            {
                char text[ONLINE_ERROR_MAX];
                std::snprintf(text, sizeof text, "Couldn't find %s. Check the address.",
                              m_join.text);
                Fail(ONLINE_ERR_BAD_ADDRESS, text);
            }
            return;
        }
        m_join.resolving = false;
        if (r < 0)
        {
            char text[ONLINE_ERROR_MAX];
            std::snprintf(text, sizeof text, "Couldn't find %s. Check the address.", m_join.text);
            Fail(ONLINE_ERR_BAD_ADDRESS, text);
            return;
        }
        m_join.host = a;
        m_join.stage = ONLINE_JOIN_CONTACTING;
    }
    if (m_join.stage != ONLINE_JOIN_CONTACTING)
        return;
    if (m_join.firstHelloUs == 0)
    {
        // The interface a link-local address belongs to cannot travel in a NetAddr.
        const NetAddr& a = m_join.host;
        if (a.family == 6 && a.ip[0] == 0xFE && (a.ip[1] & 0xC0) == 0x80)
        {
            Fail(ONLINE_ERR_BAD_ADDRESS,
                 "A link-local address (fe80::) can't be joined. Enter the room's IPv4 address.");
            return;
        }
        m_join.firstHelloUs = now;
    }
    if (Since(now, m_join.firstHelloUs) >= kJoinGiveUpUs)
    {
        char text[ONLINE_ERROR_MAX];
        std::snprintf(text, sizeof text,
                      "Couldn't reach a room at %s. Check the address, or try again.", m_join.text);
        Fail(ONLINE_ERR_UNREACHABLE, text);
        return;
    }
    if (m_join.helloUs != 0 && Since(now, m_join.helloUs) < kHelloEveryUs)
        return;
    m_join.helloUs = now;
    Hello h;
    std::memcpy(h.publicKey, m_join.key.publicKey, 32);
    std::memcpy(h.nonce, m_join.nonce, 16);
    uint8_t out[kHelloSize];
    EncodeHello(h, out);
    // An IPv4-only socket, or a device with no IPv6 route, fails the send at once.
    if (m_game != NULL && m_game->Send(m_join.host, out, sizeof out) != 0 &&
        m_join.host.family == 6)
        Fail(ONLINE_ERR_UNREACHABLE,
             "This device couldn't send to that IPv6 address. Enter the room's IPv4 address.");
}

void Session::NextStunServer(uint64_t now)
{
    m_public.stunResolving = false;
    std::memset(&m_public.stunAddr, 0, sizeof m_public.stunAddr);
    if (++m_public.stunServer >= kStunServerCount)
        return;
    if (!m_env->Random(m_public.txid, sizeof m_public.txid))
    {
        m_public.stunServer = kStunServerCount;
        return;
    }
    const StunServer& server = kStunServers[m_public.stunServer];
    m_public.stunResolving = true;
    m_public.stunSends = 0;
    m_public.stunStartUs = now;
    m_env->StartResolve(server.host, server.port);
}

void Session::PublicTimers(uint64_t now)
{
    if (m_public.active && m_public.state == ONLINE_PUBLIC_OPEN &&
        m_env->PollPortMap(m_public.mapWan, sizeof m_public.mapWan) != PORTMAP_OPEN)
    {
        m_public.state = ONLINE_PUBLIC_FOUND;
        m_env->Log("the router stopped forwarding the room's port");
    }
    if (!m_public.active || m_public.state != ONLINE_PUBLIC_LOOKING)
        return;
    if (m_public.map == PORTMAP_PENDING)
        m_public.map = m_env->PollPortMap(m_public.mapWan, sizeof m_public.mapWan);
    if (m_public.stunServer < kStunServerCount && m_public.mapped.family == 0)
    {
        const char* name = kStunServers[m_public.stunServer].host;
        char line[160];
        if (m_public.stunResolving)
        {
            NetAddr a;
            const int r = m_env->PollResolve(&a);
            if (r < 0 || (r > 0 && a.family != 4) ||
                (r == 0 && Since(now, m_public.stunStartUs) > kStunResolveGiveUpUs))
            {
                std::snprintf(line, sizeof line, "couldn't find the STUN server %s", name);
                m_env->Log(line);
                NextStunServer(now);
            }
            else if (r > 0)
            {
                m_public.stunResolving = false;
                m_public.stunAddr = a;
            }
        }
        if (m_public.stunServer < kStunServerCount && m_public.stunAddr.family != 0 &&
            (m_public.stunSends == 0 || Since(now, m_public.stunSentUs) >= kStunEveryUs))
        {
            if (m_public.stunSends >= kStunSends)
            {
                std::snprintf(line, sizeof line, "no answer from the STUN server %s", name);
                m_env->Log(line);
                NextStunServer(now);
            }
            else
            {
                uint8_t out[kStunRequestSize];
                EncodeStunRequest(m_public.txid, out);
                SendPlain(m_game, m_public.stunAddr, out, sizeof out);
                m_public.stunSends++;
                m_public.stunSentUs = now;
            }
        }
    }
    SettlePublic();
}

void Session::SettlePublic()
{
    if (!m_public.active || m_public.state != ONLINE_PUBLIC_LOOKING)
        return;
    const bool stunDone = m_public.mapped.family != 0 || m_public.stunServer >= kStunServerCount;
    if (!stunDone || m_public.map == PORTMAP_PENDING)
        return;
    const unsigned port = m_roomPort;
    const NetAddr& stun = m_public.mapped;
    char stunIp[16] = "";
    if (stun.family != 0)
        std::snprintf(stunIp, sizeof stunIp, "%u.%u.%u.%u", stun.ip[0], stun.ip[1], stun.ip[2],
                      stun.ip[3]);
    const bool stunPublic = stun.family != 0 && !AddrIsLanSource(stun);
    NetAddr wan;
    const bool wanOk = AddrParseNumeric(m_public.mapWan, 1, &wan) && wan.family == 4;
    const bool wanPublic = wanOk && !AddrIsLanSource(wan);
    const bool open = m_public.map == PORTMAP_OPEN;
    // A private outside address is a second router's or, in 100.64/10, a carrier's.
    const bool routerBehind = m_public.map == PORTMAP_SHARED || (open && wanOk && !wanPublic);
    const bool wanCarrier = wanOk && wan.ip[0] == 100 && (wan.ip[1] & 0xC0) == 64;
    const bool shared =
        (stun.family != 0 && !stunPublic) || (routerBehind && (wanCarrier || stun.family == 0));
    const char* openIp = NULL;
    char line[200];
    if (shared)
    {
        m_public.state = ONLINE_PUBLIC_SHARED;
        m_public.address[0] = '\0';
        std::snprintf(line, sizeof line,
                      "the internet sees this network through a shared address (%s); only a VPN "
                      "reaches the room",
                      stun.family != 0 ? stunIp : m_public.mapWan);
    }
    else if (open && wanPublic && (stun.family == 0 || std::memcmp(stun.ip, wan.ip, 4) == 0))
        openIp = m_public.mapWan;
    else if (open && !wanOk && stunPublic)
        openIp = stunIp;
    else if (stunPublic)
    {
        if (open || routerBehind)
        {
            std::snprintf(line, sizeof line,
                          "the internet sees %s, past a second router that doesn't forward port %u",
                          stunIp, port);
            m_env->Log(line);
        }
        if (stun.port != port)
        {
            std::snprintf(line, sizeof line,
                          "the router sends port %u out as %u; a forwarded %u still reaches the "
                          "room",
                          port, (unsigned)stun.port, port);
            m_env->Log(line);
        }
        m_public.state = ONLINE_PUBLIC_FOUND;
        std::snprintf(m_public.address, sizeof m_public.address, "%s:%u", stunIp, port);
        std::snprintf(line, sizeof line, "public address %s, port not forwarded", m_public.address);
    }
    else
    {
        m_public.state = ONLINE_PUBLIC_FAILED;
        m_public.address[0] = '\0';
        std::snprintf(line, sizeof line, "no public address found");
    }
    if (openIp != NULL)
    {
        m_public.state = ONLINE_PUBLIC_OPEN;
        std::snprintf(m_public.address, sizeof m_public.address, "%s:%u", openIp, port);
        std::snprintf(line, sizeof line, "public address %s, port opened by the router",
                      m_public.address);
    }
    m_env->Log(line);
}

void Session::StopPublic()
{
    std::memset(&m_public, 0, sizeof m_public);
    m_public.map = PORTMAP_NONE;
}

void Session::MapPort(uint16_t port)
{
    char wan[16];
    // A refusal at launch may predate the network, so the next room asks again.
    if (!m_config.findPublic ||
        (port == m_mapPort && m_env->PollPortMap(wan, sizeof wan) != PORTMAP_FAILED))
        return;
    m_mapPort = port;
    m_env->StartPortMap(port);
}

void Session::SearchTimers(uint64_t now)
{
    if (m_search == NULL)
        return;
    if (m_searching && m_discoversLeft > 0 && now >= m_discoverNextUs)
    {
        Discover d;
        d.queryId = m_queryId;
        uint8_t out[kDiscoverSize];
        EncodeDiscover(d, out);
        NetAddr targets[16];
        const int n = m_env->BroadcastTargets(targets, 16, m_config.discoveryPort);
        for (int i = 0; i < n; i++)
            SendPlain(m_search, targets[i], out, sizeof out);
        m_discoversLeft--;
        m_discoverNextUs = now + 200000;
    }
    if (m_searching && now >= m_searchEndUs)
        m_searching = false;
    for (size_t i = 0; i < m_nearby.size();)
    {
        if (Since(now, m_nearby[i].heardUs) > kNearbyExpiryUs)
        {
            m_nearby.erase(m_nearby.begin() + (long)i);
            continue;
        }
        i++;
    }
    if (!m_searching && now >= m_searchCloseUs)
        ClosePipe(m_search);
}

void Session::LinkTimers(uint64_t now)
{
    for (size_t i = 0; i < m_links.size(); i++)
    {
        Link& l = *m_links[i];
        if (!l.used)
            continue;
        if (l.closing)
        {
            if (l.out.empty() || now >= l.closeAtUs)
                CloseLink(l, false, 0);
            continue;
        }
        if (!l.authed || l.machine < 0)
        {
            // A handshake with no authenticated datagram, or a link that never sent JOIN.
            if (Since(now, l.createdUs) > kPendingExpiryUs)
                CloseLink(l, false, 0);
            continue;
        }
        if (l.stuck || Since(now, l.lastHeardUs) > kLinkDeadUs)
        {
            const int machine = l.machine;
            CloseLink(l, false, 0);
            LinkGone(machine, 0, false);
        }
    }
    if (IsHost())
        for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
            if (m_room[m].present && m_room[m].link >= 0)
            {
                const Link* l = m_links[m_room[m].link];
                const bool connected = l->used && Since(now, l->lastHeardUs) <= kLinkDeadUs;
                if (connected != m_room[m].connected)
                {
                    m_room[m].connected = connected;
                    RoomChanged();
                }
            }
}

void Session::Poll()
{
    const uint64_t now = m_env->NowUs();
    for (size_t i = 0; i < m_closedPipes.size(); i++)
        m_env->ClosePipe(m_closedPipes[i]);
    m_closedPipes.clear();

    Receive(now);
    JoinTimers(now);
    PublicTimers(now);
    SearchTimers(now);
    LinkTimers(now);
    MatchTimers(now);
    RoomTimers(now);
    RunSchedule(now);
    SendDue(now);
    SendFarewells(now);
}

void Session::Flush()
{
    const uint64_t now = m_env->NowUs();
    RunSchedule(now);
    SendDue(now);
    SendFarewells(now);
}

uint64_t Session::NextWakeUs()
{
    const uint64_t now = m_env->NowUs();
    uint64_t wake = now + kMaxWaitUs;
    if (m_join.active && m_join.stage == ONLINE_JOIN_CONTACTING && m_join.helloUs != 0)
    {
        const uint64_t t = m_join.helloUs + kHelloEveryUs;
        if (t < wake)
            wake = t;
    }
    if (m_join.active && m_join.resolving)
        wake = now + 5000 < wake ? now + 5000 : wake;
    if (m_public.state == ONLINE_PUBLIC_LOOKING)
    {
        if (m_public.stunResolving)
            wake = now + 5000 < wake ? now + 5000 : wake;
        else if (m_public.stunAddr.family != 0 && m_public.mapped.family == 0 &&
                 m_public.stunSentUs + kStunEveryUs < wake)
            wake = m_public.stunSentUs + kStunEveryUs;
    }
    if (m_searching && m_discoversLeft > 0 && m_discoverNextUs < wake)
        wake = m_discoverNextUs;
    for (size_t i = 0; i < m_farewells.size(); i++)
        if (m_farewells[i].nextUs < wake)
            wake = m_farewells[i].nextUs;
    for (size_t i = 0; i < m_links.size(); i++)
    {
        const Link& l = *m_links[i];
        if (!l.used || !l.authed || LinkMuted(l, now))
            continue;
        if (l.sendNow)
            return now;
        const bool playing = m_lock.Active() && m_phase != ONLINE_PHASE_ROOM;
        const uint64_t t = l.lastSentUs + (playing ? kMatchSendUs : kIdleSendUs);
        if (t < wake)
            wake = t;
        if (!l.out.empty())
        {
            const uint64_t soon = l.lastSentUs + kReliableSoonUs;
            bool unsent = false;
            size_t n = 0;
            uint64_t resend = kMinResendUs;
            if (l.stats.HasRtt() && l.stats.SrttUs() * 3 / 2 > resend)
                resend = l.stats.SrttUs() * 3 / 2;
            for (std::deque<Outgoing>::const_iterator it = l.out.begin();
                 it != l.out.end() && n < kMaxOutstanding; ++it, n++)
            {
                if (it->lastSentUs == 0)
                    unsent = true;
                else if (it->lastSentUs + resend + 1 < wake)
                    wake = it->lastSentUs + resend + 1;
            }
            if (unsent && soon < wake)
                wake = soon;
        }
    }
    if (m_scheduleRunning && m_rate > 0)
    {
        const double pos = SchedulePos(now);
        if (pos < (double)m_loopNext + 0.999)
        {
            double next = pos < 0 ? 0.0 : (double)(uint64_t)pos + 1.0;
            const uint64_t t = m_anchorUs + (uint64_t)((next - m_anchorPos) / m_rate);
            if (t < wake)
                wake = t;
        }
    }
    if (m_goReceived && m_phase == ONLINE_PHASE_LOADING && m_startUs < wake)
        wake = m_startUs;
    if (m_barrier.active && m_barrier.resumeKnown && m_barrier.resumeUs > now &&
        m_barrier.resumeUs < wake)
        wake = m_barrier.resumeUs;
    return wake < now ? now : wake;
}

void Session::Shutdown()
{
    StopPublic();
    if (m_mapPort != 0)
        m_env->StopPortMap();
    m_mapPort = 0;
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used)
            CloseLink(*m_links[i], true, 0);
    // No time to space the goodbyes at exit.
    const uint64_t now = m_env->NowUs();
    for (int round = 0; round < 3 && !m_farewells.empty(); round++)
    {
        for (size_t i = 0; i < m_farewells.size(); i++)
            m_farewells[i].nextUs = 0;
        SendFarewells(now);
    }
    m_farewells.clear();
    for (size_t i = 0; i < m_links.size(); i++)
        delete m_links[i];
    m_links.clear();
    ClosePipe(m_game);
    ClosePipe(m_discovery);
    ClosePipe(m_search);
    for (size_t i = 0; i < m_farewellPipes.size(); i++)
        m_closedPipes.push_back(m_farewellPipes[i]);
    m_farewellPipes.clear();
    for (size_t i = 0; i < m_closedPipes.size(); i++)
        m_env->ClosePipe(m_closedPipes[i]);
    m_closedPipes.clear();
    m_phase = ONLINE_PHASE_IDLE;
}
}   // namespace online
