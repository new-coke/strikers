// The lockstep tick loop, its RNG streams, clocks, events, barriers and nets.

#include "port/lockstep.h"
#include "port/determinism.h"
#include "port/online.h"
#include "port/region.h"
#include "port/rollback.h"
#include "port/simhash.h"

#include "Game/Ball.h"
#include "Game/Goalie.h"
#include "Game/DB/StatsTracker.h"
#include "NL/nlDLRing.h"
#include "Game/FixedUpdateTask.h"
#include "Game/FlickDetection.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/NisPlayer.h"
#include "Game/Render/NetMesh.h"
#include "Game/Sys/clock.h"
#include "Game/Sys/eventman.h"
#include "Game/Team.h"
#include "NL/globalpad.h"
#include "NL/nlConfig.h"
#include "NL/nlMath.h"
#include "NL/platpad.h"
#include "dolphin/os.h"
#include "dolphin/pad.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern void AIEventHandler(Event*, void*);
extern float g_fFixedUpdateTick;
extern float g_fSimulationTick;

extern "C" {
int g_simLockstep;
int PortRbProbeWantsDouble(unsigned tick);
void PortRbProbeDoubleSave(void);
void PortRbProbeDoubleRestore(void);
void PortRbProbeSpanHash(int pass, int j);
}

extern "C" void PortLockstepInit(void)
{
    const char* v = std::getenv("STRIKERS_LOCKSTEP");
    g_simLockstep = v != NULL && std::strcmp(v, "1") == 0;
}

namespace
{
unsigned int s_tick;
unsigned int s_match;
int s_inTick;
}   // namespace

extern "C" unsigned int PortSimTick(void) { return s_tick; }

extern "C" unsigned int PortSimMatch(void) { return s_match; }

extern "C" int PortSimInTick(void) { return s_inTick; }

extern "C" void PortSimTickBegin(void) { s_inTick = 1; }

extern "C" void PortSimTickEnd(void)
{
    s_inTick = 0;
    s_tick++;
}

namespace
{
// Nanoseconds of simulation time due and not yet run, so no float residue carries between matches.
long long s_dueNs;
// Set by BeginGame: the match's schedule starts at its first frame, not at the load before it.
int s_startSchedule;
// A time scale for the top of the next tick.
int s_scalePending;
float s_pendingScale;
PADStatus s_pads[4];
// Buttons down in any frame since local input last went into a tick.
u16 s_pressedSinceTick[4];
// The sticks' polar form as the last tick left it; a pad keeps its angle while its stick rests,
// and the frame's own pad update must not set it.
nlPolar s_polar[4][2];
// An online session feeds the input, holds the ticks and hears each tick's hash.
int s_online;
// Online, the session let this match's first tick run.
int s_begun;

long long tick_ns(void) { return std::llround((double)g_fFixedUpdateTick * 1e9); }

float pace_scale(void) { return s_scalePending ? s_pendingScale : FixedUpdateTask::mTimeScale; }

void nets_restore(void);
void nets_save(void);
bool online_may_run(unsigned int k);
void online_rollback(void);
void online_tick_top(unsigned int k);
void online_tick_end(unsigned int k);

void run_tick(unsigned int k)
{
    PADStatus frame[4];
    PortSimHeapScope simHeap;
    if (s_online && k < 12 && std::getenv("STRIKERS_ROLLBACK_LOG") != NULL)
        OSReport("[rollback] tick %u%s pads %04x %d,%d | %04x %d,%d\n", k,
                 PortRollbackResimulating() ? " again" : "", s_pads[0].button, s_pads[0].stickX,
                 s_pads[0].stickY, s_pads[1].button, s_pads[1].stickX, s_pads[1].stickY);

    PortSimTickBegin();
    nets_restore();
    if (s_scalePending)
    {
        FixedUpdateTask::mTimeScale = s_pendingScale;
        s_scalePending = 0;
    }
    const int scope = PortRngSetScope(PORT_RNG_SIM);
    if (s_online)
        online_tick_top(k);
    PortSimRunTickActions(k);

    std::memcpy(frame, PadStatus::s_Current, sizeof frame);
    std::memcpy(PadStatus::s_Current, s_pads, sizeof frame);
    nlPolar framePolar[4][2];
    for (int i = 0; i < 4; i++)
    {
        cGlobalPad* pad = cPadManager::GetPad(i);
        framePolar[i][0] = pad->m_polarAnalogLeft;
        framePolar[i][1] = pad->m_polarAnalogRight;
        pad->m_polarAnalogLeft = s_polar[i][0];
        pad->m_polarAnalogRight = s_polar[i][1];
    }
    UseFixedUpdatePad();
    UpdatePlatPad(g_fFixedUpdateTick);
    cPadManager::Update(g_fFixedUpdateTick);
    FlickDetection::Update();

    PortSimStepClocks(g_fSimulationTick);
    FixedUpdateTask::CallFixedUpdateTasks();
    PortSimDispatchEvents();
    PortSimCameraStep();
    nets_save();

    std::memcpy(PadStatus::s_Current, frame, sizeof frame);
    for (int i = 0; i < 4; i++)
    {
        cGlobalPad* pad = cPadManager::GetPad(i);
        s_polar[i][0] = pad->m_polarAnalogLeft;
        s_polar[i][1] = pad->m_polarAnalogRight;
        pad->m_polarAnalogLeft = framePolar[i][0];
        pad->m_polarAnalogRight = framePolar[i][1];
    }
    PortRngSetScope(scope);
    if (s_online)
        online_tick_end(k);
    PortSimTickEnd();
}
}   // namespace

extern "C" void PortLockstepRunTicks(float dt)
{
    const long long tick = tick_ns();
    for (int i = 0; i < 4; i++)
        s_pressedSinceTick[i] |= PadStatus::s_Current[i].button;

    // Online, the first tick waits for every machine to load and for the host's start time.
    if (s_online && !s_begun)
    {
        if (!PortOnlineMayBeginMatch())
        {
            FixedUpdateTask::mAccumulatedDeltaT = 0.0f;
            return;
        }
        s_begun = 1;
    }
    // The first frame of a match runs tick 0 alone; its dt is the time spent loading.
    if (s_startSchedule)
        s_startSchedule = 0;
    else
    {
        // Online, time sync and frame stretching scale the frame time the schedule gets.
        const double rate = s_online ? (double)PortOnlineClockRate() : 1.0;
        s_dueNs += std::llround((double)dt * (double)pace_scale() * rate * 1e9);
    }
    if (s_online)
        online_rollback();
    while (s_dueNs >= tick)
    {
        const bool may = s_online ? online_may_run(s_tick) : PortSimMayRunTick(s_tick) != 0;
        if (!may ||
            !(s_online ? PortOnlineTickInput(s_tick, s_pads) : PortSimLocalInput(s_tick, s_pads)))
        {
            // Held ticks build no backlog; the picture stays on the newest tick.
            s_dueNs = tick;
            break;
        }
        s_dueNs -= tick;
        const float before = pace_scale();
        const unsigned int k = s_tick;
        if (const int span = PortRbProbeWantsDouble(k))
        {
            // Run k .. k+span-1, roll back to k, run them again with the same input.
            PADStatus pads[4];
            std::memcpy(pads, s_pads, sizeof pads);
            PortRbProbeDoubleSave();
            for (int j = 0; j < span; j++)
            {
                std::memcpy(s_pads, pads, sizeof pads);
                s_tick = k + j;
                run_tick(k + j);
                PortRbProbeSpanHash(0, j);
            }
            PortRbProbeDoubleRestore();
            s_tick = k;
            for (int j = 0; j < span; j++)
            {
                std::memcpy(s_pads, pads, sizeof pads);
                s_tick = k + j;
                run_tick(k + j);
                PortRbProbeSpanHash(1, j);
            }
        }
        else
        {
            if (s_online)
                PortRollbackSave(k);
            run_tick(k);
        }
        // A pace the tick set starts at the tick's due time, so real time already past it counts at
        // the new pace and the next tick falls a fixed real time after this one.
        const float after = pace_scale();
        if (after != before && before > 0.0f)
            s_dueNs = std::llround((double)s_dueNs * (double)after / (double)before);
    }
    FixedUpdateTask::mAccumulatedDeltaT = (float)((double)(s_dueNs < tick ? s_dueNs : tick) / 1e9);
}

extern "C" void PortSimSetTimeScale(float scale)
{
    s_pendingScale = scale;
    s_scalePending = 1;
}

extern "C" int PortSimLocalInput(unsigned int tick, PADStatus* pads)
{
    (void)tick;
    std::memcpy(pads, PadStatus::s_Current, 4 * sizeof(PADStatus));
    // The camera's turn comes before quantising, as a sender's would.
    PortSimTurnSticks(pads);
    for (int i = 0; i < 4; i++)
    {
        const PADStatus in = pads[i];
        PADStatus& out = pads[i];
        std::memset(&out, 0, sizeof out);
        out.button = (u16)((in.button | s_pressedSinceTick[i]) & PORT_INPUT_BUTTONS);
        out.stickX = PortSimQuantizeStick(in.stickX);
        out.stickY = PortSimQuantizeStick(in.stickY);
        out.substickX = PortSimQuantizeStick(in.substickX);
        out.substickY = PortSimQuantizeStick(in.substickY);
        out.triggerLeft = in.triggerLeft;
        out.triggerRight = in.triggerRight;
        out.err = in.err;
        s_pressedSinceTick[i] = 0;
    }
    return 1;
}

namespace
{
thread_local int s_rngScope = PORT_RNG_COSMETIC;

unsigned int s_cosmeticSeed;
bool s_cosmeticSeeded;
unsigned int s_presentationSeed;

PortSimMatchSettings s_settings;
bool s_sessionSettings;
// The online session installed this tick's seed; a kickoff in the tick keeps it.
bool s_reseeded;

unsigned int* cosmetic_stream(void)
{
    // Takes over where the process's stream stood, so the front end draws as it always did.
    if (!s_cosmeticSeeded)
    {
        s_cosmeticSeed = nlDefaultSeed;
        s_cosmeticSeeded = true;
    }
    return &s_cosmeticSeed;
}

// The murmur3 finaliser: every bit of the seed and the goals reaches every bit.
uint32_t mix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}
}   // namespace

extern "C" int PortRngScope(void) { return s_rngScope; }

extern "C" int PortRngSetScope(int scope)
{
    const int previous = s_rngScope;
    s_rngScope = scope;
    return previous;
}

extern "C" unsigned int* PortRngCosmeticSeed(void)
{
    s_cosmeticSeeded = true;
    return &s_cosmeticSeed;
}

extern "C" void (*g_rbprobe_draw)(unsigned int seed);
void (*g_rbprobe_draw)(unsigned int seed);

extern "C" unsigned int* PortRngStream(void)
{
    if (s_rngScope == PORT_RNG_SIM)
    {
        if (g_rbprobe_draw)
            g_rbprobe_draw(nlDefaultSeed);
        return &nlDefaultSeed;
    }
    if (s_rngScope == PORT_RNG_PRESENTATION)
        return &s_presentationSeed;
    return cosmetic_stream();
}

extern "C" void PortSimSetMatchSettings(const PortSimMatchSettings* settings)
{
    s_sessionSettings = settings != NULL;
    if (settings != NULL)
        s_settings = *settings;
}

extern "C" const PortSimMatchSettings* PortSimSettings(void) { return &s_settings; }

extern "C" int PortRngBeginMatch(void)
{
    const int previous = PortRngSetScope(PORT_RNG_SIM);
    if (!g_simLockstep)
        return previous;
    if (!s_sessionSettings)
    {
        unsigned int seed;
        if (!PortFixedSeed(&seed))
            seed = nlRandom(0xFFFFFFFFu, cosmetic_stream());
        s_settings.seed = seed;
        s_settings.bowserRepeat = GetConfigBool(Config::Global(), "bowser_repeat", false) ? 1 : 0;
        s_settings.pureGame = 0;
        s_settings.pauseAllowed = 0;
    }
    nlSetRandomSeed(s_settings.seed, &nlDefaultSeed);
    return previous;
}

extern "C" int PortRngBeginKickoff(unsigned int goals)
{
    const int previous = PortRngSetScope(PORT_RNG_SIM);
    if (g_simLockstep && !s_reseeded)
    {
        const uint32_t h = mix32(mix32(mix32(s_settings.seed) ^ goals));
        nlSetRandomSeed(h, &nlDefaultSeed);
    }
    return previous;
}

// cGame's three clocks, its only ones; in lockstep mode ClockUpdateTask leaves them to the tick.
extern "C" void PortSimStepClocks(float dt) { ClockManager::Update(dt); }

extern "C" int PortSimEventHandler(const EventHandler* handler)
{
    const EventCallback callback = handler->m_pCBFunction;
    return callback == AIEventHandler || callback == Goalie::EventHandler ||
           callback == StatsTracker::eventHandler;
}

namespace
{
const unsigned int kNoTick = 0xFFFFFFFFu;

// The tick that raised each pool slot's event, kNoTick outside one; kept apart from Event's layout.
std::vector<unsigned int> s_eventTick;

unsigned int& event_tick(EventManager* m, Event* e)
{
    const size_t slot = (size_t)((char*)e - m->m_pool) / m->m_size;
    if (s_eventTick.size() < m->m_count)
        s_eventTick.assign(m->m_count, kNoTick);
    return s_eventTick[slot];
}
}   // namespace

extern "C" int PortSimEventPending(EventManager* manager, Event* event)
{
    if (!g_simLockstep)
        return 0;
    const unsigned int tick = event_tick(manager, event);
    const unsigned int confirmed = PortRollbackConfirmed();
    return tick != kNoTick && confirmed != kNoTick && tick > confirmed;
}

extern "C" void PortSimHoldEvent(EventManager* manager, Event* event)
{
    if (!g_simLockstep)
        return;
    event_tick(manager, event) = PortSimInTick() ? PortSimTick() : kNoTick;
    if (manager->m_handlers == NULL)
        return;
    EventHandler* handler = nlDLRingGetStart(manager->m_handlers);
    for (;;)
    {
        if (PortSimEventHandler(handler) &&
            (handler->m_uDestinationMask & manager->m_dest[event->m_uEventID]) != 0)
        {
            event->m_nReferenceCount = 1;
            return;
        }
        if (handler == manager->m_handlers)
            return;
        handler = handler->m_next;
    }
}

static void dispatch_sim_event(EventManager* manager, Event* event)
{
    EventHandler* handler = nlDLRingGetStart(manager->m_handlers);
    if (handler == NULL)
        return;
    for (;;)
    {
        if (PortSimEventHandler(handler) &&
            (handler->m_uDestinationMask & manager->m_dest[event->m_uEventID]) != 0)
            handler->m_pCBFunction(event, handler->m_pCBParam);
        if (handler == manager->m_handlers)
            return;
        handler = handler->m_next;
    }
}

extern "C" void PortSimDispatchEvents(void)
{
    const int scope = PortRngSetScope(PORT_RNG_SIM);
    EventManager* manager = g_pEventManager;
    manager->m_dispatching = true;
    // Held events precede queued events; callbacks defer new events until the next pass.
    while (manager->m_keep != NULL)
    {
        Event* event = nlDLRingRemoveStart(&manager->m_keep);
        dispatch_sim_event(manager, event);
        event->m_nReferenceCount = 0;
        nlDLRingAddEnd(&manager->m_free, event);
    }
    if (manager->m_queue != NULL)
    {
        Event* event = nlDLRingGetStart(manager->m_queue);
        for (;;)
        {
            if (event->m_nReferenceCount != 0)
            {
                dispatch_sim_event(manager, event);
                event->m_nReferenceCount = 0;
            }
            if (event == manager->m_queue)
                break;
            event = event->m_next;
        }
    }
    manager->m_dispatching = false;
    nlDLRingAppendRing(&manager->m_queue, manager->m_deferred);
    manager->m_deferred = NULL;
    PortRngSetScope(scope);
}

namespace
{
// Rollback: events held at a tick boundary are the next tick's input, saved by value.
struct HeldEvent
{
    unsigned int ring;   // 0 m_keep, 1 m_queue, 2 m_deferred
    unsigned int id;
    unsigned int slot;   // its pool slot, so a load finds it if it is still queued
    unsigned int tick;   // the tick that raised it, kNoTick outside one
};

Event** held_ring(EventManager* m, unsigned int ring)
{
    return ring == 0 ? &m->m_keep : ring == 1 ? &m->m_queue : &m->m_deferred;
}

bool ring_has(Event* last, const Event* e)
{
    for (Event* it = last; it != NULL;)
    {
        if (it == e)
            return true;
        it = it->m_next;
        if (it == last)
            break;
    }
    return false;
}

void events_snapshot(void (*append)(const void* p, size_t size))
{
    EventManager* m = g_pEventManager;
    unsigned int n = 0;
    for (unsigned int r = 0; m != NULL && r < 3; r++)
    {
        Event* head = *held_ring(m, r);
        for (Event* e = head; e != NULL;)
        {
            if (e->m_nReferenceCount != 0)
                n++;
            e = e->m_next;
            if (e == head)
                break;
        }
    }
    const unsigned int saved = PortSimTick();
    append(&saved, sizeof saved);
    append(&n, sizeof n);
    // A ring's pointer is its last event: saved from the first, a load appends them back in order.
    for (unsigned int r = 0; m != NULL && r < 3; r++)
    {
        Event* last = *held_ring(m, r);
        for (Event* e = nlDLRingGetStart(last); e != NULL;)
        {
            if (e->m_nReferenceCount != 0)
            {
                const unsigned int slot = (unsigned int)(((char*)e - m->m_pool) / m->m_size);
                const HeldEvent h = { r, e->m_uEventID, slot, event_tick(m, e) };
                append(&h, sizeof h);
                append(&e->m_data, m->m_size - offsetof(Event, m_data));
            }
            if (e == last)
                break;
            e = e->m_next;
        }
    }
}

void events_check(const char* where);

size_t events_unsnapshot(const unsigned char* in)
{
    EventManager* m = g_pEventManager;
    const unsigned char* p = in;
    // The saved tick, not the loop's: the loop has not stepped back yet when a load runs.
    unsigned int now;
    std::memcpy(&now, p, sizeof now);
    p += sizeof now;
    unsigned int n;
    std::memcpy(&n, p, sizeof n);
    p += sizeof n;
    if (m == NULL)
        return (size_t)(p - in);
    // Undone ticks' events and m_keep's held ones are freed; earlier ones stay for the screen.
    for (unsigned int r = 0; r < 3; r++)
    {
        Event** ring = held_ring(m, r);
        Event* head = *ring;
        std::vector<Event*> drop;
        for (Event* e = head; e != NULL;)
        {
            const unsigned int tick = event_tick(m, e);
            if (tick != kNoTick && tick >= now)
                drop.push_back(e);
            else if (r == 0 && e->m_nReferenceCount != 0)
                drop.push_back(e);
            else
                e->m_nReferenceCount = 0;
            e = e->m_next;
            if (e == head)
                break;
        }
        for (Event* e : drop)
        {
            nlDLRingRemove(ring, e);
            e->m_nReferenceCount = 0;
            nlDLRingAddEnd(&m->m_free, e);
        }
    }
    const size_t payload = m->m_size - offsetof(Event, m_data);
    for (unsigned int i = 0; i < n; i++)
    {
        HeldEvent h;
        std::memcpy(&h, p, sizeof h);
        p += sizeof h;
        // Still queued for the screen once the simulation had it: it is held again, not copied.
        Event* e = h.slot < m->m_count ? (Event*)(m->m_pool + (size_t)h.slot * m->m_size) : NULL;
        if (e == NULL || h.ring == 0 || e->m_uEventID != h.id || event_tick(m, e) != h.tick ||
            !ring_has(*held_ring(m, h.ring), e))
        {
            e = nlDLRingRemoveStart(&m->m_free);
            if (e != NULL)
            {
                event_tick(m, e) = h.tick;
                e->m_uEventID = h.id;
                // Not queued means the screen already had it; m_keep replays only the simulation's.
                nlDLRingAddEnd(&m->m_keep, e);
            }
        }
        if (e != NULL)
        {
            e->m_nReferenceCount = 1;
            std::memcpy(&e->m_data, p, payload);
        }
        p += payload;
    }
    events_check("after load");
    return (size_t)(p - in);
}

// Diagnostic: every pool event is in exactly one ring.
void events_check(const char* where)
{
    static const bool on = std::getenv("STRIKERS_EVENT_CHECK") != NULL;
    EventManager* m = g_pEventManager;
    if (!on || m == NULL)
        return;
    std::vector<int> seen(m->m_count, 0);
    Event* rings[4] = { m->m_free, m->m_keep, m->m_queue, m->m_deferred };
    int total = 0;
    for (Event* head : rings)
        for (Event* e = head; e != NULL;)
        {
            const size_t slot = (size_t)((char*)e - m->m_pool) / m->m_size;
            if (slot >= m->m_count || (char*)e < m->m_pool)
            {
                OSReport("[events] %s: event %p outside the pool at tick %u\n", where, (void*)e,
                         s_tick);
                return;
            }
            if (++seen[slot] > 1)
            {
                OSReport("[events] %s: event slot %zu in two rings at tick %u\n", where, slot,
                         s_tick);
                return;
            }
            total++;
            e = e->m_next;
            if (e == head)
                break;
        }
    if (total != (int)m->m_count)
        OSReport("[events] %s: %d of %u events in rings at tick %u\n", where, total, m->m_count,
                 s_tick);
}

const bool s_eventState = (PortSimStateHook(events_snapshot, events_unsnapshot), true);

// The armed barrier; clearing it alone disarms everything below.
int s_barrier = PORT_BARRIER_NONE;
unsigned int s_freeze;   // the first tick held, and the tick play resumes at
unsigned int s_goalTick;
int s_event7;     // event 7 is still to come, in tick s_goalTick + 1
int s_done;       // this machine's presentation is over
int s_released;   // play resumes at the freeze tick: s_done offline, the host's RESUME online
int s_reached;    // online: the session has heard that the freeze holds
int s_pauseSlot = -1;
int s_pausesLeft[2];
u16 s_lastTickButtons[4];   // each slot's tick input in the tick before, for Start's edge
const int kPausesPerSide = 3;

// Ticks 0-2 run before the intro, as a fresh process at 50 Hz ran them.
const unsigned int kIntroTicks = 3;
// The goal script's second of simulation time: ticks g+1 to g+50 run after goal tick g.
const unsigned int kPostGoalTicks = 50;

const char* barrier_name(int kind)
{
    return kind == PORT_BARRIER_INTRO  ? "intro"
           : kind == PORT_BARRIER_GOAL ? "goal"
           : kind == PORT_BARRIER_END  ? "end"
                                       : "pause";
}

unsigned int next_tick(void) { return PortSimTick() + (PortSimInTick() ? 1u : 0u); }
}   // namespace

extern "C" void PortSimBarrierArm(int kind)
{
    s_barrier = kind;
    s_done = 0;
    s_released = 0;
    s_reached = 0;
    s_event7 = 0;
    if (kind == PORT_BARRIER_INTRO)
        s_freeze = kIntroTicks;
    else if (kind == PORT_BARRIER_GOAL)
    {
        s_goalTick = PortSimTick();
        s_event7 = 1;
        s_freeze = s_goalTick + kPostGoalTicks + 1;
    }
    else
    {
        s_freeze = next_tick();
        OSReport("[lockstep] end freezes t=%u\n", s_freeze);
    }
    nlSetRandomSeed(mix32(s_settings.seed ^ mix32(PortSimPresentationId())), &s_presentationSeed);
}

extern "C" int PortSimMayRunTick(unsigned int tick)
{
    return s_barrier == PORT_BARRIER_NONE || s_released || tick < s_freeze;
}

extern "C" void PortSimRunTickActions(unsigned int tick)
{
    if (s_barrier == PORT_BARRIER_NONE)
        return;
    if (s_event7 && tick == s_goalTick + 1)
    {
        s_event7 = 0;
        // The goal script's call only presents; the 20 draws and the targets belong to the match.
        NisPlayer::Instance()->PlayCharacterDirection();
        OSReport("[lockstep] goal t=%u, event 7 t=%u, freeze t=%u\n", s_goalTick, tick, s_freeze);
    }
    if (s_released && tick >= s_freeze)
    {
        const int kind = s_barrier;
        s_barrier = PORT_BARRIER_NONE;
        s_pauseSlot = -1;
        OSReport("[lockstep] %s resumes t=%u\n", barrier_name(kind), tick);
        if (kind != PORT_BARRIER_END && kind != PORT_BARRIER_PAUSE &&
            g_pGame->m_eGameState != GS_END_GAME)
            g_pGame->ChangeGameState(GS_KICKOFF);
    }
}

extern "C" void PortSimBarrierReached(int kind)
{
    // Only the run event 7 started counts, so event 7 always comes before the freeze.
    const unsigned int next = next_tick();
    if (kind == s_barrier && !s_event7 && next < s_freeze)
    {
        s_freeze = next;
        OSReport("[lockstep] %s event 8 t=%u, freeze t=%u\n", barrier_name(kind), next - 1, next);
    }
}

// Offline this machine is every machine, so play resumes at the freeze tick, or runs through it.
extern "C" void PortSimBarrierDone(int kind)
{
    if (kind == PORT_BARRIER_NONE || kind != s_barrier || s_done)
        return;
    s_done = 1;
    if (!s_online)
        s_released = 1;
    else if (s_reached)
        PortOnlineBarrierDone(kind);
}

extern "C" unsigned int PortSimPresentationId(void)
{
    if (s_barrier == PORT_BARRIER_INTRO)
        return 1;
    if (s_barrier == PORT_BARRIER_GOAL)
        return s_goalTick + 2;
    if (s_barrier == PORT_BARRIER_END)
        return s_freeze + 2;
    return 0;
}

extern "C" int PortSimPauseSlot(void)
{
    return s_barrier == PORT_BARRIER_PAUSE && !s_released ? s_pauseSlot : -1;
}

// Online, a freeze holds once every replay before it has run; until then a replay can undo it.
bool freeze_confirmed(void) { return !s_online || PortOnlineSettledTick() >= s_freeze; }

extern "C" int PortSimBarrier(void)
{
    return (s_barrier != PORT_BARRIER_NONE && !s_done && PortSimTick() >= s_freeze &&
            freeze_confirmed())
               ? s_barrier
               : PORT_BARRIER_NONE;
}

namespace
{
// Replays step, relax and jolt the live nets to draw them; the match keeps its own copy per tick.
struct NetCopy
{
    NetMesh* mesh;
    unsigned char object[sizeof(NetMesh)];
    std::vector<nlVector3> position;
    std::vector<nlVector3> prevPosition;
    std::vector<nlVector3> accel;
};

NetCopy s_nets[2];

void net_copy(NetCopy& c, bool save)
{
    NetMesh* m = c.mesh;
    const size_t n = (size_t)m->m_NumParticles;
    if (save)
    {
        std::memcpy(c.object, (const void*)m, sizeof c.object);
        std::memcpy(c.position.data(), m->m_v3Position, n * sizeof(nlVector3));
        std::memcpy(c.prevPosition.data(), m->m_v3PrevPosition, n * sizeof(nlVector3));
        std::memcpy(c.accel.data(), m->m_v3Accel, n * sizeof(nlVector3));
    }
    else
    {
        std::memcpy((void*)m, c.object, sizeof c.object);
        std::memcpy(m->m_v3Position, c.position.data(), n * sizeof(nlVector3));
        std::memcpy(m->m_v3PrevPosition, c.prevPosition.data(), n * sizeof(nlVector3));
        std::memcpy(m->m_v3Accel, c.accel.data(), n * sizeof(nlVector3));
    }
}

void nets_capture(void)
{
    NetMesh* meshes[2] = { NetMesh::spPositiveXNetMesh, NetMesh::spNegativeXNetMesh };
    for (int i = 0; i < 2; i++)
    {
        NetCopy& c = s_nets[i];
        c.mesh = meshes[i] != NULL && meshes[i]->m_v3Position != NULL ? meshes[i] : NULL;
        if (c.mesh == NULL)
            continue;
        const size_t n = (size_t)c.mesh->m_NumParticles;
        c.position.resize(n);
        c.prevPosition.resize(n);
        c.accel.resize(n);
        net_copy(c, true);
    }
}

// Only the nets captured at BeginGame, still the stadium's.
bool net_current(const NetCopy& c, const NetMesh* live)
{
    return c.mesh != NULL && c.mesh == live && (size_t)live->m_NumParticles == c.position.size();
}

void nets_restore(void)
{
    if (net_current(s_nets[0], NetMesh::spPositiveXNetMesh))
        net_copy(s_nets[0], false);
    if (net_current(s_nets[1], NetMesh::spNegativeXNetMesh))
        net_copy(s_nets[1], false);
}

// Rollback: the nets' per-tick copies, which live on the system heap, go with the snapshot.
void nets_snapshot(void (*append)(const void* p, size_t size))
{
    for (const NetCopy& c : s_nets)
    {
        append(c.object, sizeof c.object);
        append(c.position.data(), c.position.size() * sizeof(nlVector3));
        append(c.prevPosition.data(), c.prevPosition.size() * sizeof(nlVector3));
        append(c.accel.data(), c.accel.size() * sizeof(nlVector3));
    }
}

size_t nets_unsnapshot(const unsigned char* in)
{
    const unsigned char* p = in;
    for (NetCopy& c : s_nets)
    {
        std::memcpy(c.object, p, sizeof c.object);
        p += sizeof c.object;
        const size_t n = c.position.size() * sizeof(nlVector3);
        std::memcpy(c.position.data(), p, n);
        p += n;
        std::memcpy(c.prevPosition.data(), p, n);
        p += n;
        std::memcpy(c.accel.data(), p, n);
        p += n;
    }
    return (size_t)(p - in);
}

void nets_save(void)
{
    if (net_current(s_nets[0], NetMesh::spPositiveXNetMesh))
        net_copy(s_nets[0], true);
    if (net_current(s_nets[1], NetMesh::spNegativeXNetMesh))
        net_copy(s_nets[1], true);
}
}   // namespace

namespace
{
int s_ended;   // the session has heard this match's last tick

// This match's rollbacks, logged with its last tick.
unsigned int s_rollbacks;
unsigned int s_rollbackTicks;
unsigned int s_rollbackDeepest;
unsigned long long s_rollbackSlowestNs;

// The components a desync report compares; it names the first that differs.
const char* const kComponents[] = { "ball",  "c0",   "c1",  "c2",  "c3",   "c4",
                                    "c5",    "c6",   "c7",  "c8",  "c9",   "clk",
                                    "score", "game", "pwr", "npc", "seed", "scr" };
const int kComponentCount = (int)(sizeof kComponents / sizeof kComponents[0]);
const unsigned int kComponentEvery = 50;

// The last tick: the end holds the next one, and so does a sudden-death goal's freeze.
bool online_end_due(unsigned int last)
{
    return !s_ended && s_barrier != PORT_BARRIER_NONE && !s_released && last + 1 >= s_freeze &&
           freeze_confirmed() &&
           (s_barrier == PORT_BARRIER_END || g_pGame->m_eGameState == GS_END_GAME);
}

void online_end(unsigned int last, uint64_t hash)
{
    s_ended = 1;
    PortOnlineMatchEnded();
    OSReport("[online] last tick t=%u, hash %016llx\n", last, (unsigned long long)hash);
    OSReport("[online] rollbacks %u, %u ticks replayed, deepest %u, slowest %.2f ms\n", s_rollbacks,
             s_rollbackTicks, s_rollbackDeepest, (double)s_rollbackSlowestNs / 1e6);
}

bool online_may_run(unsigned int k)
{
    if (!PortOnlineMayRunTick(k))
        return false;
    OnlineTickActions a;
    if (s_barrier != PORT_BARRIER_NONE && !s_released && k >= s_freeze &&
        PortOnlineTickActions(k, &a) && (a.actions & ONLINE_ACTION_RESUME) != 0)
        s_released = 1;
    if (PortSimMayRunTick(k))
        return true;
    // The last tick's input can settle after it ran, with no replay to finish the match.
    if (k > 0 && online_end_due(k - 1))
    {
        PortSimHashes h;
        PortSimHash(&h);
        online_end(k - 1, h.all);
    }
    if (!s_ended && s_barrier != PORT_BARRIER_NONE && !s_reached && freeze_confirmed())
    {
        s_reached = 1;
        PortOnlineBarrierReached(s_barrier, k);
        if (s_done)
            PortOnlineBarrierDone(s_barrier);
    }
    return false;
}

extern "C" unsigned long long port_monotonic_ns(void);
extern "C" const char* PortSymbolName(const void* p);
extern "C" void PortRollbackInputRegion(const void* p);
extern "C" void PortRollbackDataTake(int which);
extern "C" void PortRollbackDataReport(void);
extern "C" void PortRollbackDiff(unsigned int a, unsigned int b,
                                 void (*report)(const char*, size_t));
extern "C" const void* PortRollbackRegion(size_t index, size_t* size);

// Ticks that ran on a wrong guess run again from the first one's snapshot, without sound or rumble.
void online_rollback(void)
{
    const unsigned int confirmed = PortOnlineConfirmedTick();
    PortRollbackSetConfirmed(confirmed > 0 ? confirmed - 1 : 0);
    const unsigned int to = PortOnlineRollbackTo();
    if (to == 0xFFFFFFFFu || to >= s_tick)
        return;
    if (!PortRollbackHas(to))
    {
        OSReport("[rollback] no snapshot for tick %u at tick %u\n", to, s_tick);
        return;
    }
    const unsigned int now = s_tick;
    static const bool log = std::getenv("STRIKERS_ROLLBACK_LOG") != NULL;
    if (log)
        OSReport("[rollback] to %u from %u\n", to, now);
    const unsigned long long replayStart = port_monotonic_ns();
    PortRollbackLoad(to);
    PortRollbackSetResimulating(1);
    // Diagnostic (STRIKERS_ROLLBACK_CHECK): a replay's first tick runs twice and must match.
    static const bool check = std::getenv("STRIKERS_ROLLBACK_CHECK") != NULL;
    if (check)
    {
        s_tick = to;
        if (PortOnlineTickInput(to, s_pads))
        {
            static unsigned char ballA[sizeof(cBall)], ballB[sizeof(cBall)];
            PADStatus input[4];
            std::memcpy(input, s_pads, sizeof input);
            PortRollbackDataTake(0);
            run_tick(to);
            PortSimHashes a;
            PortSimHash(&a);
            PortRollbackSave(PORT_ROLLBACK_SCRATCH);
            std::memcpy(ballA, g_pBall, sizeof ballA);
            PortRollbackLoad(to);
            PortRollbackDataTake(1);
            s_tick = to;
            // The same input as the first run: the network may have brought newer records since.
            std::memcpy(s_pads, input, sizeof input);
            run_tick(to);
            PortSimHashes b;
            PortSimHash(&b);
            PortRollbackSave(PORT_ROLLBACK_SCRATCH + 1);
            std::memcpy(ballB, g_pBall, sizeof ballB);
            if (a.all != b.all || a.scr != b.scr)
            {
                char which[160] = "";
                int at = 0;
                const uint64_t ca[] = {
                    a.ball, a.clk, a.score, a.game, a.pwr, a.npc, a.seed, a.scr
                };
                const uint64_t cb[] = {
                    b.ball, b.clk, b.score, b.game, b.pwr, b.npc, b.seed, b.scr
                };
                static const char* const kn[] = { "ball", "clk", "score", "game",
                                                  "pwr",  "npc", "seed",  "scr" };
                for (int i = 0; i < 8; i++)
                    if (ca[i] != cb[i])
                        at += std::snprintf(which + at, sizeof which - at, " %s", kn[i]);
                for (int i = 0; i < 10; i++)
                    if (a.perChar[i] != b.perChar[i])
                        at += std::snprintf(which + at, sizeof which - at, " c%d", i);
                OSReport("[rollback] CHECK t=%u replays differently in%s\n", to, which);
                for (size_t o = 0; o + 4 <= sizeof ballA; o += 4)
                    if (std::memcmp(ballA + o, ballB + o, 4) != 0)
                        OSReport("[rollback]   ball +0x%zx: %08x vs %08x\n", o,
                                 *(const uint32_t*)(ballA + o), *(const uint32_t*)(ballB + o));
                OSReport("[rollback]   binary data that differs between the two starts:\n");
                PortRollbackDataReport();
                static int diffs;
                diffs = 0;
                PortRollbackDiff(PORT_ROLLBACK_SCRATCH, PORT_ROLLBACK_SCRATCH + 1,
                                 [](const char* what, size_t o) {
                                     if (diffs++ >= 24)
                                         return;
                                     if (std::strcmp(what, "static") == 0)
                                     {
                                         size_t n = 0;
                                         const void* p = PortRollbackRegion(o, &n);
                                         OSReport("[rollback]   static #%zu %p (%zu B) %s\n", o, p,
                                                  n, PortSymbolName(p));
                                     }
                                     else
                                         OSReport("[rollback]   heap +0x%zx\n", o);
                                 });
            }
            PortRollbackLoad(to);
        }
    }
    // A freeze the replay arms stops it, as it stops a fresh tick; Start freezes the next tick.
    unsigned int k = to;
    for (; k < now; k++)
    {
        s_tick = k;
        if (!PortSimMayRunTick(k) || !PortOnlineTickInput(k, s_pads))
            break;
        PortRollbackSave(k);
        run_tick(k);
    }
    PortRollbackSetResimulating(0);
    s_tick = k;
    if (k < now)
        PortOnlineRewind(k);
    events_check("after replay");
    const unsigned long long replayNs = port_monotonic_ns() - replayStart;
    s_rollbacks++;
    s_rollbackTicks += k - to;
    if (now - to > s_rollbackDeepest)
        s_rollbackDeepest = now - to;
    if (replayNs > s_rollbackSlowestNs)
        s_rollbackSlowestNs = replayNs;
    if (log)
        OSReport("[rollback] replayed %u ticks in %.3f ms\n", k - to, (double)replayNs / 1e6);
}

// What Choose Sides does from the pause menu, at a tick every machine runs.
void slots_to_cpu(unsigned int slots)
{
    GameInfoManager* info = nlSingleton<GameInfoManager>::Instance();
    for (int s = 0; s < 4; s++)
    {
        if (slots & (1u << s))
            info->SetPlayingSide((unsigned short)s, -1);
    }
    g_pTeams[0]->UpdateControllers();
    g_pTeams[1]->UpdateControllers();
    info->ApplyDifficultySettings();
    g_pGame->SetDifficulty(info->GetDifficulty(0), info->GetDifficulty(1), (eDifficultyID)3);
}

void online_tick_top(unsigned int k)
{
    OnlineTickActions a;
    if (!PortOnlineTickActions(k, &a))
        return;
    if (a.actions & ONLINE_ACTION_RESEED)
    {
        nlSetRandomSeed(a.seed, &nlDefaultSeed);
        s_reseeded = true;
    }
    if (a.actions & ONLINE_ACTION_SLOTS_TO_CPU)
    {
        slots_to_cpu(a.cpuSlots);
        OSReport("[online] slots %x go to the CPU at t=%u\n", (unsigned)a.cpuSlots, k);
    }
    if (a.actions & ONLINE_ACTION_END_MATCH)
        OSReport("[online] the match ends unfinished at t=%u\n", k);
}

// A slot's new Start in tick k, in play and with no barrier armed, freezes tick k+1.
void online_pause_check(unsigned int k)
{
    const int state = g_pGame->m_eGameState;
    const bool inPlay = state == GS_KICKOFF || state == GS_GAMEPLAY || state == GS_OVERTIME;
    for (int s = 0; s < 4; s++)
    {
        const bool edge = (s_pads[s].button & PAD_BUTTON_START) != 0 &&
                          (s_lastTickButtons[s] & PAD_BUTTON_START) == 0;
        s_lastTickButtons[s] = s_pads[s].button;
        if (!edge || !s_settings.pauseAllowed || !inPlay || s_barrier != PORT_BARRIER_NONE)
            continue;
        const int side =
            nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((unsigned short)s);
        if (side < 0 || side > 1 || s_pausesLeft[side] == 0)
            continue;
        s_pausesLeft[side]--;
        s_barrier = PORT_BARRIER_PAUSE;
        s_freeze = k + 1;
        s_done = 0;
        s_released = 0;
        s_reached = 0;
        s_event7 = 0;
        s_pauseSlot = s;
        OSReport("[lockstep] slot %d pauses, freeze t=%u, %d left for side %d\n", s, s_freeze,
                 s_pausesLeft[side], side);
    }
}

void online_tick_end(unsigned int k)
{
    PortSimHashes h;
    PortSimHash(&h);
    PortOnlineTickHash(k, h.all);
    static const bool logTicks = std::getenv("STRIKERS_ROLLBACK_LOG") != NULL;
    static const bool checkTicks = std::getenv("STRIKERS_ROLLBACK_CHECK") != NULL;
    if (k % kComponentEvery == 0 || (logTicks && k < 200) || checkTicks)
    {
        uint64_t c[kComponentCount] = { h.ball };
        for (int i = 0; i < 10; i++)
            c[1 + i] = h.perChar[i];
        const uint64_t rest[] = { h.clk, h.score, h.game, h.pwr, h.npc, h.seed, h.scr };
        std::memcpy(&c[11], rest, sizeof rest);
        PortOnlineTickComponents(k, c, kComponents, kComponentCount);
        if (logTicks && k < 12)
        {
            char line[1200];
            int at = std::snprintf(line, sizeof line, "[rollback] comp t=%u%s", k,
                                   PortRollbackResimulating() ? " again" : "");
            for (int i = 0; i < kComponentCount && at < (int)sizeof line; i++)
                at += std::snprintf(line + at, sizeof line - at, " %s=%04llx", kComponents[i],
                                    (unsigned long long)(c[i] & 0xFFFF));
            OSReport("%s\n", line);
        }
    }
    s_reseeded = false;
    online_pause_check(k);
    if (online_end_due(k))
        online_end(k, h.all);
    // A cutscene stops the task that asks for ticks, so the session hears the freeze here.
    if (!s_ended && s_barrier != PORT_BARRIER_NONE && !s_released && !s_reached &&
        k + 1 >= s_freeze && freeze_confirmed())
    {
        s_reached = 1;
        PortOnlineBarrierReached(s_barrier, k + 1);
        if (s_done)
            PortOnlineBarrierDone(s_barrier);
    }
}
}   // namespace

extern "C" void PortSimSetOnline(int online) { s_online = online != 0; }

extern "C" int PortSimOnline(void) { return s_online; }

extern "C" void PortSimBeginMatch(void)
{
    s_tick = 0;
    s_match++;
    // One tick due, as a fresh process's accumulator holds, so every match starts like the first.
    s_dueNs = tick_ns();
    s_startSchedule = 1;
    s_scalePending = 0;
    std::memset(s_pressedSinceTick, 0, sizeof s_pressedSinceTick);
    std::memset(s_polar, 0, sizeof s_polar);
    s_barrier = PORT_BARRIER_NONE;
    s_pauseSlot = -1;
    s_pausesLeft[0] = s_pausesLeft[1] = kPausesPerSide;
    std::memset(s_lastTickButtons, 0, sizeof s_lastTickButtons);
    s_begun = 0;
    s_ended = 0;
    s_rollbacks = 0;
    s_rollbackTicks = 0;
    s_rollbackDeepest = 0;
    s_rollbackSlowestNs = 0;
    s_reseeded = false;
    // A rollback never reaches into the previous match.
    PortRollbackReset();
    PortSimCameraBeginMatch();
    if (g_simLockstep)
        nets_capture();
}

namespace
{
// Lockstep state ticks carry forward; the schedule and the handshake never roll back.
const bool s_rollbackState =
    (PortSimState(s_pads, sizeof s_pads), PortRollbackInputRegion(s_pads),
     PortSimState(s_polar, sizeof s_polar),
     PortSimState(s_lastTickButtons, sizeof s_lastTickButtons),
     PortSimState(s_pausesLeft, sizeof s_pausesLeft), PortSimState(&s_barrier, sizeof s_barrier),
     PortSimState(&s_freeze, sizeof s_freeze), PortSimState(&s_goalTick, sizeof s_goalTick),
     PortSimState(&s_event7, sizeof s_event7), PortSimState(&s_pauseSlot, sizeof s_pauseSlot),
     PortSimState(&s_reseeded, sizeof s_reseeded),
     PortSimState(&s_scalePending, sizeof s_scalePending),
     PortSimState(&s_pendingScale, sizeof s_pendingScale),
     PortSimState(&s_presentationSeed, sizeof s_presentationSeed),
     PortSimStateHook(nets_snapshot, nets_unsnapshot), true);
}   // namespace
