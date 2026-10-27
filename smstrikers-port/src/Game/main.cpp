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
#include <aurora/aurora.h>
#include <aurora/gfx.h>   // aurora_present_waits_for_vblank
#include <dolphin/gx/GXAurora.h>   // AuroraSetViewportPolicy
#include <dolphin/vi.h>                // VILockAspectRatio
#include "port/aspect.h"
#include "port/framerate.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include "port/overlay.h"
extern "C" void PortDebugFrame(void);   // PORT: defined in Game.cpp
#include "port/launch.h"
#include "port/discord.h"
#include "port/shaders.h"   // PORT: the shader stage, held on the memory card screen
#include <aurora/main.h>   // #define main aurora_main
#include <aurora/event.h>
#include <stdio.h>   // PORT: snprintf
#endif
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
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
#include "port/native_audio.hpp"
#include "port/native_match_runtime.hpp"
#include "port/native_graphics.hpp"
#include "port/task_profile.hpp"
#include "port/snapshot_request.hpp"
#include "port/frameskip.hpp"
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
#include "Game/Sys/movie.h"
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

extern "C" void PortRenderSetCinematic(int cinematic);
static bool VitaCaptureFrameToPath(const char* path)
{
    const bool nativeRenderer=port::directmatch::uses_own_renderer();
    SceDisplayFrameBuf fb = {};
    fb.size = sizeof(fb);
    int result=0;
    std::vector<uint8_t> nativePixels;
    if (nativeRenderer) {
        if (!port::directmatch::readback_presented_rgba8(nativePixels)) result=-1;
        else {
            fb.base=nativePixels.data();fb.width=960;fb.height=544;fb.pitch=960;
            fb.pixelformat=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        }
    } else
    {
        sceGxmDisplayQueueFinish();
        result=sceDisplayGetFrameBuf(&fb,SCE_DISPLAY_SETBUF_IMMEDIATE);
    }
    if (result < 0 || fb.base == NULL || fb.width == 0 || fb.width > 4096
        || fb.height == 0 || fb.height > 4096 || fb.pitch < fb.width || fb.pixelformat != 0)
    {
        OSReport("[vita] snapshot failed: framebuffer rc=%d format=%u\n", result, fb.pixelformat);
        return false;
    }
    unsigned char* row = (unsigned char*)malloc(fb.width * 3u);
    if (row == NULL)
        return false;
    FILE* output = fopen(path, "wb");
    bool ok = output != NULL;
    if (output != NULL)
    {
        ok = fprintf(output, "P6\n%u %u\n255\n", fb.width, fb.height) >= 0;
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
        if (ferror(output))
            ok = false;
        if (fclose(output) != 0)
            ok = false;
    }
    free(row);
    return ok;
}

// Opt-in snapshots after present. Request IDs are independent of a scheduled
// snapshot, so UI inspection cannot consume a later match/play-frame capture.
// Display readback and disk writes are intentionally excluded from normal play.
static void VitaMaybeCaptureFrame()
{
    static bool initialized = false;
    static port::SnapshotRequest requests;
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
        if (PortDiagnosticsEnabled())
        {
            const char* requestValue = getenv("STRIKERS_VITA_SNAPSHOT_REQUEST");
            if (requestValue && *requestValue && std::strcmp(requestValue, "0") && !requests.configure(requestValue))
                OSReport("[vita] snapshot request disabled: path exceeds %u bytes\n",
                         unsigned(port::SnapshotRequest::MaxPathLength));
        }
    }
    if (requests.enabled())
    {
        port::SnapshotRequest::OutputPath path;
        const auto requestId = requests.poll(port_monotonic_ns, path);
        if (requestId)
        {
            const bool ok = VitaCaptureFrameToPath(path.data());
            OSReport("[vita] snapshot_request id=%u ok=%d path=%s\n",
                     unsigned(requestId), ok ? 1 : 0, path.data());
        }
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
    if(!matchTarget && !playTarget) {
        if (++heavyFrames != target)return;
    }
    // After the single shot, remove every subsequent fence/readback from play.
    target=0;matchTarget=0;playTarget=0;
    char path[128];
    snprintf(path, sizeof(path), "ux0:data/strikersVita/debug_frame_%s_%lu.ppm", playShot?"play":"3d",shotLabel);
    const bool ok = VitaCaptureFrameToPath(path);
    OSReport("[vita] snapshot 3d_frame=%lu renderer=%s ok=%d path=%s\n",
             heavyFrames,"VitaGL",ok ? 1 : 0,path);
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

    // PORT: the automatic render scale follows the window height.
    PortFollowRenderScale(ws.height);
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
    PortSetDisplayRefresh(mode != NULL ? (double)mode->refresh_rate : 0.0,
                          aurora_present_waits_for_vblank() ? 1 : 0);
}

static void PortPumpHostEvents()
{
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
}
#endif

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
static bool s_portRunning=true;
static const char* s_portExitReason=nullptr;
static unsigned long s_portFrame=0;
static port::frameskip::Scheduler s_frameskip;
static void PortPumpHostEvents()
{
    s_portRunning = port::native_runtime::pump();
    if (!s_portRunning)
        s_portExitReason = "native Start+Select exit";
}
#endif

#include "port/native_boot_screen.hpp"
#include "port/native_watchdog.hpp"
#include "port/native_avc_continuity_probe.hpp"
#if defined(STRIKERS_SAVE_D13_FAULT) && STRIKERS_SAVE_D13_FAULT == 1
#include "port/save_d13_fault.hpp"
#endif
int main(int argc, char* argv[])
{
#if defined(PORT_VITA)
    (void)port::startup::begin();
    // The master INI gate must be resolved before any runtime file is opened.
    (void)PortConfigLoad();
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    if (PortAvcContinuityProbeRequested())
        return PortRunAvcContinuityProbe();
#endif
    port::watchdog::Session watchdogSession(
        PortDiagnosticsEnabled() ? getenv("STRIKERS_VITA_WATCHDOG") : nullptr, getenv("STRIKERS_LOG"));
    if (port::watchdog::error_code())
        OSReport("[vita] watchdog startup failed: %d\n", port::watchdog::error_code());
#if defined(STRIKERS_SAVE_D13_FAULT) && STRIKERS_SAVE_D13_FAULT == 1
    port::save_d13::initialize();
#endif
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::VerifyArchive);
    const int assetVerification=port::assets::verify_archive_if_requested(
        [](unsigned current,unsigned total) noexcept {
            port::startup::show("VERIFYING GAME DATA",current,total);
        });
    if(assetVerification){port::startup::shutdown();return assetVerification<0?1:0;}
    port::directmatch::enable(true);
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
    port::startup::show("LOADING GAME DATA");
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::NativeAudioInit);
    PortNativeAudioInitialize();
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::NativeAudioReady);
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
    port::configure_task_profile();
#endif

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::RuntimeInit);
    if(!port::native_runtime::initialize())return 1;
    s_frameskip.set_mode(port::frameskip::parse_mode(getenv("STRIKERS_VITA_FRAMESKIP")));
    if (PortDiagnosticsEnabled())
        OSReport("[vita] frameskip=%s (render-only)\n",
                 port::frameskip::mode_name(s_frameskip.mode()));
    PortSetWindowAspect(960,544);
    PortSetDisplayRefresh(60.0,1);
    glxVitaReserveResourceArena();
#endif

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
#else
    (void)argc;
    (void)argv;
#endif

    if (g_DoStackWatermarkTests)
    {
        OSClearStack(g_StackWatermarkFiller);
    }

    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::GameInit);
    Initialize();
#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
    // Startup owns the native context and shaders before any game draw.
    if (!port::directmatch::preload())
    {
        OSReport("[vita] native match preload failed; see gxm_native_match_status.csv\n");
        PortAudioStop();
        PortNativeAudioShutdown();
        port::directmatch::shutdown();

        return 1;
    }
    // Direct-to-match startup has no previously displayed front-end frame.
    // Keep the game's localized loading bitmap on screen while invalid
    // resource passes and the first complete scene are being prepared.
    DisplayLoadingMessageFast();
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
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::Limiter);
        PortLimiterFlush();
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::HostInput);
        PortPumpHostEvents();
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::Debug);
        PortDebugFrame();
#if defined(PORT_VITA)
        PortBenchFrameBegin();
        PortProfilerFrameMark();
        PortBenchCaptureStatus frameskipCapture{};
        PortBenchGetCaptureStatus(&frameskipCapture);
        const bool frameskipEligible =
            g_pGame != nullptr && g_pGame->IsGameplayOrOvertime() &&
            !FrontEnd::m_bInPauseMenuState &&
            nlTaskManager::m_pInstance != nullptr &&
            nlTaskManager::m_pInstance->m_CurrState == 2 &&
            nlTaskManager::m_pInstance->m_PendingState == 2 &&
            !PortFixedTimestep() && frameskipCapture.state != PORT_CAPTURE_COLLECTING;
        port::frameskip::set_render_this_frame(
            s_frameskip.should_render(port_monotonic_ns(), 60, frameskipEligible));
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::BeginFrame);
        // vita_async_send: in play and match cinematics, the frame is sent on the
        // send thread while the next one simulates. Never across a state change.
        const unsigned asyncState = nlTaskManager::m_pInstance ? nlTaskManager::m_pInstance->m_CurrState : 0u;
        const bool asyncSend = port::directmatch::send_async_enabled() && g_pGame != nullptr &&
            !FrontEnd::m_bInPauseMenuState && nlTaskManager::m_pInstance != nullptr &&
            (asyncState == 2 || asyncState == 0x10 || asyncState == 0x100) &&
            nlTaskManager::m_pInstance->m_PendingState == asyncState && !PortFixedTimestep() &&
            port::frameskip::render_this_frame();
        port::directmatch::set_send_async_frame(asyncSend);
        if (port::directmatch::send_has_failed())
        {
            s_portExitReason = "native GXM asynchronous send failed";
            break;
        }
        if (!asyncSend && !port::directmatch::begin_frame())
        {
            s_portExitReason = "native GXM begin_frame failed";
            break;
        }
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
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::Pad);
        PortInvokePadSamplingCallback();

        PortProfilerTasksBegin();
#if defined(PORT_VITA)
        auto& taskProfile = port::task_profile();
        if (taskProfile.enabled())
        {
            PortBenchCaptureStatus status{};
            PortBenchGetCaptureStatus(&status);
            taskProfile.begin_frame(status);
        }
#endif
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::Tasks);
#if defined(PORT_VITA)
        if (frameskipCapture.state == PORT_CAPTURE_COLLECTING)
            PortBenchSetScene(nlTaskManager::m_pInstance->m_CurrState,
                nlTaskManager::m_pInstance->m_PrevState, Presentation::Instance().mCurrentFunction);
#endif
        nlTaskManager::RunAllTasks();
#if defined(PORT_VITA)
        taskProfile.end_tasks();
#endif
        PortProfilerTasksEnd();
        PortBenchSetPlayActive(g_pGame != NULL && g_pGame->IsGameplayOrOvertime());
#if defined(PORT_VITA)
        // Intros and replays present at 30 fps, celebrations uncapped. The kickoff
        // lead-up (play state, idle presentation) counts as play.
        {
            const char* presentation = Presentation::Instance().mCurrentFunction;
            const bool lead = nlTaskManager::m_pInstance->m_CurrState == 2 &&
                              (!presentation || !strcmp(presentation, "Idle"));
            const bool cinematic = g_pGame != NULL && !g_pGame->IsGameplayOrOvertime() && !lead;
            const bool celebration = presentation && !strncmp(presentation, "Goal", 4);
            PortRenderSetCinematic(!cinematic ? 0 : celebration ? 2 : 1);
        }
#endif
        if (PortDiagnosticsEnabled())
            UpdateProfile();
        PortBenchAfterTasks();
#if defined(STRIKERS_SAVE_D13_FAULT) && STRIKERS_SAVE_D13_FAULT == 1
        port::save_d13::sample_frontend();
        port::save_d13::drain(s_portFrame);
#endif

        // PORT: the audio clock. MusyX runs only inside this call; see include/port/audio.h.
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::AudioUpdate);
        PortAudioUpdate();
#if defined(PORT_VITA)
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::EndFrame);
        if (!asyncSend && !port::directmatch::end_frame())
        {
            s_portExitReason = "native GXM end_frame failed";
            break;
        }
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::Snapshot);
        VitaMaybeCaptureFrame();
        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::Capture);
        port::capture_asset_io(g_pGame != NULL && g_pGame->IsGameplayOrOvertime());
#else
        aurora_end_frame();
#endif
        s_portFrame++;
        port::watchdog::frame(static_cast<uint32_t>(s_portFrame));

        port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::FrameEnd);
        PortBenchFrameEnd();
#if defined(PORT_VITA)
        port::directmatch::finish_packet_profile_frame();
        if (taskProfile.enabled())
        {
            PortBenchCaptureStatus status{};
            PortBenchGetCaptureStatus(&status);
            taskProfile.end_frame(status, glGetCurrentFrame());
        }
#endif
    }
    if (s_portExitReason == NULL && PortQuitRequested())
        s_portExitReason = "PortQuitRequested";
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::ExitRequested);
    OSReport("[port] main loop ended at frame %lu: %s\n", s_portFrame,
             s_portExitReason != NULL ? s_portExitReason : "unknown reason");
#if defined(PORT_VITA)
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::ShutdownAudio);
    MovieStop(); // joins SceAvPlayer audio and releases movie textures before renderer shutdown
    PortAudioStop();
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::ShutdownAudioLoader);
    PortNativeAudioShutdown();
    PortProfilerStop();
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::ShutdownRenderer);
    port::directmatch::shutdown();
    port::watchdog::stage(port::watchdog::Role::Frontend, port::watchdog::Stage::ShutdownRuntime);
    port::native_runtime::shutdown();

#if defined(STRIKERS_SAVE_D13_FAULT) && STRIKERS_SAVE_D13_FAULT == 1
    port::save_d13::shutdown(s_portFrame);
#endif
    (void)watchdogSession.finish();
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
