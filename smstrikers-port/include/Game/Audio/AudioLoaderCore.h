#ifndef _AUDIOLOADERCORE_H_
#define _AUDIOLOADERCORE_H_

#include "types.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/FE/FEAudio.h"
#include "Game/GameAudio.h"
#include "Game/DB/BasicGameInfo.h"
#include "Game/Loader.h"
#include "Game/Render/Bowser.h"
#include "Game/Sys/audio.h"
#include "Game/Sys/GCStream.h"
#include "NL/nlAVLTree.h"
#include "NL/nlConfig.h"

extern Config g_FEStreamConfig;
extern AudioStreamTrack::TrackManagerBase* g_pTrackManager;

class SoundPropAccessor;

enum LoadType
{
    SND_GROUP_LOAD_NOT_LOADED = 0,
    SND_GROUP_LOAD_FROM_SAMPLE_FILE_IN_MEM = 1,
    SND_GROUP_LOAD_FROM_DISC = 2,
};

struct SndGroupData
{
    /* 0x00 */ const char* szGroupName;
    /* 0x04 */ unsigned short groupID;
    /* 0x08 */ int stackEnum;
    /* 0x0C */ int uLoadOrder;
    /* 0x10 */ LoadType loadType;
}; // total size: 0x14

extern SndGroupData sebringAudioGroups[47];

struct AudioFileData
{
    /* 0x00 */ char* szPoolFile;
    /* 0x04 */ char* szSampleFile;
    /* 0x08 */ char* szProjectFile;
    /* 0x0C */ char* szDirFile;
    /* 0x10 */ unsigned char* pool_buffer;
    /* 0x14 */ unsigned char* proj_buffer;
    /* 0x18 */ unsigned char* sdir_buffer;
    /* 0x1C */ unsigned char* samp_buffer;
    /* 0x20 */ SndGroupData* soundGroups;
    /* 0x24 */ int numSoundGroups;
}; // total size: 0x28

class LoadingManager;
struct SoundStrToIDNode;
class Bowser;

class AudioLoader : public Loader
{
public:
    virtual bool StartLoad(LoadingManager* manager);

    static void InitCrowdFromStateTransition();
    static void ResetForRematch();
    static void ResetForNewGame();
    static void ReadEntireSampleFileIntoMem(bool sync);
    static unsigned char UnloadInGameAudioData();
    static bool LoadInGameAudioData();
    static unsigned char LoadStadiumSpecificSoundGroups(eStadiumID stadiumID);
    static void SetupBowserStadiumSoundTable(Bowser* bowser);
    static void SetupCharStadiumSoundTable();
    static void SetupInGameListener();
    static void SetupPostPhysicsCameraLoad();
    static void UnloadInGame();
    static void LoadInGame();
    static void LoadPermanentSoundGroups(bool bLoadEntireSampleFileIntoMem);
    static void LoadFEButtonSoundGroup();
    static void LoadNLGDialogueGroup(bool bAsync);
    static void LoadNintendoDialogueGroup(bool bAsync);
    static void LoadFEAudioData(bool bAsync);
    static void UnloadFE();
    static void LoadFE(bool bLoadSampleFile);
    static void SetupSoundBuffers();
    static void StopStreaming();
    static void StopPauseMenuMusic();
    static void PlayPauseMenuMusic();
    static void PlayLoadLoopMusic();
    static void PlayFEMenuMusic();
    static void PlayFETitleMusicWithFade();
    static void StartFEStream(const char* pSoundName, bool bLoop, const char* pTrackName);
    static bool IsInited();
    static unsigned char IsSoundGroupLoaded(int, unsigned long);
    static bool Initialize();
    static bool ActivateDPL2(bool bEnableDPL2, bool bLoadSampleFile);
    static void SetupSoundGroups();
    static void DeleteStrToIDTables();
    static unsigned long GetWorldSFXTypeFromStr(const char* str);
    static unsigned long GetCharSFXTypeFromStr(const char* str);
    static unsigned long GetSFXIDFromStr(const char* str, SoundStrToIDNode** ppNode);
    static void SetupWorldSoundTypesAVLTree();
    static void SetupCharSoundTypesAVLTree();
    static void SetupSoundDefinesAVLTree();

    static bool gbDisableAudio;
    static bool gbStream;
    static bool g_BGM_Off;
    static bool gbDisableCrowd;
    static bool gbDisableReverb;
    static bool gReverbOn;
    static AudioFileData sebringAudioFileData;
    static nlAVLTreeSlotPool<int, SoundStrToIDNode*, DefaultKeyCompare<int> > gMusyXSoundDefineMap;
    static nlAVLTreeSlotPool<int, SoundStrToIDNode*, DefaultKeyCompare<int> > gCharSoundDefineMap;
    static nlAVLTreeSlotPool<int, SoundStrToIDNode*, DefaultKeyCompare<int> > gWorldSoundDefineMap;
    static int AUDIO_TITLE_STREAM;
    static int AUDIO_CUP_MODE_STREAM;
    static int AUDIO_FE_OPTIONS_STREAM;
    static int AUDIO_FE_STINGER_STREAM;
    static int AUDIO_LOAD_LOOP_STREAM;
    static int AUDIO_PAUSE_STREAM;
    static int AUDIO_END_GAME_STREAM;
    static int AUDIO_GAZETTE_POS_STREAM;
    static int AUDIO_GAZETTE_INTRO_STREAM;
    static int AUDIO_GAZETTE_NEG_STREAM;
    static int AUDIO_HOME_GOAL_STREAM;
    static int AUDIO_TIME_WARNING_STREAM;
    static int AUDIO_HORN_GOAL_STREAM;
};

inline unsigned char AudioLoader::IsSoundGroupLoaded(int groupEnum, unsigned long)
{
    bool bAlreadyLoaded;
    if (groupEnum < 0)
    {
        bAlreadyLoaded = false;
    }
    else
    {
        bAlreadyLoaded = false;
        if (sebringAudioGroups[groupEnum].uLoadOrder > -1 && sebringAudioGroups[groupEnum].stackEnum > -1)
        {
            bAlreadyLoaded = true;
        }
    }

    return bAlreadyLoaded;
}

#endif // _AUDIOLOADERCORE_H_
