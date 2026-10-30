// The session's commands, the room it keeps, its phases and the reliable messages that move them.

#include "session.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace online
{
namespace
{
const uint64_t kRoomRefreshUs = 2000000;
const uint64_t kExpectedDelayEveryUs = 1000000;

void copy_name(char out[ONLINE_NAME_MAX + 1], const char* in)
{
    PortOnlineCleanName(out, in != NULL ? in : "");
}

const char* region_text(int region) { return region == 1 ? "Japanese" : "USA/Europe"; }
}   // namespace

Session::Session(SessionEnv* env, const Identity& identity, const SessionConfig& config)
    : m_env(env), m_id(identity), m_config(config), m_game(NULL), m_discovery(NULL), m_search(NULL),
      m_payload(new Payload)
{
    std::memset(m_roomOthers, 0, sizeof m_roomOthers);
    m_roomOtherCount = 0;
    m_nextSeed = 0;
    std::memset(&m_settings, 0, sizeof m_settings);
    copy_name(m_settings.nickname, "Player");
    m_settings.hostPort = ONLINE_DEFAULT_PORT;
    m_noticeSerial = 0;
    m_notice = ONLINE_NOTICE_NONE;
    m_noticeMachine = -1;
    m_noticeName[0] = 0;
    m_searching = false;
    m_queryId = 0;
    m_discoversLeft = 0;
    m_discoverNextUs = 0;
    m_searchEndUs = 0;
    m_searchCloseUs = 0;
    m_helloWindowUs = 0;
    m_helloCount = 0;
    m_discoverWindowUs = 0;
    m_discoverCount = 0;
    m_timeScale = 1.0f;
    m_error = ONLINE_ERR_NONE;
    m_lastError[0] = '\0';
    std::memset(&m_public, 0, sizeof m_public);
    m_public.map = PORTMAP_NONE;
    Reset(false);
    FillState(m_env->NowUs());
}

Session::~Session()
{
    Shutdown();
    delete m_payload;
}

void Session::SetSettings(const OnlineSettings& settings)
{
    OnlineSettings next = settings;
    char raw[ONLINE_NAME_MAX + 1];
    std::memcpy(raw, settings.nickname, sizeof raw);
    raw[ONLINE_NAME_MAX] = '\0';
    if (PortOnlineCleanName(next.nickname, raw) == 0)
        std::memcpy(next.nickname, m_settings.nickname, sizeof next.nickname);
    if (next.delayTicks > ONLINE_MAX_DELAY_TICKS)
        next.delayTicks = ONLINE_MAX_DELAY_TICKS;
    next.showStats = next.showStats != 0;
    // Xbox reaches only 1025-49151, so a room there must be reachable from one.
    if (next.hostPort < 1025 || next.hostPort > 49151)
        next.hostPort = ONLINE_DEFAULT_PORT;
    m_settings = next;
}

void Session::Reset(bool keepNearby)
{
    // Link objects outlive a reset, since a message handler may still hold one.
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used)
            CloseLink(*m_links[i], false, 0);
    bool farewell = false;
    for (size_t i = 0; i < m_farewells.size(); i++)
        if (m_game != NULL && m_farewells[i].pipe == m_game)
            farewell = true;
    if (farewell)
    {
        // The goodbyes still need the socket; it closes once they are sent.
        m_farewellPipes.push_back(m_game);
        m_game = NULL;
    }
    else
        ClosePipe(m_game);
    ClosePipe(m_discovery);
    StopPublic();
    if (!keepNearby)
    {
        ClosePipe(m_search);
        m_nearby.clear();
        m_searching = false;
    }

    m_phase = ONLINE_PHASE_IDLE;
    m_hosting = false;
    m_self = -1;
    m_roomKind = ONLINE_ROOM_NONE;
    m_roomAddress[0] = '\0';
    m_roomOtherCount = 0;
    m_roomPort = 0;
    m_roomId = 0;
    m_roomRevision = 0;
    m_roomDirty = false;
    m_roomSentUs = 0;
    std::memset(m_room, 0, sizeof m_room);
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        m_room[m].link = -1;
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            m_room[m].human[h].slot = -1;
    }
    std::memset(m_local, 0, sizeof m_local);
    std::memcpy(m_nickname, m_settings.nickname, sizeof m_nickname);

    PortOnlineDescDefault(&m_desc);
    m_descRevision = 0;
    m_descValid = false;
    m_starting = false;
    m_rematch = false;
    std::memset(&m_selection, 0, sizeof m_selection);
    m_serial = 0;
    m_skipPresentation = 0;
    m_loadingSinceUs = 0;
    m_loadSentState = -1;
    m_loadSentPercent = -1;
    m_expectedDelay = ONLINE_MIN_DELAY_TICKS;
    m_expectedAtUs = 0;

    std::memset(&m_join, 0, sizeof m_join);

    m_lock.Clear();
    m_goReceived = false;
    m_startUs = 0;
    m_matchOver = false;
    m_endTick = kNoTick;
    m_delay = ONLINE_MIN_DELAY_TICKS;
    m_delayAuto = m_settings.delayTicks == 0;
    m_delayGrewUs = 0;
    m_loopNext = 0;
    m_anchorPos = 0;
    m_anchorUs = 0;
    m_rate = 1.0 / kTickUs;
    m_clockRate = 1.0f;
    m_frameUs = 0;
    m_scheduleRunning = false;
    m_stalled = false;
    m_stallStartUs = 0;
    m_stalls.clear();
    m_stallStarts.clear();
    m_advance = 0;
    m_advanceUs = 0;
    m_slowSinceUs = 0;
    m_slowMachine = -1;
    m_stamps.clear();
    std::memset(&m_barrier, 0, sizeof m_barrier);
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        m_barrierDoneKind[m] = 0;
        m_barrierDoneTick[m] = kNoTick;
    }
    m_goals = 0;
    m_opponentsLeft = false;
    m_opponentsAnswered = false;
    m_desync = ONLINE_DESYNC_NONE;
    m_desyncTick = 0;
    m_desyncCount = 0;
    m_desyncComponent[0] = '\0';
    m_desyncReport[0] = '\0';
    std::memset(m_desyncHashes, 0, sizeof m_desyncHashes);
    m_reportPending = false;
    m_events.clear();
    m_componentsCount = 0;
    m_componentsTick = 0;
    std::memset(m_ports, 0, sizeof m_ports);
    std::memset(m_edgeButtons, 0, sizeof m_edgeButtons);
    m_edgePending = false;
    m_edgeUs = 0;
    m_edgeTick = 0;
    m_inputToEffectMs = 0;
    m_voteDeadlineUs = 0;
    m_problem = ONLINE_PROBLEM_NONE;
    m_problemMachine = -1;
    m_problemSeconds = 0;
    m_problemHoldUs = 0;
}

void Session::Fail(int error, const char* sentence)
{
    const int keepError = error;
    char keep[ONLINE_ERROR_MAX];
    std::snprintf(keep, sizeof keep, "%s", sentence);
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used)
            CloseLink(*m_links[i], true, (uint8_t)error);
    Reset(true);
    m_phase = ONLINE_PHASE_ERROR;
    m_error = keepError;
    std::snprintf(m_lastError, sizeof m_lastError, "%s", keep);
    char line[ONLINE_ERROR_MAX + 32];
    std::snprintf(line, sizeof line, "error %d: %s", keepError, keep);
    m_env->Log(line);
    FillState(m_env->NowUs());
}

void Session::Notice(int notice, int machine)
{
    m_noticeSerial++;
    m_notice = notice;
    m_noticeMachine = machine;
    std::snprintf(m_noticeName, sizeof m_noticeName, "%s",
                  machine >= 0 && machine < ONLINE_MAX_MACHINES ? m_room[machine].nickname : "");
}

int Session::HumanCount() const
{
    int n = 0;
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].present)
            for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
                if (m_room[m].human[h].used)
                    n++;
    return n;
}

int Session::MachineCount() const
{
    int n = 0;
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].present)
            n++;
    return n;
}

int Session::FreeSlot() const
{
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
    {
        bool taken = false;
        for (int m = 0; m < ONLINE_MAX_MACHINES && !taken; m++)
            if (m_room[m].present)
                for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
                    if (m_room[m].human[h].used && m_room[m].human[h].slot == s)
                        taken = true;
        if (!taken)
            return s;
    }
    return -1;
}

void Session::HostAddHuman(int machine, int localIndex, const char* name)
{
    m_selection.stage = 0;
    RoomHuman& h = m_room[machine].human[localIndex];
    if (h.used)
    {
        copy_name(h.name, name);
        RoomChanged();
        return;
    }
    const int slot = FreeSlot();
    if (slot < 0)
        return;
    h.used = true;
    h.slot = (int8_t)slot;
    h.vote = ONLINE_VOTE_NONE;
    copy_name(h.name, name);
    m_desc.slotMachine[slot] = (int8_t)machine;
    // The default sides of skipfe: slots 0 and 2 home, 1 and 3 away.
    m_desc.padSide[slot] = (int8_t)(slot & 1);
    DescChanged();
}

void Session::HostRemoveHuman(int machine, int localIndex)
{
    m_selection.stage = 0;
    RoomHuman& h = m_room[machine].human[localIndex];
    if (!h.used)
        return;
    if (h.slot >= 0)
    {
        m_desc.slotMachine[h.slot] = -1;
        m_desc.padSide[h.slot] = -1;
    }
    std::memset(&h, 0, sizeof h);
    h.slot = -1;
    DescChanged();
}

void Session::HostRemoveMachine(int machine, int notice)
{
    if (machine <= 0 || machine >= ONLINE_MAX_MACHINES || !m_room[machine].present)
        return;
    if (notice != ONLINE_NOTICE_NONE)
        Notice(notice, machine);
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        HostRemoveHuman(machine, h);
    m_room[machine].present = false;
    m_room[machine].link = -1;
    std::memset(&m_room[machine], 0, sizeof m_room[machine]);
    m_room[machine].link = -1;
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        m_room[machine].human[h].slot = -1;
    if (m_phase == ONLINE_PHASE_ROOM && MachineCount() < 2)
        m_phase = ONLINE_PHASE_HOSTING;
    // A start waiting on this machine stops, and a rematch goes back to the votes.
    m_starting = false;
    m_rematch = false;
    RoomChanged();
}

void Session::FillDescIdentity(OnlineMatchDesc* desc) const
{
    desc->protocol = ONLINE_PROTOCOL_VERSION;
    desc->build = m_id.build;
    desc->dataFamily = m_id.dataFamily;
}

void Session::DescChanged()
{
    if (!IsHost())
        return;
    FillDescIdentity(&m_desc);
    m_descRevision++;
    m_descValid = PortOnlineDescCheck(&m_desc) == NULL;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_room[m].present)
            continue;
        m_room[m].ready = false;
        m_room[m].descAcked = false;
        SendDesc(m);
    }
    m_room[0].descAcked = true;
    m_starting = false;
    RoomChanged();
}

void Session::SendDesc(int machine)
{
    Message m;
    std::memset(&m, 0, sizeof m);
    m.u.desc.revision = m_descRevision;
    PortOnlineDescEncode(&m_desc, m.u.desc.desc);
    SendReliableTo(machine, MSG_DESC, m);
}

void Session::SendRoomState()
{
    if (!IsHost())
        return;
    const uint64_t now = m_env->NowUs();
    Message msg;
    std::memset(&msg, 0, sizeof msg);
    MsgRoomState& r = msg.u.roomState;
    r.revision = ++m_roomRevision;
    r.phase = (uint8_t)(m_phase == ONLINE_PHASE_HOSTING ? ONLINE_PHASE_ROOM : m_phase);
    r.host = 0;
    r.delayTicks = (uint8_t)(m_phase == ONLINE_PHASE_MATCH || m_phase == ONLINE_PHASE_POST_MATCH
                                 ? m_delay
                                 : m_expectedDelay);
    uint64_t left = m_voteDeadlineUs > now ? (m_voteDeadlineUs - now + 999999) / 1000000 : 0;
    r.voteSeconds = (uint8_t)(m_phase == ONLINE_PHASE_POST_MATCH ? (left > 60 ? 60 : left) : 0);
    r.matchSerial = m_serial;
    r.selection = m_selection;
    for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
    {
        const RoomMachine& s = m_room[i];
        WireMachine& w = r.machine[i];
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            w.human[h].slot = -1;
        if (!s.present)
            continue;
        w.present = 1;
        w.platform = s.platform;
        w.flags = (uint8_t)((s.connected ? MACHINE_CONNECTED : 0) | (s.ready ? MACHINE_READY : 0) |
                            (s.descAcked ? MACHINE_DESC_ACKED : 0));
        w.loadState = s.loadState;
        w.loadPercent = s.loadPercent;
        w.lossPercent = s.lossPercent;
        w.pingMs = s.pingMs;
        w.jitterMs = s.jitterMs;
        std::memcpy(w.nickname, s.nickname, sizeof w.nickname);
        int count = 0;
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        {
            if (!s.human[h].used)
                continue;
            w.human[h].slot = s.human[h].slot;
            w.human[h].vote = s.human[h].vote;
            std::memcpy(w.human[h].name, s.human[h].name, sizeof w.human[h].name);
            count++;
        }
        w.humanCount = (uint8_t)count;
    }
    BroadcastReliable(MSG_ROOM_STATE, msg);
    m_roomDirty = false;
    m_roomSentUs = now;
}

void Session::BroadcastReliable(uint8_t kind, const Message& m)
{
    for (int i = 1; i < ONLINE_MAX_MACHINES; i++)
        if (m_room[i].present && m_room[i].connected)
            SendReliableTo(i, kind, m);
}

void Session::SendReliableTo(int machine, uint8_t kind, const Message& m)
{
    Link* l = MachineLink(machine);
    if (l != NULL)
        Queue(*l, kind, m, 0);
}

uint32_t Session::WorstRttUs() const
{
    uint32_t worst = 0;
    for (size_t i = 0; i < m_links.size(); i++)
    {
        const Link& l = *m_links[i];
        if (l.used && l.machine >= 0 && l.stats.HasRtt() && l.stats.SrttUs() > worst)
            worst = l.stats.SrttUs();
    }
    return worst;
}

uint8_t Session::ExpectedDelay(uint64_t now)
{
    if (m_settings.delayTicks != 0)
        return m_settings.delayTicks;
    // The worst path's input is guessed as far as snapshots reach; only the rest becomes delay.
    double worstUs = 0;
    double rtt[ONLINE_MAX_MACHINES] = {};
    double sd[ONLINE_MAX_MACHINES] = {};
    bool have[ONLINE_MAX_MACHINES] = {};
    for (size_t i = 0; i < m_links.size(); i++)
    {
        const Link& l = *m_links[i];
        if (!l.used || l.machine < 0 || l.machine >= ONLINE_MAX_MACHINES)
            continue;
        have[l.machine] = l.stats.Window(now, 60, &rtt[l.machine], &sd[l.machine]);
    }
    for (int a = 0; a < ONLINE_MAX_MACHINES; a++)
    {
        if (!have[a])
            continue;
        const double direct = rtt[a] / 2 + 2 * sd[a] + 3000;
        if (direct > worstUs)
            worstUs = direct;
        if (!IsHost())
            continue;
        for (int b = a + 1; b < ONLINE_MAX_MACHINES; b++)
        {
            if (!have[b])
                continue;
            const double j = std::sqrt(sd[a] * sd[a] + sd[b] * sd[b]);
            const double star = rtt[a] / 2 + rtt[b] / 2 + 2 * j + 3000;
            if (star > worstUs)
                worstUs = star;
        }
    }
    const double guessedUs = (double)kMaxAhead * kTickUs;
    int d = worstUs > guessedUs ? (int)((worstUs - guessedUs + kTickUs - 1) / kTickUs) : 0;
    if (d < ONLINE_MIN_DELAY_TICKS)
        d = ONLINE_MIN_DELAY_TICKS;
    if (d > ONLINE_MAX_DELAY_TICKS)
        d = ONLINE_MAX_DELAY_TICKS;
    return (uint8_t)d;
}

int Session::Host(uint16_t port, const char* nickname)
{
    if (m_phase != ONLINE_PHASE_IDLE && m_phase != ONLINE_PHASE_ERROR)
        return 0;
    if (!m_config.canHost)
    {
        Fail(ONLINE_ERR_CANT_HOST, "This device can't host a room. Join one instead.");
        return 0;
    }
    Reset(true);
    m_error = ONLINE_ERR_NONE;
    m_lastError[0] = '\0';
    if (nickname != NULL && PortOnlineCleanName(m_nickname, nickname) == 0)
        std::memcpy(m_nickname, m_settings.nickname, sizeof m_nickname);
    if (port == 0)
        port = m_settings.hostPort;

    int err = PIPE_OK;
    m_game = m_env->OpenPipe(PIPE_GAME, port, &err);
    if (m_game == NULL)
    {
        char text[ONLINE_ERROR_MAX];
        if (err == PIPE_ERR_IN_USE)
        {
            std::snprintf(text, sizeof text, "Port %u is in use. Pick another in Online settings.",
                          (unsigned)port);
            Fail(ONLINE_ERR_PORT_IN_USE, text);
        }
        else
            Fail(ONLINE_ERR_NETWORK, "No network is available.");
        return 0;
    }
    m_discovery = m_env->OpenPipe(PIPE_DISCOVERY, m_config.discoveryPort, &err);
    if (m_discovery == NULL)
        m_env->Log("no LAN discovery listener; Nearby won't find this room");

    const uint64_t now = m_env->NowUs();
    m_hosting = true;
    m_self = 0;
    m_phase = ONLINE_PHASE_HOSTING;
    m_roomKind = ONLINE_ROOM_DIRECT;
    m_roomPort = m_env->PipePort(m_game);
    if (m_roomPort == 0)
        m_roomPort = port;
    char lan[64];
    if (m_env->LanAddress(lan, sizeof lan))
        std::snprintf(m_roomAddress, sizeof m_roomAddress, "%s:%u", lan, (unsigned)m_roomPort);
    else
        std::snprintf(m_roomAddress, sizeof m_roomAddress, "127.0.0.1:%u", (unsigned)m_roomPort);
    char others[ONLINE_MAX_OTHER_ADDRESSES][16];
    m_roomOtherCount = m_env->OtherAddresses(others, ONLINE_MAX_OTHER_ADDRESSES);
    for (int i = 0; i < m_roomOtherCount; i++)
        std::snprintf(m_roomOthers[i], sizeof m_roomOthers[i], "%s:%u", others[i],
                      (unsigned)m_roomPort);
    m_roomId = 0;
    while (m_roomId == 0)
        if (!m_env->Random(&m_roomId, sizeof m_roomId))
        {
            Fail(ONLINE_ERR_NETWORK,
                 "This system's random source failed, so online play cannot start.");
            return 0;
        }

    RoomMachine& me = m_room[0];
    me.present = true;
    me.connected = true;
    me.ready = true;
    me.descAcked = true;
    me.platform = m_id.platform;
    me.link = -1;
    std::memcpy(me.nickname, m_nickname, sizeof me.nickname);

    PortOnlineDescDefault(&m_desc);
    FillDescIdentity(&m_desc);
    m_local[0].used = true;
    m_local[0].localPort = 0;
    std::memcpy(m_local[0].name, m_nickname, sizeof m_local[0].name);
    HostAddHuman(0, 0, m_nickname);
    m_expectedDelay = ExpectedDelay(now);
    m_expectedAtUs = now;
    char line[160];
    int n = std::snprintf(line, sizeof line, "hosting on port %u at %s", (unsigned)m_roomPort,
                          m_roomAddress);
    for (int i = 0; i < m_roomOtherCount && n > 0 && n < (int)sizeof line; i++)
        n += std::snprintf(line + n, sizeof line - (size_t)n, "%s %s", i == 0 ? ", also" : "",
                           m_roomOthers[i]);
    m_env->Log(line);
    if (m_config.findPublic)
    {
        m_public.active = true;
        m_public.state = ONLINE_PUBLIC_LOOKING;
        m_public.stunServer = -1;
        m_public.mapStarted = true;
        m_env->StartPortMap(m_roomPort);
        m_public.map = m_env->PollPortMap(m_public.mapWan, sizeof m_public.mapWan);
        NextStunServer(now);
        PublicTimers(now);
    }
    FillState(now);
    return 1;
}

int Session::Join(const char* address, const char* nickname)
{
    if (m_phase != ONLINE_PHASE_IDLE && m_phase != ONLINE_PHASE_ERROR)
        return 0;
    char host[ONLINE_ADDRESS_MAX];
    uint16_t port = 0;
    if (!AddrSplit(address, host, sizeof host, &port, ONLINE_DEFAULT_PORT))
    {
        Fail(ONLINE_ERR_BAD_ADDRESS, "That address isn't valid. Enter it as host:port.");
        return 0;
    }
    bool lan = false;
    for (size_t i = 0; i < m_nearby.size(); i++)
        if (address != NULL && std::strcmp(m_nearby[i].room.address, address) == 0)
            lan = true;
    Reset(true);
    m_error = ONLINE_ERR_NONE;
    m_lastError[0] = '\0';
    if (nickname != NULL && PortOnlineCleanName(m_nickname, nickname) == 0)
        std::memcpy(m_nickname, m_settings.nickname, sizeof m_nickname);

    int err = PIPE_OK;
    m_game = m_env->OpenPipe(PIPE_GAME, 0, &err);
    if (m_game == NULL)
    {
        Fail(ONLINE_ERR_NETWORK, "No network is available.");
        return 0;
    }

    const uint64_t now = m_env->NowUs();
    m_phase = ONLINE_PHASE_JOINING;
    m_roomKind = lan ? ONLINE_ROOM_LAN : ONLINE_ROOM_DIRECT;
    std::snprintf(m_roomAddress, sizeof m_roomAddress, "%s", address);
    m_roomPort = port;
    m_join.active = true;
    m_join.stage = ONLINE_JOIN_RESOLVING;
    m_join.port = port;
    m_join.startUs = now;
    std::snprintf(m_join.text, sizeof m_join.text, "%s", address);
    if (!m_env->Random(m_join.key.secret, sizeof m_join.key.secret) ||
        !m_env->Random(m_join.nonce, sizeof m_join.nonce))
    {
        Fail(ONLINE_ERR_NETWORK,
             "This system's random source failed, so online play cannot start.");
        return 0;
    }
    KeyPairFromSecret(&m_join.key);
    m_local[0].used = true;
    m_local[0].localPort = 0;
    std::memcpy(m_local[0].name, m_nickname, sizeof m_local[0].name);

    if (AddrParseNumeric(host, port, &m_join.host))
        m_join.stage = ONLINE_JOIN_CONTACTING;
    else
    {
        m_join.resolving = true;
        m_env->StartResolve(host, port);
    }
    JoinTimers(now);
    FillState(now);
    return 1;
}

int Session::RefreshNearby()
{
    const uint64_t now = m_env->NowUs();
    if (m_search == NULL)
    {
        int err = PIPE_OK;
        m_search = m_env->OpenPipe(PIPE_SEARCH, 0, &err);
        if (m_search == NULL)
            return 0;
    }
    uint32_t id = 0;
    while (id == 0)
        if (!m_env->Random(&id, sizeof id))
            return 0;
    m_queryId = id;
    m_searching = true;
    m_discoversLeft = 3;
    m_discoverNextUs = now;
    m_searchEndUs = now + 1000000;
    m_searchCloseUs = now + 15000000;
    SearchTimers(now);
    FillState(now);
    return 1;
}

void Session::Leave()
{
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used)
            CloseLink(*m_links[i], true, 0);
    Reset(true);
    m_error = ONLINE_ERR_NONE;
    m_lastError[0] = '\0';
    FillState(m_env->NowUs());
}

int Session::AddLocalPlayer(int localPort, const char* name)
{
    if (localPort < 0 || localPort > 3)
        return 0;
    if (m_phase != ONLINE_PHASE_HOSTING && m_phase != ONLINE_PHASE_ROOM)
        return 0;
    if (m_starting)
        return 0;
    int index = -1;
    for (int i = 0; i < ONLINE_MAX_LOCAL_HUMANS; i++)
    {
        if (m_local[i].used && m_local[i].localPort == localPort)
            return 0;
        if (!m_local[i].used && index < 0)
            index = i;
    }
    if (index < 0 || HumanCount() >= ONLINE_MAX_SLOTS)
        return 0;
    char clean[ONLINE_NAME_MAX + 1];
    if (name == NULL || PortOnlineCleanName(clean, name) == 0)
    {
        char def[64];
        std::snprintf(def, sizeof def, "Player %d", HumanCount() + 1);
        copy_name(clean, def);
    }
    m_local[index].used = true;
    m_local[index].localPort = localPort;
    std::memcpy(m_local[index].name, clean, sizeof clean);
    if (IsHost())
        HostAddHuman(0, index, clean);
    else
    {
        Message m;
        std::memset(&m, 0, sizeof m);
        m.u.localPlayer.op = 1;
        m.u.localPlayer.localIndex = (uint8_t)index;
        std::memcpy(m.u.localPlayer.name, clean, sizeof clean);
        SendReliableTo(0, MSG_LOCAL_PLAYER, m);
    }
    return 1;
}

int Session::RemoveLocalPlayer(int localPort)
{
    if (m_phase != ONLINE_PHASE_HOSTING && m_phase != ONLINE_PHASE_ROOM)
        return 0;
    if (m_starting)
        return 0;
    int index = -1;
    int used = 0;
    for (int i = 0; i < ONLINE_MAX_LOCAL_HUMANS; i++)
    {
        if (!m_local[i].used)
            continue;
        used++;
        if (m_local[i].localPort == localPort)
            index = i;
    }
    // A machine keeps at least one human.
    if (index < 0 || used < 2)
        return 0;
    std::memset(&m_local[index], 0, sizeof m_local[index]);
    if (IsHost())
        HostRemoveHuman(0, index);
    else
    {
        Message m;
        std::memset(&m, 0, sizeof m);
        m.u.localPlayer.op = 2;
        m.u.localPlayer.localIndex = (uint8_t)index;
        SendReliableTo(0, MSG_LOCAL_PLAYER, m);
    }
    return 1;
}

int Session::SetDesc(const OnlineMatchDesc& desc)
{
    if (!IsHost() || (m_phase != ONLINE_PHASE_HOSTING && m_phase != ONLINE_PHASE_ROOM) ||
        m_starting)
        return 0;
    OnlineMatchDesc d = desc;
    FillDescIdentity(&d);
    m_nextSeed = d.seed;
    d.seed = 0;
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
    {
        d.slotMachine[s] = m_desc.slotMachine[s];
        if (d.slotMachine[s] < 0)
            d.padSide[s] = -1;
    }
    if (PortOnlineDescCheck(&d) != NULL)
        return 0;
    if (std::memcmp(&d, &m_desc, sizeof d) == 0)
        return 1;
    m_desc = d;
    DescChanged();
    return 1;
}

int Session::SelectionOwner(int side) const
{
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
        if (m_desc.padSide[s] == side && m_desc.slotMachine[s] >= 0)
            return m_desc.slotMachine[s];
    return 0;
}

int Session::Select(int op, int side, int team, int sidekick, int phase)
{
    if (op < ONLINE_SELECT_BEGIN || op > ONLINE_SELECT_OPTIONS || side < 0 || side > 1 ||
        team < 0 || team > 8 || sidekick < 0 || sidekick > 3 || phase < 0 || phase > 3 ||
        m_phase != ONLINE_PHASE_ROOM || m_starting)
        return 0;
    Message m = {};
    m.u.select = { m_selection.epoch, (uint8_t)op,       (uint8_t)side,
                   (uint8_t)team,     (uint8_t)sidekick, (uint8_t)phase };
    if (IsHost())
        return ApplySelect(m_self, m.u.select);
    if (op != ONLINE_SELECT_CANCEL && op != ONLINE_SELECT_BACK &&
        (op != ONLINE_SELECT_TEAM || SelectionOwner(side) != m_self))
        return 0;
    SendReliableTo(0, MSG_SELECT, m);
    return 1;
}

int Session::ApplySelect(int from, const MsgSelect& select)
{
    if (m_phase != ONLINE_PHASE_ROOM || m_starting || select.epoch != m_selection.epoch ||
        from < 0 || !m_room[from].present)
        return 0;
    switch (select.op)
    {
    case ONLINE_SELECT_BEGIN:
        if (from != 0 || m_selection.stage != 0 || MachineCount() < 2)
            return 0;
        m_selection.epoch++;
        m_selection.optionsOpen = 0;
        m_selection.stage = 1;
        m_selection.phase[0] = m_selection.phase[1] = 1;
        break;
    case ONLINE_SELECT_CANCEL: m_selection.stage = 0; break;
    case ONLINE_SELECT_TEAM:
    {
        const int side = select.side;
        if (side > 1 || select.team > 8 || select.sidekick > 3 || select.phase < 1 ||
            select.phase > 3 || m_selection.stage != 1 || SelectionOwner(side) != from)
            return 0;
        OnlineMatchDesc d = m_desc;
        d.team[side] = select.team;
        d.sidekick[side] = select.sidekick;
        if (!SetDesc(d))
            return 0;
        m_selection.phase[side] = select.phase;
        if (m_selection.phase[0] == 3 && m_selection.phase[1] == 3 && !m_selection.optionsOpen)
            m_selection.stage = 2;
        break;
    }
    case ONLINE_SELECT_BACK:
        if (m_selection.stage != 2)
            return 0;
        m_selection.stage = 1;
        m_selection.phase[0] = m_selection.phase[1] = 1;
        m_selection.epoch++;
        break;
    case ONLINE_SELECT_OPTIONS:
        if (from != 0 || m_selection.stage != 1 || select.phase > 1)
            return 0;
        m_selection.optionsOpen = select.phase;
        if (!m_selection.optionsOpen && m_selection.phase[0] == 3 && m_selection.phase[1] == 3)
            m_selection.stage = 2;
        break;
    case ONLINE_SELECT_FINISH:
        if (from != 0 || m_selection.stage != 2)
            return 0;
        for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
            if (m_room[m].present)
                m_room[m].ready = true;
        if (!Start())
            return 0;
        m_selection.stage = 3;
        break;
    default: return 0;
    }
    RoomChanged();
    return 1;
}

int Session::SetReady(int ready)
{
    if (m_phase != ONLINE_PHASE_ROOM && m_phase != ONLINE_PHASE_HOSTING)
        return 0;
    if (IsHost())
        return 1;
    if (m_self < 1)
        return 0;
    m_room[m_self].ready = ready != 0;
    Message m;
    std::memset(&m, 0, sizeof m);
    m.u.ready = (uint8_t)(ready != 0);
    SendReliableTo(0, MSG_READY, m);
    return 1;
}

int Session::Start()
{
    const uint64_t now = m_env->NowUs();
    FillState(now);
    if (!m_state.canStart)
        return 0;
    uint32_t seed = m_nextSeed;
    while (seed == 0)
        if (!m_env->Random(&seed, sizeof seed))
            return 0;
    m_nextSeed = 0;
    m_desc.seed = seed;
    FillDescIdentity(&m_desc);
    m_descRevision++;
    m_starting = true;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_room[m].present)
            continue;
        m_room[m].descAcked = false;
        SendDesc(m);
    }
    RoomChanged();
    return 1;
}

int Session::Vote(int slot, int vote)
{
    if (m_phase != ONLINE_PHASE_POST_MATCH || vote < ONLINE_VOTE_REMATCH ||
        vote > ONLINE_VOTE_LEAVE)
        return 0;
    if (m_self < 0)
        return 0;
    bool any = false;
    RoomMachine& me = m_room[m_self];
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
    {
        if (!me.human[h].used || (slot >= 0 && me.human[h].slot != slot))
            continue;
        any = true;
        me.human[h].vote = (uint8_t)vote;
        if (!IsHost())
        {
            Message m;
            std::memset(&m, 0, sizeof m);
            m.u.vote.matchSerial = m_serial;
            m.u.vote.slot = me.human[h].slot;
            m.u.vote.vote = (uint8_t)vote;
            SendReliableTo(0, MSG_VOTE, m);
        }
    }
    if (!any)
        return 0;
    if (vote == ONLINE_VOTE_LEAVE)
    {
        Flush();
        Leave();
        return 1;
    }
    if (IsHost())
    {
        RoomChanged();
        HostCheckVotes(m_env->NowUs());
    }
    return 1;
}

int Session::AfterOpponentsLeft(int continueAgainstCpu)
{
    if (!m_opponentsLeft || m_opponentsAnswered || m_phase != ONLINE_PHASE_MATCH)
        return 0;
    m_opponentsAnswered = true;
    if (continueAgainstCpu)
        return 1;
    if (SoloMatch())
    {
        Stamp(m_loopNext, ONLINE_ACTION_END_MATCH, 0, 0, 0);
        Record("end tick %u", m_loopNext);
        return 1;
    }
    if (IsHost())
    {
        Message m;
        std::memset(&m, 0, sizeof m);
        m.u.serialByte.matchSerial = m_serial;
        m.u.serialByte.value = ABORT_HOST_ENDED;
        BroadcastReliable(MSG_ABORT, m);
        BackToRoom();
        return 1;
    }
    return 0;
}

void Session::HostCheckStart()
{
    if (!IsHost() || !m_starting)
        return;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].present && !m_room[m].descAcked)
            return;
    m_starting = false;
    const uint32_t serial = m_serial + 1;
    Message msg;
    std::memset(&msg, 0, sizeof msg);
    msg.u.start.matchSerial = serial;
    msg.u.start.descRevision = m_descRevision;
    msg.u.start.descHash = PortOnlineDescHash(&m_desc);
    BroadcastReliable(MSG_START, msg);
    BeginLoading(serial);
}

void Session::BeginLoading(uint32_t serial)
{
    const uint64_t now = m_env->NowUs();
    m_serial = serial;
    m_skipPresentation = 0;
    m_phase = ONLINE_PHASE_LOADING;
    m_selection.stage = 0;
    m_loadingSinceUs = now;
    m_loadSentState = -1;
    m_loadSentPercent = -1;
    m_goReceived = false;
    m_matchOver = false;
    m_endTick = kNoTick;
    m_starting = false;
    m_rematch = false;
    m_lock.Clear();
    m_desync = ONLINE_DESYNC_NONE;
    m_desyncCount = 0;
    m_desyncComponent[0] = '\0';
    m_desyncReport[0] = '\0';
    m_reportPending = false;
    m_opponentsLeft = false;
    m_opponentsAnswered = false;
    m_events.clear();
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        m_room[m].loadState = ONLINE_LOAD_IDLE;
        m_room[m].loadPercent = 0;
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            m_room[m].human[h].vote = ONLINE_VOTE_NONE;
    }
    char line[64];
    std::snprintf(line, sizeof line, "match %u agreed", (unsigned)serial);
    m_env->Log(line);
    RoomChanged();
}

void Session::HostCheckLoaded()
{
    if (!IsHost() || m_phase != ONLINE_PHASE_LOADING || m_goReceived)
        return;
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].present && m_room[m].loadState != ONLINE_LOAD_READY)
            return;
    const uint64_t now = m_env->NowUs();
    uint64_t lead = 2ull * WorstRttUs() + 100000;
    if (lead < 100000)
        lead = 100000;
    if (lead > 1000000)
        lead = 1000000;
    const uint8_t delay = ExpectedDelay(now);
    Message msg;
    std::memset(&msg, 0, sizeof msg);
    msg.u.go.matchSerial = m_serial;
    msg.u.go.startInUs = (uint32_t)lead;
    msg.u.go.delayTicks = delay;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
    {
        Link* l = MachineLink(m);
        if (l != NULL && m_room[m].present)
            Queue(*l, MSG_GO, msg, now + lead);
    }
    BeginMatch((uint32_t)lead, delay, now);
}

void Session::HostCheckVotes(uint64_t now)
{
    // Once decided, a rematch is starting and late votes change nothing.
    if (!IsHost() || m_phase != ONLINE_PHASE_POST_MATCH || m_starting || m_rematch)
        return;
    // Connected machines only: one dropped from the match stays present until the room.
    int machines = 0;
    int humans = 0;
    int voted = 0;
    int rematch = 0;
    int change = 0;
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_room[m].present || !m_room[m].connected)
            continue;
        machines++;
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        {
            const RoomHuman& hu = m_room[m].human[h];
            if (!hu.used)
                continue;
            humans++;
            if (hu.vote == ONLINE_VOTE_NONE || hu.vote == ONLINE_VOTE_LEAVE)
                continue;
            voted++;
            if (hu.vote == ONLINE_VOTE_REMATCH)
                rematch++;
            if (hu.vote == ONLINE_VOTE_CHANGE_TEAMS)
                change++;
        }
    }
    const bool timeUp = now >= m_voteDeadlineUs;
    if (voted < humans && !timeUp)
        return;
    int outcome;
    if (humans > 0 && rematch == humans && machines >= 2)
        outcome = ONLINE_VOTE_REMATCH;
    else
        outcome = change > 0 ? ONLINE_VOTE_CHANGE_TEAMS : ONLINE_VOTE_BACK_TO_ROOM;
    uint32_t seed = 0;
    while (outcome == ONLINE_VOTE_REMATCH && seed == 0)
        if (!m_env->Random(&seed, sizeof seed))
            outcome = ONLINE_VOTE_BACK_TO_ROOM;

    Message msg;
    std::memset(&msg, 0, sizeof msg);
    msg.u.serialByte.matchSerial = m_serial;
    msg.u.serialByte.value = (uint8_t)outcome;
    BroadcastReliable(MSG_VOTE_RESULT, msg);
    if (outcome == ONLINE_VOTE_REMATCH)
    {
        RemoveDroppedMachines();
        if (MachineCount() < 2)
        {
            BackToRoom();
            return;
        }
        Notice(ONLINE_NOTICE_REMATCH, -1);
        m_rematch = true;
        m_lock.Clear();
        m_scheduleRunning = false;
        m_desc.seed = seed;
        FillDescIdentity(&m_desc);
        m_descRevision++;
        m_starting = true;
        for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
        {
            if (!m_room[m].present)
                continue;
            m_room[m].descAcked = false;
            SendDesc(m);
        }
        m_voteDeadlineUs = ~0ull;
        RoomChanged();
        HostCheckStart();
        return;
    }
    BackToRoom();
}

void Session::RemoveDroppedMachines()
{
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].present && MachineLink(m) == NULL)
            HostRemoveMachine(m, ONLINE_NOTICE_NONE);
}

void Session::BackToRoom()
{
    EndMatchState();
    m_phase = ONLINE_PHASE_ROOM;
    m_starting = false;
    m_rematch = false;
    m_voteDeadlineUs = 0;
    if (IsHost())
    {
        RemoveDroppedMachines();
        m_desc.seed = 0;
        if (MachineCount() < 2)
            m_phase = ONLINE_PHASE_HOSTING;
    }
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        if (m != 0 || !IsHost())
            m_room[m].ready = false;
        m_room[m].loadState = ONLINE_LOAD_IDLE;
        m_room[m].loadPercent = 0;
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            m_room[m].human[h].vote = ONLINE_VOTE_NONE;
    }
    RoomChanged();
}

void Session::LinkGone(int machine, int reason, bool bye)
{
    if (IsHost())
    {
        if (machine <= 0 || !m_room[machine].present)
            return;
        m_room[machine].connected = false;
        m_room[machine].link = -1;
        if (m_phase == ONLINE_PHASE_MATCH && m_lock.Active() && m_lock.InMatch(machine))
        {
            DropMachine(machine);
            RoomChanged();
            return;
        }
        if (m_phase == ONLINE_PHASE_LOADING)
        {
            Message msg;
            std::memset(&msg, 0, sizeof msg);
            msg.u.serialByte.matchSerial = m_serial;
            msg.u.serialByte.value = ABORT_LEFT_WHILE_LOADING;
            HostRemoveMachine(machine, ONLINE_NOTICE_LEFT);
            BroadcastReliable(MSG_ABORT, msg);
            BackToRoom();
            return;
        }
        HostRemoveMachine(machine, ONLINE_NOTICE_LEFT);
        if (m_phase == ONLINE_PHASE_POST_MATCH)
        {
            // A host left alone has nobody to vote with.
            if (MachineCount() < 2)
                BackToRoom();
            else
                HostCheckVotes(m_env->NowUs());
        }
        return;
    }

    // A guest's only link is the host.
    if (m_phase == ONLINE_PHASE_JOINING)
    {
        char text[ONLINE_ERROR_MAX];
        std::snprintf(text, sizeof text,
                      "Couldn't reach a room at %s. Check the address, or try again.", m_join.text);
        Fail(ONLINE_ERR_UNREACHABLE, text);
        return;
    }
    if (m_phase == ONLINE_PHASE_MATCH && m_lock.Active() && !m_matchOver)
    {
        m_room[0].connected = false;
        // Every other machine's input came through the host, so they all go with it.
        for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
            DropMachine(m);
        return;
    }
    if (bye && reason == 0)
        Fail(ONLINE_ERR_HOST_LEFT, "The host closed the room.");
    else if (bye)
        Fail(reason, "The host ended the connection.");
    else
        Fail(ONLINE_ERR_CONNECTION_LOST, "Lost the connection to the room.");
}

void Session::HandleMessage(Link& link, const Message& m, uint64_t now, uint64_t arrivedUs)
{
    const bool host = IsHost();
    const int from = link.machine;
    switch (m.kind)
    {
    case MSG_JOIN:
        if (host && from < 0 && !link.closing)
            HostJoin(link, m.u.join);
        return;
    case MSG_JOIN_ACCEPT:
        if (host || m_phase != ONLINE_PHASE_JOINING)
            return;
        m_self = m.u.joinAccept.machine;
        m_roomId = m.u.joinAccept.roomId;
        m_join.stage = ONLINE_JOIN_ADMITTED;
        m_join.active = false;
        m_phase = ONLINE_PHASE_ROOM;
        {
            char line[64];
            std::snprintf(line, sizeof line, "joined as machine %d", m_self);
            m_env->Log(line);
        }
        return;
    case MSG_JOIN_REFUSE:
    {
        if (host || m_phase != ONLINE_PHASE_JOINING)
            return;
        const MsgJoinRefuse& r = m.u.joinRefuse;
        char text[ONLINE_ERROR_MAX];
        switch (r.reason)
        {
        case ONLINE_ERR_VERSION:
            std::snprintf(text, sizeof text,
                          "That room runs version %s. Both machines need the same version.",
                          r.hostVersion);
            break;
        case ONLINE_ERR_DATA_FAMILY:
            std::snprintf(text, sizeof text, "That room uses %s game data.",
                          region_text(r.hostDataRegion));
            break;
        case ONLINE_ERR_ROOM_FULL: std::snprintf(text, sizeof text, "That room is full."); break;
        case ONLINE_ERR_IN_MATCH:
            std::snprintf(text, sizeof text, "That room is playing a match. Join after it ends.");
            break;
        default: std::snprintf(text, sizeof text, "That room turned this machine away."); break;
        }
        Fail(r.reason, text);
        return;
    }
    case MSG_ROOM_STATE:
        if (!host && m_self > 0)
            GuestRoomState(m.u.roomState);
        return;
    case MSG_DESC:
        if (!host && m_self > 0)
            GuestDesc(link, m.u.desc);
        return;
    case MSG_DESC_ACK:
    {
        if (!host || from <= 0)
            return;
        const MsgDescAck& a = m.u.descAck;
        if (a.accepted && a.revision == m_descRevision && a.hash == PortOnlineDescHash(&m_desc))
        {
            m_room[from].descAcked = true;
            RoomChanged();
            HostCheckStart();
            return;
        }
        if (!a.accepted && (m_starting || m_phase == ONLINE_PHASE_LOADING))
        {
            Message msg;
            std::memset(&msg, 0, sizeof msg);
            msg.u.serialByte.matchSerial = m_serial;
            msg.u.serialByte.value = ABORT_SETUP_MISMATCH;
            BroadcastReliable(MSG_ABORT, msg);
            Notice(ONLINE_NOTICE_SETUP_MISMATCH, from);
            BackToRoom();
        }
        return;
    }
    case MSG_LOCAL_PLAYER:
    {
        if (!host || from <= 0)
            return;
        if ((m_phase != ONLINE_PHASE_ROOM && m_phase != ONLINE_PHASE_HOSTING) || m_starting)
            return;
        const MsgLocalPlayer& l = m.u.localPlayer;
        RoomMachine& rm = m_room[from];
        switch (l.op)
        {
        case 1:
            if (HumanCount() < ONLINE_MAX_SLOTS)
                HostAddHuman(from, l.localIndex, l.name);
            break;
        case 2:
        {
            int used = 0;
            for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
                used += rm.human[h].used ? 1 : 0;
            if (used > 1)
                HostRemoveHuman(from, l.localIndex);
            break;
        }
        case 3:
            if (rm.human[l.localIndex].used)
            {
                std::memcpy(rm.human[l.localIndex].name, l.name, sizeof l.name);
                RoomChanged();
            }
            break;
        case 4:
            std::memcpy(rm.nickname, l.name, sizeof l.name);
            RoomChanged();
            break;
        }
        return;
    }
    case MSG_READY:
        if (!host || from <= 0)
            return;
        m_room[from].ready = m.u.ready != 0;
        RoomChanged();
        return;
    case MSG_START:
    {
        if (host || m_self <= 0)
            return;
        const MsgStart& s = m.u.start;
        if (m_descValid && s.descRevision == m_descRevision &&
            s.descHash == PortOnlineDescHash(&m_desc))
        {
            if (m_phase == ONLINE_PHASE_ROOM || m_phase == ONLINE_PHASE_POST_MATCH)
                BeginLoading(s.matchSerial);
            return;
        }
        Message msg;
        std::memset(&msg, 0, sizeof msg);
        msg.u.descAck.revision = s.descRevision;
        msg.u.descAck.accepted = 0;
        msg.u.descAck.reason = 2;
        Queue(link, MSG_DESC_ACK, msg, 0);
        return;
    }
    case MSG_LOAD_PROGRESS:
        if (!host || from <= 0 || m.u.loadProgress.matchSerial != m_serial)
            return;
        m_room[from].loadState = m.u.loadProgress.loadState;
        m_room[from].loadPercent = m.u.loadProgress.percent;
        RoomChanged();
        HostCheckLoaded();
        return;
    case MSG_GO:
    {
        if (host || m.u.go.matchSerial != m_serial || m_phase != ONLINE_PHASE_LOADING ||
            m_goReceived)
            return;
        // The host's start moment, less the half round trip the message spent getting here;
        // one held behind a lost message counts from its arrival.
        uint64_t half = link.stats.HasRtt() ? link.stats.SrttUs() / 2 : 0;
        uint64_t lead = m.u.go.startInUs > half ? m.u.go.startInUs - half : 0;
        BeginMatch((uint32_t)lead, m.u.go.delayTicks, arrivedUs);
        return;
    }
    case MSG_SELECT:
        if (host && from > 0)
            ApplySelect(from, m.u.select);
        return;
    case MSG_SKIP:
        if (!m_lock.InMatch(from) || m_lock.Dropped(from) || m.u.skip.matchSerial != m_serial)
            return;
        PresentationSkip(m.u.skip.presentation, true);
        return;
    case MSG_BARRIER_DONE:
        if (!host || from <= 0 || m.u.barrierDone.matchSerial != m_serial)
            return;
        m_barrierDoneKind[from] = m.u.barrierDone.kind;
        m_barrierDoneTick[from] = m.u.barrierDone.tick;
        HostCheckBarrier(now);
        return;
    case MSG_RESUME:
    {
        if (host || m.u.resume.matchSerial != m_serial)
            return;
        uint64_t half = link.stats.HasRtt() ? link.stats.SrttUs() / 2 : 0;
        uint64_t lead = m.u.resume.startInUs > half ? m.u.resume.startInUs - half : 0;
        ApplyResume(m.u.resume, arrivedUs + lead);
        return;
    }
    case MSG_DROP:
        if (host || m.u.drop.matchSerial != m_serial || m.u.drop.machine == m_self)
            return;
        ApplyDrop(m.u.drop.machine, m.u.drop.tick, m.u.drop.cpuSlots);
        return;
    case MSG_DESYNC:
        // Only a machine playing this match reports one; a link that never joined is no such thing.
        if (!m_lock.InMatch(from) || m.u.desync.matchSerial != m_serial)
            return;
        RaiseDesync(m.u.desync.tick, m.u.desync.reporter, m.u.desync.hash, true);
        return;
    case MSG_VOTE:
    {
        // A guest whose match ended first votes while this machine still plays the last ticks.
        if (!host || from <= 0 || m.u.vote.matchSerial != m_serial ||
            (m_phase != ONLINE_PHASE_POST_MATCH && m_phase != ONLINE_PHASE_MATCH))
            return;
        RoomMachine& rm = m_room[from];
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            if (rm.human[h].used && rm.human[h].slot == m.u.vote.slot)
                rm.human[h].vote = m.u.vote.vote;
        RoomChanged();
        HostCheckVotes(now);
        return;
    }
    case MSG_VOTE_RESULT:
        if (host || m.u.serialByte.matchSerial != m_serial)
            return;
        if (m.u.serialByte.value == ONLINE_VOTE_REMATCH)
        {
            Notice(ONLINE_NOTICE_REMATCH, -1);
            m_lock.Clear();
            m_scheduleRunning = false;
            return;
        }
        BackToRoom();
        return;
    case MSG_ABORT:
        if (host || m.u.serialByte.matchSerial != m_serial)
            return;
        if (m.u.serialByte.value == ABORT_SETUP_MISMATCH)
            Notice(ONLINE_NOTICE_SETUP_MISMATCH, 0);
        if (m.u.serialByte.value == ABORT_DESYNC)
        {
            if (m_desync == ONLINE_DESYNC_NONE)
                RaiseDesync(m_loopNext, 0, NULL, true);
            if (m_reportPending)
                WriteReport();
            m_desync = ONLINE_DESYNC_ENDED;
        }
        BackToRoom();
        return;
    case MSG_DELAY:
        if (host || m.u.serialByte.matchSerial != m_serial)
            return;
        if (m.u.serialByte.value > m_delay)
        {
            m_delay = m.u.serialByte.value;
            m_lock.SetDelay(m_delay);
            Record("delay tick %u ticks %d", m_loopNext, m_delay);
        }
        return;
    default: return;
    }
}

void Session::HostJoin(Link& link, const MsgJoin& j)
{
    uint8_t reason = 0;
    int free = -1;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
        if (!m_room[m].present)
        {
            free = m;
            break;
        }
    if (j.protocol != ONLINE_PROTOCOL_VERSION)
        reason = ONLINE_ERR_VERSION;
    else if (free < 0 || HumanCount() + j.humanCount > ONLINE_MAX_SLOTS)
        reason = ONLINE_ERR_ROOM_FULL;
    else if (m_phase != ONLINE_PHASE_ROOM && m_phase != ONLINE_PHASE_HOSTING)
        reason = ONLINE_ERR_IN_MATCH;
    else if (m_starting)
        reason = ONLINE_ERR_IN_MATCH;
    else if (std::strcmp(j.version, m_id.version) != 0)
        reason = ONLINE_ERR_VERSION;
    else if (j.dataFamily != m_id.dataFamily)
        reason = ONLINE_ERR_DATA_FAMILY;
    else if (j.build != m_id.build)
        reason = ONLINE_ERR_VERSION;

    char line[160];
    if (reason != 0)
    {
        Message m;
        std::memset(&m, 0, sizeof m);
        m.u.joinRefuse.reason = reason;
        m.u.joinRefuse.hostPlatform = m_id.platform;
        m.u.joinRefuse.hostDataRegion = m_id.dataRegion;
        std::memcpy(m.u.joinRefuse.hostVersion, m_id.version, sizeof m_id.version);
        Queue(link, MSG_JOIN_REFUSE, m, 0);
        link.closing = true;
        link.closeAtUs = m_env->NowUs() + 2000000;
        std::snprintf(line, sizeof line, "refused %s (%s, version %s): reason %d", j.nickname,
                      PortOnlinePlatformName(j.platform), j.version, reason);
        m_env->Log(line);
        return;
    }

    link.machine = free;
    RoomMachine& rm = m_room[free];
    std::memset(&rm, 0, sizeof rm);
    for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        rm.human[h].slot = -1;
    rm.present = true;
    rm.connected = true;
    rm.platform = j.platform;
    std::memcpy(rm.nickname, j.nickname, sizeof rm.nickname);
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i] == &link)
            rm.link = (int)i;

    Message m;
    std::memset(&m, 0, sizeof m);
    m.u.joinAccept.machine = (uint8_t)free;
    m.u.joinAccept.roomId = m_roomId;
    Queue(link, MSG_JOIN_ACCEPT, m, 0);
    // Adding the humans sends the descriptor with their slots.
    for (int h = 0; h < j.humanCount; h++)
        HostAddHuman(free, h, j.human[h]);
    m_phase = ONLINE_PHASE_ROOM;
    Notice(ONLINE_NOTICE_JOINED, free);
    RoomChanged();
    SendRoomState();
    std::snprintf(line, sizeof line, "machine %d joined: %s on %s", free, j.nickname,
                  PortOnlinePlatformName(j.platform));
    m_env->Log(line);
}

void Session::GuestRoomState(const MsgRoomState& r)
{
    m_selection = r.selection;
    for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
    {
        const WireMachine& w = r.machine[i];
        RoomMachine& rm = m_room[i];
        const bool wasPresent = rm.present;
        if (!w.present && wasPresent && i != m_self && i != 0)
            Notice(ONLINE_NOTICE_LEFT, i);
        const uint8_t myLoad = rm.loadState;
        const uint8_t myPercent = rm.loadPercent;
        std::memset(&rm, 0, sizeof rm);
        rm.link = -1;
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            rm.human[h].slot = -1;
        if (!w.present)
        {
            continue;
        }
        rm.present = true;
        rm.connected = (w.flags & MACHINE_CONNECTED) != 0;
        rm.ready = (w.flags & MACHINE_READY) != 0;
        rm.descAcked = (w.flags & MACHINE_DESC_ACKED) != 0;
        rm.platform = w.platform;
        rm.loadState = w.loadState;
        rm.loadPercent = w.loadPercent;
        rm.lossPercent = w.lossPercent;
        rm.pingMs = w.pingMs;
        rm.jitterMs = w.jitterMs;
        std::memcpy(rm.nickname, w.nickname, sizeof rm.nickname);
        if (!wasPresent && i != m_self && i != 0)
            Notice(ONLINE_NOTICE_JOINED, i);
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
        {
            if (w.human[h].slot < 0)
                continue;
            rm.human[h].used = true;
            rm.human[h].slot = w.human[h].slot;
            rm.human[h].vote = w.human[h].vote;
            std::memcpy(rm.human[h].name, w.human[h].name, sizeof rm.human[h].name);
        }
        if (i == m_self)
        {
            // This machine's own load report is fresher than the host's echo of it.
            rm.loadState = myLoad;
            rm.loadPercent = myPercent;
        }
    }
    m_expectedDelay = r.delayTicks;
    if (m_phase == ONLINE_PHASE_POST_MATCH)
        m_voteDeadlineUs = m_env->NowUs() + (uint64_t)r.voteSeconds * 1000000u;
    if (!m_room[m_self].present && m_phase != ONLINE_PHASE_MATCH)
        Fail(ONLINE_ERR_CONNECTION_LOST, "The host removed this machine from the room.");
}

void Session::GuestDesc(Link& link, const MsgDesc& d)
{
    OnlineMatchDesc desc;
    const char* bad = PortOnlineDescDecode(d.desc, &desc);
    uint8_t reason = 0;
    if (bad != NULL)
    {
        reason = 1;
        char line[96];
        std::snprintf(line, sizeof line, "descriptor rejected: %s", bad);
        m_env->Log(line);
    }
    else if (desc.build != m_id.build || desc.dataFamily != m_id.dataFamily)
        reason = 2;
    Message m;
    std::memset(&m, 0, sizeof m);
    m.u.descAck.revision = d.revision;
    m.u.descAck.accepted = reason == 0;
    m.u.descAck.reason = reason;
    if (reason == 0)
    {
        m_desc = desc;
        m_descRevision = d.revision;
        m_descValid = true;
        m.u.descAck.hash = PortOnlineDescHash(&desc);
    }
    Queue(link, MSG_DESC_ACK, m, 0);
}

void Session::RoomTimers(uint64_t now)
{
    if (!IsHost())
        return;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
    {
        RoomMachine& rm = m_room[m];
        Link* l = MachineLink(m);
        if (!rm.present || l == NULL)
            continue;
        rm.pingMs = (uint16_t)(l->stats.HasRtt() ? l->stats.SrttUs() / 1000 : 0);
        const uint32_t jitter = l->stats.JitterUs(now) / 1000;
        rm.jitterMs = (uint16_t)(jitter > 65535 ? 65535 : jitter);
        rm.lossPercent = (uint8_t)l->stats.LossPercent(now);
    }
    if (Since(now, m_expectedAtUs) >= kExpectedDelayEveryUs)
    {
        m_expectedAtUs = now;
        const uint8_t d = ExpectedDelay(now);
        if (d != m_expectedDelay && m_phase != ONLINE_PHASE_MATCH)
        {
            m_expectedDelay = d;
            RoomChanged();
        }
    }
    if (m_phase == ONLINE_PHASE_POST_MATCH && m_voteDeadlineUs != ~0ull)
        HostCheckVotes(now);
    if (m_roomDirty || Since(now, m_roomSentUs) >= kRoomRefreshUs)
        SendRoomState();
}

bool Session::Active() const
{
    return m_phase != ONLINE_PHASE_IDLE && m_phase != ONLINE_PHASE_ERROR;
}

bool Session::InMatch() const { return m_phase == ONLINE_PHASE_MATCH && m_lock.Active(); }

void Session::FillState(uint64_t now)
{
    OnlineState& s = m_state;
    std::memset(&s, 0, sizeof s);
    s.phase = m_phase;
    s.joinStage = m_join.stage;
    s.joinSeconds =
        m_phase == ONLINE_PHASE_JOINING ? (float)(Since(now, m_join.startUs)) / 1e6f : 0.0f;
    s.roomKind = m_roomKind;
    std::memcpy(s.roomAddress, m_roomAddress, sizeof s.roomAddress);
    std::memcpy(s.roomOtherAddresses, m_roomOthers, sizeof s.roomOtherAddresses);
    s.roomOtherAddressCount = m_roomOtherCount;
    s.roomPort = m_roomPort;
    s.publicState = m_public.state;
    std::memcpy(s.publicAddress, m_public.address, sizeof s.publicAddress);
    s.roomId = m_roomId;
    s.self = m_self;
    s.host = m_phase == ONLINE_PHASE_IDLE || m_phase == ONLINE_PHASE_ERROR ||
                     m_phase == ONLINE_PHASE_JOINING
                 ? -1
                 : 0;

    const Link* hostLink = NULL;
    if (!IsHost())
        for (size_t i = 0; i < m_links.size(); i++)
            if (m_links[i]->used && m_links[i]->machine == 0)
                hostLink = m_links[i];

    int count = 0;
    for (int m = 0; m < ONLINE_MAX_MACHINES && s.host >= 0; m++)
    {
        const RoomMachine& rm = m_room[m];
        OnlineMachine& om = s.machines[m];
        if (!rm.present)
            continue;
        count++;
        std::memcpy(om.nickname, rm.nickname, sizeof om.nickname);
        om.present = 1;
        om.connected = rm.connected;
        om.isHost = m == 0;
        om.isSelf = m == m_self;
        om.platform = rm.platform;
        om.ready = rm.ready;
        om.descAcked = rm.descAcked;
        om.loadState = rm.loadState;
        om.loadPercent = rm.loadPercent;
        om.lossPercent = rm.lossPercent;
        om.pingMs = rm.pingMs;
        om.jitterMs = rm.jitterMs;
        if (m == m_self)
        {
            om.connected = 1;
            om.pingMs = 0;
            om.jitterMs = 0;
            om.lossPercent = 0;
        }
        else if (m == 0 && hostLink != NULL)
        {
            // A guest shows its own round trip to the host.
            om.pingMs = (uint16_t)(hostLink->stats.HasRtt() ? hostLink->stats.SrttUs() / 1000 : 0);
            const uint32_t jitter = hostLink->stats.JitterUs(now) / 1000;
            om.jitterMs = (uint16_t)(jitter > 65535 ? 65535 : jitter);
            om.lossPercent = (uint8_t)hostLink->stats.LossPercent(now);
            om.connected = !m_lock.Dropped(0) || !m_lock.Active();
        }
        int humans = 0;
        // Humans in ascending slot order, as the input record lists them.
        for (int pass = 0; pass < ONLINE_MAX_SLOTS; pass++)
            for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            {
                const RoomHuman& rh = rm.human[h];
                if (!rh.used || rh.slot != pass || humans >= ONLINE_MAX_LOCAL_HUMANS)
                    continue;
                OnlineHuman& oh = om.humans[humans++];
                std::memcpy(oh.name, rh.name, sizeof oh.name);
                oh.slot = rh.slot;
                oh.side = m_desc.padSide[rh.slot];
                oh.localPort = (int8_t)(m == m_self && m_local[h].used ? m_local[h].localPort : -1);
                oh.vote = rh.vote;
            }
        om.humanCount = (uint8_t)humans;
    }
    s.machineCount = count;

    if (IsHost() && m_phase == ONLINE_PHASE_ROOM && !m_starting && count >= 2 && m_descValid)
    {
        bool all = true;
        for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
            if (m_room[m].present && (!m_room[m].ready || !m_room[m].connected))
                all = false;
        s.canStart = all;
    }

    s.selection = m_selection;
    s.desc = m_desc;
    s.descHash = PortOnlineDescHash(&m_desc);
    s.matchSerial = m_serial;

    const bool playing = m_lock.Active();
    s.delayTicks = playing ? m_delay : m_expectedDelay;
    s.delayMs = s.delayTicks * ONLINE_TICK_MS;
    s.delayAuto = m_settings.delayTicks == 0;

    s.tick = m_loopNext;
    const uint32_t confirmed = playing ? m_lock.Confirmed() : 0;
    s.confirmedTick = confirmed == kNoTick ? m_loopNext : confirmed;
    s.advanceTicks = (float)m_advance;
    int stalls = 0;
    int longest = 0;
    for (size_t i = 0; i < m_stalls.size(); i++)
    {
        if (Since(now, m_stalls[i].startUs) > 60000000)
            continue;
        stalls++;
        if ((int)m_stalls[i].ms > longest)
            longest = (int)m_stalls[i].ms;
    }
    if (m_stalled && Since(now, m_stallStartUs) <= 60000000)
    {
        stalls++;
        const int ms = (int)((Since(now, m_stallStartUs)) / 1000);
        if (ms > longest)
            longest = ms;
    }
    s.stallsLastMinute = stalls;
    s.longestStallMs = longest;
    s.inputToEffectMs = m_inputToEffectMs;

    s.problem = m_problem;
    s.problemMachine = m_problemMachine;
    s.problemSeconds = m_problemSeconds;
    s.opponentsLeft = m_opponentsLeft && !m_opponentsAnswered;

    s.desync = m_desync;
    s.desyncTick = m_desyncTick;
    s.desyncCount = m_desyncCount;
    std::memcpy(s.desyncComponent, m_desyncComponent, sizeof s.desyncComponent);
    std::memcpy(s.desyncReport, m_desyncReport, sizeof s.desyncReport);

    if (m_phase == ONLINE_PHASE_POST_MATCH && m_voteDeadlineUs != ~0ull && m_voteDeadlineUs > now)
        s.voteSeconds = (float)(m_voteDeadlineUs - now) / 1e6f;

    s.nearbySearching = m_searching;
    int n = 0;
    for (size_t i = 0; i < m_nearby.size() && n < ONLINE_MAX_NEARBY; i++)
    {
        s.nearby[n] = m_nearby[i].room;
        const uint64_t age = (Since(now, m_nearby[i].heardUs)) / 1000;
        s.nearby[n].ageMs = (uint16_t)(age > 65535 ? 65535 : age);
        n++;
    }
    s.nearbyCount = n;

    s.noticeSerial = m_noticeSerial;
    s.notice = m_notice;
    s.noticeMachine = m_noticeMachine;
    std::memcpy(s.noticeName, m_noticeName, sizeof s.noticeName);
    s.error = m_error;
    std::memcpy(s.lastError, m_lastError, sizeof s.lastError);
    s.settings = m_settings;
}

void Session::Update(float timeScale)
{
    const uint64_t now = m_env->NowUs();
    m_frameUs = now;
    if (timeScale > 0.0f && timeScale != m_timeScale)
    {
        if (m_scheduleRunning)
            Reanchor(SchedulePos(now), now);
        m_timeScale = timeScale;
        m_rate = (double)m_clockRate * m_timeScale / kTickUs;
    }
    if (m_reportPending)
        WriteReport();
    if (m_lock.Active())
        CommitConfirmed();
    UpdateProblem(now);
    FillState(now);
}
}   // namespace online
