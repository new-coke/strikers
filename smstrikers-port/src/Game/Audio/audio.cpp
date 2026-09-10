#include "NL/nlLexicalCast.h"
#include "Game/Sys/audio.h"
#include "Game/Sys/GCStream.h"
#include "Game/Sys/debug.h"
#include "Game/Game.h"
#include "Game/GameAudio.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/Audio/AudioLoaderCore.h"
#include "Game/Camera/CameraMan.h"
#include "Game/TransitionTask.h"
#include "Game/BasicStadiumCore.h"

#include "NL/nlAlgorithm.h"
#include "NL/nlList.h"
#include "NL/nlMemory.h"
#include "NL/nlSortedSlot.h"
#include "NL/nlTimer.h"
#include "NL/plat/plataudio.h"

// Include PlatStream.h after plataudio.h to get the PlatAudio class
// The namespace PlatAudio and class PlatAudio can coexist
#include "Game/Sys/PlatStream.h"

enum FadeType
{
    FADE_TYPE_NONE = 0,
    FADE_TYPE_SFX = 1,
    FADE_TYPE_FILTER = 2,
    FADE_TYPE_FILTER_ALL = 3,
    FADE_TYPE_VOLGROUP = 4,
};

struct FadeAudioData
{
    FadeType fadeType; // offset 0x0, size 0x4
    union
    {
        int index;            // offset 0x0, size 0x4
        SFXEmitter* pEmitter; // offset 0x0, size 0x4
    } identifier;             // offset 0x4, size 0x4
    float fadeStepSize;       // offset 0x8, size 0x4
    float fadeTimeStart;      // offset 0xC, size 0x4
    float fadeDuration;       // offset 0x10, size 0x4
    float targetVol;          // offset 0x14, size 0x4
    union
    {
        float floatVal;          // offset 0x0, size 0x4
        float* floatPtrVal;      // offset 0x0, size 0x4
    } currentVol;                // offset 0x18, size 0x4
    bool bShutDownAfterDuration; // offset 0x1C, size 0x1
    bool isEmitter;              // offset 0x1D, size 0x1
    bool bTurnFilterOn;          // offset 0x1E, size 0x1
    bool bFilterOn;              // offset 0x1F, size 0x1
    bool bPitchBendOn;           // offset 0x20, size 0x1
    bool bPitchBendApplied;      // offset 0x21, size 0x1
    u8 _pad[2];                  // offset 0x22, size 0x2
    float totalEstimatedTime;    // offset 0x24, size 0x4
    FadeAudioData* next;         // offset 0x28, size 0x4
};

FadeAudioData* g_pFadeList;

static bool gbFilterOn = false;
static bool gbPitchBent = false;
static bool gbUseHiQualityReverb = false;
bool gbListenerInit = false;

bool g_bAudioInitialized = false;
bool g_bAudioInGameLoaded = false;
bool g_bWorldSFXInitialized = false;

extern const char* AUDIO_DEFAULT_CONFIG_FILE;
namespace Audio
{
SND_LISTENER gListener;
}

static SND_AUX_REVERBSTD gReverbStdSettings;
static SND_AUX_REVERBHI gReverbHiSettings;
static SND_AUX_REVERBSTD gDPL2ReverbStdSettings;
static SND_AUX_REVERBHI gDPL2ReverbHiSettings;

Audio::SoundAttributes gDelayedSFX[15];
f32 gfVolumeGroups[23]; // extern'd for StreamTrack.h
#include "Game/Audio/AudioStream.h"
const char* AUDIO_DEFAULT_VOLUMEGROUPS_CONFIG_FILE = "audio/VolumeGroups.ini";
static float gfSilenceTimer = -1.0f;
unsigned long uCurrentSFXVolume = 0x7F;

namespace Audio
{
unsigned long uSFXVolume = 0x7F;
bool gbStartingGame = true;
} // namespace Audio
static void ReadVolGroupSettings();

extern SoundPropAccessor* gpWORLDSoundPropAccessor;
extern SoundPropAccessor* gpPWRUPSoundPropAccessor;
extern SoundPropAccessor* gpSTADGENSoundPropAccessor;
extern SoundPropAccessor* gpCROWDSoundPropAccessor;

// PORT: was a local `namespace AudioScriptEventMgr { void Update(); }`.
#include "Game/Audio/AudioScriptEventMgr.h"

namespace Audio
{

static inline FadeAudioData* RemoveFadeData(FadeAudioData* pFadeAudioData)
{
    nlListRemoveElement<FadeAudioData>(&g_pFadeList, pFadeAudioData, NULL);
    FadeAudioData* pNextFadeAudioData = pFadeAudioData->next;
    delete pFadeAudioData;
    return pNextFadeAudioData;
}

static void RemoveFadeData(FadeType, int);
static bool IsFadeDataInList(FadeType, unsigned long, bool, float);
static void AddFilterFadeData(float, float, float, float, FadeType, float);
static void AddSFXVolFadeData(unsigned long, float*, float, float, float, bool, float);
static void FadeSFXVolToZero(unsigned long, float*, float, bool, float);
static void FadeSFXVolume(unsigned long, float*, float, float, bool, float);
static void SetFilterFreqOnAllCurrentSFX(unsigned short);

float gChantDelayTimer;
float g_fAudioTimer = 0.0f;
bool gbGameIsPaused = false;
bool g_bHomeTeamHasJustScored = false;

/**
 * Offset/Address/Size: 0x4C18 | 0x8014112C | size: 0x100
 */
void Audio::SoundAttributes::Init()
{
    me_ClassType = 0;
    mu_Type = -1;
    mu_SfxID = -1;
    mu_VoiceID = PlatAudio::GetSndIDError();

    mf_Volume = 100.0f;
    mf_VolReverb = 100.0f;
    mf_Attenuate = 1.0f;
    mf_VolAdjustment = 0.0f;
    mf_Panning = 100.0f;
    mf_DelayTime = -1.0f;
    mf_DebugTimer = 0.0f;

    mb_Is3D = false;
    mb_IsPlaying = false;
    mb_KeepTrack = true;
    mb_HasCutoff = false;
    mb_Update3DContinuously = false;
    mb_Pausable = false;
    mb_Restartable = false;
    mb_UseDoppler = false;
    mf_ReturnEmitterOnPlay = false;

    mf_CutoffTime = -1.0f;
    mp_OwnerSFX = NULL;
    mp_PhysObj = NULL;

    pos.pvPos = NULL;
    dir.pvDir = NULL;

    pos.vPos.x = 0.0f;
    pos.vPos.y = 0.0f;
    pos.vPos.z = 0.0f;
    dir.vDir.x = 0.0f;
    dir.vDir.y = 0.0f;
    dir.vDir.z = 0.0f;

    posUpdateMethod = NONE;
    ms_EventName = 0;
    mi_SFXPriority = 0;
    mi_GroupPriority = -1;
    mi_VolGroup = -1;
    mi_EmitterGroup = 0;

    mb_FilterOn = false;
    mu_FilterFreq = 0;
    mu_Pitch = 0x2000;
    mb_NoPhasingFilter = true;
    m_unk_0x7B = false;
    m_unk_0x7C = true;
}

/**
 * Offset/Address/Size: 0x4C0C | 0x80141120 | size: 0xC
 */
void Audio::SoundAttributes::SetSoundType(unsigned long soundType, bool bIs3D)
{
    mu_Type = soundType;
    mb_Is3D = bIs3D;
}

/**
 * Offset/Address/Size: 0x4BF8 | 0x8014110C | size: 0x14
 */
void Audio::SoundAttributes::UsePhysObj(PhysicsObject* obj)
{
    mp_PhysObj = obj;
    posUpdateMethod = PHYSOBJ;
    mb_Update3DContinuously = true;
}

/**
 * Offset/Address/Size: 0x4BDC | 0x801410F0 | size: 0x1C
 */
void Audio::SoundAttributes::UseVectorPtrs(const nlVector3* v1, const nlVector3* v2)
{
    pos.pvPos = v1;
    dir.pvDir = v2;
    posUpdateMethod = PTRS_TO_VECTORS;
    mb_Update3DContinuously = true;
}

/**
 * Offset/Address/Size: 0x4B98 | 0x801410AC | size: 0x44
 */
void Audio::SoundAttributes::UseVectors(const nlVector3& p, const nlVector3& d)
{
    pos.vPos = p;
    dir.vDir = d;
    posUpdateMethod = VECTORS;
    mb_Update3DContinuously = true;
}

/**
 * Offset/Address/Size: 0x4B6C | 0x80141080 | size: 0x2C
 */
void Audio::SoundAttributes::UseStationaryPosVector(const nlVector3& position)
{
    pos.vPos = position;
    posUpdateMethod = VECTORS;
    mb_Update3DContinuously = true;
}

/**
 * Offset/Address/Size: 0x4A10 | 0x80140F24 | size: 0x15C
 */
bool Initialize(bool bInit)
{
    const bool bPlatformStarted = PlatAudio::Initialize(bInit);

    // PORT: created whether or not the platform layer started.
    if (g_pTrackManager == NULL)
    {
        CreateTrackMgr<3>();
    }

    if (!bPlatformStarted)
    {
        return false;
    }

    for (int i = 0; i < 15; i++)
    {
        gDelayedSFX[i].Init();
    }

    static bool bAlreadySetupSoundAVLTrees = false;

    if (!bAlreadySetupSoundAVLTrees)
    {
        for (int i = 0; i < 211; i++)
        {
            gWorldSoundTypeEnumMap[i] = -1;
            gWorldSFXInfo[i].typeID = (unsigned long)-1;
            gWorldSFXInfo[i].typeStr = NULL;
            gWorldSFXInfo[i].musyxStr = NULL;
            gWorldSFXInfo[i].musyxID = (unsigned long)-1;
            gWorldSFXInfo[i].fVolume = 100.0f;
            gWorldSFXInfo[i].fDelay = -1.0f;
            gWorldSFXInfo[i].fVolReverb = 100.0f;
            gWorldSFXInfo[i].volGrp = -1;
            gWorldSFXInfo[i].sfxPriority = 0;
            gWorldSFXInfo[i].uHashVal = 0;
            gWorldSFXInfo[i].pSoundPropAccessor = NULL;
            gWorldSFXInfo[i].bSoundPropTableReloaded = 0;
            gWorldSFXInfo[i].pSoundProp = NULL;
            gWorldSFXInfo[i].pOwner = NULL;
            gWorldSFXInfo[i].lastVoiceID = (unsigned long)-1;
            gWorldSFXInfo[i].pLastEmitter = NULL;
            gWorldSFXInfo[i].m_unk_0x40 = false;
            gWorldSFXInfo[i].typeID = i;
        }

        AudioLoader::SetupSoundDefinesAVLTree();
        AudioLoader::SetupCharSoundTypesAVLTree();
        AudioLoader::SetupWorldSoundTypesAVLTree();
        AudioLoader::SetupSoundGroups();
        bAlreadySetupSoundAVLTrees = true;
    }

    ReadVolGroupSettings();
    Config::Global().LoadFromFile("audio/CrowdScript.ini");
    ::g_bAudioInitialized = true;
    return true;
}

} // namespace Audio

static bool gbTestPrintout;

/**
 * Offset/Address/Size: 0x3AB0 | 0x8013FFC4 | size: 0xF60
 */
static void ReadVolGroupSettings()
{
    int i;
    for (i = 0; i < 23; i++)
    {
        gfVolumeGroups[i] = 1.0f;
    }

    gfVolumeGroups[2] = 0.9f;

    Config& globalConfig = Config::Global();
    TagValuePair& crowdOnlyTvp = globalConfig.FindTvp("crowd_only");
    bool crowdOnly;
    if (crowdOnlyTvp.tag == NULL)
    {
        globalConfig.Set("crowd_only", false);
        crowdOnly = false;
    }
    else
    {
        bool parsedCrowdOnly;

        if (crowdOnlyTvp.type == _BOOL)
        {
            parsedCrowdOnly = LexicalCast<bool, bool>(crowdOnlyTvp.value.b);
        }
        else if (crowdOnlyTvp.type == _INT)
        {
            parsedCrowdOnly = LexicalCast<bool, int>(crowdOnlyTvp.value.i);
        }
        else if (crowdOnlyTvp.type == _FLOAT)
        {
            parsedCrowdOnly = LexicalCast<bool, float>(crowdOnlyTvp.value.f);
        }
        else if (crowdOnlyTvp.type == _STRING)
        {
            parsedCrowdOnly = LexicalCast<bool, const char*>(crowdOnlyTvp.value.s);
        }
        else
        {
            parsedCrowdOnly = false;
        }

        crowdOnly = parsedCrowdOnly;
    }

    Config config(Config::ALLOCATE_HIGH);
    config.LoadFromFile(AUDIO_DEFAULT_VOLUMEGROUPS_CONFIG_FILE);

    float crowd;
    {
        TagValuePair& tvp = config.FindTvp("Crowd");
        if (tvp.tag == NULL)
        {
            config.Set("Crowd", 0.8f);
            crowd = 0.8f;
        }
        else
        {
            float parsedCrowd;

            if (tvp.type == _BOOL)
            {
                parsedCrowd = LexicalCast<float, bool>(tvp.value.b);
            }
            else if (tvp.type == _INT)
            {
                parsedCrowd = LexicalCast<float, int>(tvp.value.i);
            }
            else if (tvp.type == _FLOAT)
            {
                parsedCrowd = LexicalCast<float, float>(tvp.value.f);
            }
            else if (tvp.type == _STRING)
            {
                parsedCrowd = LexicalCast<float, const char*>(tvp.value.s);
            }
            else
            {
                parsedCrowd = 0.0f;
            }

            crowd = parsedCrowd;
        }
    }

    Audio::SetVolGroupVolume(2, crowd, 500);
    gfVolumeGroups[5] = crowd;

    if (!crowdOnly)
    {
        float fe;
        {
            TagValuePair& tvp = config.FindTvp("FE");
            if (tvp.tag == NULL)
            {
                config.Set("FE", 0.8f);
                fe = 0.8f;
            }
            else
            {
                float parsedFe;

                if (tvp.type == _BOOL)
                {
                    parsedFe = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedFe = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedFe = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedFe = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedFe = 0.0f;
                }

                fe = parsedFe;
            }
        }

        float birdo;
        {
            TagValuePair& tvp = config.FindTvp("Birdo");
            if (tvp.tag == NULL)
            {
                config.Set("Birdo", 1.0f);
                birdo = 1.0f;
            }
            else
            {
                float parsedBirdo;

                if (tvp.type == _BOOL)
                {
                    parsedBirdo = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedBirdo = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedBirdo = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedBirdo = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedBirdo = 0.0f;
                }

                birdo = parsedBirdo;
            }
        }

        float bowser;
        {
            TagValuePair& tvp = config.FindTvp("Bowser");
            if (tvp.tag == NULL)
            {
                config.Set("Bowser", 1.0f);
                bowser = 1.0f;
            }
            else
            {
                float parsedBowser;

                if (tvp.type == _BOOL)
                {
                    parsedBowser = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedBowser = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedBowser = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedBowser = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedBowser = 0.0f;
                }

                bowser = parsedBowser;
            }
        }

        float critter;
        {
            TagValuePair& tvp = config.FindTvp("Critter");
            if (tvp.tag == NULL)
            {
                config.Set("Critter", 1.0f);
                critter = 1.0f;
            }
            else
            {
                float parsedCritter;

                if (tvp.type == _BOOL)
                {
                    parsedCritter = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedCritter = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedCritter = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedCritter = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedCritter = 0.0f;
                }

                critter = parsedCritter;
            }
        }

        float daisy;
        {
            TagValuePair& tvp = config.FindTvp("Daisy");
            if (tvp.tag == NULL)
            {
                config.Set("Daisy", 1.0f);
                daisy = 1.0f;
            }
            else
            {
                float parsedDaisy;

                if (tvp.type == _BOOL)
                {
                    parsedDaisy = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedDaisy = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedDaisy = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedDaisy = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedDaisy = 0.0f;
                }

                daisy = parsedDaisy;
            }
        }

        float dk;
        {
            TagValuePair& tvp = config.FindTvp("DK");
            if (tvp.tag == NULL)
            {
                config.Set("DK", 1.0f);
                dk = 1.0f;
            }
            else
            {
                float parsedDk;

                if (tvp.type == _BOOL)
                {
                    parsedDk = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedDk = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedDk = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedDk = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedDk = 0.0f;
                }

                dk = parsedDk;
            }
        }

        float hammerBrothers;
        {
            TagValuePair& tvp = config.FindTvp("Hammer Brothers");
            if (tvp.tag == NULL)
            {
                config.Set("Hammer Brothers", 1.0f);
                hammerBrothers = 1.0f;
            }
            else
            {
                float parsedHammerBrothers;

                if (tvp.type == _BOOL)
                {
                    parsedHammerBrothers = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedHammerBrothers = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedHammerBrothers = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedHammerBrothers = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedHammerBrothers = 0.0f;
                }

                hammerBrothers = parsedHammerBrothers;
            }
        }

        float koopa;
        {
            TagValuePair& tvp = config.FindTvp("Koopa");
            if (tvp.tag == NULL)
            {
                config.Set("Koopa", 1.0f);
                koopa = 1.0f;
            }
            else
            {
                float parsedKoopa;

                if (tvp.type == _BOOL)
                {
                    parsedKoopa = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedKoopa = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedKoopa = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedKoopa = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedKoopa = 0.0f;
                }

                koopa = parsedKoopa;
            }
        }

        float luigi;
        {
            TagValuePair& tvp = config.FindTvp("Luigi");
            if (tvp.tag == NULL)
            {
                config.Set("Luigi", 1.0f);
                luigi = 1.0f;
            }
            else
            {
                float parsedLuigi;

                if (tvp.type == _BOOL)
                {
                    parsedLuigi = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedLuigi = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedLuigi = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedLuigi = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedLuigi = 0.0f;
                }

                luigi = parsedLuigi;
            }
        }

        float mario;
        {
            TagValuePair& tvp = config.FindTvp("Mario");
            if (tvp.tag == NULL)
            {
                config.Set("Mario", 1.0f);
                mario = 1.0f;
            }
            else
            {
                float parsedMario;

                if (tvp.type == _BOOL)
                {
                    parsedMario = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedMario = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedMario = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedMario = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedMario = 0.0f;
                }

                mario = parsedMario;
            }
        }

        float peach;
        {
            TagValuePair& tvp = config.FindTvp("Peach");
            if (tvp.tag == NULL)
            {
                config.Set("Peach", 1.0f);
                peach = 1.0f;
            }
            else
            {
                float parsedPeach;

                if (tvp.type == _BOOL)
                {
                    parsedPeach = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedPeach = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedPeach = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedPeach = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedPeach = 0.0f;
                }

                peach = parsedPeach;
            }
        }

        float super;
        {
            TagValuePair& tvp = config.FindTvp("Super");
            if (tvp.tag == NULL)
            {
                config.Set("Super", 1.0f);
                super = 1.0f;
            }
            else
            {
                float parsedSuper;

                if (tvp.type == _BOOL)
                {
                    parsedSuper = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedSuper = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedSuper = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedSuper = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedSuper = 0.0f;
                }

                super = parsedSuper;
            }
        }

        float toad;
        {
            TagValuePair& tvp = config.FindTvp("Toad");
            if (tvp.tag == NULL)
            {
                config.Set("Toad", 1.0f);
                toad = 1.0f;
            }
            else
            {
                float parsedToad;

                if (tvp.type == _BOOL)
                {
                    parsedToad = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedToad = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedToad = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedToad = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedToad = 0.0f;
                }

                toad = parsedToad;
            }
        }

        float waluigi;
        {
            TagValuePair& tvp = config.FindTvp("Waluigi");
            if (tvp.tag == NULL)
            {
                config.Set("Waluigi", 1.0f);
                waluigi = 1.0f;
            }
            else
            {
                float parsedWaluigi;

                if (tvp.type == _BOOL)
                {
                    parsedWaluigi = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedWaluigi = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedWaluigi = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedWaluigi = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedWaluigi = 0.0f;
                }

                waluigi = parsedWaluigi;
            }
        }

        float wario;
        {
            TagValuePair& tvp = config.FindTvp("Wario");
            if (tvp.tag == NULL)
            {
                config.Set("Wario", 1.0f);
                wario = 1.0f;
            }
            else
            {
                float parsedWario;

                if (tvp.type == _BOOL)
                {
                    parsedWario = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedWario = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedWario = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedWario = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedWario = 0.0f;
                }

                wario = parsedWario;
            }
        }

        float yoshi;
        {
            TagValuePair& tvp = config.FindTvp("Yoshi");
            if (tvp.tag == NULL)
            {
                config.Set("Yoshi", 1.0f);
                yoshi = 1.0f;
            }
            else
            {
                float parsedYoshi;

                if (tvp.type == _BOOL)
                {
                    parsedYoshi = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedYoshi = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedYoshi = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedYoshi = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedYoshi = 0.0f;
                }

                yoshi = parsedYoshi;
            }
        }

        float allIngameDialogue;
        {
            TagValuePair& tvp = config.FindTvp("All Ingame Dialogue");
            if (tvp.tag == NULL)
            {
                config.Set("All Ingame Dialogue", 1.0f);
                allIngameDialogue = 1.0f;
            }
            else
            {
                float parsedAllIngameDialogue;

                if (tvp.type == _BOOL)
                {
                    parsedAllIngameDialogue = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedAllIngameDialogue = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedAllIngameDialogue = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedAllIngameDialogue = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedAllIngameDialogue = 0.0f;
                }

                allIngameDialogue = parsedAllIngameDialogue;
            }
        }

        float allFEDialogue;
        {
            TagValuePair& tvp = config.FindTvp("All FE Dialogue");
            if (tvp.tag == NULL)
            {
                config.Set("All FE Dialogue", 1.0f);
                allFEDialogue = 1.0f;
            }
            else
            {
                float parsedAllFEDialogue;

                if (tvp.type == _BOOL)
                {
                    parsedAllFEDialogue = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedAllFEDialogue = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedAllFEDialogue = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedAllFEDialogue = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedAllFEDialogue = 0.0f;
                }

                allFEDialogue = parsedAllFEDialogue;
            }
        }

        float everythingElse;
        {
            TagValuePair& tvp = config.FindTvp("Everything Else");
            if (tvp.tag == NULL)
            {
                config.Set("Everything Else", 0.8f);
                everythingElse = 0.8f;
            }
            else
            {
                float parsedEverythingElse;

                if (tvp.type == _BOOL)
                {
                    parsedEverythingElse = LexicalCast<float, bool>(tvp.value.b);
                }
                else if (tvp.type == _INT)
                {
                    parsedEverythingElse = LexicalCast<float, int>(tvp.value.i);
                }
                else if (tvp.type == _FLOAT)
                {
                    parsedEverythingElse = LexicalCast<float, float>(tvp.value.f);
                }
                else if (tvp.type == _STRING)
                {
                    parsedEverythingElse = LexicalCast<float, const char*>(tvp.value.s);
                }
                else
                {
                    parsedEverythingElse = 0.0f;
                }

                everythingElse = parsedEverythingElse;
            }
        }

        Audio::SetVolGroupVolume(3, fe, 500);
        gfVolumeGroups[6] = fe;

        Audio::SetVolGroupVolume(4, allFEDialogue, 500);
        gfVolumeGroups[7] = allFEDialogue;

        Audio::SetVolGroupVolume(1, everythingElse, 500);

        gfVolumeGroups[2] = fe;
        gfVolumeGroups[3] = allIngameDialogue;
        gfVolumeGroups[8] = birdo;
        gfVolumeGroups[9] = bowser;
        gfVolumeGroups[10] = critter;
        gfVolumeGroups[11] = daisy;
        gfVolumeGroups[12] = dk;
        gfVolumeGroups[13] = hammerBrothers;
        gfVolumeGroups[14] = koopa;
        gfVolumeGroups[15] = luigi;
        gfVolumeGroups[16] = mario;
        gfVolumeGroups[17] = peach;
        gfVolumeGroups[18] = super;
        gfVolumeGroups[19] = toad;
        gfVolumeGroups[20] = waluigi;
        gfVolumeGroups[21] = wario;
        gfVolumeGroups[22] = yoshi;

        Audio::SetVolGroupVolume(5, allIngameDialogue * birdo, 500);
        Audio::SetVolGroupVolume(6, allIngameDialogue * bowser, 500);
        Audio::SetVolGroupVolume(7, allIngameDialogue * critter, 500);
        Audio::SetVolGroupVolume(8, allIngameDialogue * daisy, 500);
        Audio::SetVolGroupVolume(9, allIngameDialogue * dk, 500);
        Audio::SetVolGroupVolume(10, allIngameDialogue * hammerBrothers, 500);
        Audio::SetVolGroupVolume(11, allIngameDialogue * koopa, 500);
        Audio::SetVolGroupVolume(12, allIngameDialogue * luigi, 500);
        Audio::SetVolGroupVolume(13, allIngameDialogue * mario, 500);
        Audio::SetVolGroupVolume(14, allIngameDialogue * peach, 500);
        Audio::SetVolGroupVolume(15, allIngameDialogue * super, 500);
        Audio::SetVolGroupVolume(16, allIngameDialogue * toad, 500);
        Audio::SetVolGroupVolume(17, allIngameDialogue * waluigi, 500);
        Audio::SetVolGroupVolume(18, allIngameDialogue * wario, 500);
        Audio::SetVolGroupVolume(19, allIngameDialogue * yoshi, 500);
    }
}

namespace Audio
{

static inline char* GetStadiumStr(eStadiumID stadiumID)
{
    char* retval;

    switch (stadiumID)
    {
    case STAD_MARIO_STADIUM:
        retval = "STAD_MARIO";
        break;
    case STAD_PEACH_TOAD_STADIUM:
        retval = "STAD_PEACH";
        break;
    case STAD_DK_DAISY:
        retval = "STAD_DK";
        break;
    case STAD_WARIO_STADIUM:
        retval = "STAD_WARIO";
        break;
    case STAD_YOSHI_STADIUM:
        retval = "STAD_YOSHI";
        break;
    case STAD_SUPER_STADIUM:
        retval = "STAD_SUPER";
        break;
    case STAD_FORBIDDEN_DOME:
        retval = "STAD_DOME";
        break;
    default:
        retval = "NOT_YET_DEFINED";
        break;
    }

    return retval;
}

/**
 * Offset/Address/Size: 0x337C | 0x8013F890 | size: 0x734
 */
bool InitializeReverb(eStadiumID stadiumID, unsigned char studio)
{
    MusyXEffectType type;
    void* pAuxEffectSettings;
    bool bResult;
    char reverbStr[80];
    char headerStr[80];
    SND_AUX_REVERBHI* pReverbHiSettings;
    SND_AUX_REVERBSTD* pReverbStdSettings;
    float coloration;
    float mix;
    float time;
    float damping;
    float preDelay;
    float crosstalk;

    if (AudioLoader::gbDisableReverb)
    {
        return true;
    }

    Config config(Config::ALLOCATE_HIGH);
    config.LoadFromFile(AUDIO_DEFAULT_CONFIG_FILE);

    memset(reverbStr, 0, sizeof(reverbStr));
    memset(headerStr, 0, sizeof(headerStr));

    if (stadiumID == STAD_INVALID)
    {
        nlStrNCpy(headerStr, "DEFAULT", 0x50);
    }
    else
    {
        char* stadiumStr = GetStadiumStr(stadiumID);
        nlStrNCpy(headerStr, stadiumStr, 0x50);
    }

    nlStrLen(headerStr);

    nlStrNCat(reverbStr, headerStr, " Use High Quality Reverb", 0x50);
    {
        TagValuePair& tvp = config.FindTvp(reverbStr);

        if (tvp.tag == NULL)
        {
            config.Set(reverbStr, true);
            bResult = true;
        }
        else if (tvp.type == _BOOL)
        {
            bResult = LexicalCast<bool, bool>(tvp.value.b);
        }
        else if (tvp.type == _INT)
        {
            bResult = LexicalCast<bool, int>(tvp.value.i);
        }
        else if (tvp.type == _FLOAT)
        {
            bResult = LexicalCast<bool, float>(tvp.value.f);
        }
        else if (tvp.type == _STRING)
        {
            bResult = LexicalCast<bool, const char*>(tvp.value.s);
        }
        else
        {
            bResult = false;
        }
    }
    gbUseHiQualityReverb = bResult;

    nlStrNCat(reverbStr, headerStr, " Reverb Coloration", 0x50);
    {
        TagValuePair& tvp = config.FindTvp(reverbStr);
        if (tvp.tag == NULL)
        {
            config.Set(reverbStr, 0.9f);
            coloration = 0.9f;
        }
        else
        {
            float fValue;

            if (tvp.type == _BOOL)
            {
                fValue = LexicalCast<float, bool>(tvp.value.b);
            }
            else if (tvp.type == _INT)
            {
                fValue = LexicalCast<float, int>(tvp.value.i);
            }
            else if (tvp.type == _FLOAT)
            {
                fValue = LexicalCast<float, float>(tvp.value.f);
            }
            else if (tvp.type == _STRING)
            {
                fValue = LexicalCast<float, const char*>(tvp.value.s);
            }
            else
            {
                fValue = 0.0f;
            }

            coloration = fValue;
        }
    }

    nlStrNCat(reverbStr, headerStr, " Reverb Mix", 0x50);
    {
        TagValuePair& tvp = config.FindTvp(reverbStr);
        if (tvp.tag == NULL)
        {
            config.Set(reverbStr, 0.3f);
            mix = 0.3f;
        }
        else
        {
            float fValue;

            if (tvp.type == _BOOL)
            {
                fValue = LexicalCast<float, bool>(tvp.value.b);
            }
            else if (tvp.type == _INT)
            {
                fValue = LexicalCast<float, int>(tvp.value.i);
            }
            else if (tvp.type == _FLOAT)
            {
                fValue = LexicalCast<float, float>(tvp.value.f);
            }
            else if (tvp.type == _STRING)
            {
                fValue = LexicalCast<float, const char*>(tvp.value.s);
            }
            else
            {
                fValue = 0.0f;
            }

            mix = fValue;
        }
    }

    nlStrNCat(reverbStr, headerStr, " Reverb Time", 0x50);
    {
        TagValuePair& tvp = config.FindTvp(reverbStr);
        if (tvp.tag == NULL)
        {
            config.Set(reverbStr, 3.5f);
            time = 3.5f;
        }
        else
        {
            float fValue;

            if (tvp.type == _BOOL)
            {
                fValue = LexicalCast<float, bool>(tvp.value.b);
            }
            else if (tvp.type == _INT)
            {
                fValue = LexicalCast<float, int>(tvp.value.i);
            }
            else if (tvp.type == _FLOAT)
            {
                fValue = LexicalCast<float, float>(tvp.value.f);
            }
            else if (tvp.type == _STRING)
            {
                fValue = LexicalCast<float, const char*>(tvp.value.s);
            }
            else
            {
                fValue = 0.0f;
            }

            time = fValue;
        }
    }

    nlStrNCat(reverbStr, headerStr, " Reverb Damping", 0x50);
    {
        TagValuePair& tvp = config.FindTvp(reverbStr);
        if (tvp.tag == NULL)
        {
            config.Set(reverbStr, 0.5f);
            damping = 0.5f;
        }
        else
        {
            float fValue;

            if (tvp.type == _BOOL)
            {
                fValue = LexicalCast<float, bool>(tvp.value.b);
            }
            else if (tvp.type == _INT)
            {
                fValue = LexicalCast<float, int>(tvp.value.i);
            }
            else if (tvp.type == _FLOAT)
            {
                fValue = LexicalCast<float, float>(tvp.value.f);
            }
            else if (tvp.type == _STRING)
            {
                fValue = LexicalCast<float, const char*>(tvp.value.s);
            }
            else
            {
                fValue = 0.0f;
            }

            damping = fValue;
        }
    }

    nlStrNCat(reverbStr, headerStr, " Reverb Pre Delay", 0x50);
    {
        TagValuePair& tvp = config.FindTvp(reverbStr);
        if (tvp.tag == NULL)
        {
            config.Set(reverbStr, 0.1f);
            preDelay = 0.1f;
        }
        else
        {
            float fValue;

            if (tvp.type == _BOOL)
            {
                fValue = LexicalCast<float, bool>(tvp.value.b);
            }
            else if (tvp.type == _INT)
            {
                fValue = LexicalCast<float, int>(tvp.value.i);
            }
            else if (tvp.type == _FLOAT)
            {
                fValue = LexicalCast<float, float>(tvp.value.f);
            }
            else if (tvp.type == _STRING)
            {
                fValue = LexicalCast<float, const char*>(tvp.value.s);
            }
            else
            {
                fValue = 0.0f;
            }

            preDelay = fValue;
        }
    }

    nlStrNCat(reverbStr, headerStr, " Reverb Crosstalk", 0x50);
    {
        TagValuePair& tvp = config.FindTvp(reverbStr);
        if (tvp.tag == NULL)
        {
            config.Set(reverbStr, 0.0f);
            crosstalk = 0.0f;
        }
        else
        {
            float fValue;

            if (tvp.type == _BOOL)
            {
                fValue = LexicalCast<float, bool>(tvp.value.b);
            }
            else if (tvp.type == _INT)
            {
                fValue = LexicalCast<float, int>(tvp.value.i);
            }
            else if (tvp.type == _FLOAT)
            {
                fValue = LexicalCast<float, float>(tvp.value.f);
            }
            else if (tvp.type == _STRING)
            {
                fValue = LexicalCast<float, const char*>(tvp.value.s);
            }
            else
            {
                fValue = 0.0f;
            }

            crosstalk = fValue;
        }
    }

    if (gbUseHiQualityReverb)
    {
        if (PlatAudio::gUsingDolbyProLogic2)
        {
            pReverbHiSettings = &gDPL2ReverbHiSettings;
        }
        else
        {
            pReverbHiSettings = &gReverbHiSettings;
        }

        pAuxEffectSettings = pReverbHiSettings;
        pReverbHiSettings->tempDisableFX = false;
        type = MUSYX_EFFECT_REVERB_HI;
        pReverbHiSettings->time = time;
        pReverbHiSettings->preDelay = preDelay;
        pReverbHiSettings->damping = damping;
        pReverbHiSettings->coloration = coloration;
        pReverbHiSettings->crosstalk = crosstalk;
        pReverbHiSettings->mix = mix;
    }
    else
    {
        if (PlatAudio::gUsingDolbyProLogic2)
        {
            pReverbStdSettings = &gDPL2ReverbStdSettings;
        }
        else
        {
            pReverbStdSettings = &gReverbStdSettings;
        }

        pAuxEffectSettings = pReverbStdSettings;
        pReverbStdSettings->tempDisableFX = false;
        type = MUSYX_EFFECT_REVERB;
        pReverbStdSettings->time = time;
        pReverbStdSettings->preDelay = preDelay;
        pReverbStdSettings->damping = damping;
        pReverbStdSettings->coloration = coloration;
        pReverbStdSettings->mix = mix;
    }

    if (AudioLoader::gReverbOn)
    {
        nlPrintf("Reverb already on, calling UpdateAuxEffectA().\n");
        bResult = PlatAudio::UpdateAuxEffectA(type, pAuxEffectSettings);
    }
    else
    {
        nlPrintf("Reverb hasn't been turned on yet, this must be the very 1st time from Audio::Initialize().\n");
        nlPrintf("Calling AddAuxEffectA().\n");
        bResult = PlatAudio::AddAuxEffectA(type, pAuxEffectSettings, studio);
    }

    if (bResult)
    {
        nlPrintf("Audio::InitializeReverb() successful...\n");
        return true;
    }

    nlPrintf("Audio::InitializeReverb() unsuccessful.\n");
    return false;
}

/**
 * Offset/Address/Size: 0x32E0 | 0x8013F7F4 | size: 0x9C
 */
bool ShutdownReverb()
{
    long long currTime = OSGetTime();
    nlPrintf("Audio::ShutdownReverb(), turning reverb off at %d\n", currTime);

    if (!AudioLoader::gReverbOn)
    {
        nlPrintf("Audio::ShutdownReverb(), gReverbOn should never be off.\n");
        return false;
    }

    nlPrintf("Audio::ShutdownReverb(), now shutting reverb down...\n");
    if (!PlatAudio::ShutdownAuxEffectA())
    {
        nlPrintf("Audio::ShutdownReverb(), PlatAudio::ShutdownAuxEffectA() returned false.\n");
        return false;
    }

    AudioLoader::gReverbOn = false;
    return true;
}

/**
 * Offset/Address/Size: 0x32D8 | 0x8013F7EC | size: 0x8
 */
bool IsInited()
{
    return ::g_bAudioInitialized;
}

bool IsInGameLoaded()
{
    return ::g_bAudioInGameLoaded;
}

/**
 * Offset/Address/Size: 0x3258 | 0x8013F76C | size: 0x80
 */
void LoadInGameSFX()
{
    for (int i = 0; i < 64; i++)
    {
        PlatAudio::InitEmitter((unsigned long)i);
    }

    for (int j = 0; j < 15; j++)
    {
        gDelayedSFX[j].Init();
    }

    g_bHomeTeamHasJustScored = false;
    g_fAudioTimer = 0.0f;
    g_bAudioInGameLoaded = true;
}

/**
 * Offset/Address/Size: 0x31A8 | 0x8013F6BC | size: 0xB0
 */
void UnloadInGameSFX()
{
    for (int i = 0; i < 64; i++)
    {
        PlatAudio::RemoveEmitter(i);
        PlatAudio::InitEmitter(i);
    }

    for (int i = 0; i < 15; i++)
    {
        gDelayedSFX[i].Init();
    }

    gbStartingGame = true;
    g_bHomeTeamHasJustScored = false;

    if (gbListenerInit)
    {
        PlatAudio::Remove3DSFXListener(&gListener);
        gbListenerInit = false;
    }

    g_bAudioInGameLoaded = false;
    g_fAudioTimer = 0.0f;
}

/**
 * Offset/Address/Size: 0x30E8 | 0x8013F5FC | size: 0xC0
 */
void LoadWorldSFX()
{
    if (::g_bWorldSFXInitialized)
        return;

    gWorldSFX.Init();
    gPowerupSFX.Init();
    gStadGenSFX.Init();

    if (!AudioLoader::gbDisableCrowd)
        gCrowdSFX.Init();

    gWorldSFX.SetSFX(gpWORLDSoundPropAccessor);
    gPowerupSFX.SetSFX(gpPWRUPSoundPropAccessor);
    gStadGenSFX.SetSFX(gpSTADGENSoundPropAccessor);

    if (!AudioLoader::gbDisableCrowd)
        gCrowdSFX.SetSFX(gpCROWDSoundPropAccessor);

    gbGameIsPaused = false;
    ::g_bWorldSFXInitialized = true;
}

/**
 * Offset/Address/Size: 0x30E0 | 0x8013F5F4 | size: 0x8
 */
bool IsWorldSFXLoaded()
{
    return ::g_bWorldSFXInitialized;
}

/**
 * Offset/Address/Size: 0x307C | 0x8013F590 | size: 0x64
 */
void UnloadWorldSFX()
{
    if (!::g_bWorldSFXInitialized)
        return;

    gWorldSFX.ShutdownPlaySet();
    gPowerupSFX.ShutdownPlaySet();
    gStadGenSFX.ShutdownPlaySet();
    gCrowdSFX.ShutdownPlaySet();
    gbGameIsPaused = false;
    ::g_bWorldSFXInitialized = false;
}

/**
 * Offset/Address/Size: 0x3070 | 0x8013F584 | size: 0xC
 */
void ResetPauseStatus()
{
    gbGameIsPaused = false;
}

/**
 * Offset/Address/Size: 0x3028 | 0x8013F53C | size: 0x48
 */
void ResetForNewGame()
{
    gbGameIsPaused = false;
    gbStartingGame = true;
    g_bHomeTeamHasJustScored = false;
    gChantDelayTimer = 0.0f;
    nlDeleteList<FadeAudioData>(&g_pFadeList);
    g_pFadeList = NULL;
}

/**
 * Offset/Address/Size: 0x3008 | 0x8013F51C | size: 0x20
 */
void Silence()
{
    PlatAudio::StopAllSound();
}

void SilenceIn(float fSeconds)
{
    gfSilenceTimer = g_fAudioTimer + fSeconds;
}

/**
 * Offset/Address/Size: 0x2FC4 | 0x8013F4D8 | size: 0x44
 */
void Shutdown()
{
    if (::g_bAudioInitialized)
    {
        if (PlatAudio::IsStreamingInited())
        {
            PlatAudio::ShutdownStreaming();
        }

        PlatAudio::Shutdown();
        ::g_bAudioInitialized = false;
    }
}

/**
 * Offset/Address/Size: 0x2FBC | 0x8013F4D0 | size: 0x8
 */
float GetAudioTimer()
{
    return g_fAudioTimer;
}

/**
 * Offset/Address/Size: 0x2F9C | 0x8013F4B0 | size: 0x20
 */
unsigned long PlaySFX(const SFXStartInfo& info)
{
    return PlatAudio::PlaySFX(info);
}

/**
 * Offset/Address/Size: 0x2E20 | 0x8013F334 | size: 0x17C
 */
unsigned long PlaySFXbyID(const SoundAttributes& attrs, unsigned long sfxID, float fVol, float fRevVol, int volGroup)
{
    if (!::g_bAudioInitialized)
    {
        return (uintptr_t)-1;
    }

    if (sfxID != (unsigned long)-1)
    {
        if (attrs.mf_DelayTime <= 0.0f)
        {
            if (volGroup > -1)
            {
                if (volGroup < 0)
                {
                    PlatAudio::SetSFXVolumeGroup(sfxID, 0);
                }
                else if (volGroup > 255)
                {
                    PlatAudio::SetSFXVolumeGroup(sfxID, 255);
                }
                else
                {
                    PlatAudio::SetSFXVolumeGroup(sfxID, (unsigned char)volGroup);
                }
            }

            SFXStartInfo info;
            info.uSFXID = (unsigned long)-1;
            info.fVolume = 100.0f;
            info.fPan = 100.0f;
            info.fVolReverb = 100.0f;
            info.uSurroundPan = 0xFF;
            info.uPitchBend = 0x2000;
            info.bActivateFilter = false;
            info.filterFreq = 0;
            info.uModulation = 0;
            info.uDoppler = 0x2000;

            info.uSFXID = sfxID;
            info.fVolume = fVol;
            info.fPan = attrs.mf_Panning;
            info.fVolReverb = fRevVol;
            info.bActivateFilter = attrs.mb_FilterOn;
            info.filterFreq = attrs.mu_FilterFreq;
            info.uPitchBend = attrs.mu_Pitch;

            unsigned long voiceID = PlatAudio::PlaySFX(info);

            if (attrs.mb_KeepTrack)
            {
                attrs.mp_OwnerSFX->KeepTrack(0, attrs, voiceID);
            }

            return voiceID;
        }

        AddDelayedSFX(attrs, sfxID, fVol, fRevVol, (cGameSFX*)0);
    }

    return (uintptr_t)-1;
}

/**
 * Offset/Address/Size: 0x2A3C | 0x8013EF50 | size: 0x3E4
 */
int AddDelayedSFX(const SoundAttributes& sfxData, unsigned long uSFXID, float volume, float delay, cGameSFX* pOwnerSFX)
{
    if (!::g_bAudioInitialized)
    {
        return -1;
    }

    if (uSFXID != (unsigned long)-1)
    {
        int slot = -1;
        for (int i = 0; i < 15; i++)
        {
            if (gDelayedSFX[i].mu_Type == (u32)-1)
            {
                slot = i;
                break;
            }
        }

        if (slot == -1)
        {
            tDebugPrintManager::Print(DC_SOUND, "Too many delayed sfx - finding oldest slot and killing it\n");

            float min = gDelayedSFX[0].mf_DebugTimer;
            int minIndex = 0;
            for (int i = 1; i < 15; i++)
            {
                if (gDelayedSFX[i].mf_DebugTimer < min)
                {
                    min = gDelayedSFX[i].mf_DebugTimer;
                    minIndex = i;
                }
            }
            slot = minIndex;
        }

        if (slot >= 0)
        {
            gDelayedSFX[slot].Init();
            gDelayedSFX[slot] = sfxData;

            if (pOwnerSFX != NULL)
            {
                gDelayedSFX[slot].mp_OwnerSFX = pOwnerSFX;
            }

            if (sfxData.mf_CutoffTime >= 0.0f)
            {
                gDelayedSFX[slot].mb_HasCutoff = true;
            }

            if (g_pGame != NULL)
            {
                gDelayedSFX[slot].mf_DebugTimer = g_pGame->GetGameTime();
            }
            else
            {
                gDelayedSFX[slot].mf_DebugTimer = g_fAudioTimer;
            }

            return uSFXID;
        }
    }

    return -1;
}

/**
 * Offset/Address/Size: 0x2954 | 0x8013EE68 | size: 0xE8
 */
int IsDelayedCharSFX(unsigned long sfxType, cGameSFX* pOwner)
{
    for (int i = 0; i < 15; i++)
    {
        if (gDelayedSFX[i].mu_Type == sfxType && gDelayedSFX[i].mp_OwnerSFX == pOwner)
        {
            return i;
        }
    }
    return -1;
}

/**
 * Offset/Address/Size: 0x2924 | 0x8013EE38 | size: 0x30
 */
void RemoveDelayedSFX(unsigned long index)
{
    gDelayedSFX[index].Init();
}

/**
 * Offset/Address/Size: 0x27E0 | 0x8013ECF4 | size: 0x144
 */

uintptr_t PlayWorldSFXbyStr(const char* szSFXType, float fVol, float fDelay, bool bIs3D, bool bKeepTrack, const nlVector3* pInitialPosVector, const nlVector3* pInitialDirVector, unsigned long* pType)
{
    if (!::g_bAudioInitialized)
    {
        return (uintptr_t)-1;
    }

    eWorldSFX type = (eWorldSFX)AudioLoader::GetWorldSFXTypeFromStr(szSFXType);

    if (pType != NULL)
    {
        *pType = type;
    }

    if (!::g_bWorldSFXInitialized)
    {
        return (uintptr_t)-1;
    }

    SoundAttributes attr;
    attr.Init();
    attr.mu_Type = type;
    attr.mb_Is3D = bIs3D;
    attr.mf_Volume = fVol;
    attr.mf_DelayTime = fDelay;
    attr.mb_KeepTrack = bKeepTrack;

    if (bIs3D)
    {
        attr.mb_Is3D = bIs3D;
        attr.pos.pvPos = pInitialPosVector;
        attr.dir.pvDir = pInitialDirVector;
        attr.posUpdateMethod = PTRS_TO_VECTORS;
        attr.mb_Update3DContinuously = true;
        attr.mf_ReturnEmitterOnPlay = true;
    }

    if (type < 0x5E)
    {
        return gWorldSFX.Play(attr);
    }
    else if (type < 0x92)
    {
        return gPowerupSFX.Play(attr);
    }
    else if (type < 0xB1)
    {
        return gCrowdSFX.Play(attr);
    }
    else if (type < 0xD3)
    {
        return gStadGenSFX.Play(attr);
    }

    return (uintptr_t)-1;
}

/**
 * Offset/Address/Size: 0x26DC | 0x8013EBF0 | size: 0x104
 */
uintptr_t PlayCharSFXbyStr(const char* szSFXType, NisCharacterClass charIdentifier, float fVol, float fDelay, bool bIs3D, bool bKeepTrack, const nlVector3* pInitialPosVector, const nlVector3* pInitialDirVector, unsigned long* pType)
{
    if (!::g_bAudioInitialized)
    {
        return -1;
    }

    unsigned long sfxType = AudioLoader::GetCharSFXTypeFromStr(szSFXType);

    if (pType != NULL)
    {
        *pType = sfxType;
    }

    if (g_pGame == NULL)
    {
        return -1;
    }

    SoundAttributes sa;
    sa.Init();
    sa.mu_Type = sfxType;
    sa.mb_Is3D = bIs3D;
    sa.mf_Volume = fVol;
    sa.mf_DelayTime = fDelay;
    sa.mb_KeepTrack = bKeepTrack;

    if (bIs3D)
    {
        sa.mb_Is3D = bIs3D;
        sa.pos.pvPos = pInitialPosVector;
        sa.dir.pvDir = pInitialDirVector;
        sa.posUpdateMethod = PTRS_TO_VECTORS;
        sa.mb_Update3DContinuously = true;
        sa.mf_ReturnEmitterOnPlay = true;
    }

    cCharacter* character = cCharacterSFX::GetCharacterFromNisCharClass(charIdentifier);
    if (character == NULL)
    {
        return -1;
    }

    return character->m_pCharacterSFX->Play(sa);
}

/**
 * Offset/Address/Size: 0x2628 | 0x8013EB3C | size: 0xB4
 */
void StopWorldSFXbyStr(const char* szSFXType)
{
    if (!::g_bAudioInitialized)
        return;
    unsigned long type = AudioLoader::GetWorldSFXTypeFromStr(szSFXType);
    if (!::g_bWorldSFXInitialized)
        return;

    if ((int)type < 94)
    {
        gWorldSFX.Stop((eWorldSFX)type, cGameSFX::SFX_STOP_FIRST);
    }
    else if ((int)type < 146)
    {
        gPowerupSFX.Stop((eWorldSFX)type, cGameSFX::SFX_STOP_FIRST);
    }
    else if ((int)type < 177)
    {
        gCrowdSFX.Stop((eWorldSFX)type, cGameSFX::SFX_STOP_FIRST);
    }
    else if ((int)type < 211)
    {
        gStadGenSFX.Stop((eWorldSFX)type, cGameSFX::SFX_STOP_FIRST);
    }
}

/**
 * Offset/Address/Size: 0x25C0 | 0x8013EAD4 | size: 0x68
 */
void StopCharSFXbyStr(const char* szSFXType, NisCharacterClass charIdentifier)
{
    if (!::g_bAudioInitialized)
        return;
    Audio::eCharSFX sfxType = (Audio::eCharSFX)AudioLoader::GetCharSFXTypeFromStr(szSFXType);
    if (g_pGame == NULL)
        return;
    cCharacter* character = Audio::cCharacterSFX::GetCharacterFromNisCharClass(charIdentifier);
    if (character == NULL)
        return;
    character->StopSFX(sfxType);
}

/**
 * Offset/Address/Size: 0x23AC | 0x8013E8C0 | size: 0x214
 */
unsigned long PlaySFXEventFromScript(const SoundEventData& sfxEventData, const char* szSFXType, float fVol, float fDelay)
{
    if (!::g_bAudioInitialized || !::g_bWorldSFXInitialized)
    {
        return -1;
    }

    eWorldSFX uSFXType = (eWorldSFX)AudioLoader::GetWorldSFXTypeFromStr(szSFXType);
    if ((unsigned long)(uSFXType + 0x10000) == 0xFFFF)
    {
        return -1;
    }

    SoundAttributes sndAtr;
    sndAtr.Init();
    sndAtr.mu_Type = uSFXType;
    sndAtr.mb_Is3D = false;
    sndAtr.mf_Volume = fVol;
    sndAtr.mf_DelayTime = fDelay;
    sndAtr.ms_EventName = sfxEventData.eventName;
    sndAtr.mi_GroupPriority = sfxEventData.eventPriority;

    const char* pdest = strstr(szSFXType, "CROWDSFX");
    int loc = pdest - szSFXType;
    if (pdest != NULL && loc == 0)
    {
        return gCrowdSFX.Play(sndAtr);
    }

    pdest = strstr(szSFXType, "NISSFX");
    loc = pdest - szSFXType;
    if (pdest != NULL && loc == 0)
    {
        return gWorldSFX.Play(sndAtr);
    }

    pdest = strstr(szSFXType, "BALLSFX");
    loc = pdest - szSFXType;
    if (pdest != NULL && loc == 0)
    {
        return gStadGenSFX.Play(sndAtr);
    }

    pdest = strstr(szSFXType, "STADSFX");
    loc = pdest - szSFXType;
    if (pdest != NULL && loc == 0)
    {
        return gStadGenSFX.Play(sndAtr);
    }

    pdest = strstr(szSFXType, "WORLDSFX");
    loc = pdest - szSFXType;
    if (pdest != NULL && loc == 0)
    {
        return gWorldSFX.Play(sndAtr);
    }

    pdest = strstr(szSFXType, "PWRUPSFX");
    loc = pdest - szSFXType;
    if (pdest != NULL && loc == 0)
    {
        return gPowerupSFX.Play(sndAtr);
    }

    return gWorldSFX.Play(sndAtr);
}

/**
 * Offset/Address/Size: 0x2378 | 0x8013E88C | size: 0x34
 */
bool StopSFX(unsigned long sfxID)
{
    if (::g_bAudioInitialized)
    {
        return PlatAudio::StopSFX(sfxID);
    }
    return false;
}

/**
 * Offset/Address/Size: 0x2344 | 0x8013E858 | size: 0x34
 */
bool IsSFXPlaying(unsigned long sfxID)
{
    if (::g_bAudioInitialized)
    {
        return PlatAudio::IsSFXPlaying(sfxID);
    }
    return false;
}

/**
 * Offset/Address/Size: 0x2324 | 0x8013E838 | size: 0x20
 */
int GetSndIDError()
{
    return PlatAudio::GetSndIDError();
}

static inline void UpdateDelayedAudio(float fDeltaT)
{
    int i;

    for (i = 0; i < 15; i++)
    {
        if (gDelayedSFX[i].mu_Type != (u32)-1)
        {
            if (gDelayedSFX[i].mf_DelayTime >= 0.0f && gDelayedSFX[i].mf_DelayTime - fDeltaT <= 0.0f)
            {
                if (gDelayedSFX[i].me_ClassType == CHAR)
                {
                    gDelayedSFX[i].mf_DelayTime = -1.0f;
                    gDelayedSFX[i].mu_VoiceID = ((cCharacterSFX*)gDelayedSFX[i].mp_OwnerSFX)->Play(gDelayedSFX[i]);
                }
                else
                {
                    gDelayedSFX[i].mf_DelayTime = -1.0f;
                    gDelayedSFX[i].mu_VoiceID = gDelayedSFX[i].mp_OwnerSFX->Play(gDelayedSFX[i]);
                }

                PlatAudio::SetSFXReverbVol(gDelayedSFX[i].mu_VoiceID, gDelayedSFX[i].mf_VolReverb);
                gDelayedSFX[i].Init();
            }
            else
            {
                gDelayedSFX[i].mf_DelayTime -= fDeltaT;
            }
        }
    }
}

static inline void Update3DSFXListenerPos();

/**
 * Offset/Address/Size: 0x1FD0 | 0x8013E4E4 | size: 0x354
 */
void Audio::Update(float fDeltaT)
{
    if (::g_bAudioInitialized == false)
    {
        return;
    }

    g_fAudioTimer += fDeltaT;
    if (uSFXVolume != uCurrentSFXVolume)
    {
        sndVolume((u8)uSFXVolume, 0x1F4, 0xFE);
        uCurrentSFXVolume = uSFXVolume;
    }

    UpdateFades(fDeltaT);
    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->Update(fDeltaT);

    if (g_pGame != NULL)
    {
        CrowdMood::Update(fDeltaT);
        AudioScriptEventMgr::Update();
    }

    if (::g_bWorldSFXInitialized)
    {
        gWorldSFX.UpdateAllTrackedSFX(fDeltaT);
        gPowerupSFX.UpdateAllTrackedSFX(fDeltaT);
        gStadGenSFX.UpdateAllTrackedSFX(fDeltaT);
        gCrowdSFX.UpdateAllTrackedSFX(fDeltaT);
    }

    if (g_pGame != NULL)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 5; j++)
            {
                pTeam->GetPlayer(j)->m_pCharacterSFX->UpdateAllTrackedSFX(fDeltaT);
            }
        }

        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->UpdateAllTrackedSFX(fDeltaT);
    }

    if (gfSilenceTimer > 0.0f && g_fAudioTimer > gfSilenceTimer)
    {
        PlatAudio::StopAllSound();
        gfSilenceTimer = -1.0f;
    }

    if (gbGameIsPaused == false)
    {
        UpdateDelayedAudio(fDeltaT);
        Update3DSFXListenerPos();
        Update3DSFXEmitters();
    }
}

static inline void ActivateFilterOnAllCurrentSFXInl(bool bOn)
{
    gWorldSFX.ActivateFilterOnAllTrackedSFX(bOn);
    gPowerupSFX.ActivateFilterOnAllTrackedSFX(bOn);
    gStadGenSFX.ActivateFilterOnAllTrackedSFX(bOn);
    gCrowdSFX.ActivateFilterOnAllTrackedSFX(bOn);

    if (g_pGame)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 5; j++)
            {
                cPlayer* pPlayer = pTeam->GetPlayer(j);
                pPlayer->m_pCharacterSFX->ActivateFilterOnAllTrackedSFX(bOn);
            }
        }

        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->ActivateFilterOnAllTrackedSFX(bOn);
    }

    CrowdMood::ActivateLPF(bOn);
    gbFilterOn = bOn;
}

static inline void SetFilterFreqOnAllCurrentSFXInl(unsigned short freq)
{
    gWorldSFX.SetFilterFreqOnAllTrackedSFX(freq);
    gPowerupSFX.SetFilterFreqOnAllTrackedSFX(freq);
    gStadGenSFX.SetFilterFreqOnAllTrackedSFX(freq);
    gCrowdSFX.SetFilterFreqOnAllTrackedSFX(freq);

    if (g_pGame)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 5; j++)
            {
                cPlayer* pPlayer = pTeam->GetPlayer(j);
                pPlayer->m_pCharacterSFX->SetFilterFreqOnAllTrackedSFX(freq);
            }
        }

        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->SetFilterFreqOnAllTrackedSFX(freq);
    }

    CrowdMood::SetLPF(freq);
}

static inline void SetPitchBendOnAllDialogueSFXInl(unsigned short pitch)
{
    for (int i = 0; i < 2; i++)
    {
        cTeam* pTeam = g_pTeams[i];
        for (int j = 0; j < 5; j++)
        {
            pTeam->GetPlayer(j)->m_pCharacterSFX->SetPitchBendOnAllDialogueSFX(pitch);
        }
    }
    BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->SetPitchBendOnAllDialogueSFX(pitch);
}

/**
 * Offset/Address/Size: 0x159C | 0x8013DAB0 | size: 0xA34
 */
void UpdateFades(float fDeltaT)
{
    FadeAudioData* pFadeData;
    float newVol;

    pFadeData = g_pFadeList;
    while (pFadeData != NULL)
    {
        float totalEstimatedTime = pFadeData->totalEstimatedTime;
        if (-1.0f == totalEstimatedTime)
        {
            pFadeData->totalEstimatedTime = pFadeData->fadeTimeStart + pFadeData->fadeDuration;
        }
        else
        {
            if (totalEstimatedTime < -10.0f)
            {
                pFadeData = RemoveFadeData(pFadeData);
                continue;
            }
            pFadeData->totalEstimatedTime = totalEstimatedTime - fDeltaT;
        }

        switch (pFadeData->fadeType)
        {
        case FADE_TYPE_SFX:
        {
            if (pFadeData->isEmitter)
            {
                if (!PlatAudio::IsEmitterActive(pFadeData->identifier.pEmitter))
                {
                    break;
                }
            }

            bool isPlaying;
            u32 sfxID = pFadeData->identifier.index;
            if (::g_bAudioInitialized)
            {
                isPlaying = PlatAudio::IsSFXPlaying(sfxID);
            }
            else
            {
                isPlaying = false;
            }
            if (!isPlaying)
            {
                break;
            }

            if (pFadeData->fadeTimeStart > 0.0f)
            {
                pFadeData->fadeTimeStart -= fDeltaT;
            }

            if (pFadeData->fadeTimeStart <= 0.0f && pFadeData->fadeDuration > 0.0f)
            {
                pFadeData->fadeDuration -= fDeltaT;
                if (pFadeData->fadeDuration < 0.01f)
                {
                    pFadeData->fadeDuration = 0.0f;
                }

                float deltaVol = fDeltaT * pFadeData->fadeStepSize;
                float currentVol = *pFadeData->currentVol.floatPtrVal;
                newVol = currentVol + deltaVol;

                if (pFadeData->isEmitter)
                {
                    if (pFadeData->identifier.pEmitter->posUpdateMethod == PHYSOBJ)
                    {
                        nlVector3 vel = { 0.0f, 0.0f, 0.0f };
                        if (pFadeData->identifier.pEmitter->pPhysObj->m_bodyID)
                        {
                            nlVector3& linVel = pFadeData->identifier.pEmitter->pPhysObj->GetLinearVelocity();
                            vel = linVel;
                        }
                        PlatAudio::Update3DSFXEmitter(
                            pFadeData->identifier.pEmitter,
                            pFadeData->identifier.pEmitter->pPhysObj->GetPosition(),
                            vel,
                            newVol);
                    }
                    else if (pFadeData->identifier.pEmitter->posUpdateMethod == PTRS_TO_VECTORS)
                    {
                        PlatAudio::Update3DSFXEmitter(
                            pFadeData->identifier.pEmitter,
                            *pFadeData->identifier.pEmitter->pos.pvPos,
                            *pFadeData->identifier.pEmitter->dir.pvDir,
                            newVol);
                    }
                    else if (pFadeData->identifier.pEmitter->posUpdateMethod == VECTORS)
                    {
                        PlatAudio::Update3DSFXEmitter(
                            pFadeData->identifier.pEmitter,
                            pFadeData->identifier.pEmitter->pos.vPos,
                            pFadeData->identifier.pEmitter->dir.vDir,
                            newVol);
                    }
                }
                else
                {
                    PlatAudio::SetSFXVolume(pFadeData->identifier.index, newVol);
                }

                *pFadeData->currentVol.floatPtrVal = newVol;
            }

            if (pFadeData->fadeDuration <= 0.0f)
            {
                if (pFadeData->isEmitter)
                {
                    if (pFadeData->identifier.pEmitter->posUpdateMethod == PHYSOBJ)
                    {
                        nlVector3 vel = { 0.0f, 0.0f, 0.0f };
                        if (pFadeData->identifier.pEmitter->pPhysObj->m_bodyID)
                        {
                            nlVector3& linVel = pFadeData->identifier.pEmitter->pPhysObj->GetLinearVelocity();
                            vel = linVel;
                        }
                        float targetVol = pFadeData->targetVol;
                        const nlVector3& pos = pFadeData->identifier.pEmitter->pPhysObj->GetPosition();
                        PlatAudio::Update3DSFXEmitter(
                            pFadeData->identifier.pEmitter,
                            pos,
                            vel,
                            targetVol);
                    }
                    else if (pFadeData->identifier.pEmitter->posUpdateMethod == PTRS_TO_VECTORS)
                    {
                        PlatAudio::Update3DSFXEmitter(
                            pFadeData->identifier.pEmitter,
                            *pFadeData->identifier.pEmitter->pos.pvPos,
                            *pFadeData->identifier.pEmitter->dir.pvDir,
                            pFadeData->targetVol);
                    }
                    else if (pFadeData->identifier.pEmitter->posUpdateMethod == VECTORS)
                    {
                        PlatAudio::Update3DSFXEmitter(
                            pFadeData->identifier.pEmitter,
                            pFadeData->identifier.pEmitter->pos.vPos,
                            pFadeData->identifier.pEmitter->dir.vDir,
                            pFadeData->targetVol);
                    }
                }
                else
                {
                    PlatAudio::SetSFXVolume(pFadeData->identifier.index, pFadeData->targetVol);
                }

                if (pFadeData->bShutDownAfterDuration)
                {
                    if (pFadeData->isEmitter)
                    {
                        PlatAudio::RemoveEmitter(pFadeData->identifier.pEmitter);
                    }
                    else
                    {
                        u32 sfxID = pFadeData->identifier.index;
                        if (::g_bAudioInitialized)
                        {
                            PlatAudio::StopSFX(sfxID);
                        }
                    }
                }

                pFadeData = RemoveFadeData(pFadeData);
                continue;
            }
            break;
        }

        case FADE_TYPE_FILTER:
        {
            if (pFadeData->fadeTimeStart > 0.0f)
            {
                pFadeData->fadeTimeStart -= fDeltaT;
            }

            if (pFadeData->fadeTimeStart <= 0.0f && pFadeData->fadeDuration > 0.0f)
            {
                if (pFadeData->bTurnFilterOn && !pFadeData->bFilterOn)
                {
                    ActivateFilterOnAllCurrentSFXInl(true);
                    pFadeData->bFilterOn = true;
                }

                pFadeData->fadeDuration -= fDeltaT;
                if (pFadeData->fadeDuration < 0.01f)
                {
                    pFadeData->fadeDuration = 0.0f;
                }

                float deltaVal = fDeltaT * pFadeData->fadeStepSize;
                float newVal = pFadeData->currentVol.floatVal + deltaVal;
                newVal = (newVal >= 0.0f) ? newVal : 0.0f;
                newVal = (newVal <= 1.0f) ? newVal : 1.0f;

                float newFreq = 16383.0f * newVal;
                float roundOffset;
                if (newFreq < 0.0f)
                {
                    roundOffset = -0.5f;
                }
                else
                {
                    roundOffset = 0.5f;
                }
                newFreq += roundOffset;

                unsigned short targetFreq = (unsigned short)(s32)newFreq;
                if (targetFreq > 0x3FFF)
                {
                    targetFreq = 0x3FFF;
                }
                SetFilterFreqOnAllCurrentSFXInl(targetFreq);
                pFadeData->currentVol.floatVal = newVal;
            }

            if (pFadeData->fadeDuration <= 0.0f)
            {
                float targetFreqFloat = 16383.0f * pFadeData->targetVol;
                targetFreqFloat += (targetFreqFloat < 0.0f) ? -0.5f : 0.5f;

                unsigned short targetFreq = (unsigned short)(s32)targetFreqFloat;
                if (targetFreq > 0x3FFF)
                {
                    targetFreq = 0x3FFF;
                }
                SetFilterFreqOnAllCurrentSFXInl(targetFreq);

                if (pFadeData->bShutDownAfterDuration)
                {
                    ActivateFilterOnAllCurrentSFXInl(false);
                    pFadeData->bFilterOn = false;
                }

                pFadeData = RemoveFadeData(pFadeData);
                continue;
            }
            break;
        }

        case FADE_TYPE_FILTER_ALL:
        {
            if (pFadeData->fadeTimeStart > 0.0f)
            {
                pFadeData->fadeTimeStart -= fDeltaT;
            }

            if (pFadeData->fadeTimeStart <= 0.0f && pFadeData->fadeDuration > 0.0f)
            {
                pFadeData->fadeDuration -= fDeltaT;
                if (pFadeData->fadeDuration < 0.01f)
                {
                    pFadeData->fadeDuration = 0.0f;
                }

                float deltaVal = fDeltaT * pFadeData->fadeStepSize;
                float newVal = pFadeData->currentVol.floatVal + deltaVal;
                newVal = (newVal >= 0.0f) ? newVal : 0.0f;
                newVal = (newVal <= 1.0f) ? newVal : 1.0f;

                float newPitch = 8192.0f - (8192.0f - (8192.0f * newVal));
                cGame* game = g_pGame;
                float roundOffset;
                if (newPitch < 0.0f)
                {
                    roundOffset = -0.5f;
                }
                else
                {
                    roundOffset = 0.5f;
                }
                newPitch += roundOffset;

                unsigned short targetFreq = (unsigned short)(s32)newPitch;
                if (targetFreq > 0x3FFF)
                {
                    targetFreq = 0x3FFF;
                }

                if (game != NULL)
                {
                    SetPitchBendOnAllDialogueSFXInl(targetFreq);
                }

                gbPitchBent = targetFreq != 0x2000;
                pFadeData->bPitchBendApplied = true;
                pFadeData->currentVol.floatVal = newVal;
            }

            if (pFadeData->fadeDuration <= 0.0f)
            {
                if (pFadeData->bShutDownAfterDuration)
                {
                    if (g_pGame != NULL)
                    {
                        SetPitchBendOnAllDialogueSFXInl(0x2000);
                    }
                    gbPitchBent = false;
                    pFadeData->bPitchBendApplied = false;
                    gbPitchBent = false;
                }

                pFadeData = RemoveFadeData(pFadeData);
                continue;
            }
            break;
        }
        case FADE_TYPE_VOLGROUP:
            break;
        }

        pFadeData = pFadeData->next;
    }
}

static inline void Update3DSFXListenerPos()
{
    nlVector3 vUp;
    nlVector3 vHeading;
    nlVector3 vCameraPos;

    if (!::gbListenerInit)
    {
        return;
    }

    cBaseCamera* pCamera = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
    vCameraPos = pCamera->GetCameraPosition();
    nlVector3 vDir = { 0.0f, 0.0f, 0.0f };
    cCameraManager::GetViewVector(vHeading);
    cCameraManager::GetUpVector(vUp);

    if (::gbTestPrintout)
    {
        nlPrintf("Listener Pos: %0.2f,%0.2f,%0.2f Heading: %0.2f,%0.2f,%0.2f Up: %0.2f,%0.2f,%0.2f\n",
            vCameraPos.x,
            vCameraPos.y,
            vCameraPos.z,
            vHeading.x,
            vHeading.y,
            vHeading.z,
            vUp.x,
            vUp.y,
            vUp.z);
    }

    PlatAudio::Update3DSFXListener(&gListener, vCameraPos, vDir, vHeading, vUp, 1.0f);
}

/**
 * Offset/Address/Size: 0x1340 | 0x8013D854 | size: 0x25C
 */
void Update3DSFXEmitters()
{
    SFXEmitter* emitter;
    int i;
    float fVol;
    nlVector3 vel;

    for (i = 0; i < 64; i++)
    {
        emitter = PlatAudio::GetSFXEmitter(i);
        if (!emitter)
            continue;

        if (!PlatAudio::IsEmitterActive(emitter))
        {
            if (emitter->pOwner)
            {
                emitter->pOwner = NULL;
            }
            continue;
        }

        if (!emitter->pOwner)
            continue;
        bool stopping = emitter->bIsStopping;
        cGameSFX* pSFXOwner = (cGameSFX*)emitter->pOwner;

        if (!stopping)
        {
            fVol = pSFXOwner->GetSFXVol(emitter->soundType);

            if (emitter->m_unk_0x5F)
            {
                cBaseCamera* pCamera = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
                nlVector3 pos = pCamera->GetCameraPosition();
                nlVector3 camVel = { 0.0f, 0.0f, 0.0f };
                PlatAudio::Update3DSFXEmitter(emitter, pos, camVel, fVol);
            }
            else if (emitter->posUpdateMethod == PHYSOBJ)
            {
                nlVector3 vel = { 0.0f, 0.0f, 0.0f };
                if (emitter->pPhysObj->m_bodyID)
                {
                    nlVector3& linVel = emitter->pPhysObj->GetLinearVelocity();
                    vel = linVel;
                }
                PlatAudio::Update3DSFXEmitter(emitter, emitter->pPhysObj->GetPosition(), vel, fVol);
            }
            else if (emitter->posUpdateMethod == PTRS_TO_VECTORS)
            {
                PlatAudio::Update3DSFXEmitter(emitter, *emitter->pos.pvPos, *emitter->dir.pvDir, fVol);
                if (gbTestPrintout)
                {
                    nlPrintf("3D Sound Type: %d, Pos: %0.2f,%0.2f,%0.2f Heading: %0.2f,%0.2f,%0.2f Up: %0.2f,%0.2f,%0.2f\n",
                        emitter->soundType,
                        emitter->pos.pvPos->x,
                        emitter->pos.pvPos->y,
                        emitter->pos.pvPos->z);
                }
            }
            else if (emitter->posUpdateMethod == VECTORS)
            {
                PlatAudio::Update3DSFXEmitter(emitter, emitter->pos.vPos, emitter->dir.vDir, fVol);
            }
        }
        else
        {
            if (emitter->bKeepTrack)
            {
                switch (pSFXOwner->GetClassType())
                {
                case WORLD:
                    pSFXOwner->StopEmitter(emitter, 0);
                    break;
                case CHAR:
                {
                    cCharacterSFX* pCharSFX = (cCharacterSFX*)pSFXOwner;
                    pCharSFX->Stop((eCharSFX)emitter->soundType, cGameSFX::SFX_STOP_FIRST);
                    break;
                }
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x1320 | 0x8013D834 | size: 0x20
 */
void SetSFXVolume(unsigned long voiceID, float volume)
{
    PlatAudio::SetSFXVolume(voiceID, volume);
}

/**
 * Offset/Address/Size: 0x12D4 | 0x8013D7E8 | size: 0x4C
 */
void SetSFXVolumeGroup(unsigned long uSFXID, int volGroup)
{
    if (volGroup < 0)
    {
        PlatAudio::SetSFXVolumeGroup(uSFXID, 0);
    }
    else if (volGroup > 255)
    {
        PlatAudio::SetSFXVolumeGroup(uSFXID, 255);
    }
    else
    {
        PlatAudio::SetSFXVolumeGroup(uSFXID, (unsigned char)volGroup);
    }
}

/**
 * Offset/Address/Size: 0x1108 | 0x8013D61C | size: 0x1CC
 */
void SetVolGroupVolume(int volGroup, float fVol, int fadeTime)
{
    unsigned char group;
    unsigned short fade;

    volGroup = (volGroup >= 0) ? volGroup : 0;
    volGroup = (volGroup > 255) ? 255 : volGroup;
    group = (unsigned char)volGroup;

    fadeTime = (fadeTime >= 0) ? fadeTime : 0;
    fadeTime = (fadeTime > 65535) ? 65535 : fadeTime;
    fade = (unsigned short)fadeTime;

    if (group == 0x20)
    {
        PlatAudio::SetVolGroupVolume(0x1e, fVol, fade);
        PlatAudio::SetVolGroupVolume(0x1f, fVol, fade);
        PlatAudio::SetVolGroupVolume(0x04, fVol, fade);
    }
    else if (group == 0x04)
    {
        PlatAudio::SetVolGroupVolume(0x04, fVol, fade);
    }
    else
    {
        u8 inRange = ((int)group >= 5 && (int)group <= 0x13);
        if (inRange)
        {
            PlatAudio::SetVolGroupVolume(group, fVol, fade);
        }
        else
        {
            switch (group)
            {
            case 2:
                PlatAudio::SetVolGroupVolume(2, fVol, fade);
                break;
            case 1:
            case 0x1e:
            {
                float vol = fVol;
                if (group == 0x1e)
                    vol = fVol * gfVolumeGroups[2];
                PlatAudio::SetVolGroupVolume(1, 0.9f * vol, fade);
                if (group == 1)
                    return;
            }
            case 3:
            {
                float vol = fVol;
                if (group == 0x1e)
                    vol = fVol * gfVolumeGroups[6];
                PlatAudio::SetVolGroupVolume(3, vol, fade);
                if (group == 3)
                    return;
            }
            }

            if (group != 0x1e)
                PlatAudio::SetVolGroupVolume(group, fVol, fade);
        }
    }
}

/**
 * Offset/Address/Size: 0x1028 | 0x8013D53C | size: 0xE0
 */
void Audio::ActivateFilterOnAllCurrentSFX(bool bOn)
{
    gWorldSFX.ActivateFilterOnAllTrackedSFX(bOn);
    gPowerupSFX.ActivateFilterOnAllTrackedSFX(bOn);
    gStadGenSFX.ActivateFilterOnAllTrackedSFX(bOn);
    gCrowdSFX.ActivateFilterOnAllTrackedSFX(bOn);

    if (g_pGame)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 5; j++)
            {
                cPlayer* pPlayer = pTeam->GetPlayer(j);
                pPlayer->m_pCharacterSFX->ActivateFilterOnAllTrackedSFX(bOn);
            }
        }

        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->ActivateFilterOnAllTrackedSFX(bOn);
    }

    CrowdMood::ActivateLPF(bOn);
    gbFilterOn = bOn;
}

static void SetFilterFreqOnAllCurrentSFX(unsigned short freq)
{
    if (freq > 0x3FFF)
    {
        freq = 0x3FFF;
    }

    gWorldSFX.SetFilterFreqOnAllTrackedSFX(freq);
    gPowerupSFX.SetFilterFreqOnAllTrackedSFX(freq);
    gStadGenSFX.SetFilterFreqOnAllTrackedSFX(freq);
    gCrowdSFX.SetFilterFreqOnAllTrackedSFX(freq);

    if (g_pGame)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 5; j++)
            {
                cPlayer* pPlayer = pTeam->GetPlayer(j);
                pPlayer->m_pCharacterSFX->SetFilterFreqOnAllTrackedSFX(freq);
            }
        }

        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->SetFilterFreqOnAllTrackedSFX(freq);
    }

    CrowdMood::SetLPF(freq);
}

/**
 * Offset/Address/Size: 0xF6C | 0x8013D480 | size: 0xBC
 */
void Audio::SetPitchBendOnAllDialogueSFX(unsigned short pitch)
{
    if (pitch > 0x3FFF)
    {
        pitch = 0x3FFF;
    }
    if (g_pGame != NULL)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 5; j++)
            {
                pTeam->GetPlayer(j)->m_pCharacterSFX->SetPitchBendOnAllDialogueSFX(pitch);
            }
        }
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->SetPitchBendOnAllDialogueSFX(pitch);
    }
    gbPitchBent = (pitch != 0x2000);
}

/**
 * Offset/Address/Size: 0xEE4 | 0x8013D3F8 | size: 0x88
 */
bool ActivateFilterOnSFX(unsigned long uVoiceID, bool bOn)
{
    bool isPlaying;
    if (::g_bAudioInitialized)
    {
        isPlaying = PlatAudio::IsSFXPlaying(uVoiceID);
    }
    else
    {
        isPlaying = false;
    }
    if (!isPlaying)
    {
        return true;
    }
    if (bOn)
    {
        return PlatAudio::SetMIDIControllerVal14Bit(uVoiceID, 0x4F, 0x2000);
    }
    else
    {
        return PlatAudio::SetMIDIControllerVal14Bit(uVoiceID, 0x4F, 0x1FFF);
    }
}

/**
 * Offset/Address/Size: 0xE6C | 0x8013D380 | size: 0x78
 */
bool SetFilterFreqOnSFX(unsigned long uVoiceID, unsigned short freq)
{
    bool isPlaying;
    if (::g_bAudioInitialized)
    {
        isPlaying = PlatAudio::IsSFXPlaying(uVoiceID);
    }
    else
    {
        isPlaying = false;
    }
    if (!isPlaying)
    {
        return true;
    }
    if (freq > 0x3FFF)
    {
        freq = 0x3FFF;
    }
    return PlatAudio::SetFilterFreqOnSFX(uVoiceID, freq);
}

/**
 * Offset/Address/Size: 0xDF4 | 0x8013D308 | size: 0x78
 */
bool SetPitchBendOnSFX(unsigned long uVoiceID, unsigned short pitch)
{
    bool isPlaying;
    if (::g_bAudioInitialized)
    {
        isPlaying = PlatAudio::IsSFXPlaying(uVoiceID);
    }
    else
    {
        isPlaying = false;
    }
    if (!isPlaying)
    {
        return true;
    }
    if (pitch > 0x3FFF)
    {
        pitch = 0x3FFF;
    }
    return PlatAudio::SetPitchBendOnSFX(uVoiceID, pitch);
}

/**
 * Offset/Address/Size: 0xDD4 | 0x8013D2E8 | size: 0x20
 */
void SetOutputMode(MusyXOutputType outputType)
{
    PlatAudio::SetOutputMode(outputType);
}

/**
 * Offset/Address/Size: 0xDCC | 0x8013D2E0 | size: 0x8
 */
bool IsListenerActive()
{
    return gbListenerInit;
}

/**
 * Offset/Address/Size: 0xDC4 | 0x8013D2D8 | size: 0x8
 */
void SetListenerActive(bool active)
{
    gbListenerInit = active;
}

/**
 * Offset/Address/Size: 0xDA4 | 0x8013D2B8 | size: 0x20
 */
SFXEmitter* GetEmitter(unsigned long id)
{
    return PlatAudio::GetSFXEmitter(id);
}

/**
 * Offset/Address/Size: 0xD84 | 0x8013D298 | size: 0x20
 */
SFXEmitter* GetFreeEmitter(unsigned long& id)
{
    return PlatAudio::GetFreeEmitter(id);
}

/**
 * Offset/Address/Size: 0xD64 | 0x8013D278 | size: 0x20
 */
unsigned long Add3DSFXEmitter(const EmitterStartInfo& emitterStartInfo)
{
    return PlatAudio::Add3DSFXEmitter(emitterStartInfo);
}

/**
 * Offset/Address/Size: 0xD44 | 0x8013D258 | size: 0x20
 */
bool Remove3DSFXEmitter(SFXEmitter* emitter)
{
    return PlatAudio::RemoveEmitter(emitter);
}

/**
 * Offset/Address/Size: 0xD24 | 0x8013D238 | size: 0x20
 */
u32 GetEmitterVoiceID(SFXEmitter* emitter)
{
    return PlatAudio::GetEmitterVoiceID(emitter);
}

/**
 * Offset/Address/Size: 0xD04 | 0x8013D218 | size: 0x20
 */
bool IsEmitterActive(SFXEmitter* emitter)
{
    return PlatAudio::IsEmitterActive(emitter);
}

static void RemoveFadeData(FadeType fadeType, int streamIndex)
{
    FadeAudioData* pFadeData = g_pFadeList;
    while (pFadeData != NULL)
    {
        if (pFadeData->fadeType == FADE_TYPE_FILTER || pFadeData->fadeType == FADE_TYPE_FILTER_ALL
            || (pFadeData->fadeType == fadeType && pFadeData->identifier.index == streamIndex))
        {
            nlListRemoveElement<FadeAudioData>(&g_pFadeList, pFadeData, NULL);
            FadeAudioData* pNextFadeAudioData = pFadeData->next;
            delete pFadeData;
            pFadeData = pNextFadeAudioData;
        }
        else
        {
            pFadeData = pFadeData->next;
        }
    }
}

/**
 * Offset/Address/Size: 0xCD8 | 0x8013D1EC | size: 0x2C
 */
void ClearFadeData()
{
    nlDeleteList<FadeAudioData>(&g_pFadeList);
    g_pFadeList = NULL;
}

static bool IsFadeDataInList(FadeType fadeType, unsigned long identifier, bool bIs3D, float targetVol)
{
    FadeAudioData* pFadeData = g_pFadeList;
    while (pFadeData != NULL)
    {
        if (pFadeData->fadeType == fadeType)
        {
            if (fadeType == FADE_TYPE_SFX)
            {
                if (pFadeData->identifier.index == (int)identifier && pFadeData->isEmitter == bIs3D)
                {
                    return true;
                }
            }
            else if ((fadeType == FADE_TYPE_FILTER || fadeType == FADE_TYPE_FILTER_ALL
                         || fadeType == FADE_TYPE_VOLGROUP)
                     && pFadeData->targetVol == targetVol)
            {
                return true;
            }
        }
        pFadeData = pFadeData->next;
    }
    return false;
}

static void FadeSFXVolume(unsigned long identifier, float* currentVol, float fadeToVol, float fadeDuration, bool bIs3D,
    float fadeTimeStart)
{
    float volDiff = fadeToVol - *currentVol;
    float stepSize;
    if (fadeDuration <= 0.0f)
    {
        stepSize = volDiff;
    }
    else
    {
        stepSize = volDiff / fadeDuration;
    }

    AddSFXVolFadeData(identifier, currentVol, stepSize, fadeDuration, fadeToVol, bIs3D, fadeTimeStart);
}

static void FadeSFXVolToZero(
    unsigned long identifier, float* currentVol, float fadeDuration, bool bIs3D, float fadeTimeStart)
{
    float volDiff = 0.0f - *currentVol;
    float stepSize;
    if (fadeDuration <= 0.0f)
    {
        stepSize = volDiff;
    }
    else
    {
        stepSize = volDiff / fadeDuration;
    }

    AddSFXVolFadeData(identifier, currentVol, stepSize, fadeDuration, 0.0f, bIs3D, fadeTimeStart);
}

static void AddSFXVolFadeData(unsigned long identifier, float* currentVol, float fadeStepSize, float fadeDuration,
    float targetVol, bool bIs3D, float fadeTimeStart)
{
    TRANSITION_STATE state = (TRANSITION_STATE)0;
    TransitionTask* pTask = TransitionTask::sm_pGlobalTask;
    if (pTask != NULL)
    {
        state = pTask->m_TransitionState;
    }
    if (state == eTS_Destroying)
    {
        return;
    }

    if (IsFadeDataInList(FADE_TYPE_SFX, identifier, bIs3D, targetVol))
    {
        return;
    }

    FadeAudioData* pFadeData = (FadeAudioData*)nlMalloc(sizeof(FadeAudioData), 8, false);
    pFadeData->fadeType = FADE_TYPE_NONE;
    pFadeData->identifier.index = -1;
    pFadeData->identifier.index = 0;
    pFadeData->fadeStepSize = 0.0f;
    pFadeData->fadeTimeStart = 0.0f;
    pFadeData->fadeDuration = 0.0f;
    pFadeData->targetVol = 0.0f;
    pFadeData->currentVol.floatVal = -1.0f;
    pFadeData->currentVol.floatPtrVal = NULL;
    pFadeData->bShutDownAfterDuration = false;
    pFadeData->isEmitter = false;
    pFadeData->bTurnFilterOn = false;
    pFadeData->bFilterOn = false;
    pFadeData->bPitchBendOn = false;
    pFadeData->bPitchBendApplied = false;
    pFadeData->totalEstimatedTime = -1.0f;

    pFadeData->fadeType = FADE_TYPE_SFX;
    pFadeData->identifier.index = identifier;
    pFadeData->fadeStepSize = fadeStepSize;
    pFadeData->fadeTimeStart = fadeTimeStart;
    pFadeData->fadeDuration = fadeDuration;
    pFadeData->targetVol = targetVol;
    pFadeData->currentVol.floatPtrVal = currentVol;
    pFadeData->bShutDownAfterDuration = targetVol == 0.0f;
    pFadeData->isEmitter = bIs3D;
    pFadeData->next = NULL;
    nlListAddStart<FadeAudioData>(&g_pFadeList, pFadeData, NULL);
}

static inline FadeAudioData* FindFadeData(FadeType fadeType, float targetVol)
{
    FadeAudioData* pFadeData = g_pFadeList;
    while (pFadeData != NULL)
    {
        if (pFadeData->fadeType == fadeType)
        {
            if (pFadeData->targetVol == targetVol)
            {
                return pFadeData;
            }
        }
        pFadeData = pFadeData->next;
    }
    return NULL;
}

static inline void AddFilterFadeData(float currentVal, float fadeStepSize, float fadeDuration, float targetVal,
    float fadeTimeStart)
{
    TransitionTask* pTask = TransitionTask::sm_pGlobalTask;
    TRANSITION_STATE state;
    if (pTask != NULL)
    {
        state = pTask->m_TransitionState;
    }
    else
    {
        state = (TRANSITION_STATE)0;
    }
    if (state == eTS_Destroying)
    {
        return;
    }

    FadeAudioData* existing = FindFadeData(FADE_TYPE_FILTER, targetVal);
    if (existing != NULL)
    {
        nlListRemoveElement<FadeAudioData>(&g_pFadeList, existing, NULL);
        delete existing;
    }

    FadeAudioData* newFade = (FadeAudioData*)nlMalloc(sizeof(FadeAudioData), 8, false);

    newFade->fadeType = FADE_TYPE_NONE;
    newFade->identifier.index = -1;
    newFade->identifier.index = 0;
    newFade->fadeStepSize = 0.0f;
    newFade->fadeTimeStart = 0.0f;
    newFade->fadeDuration = 0.0f;
    newFade->targetVol = 0.0f;
    newFade->currentVol.floatVal = -1.0f;
    newFade->currentVol.floatPtrVal = NULL;
    newFade->bShutDownAfterDuration = false;
    newFade->isEmitter = false;
    newFade->bTurnFilterOn = false;
    newFade->bFilterOn = false;
    newFade->bPitchBendOn = false;
    newFade->bPitchBendApplied = false;
    newFade->totalEstimatedTime = -1.0f;

    newFade->fadeType = FADE_TYPE_FILTER;
    newFade->fadeStepSize = fadeStepSize;
    newFade->fadeTimeStart = fadeTimeStart;
    newFade->fadeDuration = fadeDuration;
    newFade->targetVol = targetVal;
    newFade->currentVol.floatVal = currentVal;

    if (0.0f == targetVal)
    {
        newFade->bTurnFilterOn = false;
        newFade->bShutDownAfterDuration = true;
    }
    else
    {
        newFade->bTurnFilterOn = true;
        newFade->bShutDownAfterDuration = false;
    }

    newFade->next = NULL;
    nlListAddStart<FadeAudioData>(&g_pFadeList, newFade, NULL);
}

/**
 * Offset/Address/Size: 0x7C8 | 0x8013CCDC | size: 0x510
 */
void FadeFilter(float currentVal, float fadeToVal, float fadeDuration, float fadeTimeStart)
{
    TransitionTask* pTask = TransitionTask::sm_pGlobalTask;
    TRANSITION_STATE state;
    if (pTask != NULL)
    {
        state = pTask->m_TransitionState;
    }
    else
    {
        state = (TRANSITION_STATE)0;
    }
    if (state == eTS_Destroying)
    {
        return;
    }

    float volDiff = fadeToVal - currentVal;
    float stepSize;
    if (fadeDuration <= 0.0f)
    {
        stepSize = volDiff;
        if (0.0f == fadeTimeStart)
        {
            if (0.0f == fadeToVal)
            {
                if (!gbFilterOn)
                {
                    return;
                }
                ActivateFilterOnAllCurrentSFXInl(false);
                SetFilterFreqOnAllCurrentSFXInl(0);
            }
            else
            {
                if (gbFilterOn)
                {
                    return;
                }
                ActivateFilterOnAllCurrentSFXInl(true);
                SetFilterFreqOnAllCurrentSFXInl(0x3FFF);
            }
            return;
        }
    }
    else
    {
        stepSize = volDiff / fadeDuration;
    }
    AddFilterFadeData(currentVal, stepSize, fadeDuration, fadeToVal, fadeTimeStart);
}

static inline unsigned short ApplyDialoguePitchFromTweaks(cGame* pGame)
{
    unsigned short pitchBend;

    pitchBend = 8192.0f * pGame->m_pGameTweaks->fFadePitchMin;
    if (pitchBend > 0x3FFF)
    {
        pitchBend = 0x3FFF;
    }
    if (pGame != NULL)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 5; j++)
            {
                pTeam->GetPlayer(j)->m_pCharacterSFX->SetPitchBendOnAllDialogueSFX(pitchBend);
            }
        }
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->SetPitchBendOnAllDialogueSFX(pitchBend);
    }
    return pitchBend;
}

static inline void AddPitchBendFadeData(float currentVal, float fadeStepSize, float fadeDuration, float targetVal,
    float fadeTimeStart)
{
    TransitionTask* pTask = TransitionTask::sm_pGlobalTask;
    TRANSITION_STATE state;
    if (pTask != NULL)
    {
        state = pTask->m_TransitionState;
    }
    else
    {
        state = (TRANSITION_STATE)0;
    }
    if (state == eTS_Destroying)
    {
        return;
    }

    FadeAudioData* existing = FindFadeData(FADE_TYPE_FILTER_ALL, targetVal);
    if (existing != NULL)
    {
        nlListRemoveElement<FadeAudioData>(&g_pFadeList, existing, NULL);
        delete existing;
    }

    FadeAudioData* newFade = (FadeAudioData*)nlMalloc(sizeof(FadeAudioData), 8, false);

    newFade->fadeType = FADE_TYPE_NONE;
    newFade->identifier.index = -1;
    newFade->identifier.index = 0;
    newFade->fadeStepSize = 0.0f;
    newFade->fadeTimeStart = 0.0f;
    newFade->fadeDuration = 0.0f;
    newFade->targetVol = 0.0f;
    newFade->currentVol.floatVal = -1.0f;
    newFade->currentVol.floatPtrVal = NULL;
    newFade->bShutDownAfterDuration = false;
    newFade->isEmitter = false;
    newFade->bTurnFilterOn = false;
    newFade->bFilterOn = false;
    newFade->bPitchBendOn = false;
    newFade->bPitchBendApplied = false;
    newFade->totalEstimatedTime = -1.0f;

    newFade->fadeType = FADE_TYPE_FILTER_ALL;
    newFade->fadeStepSize = fadeStepSize;
    newFade->fadeTimeStart = fadeTimeStart;
    newFade->fadeDuration = fadeDuration;
    newFade->targetVol = targetVal;
    newFade->currentVol.floatVal = currentVal;

    if (targetVal == g_pGame->m_pGameTweaks->fFadePitchMax)
    {
        newFade->bPitchBendOn = false;
        newFade->bShutDownAfterDuration = true;
    }
    else
    {
        newFade->bPitchBendOn = true;
        newFade->bShutDownAfterDuration = false;
    }

    newFade->next = NULL;
    nlListAddStart<FadeAudioData>(&g_pFadeList, newFade, NULL);
}

/**
 * Offset/Address/Size: 0x474 | 0x8013C988 | size: 0x354
 */
void PitchBend(float currentVal, float fadeToVal, float fadeDuration, float fadeTimeStart)
{
    TransitionTask* pTask = TransitionTask::sm_pGlobalTask;
    TRANSITION_STATE state;
    if (pTask != NULL)
    {
        state = pTask->m_TransitionState;
    }
    else
    {
        state = (TRANSITION_STATE)0;
    }
    if (state == eTS_Destroying)
    {
        return;
    }

    float diff = fadeToVal - currentVal;
    float fadePerFrame;
    if (fadeDuration <= 0.0f)
    {
        fadePerFrame = diff;
        if (0.0f == fadeTimeStart)
        {
            if (1.0f == fadeToVal)
            {
                if (!gbPitchBent)
                {
                    return;
                }
                if (g_pGame != NULL)
                {
                    SetPitchBendOnAllDialogueSFXInl(0x2000);
                }
                gbPitchBent = false;
            }
            else
            {
                if (gbPitchBent)
                {
                    return;
                }
                cGame* pGame = g_pGame;
                int pitchBend = ApplyDialoguePitchFromTweaks(pGame);
                if ((unsigned short)pitchBend != 0x2000)
                {
                    gbPitchBent = true;
                }
                else
                {
                    gbPitchBent = false;
                }
            }
            return;
        }
    }
    else
    {
        fadePerFrame = diff / fadeDuration;
    }
    AddPitchBendFadeData(currentVal, fadePerFrame, fadeDuration, fadeToVal, fadeTimeStart);
}

/**
 * Offset/Address/Size: 0x3E0 | 0x8013C8F4 | size: 0x94
 */
void FadeFilterToFullStrength()
{
    if (!gbFilterOn)
    {
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        FadeFilter(tweaks->fFadeFilterFreqMin, tweaks->fFadeFilterFreqMax, tweaks->fFadeFilterSlowMoInTime, 0.0f);
    }

    if (!gbPitchBent)
    {
        f32 t = (f32)gWorldSFX.GetGroupPitch() / 8192.0f;
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        PitchBend(t, tweaks->fFadePitchMin, tweaks->fFadeFilterSlowMoInTime, 0.0f);
    }
}

/**
 * Offset/Address/Size: 0x240 | 0x8013C754 | size: 0x1A0
 */
void FadeFilterFromCurrentToZero()
{
    FadeAudioData* node;

    f32 t = (f32)gWorldSFX.GetGroupFilterFreq() / 16383.0f;
    if (t > 0.0f)
    {
        node = g_pFadeList;
        while (node != NULL)
        {
            FadeType type = node->fadeType;
            if (!(type != FADE_TYPE_FILTER && type != FADE_TYPE_FILTER_ALL
                    && !(type == FADE_TYPE_FILTER && node->identifier.index == 0)))
            {
                nlListRemoveElement<FadeAudioData>(&g_pFadeList, node, NULL);
                FadeAudioData* next = node->next;
                delete node;
                node = next;
            }
            else
            {
                node = node->next;
            }
        }
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        FadeFilter(t, tweaks->fFadeFilterFreqMin, tweaks->fFadeFilterSlowMoOutTime, 0.0f);
    }

    if (gbPitchBent)
    {
        t = (f32)gWorldSFX.GetGroupPitch() / 8192.0f;
        node = g_pFadeList;
        while (node != NULL)
        {
            FadeType type = node->fadeType;
            if (!(type != FADE_TYPE_FILTER && type != FADE_TYPE_FILTER_ALL
                    && !(type == FADE_TYPE_FILTER_ALL && node->identifier.index == 0)))
            {
                nlListRemoveElement<FadeAudioData>(&g_pFadeList, node, NULL);
                FadeAudioData* next = node->next;
                delete node;
                node = next;
            }
            else
            {
                node = node->next;
            }
        }
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        PitchBend(t, tweaks->fFadePitchMax, tweaks->fFadeFilterSlowMoOutTime, 0.0f);
    }
}

static void AddFilterFadeData(float currentVal, float fadeStepSize, float fadeDuration, float targetVal,
    FadeType fadeType, float fadeTimeStart)
{
    TRANSITION_STATE state = (TRANSITION_STATE)0;
    TransitionTask* pTask = TransitionTask::sm_pGlobalTask;
    if (pTask != NULL)
    {
        state = pTask->m_TransitionState;
    }
    if (state == eTS_Destroying)
    {
        return;
    }

    if (IsFadeDataInList(fadeType, 0, false, targetVal))
    {
        return;
    }

    FadeAudioData* pFadeData = (FadeAudioData*)nlMalloc(sizeof(FadeAudioData), 8, false);
    pFadeData->fadeType = FADE_TYPE_NONE;
    pFadeData->identifier.index = -1;
    pFadeData->identifier.index = 0;
    pFadeData->fadeStepSize = 0.0f;
    pFadeData->fadeTimeStart = 0.0f;
    pFadeData->fadeDuration = 0.0f;
    pFadeData->targetVol = 0.0f;
    pFadeData->currentVol.floatVal = -1.0f;
    pFadeData->currentVol.floatPtrVal = NULL;
    pFadeData->bShutDownAfterDuration = false;
    pFadeData->isEmitter = false;
    pFadeData->bTurnFilterOn = false;
    pFadeData->bFilterOn = false;
    pFadeData->bPitchBendOn = false;
    pFadeData->bPitchBendApplied = false;
    pFadeData->totalEstimatedTime = -1.0f;

    pFadeData->fadeType = fadeType;
    pFadeData->fadeStepSize = fadeStepSize;
    pFadeData->fadeTimeStart = fadeTimeStart;
    pFadeData->fadeDuration = fadeDuration;
    pFadeData->targetVol = targetVal;
    pFadeData->currentVol.floatVal = currentVal;

    if (fadeType == FADE_TYPE_FILTER)
    {
        if (targetVal == 0.0f)
        {
            pFadeData->bTurnFilterOn = false;
            pFadeData->bShutDownAfterDuration = true;
        }
        else
        {
            pFadeData->bTurnFilterOn = true;
            pFadeData->bShutDownAfterDuration = false;
        }
    }
    else if (fadeType == FADE_TYPE_FILTER_ALL)
    {
        if (targetVal == g_pGame->m_pGameTweaks->fFadePitchMax)
        {
            pFadeData->bPitchBendOn = false;
            pFadeData->bShutDownAfterDuration = true;
        }
        else
        {
            pFadeData->bPitchBendOn = true;
            pFadeData->bShutDownAfterDuration = false;
        }
    }

    pFadeData->next = NULL;
    nlListAddStart<FadeAudioData>(&g_pFadeList, pFadeData, NULL);
}

/**
 * Offset/Address/Size: 0x22C | 0x8013C740 | size: 0x14
 */
float Audio::MasterVolume::GetVolume(MasterVolume::VOLUME_GROUP group)
{
    return gfVolumeGroups[group];
}

/**
 * Offset/Address/Size: 0x218 | 0x8013C72C | size: 0x14
 */
void Audio::MasterVolume::SetVolume(MasterVolume::VOLUME_GROUP group, float volume)
{
    gfVolumeGroups[group] = volume;
}

/**
 * Offset/Address/Size: 0x10 | 0x8013C524 | size: 0x208
 */
void Audio::MasterVolume::SetVoiceVolume(float volume, int time)
{
    Audio::SetVolGroupVolume(5, volume * gfVolumeGroups[8], time);
    Audio::SetVolGroupVolume(6, volume * gfVolumeGroups[9], time);
    Audio::SetVolGroupVolume(7, volume * gfVolumeGroups[10], time);
    Audio::SetVolGroupVolume(8, volume * gfVolumeGroups[11], time);
    Audio::SetVolGroupVolume(9, volume * gfVolumeGroups[12], time);
    Audio::SetVolGroupVolume(10, volume * gfVolumeGroups[13], time);
    Audio::SetVolGroupVolume(11, volume * gfVolumeGroups[14], time);
    Audio::SetVolGroupVolume(12, volume * gfVolumeGroups[15], time);
    Audio::SetVolGroupVolume(13, volume * gfVolumeGroups[16], time);
    Audio::SetVolGroupVolume(14, volume * gfVolumeGroups[17], time);
    Audio::SetVolGroupVolume(15, volume * gfVolumeGroups[18], time);
    Audio::SetVolGroupVolume(16, volume * gfVolumeGroups[19], time);
    Audio::SetVolGroupVolume(17, volume * gfVolumeGroups[20], time);
    Audio::SetVolGroupVolume(18, volume * gfVolumeGroups[21], time);
    Audio::SetVolGroupVolume(19, volume * gfVolumeGroups[22], time);
    Audio::SetVolGroupVolume(4, volume * gfVolumeGroups[7], time);
    gfVolumeGroups[3] = volume;
}

/**
 * Offset/Address/Size: 0x0 | 0x8013C514 | size: 0x10
 */
float Audio::MasterVolume::GetVoiceVolume()
{
    return gfVolumeGroups[3];
}

// void nlBSearch<nlSortedSlot<AudioStreamTrack::StreamTrack, 3>::EntryLookup<AudioStreamTrack::StreamTrack>, unsigned long>(const unsigned
// long&, nlSortedSlot<AudioStreamTrack::StreamTrack, 3>::EntryLookup<AudioStreamTrack::StreamTrack>*, int)
// {
// }

// void nlDeleteList<FadeAudioData>(FadeAudioData**)
// {
// }

// void nlListRemoveElement<FadeAudioData>(FadeAudioData**, FadeAudioData*, FadeAudioData**)
// {
// }

// void nlWalkDLRing<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
// DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*))
// {
// }

// void nlWalkDLRing<DLListEntry<GCAudioStreaming::StereoAudioStream*>, DLListContainerBase<GCAudioStreaming::StereoAudioStream*,
// BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>>(DLListEntry<GCAudioStreaming::StereoAudioStream*>*,
// DLListContainerBase<GCAudioStreaming::StereoAudioStream*, BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>*, void
// (DLListContainerBase<GCAudioStreaming::StereoAudioStream*,
// BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>::*)(DLListEntry<GCAudioStreaming::StereoAudioStream*>*))
// {
// }

// void nlWalkDLRing<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>,
// DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
// nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>,
// 4>>>(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*, DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
// nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>, 4>>*, void
// (DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
// nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>,
// 4>>::*)(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*))
// {
// }

// void nlWalkDLRing<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// AudioStreamTrack::TrackManagerBase::FadeManager>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
// WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>, AudioStreamTrack::TrackManagerBase::FadeManager>*, void
// (WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// AudioStreamTrack::TrackManagerBase::FadeManager>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*))
// {
// }

// void
// nlDLRingGetStart<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>>(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*)
// {
// }

// DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*
// nlDLRingGetStart<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>(
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*)
// {
// }

// void AudioStreamTrack::TrackManager<3>::Update(float)
// {
// }

// void nlWalkRing<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
// DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>*, void
// (DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*))
// {
// }

// void nlWalkRing<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// AudioStreamTrack::TrackManagerBase::FadeManager>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
// WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>, AudioStreamTrack::TrackManagerBase::FadeManager>*, void
// (WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
// DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
// AudioStreamTrack::TrackManagerBase::FadeManager>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*))
// {
// }

} // namespace Audio
