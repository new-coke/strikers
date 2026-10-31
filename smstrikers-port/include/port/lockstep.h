// Lockstep simulation: a match as a function of its settings, its seed and each tick's input.

#ifndef PORT_LOCKSTEP_H
#define PORT_LOCKSTEP_H

#ifdef __cplusplus
extern "C" {
#endif

struct PADStatus;
struct Event;
struct EventHandler;
struct EventManager;

// Core: non-zero while matches run through the lockstep tick loop.
extern int g_simLockstep;

// Core: sets g_simLockstep from STRIKERS_LOCKSTEP=1, once, before the first match.
void PortLockstepInit(void);

// Tick loop: inside tick k, k; between ticks, the next tick's index. 0 at BeginGame.
unsigned int PortSimTick(void);

// Tick loop: BeginGame calls so far in this process.
unsigned int PortSimMatch(void);

// Tick loop: non-zero between PortSimTickBegin and PortSimTickEnd.
int PortSimInTick(void);

// Tick loop: top of cGame::BeginGame; restarts the tick count, schedule and barriers.
void PortSimBeginMatch(void);

// Tick loop: brackets every lockstep tick; the end advances the count.
void PortSimTickBegin(void);
void PortSimTickEnd(void);

// Tick loop: FixedUpdateTask::Run in task state 2 under g_simLockstep; dt is the task delta.
void PortLockstepRunTicks(float dt);

enum
{
    PORT_INPUT_BUTTONS = 0x1F7F
};

static inline signed char PortSimQuantizeStick(signed char value)
{
    return value >= -2 && value <= 2 ? 0 : (signed char)(value & ~1);
}

// Tick loop, offline: this frame's pads, with presses since the last tick ORed in and sticks
// quantised.
int PortSimLocalInput(unsigned int tick, struct PADStatus* pads);

// Tick loop: FixedUpdateTask::mTimeScale from the top of the next lockstep tick; it paces the
// ticks until then.
void PortSimSetTimeScale(float scale);

// RNG: the stream a draw on nlDefaultSeed belongs to while g_simLockstep is set.
enum
{
    PORT_RNG_COSMETIC = 0,
    PORT_RNG_SIM = 1,
    PORT_RNG_PRESENTATION = 2   // reseeded from the match seed at each barrier
};

// RNG: the current scope; the lockstep tick and its barrier actions run in PORT_RNG_SIM.
int PortRngScope(void);

// RNG: sets the scope and returns the previous one.
int PortRngSetScope(int scope);

// RNG: the stream for the current scope; nlMath sends nlDefaultSeed draws to it in lockstep.
unsigned int* PortRngStream(void);

// RNG: the cosmetic stream, for code that pins it.
unsigned int* PortRngCosmeticSeed(void);

// RNG: what the simulation takes from the match settings instead of this machine's.
typedef struct PortSimMatchSettings
{
    unsigned int seed;   // the gameplay stream's seed at BeginGame
    int bowserRepeat;
    int pureGame;
    int pauseAllowed;   // online: Start in the tick input pauses every machine
} PortSimMatchSettings;

// RNG: the session's settings for the next BeginGame; NULL goes back to STRIKERS_SEED and the ini.
void PortSimSetMatchSettings(const PortSimMatchSettings* settings);

// RNG: the settings of the match in progress.
const PortSimMatchSettings* PortSimSettings(void);

// RNG: top of cGame::BeginGame: SIM scope, the gameplay stream seeded; returns the old scope.
int PortRngBeginMatch(void);

// RNG: top of ResetForKickOff: SIM scope, reseeded from seed and goals; the old scope.
int PortRngBeginKickoff(unsigned int goals);

// Clocks: steps the simulation clocks by `dt` inside the lockstep tick, before its tasks.
void PortSimStepClocks(float dt);

// Events: runs the simulation handlers at each tick's end, and per frame when no tick runs.
void PortSimDispatchEvents(void);
int PortSimEventHandler(const struct EventHandler* handler);
void PortSimHoldEvent(struct EventManager* manager, struct Event* event);

// Events: non-zero while `event` came from an unconfirmed tick; the screen holds it until then.
int PortSimEventPending(struct EventManager* manager, struct Event* event);

// Camera: after the tick's events, steps the sim camera and the screen positions AI reads.
void PortSimCameraStep(void);

// Camera: from PortSimBeginMatch; a fresh sim camera and no screen positions carried over.
void PortSimCameraBeginMatch(void);

// Camera: turns pads[0..3]'s sticks by the camera on screen, before they become tick input.
void PortSimTurnSticks(struct PADStatus* pads);

// Barriers: what holds the tick.
enum
{
    PORT_BARRIER_NONE = 0,
    PORT_BARRIER_INTRO = 1,
    PORT_BARRIER_GOAL = 2,
    PORT_BARRIER_END = 3,
    PORT_BARRIER_PAUSE = 4
};

// Barriers: arms `kind` by its rule. The intro holds tick 3 on. A goal, called inside goal tick g,
// issues event 7 in tick g+1 and holds tick g+51 on. The end holds the tick after this one.
void PortSimBarrierArm(int kind);

// Barriers: non-zero when `tick` may run; the lockstep loop stops at the first that may not.
int PortSimMayRunTick(unsigned int tick);

// Barriers: top of `tick`, in SIM scope: the actions stamped for it (event 7, kickoff).
void PortSimRunTickActions(unsigned int tick);

// Barriers: inside a tick after event 7, brings the armed `kind`'s freeze forward to the
// next tick.
void PortSimBarrierReached(int kind);

// Barriers: this machine's presentation of `kind` ended or was skipped. Offline that ends it,
// and play resumes at the freeze tick with the kickoff reset; online the host's RESUME does.
void PortSimBarrierDone(int kind);

// Barriers: the barrier holding the tick at its freeze, or PORT_BARRIER_NONE.
int PortSimBarrier(void);
unsigned int PortSimPresentationId(void);

// Online with pause allowed: a new Start in a slot's tick input freezes the next tick on every
// machine, 3 times per side a match. The slot that paused until the host resumes, else -1.
int PortSimPauseSlot(void);

// Online: the session's input, barriers, hashes and clock drive the tick loop; 0 hands it back.
void PortSimSetOnline(int online);

// Online: non-zero from the start of an online match's load until this machine leaves it.
int PortSimOnline(void);

// Online, each match frame: ends the match once its end freeze holds, in any task state.
void PortSimOnlineEndCheck(void);

#ifdef __cplusplus
}

// RNG: a presentation entry point the simulation calls; its draws take the cosmetic stream.
class PortRngCosmetic
{
  public:
    PortRngCosmetic() : mScope(PortRngSetScope(PORT_RNG_COSMETIC)) {}
    ~PortRngCosmetic() { PortRngSetScope(mScope); }

  private:
    int mScope;
};

// RNG: a cutscene, camera or replay pick, which every machine must make the same.
class PortRngPresentation
{
  public:
    PortRngPresentation() : mScope(PortRngSetScope(PORT_RNG_PRESENTATION)) {}
    ~PortRngPresentation() { PortRngSetScope(mScope); }

  private:
    int mScope;
};

#endif

#endif   // PORT_LOCKSTEP_H
