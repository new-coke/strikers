// Production audio-option bodies are inserted by tools/test_audio_options_vita.py.
// This tests option dispatch and the memory-copy boundary, not audio rendering.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using u8 = uint8_t;
using u32 = uint32_t;
static unsigned checks;
static unsigned scenarios;
static const char* context = "initialization";
[[noreturn]] static void fail(const char* expression, int line)
{
    std::fprintf(stderr, "AUDIO_OPTIONS_FAIL %s line=%d: %s\n", context, line, expression);
    std::exit(86);
}
#define CHECK(x) do { ++checks; if (!(x)) fail(#x, __LINE__); } while (0)
#define ASSERTLINE(line, x) CHECK(x)
static void near(float a, float b) { CHECK(std::fabs(a - b) < 0.00001f); }

// INSERT_AUDIO_DECLARATIONS

static_assert(sizeof(AudioSettings) == 0x20);
static_assert(offsetof(AudioSettings, Mode) == 0x0c);
static_assert(std::is_trivially_copyable_v<AudioSettings>);
namespace Detail { struct TempStringAllocator {}; }
template<class C, class Allocator> struct BasicString
{
    std::string value;
    BasicString(const char* s) : value(s) {}
    bool operator==(const char* s) const { return value == s; }
};
static const char* configuredMode;
static unsigned configLoads;
struct Config
{
    enum Allocation { ALLOCATE_HIGH };
    explicit Config(Allocation) {}
    void LoadFromFile(const char*) { ++configLoads; }
    template<class T> T Get(const char*, const T& fallback)
    { return configuredMode ? T(configuredMode) : fallback; }
};
static int GetConfigInt(const Config&, const char*, int fallback) { return fallback; }
static const char* AUDIO_DEFAULT_CONFIG_FILE = "DefaultAudioOptions.ini";
static float Interpolate(float a, float b, float t) { return a + (b - a) * t; }

enum MusyXOutputType { MUSYX_MONO = 0, MUSYX_STEREO = 1, MUSYX_SURROUND = 2 };
enum Kind { MainGain, VoiceGain, BusGain, TrackChange, DolbyChange, OutputChange };
struct Event { Kind kind; int argument; float value; bool update; };
static std::vector<Event> events;
namespace Audio {
namespace MasterVolume {
enum Group { VG_Music, VG_SFX, VG_Voice };
static void SetVolume(Group g, float v) { events.push_back({MainGain, g, v, false}); }
static void SetVoiceVolume(float v, int n)
{ CHECK(n == 0); events.push_back({VoiceGain, VG_Voice, v, false}); }
}
static void SetVolGroupVolume(int g, float v, int n)
{ CHECK(n == 0); events.push_back({BusGain, g, v, false}); }
static void SetOutputMode(MusyXOutputType m)
{ events.push_back({OutputChange, int(m), 0, false}); }
}
struct AudioLoader
{
    static bool gbDisableAudio;
    static bool ActivateDPL2(bool on, bool update)
    { events.push_back({DolbyChange, int(on), 0, update}); return true; }
};
bool AudioLoader::gbDisableAudio;
struct TrackManager
{
    void OnMasterVolumeChange(Audio::MasterVolume::Group g)
    { events.push_back({TrackChange, g, 0, false}); }
};
static TrackManager track;
static TrackManager* g_pTrackManager;

// The audio prefix and total extent are the Vita UserInfo layout. Other saved
// fields are opaque here; tournament classes below are serialization boundaries.
struct UserInfo
{
    uint32_t mSaveID = 0;
    AudioSettings mAudioOptions;
    std::array<u8, 0x113c - 4 - sizeof(AudioSettings)> other{};
};
static_assert(offsetof(UserInfo, mAudioOptions) == 4);
static_assert(sizeof(UserInfo) == 0x113c);
struct Series
{
    std::array<u8, 16> bytes{};
    unsigned serializations = 0, deserializations = 0;
    unsigned long GetSaveDataSize() const { return bytes.size(); }
    void SerializeData(void* p) { std::memcpy(p, bytes.data(), bytes.size()); ++serializations; }
    void DeserializeData(void* p) { std::memcpy(bytes.data(), p, bytes.size()); ++deserializations; }
};
struct GameInfoManager
{
    static GameInfoManager* s_pInstance;
    UserInfo mUserInfo;
    AudioSettings mCurGameAudioSettings;
    Series mMushroomCupSeries, mFlowerCupSeries, mStarCupSeries;
    Series mBowserCupSeries, mBowserCupKnockout, mSuperMushroomCupSeries;
    Series mSuperFlowerCupSeries, mSuperStarCupSeries, mSuperBowserCupSeries;
    Series mSuperBowserCupKnockout, mCustomTournamentInfo;
    unsigned long GetMemoryCardDataSize() const;
    void GetMemoryCardData(void*);
    void SetMemoryCardData(void*);
};
GameInfoManager* GameInfoManager::s_pInstance;
struct OptionsAudioMenuV2
{
    AudioSettings& mSettings;
    AudioSettings mBackupSettings;
    explicit OptionsAudioMenuV2(AudioSettings& settings)
        : mSettings(settings), mBackupSettings(settings) {}
    void Revert();
};

#if defined(TEST_RETAIL_SRAM)
// Execute the original console getters/setters as the non-Vita control. The
// hardware SRAM lock/persistence operations are isolated host boundaries.
struct OSSram { u32 flags = 0xa4; };
static OSSram sram;
static unsigned sramWrites;
static OSSram* __OSLockSram() { return &sram; }
static void __OSUnlockSram(int save) { sramWrites += unsigned(save != 0); }
#endif
extern "C" {
// INSERT_OS_BODIES
}

// INSERT_PRODUCTION_BODIES

static AudioSettings setting(eAudioMode mode)
{
    AudioSettings s;
    s.MusicVolume = 2; s.SFXVolume = 4; s.VoiceVolume = 6;
    s.Mode = mode;
    s.DefaultMusicVolume = s.DefaultSFXVolume = s.DefaultVoiceVolume = 10;
    s.DefaultMode = INVALID;
    return s;
}
static std::array<Series*, 11> series(GameInfoManager& g)
{
    return { &g.mMushroomCupSeries, &g.mFlowerCupSeries, &g.mStarCupSeries,
        &g.mBowserCupSeries, &g.mBowserCupKnockout, &g.mSuperMushroomCupSeries,
        &g.mSuperFlowerCupSeries, &g.mSuperStarCupSeries, &g.mSuperBowserCupSeries,
        &g.mSuperBowserCupKnockout, &g.mCustomTournamentInfo };
}
static void reset(GameInfoManager& g, bool disabled, bool hasTrack, u32 osMode)
{
    GameInfoManager::s_pInstance = &g;
    AudioLoader::gbDisableAudio = disabled;
    g_pTrackManager = hasTrack ? &track : nullptr;
    OSSetSoundMode(osMode);
    events.clear();
}
static eAudioMode forcedMode(eAudioMode input, u32 system)
{
#if defined(PORT_VITA)
    (void)system;
    return input;
#else
    return system == 0 ? MONO : input == MONO ? STEREO : input;
#endif
}
static unsigned count(Kind kind)
{
    unsigned n = 0;
    for (const auto& event : events) n += event.kind == kind;
    return n;
}
static void checkDispatch(eAudioMode mode, bool applied, bool update, bool hasTrack)
{
    // Literal mode table, independent of the production switch:
    // saved Stereo=0 -> DPL false/output1; Mono=1 -> false/output0; Dolby=2 -> true/no direct output.
    const bool dolby[3] = { false, false, true };
    const int output[3] = { 1, 0, -1 };
    CHECK(mode >= STEREO && mode <= DOLBY);
    CHECK(count(DolbyChange) == unsigned(applied));
    CHECK(count(OutputChange) == unsigned(applied && output[mode] >= 0));
    CHECK(count(MainGain) == (applied ? 4u : 2u));
    CHECK(count(VoiceGain) == (applied ? 2u : 1u));
    CHECK(count(BusGain) == (applied ? 2u : 1u));
    CHECK(count(TrackChange) == (hasTrack ? (applied ? 6u : 3u) : 0u));
    unsigned modeIndex = 0;
    for (unsigned i = 0; i < events.size(); ++i)
    {
        const auto& e = events[i];
        if (e.kind == DolbyChange)
        { CHECK(e.argument == int(dolby[mode])); CHECK(e.update == update); modeIndex = i; }
        if (e.kind == OutputChange)
        { CHECK(e.argument == output[mode]); CHECK(i == modeIndex + 1); }
        if (e.kind == MainGain)
        {
            CHECK(e.argument == Audio::MasterVolume::VG_Music || e.argument == Audio::MasterVolume::VG_SFX);
            near(e.value, 0.36f);
        }
        if (e.kind == VoiceGain) near(e.value, 0.6f);
        if (e.kind == BusGain) { CHECK(e.argument == 30); near(e.value, 0.36f); }
    }
}
static void checkPublished(const GameInfoManager& g, const AudioSettings& s)
{
    CHECK(g.mCurGameAudioSettings.Mode == s.Mode);
    CHECK(g.mCurGameAudioSettings.MusicVolume == s.MusicVolume);
    CHECK(g.mCurGameAudioSettings.SFXVolume == s.SFXVolume);
    CHECK(g.mCurGameAudioSettings.VoiceVolume == s.VoiceVolume);
}
static void matrix()
{
    context = "apply-matrix";
    for (int mode = 0; mode < 3; ++mode)
    for (bool apply : {false, true})
    for (bool update : {false, true})
    for (bool disabled : {false, true})
    for (bool hasTrack : {false, true})
    {
        ++scenarios;
        GameInfoManager g;
        auto s = setting(eAudioMode(mode));
        reset(g, disabled, hasTrack, 1);
        const auto before = s, published = g.mCurGameAudioSettings;
        s.ApplySettings(apply, update);
        CHECK(s.Mode == mode);
        if (disabled)
        {
            CHECK(events.empty());
            CHECK(std::memcmp(&s, &before, sizeof s) == 0);
            CHECK(std::memcmp(&g.mCurGameAudioSettings, &published, sizeof published) == 0);
            CHECK(OSGetSoundMode() == 1);
        }
        else
        {
            checkDispatch(eAudioMode(mode), apply, update, hasTrack);
            CHECK(s.DefaultMode == (apply ? eAudioMode(mode) : INVALID));
            checkPublished(g, s);
            CHECK(OSGetSoundMode() == (apply ? u32(mode != MONO) : 1u));
            events.clear();
            // Reapplying identical values must not dispatch audio or invalidate the cache.
            s.ApplySettings(apply, update);
            CHECK(events.empty());
        }
    }
    context = "force-matrix";
    for (int mode = 0; mode < 3; ++mode)
    for (u32 osMode : {0u, 1u})
    for (bool update : {false, true})
    for (bool disabled : {false, true})
    for (bool hasTrack : {false, true})
    {
        ++scenarios;
        GameInfoManager g;
        auto s = setting(eAudioMode(mode));
        s.DefaultMode = s.Mode;
        reset(g, disabled, hasTrack, osMode);
        const auto before = s, published = g.mCurGameAudioSettings;
        s.ForceApplySettings(update);
        if (disabled)
        {
            CHECK(events.empty());
            CHECK(std::memcmp(&s, &before, sizeof s) == 0);
            CHECK(std::memcmp(&g.mCurGameAudioSettings, &published, sizeof published) == 0);
            CHECK(OSGetSoundMode() == osMode);
        }
        else
        {
            const auto expected = forcedMode(eAudioMode(mode), osMode);
            CHECK(s.Mode == expected);
            CHECK(s.DefaultMode == expected);
            checkDispatch(expected, true, update, hasTrack);
            checkPublished(g, s);
            CHECK(OSGetSoundMode() == u32(expected != MONO));
        }
    }
}
static void defaults()
{
    context = "defaults";
    CHECK(OSGetSoundMode() == 1); // Fresh process default, before any setter.
    AudioSettings initial;
    CHECK(initial.Mode == STEREO && initial.DefaultMode == STEREO);
    CHECK(initial.MusicVolume == 10 && initial.SFXVolume == 10 && initial.VoiceVolume == 10);
    const char* modes[] = { "STEREO", "MONO", "DOLBY", nullptr, "undefined" };
    for (const char* mode : modes)
    for (u32 osMode : {0u, 1u})
    {
        ++scenarios;
        OSSetSoundMode(osMode);
        configuredMode = mode;
        const auto before = configLoads;
        initial.InitializeDefaults();
        CHECK(configLoads == before + 1);
        const auto expected = mode && std::strcmp(mode, "STEREO") == 0 ? STEREO
            : mode && std::strcmp(mode, "MONO") == 0 ? MONO
            : mode && std::strcmp(mode, "DOLBY") == 0 ? DOLBY : osMode == 0 ? MONO : STEREO;
        CHECK(initial.Mode == expected);
        CHECK(OSGetSoundMode() == osMode);
    }
#if defined(TEST_RETAIL_SRAM)
    CHECK((sram.flags & ~4u) == 0xa0);
#endif
    configuredMode = nullptr;
}
static void roundtrip(eAudioMode mode)
{
    context = mode == MONO ? "reboot-mono" : "reboot-other";
    ++scenarios;
    GameInfoManager original, restored;
    original.mUserInfo.mSaveID = 0x9badf00d;
    original.mUserInfo.mAudioOptions = setting(mode);
    original.mUserInfo.mAudioOptions.DefaultMode = mode; // Typical serialized applied state.
    original.mUserInfo.other.fill(0x5a);
    unsigned label = 1;
    for (auto* part : series(original)) part->bytes.fill(u8(label++));
    const auto n = original.GetMemoryCardDataSize();
    std::vector<u8> storage(n + 32, 0xa7);
    original.GetMemoryCardData(storage.data() + 16);
    for (unsigned i = 0; i < 16; ++i)
    { CHECK(storage[i] == 0xa7); CHECK(storage[n + 16 + i] == 0xa7); }
    for (const auto* part : series(original)) CHECK(part->serializations == 1);
    // A new launch starts at system stereo, but an existing game's saved choice
    // reaches SetMemoryCardData after the unchanged size/dataCRC/iconCRC checks.
    reset(restored, false, true, 1);
    restored.SetMemoryCardData(storage.data() + 16);
    CHECK(std::memcmp(&restored.mUserInfo, &original.mUserInfo, sizeof(UserInfo)) == 0);
    CHECK(restored.mUserInfo.mAudioOptions.Mode == mode);
    const auto sourceParts = series(original), destinationParts = series(restored);
    for (unsigned i = 0; i < sourceParts.size(); ++i)
    {
        CHECK(destinationParts[i]->deserializations == 1);
        CHECK(destinationParts[i]->bytes == sourceParts[i]->bytes);
    }
    restored.mUserInfo.mAudioOptions.ForceApplySettings(false);
    CHECK(restored.mUserInfo.mAudioOptions.Mode == forcedMode(mode, 1));
    CHECK(restored.mUserInfo.mAudioOptions.DefaultMode == forcedMode(mode, 1));
    CHECK(restored.mUserInfo.other == original.mUserInfo.other);
    checkPublished(restored, restored.mUserInfo.mAudioOptions);
}
static void revert(eAudioMode mode)
{
    context = mode == MONO ? "revert-mono" : "revert-other";
    ++scenarios;
    GameInfoManager g;
    auto s = setting(mode);
    OptionsAudioMenuV2 menu(s);
    const auto other = mode == MONO ? STEREO : MONO;
    s.Mode = other;
    reset(g, false, false, 1);
    s.ApplySettings(true, false);
    const auto systemBeforeRevert = OSGetSoundMode();
    s.MusicVolume = 9;
    events.clear();
    menu.Revert();
    CHECK(s.Mode == forcedMode(mode, systemBeforeRevert));
    CHECK(s.MusicVolume == menu.mBackupSettings.MusicVolume);
    CHECK(s.SFXVolume == menu.mBackupSettings.SFXVolume);
    CHECK(s.VoiceVolume == menu.mBackupSettings.VoiceVolume);
    CHECK(count(TrackChange) == 0);
    checkPublished(g, s);
}
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::string selection = argv[1];
    if (selection == "reboot-mono") roundtrip(MONO);
    else if (selection == "revert-mono") revert(MONO);
    else if (selection == "system-readback")
    {
        context = "system-readback";
        OSSetSoundMode(0);
        CHECK(OSGetSoundMode() == 0);
        OSSetSoundMode(1);
        CHECK(OSGetSoundMode() == 1);
        ++scenarios;
    }
    else if (selection == "stereo-control") roundtrip(STEREO);
    else if (selection == "suite")
    {
        defaults();
        matrix();
        for (int mode = 0; mode < 3; ++mode)
        { roundtrip(eAudioMode(mode)); revert(eAudioMode(mode)); }
    }
    else return 2;
    std::printf("Audio options PASS scenarios=%u checks=%u variant=%s\n", scenarios, checks,
#if defined(PORT_VITA)
                "vita"
#else
                "retail"
#endif
    );
    return 0;
}
