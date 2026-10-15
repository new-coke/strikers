#include "port/prompts.h"
#include "port/region.h"  // PORT: one binary, three discs
#include "port/host.h"
#include "types.h"
#include "NL/nlBind.h"
#include "NL/nlFunction.h"
#include "Game/main.h"
#if defined(PORT_VITA)
#include "Game/Ball.h"
#endif

#if defined(PORT_USE_AURORA)
#if defined(PORT_VITA)
#include <aurora_vita_backend.hpp>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <psp2/power.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <vector>
#include <SDL3/SDL_events.h>
#else
#include <aurora/aurora.h>
#include <aurora/gfx.h>   // aurora_present_waits_for_vblank
#endif
#include <dolphin/gx/GXAurora.h>   // AuroraSetViewportPolicy
#include <dolphin/vi.h>                // VILockAspectRatio
#include "port/aspect.h"
#include "port/framerate.h"
#if !defined(PORT_VITA)
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#endif
#include "port/overlay.h"
extern "C" void PortDebugFrame(void);   // PORT: defined in Game.cpp
#include "port/launch.h"
#if !defined(PORT_VITA)
#include "port/discord.h"
#include "port/shaders.h"   // PORT: the shader stage, held on the memory card screen
#include <aurora/main.h>   // #define main aurora_main
#include <aurora/event.h>
#endif
#include <stdio.h>   // PORT: snprintf
#endif
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <vector>
#include "port/aspect.h"
#include "port/framerate.h"
#include "port/overlay.h"
#include "port/launch.h"
#include "port/native_runtime.hpp"
#include "port/asset_io_capture.hpp"
extern "C" void PortDebugFrame(void);
extern "C" void PortInvokePadSamplingCallback(void);
#endif
#include "port/benchmark.h"
#include "port/audio.h"
#include "port/vita_profiler.h"
#if defined(PORT_VITA)
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/vita_performance_capture.hpp"
#include "port/vita_view_draw_capture.hpp"
#endif
#include "port/native_assets.hpp"
#include "port/native_audio.hpp"
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
#include "port/native_match_direct.hpp"
#endif
#if defined(STRIKERS_VITA_GXM_DIRECT_PROBE)
#include "port/native_gxm_direct_probe.hpp"
#endif
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include <aurora_vita_shader_debug.h>
#endif
#endif
#include "port/determinism.h"
#include "port/config.h"
#include "port/texture_packs.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Sys/audio.h"
#include "Game/Sys/clock.h"
#include "NL/gl/glView.h"
#include "NL/gl/glTexture.h"
#include "NL/gl/glState.h"
#include "NL/gl/gl.h"
#include "NL/gl/glAppAttach.h"
#include "NL/gl/glMemory.h"
#include "NL/glx/glxMemory.h"
#include "Game/Effects/ParticleSystem.h"
#include "Game/Transitions/ModelTransition.h"
#include "NL/nlConfig.h"
#include "NL/nlMain.h"
#include "NL/nlFileGC.h"
#include "NL/nlLocalization.h"
#include "NL/nlString.h"
#include "NL/platpad.h"
#include "Game/Game.h"
#include "Game/ProfileTask.h"
#include "Game/Sys/FloatingPointExceptions.h"
#include "Game/Sys/CallStackDumper.h"
#include "Game/Sys/eventman.h"
#include "Game/Sys/gcmemcard.h"
#include "Game/Sys/debug.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioEventHandler.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/FE/LidOpenMessage.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feManager.h"
#include "Game/BeginFrameTask.h"
#include "Game/DispatchEventsTask.h"
#include "Game/PlatPadUpdateTask.h"
#include "Game/FixedUpdateTask.h"
#include "Game/WorldUpdateTask.h"
#include "Game/GameRenderTask.h"
#include "Game/FrontEndTask.h"
#include "Game/ParticleUpdateTask.h"
#include "Game/TweakerTask.h"
#include "Game/EndFrameTask.h"
#include "Game/TransitionTask.h"
#include "Game/ComUpdateTask.h"
#include "Game/TestTask.h"
#include "Game/Loader/LoadingManager.h"
#include "Game/GL/GLInventory.h"
#include "Game/Debug/ShapeRender.h"
#include "Game/Debug/FrameCounter.h"
#include "Game/GameInfo.h"
#include "Game/DB/StatsTracker.h"
#include "Game/DB/SaveLoad.h"
#include "Game/Render/Wiper.h"
#include "Game/Render/depthoffield.h"
#include "Game/Render/FlareHandler.h"
#include "Game/AI/AIPad.h"
#include "Game/OverlayManager.h"
#include "Game/PadActions.h"
#include "Game/Pad/FlickDetection.h"
#include "Game/Replay.h"
#include "Game/ReplayManager.h"
#include "Game/ReplayChoreo.h"
#include "Game/NisPlayer.h"
#include "Game/Render/Presentation.h"
#include "Game/Render/CrowdManager.h"
#include "Game/ResetTask.h"
#include "NL/nlDebug.h"
#include "NL/nlTask.h"
#include "dolphin/os/OSThread.h"
#include "dolphin/si.h"
#include "dolphin/card.h"

class AudioUpdateTask : public nlTask
{
public:
    /**
     * Offset/Address/Size: 0x1BB4 | 0x801750C4 | size: 0x8
     */
    virtual const char* GetName() { return "Audio"; }
    /**
     * Offset/Address/Size: 0x1BBC | 0x801750CC | size: 0x20
     */
    virtual void Run(float dt)
    {
        Audio::Update(dt);
    }
};

class ClockUpdateTask : public nlTask
{
public:
    /**
     * Offset/Address/Size: 0x1BDC | 0x801750EC | size: 0x8
     */
    virtual const char* GetName() { return "Clock"; }
    /**
     * Offset/Address/Size: 0x1BE4 | 0x801750F4 | size: 0x20
     */
    virtual void Run(float dt)
    {
        ClockManager::Update(dt);
    }
};

#if defined(PORT_VITA)
extern "C" {
// Reserve a large newlib heap now that ATTRIBUTE2 grants the title the
// extended Vita user-memory budget. This is consumed by newlib malloc/free;
// the game's own 96 MiB memblock allocator remains independently bounded.
// Keep this at VitaSDK's normal 128 MiB: the application also reserves a
// separate 96 MiB USER_RW memblock for the GameCube VM/standard allocators.
// A 256 MiB newlib heap starves that memblock before main() even starts.
unsigned int _newlib_heap_size_user = 128u * 1024u * 1024u;
}

namespace
{
constexpr unsigned VitaBootWidth = 960;
constexpr unsigned VitaBootHeight = 544;
constexpr unsigned VitaBootPitch = 1024;

struct VitaBootUiState
{
    SceUID memblock = -1;
    unsigned int* pixels = NULL;
    bool active = false;
};

VitaBootUiState g_vitaBootUi;

static const unsigned char kBootFont[26][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // A
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // B
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // C
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, // D
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // E
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // F
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, // G
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // H
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // I
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // J
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // K
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // L
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // M
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, // N
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // O
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // P
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // Q
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // R
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, // S
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // T
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // U
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, // V
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, // W
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // X
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, // Y
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // Z
};

static unsigned VitaBootTextLength(const char* text)
{
    unsigned length = 0;
    while (text != NULL && text[length] != '\0')
        ++length;
    return length;
}

static void VitaBootRect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned int color)
{
    if (g_vitaBootUi.pixels == NULL)
        return;
    if (x >= VitaBootWidth || y >= VitaBootHeight)
        return;
    if (x + w > VitaBootWidth) w = VitaBootWidth - x;
    if (y + h > VitaBootHeight) h = VitaBootHeight - y;
    for (unsigned row = 0; row < h; ++row)
    {
        unsigned int* dst = g_vitaBootUi.pixels + (y + row) * VitaBootPitch + x;
        for (unsigned col = 0; col < w; ++col)
            dst[col] = color;
    }
}

static void VitaBootText(const char* text, unsigned x, unsigned y, unsigned scale, unsigned int color)
{
    if (text == NULL || scale == 0)
        return;
    for (unsigned n = 0; text[n] != '\0'; ++n)
    {
        char c = text[n];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z')
        {
            const unsigned char* rows = kBootFont[c - 'A'];
            for (unsigned row = 0; row < 7; ++row)
                for (unsigned col = 0; col < 5; ++col)
                    if ((rows[row] & (1u << (4u - col))) != 0)
                        VitaBootRect(x + col * scale, y + row * scale, scale, scale, color);
        }
        x += 6 * scale;
    }
}

static void VitaBootDraw(unsigned percent, const char* status)
{
    if (!g_vitaBootUi.active || g_vitaBootUi.pixels == NULL)
        return;
    if (percent > 100) percent = 100;

    VitaBootRect(0, 0, VitaBootWidth, VitaBootHeight, 0xFF101010u);
    VitaBootRect(150, 190, 660, 164, 0xFF202020u);

    const unsigned statusScale = 3;
    const unsigned statusWidth = VitaBootTextLength(status) * 6u * statusScale;
    const unsigned statusX = statusWidth < VitaBootWidth ? (VitaBootWidth - statusWidth) / 2u : 20u;
    VitaBootText(status, statusX, 225, statusScale, 0xFFFFFFFFu);

    VitaBootText("PLEASE WAIT", 363, 270, 2, 0xFFB0B0B0u);

    constexpr unsigned barX = 240;
    constexpr unsigned barY = 316;
    constexpr unsigned barW = 480;
    constexpr unsigned barH = 12;
    VitaBootRect(barX, barY, barW, barH, 0xFF505050u);
    VitaBootRect(barX + 2, barY + 2, ((barW - 4) * percent) / 100u, barH - 4, 0xFFE0E0E0u);
}

static bool VitaBootQueueFrame()
{
    if (!g_vitaBootUi.active || g_vitaBootUi.pixels == NULL)
        return false;
    SceDisplayFrameBuf fb = {};
    fb.size = sizeof(fb);
    fb.base = g_vitaBootUi.pixels;
    fb.pitch = VitaBootPitch;
    fb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    fb.width = VitaBootWidth;
    fb.height = VitaBootHeight;
    // CPU-backed Vita framebuffers are queued for the next scanout. Using the
    // immediate mode here is unreliable before GXM owns the display queue and
    // can leave the launch framebuffer visible for the entire prewarm.
    return sceDisplaySetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME) >= 0;
}

static bool VitaBootPresent()
{
    if (!VitaBootQueueFrame())
        return false;
    (void)sceDisplayWaitVblankStart();
    return true;
}

static bool VitaBootBegin()
{
    if (g_vitaBootUi.active)
        return true;
    const SceSize framebufferBytes = (SceSize)(VitaBootPitch * VitaBootHeight * sizeof(unsigned int));
    const SceSize bytes = (framebufferBytes + 0x3FFFFu) & ~0x3FFFFu;
    g_vitaBootUi.memblock = sceKernelAllocMemBlock(
        "strikersBootUi", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, bytes, NULL);
    if (g_vitaBootUi.memblock < 0)
        return false;
    void* base = NULL;
    if (sceKernelGetMemBlockBase(g_vitaBootUi.memblock, &base) < 0 || base == NULL)
    {
        sceKernelFreeMemBlock(g_vitaBootUi.memblock);
        g_vitaBootUi.memblock = -1;
        return false;
    }
    g_vitaBootUi.pixels = static_cast<unsigned int*>(base);
    g_vitaBootUi.active = true;
    VitaBootDraw(5, "PREPARING RENDERER");
    if (!VitaBootPresent())
    {
        sceKernelFreeMemBlock(g_vitaBootUi.memblock);
        g_vitaBootUi.memblock = -1;
        g_vitaBootUi.pixels = NULL;
        g_vitaBootUi.active = false;
        return false;
    }
    return true;
}

static void VitaBootProgress(const char* phase, size_t current, size_t total, void*)
{
    if (!g_vitaBootUi.active && !VitaBootBegin())
        return;
    unsigned percent = 15;
    const char* status = "PREPARING RENDERER";
    if (phase != NULL && strcmp(phase, "program_cache") == 0)
    {
        status = "LOADING SHADER CACHE";
        percent = current >= total ? 40u : 20u;
    }
    else if (phase != NULL && strcmp(phase, "pipeline_prewarm") == 0)
    {
        status = "PREPARING SHADERS";
        const size_t safeTotal = total != 0 ? total : 1;
        const size_t clamped = current < safeTotal ? current : safeTotal;
        percent = 40u + (unsigned)((clamped * 55u) / safeTotal);
    }
    static const char* s_lastStatus = NULL;
    static unsigned s_lastPercent = 0;
    if (status == s_lastStatus && percent < 100u && percent < s_lastPercent + 5u)
        return;
    s_lastStatus = status;
    s_lastPercent = percent;
    VitaBootDraw(percent, status);
    // sceGxmInitialize may reconfigure display ownership. Reassert the boot
    // framebuffer at each visible progress step without blocking on a VBlank.
    (void)VitaBootQueueFrame();
}

static void VitaBootReady()
{
    VitaBootDraw(100, "STARTING GAME");
    (void)VitaBootQueueFrame();
}

static void VitaBootRelease()
{
    if (g_vitaBootUi.pixels != NULL)
    {
        SceDisplayFrameBuf current = {};
        current.size = sizeof(current);
        if (sceDisplayGetFrameBuf(&current, SCE_DISPLAY_SETBUF_IMMEDIATE) >= 0
            && current.base == g_vitaBootUi.pixels)
        {
            (void)sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_SETBUF_IMMEDIATE);
        }
    }
    if (g_vitaBootUi.memblock >= 0)
        sceKernelFreeMemBlock(g_vitaBootUi.memblock);
    g_vitaBootUi.memblock = -1;
    g_vitaBootUi.pixels = NULL;
    g_vitaBootUi.active = false;
}
}

// One opt-in diagnostic snapshot, after the selected completed 3D frame. The
// display queue wait and disk write are intentionally not part of normal play.
static void VitaMaybeCaptureFrame()
{
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
    const bool nativeRenderer=port::directmatch::uses_own_renderer();
#else
    const bool nativeRenderer=false;
#endif
    static bool initialized = false;
    static unsigned long target = 0;
    static unsigned long matchTarget = 0;
    static unsigned long playTarget = 0;
    static float goalSnapshotX = 0;
    unsigned long selectedPlayFrame = 0;
    static unsigned long heavyFrames = 0;
    if (!initialized)
    {
        initialized = true;
        const char* value = getenv("STRIKERS_VITA_SNAPSHOT_3D_FRAME");
        if (value != NULL)
            target = strtoul(value, NULL, 10);
        const char* matchValue=getenv("STRIKERS_VITA_SNAPSHOT_MATCH_FRAME");
        if(matchValue!=NULL)matchTarget=strtoul(matchValue,NULL,10);
        const char* playValue=getenv("STRIKERS_VITA_SNAPSHOT_PLAY_FRAME");
        if(playValue!=NULL)playTarget=strtoul(playValue,NULL,10);
        const char* goalValue=getenv("STRIKERS_VITA_SNAPSHOT_GOAL_X");
        if(goalValue!=NULL&&PortDiagnosticsEnabled())goalSnapshotX=fabsf(strtof(goalValue,NULL));
    }
    if (target == 0 && matchTarget == 0 && playTarget == 0)
        return;
    unsigned long shotLabel=target;
    const bool playShot=playTarget!=0;
    if(playTarget || matchTarget) {
        PortBenchLive live;PortBenchGetLive(&live);
        if(playTarget) {
            if(!live.playActive||live.playFrames<playTarget)return;
            if(goalSnapshotX>0&&(!g_pBall||fabsf(g_pBall->m_v3Position.x)<goalSnapshotX))return;
            selectedPlayFrame=live.playFrames;
            shotLabel=playTarget;
        } else {
            if(!live.matchActive||live.frames<matchTarget)return;
            shotLabel=matchTarget;
        }
    } else if(!PortDiagnosticsEnabled())return;
#if defined(STRIKERS_VITA_GX_THREAD)
    // Async-GX builds need an explicit fence before reading renderer state.
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    if (!nativeRenderer) aurora::vita::wait_for_render_idle();
#endif
#endif
    if(!matchTarget && !playTarget) {
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
        if (!nativeRenderer && aurora::vita::telemetry().frame().counters.triangles < 10000)return;
#endif
        if (++heavyFrames != target)return;
    }
    // After the single shot, remove every subsequent fence/readback from play.
    target=0;matchTarget=0;playTarget=0;
    SceDisplayFrameBuf fb = {};
    fb.size = sizeof(fb);
    int result=0;
    std::vector<uint8_t> nativePixels;
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
    if (nativeRenderer) {
        if (!port::directmatch::readback_presented_rgba8(nativePixels)) result=-1;
        else {
            fb.base=nativePixels.data();fb.width=960;fb.height=544;fb.pitch=960;
            fb.pixelformat=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        }
    } else
#endif
    {
        sceGxmDisplayQueueFinish();
        result=sceDisplayGetFrameBuf(&fb,SCE_DISPLAY_SETBUF_IMMEDIATE);
    }
    if (result < 0 || fb.base == NULL || fb.width == 0 || fb.width > 4096
        || fb.height == 0 || fb.height > 4096 || fb.pitch < fb.width || fb.pixelformat != 0)
    {
        OSReport("[vita] snapshot failed: framebuffer rc=%d format=%u\n", result, fb.pixelformat);
        return;
    }
    unsigned char* row = (unsigned char*)malloc(fb.width * 3u);
    if (row == NULL)
        return;
    char path[128];
    snprintf(path, sizeof(path), "ux0:data/strikersVita/debug_frame_%s_%lu.ppm", playShot?"play":"3d",shotLabel);
    FILE* output = fopen(path, "wb");
    bool ok = output != NULL;
    if (output != NULL)
    {
        fprintf(output, "P6\n%u %u\n255\n", fb.width, fb.height);
        const unsigned char* pixels = (const unsigned char*)fb.base;
        for (unsigned int y = 0; y < fb.height && ok; ++y)
        {
            const unsigned char* source = pixels + y * fb.pitch * 4u;
            for (unsigned int x = 0; x < fb.width; ++x)
            {
                row[x*3u] = source[x*4u];
                row[x*3u+1u] = source[x*4u+1u];
                row[x*3u+2u] = source[x*4u+2u];
            }
            ok = fwrite(row, 3u, fb.width, output) == fb.width;
        }
        if (fclose(output) != 0)
            ok = false;
    }
    free(row);
    OSReport("[vita] snapshot 3d_frame=%lu renderer=%s ok=%d path=%s\n",
             heavyFrames,nativeRenderer?"strikers_native_gxm":"aurora",ok ? 1 : 0,path);
    if(goalSnapshotX>0&&g_pBall)
        OSReport("[vita] goal snapshot live_frame=%lu ball_x=%.3f threshold=%.3f\n",selectedPlayFrame,g_pBall->m_v3Position.x,goalSnapshotX);
}
#endif

bool g_bProfiling = false;
bool g_bTweaking = false;
bool g_e3_Build = false;
bool g_Europe = false;
bool g_bFranticPausing = false;
nlLocalization::nlLanguage g_Language = nlLocalization::LangEnglish;
LoadingManager* g_pTheLoadingManagerTask = nullptr;

FrameCounter g_FrameCounter("frame", "send");

static ComUpdateTask comUpdateTask;
static TransitionTask transitionTask;
static DispatchEventsTask dispatchEventsTask;
static PlatPadUpdateTask platPadUpdateTask;
static FrontEndTask frontEndTask;
static WorldUpdateTask worldUpdateTask;
static GameRenderTask gameRenderTask;
static ParticleUpdateTask particleUpdateTask;
static ClockUpdateTask clockUpdateTask;
static BeginFrameTask beginFrameTask;
static AudioUpdateTask audioUpdateTask;
static EndFrameTask endFrameTask;
static TweakerTask tweakerTask;
static FixedUpdateTask fixedUpdateTask;

static TestTask testTask;
static ResetTask resetTask;

static void Initialize();
static void SetupViews();
static void AddTasks();
static void PreInitFS();
static void DoMemCheck();

/**
 * Offset/Address/Size: 0x1BAC | 0x801750BC | size: 0x8
 */
const int* GetRegion()
{
    // PORT: read from the disc header at DVDInit rather than compiled in, one binary runs all three discs.
    static int g_Region = -1;
    g_Region = port_region();
    return &g_Region;
}

static void PreInitFS()
{
}

/**
 * Offset/Address/Size: 0x354 | 0x80173864 | size: 0x1858
 */
static void Initialize()
{
    nlRegHandleDVDMessageCB(Function<void(int)>(DisplayDVDMessageSebring));
    nlRegHandleDVDAllClearCB(Function<void(int)>(DVDAllClearSebring));
    nlRegCheckForResetFromFSCB(Function<FnVoidVoid>(
        Bind<void>(MemFun<ResetTask, void>(&ResetTask::FSCheckForReset), &resetTask)));

    nlInit();
    bool glStartupSuccessful = glStartup();
    if (!glStartupSuccessful)
    {
        nlBreak();
    }
    // PORT: STRIKERS_SEED replaces the clock, so that a headless run can be repeated; unset.
    {
        unsigned int fixedSeed;
        if (PortFixedSeed(&fixedSeed))
            nlSetRandomSeed(fixedSeed, &nlDefaultSeed);
        else
            nlSetRandomSeed(OSGetTick(), &nlDefaultSeed);
    }
    bool globalTexturesLoaded = glLoadTextureBundle("global.glt");
    if (!globalTexturesLoaded)
    {
        nlBreak();
    }

    Config::Global().LoadFromFile("common.ini");
    Config::Global().LoadFromFile("platform.ini");
    Config::Global().LoadFromFile("locale.ini");
    Config::Global().LoadFromFile("user.ini");

#if defined(PORT_VITA)
    // Opt-in device comparison through the game's existing skipfe/friendly
    // path. All AI, physics, replay, audio and rendering tasks remain active.
    // The config/seed identify a reproducible match without changing disc data.
    const char* deviceTestMatch=getenv("STRIKERS_VITA_TEST_MATCH");
    if(deviceTestMatch && strcmp(deviceTestMatch,"1")==0) {
        Config::Global().Set("skipfe",true);
        Config::Global().Set("dont_set_sides_when_skipfe",true);
    }
#endif

    {
        static const char* const kTeamVars[5][2] = {
            {"STRIKERS_TEAM1", "team1"}, {"STRIKERS_TEAM2", "team2"},
            {"STRIKERS_SIDEKICK1", "sidekick1"}, {"STRIKERS_SIDEKICK2", "sidekick2"},
            {"STRIKERS_STADIUM", "stadium"}};
        for (int i = 0; i < 5; ++i)
        {
            const char* v = getenv(kTeamVars[i][0]);
            if (v != NULL && *v != '\0')
                Config::Global().Set(kTeamVars[i][1], v);
        }
    }

    GetConfigBool(Config::Global(), "DiskAccess", false);
    if (GetConfigBool(Config::Global(), "e3_build", false))
    {
        g_e3_Build = true;
    }
    if (GetConfigBool(Config::Global(), "frantic_pausing", false))
    {
        g_bFranticPausing = true;
    }

    g_bFrameSmiler = GetConfigBool(Config::Global(), "Frame_Smiler", false);
    g_bFrameStatsOnScreen = GetConfigBool(Config::Global(), "Frame_Stats_On_Screen", false);
    g_bFrameStatsOnDisk = GetConfigBool(Config::Global(), "Frame_Stats_On_Disk", false);
    g_bRunSimAndRenderInLockStep = GetConfigBool(Config::Global(), "lockstep", false);
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    // Native gameplay always consumes elapsed time through the original
    // 50 Hz accumulator, independently of the number of presented images.
    g_bRunSimAndRenderInLockStep = false;
#endif

    // PORT: 64 bytes fits a console Event and its payload; a host Event is larger.
    {
        unsigned long uEventPool = 4096;
        const char* poolEnv = getenv("STRIKERS_EVENT_POOL");
        if (poolEnv != NULL && *poolEnv != '\0')
        {
            // Smaller as well as larger: shrinking the pool is how a burst that is too rare to catch at 128 gets caught at all.
            unsigned long v = strtoul(poolEnv, NULL, 10);
            if (v >= 16)
                uEventPool = v;
        }
        EventManager::Create(uEventPool, sizeof(Event) + 3 * (64 - 0x14));
    }
    g_pEventManager->AddEventHandler(FrontEnd::FEEventHandler, NULL, 2);
    g_pEventManager->AddEventHandler(OverlayManager::FEEventHandler, NULL, (u32)-1);
    g_pEventManager->AddEventHandler(FEAudioEventHandler, NULL, 2);
    g_pEventManager->AddEventHandler(Audio::AudioEventHandler, NULL, 0x16);

    nlTaskManager::Startup(0x10000);
    tDebugPrintManager::Initialize();
    ClockManager::Initialize();

    // PORT: the default is the game's, sound on; STRIKERS_AUDIO is checked after the config file so that it wins, because user.ini lives in the disc data.
    AudioLoader::gbDisableAudio = GetConfigBool(Config::Global(), "no_audio", false);
    {
        const char* portAudioEnv = getenv("STRIKERS_AUDIO");
        if (portAudioEnv != NULL && *portAudioEnv != '\0')
            AudioLoader::gbDisableAudio = (*portAudioEnv == '0');
    }
    AudioLoader::gbStream = !GetConfigBool(Config::Global(), "no_stream", false);
    AudioLoader::g_BGM_Off = GetConfigBool(Config::Global(), "no_bgm", false);
    AudioLoader::gbDisableCrowd = GetConfigBool(Config::Global(), "no_crowd", false);
    AudioLoader::gbDisableReverb = GetConfigBool(Config::Global(), "no_reverb", false);

    CrowdMood::ReadConfig();

    if (!AudioLoader::gbDisableAudio)
    {
        AudioLoader::Initialize();
        AudioLoader::SetupSoundBuffers();
        AudioLoader::LoadFEButtonSoundGroup();
        Audio::InitStreaming();
    }

    g_pEventManager->AddEventHandler(ReplayManager::EventHandler, ReplayManager::Instance(), (u32)-1);
    g_pEventManager->AddEventHandler(ReplayChoreo::EventHandler, &ReplayChoreo::Instance(), (u32)-1);
    g_pEventManager->AddEventHandler(NisPlayer::EventHandler, NisPlayer::Instance(), (u32)-1);
    g_pEventManager->AddEventHandler(Presentation::EventHandler, &Presentation::Instance(), (u32)-1);
    g_pEventManager->AddEventHandler(CrowdManager::EventHandler, &CrowdManager::instance, (u32)-1);

    glResourceMark();
    glInventory.Create();
    InitPads();
    SISetSamplingRate(0);
    PADSetSamplingCallback(VBlankPadUpdate);
    AIPadManager::Startup();
    FEInput::Initialize();
    FlickDetection::Initialize();
#if defined(PORT_USE_AURORA)
    // game code / maker code, read from the disc that is loaded
    CARDInit(port_disc_game_code(), port_disc_maker_code());
#else
    CARDInit();
#endif
    MemCard::s_InitDone = true;

    g_pTheLoadingManagerTask = new (nlMalloc(sizeof(LoadingManager), 8, false)) LoadingManager(0x14);

    if (!GetConfigBool(Config::Global(), "DisableComListener", false))
    {
        comUpdateTask.Initialize();
    }

    TransitionTask::sm_pGlobalTask = &transitionTask;
    transitionTask.Initialize(*g_pTheLoadingManagerTask);

    testTask.Initialize();
    nlLocalization::Initialize();

    DVDDiskID* diskid = DVDGetCurrentDiskID();
    if (diskid->gameName[0] == 'G' && diskid->gameName[1] == '4' && diskid->gameName[2] == 'Q' && diskid->gameName[3] == 'P')
    {
        switch (OSGetLanguage())
        {
        case 1:
            g_Language = nlLocalization::LangGerman;
            break;
        case 2:
            g_Language = nlLocalization::LangFrench;
            break;
        case 3:
            g_Language = nlLocalization::LangSpanish;
            break;
        case 4:
            g_Language = nlLocalization::LangItalian;
            break;
        default:
            g_Language = nlLocalization::LangUKEnglish;
            break;
        }
        g_Europe = true;
    }
    else if (diskid->gameName[0] == 'G' && diskid->gameName[1] == '4' && diskid->gameName[2] == 'Q' && diskid->gameName[3] == 'J')
    {
        // PORT: the Japanese disc carries every language's strings, fonts and menus, so `language` picks one; unset is Japanese.
        static const nlLocalization::nlLanguage kJapaneseDisc[] = {
            nlLocalization::LangEnglish, nlLocalization::LangGerman,  nlLocalization::LangFrench,
            nlLocalization::LangSpanish, nlLocalization::LangItalian, nlLocalization::LangJapanese,
        };
        const int language = port_language();
        g_Language = language >= 0 ? kJapaneseDisc[language] : nlLocalization::LangJapanese;
    }
    else if (diskid->gameName[0] == 'G' && diskid->gameName[1] == '4' && diskid->gameName[2] == 'Q' && diskid->gameName[3] == 'E')
    {
        g_Language = nlLocalization::LangEnglish;
        // PORT: the American disc has English only, so a `language` line is reported as ignored.
        if (port_language() >= 0)
            OSReport("[port] language: ignored, the American disc plays in English\n");
    }
    else
    {
        BasicString<char, Detail::TempStringAllocator> userlanguage = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >(
            "Language", BasicString<char, Detail::TempStringAllocator>("eng"));

        if (nlStrICmp(userlanguage.c_str(), "eng") == 0)
        {
            g_Language = nlLocalization::LangEnglish;
        }
        else if (nlStrICmp(userlanguage.c_str(), "jpn") == 0)
        {
            g_Language = nlLocalization::LangJapanese;
        }
        else if (nlStrICmp(userlanguage.c_str(), "deu") == 0)
        {
            g_Language = nlLocalization::LangGerman;
        }
        else if (nlStrICmp(userlanguage.c_str(), "fre") == 0)
        {
            g_Language = nlLocalization::LangFrench;
        }
        else if (nlStrICmp(userlanguage.c_str(), "ita") == 0)
        {
            g_Language = nlLocalization::LangItalian;
        }
        else if (nlStrICmp(userlanguage.c_str(), "spa") == 0)
        {
            g_Language = nlLocalization::LangSpanish;
        }
        else if (nlStrICmp(userlanguage.c_str(), "uke") == 0)
        {
            g_Language = nlLocalization::LangUKEnglish;
        }
        else if (nlStrICmp(userlanguage.c_str(), "longest") == 0)
        {
            g_Language = nlLocalization::LangLongestStrings;
        }
    }
    OSReport("[port] language: %s\n", nlLocalization::LanguageName[g_Language]);   // PORT: which table the disc and `language` chose

    LoadMemoryCardIconData();

    if (nlSingleton<GameInfoManager>::s_pInstance == NULL)
    {
        nlSingleton<GameInfoManager>::s_pInstance = new (nlMalloc(sizeof(GameInfoManager), 8, false)) GameInfoManager();
    }
    if (nlSingleton<StatsTracker>::s_pInstance == NULL)
    {
        nlSingleton<StatsTracker>::s_pInstance = new (nlMalloc(sizeof(StatsTracker), 8, false)) StatsTracker();
    }

    AddTasks();

    SetupViews();
    g_ShapeRenderer.Initialize();
    Wiper::Instance().Initialize();
    DepthOfFieldManager::instance.Initialize();
    FlareHandler::instance.Initialize();
    glAppStartup();

    BasicString<char, Detail::TempStringAllocator> skinString = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >("Skinning", BasicString<char, Detail::TempStringAllocator>("both"));
    BasicString<char, Detail::TempStringAllocator> replaySkin = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >("UserReplaySkinning", BasicString<char, Detail::TempStringAllocator>("blend"));
    if (skinString == "both")
    {
        BeginFrameTask::s_GameplaySkin = eModelSkin_Both;
    }
    else if (skinString == "rigid")
    {
        BeginFrameTask::s_GameplaySkin = eModelSkin_Rigid;
    }
    else
    {
        BeginFrameTask::s_GameplaySkin = eModelSkin_Blend;
    }
    if (replaySkin == "both")
    {
        BeginFrameTask::s_ReplaySkin = eModelSkin_Both;
    }
    else if (replaySkin == "rigid")
    {
        BeginFrameTask::s_ReplaySkin = eModelSkin_Rigid;
    }
    else
    {
        BeginFrameTask::s_ReplaySkin = eModelSkin_Blend;
    }
}

static void AddTasks()
{
    nlTaskManager::AddTask(&resetTask, 0, -1);
    nlTaskManager::AddTask(&beginFrameTask, 2, -1);
    nlTaskManager::AddTask(&dispatchEventsTask, 0x14, -1);
    nlTaskManager::AddTask(&platPadUpdateTask, 3, -1);
    nlTaskManager::AddTask(&clockUpdateTask, 5, -1);
    nlTaskManager::AddTask(&fixedUpdateTask, 7, -1);
    nlTaskManager::AddTask(&worldUpdateTask, 8, 0x20013);
    nlTaskManager::AddTask(&gameRenderTask, 0xa, 0x20113);
    nlTaskManager::AddTask(&frontEndTask, 0xc, 0x117);
    nlTaskManager::AddTask(&particleUpdateTask, 0xb, 0x20113);
    nlTaskManager::AddTask(&tweakerTask, 0xd, -1);
    nlTaskManager::AddTask(&audioUpdateTask, 0xe, -1);
    nlTaskManager::AddTask(&endFrameTask, 0xf, -1);
    nlTaskManager::AddTask(&transitionTask, 1, -1);
    nlTaskManager::AddTask(g_pTheLoadingManagerTask, 6, -1);

    if (!GetConfigBool(Config::Global(), "DisableComListener", false))
    {
        nlTaskManager::AddTask(&comUpdateTask, 0x10, -1);
    }
    if (GetConfigBool(Config::Global(), "test/enable", false))
    {
        nlTaskManager::AddTask(&testTask, 0x12, -1);
    }
}

/**
 * Offset/Address/Size: 0x224 | 0x80173734 | size: 0x130
 */
static void SetupViews()
{
    static eGLView sort_none[] = {
        GLV_Shadow0, GLV_Shadow1, GLV_UnsortedPerspective, GLV_InvisiblePlane, GLV_ElectricFence, GLV_UnsortedOrtho, GLV_ShadowBlend0, GLV_ShadowBlend1, GLV_Debug, GLV_Transitions, GLV_CoPlanar0, GLV_CoPlanar
    };

    static eGLView disabled_views[] = {
        GLV_ShadowBlend0, GLV_ShadowBlend1, GLV_ScreenBlur, GLV_ScreenBlur2
    };

    for (int iview = 0; iview < GLV_Num; iview++)
    {
        glViewSetTarget((eGLView)iview, GLTG_Main);
    }

    glViewSetSortMode(GLV_FrontEnd, GLVSort_TransformedDepth);
    glViewSetSortMode(GLV_Anark, GLVSort_Reverse);

    for (int iview = 0; iview < sizeof(sort_none) / sizeof(eGLView); iview++)
    {
        glViewSetSortMode(sort_none[iview], GLVSort_None);
    }

    for (int iview = 0; iview < sizeof(disabled_views) / sizeof(eGLView); iview++)
    {
        glViewSetEnable(disabled_views[iview], false);
    }

    u32 uWarbleTexture = glGetTexture("target/warble");
    bool bWarbleLoaded = glTextureLoad(uWarbleTexture);
    if (!bWarbleLoaded)
    {
        glViewSetEnable(GLV_Warble, false);
        glViewSetEnable(GLV_WarbleBlend, false);
    }

    glViewSetDepthClear(GLV_CameraSpace, true);
    glViewSetDepthClear(GLV_Transitions, true);
    glViewSetDepthClear(GLV_Transitions3D, true);
    glViewSetDepthClear(GLV_Anark3D_BG, true);
    glViewSetDepthClear(GLV_Anark3D_FG, true);

    ParticleSystem::ClearViews();
    ParticleSystem::AddView(GLV_Particles);

    ModeledScreenTransition::s_3DView = GLV_Transitions3D;
}

static void DoMemCheck()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x80173510 | size: 0x224
 */
#if defined(PORT_USE_AURORA)
// Set by main() so the frame loop can end cleanly when the window is closed.
static bool s_portRunning = true;
static const char* s_portExitReason = NULL;

// src/platform/input.cpp, default keyboard mapping for port 0.
extern "C" void PortInstallKeyboardBindings(void);
extern "C" void PortUpdateSyntheticInput(unsigned long frame);
// src/platform/aurora_compat.c, stands in for the VI retrace interrupt that drove controller sampling on console.
extern "C" void PortInvokePadSamplingCallback(void);

static unsigned long s_portFrame = 0;

#if defined(PORT_VITA)
enum VitaShaderProfile
{
    VITA_SHADER_CONTROL = 0,
    VITA_SHADER_WARM,
    VITA_SHADER_SEALED,
};

#ifndef STRIKERS_VITA_SHADER_PROFILE_DEFAULT
#define STRIKERS_VITA_SHADER_PROFILE_DEFAULT "SEALED"
#endif

#ifndef STRIKERS_VITA_STATIC_GEOMETRY_MB_DEFAULT
#define STRIKERS_VITA_STATIC_GEOMETRY_MB_DEFAULT 8
#endif

static VitaShaderProfile s_vitaShaderProfile = VITA_SHADER_SEALED;
static bool s_vitaShaderGameplayActive = false;

static bool PortAsciiEqualIgnoreCase(const char* a, const char* b)
{
    if (a == NULL || b == NULL)
        return false;
    while (*a != '\0' && *b != '\0')
    {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb)
            return false;
    }
    return *a == '\0' && *b == '\0';
}

static VitaShaderProfile PortVitaReadShaderProfile()
{
    const char* value = getenv("STRIKERS_GXM_SHADER_PROFILE");
    if (value == NULL || *value == '\0')
        value = STRIKERS_VITA_SHADER_PROFILE_DEFAULT;

    if (PortAsciiEqualIgnoreCase(value, "CONTROL"))
        return VITA_SHADER_CONTROL;
    if (PortAsciiEqualIgnoreCase(value, "WARM"))
        return VITA_SHADER_WARM;
    return VITA_SHADER_SEALED;
}


#define PORT_VITA_GXM_COUNTER(snapshot, field) \
    ([](const auto& s) -> unsigned int { \
        if constexpr (requires { s.field; }) return static_cast<unsigned int>(s.field); \
        else return 0u; \
    }(snapshot))

static void PortVitaSampleRendererStats()
{
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
    if (port::directmatch::uses_own_renderer()) return;
#endif
    if (!PortDiagnosticsEnabled())
        return;
    const aurora::vita::PerformanceSnapshot perf = aurora::vita::completed_performance_snapshot();
    static uint64_t s_lastNativePipelineUs = 0;
    static uint64_t s_lastNativeTextureUs = 0;
    static uint64_t s_lastNativeDrawUs = 0;
    static uint64_t s_lastNativeEfbEndSceneUs = 0;
    static uint64_t s_lastNativeEfbTransferSubmitUs = 0;
    static uint64_t s_lastNativeEfbTransferWaitUs = 0;
    static uint64_t s_lastNativeEfbCpuFixupUs = 0;
    if (perf.nativeTimingsSampled)
    {
        s_lastNativePipelineUs = perf.nativePipelineUs;
        s_lastNativeTextureUs = perf.nativeTextureUs;
        s_lastNativeDrawUs = perf.nativeDrawUs;
        s_lastNativeEfbEndSceneUs = perf.nativeEfbEndSceneUs;
        s_lastNativeEfbTransferSubmitUs = perf.nativeEfbTransferSubmitUs;
        s_lastNativeEfbTransferWaitUs = perf.nativeEfbTransferWaitUs;
        s_lastNativeEfbCpuFixupUs = perf.nativeEfbCpuFixupUs;
    }
    PortBenchRendererStats stats = {};
    stats.valid = 1;
    stats.shaderRuntimeCompilationEnabled = perf.shaderRuntimeCompilationEnabled ? 1 : 0;
    stats.shaderRuntimeCompiles = perf.shaderRuntimeCompiles;
    stats.shaderRuntimeCompileUs = perf.shaderRuntimeCompileUs;
    stats.shaderCompileBlockedMisses = perf.shaderCompileBlockedMisses;
    stats.shaderDiskCacheHits = perf.shaderDiskCacheHits;
    stats.shaderDiskCacheMisses = perf.shaderDiskCacheMisses;
    stats.frameUs = perf.frameUs;
    stats.rendererCpuFrameUs = perf.rendererCpuFrameUs;
    stats.displayQueueLastUs = perf.displayQueueLastUs;
    stats.displayQueueMaxUs = perf.displayQueueMaxUs;
    stats.displayQueueAverageUs = perf.displayQueueAverageUs;
    stats.displayQueueSamples = perf.displayQueueSamples;
    stats.displayQueueBlockedPercent = perf.displayQueueBlockedPercent;
    stats.gpuBackpressureLikely = perf.gpuBackpressureLikely ? 1 : 0;
    stats.nativeTimingsSampled = perf.nativeTimingsSampled ? 1 : 0;
    stats.nativePipelineUs = s_lastNativePipelineUs;
    stats.nativeTextureUs = s_lastNativeTextureUs;
    stats.nativeDrawUs = s_lastNativeDrawUs;
    stats.nativeSceneCount = perf.nativeSceneCount;
    stats.nativeEfbCopies = perf.nativeEfbCopies;
    stats.frameIndex = perf.frameIndex;
    // gxm-optimization counters are absent from older Aurora revisions; the
    // testbed builds the A/B baseline from the same Strikers source.
    stats.nativeDepthLoadScenes = PORT_VITA_GXM_COUNTER(perf, nativeDepthLoadScenes);
    stats.nativeDepthStoreScenes = PORT_VITA_GXM_COUNTER(perf, nativeDepthStoreScenes);
    stats.nativeDepthlessScenes = PORT_VITA_GXM_COUNTER(perf, nativeDepthlessScenes);
    stats.nativeFinishCalls = PORT_VITA_GXM_COUNTER(perf, nativeFinishCalls);
    stats.nativeScissorFreeDraws = PORT_VITA_GXM_COUNTER(perf, nativeScissorFreeDraws);
    stats.nativeEfbEndSceneUs = s_lastNativeEfbEndSceneUs;
    stats.nativeEfbTransferSubmitUs = s_lastNativeEfbTransferSubmitUs;
    stats.nativeEfbTransferWaitUs = s_lastNativeEfbTransferWaitUs;
    stats.nativeEfbCpuFixupUs = s_lastNativeEfbCpuFixupUs;
    stats.staticGeometryHits = perf.staticGeometryHits;
    stats.staticGeometryMisses = perf.staticGeometryMisses;
    stats.staticGeometryLookupFallbacks = perf.staticGeometryLookupFallbacks;
    stats.staticGeometryBytes = perf.staticGeometryBytes;
    stats.staticGeometryEntries = perf.staticGeometryEntries;
    PortBenchSetRendererStats(&stats);
}

void PortVitaShaderCacheBeginLoading()
{
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
    if (port::directmatch::uses_own_renderer()) return;
#endif
    if (s_vitaShaderProfile != VITA_SHADER_SEALED)
        return;
    if (!aurora::vita::runtime_shader_compilation_enabled())
        aurora::vita::set_runtime_shader_compilation_enabled(true);
}

void PortVitaShaderCacheEndLoading(bool enteringGameplay)
{
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
    if (port::directmatch::uses_own_renderer()) return;
#endif
    if (s_vitaShaderProfile == VITA_SHADER_CONTROL)
        return;

    // The stable Aurora backend performs manifest/program-cache prewarm during
    // renderer initialization. It has no post-loading prewarm entry point.
    (void)enteringGameplay;

    if (s_vitaShaderProfile == VITA_SHADER_SEALED)
        aurora::vita::set_runtime_shader_compilation_enabled(false);
}

void PortVitaShaderCacheEnterGameplay()
{
    if (s_vitaShaderGameplayActive)
        return;

    PortVitaSampleRendererStats();
    PortBenchRendererGameplayStart();
    s_vitaShaderGameplayActive = true;
}

void PortVitaShaderCacheLeaveGameplay()
{
    if (!s_vitaShaderGameplayActive)
        return;

    // Snapshot before reopening the compiler so the gameplay delta cannot be
    // polluted by front-end variants compiled immediately after the transition.
    PortVitaSampleRendererStats();
    PortBenchRendererGameplayEnd();
    s_vitaShaderGameplayActive = false;
}
#endif

// Numeric environment overrides for the graphics configuration.
static unsigned long PortEnvU32(const char* name, unsigned long fallback)
{
    const char* v = getenv(name);
    if (v == NULL || *v == '\0')
        return fallback;
    return strtoul(v, NULL, 10);
}

static float PortEnvFloat(const char* name, float fallback)
{
    const char* v = getenv(name);
    if (v == NULL || *v == '\0')
        return fallback;
    return (float)atof(v);
}

// STRIKERS_CAPTURE writes a frame as a binary PPM; Aurora records the readback on the frame's own command encoder.

static void PortRequestManualShot()
{
#if defined(PORT_VITA)
    return;
#else
    static unsigned long s_shotIndex;
    static char s_shotPath[1024];

    const char* dir = getenv("STRIKERS_SHOT_DIR");
    // PORT: /tmp is not a directory on Windows.
#if defined(_WIN32)
    if (dir == NULL || *dir == '\0')
        dir = getenv("TEMP");
    if (dir == NULL || *dir == '\0')
        dir = ".";
#else
    if (dir == NULL || *dir == '\0')
        dir = "/tmp";
#endif

    snprintf(s_shotPath, sizeof s_shotPath, "%s/strikers-shot-%03lu.ppm",
             dir, s_shotIndex++);
    OSReport("[shot] frame %lu -> %s\n", s_portFrame, s_shotPath);
    aurora_capture_frame(s_shotPath);
#endif
}

// PORT: keep the picture the shape of the window; idempotent, because everything below the generation test is skipped unless the shape actually moved.
static void PortFollowWindowShape()
{
#if defined(PORT_VITA)
    const unsigned int gen = PortAspectGeneration();
    PortFollowRenderScale(544);
    PortSetWindowAspect(960, 544);
    if (PortAspectGeneration() != gen)
        PortApplyAspectChange();
#else
    const AuroraWindowSize ws = aurora_window_size();
    const unsigned int gen = PortAspectGeneration();

    // PORT: the automatic render scale follows the window height.
    PortFollowRenderScale(ws.height);
    PortSetWindowAspect(ws.width, ws.height);
    if (PortAspectGeneration() == gen)
        return;

    PortApplyAspectChange();
    aurora_apply_frame_buffer_resize();
#endif
}

#if !defined(PORT_VITA)
static SDL_Window* s_portWindow;
#endif

static void PortFollowDisplayRefresh()
{
#if defined(PORT_VITA)
    PortSetDisplayRefresh(60.0, 1);
#else
    if (s_portWindow == NULL)
        return;
    const SDL_DisplayMode* mode =
        SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(s_portWindow));
    PortSetDisplayRefresh(mode != NULL ? (double)mode->refresh_rate : 0.0,
                          aurora_present_waits_for_vblank() ? 1 : 0);
#endif
}

static void PortPumpHostEvents()
{
#if defined(PORT_VITA)
    SDL_PumpEvents();
    PortFollowWindowShape();
    SceCtrlData pad = {};
    static unsigned int s_exitComboFrames;
    if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0)
    {
        const bool exitCombo =
            (pad.buttons & (SCE_CTRL_START | SCE_CTRL_SELECT))
                == (SCE_CTRL_START | SCE_CTRL_SELECT);
        if (exitCombo)
        {
            // PORT: START+SELECT used to quit on a single poll.  During FE
            // bring-up that is far too easy to trigger while testing buttons,
            // and looks exactly like an unexplained clean process exit.  Require
            // roughly two seconds at 60 Hz and name the reason in the log.
            if (++s_exitComboFrames >= 120 && s_portRunning)
            {
                s_portExitReason = "START+SELECT held for 2 seconds";
                OSReport("[port] exit requested: %s\n", s_portExitReason);
                s_portRunning = false;
            }
        }
        else
        {
            s_exitComboFrames = 0;
        }
    }
#else
    // PORT: polled rather than waited for; the event below is only the fast path.
    PortFollowWindowShape();

    for (const AuroraEvent* ev = aurora_update(); ev && ev->type != AURORA_NONE; ++ev)
    {
        if (ev->type == AURORA_EXIT)
        {
            s_portExitReason = "window close";
            s_portRunning = false;
        }

        // PORT: the picture follows the window rather than the shape the window had at startup, so maximising it does not put black bars down the sides.
        if (ev->type == AURORA_WINDOW_RESIZED
            || ev->type == AURORA_DISPLAY_SCALE_CHANGED)
        {
            PortFollowWindowShape();
        }

        if (ev->type == AURORA_SDL_EVENT
            && ev->sdl.type == SDL_EVENT_KEY_DOWN
            && !ev->sdl.key.repeat
            && (ev->sdl.key.scancode == SDL_SCANCODE_F12
                || ev->sdl.key.scancode == SDL_SCANCODE_P))
        {
            PortRequestManualShot();
        }

        // PORT: every key goes to the overlay, which owns the hotkeys, F1 for the menu among them.
        if (ev->type == AURORA_SDL_EVENT
            && (ev->sdl.type == SDL_EVENT_KEY_DOWN || ev->sdl.type == SDL_EVENT_KEY_UP)
            && !ev->sdl.key.repeat)
        {
            PortOverlayHandleKey((int)ev->sdl.key.scancode,
                                 ev->sdl.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
        }

        if (ev->type == AURORA_SDL_EVENT)
            PortPromptsEvent(&ev->sdl); // PORT: button prompts

        // PORT: the limiter follows the display the window is on, which can move or change mode.
        if (ev->type == AURORA_SDL_EVENT
            && (ev->sdl.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED
                || ev->sdl.type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED))
        {
            PortFollowDisplayRefresh();
        }
    }
#endif
}
#endif

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
static bool s_portRunning=true;
static const char* s_portExitReason=nullptr;
static unsigned long s_portFrame=0;
static void PortPumpHostEvents() {s_portRunning=port::native_runtime::pump();}
#endif

int main(int argc, char* argv[])
{
#if defined(PORT_VITA)
    // The master INI gate must be resolved before any runtime file is opened.
    (void)PortConfigLoad();
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    port::directmatch::enable(true);
#else
    if(const char* setting=getenv("STRIKERS_GXM_NATIVE_MATCH_DIRECT"))
        port::directmatch::enable(strcmp(setting,"1")==0);
#endif
#endif
#if defined(STRIKERS_VITA_GXM_DIRECT_PROBE)
    // Self-contained GXM-only probe. Run before audio, the game engine or any
    // Aurora GX initialization so zero GX commands are issued in this mode.
    if (const char* direct = getenv("STRIKERS_GXM_DIRECT_PROBE");
        direct != nullptr && (strcmp(direct, "1") == 0 || strcmp(direct, "2") == 0))
        return port::run_native_gxm_direct_probe(unsigned(direct[0] - '0'));
#endif
#if !defined(STRIKERS_VITA_NO_LOGS)
    // Keep the Vita diagnostics on the memory card. stderr is deliberately
    // unbuffered so the last useful line survives a crash or forced exit.
    char vitaLogDir[64];
    (void)port_executable_dir(vitaLogDir, sizeof vitaLogDir);
    if (PortDiagnosticsEnabled() && getenv("STRIKERS_LOG") == NULL
        && freopen("ux0:data/strikersVita/runtime.log", "w", stderr) != NULL)
    {
        setvbuf(stderr, NULL, _IONBF, 0);
        fprintf(stderr, "[vita] runtime log started\n");
    }
#endif
    PortNativeAudioInitialize();
#endif

    // PORT: strikers.ini -> environment, before anything reads one.
    {
        const int applied = PortConfigLoad();
        if (applied > 0)
            OSReport("[port] %s: %d setting(s)\n",
                     PortConfigPath(), applied);
#if defined(PORT_VITA)
        // strikers.ini has no other visible effect on Vita; report exactly what
        // the loader saw so a silent setenv/fopen failure cannot hide.
        if (PortDiagnosticsEnabled())
        {
            FILE* probe = fopen("ux0:data/strikersVita/strikers.ini", "rb");
            const int probeOpen = probe != NULL;
            if (probe != NULL)
                fclose(probe);
            const int setRc = setenv("STRIKERS_CONFIG_PROBE", "1", 1);
            const char* probeValue = getenv("STRIKERS_CONFIG_PROBE");
            OSReport("[port] config path=%s applied=%d fopen=%d setenv_rc=%d probe=%s\n",
                     PortConfigPath() != NULL ? PortConfigPath() : "(none)", applied, probeOpen, setRc,
                     probeValue != NULL ? probeValue : "(null)");
        }
#endif
    }
    PortBenchInit();
#if defined(PORT_VITA)
    // Register only the shaders used by actual gameplay draws when opted in.
    // With the menu disabled this adds no per-draw counters to the GXM path.
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    aurora_vita_shader_debug_capture(PortVitaDebugMenuEnabled());
#endif
#endif

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    if(!port::native_runtime::initialize())return 1;
    port::initialize_native_assets();
    PortSetWindowAspect(960,544);
    PortSetDisplayRefresh(60.0,1);
    glxVitaReserveResourceArena();
#endif

#if defined(PORT_USE_AURORA)
#if defined(PORT_VITA)
    {
        sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
        // Keep the producer/game thread on core 0. With async GX enabled, core
        // 2 is the sole GX/GXM owner while core 1 is the helper core used by
        // Aurora's single CPU worker and the low-duty audio thread.
        (void)sceKernelChangeThreadCpuAffinityMask(
            sceKernelGetThreadId(), SCE_KERNEL_CPU_MASK_USER_0);
        OSReport("[vita] clocks at boot: cpu=%d bus=%d gpu=%d xbar=%d MHz\n",
                 scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(),
                 scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency());
        const char* cpuMHz = getenv("STRIKERS_CPU_MHZ");
        if (cpuMHz != NULL)
        {
            char* end=NULL;const long requested=strtol(cpuMHz,&end,10);
            if(end && *cpuMHz && !*end && (requested==111||requested==166||requested==222||
                requested==333||requested==444||requested==500)) {
                const int result=scePowerSetArmClockFrequency((int)requested);
                OSReport("[vita] requested cpu=%ld MHz rc=%d actual=%d\n",requested,result,scePowerGetArmClockFrequency());
                // Stock kernels can reject 500; the previous implementation
                // silently left the default 500 INI at 333. Keep a usable
                // documented fallback and report the real hardware value.
                if(requested==500 && (result<0||scePowerGetArmClockFrequency()!=500)) {
                    const int fallback=scePowerSetArmClockFrequency(444);
                    OSReport("[vita] cpu fallback=444 MHz rc=%d actual=%d\n",fallback,scePowerGetArmClockFrequency());
                }
            } else OSReport("[vita] invalid cpu_mhz=%s; actual=%d\n",cpuMHz,scePowerGetArmClockFrequency());
        }
        const char* gpuMHz = getenv("STRIKERS_GPU_MHZ");
        if (gpuMHz != NULL && strcmp(gpuMHz, "222") == 0)
        {
            const int gpuResult = scePowerSetGpuClockFrequency(222);
            const int busResult = scePowerSetBusClockFrequency(222);
            const int xbarResult = scePowerSetGpuXbarClockFrequency(166);
            OSReport("[vita] requested gpu=222 bus=222 xbar=166 rc=%d/%d/%d reported=%d/%d/%d\n",
                     gpuResult, busResult, xbarResult,
                     scePowerGetGpuClockFrequency(), scePowerGetBusClockFrequency(),
                     scePowerGetGpuXbarClockFrequency());
        }
        SceKernelFreeMemorySizeInfo memInfo = {};
        memInfo.size = sizeof(memInfo);
        if (sceKernelGetFreeMemorySize(&memInfo) >= 0)
        {
            OSReport("[vita] pre-Aurora free memory: user=%u cdram=%u phycont=%u\n",
                     (unsigned int)memInfo.size_user,
                     (unsigned int)memInfo.size_cdram,
                     (unsigned int)memInfo.size_phycont);
        }
        if (!glxVitaReserveResourceArena())
            OSReport("[vita] warning: could not pre-reserve the full GLX resource arena\n");
        if (sceKernelGetFreeMemorySize(&memInfo) >= 0)
        {
            OSReport("[vita] post-GLX-reserve free memory: user=%u cdram=%u phycont=%u\n",
                     (unsigned int)memInfo.size_user,
                     (unsigned int)memInfo.size_cdram,
                     (unsigned int)memInfo.size_phycont);
        }
        aurora::vita::BackendConfig cfg = {};
#if defined(STRIKERS_VITA_GC_NATIVE_RES)
        // Experimental performance mode: retain the Vita 960x544 scanout while
        // rasterizing the GameCube scene at its conventional 640x480 XFB size.
        // Aurora scales the final image to the display extent.
        cfg.render_width = 640;
        cfg.render_height = 480;
#endif
        unsigned int renderWidth = 0, renderHeight = 0;
        const int renderOverride = PortConfigVitaRenderSize(&renderWidth, &renderHeight);
        if (renderOverride > 0)
        {
            cfg.render_width = renderWidth;
            cfg.render_height = renderHeight;
        }
        else if (renderOverride < 0)
        {
            OSReport("[vita] unsupported gxm_internal_resolution=%s; using build default\n",
                     getenv("STRIKERS_GXM_INTERNAL_RESOLUTION"));
        }
        const char* distinctCores = getenv("STRIKERS_AURORA_DISTINCT_CPU_CORES");
        if (distinctCores != NULL)
            cfg.cpu_distinct_core_dispatch = distinctCores[0] == '1';
        const char* immediateView = getenv("STRIKERS_GXM_IMMEDIATE_DRAW_VIEW");
        if (immediateView != NULL)
            cfg.gxm_immediate_draw_view = immediateView[0] == '1';
        OSReport("[vita] native candidate distinct_cores=%u immediate_draw_view=%u\n",
                 cfg.cpu_distinct_core_dispatch ? 1u : 0u,
                 cfg.gxm_immediate_draw_view ? 1u : 0u);
        cfg.vgl_legacy_pool_size = 0;
        // Do not use vglInitExtended's threshold mode here: with zero CDRAM
        // and PHYCONT thresholds it turns almost every currently-free page into
        // a vitaGL heap. Strikers already owns a long-lived GLX texture arena,
        // so give the renderer explicit bounded pools and preserve OS/newlib /
        // shader-compiler headroom.
        if (sceKernelGetFreeMemorySize(&memInfo) >= 0)
        {
            const u32 mb = 1024u * 1024u;
            // The circular pool and dynamic Aurora streaming buffers are CPU
            // mapped, so vitaGL allocates them from RAM before PHYCONT/CDRAM.
            // A 4 MiB RAM pool forced most streaming pages into scarce CDRAM,
            // directly competing with the gameplay texture cache.
            cfg.vgl_ram_pool_size = memInfo.size_user > 24u * mb
                ? ((memInfo.size_user - 24u * mb < 20u * mb) ? memInfo.size_user - 24u * mb : 20u * mb)
                : 0;
            cfg.vgl_cdram_pool_size = memInfo.size_cdram > 16u * mb
                ? ((memInfo.size_cdram - 16u * mb < 40u * mb) ? memInfo.size_cdram - 16u * mb : 40u * mb)
                : 0;
            const char* cdramPoolMb = getenv("STRIKERS_VGL_CDRAM_MB");
            if (cdramPoolMb != NULL)
            {
                const unsigned long value = strtoul(cdramPoolMb, NULL, 10);
                if (value >= 32 && value <= 96 && memInfo.size_cdram >= (value + 16u) * mb)
                    cfg.vgl_cdram_pool_size = (unsigned int)value * mb;
            }
            cfg.vgl_phycont_pool_size = memInfo.size_phycont > 8u * mb
                ? ((memInfo.size_phycont - 8u * mb < 8u * mb) ? memInfo.size_phycont - 8u * mb : 8u * mb)
                : 0;
            OSReport("[vita] vitaGL fixed pools: ram=%u KB cdram=%u KB phycont=%u KB\n",
                     cfg.vgl_ram_pool_size >> 10,
                     cfg.vgl_cdram_pool_size >> 10,
                     cfg.vgl_phycont_pool_size >> 10);
        }
        cfg.vgl_circular_pool_size = 12 * 1024 * 1024;
        cfg.vgl_display_buffer_count = 3;
        // Aurora's multi-buffered VBO/IBO pages are long-lived dynamic buffers.
        // Keep them out of vitaGL's circular scratch pool: that same pool stages
        // compressed texture transfers, and a large-frame rollover must not
        // consume the memory needed by the next CMPR/DXT1 upload.
        cfg.vgl_scratch_dynamic = false;
        cfg.vgl_scratch_stream = true;
        // The gameplay scene exceeds 16 MiB of resident GX textures in a
        // single frame. Aurora's cache cannot evict textures already referenced
        // by queued draws, so the smaller budget turns character/stadium
        // textures into fallbacks even though the vitaGL CDRAM pool still has
        // room. Match Aurora-Vita's normal 24 MiB cache budget.
        cfg.texture_cache_budget = 26 * 1024 * 1024;
        const char* textureCacheMb = getenv("STRIKERS_TEXTURE_CACHE_MB");
        if (textureCacheMb != NULL)
        {
            const unsigned long value = strtoul(textureCacheMb, NULL, 10);
            if (value >= 16 && value <= 64)
                cfg.texture_cache_budget = (unsigned int)value * 1024u * 1024u;
        }
        // The arena now rolls over safely inside a frame; use Aurora's normal
        // page size so stadium/crowd batches amortize buffer orphaning while
        // keeping peak transient storage bounded.
        cfg.stream_vertex_bytes = 8 * 1024 * 1024;
        cfg.stream_index_bytes = 512 * 1024;
        cfg.stream_slots = 3;
        // GX/GXM retains a single owner. CPU3 only executes independent
        // preparation chunks through Aurora's quota-aware scheduler; OFF
        // retains the validated three-core helper topology.
        enum VitaCore3Mode { VITA_CORE3_OFF, VITA_CORE3_AUTO, VITA_CORE3_ON };
        VitaCore3Mode core3Mode = VITA_CORE3_AUTO;
        const char* core3 = getenv("STRIKERS_VITA_CORE3");
        if (core3 != NULL)
        {
            if (strcmp(core3, "0") == 0 || strcmp(core3, "off") == 0)
                core3Mode = VITA_CORE3_OFF;
            else if (strcmp(core3, "1") == 0 || strcmp(core3, "on") == 0)
                core3Mode = VITA_CORE3_ON;
            else if (strcmp(core3, "auto") == 0)
                core3Mode = VITA_CORE3_AUTO;
        }
        cfg.cpu_worker_threads = core3Mode == VITA_CORE3_OFF ? 2u : 3u;
        cfg.cpu_core3_budget_enabled = core3Mode != VITA_CORE3_OFF;
        // Renderer helpers retain three lanes. Vertex decode/transform/pack
        // opts into CPU3 internally; game pose jobs use it only with the
        // separate INI experiment below and the same budget enforcement.
        cfg.cpu_renderer_execution_lanes = 3;
        const char* gameCore3=getenv("STRIKERS_VITA_GAME_CORE3");
        cfg.cpu_game_execution_lanes = gameCore3 && strcmp(gameCore3,"1")==0 &&
            cfg.cpu_core3_budget_enabled ? 4u : 3u;
        const char* preparedLists=getenv("STRIKERS_GXM_PREPARED_DL");
        cfg.prepared_display_lists=preparedLists && strcmp(preparedLists,"1")==0;
        OSReport("[vita] gxm_prepared_dl=%u\n", cfg.prepared_display_lists?1u:0u);
        const char* cdramGeometry=getenv("STRIKERS_GXM_RESIDENT_CDRAM");
        cfg.resident_geometry_cdram=cdramGeometry && strcmp(cdramGeometry,"1")==0;
        const char* bc1=getenv("STRIKERS_GXM_EXACT_BC1");
        cfg.exact_bc1=bc1 && strcmp(bc1,"1")==0;
        const char* nativeAssets=getenv("STRIKERS_GXM_NATIVE_ASSETS");
        if(nativeAssets && strcmp(nativeAssets,"1")==0) {
            port::initialize_native_assets();cfg.native_asset_reader=port::read_native_asset;
        }
        {
            const char* packed = getenv("STRIKERS_GXM_NATIVE_GPU_STATIC");
            cfg.gxm_native_gpu_static = packed && strcmp(packed,"1")==0;
            OSReport("[vita] gxm_native_gpu_static=%u\n", cfg.gxm_native_gpu_static?1u:0u);
        }
        cfg.cpu_parallel_min_vertices = 64;
        const char* workerCount = getenv("STRIKERS_AURORA_CPU_WORKERS");
        if (workerCount != NULL && workerCount[0] >= '0' && workerCount[0] <= '3' && workerCount[1] == '\0')
        {
            cfg.cpu_worker_threads = (unsigned int)(workerCount[0] - '0');
            if (core3Mode == VITA_CORE3_OFF && cfg.cpu_worker_threads > 2)
                cfg.cpu_worker_threads = 2;
        }
        const auto readCore3Unsigned = [](const char* name, unsigned int fallback,
                                          unsigned int minValue, unsigned int maxValue) {
            const char* text = getenv(name);
            if (text == NULL || text[0] == '\0')
                return fallback;
            char* end = NULL;
            const unsigned long value = strtoul(text, &end, 10);
            if (end == text || *end != '\0' || value < minValue || value > maxValue)
                return fallback;
            return (unsigned int)value;
        };
        cfg.cpu_core3_max_total_percent = readCore3Unsigned(
            "STRIKERS_VITA_CORE3_MAX_TOTAL_PCT", 70, 1, 100);
        cfg.cpu_core3_guard_percent = readCore3Unsigned(
            "STRIKERS_VITA_CORE3_GUARD_PCT", 5, 0, cfg.cpu_core3_max_total_percent);
        cfg.cpu_core3_window_ms = readCore3Unsigned(
            "STRIKERS_VITA_CORE3_WINDOW_MS", 100, 10, 1000);
        cfg.cpu_core3_long_window_ms = 1000;
        if (cfg.cpu_core3_long_window_ms < cfg.cpu_core3_window_ms)
            cfg.cpu_core3_long_window_ms = cfg.cpu_core3_window_ms;
        cfg.cpu_core3_chunk_target_us = readCore3Unsigned(
            "STRIKERS_VITA_CORE3_CHUNK_TARGET_US", 250, 50, 2000);
        cfg.cpu_core3_sample_period_us = readCore3Unsigned(
            "STRIKERS_VITA_CORE3_SAMPLE_US", 10000, 1000, 50000);
        OSReport("[vita] cpu helpers requested=%u renderer_lanes=%u game_lanes=%u core3_mode=%s budget=%u max_total=%u guard=%u window_ms=%u chunk_us=%u sample_us=%u\n",
                 (unsigned int)cfg.cpu_worker_threads,
                 (unsigned int)cfg.cpu_renderer_execution_lanes,
                 (unsigned int)cfg.cpu_game_execution_lanes,
                 core3Mode == VITA_CORE3_OFF ? "off" : (core3Mode == VITA_CORE3_ON ? "on" : "auto"),
                 cfg.cpu_core3_budget_enabled ? 1u : 0u,
                 (unsigned int)cfg.cpu_core3_max_total_percent,
                 (unsigned int)cfg.cpu_core3_guard_percent,
                 (unsigned int)cfg.cpu_core3_window_ms,
                 (unsigned int)cfg.cpu_core3_chunk_target_us,
                 (unsigned int)cfg.cpu_core3_sample_period_us);
        const char* parallelMin = getenv("STRIKERS_AURORA_PARALLEL_MIN");
        if (parallelMin != NULL)
        {
            const unsigned long value = strtoul(parallelMin, NULL, 10);
            if (value >= 64 && value <= 65536)
                cfg.cpu_parallel_min_vertices = (unsigned int)value;
        }
        cfg.wait_vblank = true;
#if defined(STRIKERS_VITA_FORCE_60HZ)
        // NEXTFRAME remains tear-free, but waiting in the display callback can
        // quantize a frame that barely misses vblank straight down to 30 Hz.
        // Pace the producer in VI instead; the callback itself stays nonblocking.
        cfg.wait_vblank = false;
        float displayHz = 60.0f;
        if (sceDisplayGetRefreshRate(&displayHz) < 0 || displayHz < 50.0f || displayHz > 70.0f)
            displayHz = 60.0f;
        PortSetDisplayRefresh((double)displayHz, 0);
        PortSetFrameLimit((double)displayHz);
#else
        const char* vitaVsync = getenv("STRIKERS_VITA_VSYNC");
        if (vitaVsync != NULL)
            cfg.wait_vblank = vitaVsync[0] != '0';
#endif
        OSReport("[vita] GXM callback vblank wait=%u (NEXTFRAME present always enabled)\n",
                 cfg.wait_vblank ? 1u : 0u);
        // Keep lightweight timing telemetry enabled in normal builds, but do
        // not pay for per-draw coverage/trace/geometry diagnostics unless a
        // developer explicitly requests them in strikers.ini/environment.
        const char* auroraDiagnostics = getenv("STRIKERS_AURORA_DIAGNOSTICS");
        const bool fullAuroraDiagnostics = auroraDiagnostics != NULL
            && auroraDiagnostics[0] != '\0' && auroraDiagnostics[0] != '0';
        const char* perfCapture=getenv("STRIKERS_PERFORMANCE_CAPTURE");
        cfg.diagnostics = fullAuroraDiagnostics;
        cfg.performance_attribution = perfCapture && *perfCapture && PortDiagnosticsEnabled();
        cfg.diagnostics_enabled = PortDiagnosticsEnabled() != 0;
        const char* splitVertexPhases = getenv("STRIKERS_PROFILE_VERTEX_PHASES");
        cfg.profile_split_vertex_phases = splitVertexPhases != NULL && splitVertexPhases[0] == '1';
        const char* textureDiagnostics = getenv("STRIKERS_VITA_TEXTURE_DIAGNOSTICS");
        cfg.texture_decode_diagnostics = textureDiagnostics != NULL && textureDiagnostics[0] == '1';
#if defined(AURORA_VITA_RENDERER_GXM)
        // Keep all persistent Aurora data under Strikers' own ux0:data folder.
        // Do not rely on TITLE_ID-derived paths: homebrew data roots are chosen
        // by the application and must survive rebuilds/title metadata changes.
        cfg.data_root_path = "ux0:data/strikersVita/aurora-vita";

        // D16 halves native depth storage/bandwidth versus the conservative
        // higher-precision path. Keep a runtime escape hatch for hardware A/B
        // checks on stages that expose precision-sensitive depth ranges.
        cfg.gxm_d16_depth = true;
        const char* gxmD16 = getenv("STRIKERS_GXM_D16");
        if (gxmD16 != NULL)
            cfg.gxm_d16_depth = gxmD16[0] != '0';
        // Preserve the hardware-proven Strikers baseline. Larger parameter
        // buffers remain available through STRIKERS_GXM_PARAMETER_MB for
        // targeted experiments, but are not the default.
        cfg.gxm_parameter_buffer_bytes = 4u * 1024u * 1024u;
        const char* parameterMb = getenv("STRIKERS_GXM_PARAMETER_MB");
        if (parameterMb != NULL)
        {
            const unsigned long value = strtoul(parameterMb, NULL, 10);
            if (value >= 1 && value <= 32)
                cfg.gxm_parameter_buffer_bytes = (size_t)value * 1024u * 1024u;
        }
        // Native GXM can keep immutable object-space GX geometry resident and
        // perform fixed PN/texgen work in its vertex shader. The separated GX
        // bring-up must start from the CPU vertex path: the experimental static
        // geometry/fixed-vertex path is the current freeze suspect and Aurora's
        // own regression audit recommends budget=0 for the control profile.
        {
        cfg.static_geometry_budget =
            (unsigned int)STRIKERS_VITA_STATIC_GEOMETRY_MB_DEFAULT * 1024u * 1024u;
        // Strikers submits a large amount of dynamic character/shadow geometry.
        // Do not spend CPU hashing those transient GX streams just to discover
        // they are unsuitable for persistent reuse; only display-list-backed
        // sources enter the fixed-vertex cache.
        cfg.static_geometry_stable_only = true;
        }
        cfg.static_geometry_min_vertices = 16;
        {
            // CDRAM display-list shadows (Aurora gxm-optimization); gxm_dl_shadow=0 disables.
            const char* dlShadow = getenv("STRIKERS_GXM_DL_SHADOW");
            const bool enableDlShadow = dlShadow == NULL || dlShadow[0] != '0';
            [&](auto& config) {
                if constexpr (requires { config.display_list_shadow; })
                    config.display_list_shadow = enableDlShadow;
            }(cfg);
            (void)enableDlShadow;
        }
        {
            // Producer BP material cache stays opt-in until device A/B passes.
            const char* bpCache = getenv("STRIKERS_GXM_BP_CACHE");
            const bool enableBpCache = bpCache != NULL && bpCache[0] == '1';
            [&](auto& config) {
                if constexpr (requires { config.bp_write_cache; })
                    config.bp_write_cache = enableBpCache;
            }(cfg);
            OSReport("[vita] gxm_bp_cache=%u\n", enableBpCache ? 1u : 0u);
        }
        {
            // Isolated A2a candidate; keep disabled until reference vs candidate
            // draw/output and hardware frame-time comparisons pass.
            const char* value = getenv("STRIKERS_GXM_XF_EQUAL_POS_WRITES");
            const bool enabled = value && strcmp(value, "1") == 0;
            cfg.gxm_xf_equal_pos_writes = enabled;
            OSReport("[vita] gxm_xf_equal_pos_writes=%u\n", enabled ? 1u : 0u);
        }
        {
            // A5 per-frame state invalidation: independently test exact XF
            // matrix writes and inactive/equivalent TEV writes. Neither
            // bypasses any draw, skinning matrix update, depth or shadow pass.
            const char* matrices=getenv("STRIKERS_GXM_XF_EQUAL_MATRIX_WRITES");
            cfg.gxm_xf_equal_matrix_writes=matrices && strcmp(matrices,"1")==0;
            const char* tev=getenv("STRIKERS_GXM_TEV_DECODED_WRITE_GATE");
            cfg.gxm_tev_decoded_write_gate=tev && strcmp(tev,"1")==0;
            OSReport("[vita] gxm_xf_equal_matrix_writes=%u gxm_tev_decoded_write_gate=%u\n",
                cfg.gxm_xf_equal_matrix_writes?1u:0u,cfg.gxm_tev_decoded_write_gate?1u:0u);
        }
        {
            const char* value=getenv("STRIKERS_GXM_RASTER_UNIFORM_REUSE");
            cfg.gxm_raster_uniform_reuse=value && strcmp(value,"1")==0;
            OSReport("[vita] gxm_raster_uniform_reuse=%u\n",cfg.gxm_raster_uniform_reuse?1u:0u);
        }
        {
            const char* value=getenv("STRIKERS_GXM_NATIVE_PN_PALETTE");
            cfg.gxm_native_pn_palette=value && strcmp(value,"1")==0;
            OSReport("[vita] gxm_native_pn_palette=%u scope=characters_pointlit\n",cfg.gxm_native_pn_palette?1u:0u);
        }
        {
            const char* prepareCache = getenv("STRIKERS_GXM_FRAGMENT_PREPARE_CACHE");
            const bool enablePrepareCache = prepareCache != NULL && prepareCache[0] == '1';
            [&](auto& config) {
                if constexpr (requires { config.gxm_fragment_prepare_cache; })
                    config.gxm_fragment_prepare_cache = enablePrepareCache;
            }(cfg);
            OSReport("[vita] gxm_fragment_prepare_cache=%u\n", enablePrepareCache ? 1u : 0u);
        }
        {
            // A3 indexed-PN experiment: restore a complete, CPU-owned GXM
            // vertex default-uniform image and update only changed parameters.
            // Hardware A/B must precede any decision to enable by default.
            const char* value = getenv("STRIKERS_GXM_UNIFORM_DELTA_UPLOAD");
            cfg.gxm_uniform_delta_upload = value && strcmp(value, "1") == 0;
            OSReport("[vita] gxm_uniform_delta_upload=%u\n", cfg.gxm_uniform_delta_upload ? 1u : 0u);
        }
        {
            const char* value=getenv("STRIKERS_GXM_VERTEX_REUSE_PROBE");
            cfg.gxm_vertex_reuse_probe=value && strcmp(value,"1")==0;
            OSReport("[vita] gxm_vertex_reuse_probe=%u sample_period=16\n",
                cfg.gxm_vertex_reuse_probe?1u:0u);
        }
        {
            // A4 optimizes Cg generation, not GX draw/filter state. The two
            // independent transformations can be compared one at a time.
            const char* value = getenv("STRIKERS_GXM_A4_FRAGMENT_OPT");
            char* end = NULL;
            const unsigned long mask = value ? strtoul(value, &end, 0) : 0ul;
            cfg.gxm_a4_fragment_opt = value && end != value && *end == '\0' && mask <= 3ul
                ? static_cast<uint8_t>(mask) : 0u;
            OSReport("[vita] gxm_a4_fragment_opt=%u\n", unsigned(cfg.gxm_a4_fragment_opt));
        }
        {
            const char* uniformPool = getenv("STRIKERS_GXM_FIXED_UNIFORM_POOL");
            const bool enableUniformPool = uniformPool != NULL && uniformPool[0] == '1';
            [&](auto& config) {
                if constexpr (requires { config.gxm_fixed_uniform_pool; })
                    config.gxm_fixed_uniform_pool = enableUniformPool;
            }(cfg);
            OSReport("[vita] gxm_fixed_uniform_pool=%u\n", enableUniformPool ? 1u : 0u);
        }
        {
            const char* geometryPreflight = getenv("STRIKERS_GXM_GEOMETRY_PREFLIGHT");
            const bool enableGeometryPreflight = geometryPreflight != NULL && geometryPreflight[0] == '1';
            [&](auto& config) {
                if constexpr (requires { config.gxm_geometry_preflight; })
                    config.gxm_geometry_preflight = enableGeometryPreflight;
            }(cfg);
            OSReport("[vita] gxm_geometry_preflight=%u\n", enableGeometryPreflight ? 1u : 0u);
        }
        {
            // Aurora's adjacent-draw merge stays opt-in for device comparisons.
            const char* localBatching = getenv("STRIKERS_GXM_LOCAL_DRAW_BATCHING");
            const bool enableLocalBatching = localBatching != NULL && localBatching[0] == '1';
            cfg.gxm_local_draw_batching = enableLocalBatching;
            OSReport("[vita] gxm_local_draw_batching=%u\n", enableLocalBatching ? 1u : 0u);
        }
        {
            // Bisection mask for the Aurora gxm-optimization changes (strikers.ini gxm_disable).
            const char* disable = getenv("STRIKERS_GXM_DISABLE");
            const unsigned long mask = disable != NULL ? strtoul(disable, NULL, 0) : 0ul;
            [&](auto& config) {
                if constexpr (requires { config.gxm_disable_mask; })
                    config.gxm_disable_mask = (uint32_t)mask;
            }(cfg);
            OSReport("[vita] gxm_disable_mask=0x%lx\n", mask);
        }
        const char* fixedMin = getenv("STRIKERS_GXM_FIXED_MIN");
        if (fixedMin != NULL)
        {
            const unsigned long value = strtoul(fixedMin, NULL, 10);
            if (value >= 3 && value <= 256)
                cfg.static_geometry_min_vertices = (unsigned int)value;
        }
        s_vitaShaderProfile = PortVitaReadShaderProfile();
        // Reuse the persistent GXP cache introduced by the upstream preload
        // path, while keeping CONTROL as a true no-preload baseline.
        cfg.gxm_preload_program_cache = s_vitaShaderProfile != VITA_SHADER_CONTROL;
        cfg.gxm_program_cache_preload_limit = 1024;
        // Stable Aurora performs this prewarm during renderer initialization.
        cfg.pipeline_prewarm_limit = s_vitaShaderProfile == VITA_SHADER_CONTROL ? 0 : 64;
        // Strikers seals manually at the loading -> gameplay boundary. Menus,
        // stage loading and training runs must retain the ability to discover
        // and persist new variants.
        cfg.gxm_seal_shader_cache_after_prewarm = false;
        cfg.startup_progress = VitaBootProgress;
        cfg.startup_progress_user = NULL;
        // The native shader now reproduces GX channel lighting and COLOR0/COLOR1
        // texgen semantics. Keep an environment escape hatch for immediate A/B
        // validation against the graphics-proven CPU vertex path.
#if 0 // The GX worker keeps the same renderer configuration as the synchronous path.
        cfg.gxm_lit_fixed_vertex_gpu = false;
        cfg.gxm_streamed_fixed_vertex_gpu = false;
        cfg.gxm_dynamic_tex_matrix_gpu = false;
        cfg.gxm_bump_fixed_vertex_gpu = false;
        cfg.gxm_primitive_expand_gpu = false;
#else
        cfg.gxm_lit_fixed_vertex_gpu = true;
        // Keep the hardware-proven CPU vertex path as the boot default. The
        // Vita quick menu can enable the streamed fixed-vertex experiment live
        // for A/B tests without requiring another build.
        cfg.gxm_streamed_fixed_vertex_gpu = false;
        // Extended persistent-GX path modelled after the successful Melee Vita
        // renderer: keep matrix selection, bump basis and stable point/line
        // expansion on GXM whenever Aurora can prove the draw is cache-safe.
        // Keep per-vertex GX texture-matrix selection on the proven CPU path
        // for Strikers for now. Hardware testing showed a mirrored + vertically
        // flipped Health & Safety FE image with this experimental GPU path.
        // Aurora retains the implementation and STRIKERS_GXM_TEXMTX_GPU=1 can
        // still opt back in for focused A/B validation.
        cfg.gxm_dynamic_tex_matrix_gpu = false;
        cfg.gxm_bump_fixed_vertex_gpu = true;
        cfg.gxm_primitive_expand_gpu = true;
#endif
        const char* litGpu = getenv("STRIKERS_GXM_LIT_GPU");
        if (litGpu != NULL)
            cfg.gxm_lit_fixed_vertex_gpu = litGpu[0] == '1';
        const char* streamedGpu = getenv("STRIKERS_GXM_STREAMED_VERTEX_GPU");
        if (streamedGpu != NULL)
            cfg.gxm_streamed_fixed_vertex_gpu = streamedGpu[0] == '1';
        const char* texMtxGpu = getenv("STRIKERS_GXM_TEXMTX_GPU");
        if (texMtxGpu != NULL)
            cfg.gxm_dynamic_tex_matrix_gpu = texMtxGpu[0] == '1';
        const char* bumpGpu = getenv("STRIKERS_GXM_BUMP_GPU");
        if (bumpGpu != NULL)
            cfg.gxm_bump_fixed_vertex_gpu = bumpGpu[0] == '1';
        const char* primitiveGpu = getenv("STRIKERS_GXM_PRIM_GPU");
        if (primitiveGpu != NULL)
            cfg.gxm_primitive_expand_gpu = primitiveGpu[0] == '1';
#endif
        const char* staticGeometryMb = getenv("STRIKERS_STATIC_GEOMETRY_MB");
        if (staticGeometryMb != NULL)
        {
            const unsigned long value = strtoul(staticGeometryMb, NULL, 10);
            if (value <= 32)
                cfg.static_geometry_budget = (unsigned int)value * 1024u * 1024u;
        }
        const char* drawLimit = getenv("STRIKERS_VITA_DRAW_LIMIT");
        if (drawLimit != NULL)
            cfg.diagnostic_draw_limit = (unsigned int)strtoul(drawLimit, NULL, 10);
        OSReport("[vita] static geometry budget=%u KB gpu_fixed_vertex=%d streamed_gpu=%d lit_gpu=%d texmtx_gpu=%d bump_gpu=%d prim_gpu=%d split_vertex_phases=%d\n",
                 (unsigned int)(cfg.static_geometry_budget >> 10), cfg.static_geometry_budget != 0,
                 cfg.gxm_streamed_fixed_vertex_gpu ? 1 : 0,
                 cfg.gxm_lit_fixed_vertex_gpu ? 1 : 0,
                 cfg.gxm_dynamic_tex_matrix_gpu ? 1 : 0,
                 cfg.gxm_bump_fixed_vertex_gpu ? 1 : 0,
                 cfg.gxm_primitive_expand_gpu ? 1 : 0,
                 cfg.profile_split_vertex_phases ? 1 : 0);
        if (cfg.diagnostic_draw_limit != 0)
            OSReport("[vita] diagnostic draw limit=%u\n", (unsigned int)cfg.diagnostic_draw_limit);
        cfg.strict_unsupported = false;
        cfg.diagnostics_period_frames = cfg.performance_attribution ? 24000 : 10;
#if defined(STRIKERS_VITA_NO_LOGS)
        // Use Aurora Vita's native runtime logging switch rather than compiling
        // logging code out. This keeps renderer behavior identical while making
        // the performance build silent and prevents diagnostic files being opened.
        cfg.log_level = aurora::vita::RuntimeLogLevel::Silent;
        cfg.diagnostics = false;
        cfg.profile_split_vertex_phases = false;
        cfg.texture_decode_diagnostics = false;
        // diagnostic_draw_limit stays as set by strikers.ini (vita_draw_limit, default 0).
        cfg.telemetry_log_path = NULL;
        cfg.coverage_log_path = NULL;
        cfg.trace_log_path = NULL;
#else
        cfg.telemetry_log_path = fullAuroraDiagnostics ? "ux0:data/strikersVita/aurora_telemetry.log" : NULL;
        cfg.coverage_log_path = fullAuroraDiagnostics ? "ux0:data/strikersVita/aurora_coverage.log" : NULL;
        cfg.trace_log_path = fullAuroraDiagnostics ? "ux0:data/strikersVita/aurora_trace.log" : NULL;
#endif
        if (!PortDiagnosticsEnabled())
        {
            cfg.log_level = aurora::vita::RuntimeLogLevel::Silent;
            cfg.diagnostics = false;
            cfg.profile_split_vertex_phases = false;
            cfg.texture_decode_diagnostics = false;
            cfg.telemetry_log_path = NULL;
            cfg.coverage_log_path = NULL;
            cfg.trace_log_path = NULL;
        }
        const char* vita3dDiagnostics = getenv("STRIKERS_VITA_3D_DIAGNOSTICS");
        const bool verboseVita3d = vita3dDiagnostics != NULL
            && vita3dDiagnostics[0] != '\0' && vita3dDiagnostics[0] != '0';
        OSReport("[vita] 3D perf profile=cpu-color-v3 stream_v=%uKB stream_i=%uKB parallel_min=%u aurora_diag=%u vita3d_diag=%u\n",
                 (unsigned int)(cfg.stream_vertex_bytes >> 10),
                 (unsigned int)(cfg.stream_index_bytes >> 10),
                 (unsigned int)cfg.cpu_parallel_min_vertices,
                 fullAuroraDiagnostics ? 1u : 0u,
                 verboseVita3d ? 1u : 0u);
        // Create the temporary scanout before Aurora reserves its CDRAM pool.
        // WARM/SEALED profiles perform synchronous program-cache preload and
        // pipeline prewarm inside initialize(), so users otherwise see an
        // apparently frozen black screen during that work.
        if (s_vitaShaderProfile != VITA_SHADER_CONTROL && !VitaBootBegin())
            OSReport("[vita] warning: prewarm loading UI allocation failed\n");
        if (!aurora::vita::initialize(cfg))
        {
            OSReport("[vita] Aurora backend init failed: %u %s\n",
                     (unsigned int)aurora::vita::last_init_failure(),
                     aurora::vita::last_init_failure_detail());
            VitaBootRelease();
            return 1;
        }
        VitaBootReady();
        // Boot starts inside a loading window. TransitionTask closes it after
        // the first front-end state is fully materialized.
        aurora::vita::set_runtime_shader_compilation_enabled(true);
        PortVitaSampleRendererStats();
        PortSetWindowAspect(960, 544);
        PortSetDisplayRefresh(60.0, 1);
        VILockAspectRatio((int)(PortTargetAspect() * 10000.0f), 10000);
    }
#else
    {
        AuroraConfig cfg = {};
        cfg.appName = "Super Mario Strikers";
        // PORT: STRIKERS_BACKEND picks the graphics backend. Unset takes Aurora's own preference order.
        AuroraBackend wantBackend = BACKEND_AUTO;
#if defined(_WIN32)
        wantBackend = BACKEND_VULKAN;
        cfg.desiredBackend = wantBackend;
        int backendWasAsked = 0;
#else
        int backendWasAsked = 0;
#endif
        {
            const char* want = getenv("STRIKERS_BACKEND");
            if (want != NULL && *want != '\0')
            {
                backendWasAsked = 1;
                static const struct { const char* name; AuroraBackend id; } kBackends[] = {
                    { "auto", BACKEND_AUTO },         { "d3d11", BACKEND_D3D11 },
                    { "d3d12", BACKEND_D3D12 },       { "metal", BACKEND_METAL },
                    { "vulkan", BACKEND_VULKAN },     { "opengl", BACKEND_OPENGL },
                    { "opengles", BACKEND_OPENGLES }, { "webgpu", BACKEND_WEBGPU },
                    { "null", BACKEND_NULL },
                };
                const size_t nBackends = sizeof(kBackends) / sizeof(kBackends[0]);
                size_t bi = 0;
                for (; bi < nBackends; bi++)
                {
                    // Local, because this is two lines and pulling in a case-insensitive compare from NL for it is not worth the template instantiation.
                    const char* a = want;
                    const char* b = kBackends[bi].name;
                    while (*a != '\0' && *b != '\0')
                    {
                        char ca = *a;
                        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
                        if (ca != *b) break;
                        a++; b++;
                    }
                    if (*a == '\0' && *b == '\0')
                    {
                        wantBackend = kBackends[bi].id;
                        cfg.desiredBackend = wantBackend;
                        break;
                    }
                }
                if (bi == nBackends)
                {
                    OSReport("[port] STRIKERS_BACKEND=%s is not a backend name; "
                             "expected one of auto d3d11 d3d12 metal vulkan "
                             "opengl opengles webgpu null\n", want);
                    return 1;
                }
            }
        }
        // 1080p, 16:9: the logical framebuffer follows the display aspect, so a 16:9 window is filled rather than pillarboxed.
        cfg.windowWidth = 1920;
        cfg.windowHeight = 1080;
        cfg.vsync = PortEnvU32("STRIKERS_VSYNC", 0) != 0;

        // STRIKERS_MSAA sets the sample count and can only be chosen here. 1 by default: 4x costs an Intel N100 half its frame rate.
        cfg.msaa = (uint32_t)PortEnvU32("STRIKERS_MSAA", 1);
        // The port widened pointers rather than emulating the console memory map.
        cfg.mem1Size = 0;
        cfg.mem2Size = 0;
        // PORT: aurora_initialize reads startFullscreen, the window icon and the cache directory once, so they are chosen here.
        PortAuroraConfigure(&cfg);
        AuroraInfo info = aurora_initialize(argc, argv, &cfg);

        // STRIKERS_ASPECT=auto takes the shape of the window.
        PortSetWindowAspect(info.windowSize.width, info.windowSize.height);

        // PORT: cap to the display's refresh rate rather than to a constant.
        {
            const SDL_DisplayMode* mode =
                SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(info.window));
            // 0 for "unknown", refresh_rate is documented as 0.0f when the mode does not report one.
            // PORT: with vsync off, a surface that cannot skip the wait still waits for the display's refresh.
            PortSetDisplayRefresh(mode != NULL ? (double)mode->refresh_rate : 0.0,
                                  aurora_present_waits_for_vblank() ? 1 : 0);
            // PORT: kept for the event pump, which re-derives the rate when the display changes.
            s_portWindow = info.window;
        }

        // PORT: Aurora falls back to its preference order when the requested backend will not start.
        if (wantBackend != BACKEND_AUTO && info.backend != wantBackend)
        {
            if (backendWasAsked)
            {
                OSReport("[port] WARNING: asked for backend %d, got %d; Aurora "
                         "fell back. Any measurement from this run is of the "
                         "backend it got.\n",
                         (int)wantBackend, (int)info.backend);
            }
            else
            {
                OSReport("[port] Vulkan did not start; fell back to backend %d. "
                         "Expect roughly half the frame rate, but it runs.\n",
                         (int)info.backend);
            }
        }

        // Render above the console's framebuffer size.
        {
            float scale = (float)info.windowSize.height / 448.0f;
            if (scale > 3.0f)
                scale = 3.0f;
            if (scale < 1.0f)
                scale = 1.0f;
            aurora_set_frame_buffer_scale(PortEnvFloat("STRIKERS_RES_SCALE", scale));
        }

        // Hold the render target at the VI mode's aspect.
        VILockAspectRatio((int)(PortTargetAspect() * 10000.0f), 10000);
        AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);

        PortTexturesInit(info.userPath); // PORT: texture packs

        // Aurora's PAD reads SDL gamepads and reports PAD_ERR_NO_CONTROLLER when there is neither a gamepad nor a keyboard binding.
        PortInstallKeyboardBindings();

        PortDiscordInit();
    }
#endif
#else
    (void)argc;
    (void)argv;
#endif

    if (g_DoStackWatermarkTests)
    {
        OSClearStack(g_StackWatermarkFiller);
    }

    Initialize();
#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
    // Startup owns the native context and shaders before any game draw.
    if (!port::directmatch::preload())
    {
        OSReport("[vita] native match preload failed; see gxm_native_match_status.csv\n");
        PortAudioStop();
        PortNativeAudioShutdown();
        port::directmatch::shutdown();
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
        aurora::vita::shutdown();
#endif
        port::shutdown_native_assets();
        VitaBootRelease();
        return 1;
    }
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    // Direct-to-match startup has no previously displayed front-end frame.
    // Keep the game's localized loading bitmap on screen while invalid
    // resource passes and the first complete scene are being prepared.
    DisplayLoadingMessageFast();
#endif
#endif
    PortProfilerStart();

    fopen("flushfile.txt", "r");

    nlTaskManager::SetNextState(GetConfigBool(Config::Global(), "skipfe", false) ? 2 : 4);

    if (GetConfigBool(Config::Global(), "enableFloatingPointExceptions", false))
    {
        InstallFloatingPointExceptionHandler();
    }

    if (GetConfigBool(Config::Global(), "callStackDumper", false))
    {
        InstallCallStackDumper();
    }

#if defined(PORT_USE_AURORA) || defined(STRIKERS_VITA_NATIVE_RUNTIME)
#if !defined(PORT_VITA)
    PortShaderStageBegin();   // PORT: logs the pipeline queue the boot memory card screen waits for
#endif

    while (s_portRunning && !PortQuitRequested())
    {
        // PORT: the deferred limiter sleep goes before the event pump so the frame reads input after it.
        PortLimiterFlush();
        PortPumpHostEvents();
        PortDebugFrame();
#if defined(PORT_VITA)
        PortBenchFrameBegin();
        PortProfilerFrameMark();
        bool ownNativeRenderer = false;
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
        ownNativeRenderer = port::directmatch::uses_own_renderer();
#endif
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
        if (!ownNativeRenderer && !aurora::vita::begin_frame())
            continue;
#endif              // minimised or surface lost; nothing to draw
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
        if (!port::directmatch::begin_frame())
        {
            s_portExitReason = "native GXM begin_frame failed";
            break;
        }
#endif
#else
        // PORT: ReturnToFE leaves the pause flag set, so it is a pause only while a match exists.
        PortDiscordUpdate(FrontEnd::m_bInPauseMenuState && g_pGame != NULL ? 1 : 0);

        // PORT: timed, since the swapchain acquire blocks inside aurora_begin_frame under vsync.
        const unsigned long long acquireStart = port_monotonic_ns();
        if (!aurora_begin_frame())
            continue;              // minimised or surface lost; nothing to draw
        const unsigned long long acquireNs = port_monotonic_ns() - acquireStart;
        PortBenchFrameBegin();
        PortBenchAddAcquire(acquireNs);

        // PORT: SDL's pad state only changes in a pump and the acquire blocks under vsync, so pump again after it.
        {
            static int s_latePump = -1;
            if (s_latePump < 0)
            {
                const char* e = getenv("STRIKERS_LATE_PUMP");
                s_latePump = (e != NULL && *e == '0') ? 0 : 1;
            }
            if (s_latePump)
            {
                SDL_PumpEvents();
            }
        }

        PortPromptsFrame(); // PORT: button prompts
#endif

        // Sample the pad before the tasks that read it. main() registers VBlankPadUpdate through PADSetSamplingCallback.
        PortInvokePadSamplingCallback();

        PortProfilerTasksBegin();
        nlTaskManager::RunAllTasks();
        PortProfilerTasksEnd();
        PortBenchSetPlayActive(g_pGame != NULL && g_pGame->IsGameplayOrOvertime());
        if (PortDiagnosticsEnabled())
            UpdateProfile();
        PortBenchAfterTasks();

        // PORT: the audio clock. MusyX runs only inside this call; see include/port/audio.h.
        PortAudioUpdate();
#if defined(PORT_VITA)
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
        if (!port::directmatch::end_frame())
        {
            s_portExitReason = "native GXM end_frame failed";
            break;
        }
#endif
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
        if (!ownNativeRenderer) aurora::vita::end_frame();
#endif
        if (g_vitaBootUi.active)
        {
            // The first renderer present has now replaced the temporary CPU
            // framebuffer. Finish that one queue entry before releasing CDRAM.
            sceGxmDisplayQueueFinish();
            VitaBootRelease();
        }
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
        if (!ownNativeRenderer && PortDiagnosticsEnabled())
        {
            const aurora::vita::PerformanceSnapshot perf = aurora::vita::completed_performance_snapshot();
            PortProfilerRendererSample sample = {};
            sample.rendererCpuFrameUs = perf.rendererCpuFrameUs;
            sample.displayQueueLastUs = perf.displayQueueLastUs;
            sample.nativePipelineUs = perf.nativePipelineUs;
            sample.nativeTextureUs = perf.nativeTextureUs;
            sample.nativeDrawUs = perf.nativeDrawUs;
            sample.staticGeometryHits = perf.staticGeometryHits;
            sample.staticGeometryMisses = perf.staticGeometryMisses;
            sample.staticGeometryBytes = perf.staticGeometryBytes;
            sample.staticGeometryEntries = (uint32_t)perf.staticGeometryEntries;
            sample.nativeSceneCount = perf.nativeSceneCount;
            sample.displayQueueBlockedPercent = perf.displayQueueBlockedPercent;
            sample.gpuBackpressureLikely = perf.gpuBackpressureLikely ? 1u : 0u;
            sample.nativeTimingsSampled = perf.nativeTimingsSampled ? 1u : 0u;
            PortProfilerRecordRendererSample(&sample);
        }
        if (!ownNativeRenderer)
        {
            // Aurora snapshots/readback are invalid after graphics handoff.
            // The native renderer reports its own coverage and error status.
            port::capture_completed_performance(g_pGame != NULL && g_pGame->IsGameplayOrOvertime());
            port::capture_view_draw(g_pGame != NULL && g_pGame->IsGameplayOrOvertime());
        }
#endif
        VitaMaybeCaptureFrame();
        port::capture_asset_io(g_pGame != NULL && g_pGame->IsGameplayOrOvertime());
#else
        aurora_end_frame();
#endif
        s_portFrame++;
#if defined(PORT_VITA)
        #if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
        if (PortDiagnosticsEnabled() && (s_portFrame % 60u) == 0u)
            PortVitaSampleRendererStats();
#endif
#endif
        PortBenchFrameEnd();
    }
    if (s_portExitReason == NULL && PortQuitRequested())
        s_portExitReason = "PortQuitRequested";
    OSReport("[port] main loop ended at frame %lu: %s\n", s_portFrame,
             s_portExitReason != NULL ? s_portExitReason : "unknown reason");
#if defined(PORT_VITA)
    PortAudioStop();
    PortNativeAudioShutdown();
    PortProfilerStop();
#if defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
    port::directmatch::shutdown();
#endif
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    aurora::vita::shutdown();
#else
    port::native_runtime::shutdown();
#endif
    port::shutdown_native_assets();
    VitaBootRelease();
    sceKernelExitProcess(0);
#else
    PortDiscordShutdown();
    aurora_shutdown();
#endif
    return 0;
#else
    for (;;)
    {
        nlTaskManager::RunAllTasks();
        UpdateProfile();
    }
#endif
}
