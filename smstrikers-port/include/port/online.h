// Online play in input lockstep: the session's polled state, commands and game hooks; main thread.

#ifndef PORT_ONLINE_H
#define PORT_ONLINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PADStatus;

#define ONLINE_PROTOCOL_VERSION 4

#define ONLINE_MAX_MACHINES 4
#define ONLINE_MAX_SLOTS 4
#define ONLINE_MAX_LOCAL_HUMANS 2
#define ONLINE_MAX_STADIUMS 7
#define ONLINE_MAX_NEARBY 8
#define ONLINE_MIN_DELAY_TICKS 0
#define ONLINE_MAX_DELAY_TICKS 8
#define ONLINE_TICK_MS 20

// Bytes of UTF-8 in a nickname or player name, without the terminator.
#define ONLINE_NAME_MAX 16
#define ONLINE_ADDRESS_MAX 96
#define ONLINE_ERROR_MAX 160
#define ONLINE_PATH_MAX 512
#define ONLINE_MAX_OTHER_ADDRESSES 3

// UDP ports; Xbox only reaches 1025-49151.
#define ONLINE_DEFAULT_PORT 41210
#define ONLINE_DISCOVERY_PORT 41211

// Encoded size of OnlineMatchDesc on the wire and in replays.
#define ONLINE_DESC_BYTES 64

// Bits of OnlineMatchDesc.cheats.
enum
{
    ONLINE_CHEAT_STUNNED_GOALIES = 1 << 0,
    ONLINE_CHEAT_INFINITE_POWERUPS = 1 << 1,
    ONLINE_CHEAT_TILTING_FIELD = 1 << 2,
    ONLINE_CHEAT_PERFECT_STRIKES = 1 << 3,
    ONLINE_CHEAT_ALL = 0x0F
};

// Bits of OnlineMatchDesc.powerupMask, in PowerupSettings' field order.
enum
{
    ONLINE_POWERUP_RED_SHELLS = 1 << 0,
    ONLINE_POWERUP_GREEN_SHELLS = 1 << 1,
    ONLINE_POWERUP_BLUE_SHELLS = 1 << 2,
    ONLINE_POWERUP_SPINY_SHELLS = 1 << 3,
    ONLINE_POWERUP_STARMAN = 1 << 4,
    ONLINE_POWERUP_TWISTER = 1 << 5,
    ONLINE_POWERUP_BOBOMBS = 1 << 6,
    ONLINE_POWERUP_BANANAS = 1 << 7
};

// What makes two machines' matches the same; on the wire, 64 bytes little-endian in field order.
typedef struct OnlineMatchDesc
{
    uint16_t protocol;          // ONLINE_PROTOCOL_VERSION
    uint16_t gameTime;          // seconds: 120, 180, 240, 300, 600, 900, 1200, 1800, 2700 or 3600
    uint32_t build;             // hash of the version string, binary family and protocol
    uint32_t dataFamily;        // hash of the simulation data and parsed settings
    uint32_t reserved0;         // zero
    uint32_t seed;              // gameplay RNG seed at BeginGame
    int8_t   team[2];           // eTeamID 0-8, home then away
    int8_t   sidekick[2];       // eSidekickID 0-3
    int8_t   padSide[4];        // per slot: -1 none, 0 home, 1 away
    int8_t   slotMachine[4];    // per slot: the machine whose human plays it, or -1
    int8_t   stadium;           // eStadiumID 0-6
    uint8_t  skill;             // GameplaySettings::eSkillLevel 0-4
    uint8_t  powerUps;          // GameplaySettings::PowerUps
    uint8_t  powerupMask;       // ONLINE_POWERUP_* bits
    uint8_t  customPowerups;    // CheatSettings::CustomPowerups 0-5
    uint8_t  cheats;            // ONLINE_CHEAT_* bits
    uint8_t  shoot2Score;       // Super Strike on
    uint8_t  bowser;            // GameplaySettings::BowserAttackEnabled
    uint8_t  bowserRepeat;      // the bowser_repeat ini key
    uint8_t  pureGame;          // the pure_game ini key
    uint8_t  pauseAllowed;      // Start pauses every machine instead of opening a personal menu
    uint8_t  reserved[21];
} OnlineMatchDesc;

// A valid descriptor: Mario against Luigi at the Peach and Toad stadium, no humans, identity 0.
void PortOnlineDescDefault(OnlineMatchDesc* desc);

// The length `step` entries after `gameTime` in the accepted list, wrapping; the first if unlisted.
uint16_t PortOnlineGameTimeStep(uint16_t gameTime, int step);

// NULL when every field is in range, else the name of the first field that is not.
const char* PortOnlineDescCheck(const OnlineMatchDesc* desc);

// The descriptor's wire bytes.
void PortOnlineDescEncode(const OnlineMatchDesc* desc, uint8_t out[ONLINE_DESC_BYTES]);

// Parses and checks wire bytes; `out` is written only when the result is NULL.
const char* PortOnlineDescDecode(const uint8_t in[ONLINE_DESC_BYTES], OnlineMatchDesc* out);

// FNV-1a 64 of the encoded bytes; machines compare it before loading.
uint64_t PortOnlineDescHash(const OnlineMatchDesc* desc);

// `in` as a name in `out`: whole UTF-8 characters, no controls, trimmed; returns its length.
int PortOnlineCleanName(char out[ONLINE_NAME_MAX + 1], const char* in);

enum
{
    ONLINE_PHASE_IDLE = 0,
    ONLINE_PHASE_HOSTING,       // the room is open and no guest has joined
    ONLINE_PHASE_JOINING,       // contacting a room; joinStage says how far
    ONLINE_PHASE_ROOM,          // in a room with at least one other machine, between matches
    ONLINE_PHASE_LOADING,       // a match is agreed; machines load and compile pipelines
    ONLINE_PHASE_MATCH,
    ONLINE_PHASE_POST_MATCH,    // after the final whistle; humans vote
    ONLINE_PHASE_ERROR          // lastError says why; PortOnlineLeave returns to idle
};

enum
{
    ONLINE_JOIN_RESOLVING = 0,
    ONLINE_JOIN_CONTACTING,     // hello sent, no answer yet
    ONLINE_JOIN_HANDSHAKE,      // keys exchanged, compatibility being checked
    ONLINE_JOIN_ADMITTED
};

enum
{
    ONLINE_ROOM_NONE = 0,
    ONLINE_ROOM_LAN,
    ONLINE_ROOM_DIRECT
};

// How far a host has got finding the address players outside its network join.
enum
{
    ONLINE_PUBLIC_NONE = 0,   // not hosting, or this platform never looks
    ONLINE_PUBLIC_LOOKING,    // asking the router and a STUN server
    ONLINE_PUBLIC_OPEN,       // the router forwarded the port: publicAddress takes joins
    ONLINE_PUBLIC_FOUND,      // STUN answered and the router did not open the port
    ONLINE_PUBLIC_SHARED,     // the internet sees an address other homes share
    ONLINE_PUBLIC_FAILED      // neither answered
};

enum
{
    ONLINE_PLATFORM_UNKNOWN = 0,
    ONLINE_PLATFORM_MACOS,
    ONLINE_PLATFORM_WINDOWS,
    ONLINE_PLATFORM_LINUX,
    ONLINE_PLATFORM_STEAM_DECK,
    ONLINE_PLATFORM_SWITCH,
    ONLINE_PLATFORM_XBOX,
    ONLINE_PLATFORM_COUNT
};

// Display name of an ONLINE_PLATFORM_* value; never NULL.
const char* PortOnlinePlatformName(int platform);

enum
{
    ONLINE_LOAD_IDLE = 0,
    ONLINE_LOAD_LOADING,
    ONLINE_LOAD_PIPELINES,      // loaded, compiling pipelines; loadPercent counts them
    ONLINE_LOAD_READY
};

enum
{
    ONLINE_VOTE_NONE = 0,
    ONLINE_VOTE_REMATCH,
    ONLINE_VOTE_CHANGE_TEAMS,
    ONLINE_VOTE_BACK_TO_ROOM,
    ONLINE_VOTE_LEAVE
};

// The one condition the in-match corner icon names; the worst wins.
enum
{
    ONLINE_PROBLEM_NONE = 0,
    ONLINE_PROBLEM_HIGH_LATENCY,
    ONLINE_PROBLEM_UNSTABLE,        // jitter
    ONLINE_PROBLEM_PACKET_LOSS,
    ONLINE_PROBLEM_SLOW_DEVICE,     // the named machine keeps falling behind
    ONLINE_PROBLEM_SLOW_LOADING,    // the named machine is still loading after 30 s
    ONLINE_PROBLEM_WAITING          // no input from the named machine; countdown to its drop
};

enum
{
    ONLINE_DESYNC_NONE = 0,
    ONLINE_DESYNC_STOPPED,      // the match stopped at desyncTick and a report was written
    ONLINE_DESYNC_ENDED         // the desynced match ended; everyone returns to the room
};

enum
{
    ONLINE_ERR_NONE = 0,
    ONLINE_ERR_UNAVAILABLE,     // online play is not in this build
    ONLINE_ERR_NETWORK,         // no usable network or socket
    ONLINE_ERR_BAD_ADDRESS,
    ONLINE_ERR_PORT_IN_USE,
    ONLINE_ERR_CANT_HOST,       // this platform never waits for connections
    ONLINE_ERR_UNREACHABLE,
    ONLINE_ERR_ROOM_FULL,
    ONLINE_ERR_IN_MATCH,        // the room is playing; join after the match
    ONLINE_ERR_VERSION,
    ONLINE_ERR_DATA_FAMILY,
    ONLINE_ERR_HOST_LEFT,
    ONLINE_ERR_CONNECTION_LOST,
    ONLINE_ERR_PROTOCOL,        // a peer kept sending what this build cannot read
    ONLINE_ERR_SETUP_MISMATCH   // descriptor hashes differed before loading
};

// A one-off event for a toast; noticeSerial changes each time one is raised.
enum
{
    ONLINE_NOTICE_NONE = 0,
    ONLINE_NOTICE_JOINED,           // noticeMachine joined the room
    ONLINE_NOTICE_LEFT,             // noticeMachine left the room
    ONLINE_NOTICE_DROPPED_TO_CPU,   // noticeMachine dropped mid-match; the CPU plays its slots
    ONLINE_NOTICE_SETUP_MISMATCH,   // the match was called off; everyone is back in the room
    ONLINE_NOTICE_REMATCH           // every human voted for a rematch
};

typedef struct OnlineHuman
{
    char    name[ONLINE_NAME_MAX + 1];
    int8_t  slot;           // logical pad 0-3, the game's pad index
    int8_t  side;           // the descriptor's padSide for that slot
    int8_t  localPort;      // on this machine, the device port that feeds it; -1 elsewhere
    uint8_t vote;           // ONLINE_VOTE_*
} OnlineHuman;

typedef struct OnlineMachine
{
    char        nickname[ONLINE_NAME_MAX + 1];
    uint8_t     present;        // the entry is in use
    uint8_t     connected;      // heard from within the drop window
    uint8_t     isHost;
    uint8_t     isSelf;
    uint8_t     platform;       // ONLINE_PLATFORM_*
    uint8_t     ready;          // ready in the room
    uint8_t     descAcked;      // acknowledged the current descriptor's hash
    uint8_t     loadState;      // ONLINE_LOAD_*
    uint8_t     loadPercent;    // 0-100
    uint8_t     lossPercent;    // datagrams lost over the last 10 s
    uint8_t     humanCount;     // humans[0..humanCount)
    uint16_t    pingMs;         // round trip from this machine; 0 for itself
    uint16_t    jitterMs;       // standard deviation of the one-way delay
    OnlineHuman humans[ONLINE_MAX_LOCAL_HUMANS];
} OnlineMachine;

typedef struct OnlineNearbyRoom
{
    char     address[ONLINE_ADDRESS_MAX];   // "host:port", ready for PortOnlineJoin
    char     hostName[ONLINE_NAME_MAX + 1];
    uint8_t  platform;
    uint8_t  machines;
    uint8_t  humans;
    uint8_t  inMatch;
    uint8_t  compatible;    // same protocol, build and data family as this machine
    uint16_t ageMs;         // since its last answer
} OnlineNearbyRoom;

typedef struct OnlineSettings
{
    char     nickname[ONLINE_NAME_MAX + 1];
    uint8_t  delayTicks;    // 0 for Auto, else 1-8
    uint8_t  showStats;     // the in-match stats line
    uint16_t hostPort;      // UDP port a hosted room listens on
} OnlineSettings;

enum
{
    ONLINE_SELECT_BEGIN = 1,
    ONLINE_SELECT_CANCEL,
    ONLINE_SELECT_TEAM,
    ONLINE_SELECT_BACK,
    ONLINE_SELECT_FINISH,
    ONLINE_SELECT_OPTIONS
};

// Room selection: 0 lobby, 1 captains, 2 stadium, 3 waiting for the final descriptor.
typedef struct OnlineSelection
{
    uint32_t epoch;
    uint8_t stage;
    uint8_t phase[2];
    uint8_t optionsOpen;
} OnlineSelection;

int PortOnlineSelect(int op, int side, int team, int sidekick, int phase);

typedef struct OnlineState
{
    int phase;              // ONLINE_PHASE_*
    int joinStage;          // ONLINE_JOIN_*, while joining
    float joinSeconds;      // since the join began

    // The address guests use.
    int roomKind;           // ONLINE_ROOM_*: how this machine reached the room; a host's is DIRECT
    char roomAddress[ONLINE_ADDRESS_MAX];
    uint16_t roomPort;
    // A host's other addresses with the room's port, such as a VPN's.
    char roomOtherAddresses[ONLINE_MAX_OTHER_ADDRESSES][24];
    int roomOtherAddressCount;
    // A host's internet address and room port; empty until found.
    int publicState;        // ONLINE_PUBLIC_*
    char publicAddress[ONLINE_ADDRESS_MAX];
    uint32_t roomId;        // non-zero in a room; the same on every machine in it

    int self;               // this machine's index, or -1
    int host;               // the host's index, or -1
    int machineCount;
    OnlineMachine machines[ONLINE_MAX_MACHINES];    // by machine index; `present` marks use
    int canStart;           // non-zero when this machine may call PortOnlineStart

    // The host's proposal between matches, the agreed match from loading on.
    OnlineMatchDesc desc;
    OnlineSelection selection;
    uint64_t descHash;
    uint32_t matchSerial;   // changes each time a new match (or rematch) is agreed

    // Input delay: expected from pings in the room, agreed from each kickoff.
    int delayTicks;
    int delayMs;            // delayTicks x ONLINE_TICK_MS
    int delayAuto;

    uint32_t tick;              // the next tick this machine runs
    uint32_t confirmedTick;     // ticks below this have every machine's input
    float advanceTicks;         // ahead (+) or behind (-) the slowest machine, 1 s average
    int stallsLastMinute;
    int longestStallMs;         // over the last minute
    int inputToEffectMs;        // pad edge to the first frame whose tick holds it; 0 unmeasured

    int problem;                // ONLINE_PROBLEM_*
    int problemMachine;         // the machine it names, or -1
    float problemSeconds;       // WAITING: seconds until the drop; SLOW_LOADING: seconds waited

    int opponentsLeft;          // every human on the other side is gone; answered by a command

    int desync;                 // ONLINE_DESYNC_*
    uint32_t desyncTick;
    int desyncCount;            // in this match
    char desyncComponent[32];   // the first differing hash component, when known
    char desyncReport[ONLINE_PATH_MAX];

    float voteSeconds;          // post-match: seconds left in the rematch window

    int nearbySearching;
    int nearbyCount;
    OnlineNearbyRoom nearby[ONLINE_MAX_NEARBY];

    uint32_t noticeSerial;
    int notice;                 // ONLINE_NOTICE_*
    int noticeMachine;
    char noticeName[ONLINE_NAME_MAX + 1];

    int error;                  // ONLINE_ERR_*
    char lastError[ONLINE_ERROR_MAX];   // one sentence, empty when error is ONLINE_ERR_NONE

    OnlineSettings settings;
} OnlineState;

// The session's state as of the last PortOnlineUpdate; valid until the next one.
const OnlineState* PortOnlineGetState(void);

// The commands return non-zero when accepted; a refusal changes nothing.

// Opens a room Nearby finds on `port` (0: the setting); local port 0's human is `nickname`.
int PortOnlineHost(uint16_t port, const char* nickname);

// Joins the room at "host", "host:port", "a.b.c.d:port" or "[v6]:port".
int PortOnlineJoin(const char* address, const char* nickname);

// Starts a LAN search; results fill `nearby` over the next second.
int PortOnlineRefreshNearby(void);

// Leaves the room or match, or clears an error; always back to idle.
void PortOnlineLeave(void);

// Adds a human on this machine fed by the device on `localPort`; `name` NULL for a default.
int PortOnlineAddLocalPlayer(int localPort, const char* name);

// Removes the local human fed by `localPort`.
int PortOnlineRemoveLocalPlayer(int localPort);

// Host only, between matches; the session sets the identity fields, seed and slotMachine. A
// non-zero seed is kept for the next start only, so a test can replay a match.
int PortOnlineSetDesc(const OnlineMatchDesc* desc);

// This machine's ready flag in the room.
int PortOnlineSetReady(int ready);

// Host only, all ready: seeds and sends the proposal; loading starts once every machine acks it.
int PortOnlineStart(void);

// Post-match vote for the local human on `slot`, or every local human for -1.
int PortOnlineVote(int slot, int vote);

// After opponentsLeft: non-zero plays on against the CPU, zero ends the match.
int PortOnlineAfterOpponentsLeft(int continueAgainstCpu);

// Applies and saves settings; the nickname is cleaned and delayTicks clamped.
int PortOnlineSetSettings(const OnlineSettings* settings);

// Once at boot, after the config is loaded: reads the online settings.
void PortOnlineInit(void);

// At exit: says goodbye to peers and closes sockets.
void PortOnlineShutdown(void);

// Each rendered frame, after the pad sample and before the tasks; `timeScale` is the sim's.
void PortOnlineUpdate(float timeScale);

// Non-zero while this machine is in a room or match; debug ops and overrides stay off.
int PortOnlineActive(void);

// Non-zero from the agreed match's first tick until its final whistle or abandonment.
int PortOnlineInMatch(void);

// Each rendered frame, after PADRead: the four local device ports, for local slots and taps.
void PortOnlineSampleLocalPads(const struct PADStatus* ports);

// Each slot's newest confirmed input over the frame's pads, remote slots connected; 0 offline.
int PortOnlineRenderPads(struct PADStatus* pads);

enum
{
    ONLINE_SLOT_EMPTY = 0,
    ONLINE_SLOT_LOCAL,
    ONLINE_SLOT_REMOTE,
    ONLINE_SLOT_CPU         // its machine dropped; it reads neutral input with err 0
};

// Offline, every slot is ONLINE_SLOT_LOCAL and its own local port.
int PortOnlineSlotKind(int slot);
int PortOnlineSlotLocalPort(int slot);
const char* PortOnlineSlotName(int slot);

// The loading screen: progress 0-100, then loaded with the pipeline queue drained.
void PortOnlineLoadProgress(int percent);
void PortOnlineMatchLoaded(void);

// Non-zero once every machine has loaded and the host's start time has come.
int PortOnlineMayBeginMatch(void);

// The final whistle, from inside the tick that ends the match.
void PortOnlineMatchEnded(void);

// Slot input for `tick`, missing input guessed from the newest; 0 once past what snapshots reach.
int PortOnlineTickInput(unsigned int tick, struct PADStatus* pads);

// The earliest tick that ran on a wrong guess, 0xFFFFFFFF for none; asking clears it.
unsigned int PortOnlineRollbackTo(void);

// The ticks from `tick` on, which ran before a freeze held this machine, run again.
void PortOnlineRewind(unsigned int tick);

// Every machine's input is in for the ticks before this one.
unsigned int PortOnlineConfirmedTick(void);

// As PortOnlineConfirmedTick, but not past a tick still waiting to run again.
unsigned int PortOnlineSettledTick(void);

// The combined state hash of `tick`, after the tick ran.
void PortOnlineTickHash(unsigned int tick, uint64_t hash);

// Per-component hashes of `tick`, every 50 ticks, for desync reports; `names` outlive the call.
void PortOnlineTickComponents(unsigned int tick, const uint64_t* hashes,
                              const char* const* names, int count);

// Bits of OnlineTickActions.actions.
enum
{
    ONLINE_ACTION_RESUME = 1 << 0,      // the barrier `barrier` ends before this tick
    ONLINE_ACTION_RESEED = 1 << 1,      // install `seed` as the gameplay seed
    ONLINE_ACTION_SLOTS_TO_CPU = 1 << 2,    // `cpuSlots` lose their humans
    ONLINE_ACTION_END_MATCH = 1 << 3    // the match ends unfinished at this tick
};

typedef struct OnlineTickActions
{
    uint32_t actions;       // ONLINE_ACTION_*
    int      barrier;       // PORT_BARRIER_*, with ONLINE_ACTION_RESUME
    uint32_t seed;
    uint8_t  cpuSlots;      // bit per slot
} OnlineTickActions;

// The actions stamped for `tick`; returns non-zero when there are any.
int PortOnlineTickActions(unsigned int tick, OnlineTickActions* out);

// Barriers: the simulation froze for `kind` before `tick`; its presentation here ended; may it run.
void PortOnlineBarrierReached(int kind, unsigned int tick);
void PortOnlineBarrierDone(int kind);

// A shared skip belongs to one cutscene in this match; zero means no cutscene.
int PortOnlinePresentationSkip(unsigned int presentation, int pressed);
int PortOnlineMayRunTick(unsigned int tick);

// Factor on the frame time that feeds the tick schedule: time sync and frame stretching.
float PortOnlineClockRate(void);

#ifdef __cplusplus
}
#endif

#endif   // PORT_ONLINE_H
