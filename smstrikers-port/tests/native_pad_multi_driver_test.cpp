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
#include <array>
#include <deque>
#include <vector>
#include <psp2/ctrl.h>
#include <psp2/kernel/error.h>
#include "NL/platpad.h"
#include "NL/nlTicker.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feLibObject.h"
#include "port/overlay.h"
#include "port/native_pad.h"

static unsigned checks, groups, sdkCalls, overlayCalls, debugCalls, reportCalls;
static unsigned samplingCalls;
static unsigned extendedSamplingCalls, capabilityCalls, portInfoCalls, positive2Calls[5];
static int samplingResult, extendedSamplingResult, multiSupported, portInfoResult;
static SceCtrlPortInfo sdkPortInfo;
static int sdkPortResults[5] = {1, 1, 1, 1, 1};
static SceCtrlData sdkPortSamples[5];
static bool hardwareMotor[5];
static std::deque<int> actuatorResults[5];
static unsigned eventSequence, jobsShutdownSequence, startupShutdownSequence;
struct ActuatorCall { int port, result; SceCtrlActuator value; unsigned sequence; };
static std::vector<ActuatorCall> actuatorCalls;
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
    return samplingResult;
}
extern "C" int sceCtrlSetSamplingModeExt(SceCtrlPadInputMode mode)
{
    CHECK(mode == SCE_CTRL_MODE_ANALOG);
    ++extendedSamplingCalls;
    return extendedSamplingResult;
}
extern "C" int sceCtrlIsMultiControllerSupported()
{
    ++capabilityCalls;
    return multiSupported;
}
extern "C" int sceCtrlGetControllerPortInfo(SceCtrlPortInfo* output)
{
    CHECK(output != nullptr);
    ++portInfoCalls;
    *output = sdkPortInfo; // Poison/error output is intentionally writable.
    return portInfoResult;
}
extern "C" int sceCtrlPeekBufferPositive(int port, SceCtrlData* output, int count)
{
    CHECK(port == 0 && count == 1 && output != nullptr);
    ++sdkCalls;
    // Deliberately writes output even on error: those bytes must never be used.
    *output = sdkSample;
    return sdkResult;
}
extern "C" int sceCtrlPeekBufferPositive2(int port, SceCtrlData* output, int count)
{
    CHECK(port >= 0 && port < 5 && count == 1 && output != nullptr);
    ++sdkCalls;
    ++positive2Calls[port];
    *output = sdkPortSamples[port];
    return sdkPortResults[port];
}
extern "C" int sceCtrlSetActuator(int port, const SceCtrlActuator* value)
{
    CHECK(port > 0 && port < 5 && value != nullptr);
    for (const auto byte : value->unk) CHECK(byte == 0);
    CHECK((value->small == 0 && value->large == 0) ||
          (value->small == 255 && value->large == 255));
    int result = 0;
    if (!actuatorResults[port].empty())
    {
        result = actuatorResults[port].front();
        actuatorResults[port].pop_front();
    }
    actuatorCalls.push_back({port, result, *value, ++eventSequence});
    if (result == 0) hardwareMotor[port] = value->small != 0;
    if (static_cast<uint32_t>(result) == SCE_CTRL_ERROR_NO_DEVICE) hardwareMotor[port] = false;
    return result;
}
#if defined(NATIVE_PAD_BASELINE)
// This API did not exist before the staged change. The baseline is used only
// for the old normal-output digest and negative multi/rumble assertions.
extern "C" int PortPadShutdown() { return 0; }
#endif
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
    void SetPlayingSide(int i, int side) { CHECK(i >= 0 && i < 4); sides[i] = side; }
    static GameInfoManager* Instance();
};
template<class T> struct nlSingleton
{
    static T* Instance() { static T value; return &value; }
};
GameInfoManager* GameInfoManager::Instance() { return nlSingleton<GameInfoManager>::Instance(); }
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

// Render/tween/audio endpoints only; IChooseSide declarations and controller
// functions below are taken directly from the real files by the runner.
struct TLInstance
{
    bool m_bVisible = false;
    feVector3 position{};
    const feVector3& GetPosition() const { return position; }
    void SetAssetPosition(float x, float y, float z) { position.f = {x, y, z}; }
};
struct FETweener { float end = 0; void* object = nullptr; void (*setter)(void*, const float*) = nullptr; };
namespace TweenFunctions { static float linear(float, float, float, float) { return 0; } }
struct FETweenManager
{
    FETweener fixture;
    void clearTweensOnObj(void*) {}
    FETweener* createTween(float*, float* end, float duration, float delay, unsigned char count,
                          float (*)(float, float, float, float), void* object,
                          void (*setter)(void*, const float*))
    {
        CHECK(count == 1 && duration > 0 && delay == 0);
        fixture = {*end, object, setter};
        return &fixture;
    }
    void startTween(FETweener* tween) { tween->setter(tween->object, &tween->end); }
    void Update(float) {}
};
namespace FEAudio { static void PlayAnimAudioEvent(const char*, bool) {} }
#include "native_pad_choose_declarations.inc"
#include "native_pad_choose.inc"
#include "native_pad_remap.inc"

namespace port::jobs { static void shutdown() { jobsShutdownSequence = ++eventSequence; } }
namespace port::startup { static void shutdown() { startupShutdownSequence = ++eventSequence; } }
namespace port::native_runtime {
#include "native_pad_shutdown.inc"
}

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
static void resetInputWorld()
{
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
static void resetBackend(int supported)
{
    for (int port = 1; port < 5; ++port) actuatorResults[port].clear();
    CHECK(PortPadShutdown() == 0);
    for (int port = 1; port < 5; ++port) CHECK(!hardwareMotor[port]);
    actuatorCalls.clear();
    multiSupported = supported;
    samplingResult = extendedSamplingResult = portInfoResult = 0;
    std::memset(&sdkPortInfo, 0, sizeof sdkPortInfo);
    for (int port = 0; port < 5; ++port)
    {
        sdkPortResults[port] = 1;
        sdkPortSamples[port] = sample();
        positive2Calls[port] = 0;
    }
    sdkResult = 1;
    sdkSample = sample();
}
static void reset(int result = 1)
{
    resetBackend(0);
    sdkResult = result;
    resetInputWorld();
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

static cPlatPad& padAt(int slot)
{
    return *static_cast<cPlatPad*>(cPadManager::GetPad(slot));
}
static void frameMulti()
{
    ticker += 16;
    VBlankPadUpdate();
    updateBoth();
}
static void setupMulti(u8 primary, std::initializer_list<u8> ports)
{
    resetBackend(1);
    sdkPortInfo.port[0] = primary;
    int port = 1;
    for (u8 type : ports) { CHECK(port < 5); sdkPortInfo.port[port++] = type; }
    std::memset(sdkPortInfo.unk, SCE_CTRL_TYPE_DS4, sizeof sdkPortInfo.unk);
    resetInputWorld();
}
static void edgesAt(int slot, u16 pressed, u16 released)
{
    for (int category = 0; category < 2; ++category)
    {
        CHECK(padCategories[category].m_justPressed[slot] == pressed);
        CHECK(padCategories[category].m_justReleased[slot] == released);
    }
    CHECK(g_pFEInput->JustPressed((eFEINPUT_PAD)slot, PAD_BUTTON_A, false, nullptr)
          == ((pressed & PAD_BUTTON_A) != 0));
    CHECK(g_pFEInput->JustReleased((eFEINPUT_PAD)slot, PAD_BUTTON_A, false, nullptr)
          == ((released & PAD_BUTTON_A) != 0));
}
static u32 motorMask()
{
    PADStatus status[4];
    return PADRead(status);
}
static void topology()
{
    group("handheld_multicontroller_keeps_builtin_primary_and_fixed_external_slots");
    setupMulti(SCE_CTRL_TYPE_PHY, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4, 0, SCE_CTRL_TYPE_DS4});
    sdkPortSamples[0] = sample(SCE_CTRL_CROSS);
    sdkPortSamples[1] = sample(SCE_CTRL_CIRCLE);
    sdkPortSamples[2] = sample(SCE_CTRL_SQUARE);
    sdkPortSamples[4] = sample(SCE_CTRL_START); // Declared but outside four-pad capacity.
    frameMulti();
    CHECK(padAt(1).IsConnected()); // Baseline negative: old PAD exposes PAD0 only.
    CHECK(padAt(0).IsPressed(PAD_BUTTON_A, false));
    CHECK(padAt(1).IsPressed(PAD_BUTTON_B, false));
    CHECK(padAt(2).IsPressed(PAD_BUTTON_X, false) && !padAt(3).IsConnected());
    CHECK(positive2Calls[0] && positive2Calls[1] && positive2Calls[2]);
    CHECK(!positive2Calls[3] && !positive2Calls[4]);
    CHECK(motorMask() == 0x60000000u);

    group("virtual_primary_is_not_an_extra_player_and_holes_do_not_compact");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS3, 0, SCE_CTRL_TYPE_DS4, SCE_CTRL_TYPE_DS3});
    sdkPortSamples[0] = sdkPortSamples[1] = sample(SCE_CTRL_CROSS);
    sdkPortSamples[3] = sample(SCE_CTRL_SQUARE);
    sdkPortSamples[4] = sample(SCE_CTRL_TRIANGLE);
    frameMulti();
    CHECK(padAt(0).IsPressed(PAD_BUTTON_A, false));
    CHECK(!padAt(1).IsConnected());
    CHECK(padAt(2).IsPressed(PAD_BUTTON_X, false));
    CHECK(padAt(3).IsPressed(PAD_BUTTON_Y, false));
    CHECK(positive2Calls[0] == 0 && positive2Calls[2] == 0);
    CHECK(positive2Calls[1] && positive2Calls[3] && positive2Calls[4]);
    CHECK(motorMask() == 0xb0000000u);

    group("four_physical_ports_exact_masks_and_independent_axes");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4,
                                   SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4});
    const unsigned buttons[] = {SCE_CTRL_UP, SCE_CTRL_RIGHT, SCE_CTRL_DOWN, SCE_CTRL_LEFT};
    const u16 mapped[] = {PAD_BUTTON_UP, PAD_BUTTON_RIGHT, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT};
    for (int i = 0; i < 4; ++i)
        sdkPortSamples[i+1] = sample(buttons[i], i&1 ? 255 : 0, i&2 ? 255 : 0, 128, 127);
    frameMulti();
    for (int i = 0; i < 4; ++i)
    {
        CHECK(padAt(i).IsConnected() && padAt(i).IsPressed(mapped[i], false));
        CHECK((padAt(i).AnalogLeftX() > 0) == bool(i&1));
        CHECK((padAt(i).AnalogLeftY() < 0) == bool(i&2));
        edgesAt(i, mapped[i], 0);
    }
    CHECK(motorMask() == 0xf0000000u && positive2Calls[0] == 0);

    group("unknown_external_types_not_admitted_and_unpaired_primary_not_sampled");
    setupMulti(SCE_CTRL_TYPE_UNPAIRED, {SCE_CTRL_TYPE_VIRT, SCE_CTRL_TYPE_PHY, 0xff, SCE_CTRL_TYPE_DS4});
    CHECK(!padAt(0).IsConnected() && !padAt(1).IsConnected() && !padAt(2).IsConnected());
    CHECK(padAt(3).IsConnected() && motorMask() == 0x10000000u);
    CHECK(!positive2Calls[0] && !positive2Calls[1] && !positive2Calls[2] && !positive2Calls[3]);
}

static void shouldersOverlay()
{
    group("positive2_l1_r1_match_original_shoulders_and_l2_r2_do_not_alias");
    setupMulti(SCE_CTRL_TYPE_PHY, {SCE_CTRL_TYPE_DS4});
    for (int port = 0; port < 2; ++port)
        sdkPortSamples[port] = sample(SCE_CTRL_L1 | SCE_CTRL_R1, 255, 127, 128, 0);
    const unsigned before = overlayCalls;
    frameMulti();
    CHECK(overlayCalls == before+1);
    CHECK((lastOverlayButtons & (SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER)) ==
          (SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER));
    for (int i = 0; i < 2; ++i)
    {
        CHECK(padAt(i).GetPressure(PAD_TRIGGER_L, false) == 1);
        CHECK(padAt(i).GetPressure(PAD_TRIGGER_R, false) == 1);
        CHECK(padAt(i).AnalogLeftX() == 1 && padAt(i).AnalogRightY() == 1);
        edgesAt(i, PAD_TRIGGER_L | PAD_TRIGGER_R, 0);
        sdkPortSamples[i] = sample(SCE_CTRL_L2 | SCE_CTRL_R2);
    }
    frameMulti();
    for (int i = 0; i < 2; ++i)
    {
        CHECK(padAt(i).GetPressure(PAD_TRIGGER_L, false) == 0);
        CHECK(padAt(i).GetPressure(PAD_TRIGGER_R, false) == 0);
        edgesAt(i, 0, PAD_TRIGGER_L | PAD_TRIGGER_R);
    }
    CHECK(!(lastOverlayButtons & (SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER)));

    group("overlay_primary_only_and_menu_filters_all_players");
    sdkPortSamples[0] = sample(SCE_CTRL_CROSS);
    sdkPortSamples[1] = sample(SCE_CTRL_SELECT | SCE_CTRL_START | SCE_CTRL_L1 | SCE_CTRL_R1);
    frameMulti();
    CHECK(lastOverlayButtons == SCE_CTRL_CROSS);
    overlayOpen = true;
    frameMulti();
    for (int i = 0; i < 2; ++i)
    {
        CHECK(padAt(i).IsConnected() && !padAt(i).IsPressed(0xffff, false));
        CHECK(padAt(i).AnalogLeftX() == 0 && padAt(i).GetPressure(PAD_TRIGGER_L, false) == 0);
    }
    overlayOpen = false;
    frameMulti();
    CHECK(padAt(0).IsPressed(PAD_BUTTON_A, false));
    const auto* previous = PadStatus::s_Current;
    const unsigned overlayBefore = overlayCalls;
    sdkPortResults[1] = sdkError(SCE_KERNEL_ERROR_NO_MEMORY);
    sdkPortSamples[0] = sample(SCE_CTRL_START);
    frameMulti();
    CHECK(PadStatus::s_Current == previous && overlayCalls == overlayBefore);
    CHECK(padAt(0).IsPressed(PAD_BUTTON_A, false) && !padAt(0).IsPressed(PAD_BUTTON_START, false));
    edgesAt(0, 0, 0);
}

static void hotplug()
{
    group("physical_disconnect_does_not_move_other_players_and_reconnect_is_new_press");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4});
    sdkPortSamples[1] = sample(SCE_CTRL_CROSS, 255, 127);
    sdkPortSamples[2] = sample(SCE_CTRL_CIRCLE);
    frameMulti(); edgesAt(0, PAD_BUTTON_A, 0); edgesAt(1, PAD_BUTTON_B, 0);
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_UNPAIRED;
    frameMulti();
    CHECK(!padAt(0).IsConnected() && padAt(0).AnalogLeftX() == 0);
    CHECK(padAt(1).IsConnected() && padAt(1).IsPressed(PAD_BUTTON_B, false));
    edgesAt(0, 0, 0); edgesAt(1, 0, 0);
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS3;
    frameMulti();
    CHECK(padAt(0).IsConnected()); edgesAt(0, PAD_BUTTON_A, 0);
    frameMulti(); edgesAt(0, 0, 0);

    group("same_port_different_type_has_observable_disconnect_before_attach");
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS4;
    frameMulti(); CHECK(!padAt(0).IsConnected()); edgesAt(0, 0, 0);
    frameMulti(); CHECK(padAt(0).IsConnected()); edgesAt(0, PAD_BUTTON_A, 0);

    group("primary_topology_change_does_not_inherit_held_state");
    sdkPortInfo.port[0] = SCE_CTRL_TYPE_PHY;
    sdkPortSamples[0] = sample(SCE_CTRL_SQUARE);
    frameMulti();
    CHECK(!padAt(0).IsConnected() && !padAt(1).IsConnected());
    frameMulti();
    CHECK(padAt(0).IsPressed(PAD_BUTTON_X, false)); edgesAt(0, PAD_BUTTON_X, 0);
    CHECK(padAt(1).IsPressed(PAD_BUTTON_A, false)); edgesAt(1, PAD_BUTTON_A, 0);
    CHECK(padAt(2).IsPressed(PAD_BUTTON_B, false));

    group("declared_controller_missing_at_peek_is_disconnected_then_recovers");
    sdkPortResults[1] = sdkError(SCE_CTRL_ERROR_NO_DEVICE);
    frameMulti(); CHECK(!padAt(1).IsConnected()); edgesAt(1, 0, 0);
    sdkPortResults[1] = 0;
    frameMulti(); CHECK(!padAt(1).IsConnected());
    sdkPortResults[1] = 1;
    frameMulti(); CHECK(padAt(1).IsConnected()); edgesAt(1, PAD_BUTTON_A, 0);
}

static void groupPublication()
{
    group("disconnect_latch_survives_reappearance_while_another_pad_blocks_publication");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4});
    sdkPortSamples[1] = sample(SCE_CTRL_CROSS, 255, 127);
    sdkPortSamples[2] = sample(SCE_CTRL_CIRCLE);
    frameMulti();
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    CHECK(hardwareMotor[1]);
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS4;
    sdkPortResults[2] = sdkError(SCE_KERNEL_ERROR_NO_MEMORY);
    const auto* previous = PadStatus::s_Current;
    const unsigned overlayBefore = overlayCalls;
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        actuatorResults[1].push_back(sdkError(SCE_KERNEL_ERROR_NO_MEMORY));
        frameMulti();
        CHECK(PadStatus::s_Current == previous && padAt(0).IsConnected());
        CHECK(padAt(0).IsPressed(PAD_BUTTON_A, false) && padAt(0).AnalogLeftX() == 1);
        CHECK(hardwareMotor[1] && overlayCalls == overlayBefore);
        edgesAt(0, 0, 0); edgesAt(1, 0, 0);
        const auto callsBefore = actuatorCalls.size();
        PADControlMotor(0, PAD_MOTOR_RUMBLE);
        CHECK(actuatorCalls.size() == callsBefore);
        CHECK(PADReset(0x10000000u) == 1); // Resetting absent PAD3 cannot service PAD0's debt.
        CHECK(actuatorCalls.size() == callsBefore && hardwareMotor[1]);
        sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS3; // It reappears before -1 can be published.
    }
    actuatorResults[1].push_back(sdkError(SCE_CTRL_ERROR_NO_DEVICE));
    frameMulti();
    CHECK(PadStatus::s_Current == previous && !hardwareMotor[1]);
    sdkPortResults[2] = 1;
    frameMulti();
    CHECK(!padAt(0).IsConnected() && padAt(0).AnalogLeftX() == 0);
    CHECK(padAt(1).IsPressed(PAD_BUTTON_B, false)); edgesAt(0, 0, 0); edgesAt(1, 0, 0);
    frameMulti();
    CHECK(padAt(0).IsConnected()); edgesAt(0, PAD_BUTTON_A, 0);
    frameMulti(); edgesAt(0, 0, 0);

    group("late_old_motor_stop_cannot_disconnect_a_new_attach_generation");
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    CHECK(hardwareMotor[1]);
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS4;
    // PADRead and the two real UpdatePlatPad consumers may each retry a stop
    // for a now-disconnected channel. Each call remains bounded.
    for (int i = 0; i < 8; ++i) actuatorResults[1].push_back(sdkError(SCE_KERNEL_ERROR_NO_MEMORY));
    frameMulti(); CHECK(!padAt(0).IsConnected());
    frameMulti(); CHECK(padAt(0).IsConnected()); edgesAt(0, PAD_BUTTON_A, 0);
    CHECK(hardwareMotor[1]);
    actuatorResults[1].clear();
    actuatorResults[1].push_back(sdkError(SCE_CTRL_ERROR_NO_DEVICE));
    frameMulti();
    CHECK(padAt(0).IsConnected() && !hardwareMotor[1]); edgesAt(0, 0, 0);
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    CHECK(hardwareMotor[1]);
}

static void apiFailures()
{
    group("query_failure_never_uses_poison_topology_or_publishes_new_input");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4});
    sdkPortSamples[1] = sample(SCE_CTRL_CROSS);
    frameMulti();
    const auto* previous = PadStatus::s_Current;
    const unsigned readsBefore = sdkCalls, overlayBefore = overlayCalls;
    portInfoResult = sdkError(SCE_KERNEL_ERROR_NO_MEMORY);
    std::memset(&sdkPortInfo, 0xff, sizeof sdkPortInfo);
    frameMulti();
    CHECK(PadStatus::s_Current == previous && sdkCalls == readsBefore && overlayCalls == overlayBefore);
    CHECK(padAt(0).IsPressed(PAD_BUTTON_A, false)); edgesAt(0, 0, 0);
    portInfoResult = 0; // Unknown primary type is still unusable metadata.
    frameMulti(); CHECK(PadStatus::s_Current == previous && sdkCalls == readsBefore);
    sdkPortInfo.port[0] = SCE_CTRL_TYPE_VIRT;
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS3;
    sdkPortInfo.port[2] = SCE_CTRL_TYPE_DS4;
    sdkPortInfo.port[3] = sdkPortInfo.port[4] = 0;
    frameMulti(); CHECK(padAt(0).IsConnected()); edgesAt(0, 0, 0);

    group("sampling_capability_and_extended_mode_failures_retry_without_decoding");
    const int failures[] = {sdkError(SCE_KERNEL_ERROR_NO_MEMORY), sdkError(SCE_CTRL_ERROR_FATAL)};
    for (int fail : failures)
    {
        resetBackend(1);
        sdkPortInfo.port[0] = SCE_CTRL_TYPE_PHY;
        samplingResult = fail;
        const unsigned reads = sdkCalls;
        CHECK(PADInit() == FALSE);
        resetInputWorld(); CHECK(sdkCalls == reads);
        CHECK(PadStatus::s_Current[0].err == PAD_ERR_TRANSFER);
        samplingResult = 0; extendedSamplingResult = fail;
        CHECK(PADInit() == FALSE);
        frameMulti(); CHECK(sdkCalls == reads);
        extendedSamplingResult = 0;
        CHECK(PADInit() == TRUE);
        sdkPortSamples[0] = sample(SCE_CTRL_CROSS);
        frameMulti(); CHECK(padAt(0).IsPressed(PAD_BUTTON_A, false)); edgesAt(0, PAD_BUTTON_A, 0);
    }
    for (int invalidCapability : {2, sdkError(SCE_CTRL_ERROR_FATAL)})
    {
        resetBackend(invalidCapability);
        sdkPortInfo.port[0] = SCE_CTRL_TYPE_VIRT;
        sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS4;
        CHECK(PADInit() == FALSE);
        const unsigned reads = sdkCalls;
        resetInputWorld(); CHECK(sdkCalls == reads);
        multiSupported = 1;
        frameMulti(); CHECK(padAt(0).IsConnected() && !padAt(1).IsConnected());
    }

    group("per_port_zero_transient_and_oom_preserve_whole_last_sample");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4});
    sdkPortSamples[1] = sample(SCE_CTRL_CROSS);
    sdkPortSamples[2] = sample(SCE_CTRL_CIRCLE);
    frameMulti();
    for (int code : {0, sdkError(SCE_CTRL_ERROR_NOT_SUPPORTED), sdkError(SCE_KERNEL_ERROR_NO_MEMORY),
                    sdkError(SCE_CTRL_ERROR_PRIV_REQUIRED), sdkError(SCE_CTRL_ERROR_FATAL)})
    {
        const auto* last = PadStatus::s_Current;
        sdkPortResults[2] = code;
        sdkPortSamples[2] = sample(SCE_CTRL_START, 0, 255, 0, 255);
        frameMulti();
        CHECK(PadStatus::s_Current == last && padAt(1).IsPressed(PAD_BUTTON_B, false));
        CHECK(!padAt(1).IsPressed(PAD_BUTTON_START, false));
        edgesAt(0, 0, 0); edgesAt(1, 0, 0);
    }
    sdkPortResults[2] = 1; sdkPortSamples[2] = sample();
    frameMulti(); edgesAt(1, 0, PAD_BUTTON_B);
}

struct ChooseFixture
{
    IChooseSide choose;
    std::array<TLInstance, 21> instances;
    ChooseFixture()
    {
        for (int i = 0; i < 21; ++i) choose.mInstanceTable[i] = &instances[i];
        choose.mControllerDestPos[0] = -1;
        choose.mControllerDestPos[1] = 1;
        choose.mControllerDestPos[2] = 0;
        choose.ResetAndPositionControllers(true);
    }
    UpdateResult update() { return choose.Update(1.0f/60.0f, nullptr, -1); }
};

static void chooseSide()
{
    group("real_choose_side_two_players_move_ready_and_save_distinct_teams");
    setupMulti(SCE_CTRL_TYPE_PHY, {SCE_CTRL_TYPE_DS4});
    ChooseFixture menu;
    CHECK(menu.choose.AllControllersAreCentred() && !menu.choose.AtLeastOnePlayerReady());
    CHECK(menu.update() == UPDATE_OK);
    CHECK(menu.instances[0].m_bVisible && menu.instances[1].m_bVisible && !menu.instances[2].m_bVisible);
    sdkPortSamples[0] = sample(SCE_CTRL_LEFT);
    sdkPortSamples[1] = sample(SCE_CTRL_RIGHT);
    frameMulti(); CHECK(menu.update() == UPDATE_OK);
    CHECK(menu.choose.mPlayingSides[0] == 0 && menu.choose.mPlayingSides[1] == 1);
    CHECK(menu.instances[0].position.f.x == -1 && menu.instances[1].position.f.x == 1);
    sdkPortSamples[0] = sdkPortSamples[1] = sample();
    frameMulti(); menu.update();
    sdkPortSamples[0] = sample(SCE_CTRL_CROSS);
    frameMulti(); CHECK(menu.update() == UPDATE_OK);
    CHECK(menu.choose.mPlayerReady[0] && !menu.choose.mPlayerReady[1]);
    sdkPortSamples[0] = sample(); sdkPortSamples[1] = sample(SCE_CTRL_CROSS);
    frameMulti(); CHECK(menu.update() == UPDATE_GO_FORWARD);
    CHECK(menu.choose.AllPlayersReady() && menu.choose.AllPluggedInAreReady());
    CHECK(menu.instances[4].m_bVisible && menu.instances[5].m_bVisible && menu.instances[16].m_bVisible);
    menu.choose.SaveChanges();
    CHECK(GameInfoManager::Instance()->sides[0] == 0 && GameInfoManager::Instance()->sides[1] == 1);

    group("real_choose_side_transfer_does_not_repeat_ready_press_and_detach_clears_assignment");
    sdkPortResults[1] = 0;
    frameMulti(); CHECK(menu.update() == UPDATE_OK);
    CHECK(menu.choose.mPlayerReady[1]);
    sdkPortResults[1] = 1; sdkPortInfo.port[1] = 0;
    frameMulti(); CHECK(menu.update() == UPDATE_OK);
    CHECK(!menu.choose.mPlayerReady[1] && menu.choose.mPlayingSides[1] == -1);
    CHECK(!menu.instances[1].m_bVisible && !menu.instances[5].m_bVisible);
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS4; sdkPortSamples[1] = sample(SCE_CTRL_RIGHT);
    frameMulti(); CHECK(menu.update() == UPDATE_OK);
    CHECK(menu.choose.mPlayingSides[1] == 1 && !menu.choose.mPlayerReady[1]);
    menu.choose.SaveChanges();

    group("real_frontend_assigned_second_player_removal_opens_choose_sides_not_pause");
    for (int i = 0; i < 4; ++i) FrontEnd::m_ctrlConnectedState[i] = padAt(i).IsConnected();
    FrontEnd::m_bInPauseMenuState = false; FrontEnd::menuCalls = 0;
    sdkPortResults[1] = sdkError(SCE_CTRL_ERROR_FATAL);
    frameMulti(); FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 0);
    sdkPortResults[1] = sdkError(SCE_CTRL_ERROR_NO_DEVICE);
    frameMulti(); FrontEnd::UpdateForGame(1.0f/60.0f);
    CHECK(FrontEnd::menuCalls == 1 && FrontEnd::lastMenu == FrontEnd::MET_CHOOSESIDES);
    CHECK(FrontEnd::m_ctrlConnectedState[0] && !FrontEnd::m_ctrlConnectedState[1]);

    group("real_pause_choose_side_unplug_resets_ready_and_cancel_saves_remaining_player");
    menu.choose.mContext = CONTEXT_PAUSE;
    CHECK(menu.update() == UPDATE_OK);
    CHECK(!menu.choose.mPlayerReady[1] && menu.choose.mPlayingSides[1] == -1);
    sdkPortResults[1] = 1; sdkPortSamples[1] = sample();
    sdkPortSamples[0] = sample(SCE_CTRL_CIRCLE);
    frameMulti(); CHECK(menu.update() == UPDATE_GO_BACK);
    CHECK(GameInfoManager::Instance()->sides[0] == 0 && GameInfoManager::Instance()->sides[1] == -1);
}

static void rumble()
{
    group("motor_capability_mask_and_real_binary_rumble_timer");
    setupMulti(SCE_CTRL_TYPE_PHY, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4});
    CHECK(motorMask() == 0x60000000u); // Independent baseline negative.
    const auto before = actuatorCalls.size();
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    PADControlMotor(-1, PAD_MOTOR_RUMBLE); PADControlMotor(4, PAD_MOTOR_RUMBLE);
    PADControlMotor(1, 3);
    CHECK(actuatorCalls.size() == before);
    padAt(1).StartRumble(0.01f, 0.2f, 0.3f);
    CHECK(hardwareMotor[1] && !hardwareMotor[2]);
    CHECK(actuatorCalls.back().port == 1 && actuatorCalls.back().value.small == 255);
    updateBoth(); CHECK(!hardwareMotor[1] && !padAt(1).RumbleActive());
    PADControlMotor(1, PAD_MOTOR_RUMBLE); PADControlMotor(2, PAD_MOTOR_RUMBLE);
    CHECK(hardwareMotor[1] && hardwareMotor[2]);
    const auto repeated = actuatorCalls.size();
    PADControlMotor(1, PAD_MOTOR_RUMBLE);
    CHECK(actuatorCalls.size() == repeated);
    CHECK(PADReset(0x40000000u) == 1);
    CHECK(!hardwareMotor[1] && hardwareMotor[2]);
    PADControlMotor(2, PAD_MOTOR_STOP_HARD); CHECK(!hardwareMotor[2]);

    group("not_supported_capability_is_per_attach_and_start_oom_is_retryable");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS4});
    actuatorResults[1].push_back(sdkError(SCE_CTRL_ERROR_NOT_SUPPORTED));
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    CHECK(!hardwareMotor[1] && motorMask() == 0);
    const auto noRetry = actuatorCalls.size();
    PADControlMotor(0, PAD_MOTOR_RUMBLE); CHECK(actuatorCalls.size() == noRetry);
    sdkPortInfo.port[1] = 0; frameMulti();
    sdkPortInfo.port[1] = SCE_CTRL_TYPE_DS4; frameMulti();
    CHECK(motorMask() == 0x80000000u);
    actuatorResults[1].push_back(sdkError(SCE_KERNEL_ERROR_NO_MEMORY));
    PADControlMotor(0, PAD_MOTOR_RUMBLE); CHECK(!hardwareMotor[1]);
    PADControlMotor(0, PAD_MOTOR_RUMBLE); CHECK(hardwareMotor[1]);

    group("stop_failure_retains_owner_blocks_new_start_and_retries_bounded");
    actuatorResults[1].push_back(sdkError(SCE_KERNEL_ERROR_NO_MEMORY));
    const auto failedStop = actuatorCalls.size();
    PADControlMotor(0, PAD_MOTOR_STOP);
    CHECK(actuatorCalls.size() == failedStop+1 && hardwareMotor[1]);
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    CHECK(actuatorCalls.size() == failedStop+1 && hardwareMotor[1]);
    actuatorResults[1].push_back(sdkError(SCE_CTRL_ERROR_NOT_SUPPORTED));
    CHECK(PADReset(0x80000000u) == 0 && hardwareMotor[1]);
    CHECK(PADReset(0x80000000u) == 1 && !hardwareMotor[1]);
    PADControlMotor(0, PAD_MOTOR_RUMBLE); CHECK(hardwareMotor[1]);
    actuatorResults[1].push_back(sdkError(SCE_CTRL_ERROR_NO_DEVICE));
    PADControlMotor(0, PAD_MOTOR_STOP);
    CHECK(!hardwareMotor[1]);
    frameMulti(); CHECK(!padAt(0).IsConnected());
    frameMulti(); CHECK(padAt(0).IsConnected());

    group("actuator_missing_device_forces_disconnect_before_reconnect_press");
    sdkPortSamples[1] = sample(SCE_CTRL_CROSS);
    frameMulti(); edgesAt(0, PAD_BUTTON_A, 0);
    actuatorResults[1].push_back(sdkError(SCE_CTRL_ERROR_NO_DEVICE));
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    frameMulti(); CHECK(!padAt(0).IsConnected()); edgesAt(0, 0, 0);
    frameMulti(); CHECK(padAt(0).IsConnected()); edgesAt(0, PAD_BUTTON_A, 0);
}

static void shutdownMotors()
{
    group("runtime_shutdown_stops_all_physical_owners_before_jobs_and_startup_teardown");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4,
                                   SCE_CTRL_TYPE_DS3, SCE_CTRL_TYPE_DS4});
    for (int i = 0; i < 4; ++i) PADControlMotor(i, PAD_MOTOR_RUMBLE);
    for (int port = 1; port < 5; ++port) CHECK(hardwareMotor[port]);
    const auto before = actuatorCalls.size();
    port::native_runtime::shutdown();
    CHECK(actuatorCalls.size() == before+4);
    for (int port = 1; port < 5; ++port) CHECK(!hardwareMotor[port]);
    CHECK(actuatorCalls.back().sequence < jobsShutdownSequence && jobsShutdownSequence < startupShutdownSequence);
    const auto stopped = actuatorCalls.size();
    for (int i = 0; i < 4; ++i) PADControlMotor(i, PAD_MOTOR_RUMBLE);
    CHECK(actuatorCalls.size() == stopped);
    port::native_runtime::shutdown(); CHECK(actuatorCalls.size() == stopped);

    group("shutdown_error_is_bounded_visible_and_owned_until_later_retry");
    setupMulti(SCE_CTRL_TYPE_VIRT, {SCE_CTRL_TYPE_DS4});
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    actuatorResults[1].push_back(sdkError(SCE_KERNEL_ERROR_NO_MEMORY));
    const auto errorBefore = actuatorCalls.size();
    port::native_runtime::shutdown();
    CHECK(actuatorCalls.size() == errorBefore+1 && hardwareMotor[1]);
    CHECK(actuatorCalls.back().sequence < jobsShutdownSequence && jobsShutdownSequence < startupShutdownSequence);
    PADControlMotor(0, PAD_MOTOR_RUMBLE); CHECK(actuatorCalls.size() == errorBefore+1);
    CHECK(PortPadShutdown() == 0 && !hardwareMotor[1]);
    CHECK(actuatorCalls.size() == errorBefore+2);
}

int main(int argc, char** argv)
{
    cPlatPad pads[] = { cPlatPad(0), cPlatPad(1), cPlatPad(2), cPlatPad(3) };
    for (int i = 0; i < 4; ++i) cPadManager::m_aPads[i] = &pads[i];
    cPadManager::m_pRemapArray = g_pPadRemapArray;
    FEInput fe;
    g_pFEInput = &fe;
    const char* selected = argc == 2 ? argv[1] : "all";
    bool ran = false;
    for (const auto entry : { std::pair{"normal", normal}, std::pair{"transfer", transfer},
                             std::pair{"disconnect", disconnect}, std::pair{"frontend", frontend},
                             std::pair{"errors", errors}, std::pair{"topology", topology},
                             std::pair{"shoulders_overlay", shouldersOverlay},
                             std::pair{"hotplug", hotplug}, std::pair{"publication", groupPublication},
                             std::pair{"api_failures", apiFailures}, std::pair{"choose_side", chooseSide},
                             std::pair{"rumble", rumble}, std::pair{"shutdown", shutdownMotors} })
        if (std::strcmp(selected, "all") == 0 || std::strcmp(selected, entry.first) == 0)
        {
            entry.second(); ran = true;
        }
    CHECK(ran);
    std::printf("{\"status\":\"PASS\",\"case\":\"%s\",\"groups\":%u,\"checks\":%u,"
                "\"sdk_calls\":%u,\"normal_digest_fnv1a64\":\"%016llx\"}\n",
                selected, groups, checks, sdkCalls, static_cast<unsigned long long>(normalDigest));
}
