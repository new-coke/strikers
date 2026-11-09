// Switch overlay state, stats panel and CPU boost during loads.

#include "port/overlay.h"
#include "port/benchmark.h"
#include "port/control.h"
#include "port/framerate.h"

#include <aurora/gfx.h>

#include <switch.h>

#include <SDL3/SDL_gamepad.h>
#include <dolphin/pad.h>
#include <imgui.h>

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "NL/nlMemory.h"
#include "port/host.h"
#include "port/online_panel.h"
#include "port/shaders.h"
#include "port/switch/clocks.h"
#include "port/switch/memory.h"

#include <aurora/aurora.h>

extern bool g_bRunSimAndRenderInLockStep;

class cGame;
extern cGame* g_pGame;

namespace
{

char s_scene[64] = "-";
char s_stadium[64] = "-";
bool s_quit;
bool s_overlayVisible = false;
PortDebugMatch s_match;
PortDebugSession s_session;
int s_forcedStadium = -2;

const int kCommandCap = 32;
PortDebugCommand s_commands[kCommandCap];
int s_commandHead, s_commandCount;

void copy_into(char* dst, size_t cap, const char* src)
{
    std::snprintf(dst, cap, "%s", src != nullptr ? src : "-");
}

// Returns non-zero if the platform handled the command.
int platform_command(const PortDebugCommand& c)
{
    switch (c.op)
    {
    case PDBG_SET_FRAME_LIMIT:
        PortSetFrameLimit((double)c.f[0]);
        PortFrameLimitInfo(nullptr, nullptr, nullptr, nullptr);
        return 1;
    case PDBG_SET_VSYNC:
    {
        aurora_enable_vsync(c.a != 0);
        double displayHz = 0.0;
        PortFrameLimitInfo(nullptr, &displayHz, nullptr, nullptr);
        PortSetDisplayRefresh(displayHz, c.a != 0 ? 1 : 0);
        return 1;
    }
    case PDBG_SET_LOCKSTEP:
        g_bRunSimAndRenderInLockStep = c.a != 0;
        return 1;
    case PDBG_SET_DT_SNAP:
        PortSetTaskClockSnap(c.a);
        return 1;
    case PDBG_SET_WINDOW:
        // One fullscreen layer; its size follows the dock.
        return 1;
    default:
        return 0;
    }
}

}   // namespace

void PortOverlayInit(void) {}
int PortOverlayEnabled(void) { return 0; }

namespace
{

// Minus alone opens the online panel; the game reads neither stick click.
bool toggle_pressed()
{
    static bool held = false;

    bool down = false;
    for (u32 i = 0; i < PADCount(); i++)
        if (SDL_Gamepad* pad = PADGetSDLGamepadForIndex(i))
            down = down || (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK) &&
                            (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK) ||
                             SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)));

    const bool pressed = down && !held;
    held = down;
    return pressed;
}

const char* backend_name()
{
    switch (aurora_get_backend())
    {
    case BACKEND_WEBGPU:
        return "WebGPU";
    case BACKEND_D3D12:
        return "D3D12";
    case BACKEND_METAL:
        return "Metal";
    case BACKEND_VULKAN:
        return "Vulkan (NVK)";
    case BACKEND_OPENGL:
        return "OpenGL";
    case BACKEND_OPENGLES:
        return "OpenGL ES";
    default:
        return "none";
    }
}

// Returns zero if the clock service is unavailable.
u32 clock_mhz(PcvModuleId module)
{
    static bool tried = false;
    static bool ready = false;

    if (!tried)
    {
        tried = true;
        ready = R_SUCCEEDED(clkrstInitialize());
    }
    if (!ready)
        return 0;

    ClkrstSession session;
    if (R_FAILED(clkrstOpenSession(&session, module, 3)))
        return 0;

    u32 hz = 0;
    const bool ok = R_SUCCEEDED(clkrstGetClockRate(&session, &hz));
    clkrstCloseSession(&session);

    return ok ? hz / 1000000u : 0u;
}

// The longest the memory card screen waits for shaders, counted from the first frame.
const unsigned long long kStartupBoostCapNs = 120ull * 1000000000ull;
unsigned long long s_startupBoostBegin;
bool s_startupCompiled;
bool s_loadingIndicator;

void apply_cpu_boost()
{
    PortSwitchCpuBoost(!s_startupCompiled || s_loadingIndicator ? 1 : 0);
}

void update_cpu_boost()
{
    if (!s_startupCompiled)
    {
        const unsigned long long now = port_monotonic_ns();
        if (s_startupBoostBegin == 0)
            s_startupBoostBegin = now;
        uint32_t queued = 0;
        aurora_get_pipeline_counts(&queued, nullptr);
        s_startupCompiled = queued == 0 || now - s_startupBoostBegin >= kStartupBoostCapNs;
    }
    apply_cpu_boost();
}

// Mean and worst frame time, and Aurora's submit-to-done GPU time, over the last 64 frames.
double s_intervals[64];
unsigned s_intervalNext;
double s_frameMean;
double s_frameWorst;
double s_gpuSamples[64];
unsigned s_gpuNext;
uint64_t s_gpuCount;
double s_gpuMean;
double s_gpuWorst;

// A gap this long between frames is a standby or a HOME suspend.
const unsigned long long kPauseNs = 1000000000ull;

void window_stats(const double* samples, unsigned next, double* mean, double* worst)
{
    const unsigned count = next < 64 ? next : 64;
    double total = 0.0;
    *worst = 0.0;
    for (unsigned i = 0; i < count; i++)
    {
        total += samples[i];
        if (samples[i] > *worst)
            *worst = samples[i];
    }
    *mean = count > 0 ? total / count : 0.0;
}

void sample_frame()
{
    static unsigned long long last;
    const unsigned long long now = port_monotonic_ns();
    if (last != 0 && now - last >= kPauseNs)
    {
        s_intervalNext = 0;
        s_gpuNext = 0;
    }
    else if (last != 0)
    {
        s_intervals[s_intervalNext % 64] = (double)(now - last) / 1000000.0;
        s_intervalNext++;
    }
    last = now;

    uint64_t gpuLast = 0;
    uint64_t gpuCount = 0;
    aurora_gpu_frame_time(&gpuLast, nullptr, nullptr, &gpuCount);
    if (gpuCount != s_gpuCount)
    {
        s_gpuCount = gpuCount;
        // A frame submitted before a standby completes after it.
        if (gpuLast < kPauseNs)
        {
            s_gpuSamples[s_gpuNext % 64] = (double)gpuLast / 1000000.0;
            s_gpuNext++;
        }
    }

    window_stats(s_intervals, s_intervalNext, &s_frameMean, &s_frameWorst);
    window_stats(s_gpuSamples, s_gpuNext, &s_gpuMean, &s_gpuWorst);
}

void draw_overlay()
{
    const double mean = s_frameMean;
    const double worst = s_frameWorst;

    unsigned long long heapInUse = 0;
    unsigned long long heapFree = 0;
    PortSwitchHeapInfo(&heapInUse, &heapFree);

    u32 battery = 0;
    psmGetBatteryChargePercentage(&battery);
    PsmChargerType charger = PsmChargerType_Unconnected;
    psmGetChargerType(&charger);

    ImGui::SetNextWindowPos(ImVec2(12.f, 12.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    if (ImGui::Begin("strikers", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoNav))
    {
        ImGui::Text("%.1f fps   %.1f ms, worst %.1f", mean > 0.0 ? 1000.0 / mean : 0.0, mean, worst);

        PortBenchLive live;
        PortBenchGetLive(&live);
        ImGui::Text("CPU busy %.1f ms   present %.1f   sleep %.1f", live.busyMs, live.presentMs,
                    live.sleepMs);

        if (s_gpuNext > 0)
            ImGui::Text("GPU %.1f ms, worst %.1f", s_gpuMean, s_gpuWorst);
        else
            ImGui::TextUnformatted("GPU timing unavailable");
        ImGui::Separator();
        ImGui::Text("%s, %s at %.0fx%.0f", backend_name(),
                    appletGetOperationMode() == AppletOperationMode_Console ? "docked" : "handheld",
                    ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
        ImGui::Text("CPU %u MHz   GPU %u MHz   RAM %u MHz", clock_mhz(PcvModuleId_CpuBus),
                    clock_mhz(PcvModuleId_GPU), clock_mhz(PcvModuleId_EMC));
        ImGui::Text("Heap %llu MiB in use, %llu MiB free", heapInUse >> 20, heapFree >> 20);
        ImGui::Text("Game arena %u MiB free, largest block %u MiB",
                    nlVirtualTotalFree() >> 20, nlVirtualLargestBlock() >> 20);
        ImGui::Text("Battery %u%%%s", battery,
                    charger == PsmChargerType_Unconnected ? "" : ", charging");
        ImGui::Text("Shaders %d%% compiled", PortShaderStagePercent());
        ImGui::Separator();
        ImGui::Text("Scene %s   Stadium %s", s_scene, s_stadium);
    }
    ImGui::End();
}

}   // namespace

void PortSwitchLoadingIndicator(int on)
{
    // Match transitions report loads that end within a frame; FastLoad's GPU drop stalls them.
    s_loadingIndicator = on != 0 && g_pGame == nullptr;
    apply_cpu_boost();
}

void PortOverlayDraw(void)
{
    sample_frame();
    update_cpu_boost();
    if (toggle_pressed())
        s_overlayVisible = !s_overlayVisible;
    if (s_overlayVisible)
        draw_overlay();
    PortOnlinePanelDraw();
    PortControlPoll();
}

void PortOverlaySetScene(const char* name) { copy_into(s_scene, sizeof(s_scene), name); }
const char* PortOverlaySceneName(void) { return s_scene; }

void PortOverlaySetStadium(const char* name)
{
    copy_into(s_stadium, sizeof(s_stadium), name);
    PortBenchSetLabel("stadium", s_stadium);
}

void PortOverlaySetMatch(float, int, int) {}

void PortOverlayToggleMenu(void) {}
int PortOverlayMenuOpen(void) { return 0; }
void PortOverlayHandleKey(int, int) {}

int PortQuitRequested(void) { return s_quit ? 1 : 0; }
void PortRequestQuit(void) { s_quit = true; }

int PortDebugForcedStadium(void)
{
    if (s_forcedStadium == -2)
    {
        static const char* const stadiums[] = {
            "mario_stadium", "peach_toad", "dk_daisy", "wario_stadium",
            "yoshi_stadium", "super_stadium", "forbidden_dome",
        };
        s_forcedStadium = -1;
        const char* want = std::getenv("STRIKERS_FORCE_STADIUM");
        if (want != nullptr && *want != '\0')
        {
            for (int i = 0; i < 7; ++i)
                if (std::strcmp(want, stadiums[i]) == 0)
                    s_forcedStadium = i;
            if (s_forcedStadium < 0 && want[0] >= '0' && want[0] <= '6' && want[1] == '\0')
                s_forcedStadium = want[0] - '0';
        }
    }
    return s_forcedStadium;
}
void PortDebugSetForcedStadium(int index) { s_forcedStadium = (index >= 0 && index < 7) ? index : -1; }

void PortDebugSetPad(int, int, unsigned int, int, int, int, int, int, int) {}

void PortDebugSetMatch(const PortDebugMatch* match)
{
    if (match != nullptr)
        s_match = *match;
}
const PortDebugMatch* PortDebugGetMatch(void) { return &s_match; }

void PortDebugSetSession(const PortDebugSession* session)
{
    if (session != nullptr)
        s_session = *session;
}
const PortDebugSession* PortDebugGetSession(void) { return &s_session; }

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
