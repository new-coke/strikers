// Production PADRead (separate C unit), VBlank, PadStatus and FE connection flow.
// The SDK, clock and world/menu endpoints are fixtures; input bodies are intact.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <utility>
#include <initializer_list>
#include <psp2/ctrl.h>
#include "NL/platpad.h"
#include "NL/nlTicker.h"
#include "Game/FE/feInput.h"
#include "port/overlay.h"

static unsigned checks, groups, sdkCalls, overlayCalls, debugCalls, reportCalls;
static unsigned samplingCalls;
static u32 ticker;
static int sdkResult = 1;
static SceCtrlData sdkSample;
static bool overlayOpen;
static unsigned lastOverlayButtons;
static const char* currentGroup = "setup";
static uint64_t normalDigest = UINT64_C(14695981039346656037);

static void check(bool condition, const char* expression, int line)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "[pad-check-failed] group=%s line=%d expression=%s\n",
                     currentGroup, line, expression);
        std::exit(1);
    }
}
#define CHECK(expression) check((expression), #expression, __LINE__)

static void group(const char* name) { currentGroup = name; ++groups; }
static int sdkError(uint32_t bits)
{
    static_assert(sizeof(int) == sizeof(bits));
    int result;
    std::memcpy(&result, &bits, sizeof result);
    return result;
}

extern "C" int sceCtrlSetSamplingMode(SceCtrlPadInputMode mode)
{
    CHECK(mode == SCE_CTRL_MODE_ANALOG);
    ++samplingCalls;
    return SCE_CTRL_MODE_DIGITAL;
}
// Keep this original single-controller regression on the legacy SDK route.
// The multiplayer implementation references these APIs, but capability zero
// must never call them while executing any of the existing assertions below.
extern "C" int sceCtrlIsMultiControllerSupported(void)
{
    return 0;
}
extern "C" int sceCtrlSetSamplingModeExt(SceCtrlPadInputMode)
{
    CHECK(false && "legacy PAD must not configure extended sampling");
    return -1;
}
extern "C" int sceCtrlGetControllerPortInfo(SceCtrlPortInfo*)
{
    CHECK(false && "legacy PAD must not query external controller ports");
    return -1;
}
extern "C" int sceCtrlPeekBufferPositive2(int, SceCtrlData*, int)
{
    CHECK(false && "legacy PAD must not read external controller samples");
    return -1;
}
extern "C" int sceCtrlSetActuator(int, const SceCtrlActuator*)
{
    CHECK(false && "legacy PAD must not operate an external actuator");
    return -1;
}
extern "C" int sceCtrlPeekBufferPositive(int port, SceCtrlData* output, int count)
{
    CHECK(port == 0 && count == 1 && output != nullptr);
    ++sdkCalls;
    // Deliberately writes output even on error: those bytes must never be used.
    *output = sdkSample;
    return sdkResult;
}
extern "C" void PortOverlayVitaInput(unsigned int buttons)
{
    ++overlayCalls;
    lastOverlayButtons = buttons;
}
extern "C" int PortOverlayMenuOpen() { return overlayOpen; }
extern "C" void PortDebugSetPad(int, int, unsigned int, int, int, int, int, int, int)
{
    ++debugCalls;
}
extern "C" void OSReport(const char*, ...) { ++reportCalls; }
u32 nlGetTicker() { return ticker; }
f32 nlGetTickerDifference(uint start, uint end) { return static_cast<float>(end-start); }
// Polar math is unchanged by this patch and is not an oracle in this fixture.
float nlATan2f(float y, float x)
{
    const float angle = std::atan2(y, x);
    return angle < 0.0f ? angle+6.2831855f : angle;
}
float nlSqrt(float value, bool) { return std::sqrt(value); }

#include "native_pad_global.inc"
#include "native_pad_plat.inc"
#include "native_pad_fe.inc"

// Minimal world surrounding the full production FrontEnd::UpdateForGame body.
struct GameInfoManager
{
    bool mIsInStrikers101Mode = false;
    int sides[4] = { 0, -1, -1, -1 };
    int GetPlayingSide(unsigned short i) { CHECK(i < 4); return sides[i]; }
};
template<class T> struct nlSingleton
{
    static T* Instance() { static T value; return &value; }
};
struct nlTaskManager
{
    static nlTaskManager* m_pInstance;
    u32 m_CurrState = 2, m_PendingState = 2;
};
static nlTaskManager taskManager;
nlTaskManager* nlTaskManager::m_pInstance = &taskManager;
struct Presentation
{
    static Presentation& Instance() { static Presentation value; return value; }
    bool DuringEndOfGamePresentation() { return false; }
};
struct FrontEnd
{
    enum MenuEnterType { MET_PAUSE, MET_CHOOSESIDES };
    static inline bool m_bGameOver, m_bInPauseMenuState;
    static inline float m_pauseDelay;
    static inline u8 m_ctrlConnectedState[4];
    static inline eFEINPUT_PAD m_hitStartPad = FE_ALL_PADS;
    static inline unsigned menuCalls;
    static inline MenuEnterType lastMenu = MET_PAUSE;
    static void UpdateForGame(float delta);
    static void EnterMenuState(MenuEnterType type)
    {
        ++menuCalls;
        lastMenu = type;
        m_bInPauseMenuState = true;
    }
};
static unsigned char AlreadyStartedStrikers101Menu;
static unsigned char DontCheckForControllerRemovalHack;
#include "native_pad_frontend.inc"

static SceCtrlData sample(unsigned buttons = 0, u8 lx = 128, u8 ly = 127,
                          u8 rx = 128, u8 ry = 127)
{
    SceCtrlData value{};
    value.timeStamp = 123456; // Repeated timestamps are valid positive reads.
    value.buttons = buttons;
    value.lx = lx; value.ly = ly; value.rx = rx; value.ry = ry;
    return value;
}
static void updateBoth()
{
    UseDefaultPad(); UpdatePlatPad(1.0f/60.0f);
    UseFixedUpdatePad(); UpdatePlatPad(1.0f/60.0f);
    UseDefaultPad();
}
static void reset(int result = 1)
{
    sdkResult = result;
    sdkSample = sample();
    overlayOpen = false;
    std::memset(PadStatus::s_A, 0, sizeof PadStatus::s_A);
    std::memset(PadStatus::s_B, 0, sizeof PadStatus::s_B);
    PadStatus::s_Current = PadStatus::s_A;
    PadStatus::s_Next = PadStatus::s_B;
    UseDefaultPad();
    InitPlatPad();
    g_pFEInput->Reset();
    updateBoth();
    FrontEnd::m_bGameOver = FrontEnd::m_bInPauseMenuState = false;
    FrontEnd::m_pauseDelay = 0.0f;
    FrontEnd::m_hitStartPad = FE_ALL_PADS;
    FrontEnd::menuCalls = 0;
    for (int i = 0; i < 4; ++i)
        FrontEnd::m_ctrlConnectedState[i] = g_pFEInput->IsConnected((eFEINPUT_PAD)i);
    DontCheckForControllerRemovalHack = AlreadyStartedStrikers101Menu = 0;
    taskManager.m_CurrState = taskManager.m_PendingState = 2;
    *nlSingleton<GameInfoManager>::Instance() = GameInfoManager{};
}
static void frame(int result, SceCtrlData value)
{
    sdkResult = result;
    sdkSample = value;
    ticker += 16;
    VBlankPadUpdate();
    updateBoth();
}
static cPlatPad& pad0() { return *static_cast<cPlatPad*>(cPadManager::GetPad(0)); }
static void edges(u16 pressed, u16 released)
{
    for (int category = 0; category < 2; ++category)
    {
        CHECK(padCategories[category].m_justPressed[0] == pressed);
        CHECK(padCategories[category].m_justReleased[0] == released);
    }
    CHECK(g_pFEInput->JustPressed(FE_PAD1_ID, PAD_BUTTON_A, false, nullptr)
          == ((pressed & PAD_BUTTON_A) != 0));
    CHECK(g_pFEInput->JustReleased(FE_PAD1_ID, PAD_BUTTON_A, false, nullptr)
          == ((released & PAD_BUTTON_A) != 0));
}
static void otherPorts(const PADStatus* values)
{
    for (int i = 1; i < 4; ++i)
    {
        PADStatus expected{};
        expected.err = PAD_ERR_NO_CONTROLLER;
        CHECK(std::memcmp(&values[i], &expected, sizeof expected) == 0);
    }
}
static void digest(const PADStatus* values)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(values);
    for (size_t i = 0; i < sizeof(PADStatus)*PAD_MAX_CONTROLLERS; ++i)
    {
        normalDigest ^= bytes[i];
        normalDigest *= UINT64_C(1099511628211);
    }
}
static void raw(PADStatus* values, int result, SceCtrlData value)
{
    sdkResult = result;
    sdkSample = value;
    std::memset(values, 0xa5, sizeof(PADStatus)*4);
    CHECK(PADRead(values) == 0); // Native motor capability mask remains empty.
}

static void normal()
{
    group("layout_init_null_and_motor_mask");
    static_assert(sizeof(PADStatus) == 12 && offsetof(PADStatus, err) == 10);
    static_assert(sizeof(tGameCubePad) == 0xe0 && sizeof(PadStatus) == 0x39c);
    static_assert(sizeof(SceCtrlData) == 0x20 && sizeof(u32) == 4 && sizeof(s8) == 1);
    const unsigned initialCalls = sdkCalls;
    CHECK(PADRead(nullptr) == 0 && sdkCalls == initialCalls);
    CHECK(PADInit() == TRUE && samplingCalls == 1);
    CHECK(PADReset(0xffffffffu) == 1);
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    reset();

    group("all_physical_button_combinations");
    const unsigned vita[] = { SCE_CTRL_LEFT, SCE_CTRL_RIGHT, SCE_CTRL_DOWN, SCE_CTRL_UP,
        SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_SQUARE, SCE_CTRL_TRIANGLE,
        SCE_CTRL_LTRIGGER, SCE_CTRL_RTRIGGER, SCE_CTRL_SELECT, SCE_CTRL_START };
    const u16 game[] = { PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT, PAD_BUTTON_DOWN, PAD_BUTTON_UP,
        PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X, PAD_BUTTON_Y,
        PAD_TRIGGER_L, PAD_TRIGGER_R, PAD_TRIGGER_Z, PAD_BUTTON_START };
    PADStatus values[4];
    for (unsigned bits = 0; bits < 4096; ++bits)
    {
        unsigned buttons = SCE_CTRL_L3 | SCE_CTRL_R3 | SCE_CTRL_PSBUTTON | SCE_CTRL_VOLUP;
        u16 expected = 0;
        for (unsigned b = 0; b < 12; ++b)
            if (bits & (1u << b)) { buttons |= vita[b]; expected |= game[b]; }
        const unsigned priorOverlay = overlayCalls;
        raw(values, 1, sample(buttons));
        CHECK(overlayCalls == priorOverlay+1 && lastOverlayButtons == buttons);
        CHECK(values[0].err == PAD_ERR_NONE && values[0].button == expected);
        CHECK(values[0].stickX == 0 && values[0].stickY == 0);
        CHECK(values[0].substickX == 0 && values[0].substickY == 0);
        CHECK(values[0].triggerLeft == ((expected & PAD_TRIGGER_L) ? 255 : 0));
        CHECK(values[0].triggerRight == ((expected & PAD_TRIGGER_R) ? 255 : 0));
        CHECK(values[0].analogA == ((expected & PAD_BUTTON_A) ? 255 : 0));
        CHECK(values[0].analogB == ((expected & PAD_BUTTON_B) ? 255 : 0));
        otherPorts(values);
        digest(values);
        PADClampCircle(values);
        CHECK(values[0].triggerLeft == ((expected & PAD_TRIGGER_L) ? 150 : 0));
        CHECK(values[0].triggerRight == ((expected & PAD_TRIGGER_R) ? 150 : 0));
        digest(values);
    }

    group("exhaustive_axis_conversion_and_clamp_digest");
    for (int x = 0; x < 256; ++x)
        for (int y = 0; y < 256; ++y)
        {
            raw(values, 1, sample(0, x, y, 255-x, 255-y));
            CHECK(values[0].stickX == x-128 && values[0].stickY == 127-y);
            CHECK(values[0].substickX == 127-x && values[0].substickY == y-128);
            digest(values);
            PADClampCircle(values);
            CHECK(values[0].stickX >= -56 && values[0].stickX <= 56);
            CHECK(values[0].stickY >= -56 && values[0].stickY <= 56);
            CHECK(values[0].substickX >= -44 && values[0].substickX <= 44);
            CHECK(values[0].substickY >= -44 && values[0].substickY <= 44);
            if (x >= 113 && x <= 143 && y >= 112 && y <= 142)
                CHECK(values[0].stickX == 0 && values[0].stickY == 0);
            digest(values);
        }
    raw(values, 1, sample(SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER, 255, 127, 0, 127));
    PADClampCircle(values);
    CHECK(values[0].stickX == 56 && values[0].substickX == -44);
    CHECK(values[0].triggerLeft == 150 && values[0].triggerRight == 150);
    raw(values, 1, sample(0, 128, 0, 128, 255));
    PADClampCircle(values);
    CHECK(values[0].stickY == 56 && values[0].substickY == -44);

    group("valid_repeated_timestamp_and_two_categories");
    reset();
    frame(1, sample(SCE_CTRL_CROSS)); edges(PAD_BUTTON_A, 0);
    frame(1, sample(SCE_CTRL_CROSS)); edges(0, 0);
    frame(1, sample()); edges(0, PAD_BUTTON_A);
    frame(1, sample()); edges(0, 0);
    frame(1, sample(SCE_CTRL_CROSS)); edges(PAD_BUTTON_A, 0);
    CHECK(pad0().IsConnected() && g_pFEInput->IsConnected(FE_ALL_PADS));

    group("overlay_valid_input_and_analog_dpad_mapping");
    reset();
    overlayOpen = true;
    frame(1, sample(SCE_CTRL_CROSS | SCE_CTRL_START | SCE_CTRL_LTRIGGER, 255, 0, 0, 255));
    CHECK(PadStatus::s_Current[0].err == PAD_ERR_NONE);
    CHECK(PadStatus::s_Current[0].button == 0 && pad0().AnalogLeftX() == 0);
    CHECK(pad0().AnalogRightY() == 0 && pad0().GetPressure(PAD_TRIGGER_L, false) == 0);
    CHECK(debugCalls > 0);
    edges(0, 0);
    overlayOpen = false;
    g_pFEInput->EnableAnalogToDPadMapping(FE_PAD1_ID, true);
    frame(1, sample(0, 255, 127));
    CHECK(pad0().IsPressed(PAD_BUTTON_RIGHT, false));
    frame(1, sample());
    CHECK(!pad0().IsPressed(PAD_BUTTON_RIGHT, false));
}

static void transfer()
{
    group("transfer_preserves_held_sample_without_ghost_edges");
    reset();
    const auto held = sample(SCE_CTRL_CROSS | SCE_CTRL_LTRIGGER, 255, 127, 0, 127);
    frame(1, held);
    edges(PAD_BUTTON_A | PAD_TRIGGER_L, 0);
    PADStatus* const priorCurrent = PadStatus::s_Current;
    const PADStatus prior = priorCurrent[0];
    const float hold = padCategories[0].m_GameCubePads[0].fButtonStateTime[8];
    const uint32_t errors[] = { 0, SCE_CTRL_ERROR_INVALID_ARG, SCE_CTRL_ERROR_PRIV_REQUIRED,
        SCE_CTRL_ERROR_NOT_SUPPORTED, SCE_CTRL_ERROR_INVALID_MODE, SCE_CTRL_ERROR_FATAL,
        0xffffffffu, 0x80000000u, 0x8034ffffu };
    for (const auto error : errors)
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            const unsigned overlayBefore = overlayCalls;
            frame(sdkError(error), sample(SCE_CTRL_START | SCE_CTRL_CIRCLE, 0, 255, 255, 0));
            // This is the pre-patch negative oracle: old PADRead swaps in a
            // connected neutral sample and emits a false release instead.
            CHECK(PadStatus::s_Current == priorCurrent);
            CHECK(std::memcmp(&PadStatus::s_Current[0], &prior, sizeof prior) == 0);
            CHECK(overlayCalls == overlayBefore);
            CHECK(pad0().IsPressed(PAD_BUTTON_A, false));
            CHECK(!pad0().IsPressed(PAD_BUTTON_START | PAD_BUTTON_B, false));
            CHECK(pad0().AnalogLeftX() == 1 && pad0().AnalogRightX() == -1);
            CHECK(pad0().GetPressure(PAD_TRIGGER_L, false) == 1);
            CHECK(padCategories[0].m_GameCubePads[0].fButtonStateTime[8] == hold);
            CHECK(pad0().IsConnected() && g_pFEInput->IsConnected(FE_ALL_PADS));
            edges(0, 0);
        }
    frame(1, held); edges(0, 0);
    frame(1, sample()); edges(0, PAD_BUTTON_A | PAD_TRIGGER_L);
    frame(1, sample()); edges(0, 0);
    frame(1, held); edges(PAD_BUTTON_A | PAD_TRIGGER_L, 0);

    group("transfer_before_first_category_consumption");
    reset();
    sdkResult = 1; sdkSample = sample(SCE_CTRL_CROSS); ticker += 16;
    VBlankPadUpdate(); // The valid press has not yet reached either consumer.
    frame(sdkError(SCE_CTRL_ERROR_INVALID_MODE), sample(SCE_CTRL_START));
    edges(PAD_BUTTON_A, 0);
    frame(sdkError(SCE_CTRL_ERROR_INVALID_MODE), sample(SCE_CTRL_START));
    edges(0, 0);
}

static void disconnect()
{
    group("disconnect_clears_input_and_connected_predicate");
    reset();
    const auto held = sample(SCE_CTRL_CROSS | SCE_CTRL_LTRIGGER, 255, 0, 0, 255);
    frame(1, held);
    CHECK(pad0().AnalogLeftX() != 0 && pad0().AnalogRightY() != 0);
    frame(sdkError(SCE_CTRL_ERROR_NO_DEVICE), held);
    // This is the second independent negative oracle, not an err assignment.
    CHECK(!pad0().IsConnected());
    CHECK(!g_pFEInput->IsConnected(FE_PAD1_ID) && !g_pFEInput->IsConnected(FE_ALL_PADS));
    CHECK(!pad0().IsPressed(PAD_BUTTON_A | PAD_TRIGGER_L, false));
    CHECK(pad0().AnalogLeftX() == 0 && pad0().AnalogLeftY() == 0);
    CHECK(pad0().AnalogRightX() == 0 && pad0().AnalogRightY() == 0);
    CHECK(pad0().GetPressure(PAD_TRIGGER_L, false) == 0);
    for (int category = 0; category < 2; ++category)
    {
        tGameCubePad neutral{};
        CHECK(std::memcmp(&padCategories[category].m_GameCubePads[0], &neutral, sizeof neutral) == 0);
        CHECK(padCategories[category].m_previousButtons[0] == 0);
    }
    edges(0, 0); // Retail deliberately emits no synthetic release on disconnect.

    group("disconnected_transfer_and_reconnection_new_press");
    frame(sdkError(SCE_CTRL_ERROR_NO_DEVICE), held); edges(0, 0);
    PADStatus* const disconnected = PadStatus::s_Current;
    frame(0, sample(SCE_CTRL_START));
    CHECK(PadStatus::s_Current == disconnected && !pad0().IsConnected()); edges(0, 0);
    frame(sdkError(SCE_CTRL_ERROR_FATAL), sample(SCE_CTRL_START));
    CHECK(!g_pFEInput->IsConnected(FE_ALL_PADS)); edges(0, 0);
    frame(1, held);
    CHECK(pad0().IsConnected() && pad0().AnalogLeftX() > 0);
    edges(PAD_BUTTON_A | PAD_TRIGGER_L, 0);
    frame(1, held); edges(0, 0);
    frame(1, sample()); edges(0, PAD_BUTTON_A | PAD_TRIGGER_L);
    frame(1, sample()); edges(0, 0);
}

static void frontend()
{
    group("real_frontend_removal_predicate");
    reset();
    frame(1, sample(SCE_CTRL_CROSS));
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 0 && FrontEnd::m_ctrlConnectedState[0] == 1);
    frame(sdkError(SCE_CTRL_ERROR_FATAL), sample(SCE_CTRL_START));
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 0 && FrontEnd::m_ctrlConnectedState[0] == 1);
    frame(sdkError(SCE_CTRL_ERROR_NO_DEVICE), sample(SCE_CTRL_START));
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 1 && FrontEnd::lastMenu == FrontEnd::MET_CHOOSESIDES);
    CHECK(FrontEnd::m_bInPauseMenuState && FrontEnd::m_ctrlConnectedState[0] == 0);
    frame(sdkError(SCE_CTRL_ERROR_NO_DEVICE), sample(SCE_CTRL_START));
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 1);
    FrontEnd::m_bInPauseMenuState = false;
    frame(1, sample());
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 1 && FrontEnd::m_ctrlConnectedState[0] == 1);

    group("real_pause_press_and_unassigned_disconnect");
    reset();
    frame(1, sample(SCE_CTRL_START));
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 1 && FrontEnd::lastMenu == FrontEnd::MET_PAUSE);
    FrontEnd::m_bInPauseMenuState = false;
    frame(sdkError(SCE_CTRL_ERROR_INVALID_MODE), sample());
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 1 && !FrontEnd::m_bInPauseMenuState);
    reset();
    nlSingleton<GameInfoManager>::Instance()->sides[0] = -1;
    frame(sdkError(SCE_CTRL_ERROR_NO_DEVICE), sample());
    FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 0 && FrontEnd::m_ctrlConnectedState[0] == 0);
}

static void errors()
{
    group("unusable_sdk_output_never_decoded");
    reset();
    const uint32_t codes[] = { 0, SCE_CTRL_ERROR_INVALID_ARG, SCE_CTRL_ERROR_PRIV_REQUIRED,
        SCE_CTRL_ERROR_NO_DEVICE, SCE_CTRL_ERROR_NOT_SUPPORTED, SCE_CTRL_ERROR_INVALID_MODE,
        SCE_CTRL_ERROR_FATAL, 0xffffffffu, 0x80000000u };
    PADStatus values[4];
    for (const auto code : codes)
    {
        const unsigned priorOverlay = overlayCalls;
        raw(values, sdkError(code), sample(0xffffffffu, 0, 255, 0, 255));
        PADStatus expected{};
        expected.err = code == SCE_CTRL_ERROR_NO_DEVICE ? PAD_ERR_NO_CONTROLLER : PAD_ERR_TRANSFER;
        CHECK(std::memcmp(&values[0], &expected, sizeof expected) == 0);
        PADClampCircle(values);
        CHECK(std::memcmp(&values[0], &expected, sizeof expected) == 0);
        CHECK(overlayCalls == priorOverlay);
        otherPorts(values);
    }

    group("initial_error_and_probe_enabled_recovery");
    reset(0); // No known sample: retail transfer state is neutral/connected.
    CHECK(pad0().IsConnected() && !pad0().IsPressed(0xffff, false));
    CHECK(pad0().AnalogLeftX() == 0 && pad0().AnalogRightX() == 0); edges(0, 0);
    reset(sdkError(SCE_CTRL_ERROR_NO_DEVICE));
    CHECK(!pad0().IsConnected()); edges(0, 0);
    CHECK(setenv("STRIKERS_PROBE_PAD", "1", 1) == 0);
    const unsigned reportsBefore = reportCalls;
    frame(1, sample(SCE_CTRL_CROSS)); edges(PAD_BUTTON_A, 0);
    frame(0, sample(SCE_CTRL_START)); edges(0, 0);
    frame(1, sample(SCE_CTRL_CROSS)); edges(0, 0);
    CHECK(reportCalls > reportsBefore);
    CHECK(unsetenv("STRIKERS_PROBE_PAD") == 0);
    g_pFEInput->m_bEnableInput[0] = false;
    CHECK(!g_pFEInput->IsConnected(FE_PAD1_ID));
    g_pFEInput->m_bEnableInput[0] = true;
    CHECK(g_pFEInput->IsConnected(FE_PAD1_ID));
}

int main(int argc, char** argv)
{
    cPlatPad pads[] = { cPlatPad(0), cPlatPad(1), cPlatPad(2), cPlatPad(3) };
    for (int i = 0; i < 4; ++i) cPadManager::m_aPads[i] = &pads[i];
    FEInput fe;
    g_pFEInput = &fe;
    const char* selected = argc == 2 ? argv[1] : "all";
    bool ran = false;
    for (const auto entry : { std::pair{"normal", normal}, std::pair{"transfer", transfer},
                             std::pair{"disconnect", disconnect}, std::pair{"frontend", frontend},
                             std::pair{"errors", errors} })
        if (std::strcmp(selected, "all") == 0 || std::strcmp(selected, entry.first) == 0)
        {
            entry.second(); ran = true;
        }
    CHECK(ran);
    std::printf("{\"status\":\"PASS\",\"case\":\"%s\",\"groups\":%u,\"checks\":%u,"
                "\"sdk_calls\":%u,\"normal_digest_fnv1a64\":\"%016llx\"}\n",
                selected, groups, checks, sdkCalls, static_cast<unsigned long long>(normalDigest));
}
