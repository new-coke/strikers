// The debug overlay and menu. See include/port/overlay.h for why it is ImGui, where it is drawn,
// and why game state is pushed here rather than pulled.

#include "port/overlay.h"
#include "port/aspect.h"
#include "port/control.h"
#include "port/audio.h"
#include "port/benchmark.h"
#include "port/config.h"
#include "port/framerate.h"
#include "port/host.h"
#include "port/input.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <imgui.h>

#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <dolphin/pad.h>
#include <SDL3/SDL.h>

// Aurora's pool and texture accounting. Both are measurements of Aurora's own allocations that
// nothing outside Aurora could otherwise see, the staging pools are 378 MiB of this process and are
// not visible from the game side at all.
extern "C" {
void aurora_gfx_pool_stats(uint32_t* peakBytes, uint32_t* reservedBytes, size_t count);
void aurora_gfx_texture_stats(uint64_t* srcBytes, uint64_t* uploadedBytes, uint64_t* count);
void aurora_capture_frame(const char* path);

// The graphics resource arena, from glxMemory.cpp (rw_glxmemory_report).
int PortGfxArenaStats(PortGfxArena* out);

// STRIKERS_UNLOCK_ALL, settable (debughooks.cpp).
void PortSetUnlockAll(int on);
int  PortGetUnlockAll(void);
}

// The game's own debug switches. Every one of these shipped in the retail build and is set nowhere
// on the disc.
extern bool g_bRenderWorld;                 // GameRenderTask.cpp
extern bool g_bEnableGamecubePadMonkey;     // PadActions.cpp, random input
extern bool g_bRunSimAndRenderInLockStep;   // FixedUpdateTask.cpp
extern bool g_bFrameStatsOnScreen;          // BeginFrameTask.cpp
extern bool g_bFrameSmiler;                 // BeginFrameTask.cpp
extern bool g_bCoPlanarPerObject;           // DrawableModel.cpp
extern bool g_bDrawSafeFrame;               // BeginFrameTask.cpp  (was static)
extern int  g_nGridDisplaySpacing;          // BeginFrameTask.cpp  (was static)
extern unsigned char sbNoBallPickups;       // Player.cpp          (was static)
extern unsigned char g_bShadowBlobs;        // RenderShadow.cpp    (was static)
extern unsigned char g_bPreview;            // RenderShadow.cpp    (was static)
extern unsigned char g_bShadowBounds;       // RenderShadow.cpp    (was static)
extern unsigned char g_bClipToFrustum;      // world.cpp           (was static)
extern unsigned char g_bDrawBoundingSphere; // world.cpp           (was static)
extern unsigned char g_bFreezeFrustum;      // world.cpp           (was static)
extern unsigned char g_bDrawCullingInfo;    // world.cpp           (was static)
extern unsigned char g_bWhiteDiffuse;       // GameRenderTask.cpp  (was static)
extern unsigned char g_TexDetail;           // GameRenderTask.cpp  (was static)
extern unsigned char g_TexShadow;           // GameRenderTask.cpp  (was static)
extern unsigned char g_TexSelfIllum;        // GameRenderTask.cpp  (was static)
extern unsigned char g_TexGloss;            // GameRenderTask.cpp  (was static)
extern unsigned char g_bTexelDensity;       // GameRenderTask.cpp  (was static)
extern unsigned char g_bMemoryOnScreen;     // GameRenderTask.cpp  (was static)
extern bool g_bShadowVolumes;               // DrawableModel.cpp   (was static)
extern unsigned char g_bDrawBoundingBoxes;  // DrawableModel.cpp   (was static)
extern bool g_bEnableDrawableModel;         // DrawableModel.cpp   (was static)
extern bool g_bDrawPlanarShadows;           // DrawableModel.cpp   (was static)
extern bool g_bBallGlow;                    // DrawableModel.cpp   (was static)
extern bool g_bEnableDrawableSkinModel;     // DrawableSkinModel.cpp (was static)
extern int  g_nShowBones;                   // DrawableCharacter.cpp (was static)
extern unsigned char g_hudVisible;          // OverlayHandlerHUD.cpp (was static)
extern bool sbDisableCollisionDetection;    // Physics.cpp         (was static)
extern bool g_bAllowLighting;               // glxSend.cpp         (was static)
extern bool g_bAllowSpecular;               // glxSend.cpp         (was static)
extern unsigned char sbUseCheckerTexture;   // DrawableNetMesh.cpp (was static)

// This one stays: FixedUpdateTask.cpp defines it `extern "C"` (see rw_fixedupdate_timescale), so C
// linkage here is what matches the definition.
extern "C" {
float* PortSimTimeScalePtr(void);           // FixedUpdateTask.cpp, rw_fixedupdate_timescale
}

namespace
{

int s_enabled;
int s_read;

char s_scene[64] = "-";
char s_stadium[64] = "-";
float s_clock;
int s_scoreHome, s_scoreAway;

bool s_menuOpen;
bool s_quit;
PortDebugMatch s_match;
PortDebugSession s_session;
int s_forcedStadium = -2;   // -2: not yet read from the environment

struct PadSnapshot
{
    int err;
    unsigned int buttons;
    int stickX, stickY, substickX, substickY, triggerL, triggerR;
};
PadSnapshot s_pad[4];

// Commands, menu -> game.

const int kCommandCap = 32;
PortDebugCommand s_commands[kCommandCap];
int s_commandHead, s_commandCount;

void cmd(int op, int a = 0, int b = 0, int c = 0, float f0 = 0.0f, float f1 = 0.0f,
         float f2 = 0.0f, float f3 = 0.0f, const char* s = nullptr)
{
    PortDebugCommand k;
    std::memset(&k, 0, sizeof k);
    k.op = op;
    k.a = a;
    k.b = b;
    k.c = c;
    k.f[0] = f0;
    k.f[1] = f1;
    k.f[2] = f2;
    k.f[3] = f3;
    if (s != nullptr)
    {
        std::strncpy(k.s, s, sizeof k.s - 1);
    }
    PortDebugPushCommand(&k);
}

// clang-format off
const char* const kGoalieStates[] = {   // Goalie.h eGoalieActionState
    "move", "move w/ball", "save setup", "save reposition", "save",
    "miss chip shot", "dive recover", "sts setup", "sts", "sts recover",
    "sts attack setup", "sts attack", "pass", "pass intercept", "pre-crouch",
    "pursue carrier", "pursue pounce", "loose setup", "loose catch",
    "loose pickup", "loose bouncing", "loose rolling", "loose desperate",
    "offplay", "snap ball", "grab ball",
};
const char* const kFielderStates[] = {  // AI/Fielder.h eFielderActionState, from 0
    "deke", "electrocution", "hit", "hit react", "idle turn",
    "late onetimer", "loose pass", "loose shot", "onetimer", "onetouch pass",
    "pass", "post whistle", "receive pass", "running", "running w/ball",
    "turbo", "turbo turn", "shot", "SHOOT TO SCORE", "slide attack",
    "slide react", "bomb react", "shell react", "banana react", "sts hit react",
    "squish react", "slide fail react", "wait",
};
const char* const kDesires[] = {        // AI/ScriptAction.h eFielderDesireState
    "need desire", "cut and break", "deke", "get in position", "get open",
    "hit", "intercept ball", "mark", "protect ball", "run to net",
    "run upfield", "run downfield", "run to location", "pass", "shoot",
    "slide attack", "support def", "support off", "use powerup",
    "windup pass", "windup shot", "wait for thought", "USER", "finish action",
    "onetimer", "post whistle", "receive (idle)", "receive (run)", "wait",
};
const char* const kRoles[] = { "striker", "winger", "midfield", "defence" };  // eRole
const char* const kPowerups[] = {       // AI/Powerups.h ePowerUpType
    "green shell", "red shell", "spiny shell", "freeze shell", "banana",
    "bob-omb", "chain chomp", "mushroom", "star",
};
const char* const kGameStates[] = {     // Game.h eGameState, from 0
    "pre-game", "kickoff", "post goal", "end game", "gameplay", "overtime",
};
const char* const kCameraTypes[] = {    // Camera/BaseCamera.h eCameraType
    "debug (orbit)", "follow character", "follow ball", "replay", "animated",
    "kickoff", "top down", "gameplay", "matrix effect", "goal",
    "shoot to score", "anim viewer", "face closeup",
};
const char* const kDifficulties[] = {   // GameTweaks.h eDifficultyID, from 0
    "braindead", "easy", "medium", "hard", "very hard", "superhuman", "human",
};
const char* const kSkills[] = {         // DB/UserOptions.h eSkillLevel
    "training", "rookie", "professional", "superstar", "legend",
};
const char* const kCustomPowerups[] = { // DB/UserOptions.h CustomPowerups
    "off", "explosive", "freezing", "shells", "giant", "enhancement",
};
const char* const kSituations[] = { "offense", "defense", "loose" };          // eSituation
const char* const kStyles[] = { "aggressive", "moderate", "passive" };        // eTeamStyle
const char* const kUrgency[] = { "low", "med", "high" };                      // eUrgency
const char* const kCards[] = { "yellow", "yellow x2", "RED" };                // ePenaltyCardStatus, from 0
const char* const kShotMeter[] = { "-", "charging", "released", "sts charging", "sts transition", "sts released" };
const char* const kTeams[] = {          // Team.h eTeamID
    "daisy", "donkeykong", "luigi", "mario", "peach", "waluigi", "wario", "yoshi", "mystery",
};
const char* const kSidekicks[] = { "toad", "koopa", "hammerbro", "birdo" };   // eSidekickID
const char* const kStadiums[] = {       // DB/BasicGameInfo.h eStadiumID
    "mario_stadium", "peach_toad", "dk_daisy", "wario_stadium",
    "yoshi_stadium", "super_stadium", "forbidden_dome",
};
const char* const kGameModes[] = {      // GameInfo.h eGameModes
    "friendly", "mushroom cup", "flower cup", "star cup", "bowser cup",
    "super mushroom", "super flower", "super star", "super bowser",
    "tournament", "demo",
};
// clang-format on

template <size_t N>
const char* name_of(const char* const (&table)[N], int v)
{
    return (v >= 0 && v < (int)N) ? table[v] : "?";
}

const char* task_state_name(int s)
{
    switch (s)
    {
    case 0x1: return "pause / menu overlay";
    case 0x2: return "match";
    case 0x4: return "front end";
    case 0x10: return "auto replay";
    case 0x100: return "presentation / NIS";
    case 0x20000: return "debug replay";
    case 0x80000: return "health warning";
    default: return "?";
    }
}

// The front end's SceneList, for the stacks.
const char* scene_name(int s)
{
    switch (s)
    {
    case 2: return "title";
    case 3: return "main menu";
    case 4: case 5: case 6: case 7: return "choose sides";
    case 8: return "choose captains";
    case 9: return "stadium select";
    case 13: case 14: return "choose cup";
    case 15: case 16: return "cup captains";
    case 17: case 20: case 23: return "standings";
    case 27: return "popup";
    case 28: return "trophy room";
    case 38: return "options";
    case 39: return "legal";
    case 43: return "loading";
    case 49: return "quick options";
    case 50: return "vs transition";
    case 51: return "health warning";
    case 53: return "intro movie";
    case 54: return "credits";
    case 57: return "pause";
    case 58: return "choose sides (in game)";
    case 61: return "post game";
    case 67: return "HUD";
    case 68: return "in-game text";
    case 70: case 71: return "summary";
    case 72: return "goal overlay";
    case 74: return "demo overlay";
    case 75: return "winner";
    default: return "";
    }
}

void copy_into(char* dst, size_t cap, const char* src)
{
    if (src == nullptr)
        src = "-";
    size_t i = 0;
    for (; src[i] != '\0' && i < cap - 1; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

// The pools, in the order aurora_gfx_pool_stats reports them.
const char* const kPoolNames[] = { "vertex", "uniform", "index", "storage", "texUpload" };
constexpr size_t kPoolCount = sizeof(kPoolNames) / sizeof(kPoolNames[0]);

// The only way the menu opens or closes. Detach the keyboard from the pad while the menu has it:
// Aurora reads the keyboard itself through the bindings input.cpp installs, so without this every
// keystroke aimed at a text field also runs the player.
void set_menu_open(bool open)
{
    s_menuOpen = open;
    PADSetKeyboardActive(0, s_menuOpen ? FALSE : TRUE);
}

// A checkbox over a u8 flag. The game's switches are a mix of bool and u8, and ImGui wants a bool.
bool check_u8(const char* label, unsigned char* flag)
{
    bool v = *flag != 0;
    if (ImGui::Checkbox(label, &v))
    {
        *flag = v ? 1 : 0;
        return true;
    }
    return false;
}

// A checkbox that sends a command rather than writing a variable, for state that lives in the game.
bool check_cmd(const char* label, int current, int op, int a)
{
    bool v = current != 0;
    if (ImGui::Checkbox(label, &v))
    {
        cmd(op, a, v ? 1 : 0);
        return true;
    }
    return false;
}

void help(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// setenv is POSIX; Windows spells it _putenv_s, and clears with "".
void set_env(const char* var, const char* value)
{
#if defined(_WIN32)
    _putenv_s(var, value != nullptr ? value : "");
#else
    if (value != nullptr)
        setenv(var, value, 1);
    else
        unsetenv(var);
#endif
}

SDL_Window* main_window()
{
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* w = (windows != nullptr && count > 0) ? windows[0] : nullptr;
    SDL_free(windows);
    return w;
}

bool is_fullscreen()
{
    SDL_Window* w = main_window();
    return w != nullptr && (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0;
}

void set_fullscreen(bool on)
{
    SDL_Window* w = main_window();
    if (w != nullptr)
        SDL_SetWindowFullscreen(w, on);
}

// Environment switches the generated tree reads on every use, so flipping them here takes effect at
// once.
struct LiveEnv
{
    const char* var;
    const char* what;
};
const LiveEnv kLiveEnv[] = {
    { "STRIKERS_LOG_SCENES", "every front-end screen entered and package loaded" },
    { "STRIKERS_LOG_MATCH", "match state, once a second" },
    { "STRIKERS_LOG_NIS", "cutscene player and presentation clock" },
    { "STRIKERS_LOG_BUNDLES", "every bundle file opened" },
    { "STRIKERS_PROBE_PAD", "pad state on change" },
    { "STRIKERS_DUMP_FEN", "the converted front-end scene graph, per package" },
};

}   // namespace

void PortOverlayInit(void)
{
    if (s_read)
        return;
    s_read = 1;
    const char* env = std::getenv("STRIKERS_OVERLAY");
    s_enabled = (env != nullptr && *env != '\0' && std::strcmp(env, "0") != 0);

    // STRIKERS_OVERLAY=menu starts with the menu open rather than the compact heads-up form.
    if (env != nullptr && (std::strcmp(env, "menu") == 0 || std::strcmp(env, "2") == 0))
        set_menu_open(true);
}

int PortOverlayEnabled(void)
{
    PortOverlayInit();
    return s_enabled;
}

void PortOverlaySetScene(const char* name) { copy_into(s_scene, sizeof(s_scene), name); }
const char* PortOverlaySceneName(void) { return s_scene; }
void PortOverlaySetStadium(const char* name)
{
    copy_into(s_stadium, sizeof(s_stadium), name);
    // The summary needs this as much as the overlay does, and for a better reason: it is what says
    // whether two runs measured the same content.
    PortBenchSetLabel("stadium", s_stadium);
}

void PortOverlaySetMatch(float clock, int scoreHome, int scoreAway)
{
    s_clock = clock;
    s_scoreHome = scoreHome;
    s_scoreAway = scoreAway;
}

// The simulation rate from the snapshot pushes, one per fixed step; 50 is real time, sampled once a
// second.
static unsigned long s_simSteps;
static unsigned long long s_simWindowStart;
static double s_simStepsPerSec = 50.0;
static bool s_simRateKnown;

void PortDebugSetMatch(const PortDebugMatch* match)
{
    if (match != nullptr)
        s_match = *match;
    s_simSteps++;
}

// Once a frame: folds the count into a rate and, above real time, says so on stderr every five
// seconds. Below real time is normal, since the fixed step does not run during the intro,
// cutscenes, the pause menu or the front end.
static void sim_rate_tick()
{
    const unsigned long long now = port_monotonic_ns();
    if (s_simWindowStart == 0)
    {
        s_simWindowStart = now;
        s_simSteps = 0;
        return;
    }
    const unsigned long long elapsed = now - s_simWindowStart;
    if (elapsed < 1000000000ull)
        return;
    s_simStepsPerSec = (double)s_simSteps * 1e9 / (double)elapsed;
    s_simRateKnown = (s_simSteps > 0);
    s_simSteps = 0;
    s_simWindowStart = now;

    static unsigned long long s_lastWarn;
    if (s_simRateKnown && s_simStepsPerSec > 55.0 && now - s_lastWarn > 5000000000ull)
    {
        s_lastWarn = now;
        std::fprintf(stderr, "[sim] %.1f steps/s = %.2fx real time%s\n",
                     s_simStepsPerSec, s_simStepsPerSec / 50.0,
                     g_bRunSimAndRenderInLockStep ? " (sim and render in lockstep is on)" : "");
    }
}

const PortDebugMatch* PortDebugGetMatch(void) { return &s_match; }

void PortDebugSetSession(const PortDebugSession* session)
{
    if (session != nullptr)
        s_session = *session;
}

const PortDebugSession* PortDebugGetSession(void) { return &s_session; }

void PortDebugSetPad(int port, int err, unsigned int buttons, int stickX, int stickY,
                     int substickX, int substickY, int triggerL, int triggerR)
{
    if (port < 0 || port >= 4)
        return;
    PadSnapshot& p = s_pad[port];
    p.err = err;
    p.buttons = buttons;
    p.stickX = stickX;
    p.stickY = stickY;
    p.substickX = substickX;
    p.substickY = substickY;
    p.triggerL = triggerL;
    p.triggerR = triggerR;
}

int PortOverlayMenuOpen(void) { return s_menuOpen ? 1 : 0; }

void PortOverlayToggleMenu(void) { set_menu_open(!s_menuOpen); }

int PortQuitRequested(void) { return s_quit ? 1 : 0; }

void PortRequestQuit(void) { s_quit = true; }

int PortDebugForcedStadium(void)
{
    if (s_forcedStadium == -2)
    {
        // Seed from the environment once. The spelling is the one rw_gameinfo_forcestadium
        // documents: a world-file base name, or an index.
        s_forcedStadium = -1;
        const char* want = std::getenv("STRIKERS_FORCE_STADIUM");
        if (want != nullptr && *want != '\0')
        {
            for (int i = 0; i < 7; i++)
                if (std::strcmp(want, kStadiums[i]) == 0)
                    s_forcedStadium = i;
            if (s_forcedStadium < 0 && want[0] >= '0' && want[0] <= '6' && want[1] == '\0')
                s_forcedStadium = want[0] - '0';
        }
    }
    return s_forcedStadium;
}

void PortDebugSetForcedStadium(int index)
{
    s_forcedStadium = (index >= 0 && index < 7) ? index : -1;
}

// The present mode as last asked for; Aurora has no getter, so the System tab's checkbox and
// PDBG_SET_VSYNC own this between them.
static bool s_vsyncOn;
static bool s_vsyncRead;
static bool vsync_state()
{
    if (!s_vsyncRead)
    {
        s_vsyncRead = true;
        const char* e = std::getenv("STRIKERS_VSYNC");
        s_vsyncOn = !(e != nullptr && std::strcmp(e, "0") == 0);
    }
    return s_vsyncOn;
}

static void set_vsync(bool on)
{
    vsync_state();
    s_vsyncOn = on;
    aurora_enable_vsync(on);
    double displayHz = 0.0;
    PortFrameLimitInfo(nullptr, &displayHz, nullptr, nullptr);
    PortSetDisplayRefresh(displayHz, on ? 1 : 0);
}

// The game's lockstep switch, with what it means said out loud: one 20 ms step per rendered frame
// is fps/50 times real time.
static void set_lockstep(bool on)
{
    g_bRunSimAndRenderInLockStep = on;
    double limitHz = 0.0;
    PortFrameLimitInfo(&limitHz, nullptr, nullptr, nullptr);
    if (on)
        std::fprintf(stderr, "[sim] lockstep on: one 20 ms step per rendered frame, "
                             "%.0f fps is %.1fx real time\n",
                     limitHz, limitHz > 0.0 ? limitHz / 50.0 : 0.0);
    else
        std::fprintf(stderr, "[sim] lockstep off: 50 steps/s, real time\n");
}

// The ops the platform owns, executed as the queue is drained so the game's dispatcher never sees
// them; non-zero if the command was one of these.
static int platform_command(const PortDebugCommand& c)
{
    switch (c.op)
    {
    case PDBG_SET_FRAME_LIMIT:
        PortSetFrameLimit((double)c.f[0]);
        // Force the derivation now, so the [limiter] line lands on the frame the command did.
        PortFrameLimitInfo(nullptr, nullptr, nullptr, nullptr);
        return 1;
    case PDBG_SET_VSYNC:
        set_vsync(c.a != 0);
        PortFrameLimitInfo(nullptr, nullptr, nullptr, nullptr);
        return 1;
    case PDBG_SET_LOCKSTEP:
        set_lockstep(c.a != 0);
        return 1;
    default:
        return 0;
    }
}

int PortDebugPushCommand(const PortDebugCommand* c)
{
    if (c == nullptr || s_commandCount >= kCommandCap)
        return 0;
    s_commands[(s_commandHead + s_commandCount) % kCommandCap] = *c;
    s_commandCount++;
    return 1;
}

int PortDebugPopCommand(PortDebugCommand* out)
{
    while (out != nullptr && s_commandCount != 0)
    {
        *out = s_commands[s_commandHead];
        s_commandHead = (s_commandHead + 1) % kCommandCap;
        s_commandCount--;
        if (!platform_command(*out))
            return 1;
    }
    return 0;
}

// The hotkeys. All function keys, deliberately: Aurora reads the raw keyboard state for the pad
// bindings regardless of what the event pump does with an event, so a letter here would also press
// a button.
void PortOverlayHandleKey(int scancode, int down)
{
    if (!down)
        return;
    switch (scancode)
    {
    case SDL_SCANCODE_F1:
        PortOverlayToggleMenu();
        break;
    case SDL_SCANCODE_F2:
        PortOverlayInit();
        s_enabled = !s_enabled;
        break;
    case SDL_SCANCODE_F3:
    {
        // Freeze and unfreeze the simulation: the game's own slow-motion at zero.
        float* scale = PortSimTimeScalePtr();
        *scale = (*scale == 0.0f) ? 1.0f : 0.0f;
        break;
    }
    case SDL_SCANCODE_F4:
    {
        float* scale = PortSimTimeScalePtr();
        *scale = (*scale == 0.25f) ? 1.0f : 0.25f;
        break;
    }
    case SDL_SCANCODE_F11:
        set_fullscreen(!is_fullscreen());
        break;
    default:
        break;
    }
}

namespace
{

void draw_frame_tab(const PortBenchLive& live)
{
    ImGui::Text("%.1f fps   frame %.2f ms", live.fps, live.frameMs);
    ImGui::Separator();

    // The three phases, in the same terms the benchmark reports them, so a number read off the
    // screen means the same thing as a number read out of the summary.
    ImGui::Text("busy    %6.3f ms   p95 %6.3f", live.busyMs, live.busyP95Ms);
    ImGui::Text("present %6.3f ms", live.presentMs);
    ImGui::Text("sleep   %6.3f ms  (idle)", live.sleepMs);

    double limitHz = 0.0;
    PortFrameLimitInfo(&limitHz, nullptr, nullptr, nullptr);
    const double field = limitHz > 0.0 ? 1000.0 / limitHz : 1000.0 / 59.94;
    if (live.busyP95Ms > 0.0)
        ImGui::Text("headroom %.1fx at p95 against %.2f ms", field / live.busyP95Ms, field);

    // The last 256 frames. A p95 says a frame was slow; this says whether it was one frame or a
    // pattern.
    static float busy[256], frame[256];
    const size_t n = PortBenchGetHistory(busy, frame, 256);
    if (n > 1)
    {
        float top = 0.0f;
        for (size_t i = 0; i < n; i++)
            if (frame[i] > top)
                top = frame[i];
        top = top < 20.0f ? 20.0f : top;
        ImGui::PlotLines("##frame", frame, (int)n, 0, "frame ms", 0.0f, top, ImVec2(-1.0f, 60.0f));
        ImGui::PlotLines("##busy", busy, (int)n, 0, "busy ms", 0.0f, top, ImVec2(-1.0f, 60.0f));
    }

    ImGui::Separator();
    ImGui::Text("worst   %6.3f ms at frame %lu", live.worstMs, live.worstFrame);
    ImGui::Text("scene   %s", s_scene);
    ImGui::Text("stadium %s", s_stadium);
    if (live.matchActive)
        ImGui::Text("match   %5.1fs   %d - %d", (double)s_clock, s_scoreHome, s_scoreAway);

    if (ImGui::CollapsingHeader("draw calls"))
    {
        const AuroraStats* st = aurora_get_stats();
        if (st != nullptr)
        {
            ImGui::Text("draws %u   merged %u", st->drawCallCount, st->mergedDrawCallCount);
            ImGui::Text("pipelines %u created, %u queued", st->createdPipelines,
                        st->queuedPipelines);
            ImGui::Text("this frame: vert %u KB  uniform %u KB  index %u KB  tex %u KB",
                        st->lastVertSize / 1024, st->lastUniformSize / 1024,
                        st->lastIndexSize / 1024, st->lastTextureUploadSize / 1024);
        }
    }

    // GPU memory, the largest single thing in the process. Aurora's pools are fixed-size
    // compile-time constants, and the gap between peak and reserved is what a reader acts on.
    if (ImGui::CollapsingHeader("gpu pools"))
    {
        uint32_t peak[kPoolCount] = {};
        uint32_t reserved[kPoolCount] = {};
        aurora_gfx_pool_stats(peak, reserved, kPoolCount);
        uint64_t peakTotal = 0, reservedTotal = 0;
        for (size_t i = 0; i < kPoolCount; i++)
        {
            peakTotal += peak[i];
            reservedTotal += reserved[i];
            ImGui::Text("%-10s %6.2f / %6.2f MiB", kPoolNames[i], peak[i] / 1048576.0,
                        reserved[i] / 1048576.0);
        }
        ImGui::Text("%-10s %6.2f / %6.2f MiB", "peak/set", peakTotal / 1048576.0,
                    reservedTotal / 1048576.0);
    }

    if (ImGui::CollapsingHeader("textures"))
    {
        uint64_t src = 0, uploaded = 0, count = 0;
        aurora_gfx_texture_stats(&src, &uploaded, &count);
        ImGui::Text("%llu textures", (unsigned long long)count);
        ImGui::Text("%.2f MB on disc -> %.2f MB uploaded", src / 1048576.0, uploaded / 1048576.0);
        if (src > 0)
            ImGui::Text("%.2fx expansion", (double)uploaded / (double)src);
    }

    if (ImGui::Button("print benchmark summary to stderr"))
        PortBenchReport();
}

void draw_match_tab()
{
    const PortDebugMatch& m = s_match;

    ImGui::Text("scene   %s", s_scene);
    ImGui::Text("stadium %s", s_stadium);

    if (!m.valid)
    {
        ImGui::TextDisabled("no match running");
        return;
    }

    ImGui::Text("clock   %5.1f / %.0f s   %s%s", (double)m.clock, (double)m.duration,
                name_of(kGameStates, m.gameState), m.inSuddenDeath ? "   SUDDEN DEATH" : "");
    ImGui::Text("mode    %s%s", name_of(kGameModes, m.gameMode), m.demoMode ? "  (demo)" : "");

    ImGui::SeparatorText("score");
    for (int t = 0; t < 2; t++)
    {
        ImGui::PushID(t);
        int score = m.teams[t].score;
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::InputInt(t == 0 ? "home" : "away", &score))
            cmd(PDBG_SET_SCORE, t, score < 0 ? 0 : score);
        ImGui::PopID();
    }

    ImGui::SeparatorText("clock");
    if (ImGui::SmallButton("+30 s"))
        cmd(PDBG_ADD_CLOCK, 0, 0, 0, 30.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("-30 s"))
        cmd(PDBG_ADD_CLOCK, 0, 0, 0, -30.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("10 s left"))
        cmd(PDBG_ADD_CLOCK, 0, 0, 0, 10.0f - (m.duration - m.clock));
    ImGui::SameLine();
    if (ImGui::SmallButton("end match"))
        cmd(PDBG_END_MATCH);
    help("Runs the clock out. A tied score goes to sudden death, which is the game's own path.");
    if (ImGui::SmallButton("sudden death now"))
        cmd(PDBG_SUDDEN_DEATH);
    ImGui::SameLine();
    if (ImGui::SmallButton("kickoff: home"))
        cmd(PDBG_KICKOFF, 0);
    ImGui::SameLine();
    if (ImGui::SmallButton("kickoff: away"))
        cmd(PDBG_KICKOFF, 1);

    ImGui::SeparatorText("simulation");
    {
        // The game's own slow-motion: it drives this itself, Presentation resets it to 1 on a
        // possession change, and goals slow it down, so the slider shows what is in force rather
        // than owning it.
        float* scale = PortSimTimeScalePtr();
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderFloat("time scale", scale, 0.0f, 2.0f, "%.2fx");
        ImGui::SameLine();
        if (ImGui::SmallButton("freeze"))
            *scale = 0.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("1/4"))
            *scale = 0.25f;
        ImGui::SameLine();
        if (ImGui::SmallButton("1x"))
            *scale = 1.0f;
        ImGui::TextDisabled("F3 freezes, F4 quarter speed.");
        if (s_simRateKnown)
        {
            const double x = s_simStepsPerSec / 50.0;
            if (x > 1.1)
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                                   "simulation %.1f steps/s = %.2fx real time%s",
                                   s_simStepsPerSec, x,
                                   g_bRunSimAndRenderInLockStep ? "  (lockstep is on: Render tab)" : "");
            else
                ImGui::Text("simulation %.1f steps/s = %.2fx real time", s_simStepsPerSec, x);
            help("Fixed steps per wall second, counted from the game's own match update. "
                 "50 is real time in play at any frame rate; lower is normal, because the "
                 "fixed step does not run during the intro, goal cutscenes or the pause "
                 "menu. Higher is the time scale above or the lockstep switch on the "
                 "Render tab.");
        }
    }

    ImGui::SeparatorText("ball");
    const double speed =
        std::sqrt((double)m.ballVel[0] * m.ballVel[0] + (double)m.ballVel[1] * m.ballVel[1] +
                  (double)m.ballVel[2] * m.ballVel[2]);
    ImGui::Text("pos   %6.2f %6.2f %6.2f   speed %6.2f", (double)m.ballPos[0],
                (double)m.ballPos[1], (double)m.ballPos[2], speed);
    if (m.ballOwner < 0)
        ImGui::TextDisabled("loose");
    else
        ImGui::Text("held by #%d %s", m.ballOwner, m.characters[m.ballOwner].name);

    // These two are why this tab exists: they are the state behind the keeper standing next to a
    // ball it will not pick up.
    ImGui::Text("pass target %s      pickup lock %.2fs", m.ballHasPassTarget ? "SET" : "none",
                (double)m.ballNoPickup);
    if (ImGui::SmallButton("warp to centre"))
        cmd(PDBG_WARP_BALL, 0, 0, 0, 0.0f, 0.0f, 2.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("into home net"))
        cmd(PDBG_WARP_BALL, 0, 0, 0, -1.0f, 0.0f, 1.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("into away net"))
        cmd(PDBG_WARP_BALL, 0, 0, 0, 1.0f, 0.0f, 1.0f, 1.0f);
    help("The net warps put the ball just inside the goal line at a walking pace, so the "
         "game's own goal detection, scorer credit and celebration run. f[3] marks the "
         "request as net-relative; the drain resolves it against the net's real position.");

    ImGui::SeparatorText("presentation");
    ImGui::Text("%s   %.1fs%s", m.presentation, (double)m.presentationTime,
                m.nisActive ? "   NIS playing" : "");
    if (ImGui::SmallButton("skip"))
        cmd(PDBG_SKIP_PRESENTATION);
    help("Sets the same flag the pad's skip button does, so the script's own bypass "
         "windows decide when it takes. Not every phase is skippable.");
    ImGui::SameLine();
    ImGui::TextDisabled(m.skipAllowed ? "(skippable now)" : "(not skippable now)");

    ImGui::SeparatorText("bowser");
    ImGui::Text("%s   timer %.1fs", m.bowserAlive ? "on the pitch" : "away",
                (double)m.bowserTimer);
    if (ImGui::SmallButton("attack now"))
        cmd(PDBG_BOWSER_ATTACK);
    ImGui::SameLine();
    if (ImGui::SmallButton("send away"))
        cmd(PDBG_BOWSER_HIDE);

    ImGui::SeparatorText("effects");
    ImGui::Text("%d emission controllers live", m.effects);
    ImGui::SameLine();
    if (ImGui::SmallButton("kill all"))
        cmd(PDBG_KILL_EFFECTS, 0);
}

void draw_players_tab()
{
    const PortDebugMatch& m = s_match;
    if (!m.valid)
    {
        ImGui::TextDisabled("no match running");
        return;
    }

    static int s_selected = 0;

    if (ImGui::BeginTable("chars", 8,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 240.0f)))
    {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("who");
        ImGui::TableSetupColumn("role");
        ImGui::TableSetupColumn("action");
        ImGui::TableSetupColumn("wants");
        ImGui::TableSetupColumn("item");
        ImGui::TableSetupColumn("pos");
        ImGui::TableSetupColumn("flags");
        ImGui::TableHeadersRow();
        for (int i = 0; i < m.characterCount; i++)
        {
            const PortDebugCharacter& c = m.characters[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char label[16];
            std::snprintf(label, sizeof label, "%d##row", i);
            if (ImGui::Selectable(label, s_selected == i,
                                  ImGuiSelectableFlags_SpanAllColumns))
                s_selected = i;
            ImGui::TableNextColumn();
            ImGui::Text("%s%s %s", c.team == 0 ? "H " : "A ", c.name,
                        c.isGoalie ? "GK" : c.isCaptain ? "C" : "");
            ImGui::TableNextColumn();
            ImGui::Text("%s", c.isGoalie ? "goalie" : name_of(kRoles, c.role));
            ImGui::TableNextColumn();
            ImGui::Text("%s", c.isGoalie ? name_of(kGoalieStates, c.state)
                                         : name_of(kFielderStates, c.state));
            ImGui::TableNextColumn();
            if (c.isGoalie)
                ImGui::Text("urgency %s", name_of(kUrgency, c.urgency));
            else
                ImGui::Text("%s", name_of(kDesires, c.desire));
            ImGui::TableNextColumn();
            if (!c.isGoalie && c.powerup >= 0)
                ImGui::Text("%s x%d", name_of(kPowerups, c.powerup), c.powerupCount);
            ImGui::TableNextColumn();
            ImGui::Text("%6.1f %6.1f", (double)c.pos[0], (double)c.pos[1]);
            ImGui::TableNextColumn();
            ImGui::Text("%s%s%s%s%s%s", c.hasBall ? "BALL " : "", c.isHuman ? "human " : "",
                        c.frozenSeconds > 0.0f ? "frozen " : "", c.invincible ? "star " : "",
                        c.fallen ? "down " : "", c.card >= 0 ? name_of(kCards, c.card) : "");
        }
        ImGui::EndTable();
    }

    if (s_selected < 0 || s_selected >= m.characterCount)
        s_selected = 0;
    const PortDebugCharacter& c = m.characters[s_selected];

    ImGui::SeparatorText("selected");
    ImGui::Text("#%d %s   anim %s   speed %.2f", s_selected, c.name, c.anim, (double)c.speed);
    if (c.isGoalie)
        ImGui::Text("energy %.2f", (double)c.energy);
    else
        ImGui::Text("shot meter %s %.2f", name_of(kShotMeter, c.shotMeterState),
                    (double)c.shotMeterValue);

    if (ImGui::SmallButton("give ball"))
        cmd(PDBG_GIVE_BALL, s_selected);
    help("Releases the current owner and calls PickupBall, exactly as the game does at kickoff. "
         "The pickup lock on the ball and on the player can still refuse it.");
    if (!c.isGoalie)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("freeze 3 s"))
            cmd(PDBG_FREEZE_PLAYER, s_selected, 0, 0, 3.0f);
        ImGui::SameLine();
        if (ImGui::SmallButton("unfreeze"))
            cmd(PDBG_FREEZE_PLAYER, s_selected, 0, 0, 0.0f);
        ImGui::SameLine();
        if (ImGui::SmallButton("throw held item"))
            cmd(PDBG_THROW_POWERUP, s_selected, c.powerup, c.powerupCount);
    }

    ImGui::SeparatorText("teams");
    for (int t = 0; t < 2; t++)
    {
        const PortDebugTeam& tm = m.teams[t];
        ImGui::Text("%s  %s / %s   %s, %s   AI %s", t == 0 ? "home" : "away",
                    name_of(kTeams, tm.teamId), name_of(kSidekicks, tm.sidekickId),
                    name_of(kSituations, tm.situation), name_of(kStyles, tm.style),
                    name_of(kDifficulties, tm.difficulty));
    }
}

void draw_items_tab()
{
    const PortDebugMatch& m = s_match;
    if (!m.valid)
    {
        ImGui::TextDisabled("no match running");
        return;
    }

    static int s_type = 0;
    static int s_count = 1;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::Combo("item", &s_type, kPowerups, (int)(sizeof kPowerups / sizeof kPowerups[0]));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("count", &s_count);
    if (s_count < 1)
        s_count = 1;
    if (s_count > 3)
        s_count = 3;

    ImGui::Text("%d item objects live on the pitch", m.powerupObjects);
    ImGui::Text("%s", m.isPure ? "pure game: the game will not award items" : "");

    for (int t = 0; t < 2; t++)
    {
        const PortDebugTeam& tm = m.teams[t];
        ImGui::PushID(t);
        ImGui::SeparatorText(t == 0 ? "home" : "away");
        ImGui::ProgressBar(tm.powerupMeter, ImVec2(160.0f, 0.0f), "meter");
        for (int s = 0; s < 2; s++)
        {
            if (tm.powerup[s] >= 0)
                ImGui::Text("slot %d: %s x%d", s, name_of(kPowerups, tm.powerup[s]),
                            tm.powerupCount[s]);
            else
                ImGui::TextDisabled("slot %d: empty", s);
        }
        if (ImGui::SmallButton("give selected"))
            cmd(PDBG_GIVE_POWERUP, t, s_type, s_count);
        ImGui::SameLine();
        if (ImGui::SmallButton("random award"))
            cmd(PDBG_AWARD_POWERUP, t);
        help("The game's own award, the one the meter triggers at full. Refuses when items "
             "are off or the game is pure.");
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::SmallButton("destroy items on pitch"))
        cmd(PDBG_CLEAR_POWERUPS, 0);
    ImGui::SameLine();
    if (ImGui::SmallButton("clear everything"))
        cmd(PDBG_CLEAR_POWERUPS, 1);
    help("Also empties both teams' inventories.");
}

void draw_ai_tab()
{
    const PortDebugMatch& m = s_match;

    ImGui::SeparatorText("difficulty");
    ImGui::TextDisabled("The AI's chance tables, reloaded from the difficulty files.");
    ImGui::TextDisabled("A human side is normally `human`; the game re-derives it on");
    ImGui::TextDisabled("the next choose-sides screen.");
    static int s_diff[2] = { 2, 2 };
    if (m.valid)
    {
        static bool s_seeded = false;
        if (!s_seeded)
        {
            s_seeded = true;
            for (int t = 0; t < 2; t++)
                s_diff[t] = (m.teams[t].difficulty >= 0 && m.teams[t].difficulty < 7)
                                ? m.teams[t].difficulty : 2;
        }
    }
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("home", &s_diff[0], kDifficulties, 7);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("away", &s_diff[1], kDifficulties, 7);
    ImGui::BeginDisabled(!m.valid);
    if (ImGui::SmallButton("apply"))
        cmd(PDBG_SET_DIFFICULTY, s_diff[0], s_diff[1]);
    ImGui::EndDisabled();
    if (m.valid)
        ImGui::Text("in force: %s / %s", name_of(kDifficulties, m.teams[0].difficulty),
                    name_of(kDifficulties, m.teams[1].difficulty));

    ImGui::SeparatorText("who plays which side");
    ImGui::TextDisabled("Pad -> side. The teams rebind their controllers every update,");
    ImGui::TextDisabled("so this takes effect on the next frame.");
    for (int pad = 0; pad < 4; pad++)
    {
        ImGui::PushID(pad);
        int side = m.valid ? m.padSide[pad] + 1 : 0;   // 0 none, 1 home, 2 away
        const char* const kSides[] = { "none (AI)", "home", "away" };
        ImGui::SetNextItemWidth(120.0f);
        char label[16];
        std::snprintf(label, sizeof label, "pad %d", pad);
        if (ImGui::Combo(label, &side, kSides, 3))
            cmd(PDBG_SET_SIDE, pad, side - 1);
        ImGui::PopID();
    }

    ImGui::SeparatorText("switches");
    ImGui::Checkbox("pad monkey (random input on every pad)", &g_bEnableGamecubePadMonkey);
    bool noPickups = sbNoBallPickups != 0;
    if (ImGui::Checkbox("nobody can pick up the ball", &noPickups))
        sbNoBallPickups = noPickups ? 1 : 0;
    ImGui::Checkbox("disable collision detection", &sbDisableCollisionDetection);
    help("Skips the physics collision pass for everything. The retail 'noclip'.");

    if (m.valid)
    {
        ImGui::SeparatorText("teams");
        for (int t = 0; t < 2; t++)
            ImGui::Text("%s: %s, %s", t == 0 ? "home" : "away",
                        name_of(kSituations, m.teams[t].situation),
                        name_of(kStyles, m.teams[t].style));
    }
}

void draw_cheats_tab()
{
    const PortDebugMatch& m = s_match;

    ImGui::SeparatorText("match settings");
    ImGui::TextDisabled("The options screen's values for the running match. Read where");
    ImGui::TextDisabled("they are used (an item throw, a shot, Bowser's timer), so a");
    ImGui::TextDisabled("change takes effect at the next one.");
    ImGui::BeginDisabled(!m.valid);
    check_cmd("items", m.optPowerUps, PDBG_SET_OPTION, PDBG_OPT_POWERUPS);
    check_cmd("super strikes", m.optShoot2Score, PDBG_SET_OPTION, PDBG_OPT_SHOOT2SCORE);
    check_cmd("bowser attacks", m.optBowser, PDBG_SET_OPTION, PDBG_OPT_BOWSER);
    check_cmd("rumble", m.optRumble, PDBG_SET_OPTION, PDBG_OPT_RUMBLE);
    check_cmd("pure game (no items at all)", m.isPure, PDBG_SET_OPTION, PDBG_OPT_PURE);
    {
        int skill = m.skillLevel;
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("skill level", &skill, kSkills, 5))
            cmd(PDBG_SET_OPTION, PDBG_OPT_SKILL, skill);
        help("The user-facing setting. Apply a difficulty on the AI tab for it to reach the "
             "running match.");
    }

    ImGui::SeparatorText("cheats");
    ImGui::TextDisabled("The retail cheat options, as the save file holds them. The game");
    ImGui::TextDisabled("honours them in friendly and tournament modes only.");
    check_cmd("infinite items", m.cheatInfinite, PDBG_SET_OPTION, PDBG_OPT_INFINITE);
    check_cmd("glass-jaw goalies", m.cheatStunned, PDBG_SET_OPTION, PDBG_OPT_STUNNED);
    check_cmd("tilting field", m.cheatTilt, PDBG_SET_OPTION, PDBG_OPT_TILT);
    check_cmd("every super strike perfect", m.cheatPerfect, PDBG_SET_OPTION, PDBG_OPT_PERFECT);
    {
        int custom = m.cheatCustom;
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("custom items", &custom, kCustomPowerups, 6))
            cmd(PDBG_SET_OPTION, PDBG_OPT_CUSTOM, custom);
    }
    {
        bool all = (m.trophies[0] == 0xFF && m.trophies[1] == 0xFF);
        if (ImGui::Checkbox("all trophies earned (unlocks the cheats menu)", &all))
            cmd(PDBG_SET_OPTION, PDBG_OPT_TROPHIES, all ? 1 : 0);
    }
    ImGui::EndDisabled();
    if (!m.valid)
        ImGui::TextDisabled("(settings need a running match to show and set)");

    ImGui::SeparatorText("unlocks");
    {
        bool unlock = PortGetUnlockAll() != 0;
        if (ImGui::Checkbox("every stadium, team and cup available", &unlock))
            PortSetUnlockAll(unlock ? 1 : 0);
        help("Answers the game's own `givealltrophies` config test, which every unlock "
             "check consults as it runs. STRIKERS_UNLOCK_ALL sets it at boot.");
    }

    ImGui::SeparatorText("next match");
    ImGui::TextDisabled("Read when a match is set up, so these apply to the next one.");
    {
        int forced = PortDebugForcedStadium() + 1;
        const char* names[8];
        names[0] = "(the game's own pick)";
        for (int i = 0; i < 7; i++)
            names[i + 1] = kStadiums[i];
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::Combo("stadium", &forced, names, 8))
            PortDebugSetForcedStadium(forced - 1);
        help("Overrides PickStadium: attract mode, cup rounds and tournament rounds all "
             "go through it. The front end's own stadium screen does not.");
    }
    {
        static int s_team[2] = { 3, 2 };
        static int s_side[2] = { 0, 1 };
        const char* keys[4] = { "team1", "team2", "sidekick1", "sidekick2" };
        for (int t = 0; t < 2; t++)
        {
            ImGui::PushID(t);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::Combo(keys[t], &s_team[t], kTeams, 9))
            {
                char kv[64];
                std::snprintf(kv, sizeof kv, "%s=%s", keys[t], kTeams[s_team[t]]);
                cmd(PDBG_SET_CONFIG_STRING, 0, 0, 0, 0, 0, 0, 0, kv);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::Combo(keys[t + 2], &s_side[t], kSidekicks, 4))
            {
                char kv[64];
                std::snprintf(kv, sizeof kv, "%s=%s", keys[t + 2], kSidekicks[s_side[t]]);
                cmd(PDBG_SET_CONFIG_STRING, 0, 0, 0, 0, 0, 0, 0, kv);
            }
            ImGui::PopID();
        }
        help("The retail `team1`/`team2`/`sidekick1`/`sidekick2` config keys, which the "
             "game reads for any match it sets up itself. STRIKERS_TEAM1 and friends set "
             "the same keys at boot.");
    }
}

void draw_camera_tab()
{
    const PortDebugMatch& m = s_match;

    ImGui::SeparatorText("current");
    if (m.valid)
    {
        ImGui::Text("%s   fov %.1f", name_of(kCameraTypes, m.cameraType), (double)m.camFov);
        ImGui::Text("eye    %7.2f %7.2f %7.2f", (double)m.camPos[0], (double)m.camPos[1],
                    (double)m.camPos[2]);
        ImGui::Text("target %7.2f %7.2f %7.2f", (double)m.camTarget[0], (double)m.camTarget[1],
                    (double)m.camTarget[2]);

        ImGui::Text("game camera at the bottom of the stack: %s",
                    name_of(kCameraTypes, m.cameraTypeWanted));
        ImGui::TextDisabled("Not switchable from here: the retail camera switch corrupts");
        ImGui::TextDisabled("memory on this port.");
    }
    else
    {
        ImGui::TextDisabled("no match running");
    }

    ImGui::SeparatorText("framing");
    ImGui::Text("display aspect  %.4f     logical frame %u x 448", (double)PortTargetAspect(),
                PortLogicalFrameWidth());

    // The console drew its 3D through a 1.25 view volume on a 4/3 screen, 6.7% wider than geometry.
    float stretch = PortGetFrustumStretch();
    if (ImGui::SliderFloat("console stretch", &stretch, 0.0f, 1.0f, "%.2f"))
        PortSetFrustumStretch(stretch);
    ImGui::SameLine();
    ImGui::TextDisabled("(1 = as it shipped)");

    float blend = PortGetCameraBlendOverride();
    bool derived = blend < 0.0f;
    if (ImGui::Checkbox("camera blend from aspect", &derived))
        PortSetCameraBlendOverride(derived ? -1.0f : PortCameraAspectBlend());
    if (!derived)
    {
        blend = PortCameraAspectBlend();
        if (ImGui::SliderFloat("blend", &blend, 0.0f, 3.0f, "%.2f"))
            PortSetCameraBlendOverride(blend);
        ImGui::TextDisabled("0 = the 4:3 camera table, 1 = the widescreen one");
    }

    static bool bars = PortDrawCinematicBars() != 0;
    if (ImGui::Checkbox("cinematic letterbox bars", &bars))
        PortSetDrawCinematicBars(bars);
}

void draw_render_tab()
{
    ImGui::TextDisabled("The retail tweak menu's switches. Each one is a global the");
    ImGui::TextDisabled("shipped code still reads; the menu that set them was stripped.");

    ImGui::SeparatorText("what is drawn");
    ImGui::Checkbox("world", &g_bRenderWorld);
    ImGui::SameLine();
    check_u8("HUD", &g_hudVisible);
    ImGui::SameLine();
    ImGui::Checkbox("static models", &g_bEnableDrawableModel);
    ImGui::SameLine();
    ImGui::Checkbox("skinned models", &g_bEnableDrawableSkinModel);
    // Class statics the platform cannot name: World::sbStadiumRenderingDisabled and its neighbours.
    static bool s_stadiumOff, s_skyboxOff, s_charShadowsOff;
    if (ImGui::Checkbox("hide stadium", &s_stadiumOff))
        cmd(PDBG_SET_CLASS_FLAG, PDBG_CF_STADIUM_OFF, s_stadiumOff ? 1 : 0);
    help("Everything but the ball, the hammer and the skybox.");
    ImGui::SameLine();
    if (ImGui::Checkbox("hide skybox", &s_skyboxOff))
        cmd(PDBG_SET_CLASS_FLAG, PDBG_CF_SKYBOX_OFF, s_skyboxOff ? 1 : 0);

    ImGui::SeparatorText("lighting and texture stages");
    ImGui::Checkbox("lighting", &g_bAllowLighting);
    ImGui::SameLine();
    ImGui::Checkbox("specular", &g_bAllowSpecular);
    ImGui::SameLine();
    check_u8("white diffuse", &g_bWhiteDiffuse);
    help("Replaces every diffuse texture with white: the lighting alone.");
    check_u8("detail", &g_TexDetail);
    ImGui::SameLine();
    check_u8("shadow", &g_TexShadow);
    ImGui::SameLine();
    check_u8("self-illum", &g_TexSelfIllum);
    ImGui::SameLine();
    check_u8("gloss", &g_TexGloss);
    check_u8("texel density view", &g_bTexelDensity);
    ImGui::SameLine();
    ImGui::Checkbox("ball glow", &g_bBallGlow);

    ImGui::SeparatorText("shadows");
    check_u8("blob shadows", &g_bShadowBlobs);
    ImGui::SameLine();
    ImGui::Checkbox("planar", &g_bDrawPlanarShadows);
    ImGui::SameLine();
    ImGui::Checkbox("volumes", &g_bShadowVolumes);
    ImGui::SameLine();
    if (ImGui::Checkbox("no character shadows", &s_charShadowsOff))
        cmd(PDBG_SET_CLASS_FLAG, PDBG_CF_CHAR_SHADOWS_OFF, s_charShadowsOff ? 1 : 0);
    check_u8("shadow preview", &g_bPreview);
    help("Draws the shadow map's own model.");
    ImGui::SameLine();
    check_u8("shadow bounds", &g_bShadowBounds);
    help("Shadow quad outlines in blue, the light in yellow.");
    ImGui::SameLine();
    ImGui::Checkbox("co-planar per object", &g_bCoPlanarPerObject);

    ImGui::SeparatorText("culling");
    check_u8("clip to frustum", &g_bClipToFrustum);
    ImGui::SameLine();
    check_u8("freeze frustum", &g_bFreezeFrustum);
    help("Stops the culling planes following the camera, so moving the camera shows "
         "what the culler thinks is visible.");
    ImGui::SameLine();
    check_u8("culling stats", &g_bDrawCullingInfo);

    ImGui::SeparatorText("debug drawing");
    check_u8("bounding boxes", &g_bDrawBoundingBoxes);
    ImGui::SameLine();
    check_u8("bounding spheres", &g_bDrawBoundingSphere);
    ImGui::SameLine();
    check_u8("net UV checker", &sbUseCheckerTexture);
    {
        const char* const kBones[] = { "off", "endpoint bounds", "all points" };
        ImGui::SetNextItemWidth(140.0f);
        if (g_nShowBones > 2)
            g_nShowBones = 2;
        ImGui::Combo("character bones", &g_nShowBones, kBones, 3);
    }
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderInt("world grid spacing", &g_nGridDisplaySpacing, 0, 20);
    ImGui::SameLine();
    ImGui::TextDisabled("(0 = off)");

    ImGui::SeparatorText("on-screen text (the game's own)");
    ImGui::Checkbox("frame stats", &g_bFrameStatsOnScreen);
    ImGui::SameLine();
    ImGui::Checkbox("frame smiler", &g_bFrameSmiler);
    ImGui::SameLine();
    check_u8("heap banner", &g_bMemoryOnScreen);
    ImGui::SameLine();
    ImGui::Checkbox("safe frame", &g_bDrawSafeFrame);
    {
        bool lock = g_bRunSimAndRenderInLockStep;
        if (ImGui::Checkbox("sim and render in lockstep", &lock))
            set_lockstep(lock);
        double limitHz = 0.0;
        PortFrameLimitInfo(&limitHz, nullptr, nullptr, nullptr);
        ImGui::SameLine();
        if (lock)
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                               "one 20 ms step per frame: NOT real time (%.1fx at %.0f fps)",
                               limitHz > 0.0 ? limitHz / 50.0 : 0.0, limitHz);
        help("The retail tweak menu's switch. The simulation normally runs 50 fixed steps a "
             "second whatever the frame rate; with this on it runs one step per rendered "
             "frame, so the game plays at fps/50 times real time: 2.4x on a 120 Hz "
             "display, and uncapped it is a blur. If the game is running fast, this is "
             "the first thing to check; the Match tab shows the measured rate.");
    }
}

void draw_audio_tab()
{
    unsigned long buffers = 0, underruns = 0;
    int everNonSilent = 0;
    PortAudioStats(&buffers, &underruns, &everNonSilent);

    ImGui::SeparatorText("transport");
    if (!PortAudioDeviceOpen())
    {
        ImGui::TextDisabled("no audio device: STRIKERS_AUDIO is off, or the device failed to open");
        ImGui::TextDisabled("(audio cannot be switched on after boot; MusyX is not initialised)");
    }
    else
    {
        ImGui::Text("ticks %lu   underruns %lu   %s", buffers, underruns,
                    everNonSilent ? "has produced sound" : "silent so far");
        unsigned long calls = 0, over2 = 0;
        double mean = 0.0, worst = 0.0;
        PortAudioUpdateCost(&calls, &mean, &worst, &over2);
        ImGui::Text("update cost: mean %.3f ms  max %.2f ms  %lu frames over 2 ms", mean, worst,
                    over2);
    }

    ImGui::SeparatorText("mixer");
    PortAudioMixInfo mix;
    if (PortAudioMixStats(&mix))
    {
        // The six numbers that between them name every separate cause of silence.
        ImGui::Text("studios %d   voices %d   with sample %d", mix.studios, mix.voices,
                    mix.withSample);
        ImGui::Text("peak envelope 0x%04x   peak pan 0x%04x   bus peak %d / 32767", mix.peakEnv,
                    mix.peakVol, mix.busPeak);
        ImGui::Text("mix rate %u Hz", mix.mixFrq);
        float level = mix.busPeak / 32767.0f;
        ImGui::ProgressBar(level, ImVec2(200.0f, 0.0f), "bus");

        // The master stage is the port's own, so it is worth being able to see it working. rawPeak
        // over 32767 with a limitGain below 1 is the limiter doing its job; rawPeak under it and a
        // gain of 1 is the limiter out of the way.
        ImGui::Text("studios summed %d / 32767   limiter gain %.3f   master %.2f", mix.rawPeak,
                    (double)mix.limitGain, (double)mix.masterGain);
        help("The limiter and the master level have no console equivalent: the game shipped "
             "with the DSP compressor off and the final mix simply clamped. STRIKERS_AUDIO_VOLUME "
             "sets the master.");
    }
    else
    {
        ImGui::TextDisabled("the mixer has not run");
    }

    ImGui::SeparatorText("dump");
    static char s_dumpPath[256] = "/tmp/strikers-audio.wav";
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("##dumppath", s_dumpPath, sizeof s_dumpPath);
    ImGui::SameLine();
    if (mix.dumping)
    {
        if (ImGui::SmallButton("stop"))
            PortAudioDumpStop();
        ImGui::SameLine();
        ImGui::Text("%lu frames", mix.dumpFrames);
    }
    else if (ImGui::SmallButton("start"))
    {
        PortAudioDumpStart(s_dumpPath);
    }
    help("Exactly what goes to the device, as a WAV. 'Is there sound' and 'is it the right "
         "sound' are different questions; this answers the second.");

    ImGui::SeparatorText("volume groups");
    ImGui::TextDisabled("The game's master volumes. Set, not read back.");
    static float s_vol[3] = { 1.0f, 1.0f, 1.0f };
    const char* const kGroups[] = { "music", "sfx", "voice" };
    for (int g = 0; g < 3; g++)
    {
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat(kGroups[g], &s_vol[g], 0.0f, 1.0f, "%.2f"))
            cmd(PDBG_SET_VOLUME, g + 1, 0, 0, s_vol[g]);   // VG_Music = 1
    }
    if (ImGui::SmallButton("silence everything"))
        cmd(PDBG_SILENCE);

    ImGui::SeparatorText("play a world effect by name");
    static char s_sfx[64] = "Whistle";
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##sfx", s_sfx, sizeof s_sfx);
    ImGui::SameLine();
    if (ImGui::SmallButton("play"))
        cmd(PDBG_PLAY_SFX, 0, 0, 0, 0, 0, 0, 0, s_sfx);
    help("Audio::PlayWorldSFXbyStr: the eWorldSFX enumerator without its prefix. "
         "Only during a match, once the stadium's sound bank is loaded.");
}

void draw_input_tab()
{
    ImGui::SeparatorText("pads");
    const unsigned int count = PADCount();
    ImGui::Text("%u controller%s connected", count, count == 1 ? "" : "s");
    for (unsigned int i = 0; i < count && i < 4; i++)
    {
        const char* name = PADGetName(i);
        ImGui::Text("  port %u: %s", i, name != nullptr ? name : "?");
    }

    ImGui::SeparatorText("pad 0, as the game read it this frame");
    {
        const PadSnapshot& p = s_pad[0];
        if (p.err != PAD_ERR_NONE)
        {
            ImGui::TextDisabled("err %d (no controller: no gamepad, and the keyboard is %s)",
                                p.err, s_menuOpen ? "detached while the menu is open" : "unbound");
        }
        struct
        {
            unsigned int bit;
            const char* name;
        } const kBits[] = {
            { PAD_BUTTON_A, "A" },     { PAD_BUTTON_B, "B" },     { PAD_BUTTON_X, "X" },
            { PAD_BUTTON_Y, "Y" },     { PAD_TRIGGER_Z, "Z" },    { PAD_TRIGGER_L, "L" },
            { PAD_TRIGGER_R, "R" },    { PAD_BUTTON_START, "START" },
            { PAD_BUTTON_UP, "up" },   { PAD_BUTTON_DOWN, "down" },
            { PAD_BUTTON_LEFT, "left" }, { PAD_BUTTON_RIGHT, "right" },
        };
        for (size_t i = 0; i < sizeof kBits / sizeof kBits[0]; i++)
        {
            const bool on = (p.buttons & kBits[i].bit) != 0;
            if (i != 0)
                ImGui::SameLine();
            if (on)
                ImGui::Text("[%s]", kBits[i].name);
            else
                ImGui::TextDisabled(" %s ", kBits[i].name);
        }
        ImGui::Text("stick %4d %4d   c-stick %4d %4d   L %3d  R %3d", p.stickX, p.stickY,
                    p.substickX, p.substickY, p.triggerL, p.triggerR);
    }

    ImGui::SeparatorText("press");
    ImGui::TextDisabled("Held for six frames on pad 0, merged with the real pad. The same");
    ImGui::TextDisabled("thing STRIKERS_AUTOPRESS does.");
    struct
    {
        unsigned int bit;
        const char* name;
    } const kPress[] = {
        { PAD_BUTTON_A, "A" },      { PAD_BUTTON_B, "B" },        { PAD_BUTTON_X, "X" },
        { PAD_BUTTON_Y, "Y" },      { PAD_TRIGGER_Z, "Z" },       { PAD_BUTTON_START, "Start" },
        { PAD_BUTTON_UP, "Up" },    { PAD_BUTTON_DOWN, "Down" },  { PAD_BUTTON_LEFT, "Left" },
        { PAD_BUTTON_RIGHT, "Right" },
    };
    for (size_t i = 0; i < sizeof kPress / sizeof kPress[0]; i++)
    {
        if (i != 0)
            ImGui::SameLine();
        if (ImGui::SmallButton(kPress[i].name))
            PortInputPress(kPress[i].bit, 0);
    }

    ImGui::SeparatorText("record");
    static char s_recPath[256] = "/tmp/strikers.pad";
    const char* recording = PortInputRecordPath();
    if (recording != nullptr)
    {
        ImGui::Text("recording to %s", recording);
        if (ImGui::SmallButton("stop recording"))
            PortInputRecordStop();
    }
    else
    {
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##recpath", s_recPath, sizeof s_recPath);
        ImGui::SameLine();
        if (ImGui::SmallButton("start recording"))
            PortInputRecordStart(s_recPath);
    }
    help("One line per change of pad state, frames counted from boot. STRIKERS_REPLAY_INPUT "
         "plays it back from boot; the replay below plays it from now.");

    ImGui::SeparatorText("replay");
    static char s_repPath[256] = "run.pad";
    unsigned long rcount = 0, rcursor = 0, rfirst = 0, rlast = 0;
    const char* rpath = nullptr;
    if (PortInputReplayStatus(&rcount, &rcursor, &rfirst, &rlast, &rpath))
    {
        ImGui::Text("%s: state %lu / %lu   frames %lu .. %lu   now %lu", rpath, rcursor + 1,
                    rcount, rfirst, rlast, PortInputFrame());
        if (ImGui::SmallButton("stop replay"))
            PortInputReplayStop();
    }
    else
    {
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##reppath", s_repPath, sizeof s_repPath);
        ImGui::SameLine();
        if (ImGui::SmallButton("start replay"))
            PortInputReplayStart(s_repPath);
    }

    ImGui::SeparatorText("keyboard");
    ImGui::TextDisabled("Detached from the pad while this menu is open.");
    static const struct
    {
        const char* key;
        const char* what;
    } kKeys[] = {
        { "X / Z / C / V", "A / B / X / Y" },
        { "Enter", "Start" },
        { "Space", "Z" },
        { "Q / E", "L / R" },
        { "W A S D", "control stick" },
        { "I J K L", "C stick" },
        { "arrows", "D-pad" },
        { "F1", "this menu" },
        { "F2", "the compact overlay" },
        { "F3 / F4", "freeze / quarter speed" },
        { "F11", "fullscreen" },
        { "F12 or P", "screenshot, to STRIKERS_SHOT_DIR" },
    };
    if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        for (size_t i = 0; i < sizeof kKeys / sizeof kKeys[0]; i++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", kKeys[i].key);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", kKeys[i].what);
        }
        ImGui::EndTable();
    }
}

void draw_system_tab()
{
    ImGui::SeparatorText("display");
    {
        bool fs = is_fullscreen();
        if (ImGui::Checkbox("fullscreen (F11)", &fs))
            set_fullscreen(fs);

        // No getter for either of these in Aurora. The vsync state lives at file scope
        // (vsync_state) because PDBG_SET_VSYNC changes it too; the slider owns its own from the
        // environment's answer onward.
        bool vsyncOn = vsync_state();
        if (ImGui::Checkbox("vsync", &vsyncOn))
            set_vsync(vsyncOn);
        help("Re-selects the present mode. The frame limiter's margin follows: with vsync "
             "on it sits 5% above the refresh rate so the two pacers never fight.");

        static float s_scale = [] {
            const char* e = std::getenv("STRIKERS_RES_SCALE");
            const float v = e != nullptr ? (float)std::atof(e) : 1.0f;
            return v > 0.0f ? v : 1.0f;
        }();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat("internal resolution", &s_scale, 0.25f, 4.0f, "%.2fx"))
            aurora_set_frame_buffer_scale(s_scale);

        double limitHz = 0.0, displayHz = 0.0;
        int vsync = 0, overridden = 0;
        PortFrameLimitInfo(&limitHz, &displayHz, &vsync, &overridden);
        ImGui::Text("limiter %.2f Hz%s   display %.0f Hz%s", limitHz,
                    limitHz == 0.0 ? " (uncapped)" : "", displayHz, vsync ? " vsync" : "");
        if (ImGui::SmallButton("59.94"))
            PortSetFrameLimit(59.94);
        ImGui::SameLine();
        if (ImGui::SmallButton("60"))
            PortSetFrameLimit(60.0);
        ImGui::SameLine();
        if (ImGui::SmallButton("120"))
            PortSetFrameLimit(120.0);
        ImGui::SameLine();
        if (ImGui::SmallButton("uncapped"))
            PortSetFrameLimit(0.0);
        ImGui::SameLine();
        ImGui::BeginDisabled(!overridden);
        if (ImGui::SmallButton("follow display"))
            PortSetFrameLimit(-1.0);
        ImGui::EndDisabled();
        help("The frame limiter's period. 59.94 is console pacing. The simulation is "
             "not tied to it; STRIKERS_FIXED_DT is.");
    }

    ImGui::SeparatorText("capture");
    static char s_shotPath[256] = "/tmp/strikers-menu-shot.ppm";
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("##shot", s_shotPath, sizeof s_shotPath);
    ImGui::SameLine();
    if (ImGui::SmallButton("capture frame"))
        aurora_capture_frame(s_shotPath);
    help("Binary PPM of the game's render target, without this menu: Aurora captures "
         "before ImGui is composited. F12 writes numbered shots to STRIKERS_SHOT_DIR.");

    ImGui::SeparatorText("memory");
    {
        size_t used = 0, total = 0;
        port_region_stats(&used, &total);
        ImGui::Text("game region %.1f / %.1f MiB handed out", used / 1048576.0, total / 1048576.0);
        help("The port's stand-in for the console's main RAM: a bump allocator the game's "
             "own allocators carve up.");

        PortGfxArena arena;
        if (PortGfxArenaStats(&arena))
        {
            ImGui::Text("graphics arena %u / %u KB   marker level %d", arena.used >> 10,
                        arena.size >> 10, arena.markerLevel);
            if (ImGui::BeginTable("arena", 1 + arena.typeCount,
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("level");
                for (int t = 0; t < arena.typeCount; t++)
                    ImGui::TableSetupColumn(arena.typeNames[t]);
                ImGui::TableHeadersRow();
                for (int level = 0; level <= arena.markerLevel && level < 8; level++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", level);
                    for (int t = 0; t < arena.typeCount; t++)
                    {
                        ImGui::TableNextColumn();
                        if (arena.bytes[level][t] != 0)
                            ImGui::Text("%u KB", arena.bytes[level][t] >> 10);
                    }
                }
                ImGui::EndTable();
            }
            help("The game's resource memory, by marker level and type: a level whose bytes "
                 "do not return to zero when it is released is a leak, by definition. "
                 "The same table prints when the arena is exhausted.");
        }
    }

    ImGui::SeparatorText("logging");
    ImGui::TextDisabled("Switches the tree reads on every use, so they take effect at once.");
    for (size_t i = 0; i < sizeof kLiveEnv / sizeof kLiveEnv[0]; i++)
    {
        bool on = std::getenv(kLiveEnv[i].var) != nullptr;
        if (ImGui::Checkbox(kLiveEnv[i].var, &on))
        {
            set_env(kLiveEnv[i].var, on ? "1" : nullptr);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", kLiveEnv[i].what);
    }

    ImGui::SeparatorText("session");
    {
        const char* cfg = PortConfigPath();
        ImGui::Text("strikers.ini: %s", cfg != nullptr ? cfg : "(none found)");
        ImGui::Text("backend %d   frame %lu", (int)aurora_get_backend(), PortInputFrame());
        static bool s_pauseOnFocusLost = false;
        if (ImGui::Checkbox("pause when the window loses focus", &s_pauseOnFocusLost))
            aurora_set_pause_on_focus_lost(s_pauseOnFocusLost);
        if (ImGui::SmallButton("quit"))
            s_quit = true;
    }
}

void draw_session_tab()
{
    const PortDebugSession& s = s_session;
    const PortDebugMatch& m = s_match;

    ImGui::Text("task state 0x%x  %s", s.taskState, task_state_name(s.taskState));
    ImGui::Text("scene   %s", s_scene);

    ImGui::SeparatorText("front-end stack");
    if (s.feDepth == 0)
        ImGui::TextDisabled("empty");
    for (int i = 0; i < s.feDepth && i < 8; i++)
        ImGui::Text("  %d  %s", s.feStack[i], scene_name(s.feStack[i]));
    ImGui::SeparatorText("overlay stack");
    if (s.overlayDepth == 0)
        ImGui::TextDisabled("empty");
    for (int i = 0; i < s.overlayDepth && i < 8; i++)
        ImGui::Text("  %d  %s", s.overlayStack[i], scene_name(s.overlayStack[i]));

    ImGui::SeparatorText("in a match");
    ImGui::BeginDisabled(!m.valid);
    if (ImGui::SmallButton(s.inPauseMenu ? "leave pause menu" : "open pause menu"))
        cmd(PDBG_PAUSE_MENU, s.inPauseMenu ? 0 : 1);
    help("FrontEnd::EnterMenuState, which is what Start does.");
    ImGui::SameLine();
    if (ImGui::SmallButton("return to front end"))
        cmd(PDBG_RETURN_TO_FE);
    help("The pause menu's forfeit path.");
    ImGui::EndDisabled();

    ImGui::SeparatorText("start a match from here");
    ImGui::TextDisabled("The `skipfe` boot path, from the front end: sets the line-up on");
    ImGui::TextDisabled("GameInfoManager and asks the task manager for the match state.");
    static int s_team[2] = { 3, 2 };
    static int s_side[2] = { 0, 1 };
    static int s_stad = 1;
    static int s_human = 1;   // 0 none, 1 home, 2 away
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("home", &s_team[0], kTeams, 9);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##sk1", &s_side[0], kSidekicks, 4);
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("away", &s_team[1], kTeams, 9);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##sk2", &s_side[1], kSidekicks, 4);
    ImGui::SetNextItemWidth(150.0f);
    ImGui::Combo("stadium", &s_stad, kStadiums, 7);
    {
        const char* const kSides[] = { "AI vs AI", "pad 0 home", "pad 0 away" };
        ImGui::SetNextItemWidth(150.0f);
        ImGui::Combo("control", &s_human, kSides, 3);
    }
    ImGui::BeginDisabled(m.valid || s.taskState != 4);
    if (ImGui::Button("start friendly"))
        cmd(PDBG_START_MATCH, s_team[0], s_team[1], s_stad, (float)s_side[0],
            (float)s_side[1], (float)(s_human - 1));
    ImGui::EndDisabled();
    if (s.taskState != 4 && !m.valid)
        ImGui::TextDisabled("(only from the front end)");
}

}   // namespace

namespace
{

// STRIKERS_DEBUG_CMD: queue commands from the environment, for a headless run.
void queue_scripted_commands(unsigned long frame)
{
    static const char* s_spec = std::getenv("STRIKERS_DEBUG_CMD");
    static bool s_fired[32];
    if (s_spec == nullptr)
        return;
    int index = 0;
    for (const char* p = s_spec; *p != '\0' && index < 32; index++)
    {
        const char* end = std::strchr(p, ';');
        const size_t len = end != nullptr ? (size_t)(end - p) : std::strlen(p);
        char item[160];
        if (len >= sizeof item)
            break;
        std::memcpy(item, p, len);
        item[len] = '\0';
        p += len + (end != nullptr ? 1 : 0);

        char* at = std::strchr(item, '@');
        if (at == nullptr || s_fired[index])
            continue;
        *at = '\0';
        const unsigned long when = std::strtoul(at + 1, nullptr, 10);
        if (frame != when)
            continue;
        s_fired[index] = true;

        // The syntax after the @frame is the control channel's cmd line, parsed there so the two
        // cannot drift apart.
        PortDebugCommand k;
        if (!PortDebugParseCommand(item, &k))
            continue;
        PortDebugPushCommand(&k);
        std::fprintf(stderr, "[overlay] scripted command %d at frame %lu\n", k.op, frame);
    }
}

}   // namespace

void PortOverlayDraw(void)
{
    // Before the read, not after: STRIKERS_OVERLAY=menu is what opens the menu and this is where
    // that is first seen.
    PortOverlayInit();
    queue_scripted_commands(PortInputFrame());
    sim_rate_tick();

    // STRIKERS_CONTROL, polled here because this runs every frame before the STRIKERS_OVERLAY test
    // and inside Aurora's frame, which is where shot has to ask for its readback.
    PortControlPoll();
    const bool menu = s_menuOpen;
    if (!menu && !s_enabled)
        return;

    PortBenchLive live;
    PortBenchGetLive(&live);

    // Say once, on stderr, that the overlay actually drew and how big ImGui thinks the display is.
    static bool s_said = false;
    if (!s_said)
    {
        s_said = true;
        const ImGuiIO& io = ImGui::GetIO();
        std::fprintf(stderr, "[overlay] drawing; imgui display %.0fx%.0f, context %s\n",
                     (double)io.DisplaySize.x, (double)io.DisplaySize.y,
                     ImGui::GetCurrentContext() != nullptr ? "live" : "NULL");
    }

    if (!menu)
    {
        // The compact heads-up form: what the frame costs, nothing to click.
        ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.72f);
        if (ImGui::Begin("strikers", nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("%.1f fps   frame %.2f ms   busy %.2f ms (p95 %.2f)", live.fps,
                        live.frameMs, live.busyMs, live.busyP95Ms);
            ImGui::Text("scene %s   stadium %s", s_scene, s_stadium);
            if (live.matchActive)
                ImGui::Text("match %5.1fs   %d - %d", (double)s_clock, s_scoreHome, s_scoreAway);
            ImGui::Separator();
            ImGui::TextDisabled("F1 for the debug menu, F2 hides this");
        }
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);
    bool open = true;
    if (ImGui::Begin("Super Mario Strikers debug", &open))
    {
        if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyScroll))
        {
            struct Tab
            {
                const char* name;
                void (*draw)();
            };
            if (ImGui::BeginTabItem("Frame"))
            {
                draw_frame_tab(live);
                ImGui::EndTabItem();
            }
            const Tab tabs[] = {
                { "Match", draw_match_tab },   { "Players", draw_players_tab },
                { "Items", draw_items_tab },   { "AI", draw_ai_tab },
                { "Cheats", draw_cheats_tab }, { "Camera", draw_camera_tab },
                { "Render", draw_render_tab }, { "Audio", draw_audio_tab },
                { "Input", draw_input_tab },   { "Session", draw_session_tab },
                { "System", draw_system_tab },
            };
            // STRIKERS_OVERLAY_TAB=<name> selects a tab on the first frame, so a headless run can
            // exercise a tab's draw code without a hand on the mouse.
            static const char* s_wantTab = std::getenv("STRIKERS_OVERLAY_TAB");
            for (size_t i = 0; i < sizeof tabs / sizeof tabs[0]; i++)
            {
                ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
                if (s_wantTab != nullptr && std::strcmp(s_wantTab, tabs[i].name) == 0)
                {
                    flags |= ImGuiTabItemFlags_SetSelected;
                    s_wantTab = nullptr;
                }
                if (ImGui::BeginTabItem(tabs[i].name, nullptr, flags))
                {
                    ImGui::BeginChild("body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
                    tabs[i].draw();
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (!open)
        set_menu_open(false);   // the title bar's close button
}
