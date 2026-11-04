// Encoders and the checking decoders for the online wire format.

#include "wire.h"

#include <cstring>

namespace online
{
namespace
{
const uint8_t kMagic[3] = { 'S', 'M', 'O' };

bool all_zero(const uint8_t* p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (p[i] != 0)
            return false;
    return true;
}

void put_header(uint8_t* p, uint8_t cls, uint16_t protocol)
{
    p[0] = cls;
    std::memcpy(p + 1, kMagic, 3);
    Put16(p + 4, protocol);
    Put16(p + 6, 0);
}

bool is_platform(uint8_t v) { return v < ONLINE_PLATFORM_COUNT; }
bool is_flag(uint8_t v) { return v <= 1; }
bool is_delay(uint8_t v) { return v <= ONLINE_MAX_DELAY_TICKS; }
// PORT_BARRIER_INTRO, _GOAL, _END and _PAUSE.
bool is_barrier(uint8_t v) { return v >= 1 && v <= 4; }

// Start times name a moment a few round trips ahead; anything past 2 s is not one this build sends.
const uint32_t kMaxStartInUs = 2000000;

size_t message_size(uint8_t kind)
{
    switch (kind)
    {
    case MSG_JOIN:          return 82;
    case MSG_JOIN_ACCEPT:   return 8;
    case MSG_JOIN_REFUSE:   return 20;
    case MSG_ROOM_STATE:    return 276;
    case MSG_DESC:          return 68;
    case MSG_DESC_ACK:      return 14;
    case MSG_LOCAL_PLAYER:  return 20;
    case MSG_READY:         return 1;
    case MSG_START:         return 16;
    case MSG_LOAD_PROGRESS: return 6;
    case MSG_GO:            return 12;
    case MSG_BARRIER_DONE:  return 10;
    case MSG_RESUME:        return 20;
    case MSG_DROP:          return 10;
    case MSG_DESYNC:        return 44;
    case MSG_VOTE:          return 6;
    case MSG_VOTE_RESULT:   return 5;
    case MSG_ABORT:         return 5;
    case MSG_DELAY:         return 5;
    case MSG_SKIP:          return 8;
    case MSG_SELECT:        return 9;
    default:                return 0;
    }
}

// The sender's quantisation leaves every stick byte even.
bool stick_ok(uint8_t b) { return (b & 1) == 0; }

void put_machine(uint8_t* p, const WireMachine& m)
{
    std::memset(p, 0, 64);
    if (!m.present)
        return;
    p[0] = 1;
    p[1] = m.platform;
    p[2] = m.flags;
    p[3] = m.loadState;
    p[4] = m.loadPercent;
    p[5] = m.lossPercent;
    Put16(p + 6, m.pingMs);
    Put16(p + 8, m.jitterMs);
    PutName(p + 10, m.nickname);
    p[26] = m.humanCount;
    p[27] = m.delayTicks;
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
    {
        uint8_t* e = p + 28 + 18 * h;
        const WireHuman& w = m.human[h];
        if (w.slot < 0)
        {
            e[0] = 0xFF;
            continue;
        }
        e[0] = (uint8_t)w.slot;
        e[1] = w.vote;
        PutName(e + 2, w.name);
    }
}

bool get_machine(const uint8_t* p, WireMachine* m)
{
    std::memset(m, 0, sizeof *m);
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        m->human[h].slot = -1;
    if (p[0] == 0)
    {
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            m->human[h].slot = -1;
        return all_zero(p, 64);
    }
    if (p[0] != 1 || !is_platform(p[1]) || (p[2] & ~0x0F) != 0 || p[3] > ONLINE_LOAD_READY ||
        p[4] > 100 || p[5] > 100 || p[26] > ONLINE_MAX_LOCAL_HUMANS || !is_delay(p[27]))
        return false;
    m->present = 1;
    m->platform = p[1];
    m->flags = p[2];
    m->loadState = p[3];
    m->loadPercent = p[4];
    m->lossPercent = p[5];
    m->pingMs = Get16(p + 6);
    m->jitterMs = Get16(p + 8);
    if (!GetName(p + 10, m->nickname))
        return false;
    m->humanCount = p[26];
    m->delayTicks = p[27];
    int used = 0;
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
    {
        const uint8_t* e = p + 28 + 18 * h;
        const int8_t slot = (int8_t)e[0];
        if (slot == -1)
        {
            if (!all_zero(e + 1, 17))
                return false;
            continue;
        }
        if (slot < 0 || slot >= ONLINE_MAX_SLOTS || e[1] > ONLINE_VOTE_LEAVE)
            return false;
        m->human[h].slot = slot;
        m->human[h].vote = e[1];
        if (!GetName(e + 2, m->human[h].name))
            return false;
        used++;
    }
    return used == m->humanCount;
}
}   // namespace

void PutName(uint8_t* p, const char* name)
{
    char clean[ONLINE_NAME_MAX + 1];
    const int n = PortOnlineCleanName(clean, name);
    std::memset(p, 0, ONLINE_NAME_MAX);
    std::memcpy(p, clean, (size_t)n);
}

bool GetName(const uint8_t* p, char out[ONLINE_NAME_MAX + 1])
{
    size_t n = 0;
    while (n < ONLINE_NAME_MAX && p[n] != 0)
        n++;
    if (!all_zero(p + n, ONLINE_NAME_MAX - n))
        return false;
    char raw[ONLINE_NAME_MAX + 1];
    std::memcpy(raw, p, n);
    raw[n] = '\0';
    char clean[ONLINE_NAME_MAX + 1];
    PortOnlineCleanName(clean, raw);
    if (std::strcmp(clean, raw) != 0)
        return false;
    std::memcpy(out, raw, n + 1);
    return true;
}

void PutVersion(uint8_t* p, const char* version)
{
    std::memset(p, 0, 16);
    for (size_t i = 0; i < 16 && version[i] != '\0'; i++)
        p[i] = (version[i] >= 0x20 && version[i] < 0x7F) ? (uint8_t)version[i] : (uint8_t)'?';
}

bool GetVersion(const uint8_t* p, char out[17])
{
    size_t n = 0;
    while (n < 16 && p[n] != 0)
    {
        if (p[n] < 0x20 || p[n] >= 0x7F)
            return false;
        n++;
    }
    if (n == 0 || !all_zero(p + n, 16 - n))
        return false;
    std::memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

void RecordPut(uint8_t rec[kRecordSize], int human, const PadBytes& pad)
{
    uint8_t* p = rec + 8 * human;
    Put16(p, (uint16_t)(pad.buttons & PORT_INPUT_BUTTONS));
    p[2] = (uint8_t)PortSimQuantizeStick(pad.stickX);
    p[3] = (uint8_t)PortSimQuantizeStick(pad.stickY);
    p[4] = (uint8_t)PortSimQuantizeStick(pad.substickX);
    p[5] = (uint8_t)PortSimQuantizeStick(pad.substickY);
    p[6] = pad.triggerL;
    p[7] = pad.triggerR;
}

PadBytes RecordGet(const uint8_t rec[kRecordSize], int human)
{
    const uint8_t* p = rec + 8 * human;
    PadBytes pad;
    pad.buttons = Get16(p);
    pad.stickX = (int8_t)p[2];
    pad.stickY = (int8_t)p[3];
    pad.substickX = (int8_t)p[4];
    pad.substickY = (int8_t)p[5];
    pad.triggerL = p[6];
    pad.triggerR = p[7];
    return pad;
}

bool RecordValid(const uint8_t rec[kRecordSize], int humans)
{
    if ((rec[16] & 0xC0) != 0)
        return false;
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
    {
        const uint8_t* p = rec + 8 * h;
        if (h >= humans)
        {
            if (!all_zero(p, 8) || (rec[16] & (1 << h)) != 0)
                return false;
            continue;
        }
        if ((Get16(p) & ~PORT_INPUT_BUTTONS) != 0)
            return false;
        for (int i = 2; i < 6; i++)
            if (!stick_ok(p[i]))
                return false;
    }
    return true;
}

int PlainClass(const uint8_t* p, size_t n, uint16_t* protocol)
{
    if (n < 8 || std::memcmp(p + 1, kMagic, 3) != 0 || Get16(p + 6) != 0)
        return 0;
    size_t want;
    switch (p[0])
    {
    case DG_DISCOVER: want = kDiscoverSize; break;
    case DG_ANNOUNCE: want = kAnnounceSize; break;
    case DG_HELLO:    want = kHelloSize; break;
    case DG_WELCOME:  want = kWelcomeSize; break;
    case DG_REFUSE:   want = kRefuseSize; break;
    default:          return 0;
    }
    if (n != want)
        return 0;
    const uint16_t v = Get16(p + 4);
    if (v != ONLINE_PROTOCOL_VERSION && p[0] != DG_HELLO && p[0] != DG_REFUSE)
        return 0;
    if (protocol != NULL)
        *protocol = v;
    return p[0];
}

size_t EncodeDiscover(const Discover& d, uint8_t out[kDiscoverSize])
{
    std::memset(out, 0, kDiscoverSize);
    put_header(out, DG_DISCOVER, ONLINE_PROTOCOL_VERSION);
    Put32(out + 8, d.queryId);
    return kDiscoverSize;
}

size_t EncodeAnnounce(const Announce& a, uint8_t out[kAnnounceSize])
{
    std::memset(out, 0, kAnnounceSize);
    put_header(out, DG_ANNOUNCE, ONLINE_PROTOCOL_VERSION);
    Put32(out + 8, a.queryId);
    Put16(out + 12, a.gamePort);
    out[14] = a.platform;
    out[15] = a.machines;
    out[16] = a.humans;
    out[17] = a.flags;
    Put32(out + 20, a.build);
    Put32(out + 24, a.dataFamily);
    PutName(out + 32, a.hostName);
    return kAnnounceSize;
}

size_t EncodeHello(const Hello& h, uint8_t out[kHelloSize])
{
    std::memset(out, 0, kHelloSize);
    put_header(out, DG_HELLO, ONLINE_PROTOCOL_VERSION);
    std::memcpy(out + 8, h.publicKey, 32);
    std::memcpy(out + 40, h.nonce, 16);
    return kHelloSize;
}

size_t EncodeWelcome(const Welcome& w, uint8_t out[kWelcomeSize])
{
    std::memset(out, 0, kWelcomeSize);
    put_header(out, DG_WELCOME, ONLINE_PROTOCOL_VERSION);
    std::memcpy(out + 8, w.publicKey, 32);
    std::memcpy(out + 40, w.nonce, 16);
    Put32(out + 56, w.linkId);
    return kWelcomeSize;
}

size_t EncodeRefuse(const Refuse& r, uint8_t out[kRefuseSize])
{
    std::memset(out, 0, kRefuseSize);
    put_header(out, DG_REFUSE, r.protocol);
    std::memcpy(out + 8, r.nonce, 16);
    out[24] = r.reason;
    return kRefuseSize;
}

bool DecodeDiscover(const uint8_t* p, size_t n, Discover* out)
{
    if (PlainClass(p, n, NULL) != DG_DISCOVER || !all_zero(p + 12, 36))
        return false;
    out->queryId = Get32(p + 8);
    return true;
}

bool DecodeAnnounce(const uint8_t* p, size_t n, Announce* out)
{
    if (PlainClass(p, n, NULL) != DG_ANNOUNCE)
        return false;
    Announce a;
    a.queryId = Get32(p + 8);
    a.gamePort = Get16(p + 12);
    a.platform = p[14];
    a.machines = p[15];
    a.humans = p[16];
    a.flags = p[17];
    a.build = Get32(p + 20);
    a.dataFamily = Get32(p + 24);
    if (a.gamePort == 0 || !is_platform(a.platform) || a.machines < 1 ||
        a.machines > ONLINE_MAX_MACHINES || a.humans > ONLINE_MAX_SLOTS || (a.flags & ~3) != 0 ||
        Get16(p + 18) != 0 || Get32(p + 28) != 0 || !GetName(p + 32, a.hostName))
        return false;
    *out = a;
    return true;
}

bool DecodeHello(const uint8_t* p, size_t n, Hello* out)
{
    uint16_t protocol = 0;
    if (PlainClass(p, n, &protocol) != DG_HELLO || protocol != ONLINE_PROTOCOL_VERSION ||
        !all_zero(p + 56, 72))
        return false;
    std::memcpy(out->publicKey, p + 8, 32);
    std::memcpy(out->nonce, p + 40, 16);
    return true;
}

bool DecodeWelcome(const uint8_t* p, size_t n, Welcome* out)
{
    if (PlainClass(p, n, NULL) != DG_WELCOME || !all_zero(p + 60, 4) || Get32(p + 56) == 0)
        return false;
    std::memcpy(out->publicKey, p + 8, 32);
    std::memcpy(out->nonce, p + 40, 16);
    out->linkId = Get32(p + 56);
    return true;
}

bool DecodeRefuse(const uint8_t* p, size_t n, Refuse* out)
{
    uint16_t protocol = 0;
    if (PlainClass(p, n, &protocol) != DG_REFUSE || !all_zero(p + 25, 7))
        return false;
    const uint8_t reason = p[24];
    if (reason != ONLINE_ERR_VERSION && reason != ONLINE_ERR_ROOM_FULL &&
        reason != ONLINE_ERR_IN_MATCH)
        return false;
    out->protocol = protocol;
    std::memcpy(out->nonce, p + 8, 16);
    out->reason = reason;
    return true;
}

void EncodeSealedHeader(const SealedHeader& h, uint8_t out[kSealedHeader])
{
    out[0] = DG_SEALED;
    out[1] = out[2] = out[3] = 0;
    Put32(out + 4, h.linkId);
    Put64(out + 8, h.seq);
}

bool DecodeSealedHeader(const uint8_t* p, size_t n, SealedHeader* out)
{
    if (n < kSealedHeader + kTagSize || n > kMaxDatagram || p[0] != DG_SEALED || p[1] != 0 ||
        p[2] != 0 || p[3] != 0)
        return false;
    out->linkId = Get32(p + 4);
    out->seq = Get64(p + 8);
    return out->linkId != 0 && out->seq != 0;
}

size_t EncodeMessageBody(const Message& m, uint8_t out[kMaxMessageBody])
{
    const size_t size = message_size(m.kind);
    if (size == 0)
        return 0;
    std::memset(out, 0, size);
    switch (m.kind)
    {
    case MSG_JOIN:
    {
        const MsgJoin& j = m.u.join;
        Put16(out, j.protocol);
        out[2] = j.platform;
        out[3] = j.binaryFamily;
        Put32(out + 4, j.build);
        Put32(out + 8, j.dataFamily);
        out[12] = j.delayTicks;
        PutVersion(out + 16, j.version);
        PutName(out + 32, j.nickname);
        out[48] = j.humanCount;
        for (int h = 0; h < j.humanCount && h < ONLINE_MAX_LOCAL_HUMANS; h++)
            PutName(out + 50 + 16 * h, j.human[h]);
        break;
    }
    case MSG_JOIN_ACCEPT:
        out[0] = m.u.joinAccept.machine;
        Put32(out + 4, m.u.joinAccept.roomId);
        break;
    case MSG_JOIN_REFUSE:
        out[0] = m.u.joinRefuse.reason;
        out[1] = m.u.joinRefuse.hostPlatform;
        out[2] = m.u.joinRefuse.hostDataRegion;
        PutVersion(out + 4, m.u.joinRefuse.hostVersion);
        break;
    case MSG_ROOM_STATE:
    {
        const MsgRoomState& r = m.u.roomState;
        Put32(out, r.revision);
        out[4] = r.phase;
        out[5] = r.host;
        out[6] = r.delayTicks;
        out[7] = r.voteSeconds;
        Put32(out + 8, r.matchSerial);
        Put32(out + 268, r.selection.epoch);
        out[272] = r.selection.stage;
        out[273] = r.selection.phase[0];
        out[274] = r.selection.phase[1];
        out[275] = r.selection.optionsOpen;
        for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
            put_machine(out + 12 + 64 * i, r.machine[i]);
        break;
    }
    case MSG_DESC:
        Put32(out, m.u.desc.revision);
        std::memcpy(out + 4, m.u.desc.desc, ONLINE_DESC_BYTES);
        break;
    case MSG_DESC_ACK:
        Put32(out, m.u.descAck.revision);
        Put64(out + 4, m.u.descAck.hash);
        out[12] = m.u.descAck.accepted;
        out[13] = m.u.descAck.reason;
        break;
    case MSG_LOCAL_PLAYER:
        out[0] = m.u.localPlayer.op;
        out[1] = m.u.localPlayer.localIndex;
        if (m.u.localPlayer.op != 2)
            PutName(out + 4, m.u.localPlayer.name);
        break;
    case MSG_READY: out[0] = m.u.ready; break;
    case MSG_START:
        Put32(out, m.u.start.matchSerial);
        Put32(out + 4, m.u.start.descRevision);
        Put64(out + 8, m.u.start.descHash);
        break;
    case MSG_LOAD_PROGRESS:
        Put32(out, m.u.loadProgress.matchSerial);
        out[4] = m.u.loadProgress.loadState;
        out[5] = m.u.loadProgress.percent;
        break;
    case MSG_GO:
        Put32(out, m.u.go.matchSerial);
        Put32(out + 4, m.u.go.startInUs);
        out[8] = m.u.go.delayTicks;
        break;
    case MSG_SELECT:
        Put32(out, m.u.select.epoch);
        out[4] = m.u.select.op;
        out[5] = m.u.select.side;
        out[6] = m.u.select.team;
        out[7] = m.u.select.sidekick;
        out[8] = m.u.select.phase;
        break;
    case MSG_SKIP:
        Put32(out, m.u.skip.matchSerial);
        Put32(out + 4, m.u.skip.presentation);
        break;
    case MSG_BARRIER_DONE:
        Put32(out, m.u.barrierDone.matchSerial);
        out[4] = m.u.barrierDone.kind;
        Put32(out + 6, m.u.barrierDone.tick);
        break;
    case MSG_RESUME:
        Put32(out, m.u.resume.matchSerial);
        out[4] = m.u.resume.kind;
        out[5] = m.u.resume.delayTicks;
        out[6] = m.u.resume.actions;
        Put32(out + 8, m.u.resume.tick);
        Put32(out + 12, m.u.resume.startInUs);
        Put32(out + 16, m.u.resume.seed);
        break;
    case MSG_DROP:
        Put32(out, m.u.drop.matchSerial);
        out[4] = m.u.drop.machine;
        out[5] = m.u.drop.cpuSlots;
        Put32(out + 6, m.u.drop.tick);
        break;
    case MSG_DESYNC:
        Put32(out, m.u.desync.matchSerial);
        Put32(out + 4, m.u.desync.tick);
        out[8] = m.u.desync.reporter;
        for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
            Put64(out + 12 + 8 * i, m.u.desync.hash[i]);
        break;
    case MSG_VOTE:
        Put32(out, m.u.vote.matchSerial);
        out[4] = (uint8_t)m.u.vote.slot;
        out[5] = m.u.vote.vote;
        break;
    case MSG_VOTE_RESULT:
    case MSG_ABORT:
    case MSG_DELAY:
        Put32(out, m.u.serialByte.matchSerial);
        out[4] = m.u.serialByte.value;
        break;
    default: return 0;
    }
    return size;
}

bool DecodeMessageBody(uint8_t kind, const uint8_t* p, size_t n, Message* out)
{
    const size_t size = message_size(kind);
    if (size == 0 || n != size)
        return false;
    out->kind = kind;
    switch (kind)
    {
    case MSG_JOIN:
    {
        MsgJoin& j = out->u.join;
        j.protocol = Get16(p);
        j.platform = p[2];
        j.binaryFamily = p[3];
        j.build = Get32(p + 4);
        j.dataFamily = Get32(p + 8);
        j.delayTicks = p[12];
        j.humanCount = p[48];
        if (!is_platform(j.platform) || j.binaryFamily > FAMILY_OTHER ||
            !GetVersion(p + 16, j.version) || !GetName(p + 32, j.nickname) || j.humanCount < 1 ||
            j.humanCount > ONLINE_MAX_LOCAL_HUMANS || p[49] != 0 || !is_delay(p[12]) ||
            !all_zero(p + 13, 3))
            return false;
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        {
            j.human[h][0] = '\0';
            if (h >= j.humanCount)
            {
                if (!all_zero(p + 50 + 16 * h, 16))
                    return false;
            }
            else if (!GetName(p + 50 + 16 * h, j.human[h]))
                return false;
        }
        return true;
    }
    case MSG_JOIN_ACCEPT:
        out->u.joinAccept.machine = p[0];
        out->u.joinAccept.roomId = Get32(p + 4);
        return p[0] >= 1 && p[0] < ONLINE_MAX_MACHINES && all_zero(p + 1, 3);
    case MSG_JOIN_REFUSE:
        out->u.joinRefuse.reason = p[0];
        out->u.joinRefuse.hostPlatform = p[1];
        out->u.joinRefuse.hostDataRegion = p[2];
        return p[0] >= ONLINE_ERR_UNAVAILABLE && p[0] <= ONLINE_ERR_SETUP_MISMATCH &&
               is_platform(p[1]) && p[2] <= 1 && p[3] == 0 &&
               GetVersion(p + 4, out->u.joinRefuse.hostVersion);
    case MSG_ROOM_STATE:
    {
        MsgRoomState& r = out->u.roomState;
        r.revision = Get32(p);
        r.phase = p[4];
        r.host = p[5];
        r.delayTicks = p[6];
        r.voteSeconds = p[7];
        r.matchSerial = Get32(p + 8);
        r.selection.epoch = Get32(p + 268);
        r.selection.stage = p[272];
        r.selection.phase[0] = p[273];
        r.selection.phase[1] = p[274];
        r.selection.optionsOpen = p[275];
        if (p[272] > 3 || p[273] > 3 || p[274] > 3 || p[275] > 1)
            return false;
        if (r.phase < ONLINE_PHASE_ROOM || r.phase > ONLINE_PHASE_POST_MATCH ||
            r.host >= ONLINE_MAX_MACHINES || !is_delay(r.delayTicks) || r.voteSeconds > 60)
            return false;
        int slots = 0;
        int humans = 0;
        for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
        {
            if (!get_machine(p + 12 + 64 * i, &r.machine[i]))
                return false;
            for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            {
                const int slot = r.machine[i].human[h].slot;
                if (slot < 0)
                    continue;
                if (slots & (1 << slot))
                    return false;
                slots |= 1 << slot;
                humans++;
            }
        }
        return r.machine[r.host].present && humans <= ONLINE_MAX_SLOTS;
    }
    case MSG_DESC:
        out->u.desc.revision = Get32(p);
        std::memcpy(out->u.desc.desc, p + 4, ONLINE_DESC_BYTES);
        return true;
    case MSG_DESC_ACK:
        out->u.descAck.revision = Get32(p);
        out->u.descAck.hash = Get64(p + 4);
        out->u.descAck.accepted = p[12];
        out->u.descAck.reason = p[13];
        return is_flag(p[12]) && p[13] <= 2 && (p[12] == 0 || p[13] == 0);
    case MSG_LOCAL_PLAYER:
    {
        MsgLocalPlayer& l = out->u.localPlayer;
        l.op = p[0];
        l.localIndex = p[1];
        l.name[0] = '\0';
        if (l.op < 1 || l.op > 4 || l.localIndex >= ONLINE_MAX_LOCAL_HUMANS || p[2] != 0 ||
            p[3] != 0)
            return false;
        if (l.op == 2)
            return all_zero(p + 4, 16);
        return GetName(p + 4, l.name);
    }
    case MSG_READY: out->u.ready = p[0]; return is_flag(p[0]);
    case MSG_START:
        out->u.start.matchSerial = Get32(p);
        out->u.start.descRevision = Get32(p + 4);
        out->u.start.descHash = Get64(p + 8);
        return true;
    case MSG_LOAD_PROGRESS:
        out->u.loadProgress.matchSerial = Get32(p);
        out->u.loadProgress.loadState = p[4];
        out->u.loadProgress.percent = p[5];
        return p[4] <= ONLINE_LOAD_READY && p[5] <= 100;
    case MSG_GO:
        out->u.go.matchSerial = Get32(p);
        out->u.go.startInUs = Get32(p + 4);
        out->u.go.delayTicks = p[8];
        return out->u.go.startInUs <= kMaxStartInUs && is_delay(p[8]) && all_zero(p + 9, 3);
    case MSG_SELECT:
        out->u.select = { Get32(p), p[4], p[5], p[6], p[7], p[8] };
        return p[4] >= ONLINE_SELECT_BEGIN && p[4] <= ONLINE_SELECT_OPTIONS && p[5] < 2 &&
               p[6] <= 8 && p[7] < 4 && p[8] <= 3;
    case MSG_SKIP:
        out->u.skip.matchSerial = Get32(p);
        out->u.skip.presentation = Get32(p + 4);
        return out->u.skip.presentation != 0;
    case MSG_BARRIER_DONE:
        out->u.barrierDone.matchSerial = Get32(p);
        out->u.barrierDone.kind = p[4];
        out->u.barrierDone.tick = Get32(p + 6);
        return is_barrier(p[4]) && p[5] == 0;
    case MSG_RESUME:
    {
        MsgResume& r = out->u.resume;
        r.matchSerial = Get32(p);
        r.kind = p[4];
        r.delayTicks = p[5];
        r.actions = p[6];
        r.tick = Get32(p + 8);
        r.startInUs = Get32(p + 12);
        r.seed = Get32(p + 16);
        return is_barrier(r.kind) && is_delay(r.delayTicks) &&
               (r.actions & ONLINE_ACTION_RESUME) != 0 &&
               (r.actions & ~(ONLINE_ACTION_RESUME | ONLINE_ACTION_RESEED)) == 0 && p[7] == 0 &&
               r.startInUs <= kMaxStartInUs;
    }
    case MSG_DROP:
        out->u.drop.matchSerial = Get32(p);
        out->u.drop.machine = p[4];
        out->u.drop.cpuSlots = p[5];
        out->u.drop.tick = Get32(p + 6);
        return p[4] < ONLINE_MAX_MACHINES && (p[5] & ~0x0F) == 0;
    case MSG_DESYNC:
        out->u.desync.matchSerial = Get32(p);
        out->u.desync.tick = Get32(p + 4);
        out->u.desync.reporter = p[8];
        for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
            out->u.desync.hash[i] = Get64(p + 12 + 8 * i);
        return p[8] < ONLINE_MAX_MACHINES && all_zero(p + 9, 3);
    case MSG_VOTE:
        out->u.vote.matchSerial = Get32(p);
        out->u.vote.slot = (int8_t)p[4];
        out->u.vote.vote = p[5];
        return out->u.vote.slot >= 0 && out->u.vote.slot < ONLINE_MAX_SLOTS &&
               p[5] >= ONLINE_VOTE_REMATCH && p[5] <= ONLINE_VOTE_LEAVE;
    case MSG_VOTE_RESULT:
        out->u.serialByte.matchSerial = Get32(p);
        out->u.serialByte.value = p[4];
        return p[4] >= ONLINE_VOTE_REMATCH && p[4] <= ONLINE_VOTE_BACK_TO_ROOM;
    case MSG_ABORT:
        out->u.serialByte.matchSerial = Get32(p);
        out->u.serialByte.value = p[4];
        return p[4] >= ABORT_SETUP_MISMATCH && p[4] <= ABORT_HOST_ENDED;
    case MSG_DELAY:
        out->u.serialByte.matchSerial = Get32(p);
        out->u.serialByte.value = p[4];
        return is_delay(p[4]);
    default: return false;
    }
}

bool ParsePayload(const uint8_t* p, size_t n, Payload* out)
{
    out->hasTiming = false;
    out->hasRelAck = false;
    out->inputCount = 0;
    out->inputAckCount = 0;
    out->hashCount = 0;
    out->componentsCount = 0;
    out->hasBye = false;
    out->byeReason = 0;
    out->messageCount = 0;

    size_t at = 0;
    while (at < n)
    {
        if (n - at < 3)
            return false;
        const uint8_t kind = p[at];
        const size_t len = Get16(p + at + 1);
        at += 3;
        if (len > n - at)
            return false;
        const uint8_t* b = p + at;
        at += len;

        switch (kind)
        {
        case CH_TIMING:
            if (len != 20 || out->hasTiming)
                return false;
            out->hasTiming = true;
            out->timing.sendUs = Get32(b);
            out->timing.echoUs = Get32(b + 4);
            out->timing.echoHeldUs = Get32(b + 8);
            out->timing.tick = Get32(b + 12);
            out->timing.confirmedTick = Get32(b + 16);
            break;
        case CH_RELACK:
            if (len != 6 || out->hasRelAck)
                return false;
            out->hasRelAck = true;
            out->relAck.nextRelSeq = Get16(b);
            out->relAck.bitmap = Get32(b + 2);
            break;
        case CH_INPUT:
        {
            if (len < 10 || out->inputCount >= kMaxChunksPerMachine)
                return false;
            const int count = b[5];
            if (count < 1 || count > kMaxRecordsPerChunk ||
                len != 10 + kRecordSize * (size_t)count || b[4] >= ONLINE_MAX_MACHINES)
                return false;
            InputChunk& c = out->input[out->inputCount++];
            c.matchSerial = Get32(b);
            c.machine = b[4];
            c.count = (uint8_t)count;
            c.firstTick = Get32(b + 6);
            for (int i = 0; i < count; i++)
            {
                const uint8_t* r = b + 10 + kRecordSize * (size_t)i;
                if (!RecordValid(r, ONLINE_MAX_LOCAL_HUMANS))
                    return false;
                std::memcpy(c.records[i], r, kRecordSize);
            }
            break;
        }
        case CH_INPUT_ACK:
            if (len != 9 || out->inputAckCount >= kMaxChunksPerMachine ||
                b[4] >= ONLINE_MAX_MACHINES)
                return false;
            out->inputAck[out->inputAckCount].matchSerial = Get32(b);
            out->inputAck[out->inputAckCount].machine = b[4];
            out->inputAck[out->inputAckCount].nextTick = Get32(b + 5);
            out->inputAckCount++;
            break;
        case CH_HASH:
        case CH_COMPONENTS:
        {
            if (len < 10 || b[4] >= ONLINE_MAX_MACHINES)
                return false;
            const int count = b[9];
            const int cap = kind == CH_HASH ? kMaxHashesPerChunk : kMaxComponents;
            if (count < 1 || count > cap || len != 10 + 8 * (size_t)count)
                return false;
            uint64_t* hashes;
            if (kind == CH_HASH)
            {
                if (out->hashCount >= kMaxChunksPerMachine)
                    return false;
                HashChunk& c = out->hash[out->hashCount++];
                c.matchSerial = Get32(b);
                c.machine = b[4];
                c.firstTick = Get32(b + 5);
                c.count = (uint8_t)count;
                hashes = c.hash;
            }
            else
            {
                if (out->componentsCount >= kMaxChunksPerMachine)
                    return false;
                ComponentsChunk& c = out->components[out->componentsCount++];
                c.matchSerial = Get32(b);
                c.machine = b[4];
                c.tick = Get32(b + 5);
                c.count = (uint8_t)count;
                hashes = c.hash;
            }
            for (int i = 0; i < count; i++)
                hashes[i] = Get64(b + 10 + 8 * (size_t)i);
            break;
        }
        case CH_BYE:
            if (len != 1 || out->hasBye || b[0] > ONLINE_ERR_SETUP_MISMATCH)
                return false;
            out->hasBye = true;
            out->byeReason = b[0];
            break;
        default:
        {
            if (kind < MSG_JOIN || len < 2 || out->messageCount >= kMaxMessagesPerPayload)
                return false;
            Message& m = out->messages[out->messageCount];
            m.relSeq = Get16(b);
            if (!DecodeMessageBody(kind, b + 2, len - 2, &m))
                return false;
            out->messageCount++;
            break;
        }
        }
    }
    return true;
}

uint8_t* ChunkWriter::Begin(uint8_t kind, size_t length)
{
    if (length > 0xFFFF || 3 + length > Room())
        return NULL;
    uint8_t* p = m_buf + m_size;
    p[0] = kind;
    Put16(p + 1, (uint16_t)length);
    m_size += 3 + length;
    return p + 3;
}

bool ChunkWriter::Timing(const TimingChunk& t)
{
    uint8_t* b = Begin(CH_TIMING, 20);
    if (b == NULL)
        return false;
    Put32(b, t.sendUs);
    Put32(b + 4, t.echoUs);
    Put32(b + 8, t.echoHeldUs);
    Put32(b + 12, t.tick);
    Put32(b + 16, t.confirmedTick);
    return true;
}

bool ChunkWriter::RelAck(uint16_t nextRelSeq, uint32_t bitmap)
{
    uint8_t* b = Begin(CH_RELACK, 6);
    if (b == NULL)
        return false;
    Put16(b, nextRelSeq);
    Put32(b + 2, bitmap);
    return true;
}

bool ChunkWriter::Input(uint32_t serial, uint8_t machine, uint32_t firstTick, int count,
                        const uint8_t* records)
{
    if (count < 1 || count > kMaxRecordsPerChunk)
        return false;
    uint8_t* b = Begin(CH_INPUT, 10 + kRecordSize * (size_t)count);
    if (b == NULL)
        return false;
    Put32(b, serial);
    b[4] = machine;
    b[5] = (uint8_t)count;
    Put32(b + 6, firstTick);
    std::memcpy(b + 10, records, kRecordSize * (size_t)count);
    return true;
}

bool ChunkWriter::InputAck(uint32_t serial, uint8_t machine, uint32_t nextTick)
{
    uint8_t* b = Begin(CH_INPUT_ACK, 9);
    if (b == NULL)
        return false;
    Put32(b, serial);
    b[4] = machine;
    Put32(b + 5, nextTick);
    return true;
}

bool ChunkWriter::Hash(uint32_t serial, uint8_t machine, uint32_t firstTick, int count,
                       const uint64_t* hashes)
{
    if (count < 1 || count > kMaxHashesPerChunk)
        return false;
    uint8_t* b = Begin(CH_HASH, 10 + 8 * (size_t)count);
    if (b == NULL)
        return false;
    Put32(b, serial);
    b[4] = machine;
    Put32(b + 5, firstTick);
    b[9] = (uint8_t)count;
    for (int i = 0; i < count; i++)
        Put64(b + 10 + 8 * (size_t)i, hashes[i]);
    return true;
}

bool ChunkWriter::Components(uint32_t serial, uint8_t machine, uint32_t tick, int count,
                             const uint64_t* hashes)
{
    if (count < 1 || count > kMaxComponents)
        return false;
    uint8_t* b = Begin(CH_COMPONENTS, 10 + 8 * (size_t)count);
    if (b == NULL)
        return false;
    Put32(b, serial);
    b[4] = machine;
    Put32(b + 5, tick);
    b[9] = (uint8_t)count;
    for (int i = 0; i < count; i++)
        Put64(b + 10 + 8 * (size_t)i, hashes[i]);
    return true;
}

bool ChunkWriter::Bye(uint8_t reason)
{
    uint8_t* b = Begin(CH_BYE, 1);
    if (b == NULL)
        return false;
    b[0] = reason;
    return true;
}

bool ChunkWriter::Reliable(uint8_t kind, uint16_t relSeq, const uint8_t* body, size_t size)
{
    uint8_t* b = Begin(kind, 2 + size);
    if (b == NULL)
        return false;
    Put16(b, relSeq);
    std::memcpy(b + 2, body, size);
    return true;
}
}   // namespace online
