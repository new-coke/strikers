// Host boundaries for unchanged production FE/audio lifecycle bodies.
// Generated and compiled by tools/test_fe_audio_lifecycle.py; no product hooks.
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

namespace test {
bool skip = false, io = false, requireReadyBeforePush = true;
float volume = 75.0f;
unsigned visit = 0, checks = 0, trackSerial = 0, created = 0, destroyed = 0;
unsigned sampleReads = 0, samplePurges = 0, streamBuffers = 0, configReads = 0;
unsigned plays = 0, stops = 0, lookupCalls = 0;
std::vector<std::string> events;
void check(bool condition, const char* expression, int line)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FE_AUDIO_CHECK failed visit=%u line=%d: %s\n", visit, line, expression);
        std::exit(86);
    }
}
void event(const std::string& name) { events.push_back(name); }
}
#define CHECK(condition) test::check(bool(condition), #condition, __LINE__)

unsigned long nlStringLowerHash(const char* name)
{
    uint32_t hash = UINT32_MAX;
    while (*name) {
        unsigned char c = *name++;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        hash = hash * 33 + c;
    }
    return hash;
}
int nlSNPrintf(char* out, unsigned long size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int rc = std::vsnprintf(out, size, format, args);
    va_end(args);
    return rc;
}
struct Config {
    static Config& Global() { static Config value; return value; }
} g_FEStreamConfig;
bool GetConfigBool(Config& config, const char* key, bool fallback)
{
    CHECK(&config == &Config::Global() && !std::strcmp(key, "skipfe") && !fallback);
    return test::skip;
}
float GetConfigFloat(Config& config, const char* key, float fallback)
{
    CHECK(&config == &g_FEStreamConfig && !std::strcmp(key, "FE_Main_Title_With_Fade_In/Volume") && fallback == 0);
    ++test::configReads;
    return test::volume;
}
int GetConfigInt(Config& config, const char* key, int fallback)
{
    CHECK(&config == &g_FEStreamConfig && fallback == 0);
    ++test::configReads;
    if (!std::strcmp(key, "FE_Main_Title_With_Fade_In/FadeIn")) return 321;
    CHECK(!std::strcmp(key, "InterruptFadeOut"));
    return 27;
}

namespace Audio {
struct MasterVolume { enum VOLUME_GROUP { VG_Music }; };
bool IsInited() { return true; }
void LoadWorldSFX() { test::event("world.load"); }
void UnloadWorldSFX() { test::event("world.unload"); }
void UnloadInGameSFX() { test::event("game.unload"); }
void ClearFadeData() { test::event("fade.clear"); }
void DestroyPriorityStreams() { test::event("priority.destroy"); }
void Silence() { test::event("silence"); }
}
namespace AudioStreamTrack {
struct StreamTrack {
    std::string name;
    unsigned serial;
    explicit StreamTrack(const char* value) : name(value), serial(++test::trackSerial) { ++test::created; }
    ~StreamTrack() { ++test::destroyed; }
    void PlayStream(unsigned long hash, float volume, bool loop, unsigned long fade,
                    unsigned long interrupt, const char* extra, Audio::MasterVolume::VOLUME_GROUP group)
    {
        CHECK(name == "FE" && serial && hash == nlStringLowerHash("FE_Main_Title_With_Fade_In"));
        CHECK(volume == test::volume / 100.0f && loop && fade == 321 && interrupt == 27);
        CHECK(!*extra && group == Audio::MasterVolume::VG_Music);
        ++test::plays; test::event("title.play");
    }
    void Stop(unsigned long fade)
    {
        CHECK(name == "FE" && serial && fade == 27);
        ++test::stops; test::event("title.stop");
    }
};
struct Clearable { void Clear() { test::event("manager.fade.clear"); } };
struct Pool { void FreeBlocks() { test::event("pool.free"); } };
struct DeleteList { Pool m_Allocator; };
struct TrackManagerBase {
    std::unordered_map<unsigned long, std::unique_ptr<StreamTrack>> tracks;
    Clearable m_FadeMgr;
    Pool m_StreamPool;
    DeleteList m_StreamDeleteList;
    StreamTrack* GetTrack(unsigned long hash)
    {
        ++test::lookupCalls;
        const auto found = tracks.find(hash);
        return found == tracks.end() ? nullptr : found->second.get();
    }
    StreamTrack* CreateTrack(const char* name, Audio::MasterVolume::VOLUME_GROUP group)
    {
        CHECK(group == Audio::MasterVolume::VG_Music);
        const auto hash = nlStringLowerHash(name);
        CHECK(!tracks.count(hash));
        auto track = std::make_unique<StreamTrack>(name);
        auto* result = track.get();
        tracks.emplace(hash, std::move(track));
        test::event(std::string("track.create.") + name);
        return result;
    }
    void StopAllTracks(unsigned delay) { CHECK(delay == 0); test::event("tracks.stop"); }
    void DestroyAllTracks() { tracks.clear(); test::event("tracks.destroy"); }
    void PurgeStreams() { test::event("streams.purge"); }
    bool ready() const
    {
        return tracks.count(nlStringLowerHash("FE")) && tracks.count(nlStringLowerHash("FE2"));
    }
};
}
AudioStreamTrack::TrackManagerBase* g_pTrackManager = nullptr;
struct AudioFileData { const char* szSampleFile = "/audio/data/sebring.samp"; };
struct SndGroupData { int uLoadOrder = -1, stackEnum = -1; };
SndGroupData sebringAudioGroups[5];
bool gbAsyncLoadEntireSampleFileIntoMemRequestMade = false;
class AudioLoader {
public:
    static inline bool gbDisableAudio = false, gbStream = true;
    static inline AudioFileData sebringAudioFileData;
    static void LoadFE(bool);
    static void UnloadInGame();
    static void PlayFETitleMusicWithFade();
    static void UnloadInGameAudioData()
    {
        test::event("game.data.unload");
        for (auto& group : sebringAudioGroups) group = {};
    }
};
namespace PlatAudio {
void ReadEntireSampleFileIntoMemSync(const char* path)
{
    CHECK(gbAsyncLoadEntireSampleFileIntoMemRequestMade && !std::strcmp(path, "/audio/data/sebring.samp"));
    ++test::sampleReads; test::event("sample.read");
}
void LoadSoundGroup(AudioFileData& data, int id, int stack, bool async)
{
    CHECK(&data == &AudioLoader::sebringAudioFileData && (id == 0 || id == 1 || id == 3 || id == 4));
    CHECK(stack == 1 && !async && gbAsyncLoadEntireSampleFileIntoMemRequestMade);
    sebringAudioGroups[id] = {id, stack};
    test::event("group.load." + std::to_string(id));
}
void PurgeSampleFileBuffer()
{
    CHECK(gbAsyncLoadEntireSampleFileIntoMemRequestMade);
    ++test::samplePurges; test::event("sample.purge");
}
void ConfigureStreamBuffers(int count)
{
    CHECK(count == 4 && !gbAsyncLoadEntireSampleFileIntoMemRequestMade);
    ++test::streamBuffers; test::event("buffers.configure");
}
}
void ARQSetChunkSize(unsigned value) { CHECK(value == 4096); test::event("arq.chunk"); }
struct CrowdMood {
    static void SetCrowdVolume(int volume, int fade) { CHECK(!volume && !fade); test::event("crowd.volume"); }
    static void Purge(bool force) { CHECK(!force); test::event("crowd.purge"); }
};
struct SoundEventScript { static void DestroyInstance() { test::event("script.destroy"); } };
namespace FEAudio { void ResetRandomVoiceToggleSFX() { test::event("voice.reset"); } }

// INSERT_SCENE_ENUM
enum ScreenMovement { SCREEN_NOTHING };
template<class T> struct nlSingleton {
    static inline T* s_pInstance = nullptr;
    static T* Instance() { CHECK(s_pInstance); return s_pInstance; }
};
void* nlMalloc(size_t bytes, int alignment, bool clear)
{
    CHECK(alignment == 8 && !clear);
    return ::operator new(bytes);
}
struct FontManager {};
struct FEResourceManager {
    void Initialize() { test::event("resources.init"); }
    void LoadPermanentResourceBundle(const char* path) { CHECK(!std::strcmp(path, "art/fe/MainUI.Dmn")); }
};
struct FESceneManager { unsigned m_uDefaultRenderView = 0; };
struct FEAnimModelManager { void Initialize() { test::event("models.init"); } };
struct GameInfoManager {
    enum GameMode { GM_FRIENDLY, GM_DEMO };
    bool mGoToChooseCaptains = false, demo = false;
    void OnPostGameState() { test::event("postgame"); }
    void SetPlayingSide(int, int) {}
    bool IsInDemoMode() const { return demo; }
    bool IsInCupOrTournamentMode() const { return false; }
    bool IsInTournamentMode() const { return false; }
    bool IsInRegularCupMode() const { return false; }
    bool IsInSuperCupMode() const { return false; }
    void SetMode(GameMode mode) { demo = mode == GM_DEMO; }
    void IncreaseGameNumber(bool) {}
    int GetCurrentRoundNumber() const { return -5; }
    bool DetermineNextMatchups(int) { return true; }
    void IncreaseRoundNumber() {}
    void OnPostCupGameState() {}
    void ResetPlayingSides() {}
};
struct BaseSceneHandler { virtual ~BaseSceneHandler() = default; };
struct SaveLoadScene : BaseSceneHandler {
    SceneList mNextScene = SCENE_INVALID;
    static bool IsIOEnabled() { test::event("io.query"); return test::io; }
};
struct CupHubScene : BaseSceneHandler { bool mDoAutoSave = false; };
struct GameSceneManager {
    SaveLoadScene save;
    CupHubScene hub;
    SceneList lastScene = SCENE_INVALID;
    BaseSceneHandler* Push(SceneList scene, ScreenMovement movement, bool pop)
    {
        CHECK(movement == SCREEN_NOTHING && !pop);
        test::event("push." + std::to_string(scene));
        if (test::requireReadyBeforePush && !AudioLoader::gbDisableAudio) {
            CHECK(g_pTrackManager && g_pTrackManager->ready());
        }
        lastScene = scene;
        return scene == SCENE_SHOULD_LOAD_OR_SAVE ? static_cast<BaseSceneHandler*>(&save) : &hub;
    }
};
struct SHMainMenu { static inline bool mSnapMenuIntoPosition = false; static inline int mLastMenuItem = -1; };
namespace FEMusic {
void ResetCurrentFEStreamHash() { test::event("music.hash.reset"); }
void StartStreamIfDifferent(int) { test::event("music.start"); }
void StopStream() { test::event("music.stop"); }
}
struct Localization { void Load(int, bool extra) { CHECK(!extra); test::event("localization"); } } localization;
Localization* g_pLocalization = &localization;
int g_Language = 0;
bool g_e3_Build = false;
unsigned long long s_FontResourceMark;
unsigned long long glResourceMark() { return 1; }
void EnableAutoPressed() { test::event("input.init"); }
void LoadFonts() { test::event("fonts.load"); }
enum { DC_MEMORY };
struct tDebugPrintManager { static void Print(int, const char*, ...) {} };
struct Allocator { unsigned TotalFreeMemory() const { return 0; } unsigned LargestFreeBlock() const { return 0; } } StandardAllocator;
class TransitionTask {
public:
    enum State { eTS_Initializing, eTS_InState };
    State m_TransitionState = eTS_InState;
    void InitializeFEState();
};

struct TLSlide {};
struct TLTextInstance {};
TLSlide titleSlide;
TLTextInstance titleText;
struct TLComponentInstance { TLSlide* GetActiveSlide() { return &titleSlide; } } titleComponent;
struct FEPresentation { TLSlide* m_currentSlide = &titleSlide; } titlePresentation;
struct InlineHasher { explicit InlineHasher(unsigned long) {} };
template<class T, int Kind> struct FEFinder {
    template<class U, class... Keys> static T* Find(U*, Keys...)
    {
        if constexpr (Kind == 4) return &titleComponent;
        else return &titleText;
    }
};
void OSReport(const char*, ...) { CHECK(false); }
class TitleScene {
public:
    FEPresentation* m_pFEPresentation = &titlePresentation;
    TLTextInstance* mTextPressStart = nullptr;
    void SceneCreated();
};

// INSERT_PRODUCTION_BODIES

static void enter_match()
{
    if (AudioLoader::gbDisableAudio) return;
    g_pTrackManager->DestroyAllTracks();
    g_pTrackManager->CreateTrack("InGame", Audio::MasterVolume::VG_Music);
}
int main(int argc, char** argv)
{
    CHECK(argc == 7);
    test::skip = std::atoi(argv[1]) != 0;
    test::io = std::atoi(argv[2]) != 0;
    AudioLoader::gbDisableAudio = std::atoi(argv[3]) == 0;
    AudioLoader::gbStream = std::atoi(argv[4]) != 0;
    test::volume = float(std::atoi(argv[5]));
    test::requireReadyBeforePush = std::atoi(argv[6]) != 0;
    AudioStreamTrack::TrackManagerBase manager;
    if (!AudioLoader::gbDisableAudio) g_pTrackManager = &manager;
    GameInfoManager info;
    nlSingleton<GameInfoManager>::s_pInstance = &info;
    TransitionTask task;
    if (!test::skip) {
        // StateTransition(from=0x10000,to=4) performs this before Health/FE fast.
        AudioLoader::LoadFE(false);
    } else {
        enter_match();
        AudioLoader::UnloadInGame();
        CHECK(manager.tracks.empty());
    }
    unsigned priorFE = 0, priorFE2 = 0;
    for (test::visit = 1; test::visit <= 2; ++test::visit) {
        if (test::visit == 2) {
            enter_match();
            AudioLoader::UnloadInGame();
            CHECK(manager.tracks.empty());
            info.demo = true; // Existing second-entry branch pushes Title directly.
        }
        task.InitializeFEState();
        CHECK(task.m_TransitionState == TransitionTask::eTS_InState);
        auto* scenes = nlSingleton<GameSceneManager>::Instance();
        CHECK(scenes->lastScene == (test::visit == 1 ?
            (test::io ? SCENE_SHOULD_LOAD_OR_SAVE : SCENE_LEGAL) : SCENE_TITLE));
        if (test::visit == 1 && test::io) CHECK(scenes->save.mNextScene == SCENE_LEGAL);
        // In the negative-title run, leave the missing object unguarded here:
        // the unchanged production Title->Play body must expose the null member call.
        TitleScene title;
        title.SceneCreated();
        CHECK(title.mTextPressStart == &titleText);
        const bool audio = !AudioLoader::gbDisableAudio;
        CHECK(test::streamBuffers == (audio ? test::visit : 0));
        const unsigned expectedSamples = audio ? test::visit - unsigned(!test::skip) : 0;
        CHECK(test::sampleReads == expectedSamples && test::samplePurges == expectedSamples);
        CHECK(!gbAsyncLoadEntireSampleFileIntoMemRequestMade);
        const bool audible = audio && AudioLoader::gbStream;
        CHECK(test::configReads == (audible ? test::visit * 3 : 0));
        CHECK(test::plays == (audible && test::volume > 0 ? test::visit : 0));
        CHECK(test::stops == (audible && test::volume <= 0 ? test::visit : 0));
        if (audio) {
            CHECK(manager.ready() && manager.tracks.size() == 2);
            auto* fe = manager.GetTrack(nlStringLowerHash("FE"));
            auto* fe2 = manager.GetTrack(nlStringLowerHash("FE2"));
            CHECK(fe->serial > priorFE && fe2->serial > priorFE2 && fe->serial != fe2->serial);
            priorFE = fe->serial; priorFE2 = fe2->serial;
        } else {
            CHECK(manager.tracks.empty() && !test::created && !test::lookupCalls);
        }
    }
    if (!AudioLoader::gbDisableAudio) AudioLoader::UnloadInGame();
    CHECK(manager.tracks.empty() && test::created == test::destroyed);
    delete nlSingleton<FontManager>::s_pInstance;
    delete nlSingleton<FEResourceManager>::s_pInstance;
    delete nlSingleton<FESceneManager>::s_pInstance;
    delete nlSingleton<GameSceneManager>::s_pInstance;
    delete nlSingleton<FEAnimModelManager>::s_pInstance;
    std::printf("FE audio lifecycle PASS skip=%d io=%d audio=%d streams=%d volume=%.0f checks=%u tracks=%u/%u\n",
        test::skip, test::io, !AudioLoader::gbDisableAudio, AudioLoader::gbStream,
        test::volume, test::checks, test::created, test::destroyed);
}
