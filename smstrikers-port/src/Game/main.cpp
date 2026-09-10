#include "port/region.h"  // PORT: one binary, three discs
#include "types.h"
#include "NL/nlBind.h"
#include "NL/nlFunction.h"
#include "Game/main.h"

#if defined(PORT_USE_AURORA)
#include <aurora/aurora.h>
#include <dolphin/gx/GXAurora.h>   // AuroraSetViewportPolicy
#include <dolphin/vi.h>                // VILockAspectRatio
#include "port/aspect.h"
#include "port/framerate.h"
#if defined(PORT_USE_AURORA)
#include <SDL3/SDL_video.h>
#endif
#include "port/overlay.h"
extern "C" void PortDebugFrame(void);   // PORT: defined in Game.cpp
#include "port/launch.h"
#include <aurora/main.h>   // #define main aurora_main
#include <aurora/event.h>
#include <stdio.h>   // PORT: snprintf
#endif
#include "port/benchmark.h"
#include "port/audio.h"
#include "port/determinism.h"
#include "port/config.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Sys/audio.h"
#include "Game/Sys/clock.h"
#include "NL/gl/glView.h"
#include "NL/gl/glTexture.h"
#include "NL/gl/glState.h"
#include "NL/gl/gl.h"
#include "NL/gl/glAppAttach.h"
#include "NL/gl/glMemory.h"
#include "Game/Effects/ParticleSystem.h"
#include "Game/Transitions/ModelTransition.h"
#include "NL/nlConfig.h"
#include "NL/nlMain.h"
#include "NL/nlFileGC.h"
#include "NL/nlLocalization.h"
#include "NL/nlString.h"
#include "NL/platpad.h"
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

    // PORT: After the ini files so it wins.
    PortBenchInit();
    if (PortBenchWantsDemo())
    {
        Config::Global().Set("dosoak", true);
        // 0, not 5: TitleScene::Update already refuses to do anything for its first second.
        Config::Global().Set("fe_demo_mode_time_out", 0.0f);
        Config::Global().Set("be_demo_mode_time_out", 86400.0f);
    }

    {
        static const char* const kTeamVars[4][2] = {
            {"STRIKERS_TEAM1", "team1"}, {"STRIKERS_TEAM2", "team2"},
            {"STRIKERS_SIDEKICK1", "sidekick1"}, {"STRIKERS_SIDEKICK2", "sidekick2"}};
        for (int i = 0; i < 4; ++i)
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
        g_Language = nlLocalization::LangJapanese;
    }
    else if (diskid->gameName[0] == 'G' && diskid->gameName[1] == '4' && diskid->gameName[2] == 'Q' && diskid->gameName[3] == 'E')
    {
        g_Language = nlLocalization::LangEnglish;
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

// src/platform/input.cpp, default keyboard mapping for port 0.
extern "C" void PortInstallKeyboardBindings(void);
extern "C" void PortUpdateSyntheticInput(unsigned long frame);
// src/platform/aurora_compat.c, stands in for the VI retrace interrupt that drove controller sampling on console.
extern "C" void PortInvokePadSamplingCallback(void);

static unsigned long s_portFrame = 0;

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
static void PortMaybeRequestCapture()
{
    const char* capturePath = getenv("STRIKERS_CAPTURE");
    if (capturePath == NULL)
        return;

    const char* whichEnv = getenv("STRIKERS_CAPTURE_FRAME");
    unsigned long which = whichEnv != NULL ? strtoul(whichEnv, NULL, 10) : 120;
    if (s_portFrame + 1 != which)
        return;

    OSReport("requesting capture of frame %lu to %s\n", which, capturePath);
    aurora_capture_frame(capturePath);
}

static void PortRequestManualShot()
{
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
}

// PORT: keep the picture the shape of the window; idempotent, because everything below the generation test is skipped unless the shape actually moved.
static void PortFollowWindowShape()
{
    const AuroraWindowSize ws = aurora_window_size();
    const unsigned int gen = PortAspectGeneration();

    PortSetWindowAspect(ws.width, ws.height);
    if (PortAspectGeneration() == gen)
        return;

    PortApplyAspectChange();
    aurora_apply_frame_buffer_resize();
}

static SDL_Window* s_portWindow;

static void PortFollowDisplayRefresh()
{
    if (s_portWindow == NULL)
        return;
    const SDL_DisplayMode* mode =
        SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(s_portWindow));
    int vsync = 0;
    PortFrameLimitInfo(NULL, NULL, &vsync, NULL);
    PortSetDisplayRefresh(mode != NULL ? (double)mode->refresh_rate : 0.0, vsync);
}

static void PortPumpAuroraEvents()
{
    // PORT: polled rather than waited for; the event below is only the fast path.
    PortFollowWindowShape();

    for (const AuroraEvent* ev = aurora_update(); ev && ev->type != AURORA_NONE; ++ev)
    {
        if (ev->type == AURORA_EXIT)
            s_portRunning = false;

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

        // PORT: the limiter follows the display the window is on, which can move or change mode.
        if (ev->type == AURORA_SDL_EVENT
            && (ev->sdl.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED
                || ev->sdl.type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED))
        {
            PortFollowDisplayRefresh();
        }
    }
}
#endif

int main(int argc, char* argv[])
{
    // PORT: strikers.ini -> environment, before anything reads one.
    {
        const int applied = PortConfigLoad();
        if (applied > 0)
            OSReport("[port] %s: %d setting(s)\n",
                     PortConfigPath(), applied);
    }

#if defined(PORT_USE_AURORA)
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
        // Vsync. STRIKERS_VSYNC=0 selects Mailbox or Immediate instead, which is what lets the frame rate exceed the display's.
        cfg.vsync = PortEnvU32("STRIKERS_VSYNC", 1) != 0;

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
            PortSetDisplayRefresh(mode != NULL ? (double)mode->refresh_rate : 0.0,
                                  cfg.vsync ? 1 : 0);
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

        // Aurora's PAD reads SDL gamepads and reports PAD_ERR_NO_CONTROLLER when there is neither a gamepad nor a keyboard binding.
        PortInstallKeyboardBindings();
    }
#else
    (void)argc;
    (void)argv;
#endif

    if (g_DoStackWatermarkTests)
    {
        OSClearStack(g_StackWatermarkFiller);
    }

    Initialize();

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

#if defined(PORT_USE_AURORA)
    while (s_portRunning && !PortQuitRequested())
    {
        PortPumpAuroraEvents();
        PortUpdateSyntheticInput(s_portFrame);
        PortDebugFrame();

        if (!aurora_begin_frame())
            continue;              // minimised or surface lost; nothing to draw

        PortBenchFrameBegin();

        // Sample the pad before the tasks that read it. main() registers VBlankPadUpdate through PADSetSamplingCallback.
        PortInvokePadSamplingCallback();

        nlTaskManager::RunAllTasks();
        UpdateProfile();
        PortBenchAfterTasks();

        // PORT: the audio clock. MusyX runs only inside this call; see include/port/audio.h.
        PortAudioUpdate();

        // PORT: Between begin_frame and end_frame, which is the window in which Aurora's ImGui frame is open.
        PortOverlayDraw();

        PortMaybeRequestCapture();
        // PORT: STRIKERS_CAPTURE_EVERY, a shot every N frames, so a long run can be watched rather than sampled once.
        {
            static long s_everyN = -1;
            if (s_everyN < 0)
            {
                const char* e = getenv("STRIKERS_CAPTURE_EVERY");
                s_everyN = (e != NULL && *e != '\0') ? strtol(e, NULL, 10) : 0;
                if (s_everyN < 0)
                    s_everyN = 0;
            }
            if (s_everyN > 0 && s_portFrame > 0
                && (s_portFrame % (unsigned long)s_everyN) == 0)
            {
                PortRequestManualShot();
            }
        }
        aurora_end_frame();
        PortBenchFrameEnd();
        s_portFrame++;

        // Stop on the benchmark's own clock rather than a frame count: the question is always "how did it behave over N seconds".
        if (PortBenchRunSeconds() > 0.0
            && PortBenchElapsed() >= PortBenchRunSeconds())
        {
            s_portRunning = false;
        }

        if (getenv("STRIKERS_CAPTURE") != NULL
            && getenv("STRIKERS_CAPTURE_EXIT") != NULL)
        {
            const char* whichEnv = getenv("STRIKERS_CAPTURE_FRAME");
            unsigned long which = whichEnv != NULL ? strtoul(whichEnv, NULL, 10) : 120;
            if (s_portFrame >= which)
                s_portRunning = false;
        }
    }
    PortBenchReport();
    aurora_shutdown();
    return 0;
#else
    for (;;)
    {
        nlTaskManager::RunAllTasks();
        UpdateProfile();
    }
#endif
}
