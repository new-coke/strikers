// The match in lockstep: schedule, local input, game hooks, time sync, barriers and desync.

#include "session.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace online
{
namespace
{
const uint64_t kWaitingAfterUs = 1000000;
const uint64_t kDropAfterUs = 6000000;
const uint64_t kSlowLoadingUs = 30000000;
const uint64_t kSlowDeviceUs = 3000000;
const uint64_t kProblemHoldUs = 3000000;
const uint64_t kStallWindowUs = 5000000;
const uint64_t kDelayGrowEveryUs = 10000000;
const uint64_t kMaxTimingAgeUs = 250000;
const uint64_t kStretchWithinUs = 60000;
const size_t kMaxEvents = 512;

// A pause (barrier 4) resumes the match where it stopped, so it keeps the gameplay stream as it is.
uint8_t resume_actions(int kind)
{
    return (uint8_t)(ONLINE_ACTION_RESUME | (kind != 4 ? ONLINE_ACTION_RESEED : 0u));
}

uint32_t fnv32(const uint8_t* p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

void append(std::string& text, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    const int size = std::vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (size > 0)
    {
        std::vector<char> line((size_t)size + 1);
        std::vsnprintf(line.data(), line.size(), format, args);
        text.append(line.data(), (size_t)size);
    }
    va_end(args);
}

void hex(std::string& text, const uint8_t* p, size_t n)
{
    const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++)
    {
        text.push_back(digits[p[i] >> 4]);
        text.push_back(digits[p[i] & 15]);
    }
}
}   // namespace

void Session::BeginMatch(uint32_t startInUs, uint8_t delayTicks, uint64_t now)
{
    m_goReceived = true;
    m_startUs = now + startInUs;
    m_delay = delayTicks;
    m_delayAuto = m_settings.delayTicks == 0;
    m_delayGrewUs = now;
    uint8_t mask = 0;
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].present)
            mask = (uint8_t)(mask | (1 << m));
    m_lock.Begin(m_serial, m_self, mask, m_desc.slotMachine);
    m_lock.SetDelay(m_delay);
    // Ticks before the first generated one carry the local pads' current connected bits.
    SampleLocalPads(m_ports);
    m_lock.StartGeneration();
    m_loopNext = 0;
    m_stamps.clear();
    std::memset(&m_barrier, 0, sizeof m_barrier);
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        m_barrierDoneKind[m] = 0;
        m_barrierDoneTick[m] = kNoTick;
    }
    m_goals = 0;
    m_stalls.clear();
    m_stallStarts.clear();
    m_stalled = false;
    m_advance = 0;
    m_advanceUs = now;
    m_slowSinceUs = 0;
    m_slowMachine = -1;
    m_clockRate = 1.0f;
    m_rate = (double)m_timeScale / kTickUs;
    m_scheduleRunning = true;
    Reanchor(0.0, m_startUs);
    m_edgePending = false;
    m_componentsCount = 0;
    m_committed = 0;
    for (int i = 0; i < kPendingRing; i++)
        m_pending[i].tick = kNoTick;
    for (size_t i = 0; i < m_links.size(); i++)
    {
        Link& l = *m_links[i];
        for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
            l.peerHas[m] = 0;
        l.componentsLeft = 0;
        l.matchGapUs = 0;
    }
    Record("go serial %u delay %d start-in-us %u machines %x", (unsigned)m_serial, m_delay,
           (unsigned)startInUs, (unsigned)mask);
    RoomChanged();
}

void Session::EndMatchState()
{
    m_lock.Clear();
    m_scheduleRunning = false;
    m_goReceived = false;
    m_matchOver = false;
    m_endTick = kNoTick;
    m_stamps.clear();
    std::memset(&m_barrier, 0, sizeof m_barrier);
    m_stalled = false;
    m_clockRate = 1.0f;
    m_problem = ONLINE_PROBLEM_NONE;
    m_problemMachine = -1;
    m_problemSeconds = 0;
    m_opponentsLeft = false;
    m_opponentsAnswered = false;
}

bool Session::SoloMatch() const
{
    if (!m_lock.Active())
        return false;
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m != m_self && m_lock.InMatch(m) && !m_lock.Dropped(m))
            return false;
    return true;
}

double Session::SchedulePos(uint64_t now) const
{
    if (!m_scheduleRunning)
        return m_anchorPos;
    const double pos = m_anchorPos + (double)(int64_t)(now - m_anchorUs) * m_rate;
    // The schedule never runs more than a tick past the loop: a held loop holds it.
    const double cap = (double)m_loopNext + 0.999;
    return pos < cap ? pos : cap;
}

void Session::Reanchor(double pos, uint64_t now)
{
    m_anchorPos = pos;
    m_anchorUs = now;
}

void Session::RunSchedule(uint64_t now)
{
    if (!m_scheduleRunning || !m_lock.Active() || m_matchOver || m_desync != ONLINE_DESYNC_NONE)
        return;
    const double pos = SchedulePos(now);
    if (pos < 0)
        return;
    // The input for tick k + D is produced when the schedule reaches tick k.
    const uint32_t k = (uint32_t)pos;
    if (m_lock.Generate(k + (uint32_t)m_delay) == kNoTick)
        return;
    for (size_t i = 0; i < m_links.size(); i++)
        if (m_links[i]->used && m_links[i]->machine >= 0)
            m_links[i]->sendNow = true;
}

uint32_t Session::Stamped(uint32_t tick, OnlineTickActions* out) const
{
    std::memset(out, 0, sizeof *out);
    for (size_t i = 0; i < m_stamps.size(); i++)
    {
        if (m_stamps[i].tick != tick)
            continue;
        const OnlineTickActions& a = m_stamps[i].actions;
        out->actions |= a.actions;
        if (a.actions & ONLINE_ACTION_RESUME)
            out->barrier = a.barrier;
        if (a.actions & ONLINE_ACTION_RESEED)
            out->seed = a.seed;
        out->cpuSlots = (uint8_t)(out->cpuSlots | a.cpuSlots);
    }
    return out->actions;
}

void Session::Stamp(uint32_t tick, uint32_t actions, int barrier, uint32_t seed, uint8_t cpuSlots)
{
    StampedAction s;
    s.tick = tick;
    std::memset(&s.actions, 0, sizeof s.actions);
    s.actions.actions = actions;
    s.actions.barrier = barrier;
    s.actions.seed = seed;
    s.actions.cpuSlots = cpuSlots;
    m_stamps.push_back(s);
    if (actions & ONLINE_ACTION_END_MATCH)
        m_endTick = tick;
}

void Session::OnTiming(Link& link, const TimingChunk& t, uint64_t now)
{
    link.echoValid = true;
    link.echoUs = t.sendUs;
    link.echoAtUs = now;
    if (t.echoUs != 0)
    {
        const uint32_t rtt = (uint32_t)now - t.echoUs - t.echoHeldUs;
        if (rtt < 10000000u)
            link.stats.OnRtt(rtt, now);
    }
    link.timingValid = true;
    link.peerTick = t.tick;
    link.peerConfirmed = t.confirmedTick;
    link.timingAtUs = now;
}

void Session::OnInput(Link& link, const InputChunk& c)
{
    if (c.matchSerial != m_lock.Serial())
        return;
    // A machine accepts input only for its own slots: a guest sends its own, the host forwards.
    const bool allowed =
        IsHost() ? c.machine == link.machine : (link.machine == 0 && c.machine != m_self);
    if (!allowed || !m_lock.InMatch(c.machine))
    {
        link.malformed++;
        return;
    }
    // No machine gets far past this one's tick, since its loop needs this machine's input.
    const uint32_t limit = m_loopNext + 64;
    if (c.firstTick >= limit)
        return;
    const int count = c.count < (int)(limit - c.firstTick) ? c.count : (int)(limit - c.firstTick);
    if (m_lock.Add(c.machine, c.firstTick, count, c.records) < 0)
        link.malformed++;
}

void Session::OnHashes(Link& link, const HashChunk& c)
{
    if (c.matchSerial != m_lock.Serial())
        return;
    const bool allowed =
        IsHost() ? c.machine == link.machine : (link.machine == 0 && c.machine != m_self);
    if (!allowed || !m_lock.InMatch(c.machine))
    {
        link.malformed++;
        return;
    }
    for (int i = 0; i < c.count; i++)
    {
        const uint32_t tick = c.firstTick + (uint32_t)i;
        if (tick > m_loopNext + 64 + (uint32_t)m_delay || tick + 500 < m_loopNext)
            continue;
        uint32_t bad = kNoTick;
        if (m_lock.SetHash(c.machine, tick, c.hash[i], &bad))
        {
            RaiseDesync(FirstDifference(bad), m_self, NULL, false);
            return;
        }
    }
}

void Session::OnComponents(Link& link, const ComponentsChunk& c)
{
    if (c.matchSerial != m_lock.Serial())
        return;
    const bool allowed =
        IsHost() ? c.machine == link.machine : (link.machine == 0 && c.machine != m_self);
    if (!allowed || !m_lock.InMatch(c.machine))
    {
        link.malformed++;
        return;
    }
    const Lockstep::Components* newest = m_lock.ComponentsAt(c.machine, 0);
    if (newest != NULL && newest->tick == c.tick)
        return;
    m_lock.SetComponents(c.machine, c.tick, c.hash, c.count, NULL);
}

void Session::SampleLocalPads(const LocalPad ports[4])
{
    if (ports != m_ports)
        std::memcpy(m_ports, ports, sizeof m_ports);
    if (!m_lock.Active() || m_matchOver || m_self < 0)
        return;
    const uint64_t now = m_env->NowUs();
    // A press while the loop is held, before the start or at a barrier, belongs to no tick.
    const bool held = now < m_startUs || m_barrier.active;
    for (int i = 0; i < ONLINE_MAX_LOCAL_HUMANS; i++)
    {
        if (!m_local[i].used || !m_room[m_self].human[i].used)
            continue;
        const int slot = m_room[m_self].human[i].slot;
        const int port = m_local[i].localPort;
        if (slot < 0 || port < 0 || port > 3)
            continue;
        int h = -1;
        for (int k = 0; k < ONLINE_MAX_LOCAL_HUMANS; k++)
            if (m_lock.SlotOf(m_self, k) == slot)
                h = k;
        if (h < 0)
            continue;
        const LocalPad& p = m_ports[port];
        m_lock.Sample(h, p.pad, p.connected, !held);
        const uint16_t buttons = (uint16_t)(p.pad.buttons & PORT_INPUT_BUTTONS);
        if ((buttons & ~m_edgeButtons[port]) != 0 && !m_edgePending && !held)
        {
            m_edgePending = true;
            m_edgeUs = now;
            m_edgeTick = m_lock.Have(m_self);
        }
        m_edgeButtons[port] = buttons;
    }
}

int Session::RenderSlots(SlotInput out[ONLINE_MAX_SLOTS], int localPort[ONLINE_MAX_SLOTS])
{
    std::memset(out, 0, sizeof(SlotInput) * ONLINE_MAX_SLOTS);
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
        localPort[s] = -1;
    if (!Active())
        return 0;
    SlotInput confirmed[ONLINE_MAX_SLOTS];
    bool haveConfirmed = false;
    if (m_lock.Active())
    {
        const uint32_t c = m_lock.Confirmed();
        const uint32_t newest = c == kNoTick ? m_loopNext : c;
        if (newest > 0)
        {
            m_lock.Read(newest - 1, confirmed);
            haveConfirmed = true;
        }
    }
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
    {
        out[s].kind = (uint8_t)SlotKind(s);
        switch (out[s].kind)
        {
        case ONLINE_SLOT_LOCAL:
            localPort[s] = SlotLocalPort(s);
            out[s].connected = localPort[s] >= 0 && m_ports[localPort[s]].connected;
            break;
        case ONLINE_SLOT_REMOTE:
            out[s].connected = 1;
            if (haveConfirmed)
                out[s].pad = confirmed[s].pad;
            break;
        case ONLINE_SLOT_CPU: out[s].connected = 1; break;
        default:              break;
        }
    }
    return 1;
}

int Session::SlotKind(int slot) const
{
    if (slot < 0 || slot >= ONLINE_MAX_SLOTS)
        return ONLINE_SLOT_EMPTY;
    if (!Active() || m_self < 0)
        return ONLINE_SLOT_LOCAL;
    const int owner = m_lock.Active() ? m_lock.OwnerOf(slot) : m_desc.slotMachine[slot];
    if (owner < 0)
        return ONLINE_SLOT_EMPTY;
    if (m_lock.Active() && m_lock.Dropped(owner) && m_loopNext > m_lock.DropTick(owner))
        return ONLINE_SLOT_CPU;
    return owner == m_self ? ONLINE_SLOT_LOCAL : ONLINE_SLOT_REMOTE;
}

int Session::SlotLocalPort(int slot) const
{
    if (slot < 0 || slot >= ONLINE_MAX_SLOTS)
        return -1;
    if (!Active() || m_self < 0)
        return slot;
    for (int i = 0; i < ONLINE_MAX_LOCAL_HUMANS; i++)
        if (m_local[i].used && m_room[m_self].human[i].used && m_room[m_self].human[i].slot == slot)
            return m_local[i].localPort;
    return -1;
}

const char* Session::SlotName(int slot) const
{
    if (slot < 0 || slot >= ONLINE_MAX_SLOTS || !Active())
        return "";
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m_room[m].present)
            for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
                if (m_room[m].human[h].used && m_room[m].human[h].slot == slot)
                    return m_room[m].human[h].name;
    return "";
}

void Session::LoadProgress(int percent)
{
    if (m_phase != ONLINE_PHASE_LOADING || m_self < 0)
        return;
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    RoomMachine& me = m_room[m_self];
    if (me.loadState == ONLINE_LOAD_READY)
        return;
    me.loadState = ONLINE_LOAD_LOADING;
    me.loadPercent = (uint8_t)percent;
    if (IsHost())
    {
        RoomChanged();
        return;
    }
    if (m_loadSentState == ONLINE_LOAD_LOADING && percent / 10 == m_loadSentPercent / 10)
        return;
    m_loadSentState = ONLINE_LOAD_LOADING;
    m_loadSentPercent = percent;
    Message m;
    std::memset(&m, 0, sizeof m);
    m.u.loadProgress.matchSerial = m_serial;
    m.u.loadProgress.loadState = ONLINE_LOAD_LOADING;
    m.u.loadProgress.percent = (uint8_t)percent;
    SendReliableTo(0, MSG_LOAD_PROGRESS, m);
}

void Session::MatchLoaded()
{
    if (m_phase != ONLINE_PHASE_LOADING || m_self < 0)
        return;
    RoomMachine& me = m_room[m_self];
    if (me.loadState == ONLINE_LOAD_READY)
        return;
    me.loadState = ONLINE_LOAD_READY;
    me.loadPercent = 100;
    if (IsHost())
    {
        RoomChanged();
        HostCheckLoaded();
        return;
    }
    m_loadSentState = ONLINE_LOAD_READY;
    m_loadSentPercent = 100;
    Message m;
    std::memset(&m, 0, sizeof m);
    m.u.loadProgress.matchSerial = m_serial;
    m.u.loadProgress.loadState = ONLINE_LOAD_READY;
    m.u.loadProgress.percent = 100;
    SendReliableTo(0, MSG_LOAD_PROGRESS, m);
}

int Session::MayBeginMatch()
{
    if (!m_goReceived || (m_phase != ONLINE_PHASE_LOADING && m_phase != ONLINE_PHASE_MATCH))
        return 0;
    const uint64_t now = m_env->NowUs();
    if (now < m_startUs)
        return 0;
    if (m_phase == ONLINE_PHASE_LOADING)
    {
        m_phase = ONLINE_PHASE_MATCH;
        RoomChanged();
    }
    return 1;
}

void Session::MatchEnded()
{
    if (!m_lock.Active() || m_matchOver)
        return;
    const uint64_t now = m_env->NowUs();
    m_matchOver = true;
    m_endTick = m_loopNext > 0 ? m_loopNext - 1 : 0;
    m_scheduleRunning = false;
    m_phase = ONLINE_PHASE_POST_MATCH;
    if (IsHost())
        m_voteDeadlineUs = now + 60000000;
    Record("end tick %u", m_endTick);
    for (size_t i = 0; i < m_links.size(); i++)
    {
        const Link& l = *m_links[i];
        if (!l.used || l.machine < 0)
            continue;
        char line[160];
        std::snprintf(line, sizeof line,
                      "machine %d: round trip %u ms, jitter %u ms, loss %d%%, longest gap %u ms",
                      l.machine, l.stats.HasRtt() ? l.stats.SrttUs() / 1000 : 0,
                      l.stats.JitterUs(now) / 1000, l.stats.LossPercent(now),
                      (unsigned)(l.matchGapUs / 1000));
        m_env->Log(line);
    }
    RoomChanged();
}

int Session::TickInput(uint32_t tick, SlotInput out[ONLINE_MAX_SLOTS])
{
    if (!m_lock.Active() || m_desync != ONLINE_DESYNC_NONE)
        return 0;
    const uint64_t now = m_env->NowUs();
    // The loop wanting tick k means the schedule reached it, whatever the network thread saw.
    if (!m_matchOver && m_lock.Generate(tick + (uint32_t)m_delay) != kNoTick)
    {
        for (size_t i = 0; i < m_links.size(); i++)
            if (m_links[i]->used && m_links[i]->machine >= 0)
                m_links[i]->sendNow = true;
        SendDue(now);
    }
    // Input is guessed as far as snapshots reach; a tick run again always goes through.
    const bool rerun = tick < m_loopNext;
    if (!rerun && tick >= ConfirmedTick() + kMaxAhead)
    {
        NoteStall(now, true);
        Reanchor((double)tick, now);
        return 0;
    }
    m_lock.ReadPredicted(tick, out);
    if (rerun)
        return 1;
    // Between ticks: every tick before this one is finished, components and all.
    CommitConfirmed();
    NoteStall(now, false);
    m_loopNext = tick + 1;
    if (SchedulePos(now) < (double)tick)
        Reanchor((double)tick, now);
    if (m_edgePending && tick >= m_edgeTick)
    {
        const int ms = (int)((Since(now, m_edgeUs)) / 1000);
        m_inputToEffectMs = m_inputToEffectMs == 0 ? ms : (m_inputToEffectMs * 3 + ms) / 4;
        m_edgePending = false;
    }
    return 1;
}

uint32_t Session::RollbackTo()
{
    if (!m_lock.Active())
        return kNoTick;
    const uint32_t t = m_lock.TakeMispredicted();
    // A guess for a tick the loop has not reached yet needs nothing undone.
    return t != kNoTick && t < m_loopNext ? t : kNoTick;
}

uint32_t Session::SettledTick() const
{
    const uint32_t confirmed = ConfirmedTick();
    const uint32_t wrong = m_lock.Mispredicted();
    return wrong != kNoTick && wrong < confirmed ? wrong : confirmed;
}

void Session::Rewind(uint32_t tick)
{
    if (!m_lock.Active() || tick >= m_loopNext || tick < m_committed)
        return;
    for (uint32_t t = tick; t < m_loopNext; t++)
        if (m_pending[t % kPendingRing].tick == t)
            m_pending[t % kPendingRing].tick = kNoTick;
    m_loopNext = tick;
}

uint32_t Session::ConfirmedTick() const
{
    const uint32_t c = m_lock.Confirmed();
    return c == kNoTick ? m_loopNext : c;
}

void Session::TickHash(uint32_t tick, uint64_t hash)
{
    if (!m_lock.Active() || tick < m_committed)
        return;
    // The tick's components, if it takes any, come after its hash.
    PendingHash& p = m_pending[tick % kPendingRing];
    p.tick = tick;
    p.hash = hash;
    p.components = 0;
    if (tick < 12 && std::getenv("STRIKERS_ROLLBACK_LOG") != NULL)
    {
        char line[96];
        std::snprintf(line, sizeof line, "[rollback] pending t=%u hash %016llx", tick,
                      (unsigned long long)hash);
        m_env->Log(line);
    }
}

void Session::TickComponents(uint32_t tick, const uint64_t* hashes, const char* const* names,
                             int count)
{
    if (!m_lock.Active() || count < 1 || tick < m_committed)
        return;
    if (count > kMaxComponents)
        count = kMaxComponents;
    PendingHash& p = m_pending[tick % kPendingRing];
    if (p.tick != tick)
        return;
    p.components = count;
    p.names = names;
    std::memcpy(p.component, hashes, sizeof(uint64_t) * (size_t)count);
}

void Session::CommitConfirmed()
{
    // A tick waiting to run again on input that arrived since keeps its old hash out.
    uint32_t confirmed = ConfirmedTick();
    const uint32_t wrong = m_lock.Mispredicted();
    if (wrong != kNoTick && wrong < confirmed)
        confirmed = wrong;
    while (m_committed < confirmed && m_committed < m_loopNext)
    {
        const PendingHash& p = m_pending[m_committed % kPendingRing];
        if (p.tick != m_committed)
            break;
        const uint32_t tick = m_committed++;
        if (tick < 12 && std::getenv("STRIKERS_ROLLBACK_LOG") != NULL)
        {
            char line[128];
            std::snprintf(line, sizeof line,
                          "[rollback] commit t=%u hash %016llx confirmed %u loop %u", tick,
                          (unsigned long long)p.hash, ConfirmedTick(), m_loopNext);
            m_env->Log(line);
        }
        uint32_t bad = kNoTick;
        if (m_lock.SetHash(m_self, tick, p.hash, &bad) && m_desync == ONLINE_DESYNC_NONE)
            RaiseDesync(FirstDifference(bad), m_self, NULL, false);
        static const bool logAll = std::getenv("STRIKERS_ROLLBACK_CHECK") != NULL;
        if (logAll && p.components > 0)
        {
            char line[1400];
            int at = std::snprintf(line, sizeof line, "[rollback] committed t=%u", tick);
            for (int i = 0; i < p.components && at < (int)sizeof line; i++)
                at += std::snprintf(line + at, sizeof line - at, " %s=%04llx", p.names[i],
                                    (unsigned long long)(p.component[i] & 0xFFFF));
            m_env->Log(line);
        }
        if (p.components > 0)
        {
            m_lock.SetComponents(m_self, tick, p.component, p.components, p.names);
            m_componentsTick = tick;
            m_componentsCount = p.components;
            std::memcpy(m_componentsHash, p.component, sizeof(uint64_t) * (size_t)p.components);
            for (size_t i = 0; i < m_links.size(); i++)
                if (m_links[i]->used)
                    m_links[i]->componentsLeft = 3;
        }
        for (size_t i = 0; i < m_stamps.size();)
        {
            if (m_stamps[i].tick + 1 < tick)
            {
                m_stamps.erase(m_stamps.begin() + (long)i);
                continue;
            }
            i++;
        }
        if (m_endTick != kNoTick && tick >= m_endTick && !m_matchOver)
            MatchEnded();
    }
}

int Session::TickActions(uint32_t tick, OnlineTickActions* out)
{
    if (!m_lock.Active())
    {
        std::memset(out, 0, sizeof *out);
        return 0;
    }
    return Stamped(tick, out) != 0;
}

int Session::PresentationSkip(uint32_t presentation, bool pressed)
{
    if (!m_lock.Active() || presentation == 0 || presentation > m_loopNext + 64)
        return 0;
    if (pressed && presentation > m_skipPresentation)
    {
        m_skipPresentation = presentation;
        Message m = {};
        m.u.skip.matchSerial = m_serial;
        m.u.skip.presentation = presentation;
        if (IsHost())
            BroadcastReliable(MSG_SKIP, m);
        else
            SendReliableTo(0, MSG_SKIP, m);
    }
    return presentation == m_skipPresentation;
}

void Session::BarrierReached(int kind, uint32_t tick)
{
    if (!m_lock.Active() || m_matchOver)
        return;
    if (m_barrier.active && m_barrier.kind == kind && m_barrier.tick == tick)
        return;
    if (m_barrier.resumeKnown && m_barrier.tick == tick)
        return;
    std::memset(&m_barrier, 0, sizeof m_barrier);
    m_barrier.active = true;
    m_barrier.kind = kind;
    m_barrier.tick = tick;
    Reanchor((double)tick, m_env->NowUs());
    Record("barrier kind %d tick %u", kind, tick);
    HostCheckBarrier(m_env->NowUs());
}

void Session::BarrierDone(int kind)
{
    if (!m_barrier.active || kind != m_barrier.kind || m_barrier.doneSent)
        return;
    const uint64_t now = m_env->NowUs();
    m_barrier.doneSent = true;
    if (IsHost())
    {
        m_barrierDoneKind[0] = kind;
        m_barrierDoneTick[0] = m_barrier.tick;
        HostCheckBarrier(now);
        return;
    }
    if (SoloMatch())
    {
        // Nobody left to wait for: this machine names its own resume.
        MsgResume r;
        std::memset(&r, 0, sizeof r);
        r.matchSerial = m_serial;
        r.kind = (uint8_t)kind;
        r.delayTicks = (uint8_t)m_delay;
        r.actions = resume_actions(kind);
        r.tick = m_barrier.tick;
        r.seed = ReseedFor(m_goals + (kind == 2 ? 1u : 0u));
        ApplyResume(r, now);
        return;
    }
    Message m;
    std::memset(&m, 0, sizeof m);
    m.u.barrierDone.matchSerial = m_serial;
    m.u.barrierDone.kind = (uint8_t)kind;
    m.u.barrierDone.tick = m_barrier.tick;
    SendReliableTo(0, MSG_BARRIER_DONE, m);
}

void Session::HostCheckBarrier(uint64_t now)
{
    if (!IsHost() || !m_barrier.active || !m_barrier.doneSent || m_barrier.resumeKnown)
        return;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_lock.InMatch(m) || m_lock.Dropped(m))
            continue;
        if (m_barrierDoneKind[m] != m_barrier.kind || m_barrierDoneTick[m] != m_barrier.tick)
            return;
    }
    uint64_t lead = 2ull * WorstRttUs() + 100000;
    if (lead < 100000)
        lead = 100000;
    if (lead > 1000000)
        lead = 1000000;
    if (SoloMatch())
        lead = 0;
    uint8_t delay = ExpectedDelay(now);
    MsgResume r;
    std::memset(&r, 0, sizeof r);
    r.matchSerial = m_serial;
    r.kind = (uint8_t)m_barrier.kind;
    r.delayTicks = delay;
    r.actions = resume_actions(m_barrier.kind);
    r.tick = m_barrier.tick;
    r.startInUs = (uint32_t)lead;
    r.seed = ReseedFor(m_goals + (m_barrier.kind == 2 ? 1u : 0u));
    Message msg;
    std::memset(&msg, 0, sizeof msg);
    msg.u.resume = r;
    for (int m = 1; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_lock.InMatch(m) || m_lock.Dropped(m))
            continue;
        Link* l = MachineLink(m);
        if (l != NULL)
            Queue(*l, MSG_RESUME, msg, now + lead);
    }
    ApplyResume(r, now + lead);
}

void Session::ApplyResume(const MsgResume& r, uint64_t startUs)
{
    // Only the barrier this machine holds resumes: every machine reports its own before the host
    // resumes it, and anything else would pile up stamped actions.
    if (!m_lock.Active() || !m_barrier.active || m_barrier.tick != r.tick || m_barrier.resumeKnown)
        return;
    m_barrier.resumeKnown = true;
    m_barrier.resumeUs = startUs;
    if (r.kind == 2)
        m_goals++;
    if (r.delayTicks != m_delay)
    {
        m_delay = r.delayTicks;
        m_lock.SetDelay(m_delay);
    }
    m_delayGrewUs = m_env->NowUs();
    m_stallStarts.clear();
    Stamp(r.tick, r.actions, r.kind, r.seed, 0);
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        m_barrierDoneKind[m] = 0;
        m_barrierDoneTick[m] = kNoTick;
    }
    Record("resume kind %d tick %u delay %d seed %08x", r.kind, (unsigned)r.tick, r.delayTicks,
           (unsigned)r.seed);
}

int Session::MayRunTick(uint32_t tick)
{
    if (!m_lock.Active())
        return 1;
    if (m_desync != ONLINE_DESYNC_NONE)
        return 0;
    if (m_endTick != kNoTick && tick > m_endTick)
        return 0;
    if (m_barrier.active && tick >= m_barrier.tick)
    {
        const uint64_t now = m_env->NowUs();
        if (m_barrier.resumeKnown && now >= m_barrier.resumeUs)
        {
            m_barrier.active = false;
            Reanchor((double)m_barrier.tick, m_barrier.resumeUs);
            return 1;
        }
        Reanchor((double)tick, now);
        return 0;
    }
    return 1;
}

uint32_t Session::ReseedFor(uint32_t goals) const
{
    uint8_t b[12];
    Put32(b, m_desc.seed);
    Put32(b + 4, goals);
    Put32(b + 8, 0);
    return fnv32(b, sizeof b);
}

uint8_t Session::SlotsOf(int machine) const
{
    uint8_t bits = 0;
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
        if (m_lock.OwnerOf(s) == machine)
            bits = (uint8_t)(bits | (1 << s));
    return bits;
}

void Session::DropMachine(int machine)
{
    if (!m_lock.Active() || !m_lock.InMatch(machine) || m_lock.Dropped(machine) ||
        machine == m_self)
        return;
    const uint32_t tick = m_lock.Have(machine);
    const uint8_t cpu = SlotsOf(machine);
    if (IsHost())
    {
        Message m;
        std::memset(&m, 0, sizeof m);
        m.u.drop.matchSerial = m_serial;
        m.u.drop.machine = (uint8_t)machine;
        m.u.drop.cpuSlots = cpu;
        m.u.drop.tick = tick;
        for (int g = 1; g < ONLINE_MAX_MACHINES; g++)
        {
            if (g == machine || !m_lock.InMatch(g) || m_lock.Dropped(g))
                continue;
            Link* l = MachineLink(g);
            if (l != NULL)
                Queue(*l, MSG_DROP, m, 0);
        }
    }
    ApplyDrop(machine, tick, cpu);
    // A barrier waiting only on the dropped machine may end.
    HostCheckBarrier(m_env->NowUs());
    if (!IsHost() && SoloMatch() && m_barrier.active && m_barrier.doneSent &&
        !m_barrier.resumeKnown)
    {
        m_barrier.doneSent = false;
        BarrierDone(m_barrier.kind);
    }
}

void Session::ApplyDrop(int machine, uint32_t tick, uint8_t cpuSlots)
{
    if (!m_lock.Active() || !m_lock.InMatch(machine) || m_lock.Dropped(machine))
        return;
    m_lock.Drop(machine, tick);
    Stamp(tick, ONLINE_ACTION_SLOTS_TO_CPU, 0, 0, cpuSlots);
    m_room[machine].connected = false;
    Notice(ONLINE_NOTICE_DROPPED_TO_CPU, machine);
    Record("drop machine %d tick %u slots %x", machine, (unsigned)tick, (unsigned)cpuSlots);
    char line[96];
    std::snprintf(line, sizeof line, "machine %d dropped at tick %u", machine, (unsigned)tick);
    m_env->Log(line);
    CheckOpponents();
}

void Session::CheckOpponents()
{
    if (m_opponentsLeft)
        return;
    for (int side = 0; side < 2; side++)
    {
        bool mine = false;
        int theirs = 0;
        int gone = 0;
        for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
        {
            const int owner = m_lock.OwnerOf(s);
            if (owner < 0 || m_desc.padSide[s] < 0)
                continue;
            if (m_desc.padSide[s] == side && owner == m_self)
                mine = true;
            if (m_desc.padSide[s] != side)
            {
                theirs++;
                if (m_lock.Dropped(owner))
                    gone++;
            }
        }
        if (mine && theirs > 0 && gone == theirs)
            m_opponentsLeft = true;
    }
}

void Session::NoteStall(uint64_t now, bool stalled)
{
    if (stalled)
    {
        if (!m_stalled)
        {
            m_stalled = true;
            m_stallStartUs = now;
            m_stallStarts.push_back(now);
        }
        return;
    }
    if (!m_stalled)
        return;
    m_stalled = false;
    Stall s;
    s.startUs = m_stallStartUs;
    s.ms = (uint32_t)((Since(now, m_stallStartUs)) / 1000);
    m_stalls.push_back(s);
    while (!m_stalls.empty() && Since(now, m_stalls.front().startUs) > 60000000)
        m_stalls.erase(m_stalls.begin());
}

void Session::MatchTimers(uint64_t now)
{
    if (m_goReceived && m_phase == ONLINE_PHASE_LOADING && now >= m_startUs)
    {
        m_phase = ONLINE_PHASE_MATCH;
        RoomChanged();
    }
    if (!m_lock.Active())
        return;
    if (!IsHost() && m_phase == ONLINE_PHASE_POST_MATCH && MachineLink(0) == NULL)
    {
        Fail(ONLINE_ERR_HOST_LEFT, "The host left the room.");
        return;
    }
    if (m_phase != ONLINE_PHASE_MATCH || m_matchOver || m_barrier.active || now < m_startUs)
    {
        m_advanceUs = now;
        return;
    }

    // Advance against the slowest machine: this machine's tick less each peer's, projected to now.
    double worst = 0;
    int worstMachine = -1;
    for (size_t i = 0; i < m_links.size(); i++)
    {
        const Link& l = *m_links[i];
        if (!l.used || l.machine < 0 || !l.timingValid ||
            Since(now, l.timingAtUs) > kMaxTimingAgeUs)
            continue;
        if (!m_lock.InMatch(l.machine) || m_lock.Dropped(l.machine))
            continue;
        const double oneWay = l.stats.HasRtt() ? l.stats.SrttUs() / 2.0 : 0.0;
        const double peer = (double)l.peerTick +
                            ((double)(Since(now, l.timingAtUs)) + oneWay) * m_timeScale / kTickUs;
        const double adv = (double)m_loopNext - peer;
        if (worstMachine < 0 || adv > worst)
        {
            worst = adv;
            worstMachine = l.machine;
        }
    }
    if (worstMachine >= 0)
    {
        double a = (double)(Since(now, m_advanceUs)) / 1e6;
        if (a > 1)
            a = 1;
        m_advance += (worst - m_advance) * a;
        if (m_advance > 1.0)
        {
            if (m_slowSinceUs == 0 || m_slowMachine != worstMachine)
            {
                m_slowSinceUs = now;
                m_slowMachine = worstMachine;
            }
        }
        else
            m_slowSinceUs = 0;
    }
    m_advanceUs = now;

    // Auto delay grows by one tick after 2 stalls within 5 s, at most once every 10 s.
    while (!m_stallStarts.empty() && Since(now, m_stallStarts.front()) > kStallWindowUs)
        m_stallStarts.erase(m_stallStarts.begin());
    if (IsHost() && m_delayAuto && m_delay < ONLINE_MAX_DELAY_TICKS && m_stallStarts.size() >= 2 &&
        Since(now, m_delayGrewUs) >= kDelayGrowEveryUs)
    {
        m_delay++;
        m_lock.SetDelay(m_delay);
        m_delayGrewUs = now;
        m_stallStarts.clear();
        Message m;
        std::memset(&m, 0, sizeof m);
        m.u.serialByte.matchSerial = m_serial;
        m.u.serialByte.value = (uint8_t)m_delay;
        BroadcastReliable(MSG_DELAY, m);
        Record("delay tick %u ticks %d", m_loopNext, m_delay);
    }
}

float Session::ClockRate()
{
    float rate = 1.0f;
    const uint64_t now = m_env->NowUs();
    if (m_lock.Active() && m_phase == ONLINE_PHASE_MATCH && !m_matchOver && !m_barrier.active &&
        m_desync == ONLINE_DESYNC_NONE && now >= m_startUs)
    {
        if (m_advance > 0.5)
            rate = 0.995f;
        else if (m_advance < -0.5)
            rate = 1.01f;
        // Frame stretching: slow down near the guess limit rather than freeze.
        const uint32_t limit = ConfirmedTick() + kMaxAhead;
        const int missing = m_loopNext + 1 >= limit ? m_lock.Missing(ConfirmedTick()) : -1;
        if (missing >= 0)
        {
            const Link* l = MachineLink(IsHost() ? missing : 0);
            if (l != NULL && l->timingValid)
            {
                const double oneWay = l->stats.HasRtt() ? l->stats.SrttUs() / 2.0 : 0.0;
                const double peer =
                    (double)l->peerTick +
                    ((double)(Since(now, l->timingAtUs)) + oneWay) * m_timeScale / kTickUs;
                const double sentAgo = (peer - ((double)limit - kMaxAhead - m_delay)) * kTickUs;
                if (sentAgo >= 0 && sentAgo < (double)kStretchWithinUs)
                    rate = 0.75f;
            }
        }
    }
    if (rate != m_clockRate)
    {
        if (m_scheduleRunning)
            Reanchor(SchedulePos(now), now);
        m_clockRate = rate;
        m_rate = (double)m_clockRate * m_timeScale / kTickUs;
    }
    return rate;
}

void Session::UpdateProblem(uint64_t now)
{
    // The problem indicator, worst first.
    int problem = ONLINE_PROBLEM_NONE;
    int machine = -1;
    float seconds = 0;
    if (m_lock.Active() && m_phase == ONLINE_PHASE_MATCH && m_stalled && !m_matchOver)
    {
        const int missing = m_lock.Missing(ConfirmedTick());
        const Link* l = missing >= 0 ? MachineLink(IsHost() ? missing : 0) : NULL;
        if (l != NULL && Since(now, l->lastHeardUs) >= kWaitingAfterUs)
        {
            problem = ONLINE_PROBLEM_WAITING;
            machine = missing;
            const uint64_t silent = Since(now, l->lastHeardUs);
            seconds = silent >= kDropAfterUs ? 0.0f : (float)(kDropAfterUs - silent) / 1e6f;
        }
        else if (l != NULL && Since(now, m_stallStartUs) >= kWaitingAfterUs)
        {
            // Heard but sending no input: its game is frozen, and goes quiet after kLoopGiveUpUs.
            problem = ONLINE_PROBLEM_WAITING;
            machine = missing;
            const uint64_t waited = Since(now, m_stallStartUs);
            const uint64_t limit = kLoopGiveUpUs + kDropAfterUs;
            seconds = waited >= limit ? 0.0f : (float)(limit - waited) / 1e6f;
        }
    }
    if (problem == ONLINE_PROBLEM_NONE && m_phase == ONLINE_PHASE_LOADING &&
        Since(now, m_loadingSinceUs) >= kSlowLoadingUs)
    {
        int slowest = -1;
        for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
            if (m_room[m].present && m_room[m].loadState != ONLINE_LOAD_READY &&
                (slowest < 0 || m_room[m].loadPercent < m_room[slowest].loadPercent))
                slowest = m;
        if (slowest >= 0)
        {
            problem = ONLINE_PROBLEM_SLOW_LOADING;
            machine = slowest;
            seconds = (float)(Since(now, m_loadingSinceUs)) / 1e6f;
        }
    }
    if (problem == ONLINE_PROBLEM_NONE && m_slowSinceUs != 0 &&
        Since(now, m_slowSinceUs) >= kSlowDeviceUs)
    {
        problem = ONLINE_PROBLEM_SLOW_DEVICE;
        machine = m_slowMachine;
    }
    if (problem == ONLINE_PROBLEM_NONE &&
        (m_phase == ONLINE_PHASE_MATCH || m_phase == ONLINE_PHASE_ROOM))
    {
        int worst = ONLINE_PROBLEM_NONE;
        for (size_t i = 0; i < m_links.size(); i++)
        {
            const Link& l = *m_links[i];
            if (!l.used || l.machine < 0)
                continue;
            int p = ONLINE_PROBLEM_NONE;
            if (l.stats.LossPercent(now) > 5)
                p = ONLINE_PROBLEM_PACKET_LOSS;
            else if (l.stats.JitterUs(now) > 20000)
                p = ONLINE_PROBLEM_UNSTABLE;
            else if (l.stats.HasRtt() && l.stats.SrttUs() > 150000)
                p = ONLINE_PROBLEM_HIGH_LATENCY;
            if (p != ONLINE_PROBLEM_NONE && (worst == ONLINE_PROBLEM_NONE || p > worst))
            {
                worst = p;
                machine = l.machine;
            }
        }
        problem = worst;
    }
    if (problem != ONLINE_PROBLEM_NONE)
    {
        m_problem = problem;
        m_problemMachine = machine;
        m_problemSeconds = seconds;
        m_problemHoldUs = now + kProblemHoldUs;
    }
    else if (now >= m_problemHoldUs)
    {
        m_problem = ONLINE_PROBLEM_NONE;
        m_problemMachine = -1;
        m_problemSeconds = 0;
    }
    else if (m_problem == ONLINE_PROBLEM_WAITING)
    {
        // The input came; a waiting message has nothing left to count down.
        m_problem = ONLINE_PROBLEM_NONE;
        m_problemMachine = -1;
        m_problemSeconds = 0;
    }
}

uint32_t Session::FirstDifference(uint32_t tick) const
{
    uint32_t first = tick;
    for (uint32_t k = tick; k > 0 && tick - k < (uint32_t)Lockstep::kHashRing - 1;)
    {
        k--;
        bool compared = false;
        bool differ = false;
        uint64_t mine = 0;
        if (!m_lock.HashOf(m_self, k, &mine))
            break;
        for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        {
            uint64_t theirs = 0;
            if (m == m_self || !m_lock.InMatch(m) || !m_lock.HashOf(m, k, &theirs))
                continue;
            compared = true;
            if (theirs != mine)
                differ = true;
        }
        if (!compared || !differ)
            break;
        first = k;
    }
    return first;
}

void Session::RaiseDesync(uint32_t tick, int reporter, const uint64_t* hashes, bool fromPeer)
{
    if (m_desync != ONLINE_DESYNC_NONE || !m_lock.Active())
        return;
    m_desync = ONLINE_DESYNC_STOPPED;
    m_desyncTick = tick;
    m_desyncCount++;
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        uint64_t h = 0;
        if (hashes != NULL && hashes[m] != 0)
            h = hashes[m];
        else
            m_lock.HashOf(m, tick, &h);
        m_desyncHashes[m] = h;
    }

    // The first differing component, from the newest snapshot both hold at or after the tick.
    m_desyncComponent[0] = '\0';
    for (int i = Lockstep::kComponentRing - 1; i >= 0 && m_desyncComponent[0] == '\0'; i--)
    {
        const Lockstep::Components* mine = m_lock.ComponentsAt(m_self, i);
        if (mine == NULL || mine->tick < tick || mine->names == NULL)
            continue;
        for (int m = 0; m < ONLINE_MAX_MACHINES && m_desyncComponent[0] == '\0'; m++)
        {
            if (m == m_self)
                continue;
            for (int j = 0; j < Lockstep::kComponentRing; j++)
            {
                const Lockstep::Components* theirs = m_lock.ComponentsAt(m, j);
                if (theirs == NULL || theirs->tick != mine->tick)
                    continue;
                const int n = mine->count < theirs->count ? mine->count : theirs->count;
                for (int c = 0; c < n; c++)
                    if (mine->hash[c] != theirs->hash[c])
                    {
                        std::snprintf(m_desyncComponent, sizeof m_desyncComponent, "%s",
                                      mine->names[c] != NULL ? mine->names[c] : "?");
                        break;
                    }
                break;
            }
        }
    }

    Message msg;
    std::memset(&msg, 0, sizeof msg);
    msg.u.desync.matchSerial = m_serial;
    msg.u.desync.tick = tick;
    msg.u.desync.reporter =
        (uint8_t)(reporter >= 0 && reporter < ONLINE_MAX_MACHINES ? reporter : m_self);
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        msg.u.desync.hash[m] = m_desyncHashes[m];
    if (IsHost())
        BroadcastReliable(MSG_DESYNC, msg);
    else if (!fromPeer)
        SendReliableTo(0, MSG_DESYNC, msg);
    m_reportPending = true;
    Record("desync tick %u reporter %d", (unsigned)tick, reporter);
    char line[96];
    std::snprintf(line, sizeof line, "out of sync at tick %u", (unsigned)tick);
    m_env->Log(line);
}

void Session::Record(const char* fmt, ...)
{
    if (m_events.size() >= kMaxEvents)
        return;
    Event e;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(e.text, sizeof e.text, fmt, ap);
    va_end(ap);
    m_events.push_back(e);
}

void Session::WriteReport()
{
    m_reportPending = false;
    m_reportSerial = m_serial;
    std::string& text = m_reportText;
    text.clear();
    append(text, "strikers-desync 1\n");
    append(text, "version %s\n", m_id.version);
    append(text, "build %08x protocol %u platform %s family %u data %08x\n", (unsigned)m_id.build,
           (unsigned)ONLINE_PROTOCOL_VERSION, PortOnlinePlatformName(m_id.platform),
           (unsigned)m_id.binaryFamily, (unsigned)m_id.dataFamily);
    append(text, "self %d serial %u machines %x\n", m_self, (unsigned)m_serial,
           (unsigned)m_lock.Machines());
    uint8_t desc[ONLINE_DESC_BYTES];
    PortOnlineDescEncode(&m_desc, desc);
    append(text, "desc ");
    hex(text, desc, sizeof desc);
    append(text, "\nfirst-tick %u\n", (unsigned)m_desyncTick);
    append(text, "component %s\n", m_desyncComponent[0] != '\0' ? m_desyncComponent : "-");
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
        if (m_lock.InMatch(m))
            append(text, "hash-at-first %d %016llx\n", m, (unsigned long long)m_desyncHashes[m]);
    for (size_t i = 0; i < m_events.size(); i++)
        append(text, "event %s\n", m_events[i].text);
    // Every machine's records from tick 0, a run of equal records on one line.
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_lock.InMatch(m))
            continue;
        const uint32_t have = m_lock.Have(m);
        uint32_t t = 0;
        while (t < have)
        {
            const uint8_t* rec = m_lock.Record(m, t);
            uint32_t run = 1;
            while (t + run < have && std::memcmp(m_lock.Record(m, t + run), rec, kRecordSize) == 0)
                run++;
            append(text, "input %d %u %u ", m, (unsigned)t, (unsigned)run);
            hex(text, rec, kRecordSize);
            append(text, "\n");
            t += run;
        }
    }
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_lock.InMatch(m))
            continue;
        const uint32_t newest = m_lock.NewestHash(m);
        if (newest == kNoTick)
            continue;
        const uint32_t from =
            newest >= (uint32_t)Lockstep::kHashRing ? newest - Lockstep::kHashRing + 1 : 0;
        for (uint32_t t = from; t <= newest; t++)
        {
            uint64_t h;
            if (m_lock.HashOf(m, t, &h))
                append(text, "hash %d %u %016llx\n", m, (unsigned)t, (unsigned long long)h);
        }
    }
    for (int m = 0; m < ONLINE_MAX_MACHINES; m++)
    {
        if (!m_lock.InMatch(m))
            continue;
        for (int i = Lockstep::kComponentRing - 1; i >= 0; i--)
        {
            const Lockstep::Components* c = m_lock.ComponentsAt(m, i);
            if (c == NULL)
                continue;
            append(text, "components %d %u", m, (unsigned)c->tick);
            const Lockstep::Components* named = m_lock.ComponentsAt(m_self, 0);
            for (int k = 0; k < c->count; k++)
            {
                const char* name = named != NULL && named->names != NULL && k < named->count
                                       ? named->names[k]
                                       : NULL;
                append(text, " %s=%016llx", name != NULL ? name : "?",
                       (unsigned long long)c->hash[k]);
            }
            append(text, "\n");
        }
    }

    if (IsHost())
    {
        // The report is captured before the match state is cleared.
        Message m;
        std::memset(&m, 0, sizeof m);
        m.u.serialByte.matchSerial = m_serial;
        m.u.serialByte.value = ABORT_DESYNC;
        BroadcastReliable(MSG_ABORT, m);
        BackToRoom();
        m_desync = ONLINE_DESYNC_ENDED;
    }
}
std::string Session::TakeReport(uint32_t& serial)
{
    if (m_reportPending)
        WriteReport();
    std::string text;
    text.swap(m_reportText);
    serial = m_reportSerial;
    return text;
}

std::string Session::SaveReport(const std::string& text) const
{
    if (text.empty())
        return std::string();
    char path[ONLINE_PATH_MAX];
    FILE* file = m_env->ReportPath(path, sizeof path) ? std::fopen(path, "w") : NULL;
    if (file == NULL)
    {
        m_env->Log("no desync report: the user folder can't be written");
        return std::string();
    }
    const bool written = std::fwrite(text.data(), 1, text.size(), file) == text.size();
    const bool closed = std::fclose(file) == 0;
    if (!written || !closed)
    {
        m_env->Log("desync report write failed");
        return std::string();
    }
    return path;
}

void Session::ReportSaved(const std::string& path, uint32_t serial)
{
    if (!path.empty() && serial == m_serial)
        std::snprintf(m_desyncReport, sizeof m_desyncReport, "%s", path.c_str());
}
}   // namespace online
