
#ifndef _PORT_OVERLAY_H_
#define _PORT_OVERLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

// Reads STRIKERS_OVERLAY once. Safe to call repeatedly.
void PortOverlayInit(void);
int PortOverlayEnabled(void);

// Draw it. Call between aurora_begin_frame() and aurora_end_frame(); doing it anywhere else means
// drawing into an ImGui frame that is not open.
void PortOverlayDraw(void);

// Labels, pushed from game code rather than pulled from it.
void PortOverlaySetScene(const char* name);
void PortOverlaySetStadium(const char* name);
void PortOverlaySetMatch(float clock, int scoreHome, int scoreAway);

// The last of those labels, read back; the control channel's heartbeat prints it and has no window
// to draw into. Never NULL.
const char* PortOverlaySceneName(void);
void PortOverlayToggleMenu(void);
int  PortOverlayMenuOpen(void);
void PortOverlayHandleKey(int scancode, int down);

// Set by the menu's quit button; main()'s loop ends when it reads non-zero.
int PortQuitRequested(void);
void PortRequestQuit(void);

// Non-zero when the control channel's dump wants the per-frame state push with the menu closed; the
// game tests it alongside PortOverlayMenuOpen().
int PortDebugStateWanted(void);

// The stadium the menu wants PickStadium to return, or -1 for no preference.
int  PortDebugForcedStadium(void);
void PortDebugSetForcedStadium(int index);
void PortDebugSetPad(int port, int err, unsigned int buttons, int stickX, int stickY,
                     int substickX, int substickY, int triggerL, int triggerR);
typedef struct PortDebugCharacter
{
    float pos[3];
    int   team;        // 0 or 1
    int   isGoalie;
    int   state;       // action state, meaning depends on isGoalie
    int   hasBall;

    char  name[16];    // the template's own name: "mario", "toad", ...
    int   characterClass;
    int   isCaptain;
    int   isHuman;     // a pad is driving it
    char  anim[32];    // the animation inventory's enumerator for m_eAnimID
    int   desire;      // eFielderDesireState, fielders only
    int   role;        // eRole, fielders only
    int   powerup;     // ePowerUpType held, -1 for none; fielders only
    int   powerupCount;
    float frozenSeconds;
    int   invincible;
    int   fallen;
    int   card;        // ePenaltyCardStatus
    int   shotMeterState;
    float shotMeterValue;
    float speed;
    float energy;      // goalie fatigue, goalies only
    int   urgency;     // goalies only
} PortDebugCharacter;

typedef struct PortDebugTeam
{
    int   score;
    float powerupMeter;      // 0..1
    int   powerup[2];        // the two inventory slots, -1 for empty
    int   powerupCount[2];
    int   situation;         // eSituation
    int   style;             // eTeamStyle
    int   difficulty;        // eDifficultyID
    int   teamId;            // eTeamID
    int   sidekickId;        // eSidekickID
} PortDebugTeam;

typedef struct PortDebugMatch
{
    int   valid;
    int   gameState;
    int   inSuddenDeath;
    float clock;
    float duration;
    int   scoreHome;
    int   scoreAway;

    float ballPos[3];
    float ballVel[3];
    int   ballOwner;        // index into characters, -1 for none
    int   ballHasPassTarget;
    float ballNoPickup;     // seconds left on the ball's pickup lockout

    int   characterCount;
    PortDebugCharacter characters[10];
    PortDebugTeam teams[2];

    int   lastTeamToScore;
    int   isPure;            // no powerups this match
    int   powerupObjects;    // live item objects on the pitch
    float bowserTimer;
    int   bowserAlive;
    int   effects;           // live emission controllers
    char  presentation[64];
    float presentationTime;
    int   nisActive;
    int   skipAllowed;
    int   cameraType;        // eCameraType
    float camPos[3];
    float camTarget[3];
    float camFov;
    int   cameraTypeWanted;  // g_eCurrentCameraType, the bottom of the stack
    int   skillLevel;
    int   gameTime;
    int   optPowerUps, optShoot2Score, optBowser, optRumble;
    int   cheatInfinite, cheatStunned, cheatTilt, cheatPerfect, cheatCustom;
    int   trophies[2];

    // Who is playing which side: pad -> side, -1 for none.
    int   padSide[4];
    int   gameMode;
    int   demoMode;
} PortDebugMatch;

void PortDebugSetMatch(const PortDebugMatch* match);

// The last one pushed, read back; before the first push it is the zeroed struct, which reads as
// valid=0.
const PortDebugMatch* PortDebugGetMatch(void);
typedef struct PortDebugSession
{
    int taskState;          // nlTaskManager::m_CurrState
    int feDepth;            // GameSceneManager stack depth
    int feStack[8];         // its top entries, bottom first, as SceneList
    int overlayDepth;       // OverlayManager stack depth
    int overlayStack[8];
    int inPauseMenu;
    int frontEndState;
} PortDebugSession;

void PortDebugSetSession(const PortDebugSession* session);
const PortDebugSession* PortDebugGetSession(void);
typedef struct PortGfxArena
{
    unsigned int used, size;
    int  markerLevel;
    int  typeCount;
    char typeNames[8][16];
    unsigned int bytes[8][8];   // [level][type]
} PortGfxArena;

enum PortDebugOp
{
    // Numbered explicitly: STRIKERS_DEBUG_CMD names an op by number, and a script must not break
    // because an op was added in the middle.
    PDBG_NONE = 0,

    // Match. a = team, b = value, f[0] = seconds.
    PDBG_SET_SCORE = 1,          // a team, b score
    PDBG_ADD_CLOCK = 2,          // f[0] seconds added to the time remaining
    PDBG_SET_DURATION = 3,       // f[0] seconds
    PDBG_END_MATCH = 4,
    PDBG_SUDDEN_DEATH = 5,
    PDBG_KICKOFF = 6,            // a = team that kicks off
    PDBG_GIVE_BALL = 7,          // a = character index
    PDBG_WARP_BALL = 8,          // f[0..2] position; f[3] != 0: into the net f[0] < 0 ? home : away
    PDBG_SKIP_PRESENTATION = 9,
    PDBG_SET_DIFFICULTY = 10,    // a home, b away (eDifficultyID)
    PDBG_SET_SIDE = 11,          // a pad, b side (-1 none)
    PDBG_FREEZE_PLAYER = 12,     // a character, f[0] seconds
    PDBG_GIVE_POWERUP = 13,      // a team, b type, c count
    PDBG_THROW_POWERUP = 14,     // a character, b type, c count
    PDBG_AWARD_POWERUP = 15,     // a team; the game's own random award
    PDBG_CLEAR_POWERUPS = 16,    // a = 1 to clear inventories too
    PDBG_BOWSER_ATTACK = 17,
    PDBG_BOWSER_HIDE = 18,
    PDBG_KILL_EFFECTS = 19,

    // Camera: deliberately none. Switching the game camera type is the retail PAD_SWITCH_CAMERA
    // path and corrupts memory on this port; see rw_game_debugcommands.

    // Settings and cheats. a = which, b = value.
    PDBG_SET_OPTION = 20,
    PDBG_SET_CLASS_FLAG = 21,    // a = which, b = value
    PDBG_SET_CONFIG_STRING = 22, // s "key=value"; the retail line-up keys, read at the next match
    PDBG_PAUSE_MENU = 23,        // a = 1 enter, 0 exit
    PDBG_RETURN_TO_FE = 24,
    PDBG_START_MATCH = 25,       // a team1, b team2, c stadium; f[0] sidekick1, f[1] sidekick2, f[2] human side
    PDBG_SET_VOLUME = 26,        // a group, f[0]
    PDBG_PLAY_SFX = 27,          // s name
    PDBG_SILENCE = 28,

    // Diagnostics for a scripted run: each prints one line to stderr and changes nothing.
    PDBG_DUMP_USER = 29,         // "[user] dump ...", the save record in memory
    PDBG_DUMP_CUP = 30,          // "[cup] dump ...", mode, round and game number

    // The platform's own, executed where the queue is drained (inside PortDebugPopCommand) so the
    // game's dispatcher never sees them; each prints a [limiter] line.
    PDBG_SET_FRAME_LIMIT = 31,   // f[0] Hz; 0 uncapped; negative follows STRIKERS_FPS_LIMIT and the display
    PDBG_SET_VSYNC = 32,         // a = 1 on, 0 off; the limiter's margin follows
    // The game's own g_bRunSimAndRenderInLockStep: one 20 ms step per rendered frame, which runs
    // the game at fps/50 times real time.
    PDBG_SET_LOCKSTEP = 33,      // a = 1 on, 0 off
};
enum PortDebugOption
{
    PDBG_OPT_POWERUPS = 0, PDBG_OPT_SHOOT2SCORE, PDBG_OPT_BOWSER, PDBG_OPT_RUMBLE,
    PDBG_OPT_INFINITE, PDBG_OPT_STUNNED, PDBG_OPT_TILT, PDBG_OPT_PERFECT, PDBG_OPT_CUSTOM,
    PDBG_OPT_SKILL, PDBG_OPT_PURE, PDBG_OPT_TROPHIES,
};

// PDBG_SET_CLASS_FLAG targets: class statics the platform cannot name.
enum PortDebugClassFlag
{
    PDBG_CF_STADIUM_OFF = 0, PDBG_CF_SKYBOX_OFF, PDBG_CF_CHAR_SHADOWS_OFF,
};

typedef struct PortDebugCommand
{
    int   op;
    int   a, b, c;
    float f[4];
    char  s[64];
} PortDebugCommand;

// Queue a command. Returns 0 if the queue is full (it holds 32; the game drains it every frame, so
// full means the game is not running its loop).
int PortDebugPushCommand(const PortDebugCommand* cmd);
int PortDebugPopCommand(PortDebugCommand* out);

#ifdef __cplusplus
}
#endif

#endif // _PORT_OVERLAY_H_
