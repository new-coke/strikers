#include "NL/nlAVLTree.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Audio/PriorityStream.h"
#include "Game/Audio/SebringSoundDefines.h"
#include "Game/Audio/SoundEventScript.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Game.h"
#include "Game/Sys/PlatStream.h"
#include "Game/Sys/debug.h"
#include "NL/nlFileGC.h"
#include "dolphin/arq.h"

int nlSNPrintf(char*, unsigned long, const char*, ...);

bool AudioLoader::gbStream = true;

static int gLoadedStadiumGroup = -1;
static int gLoadedHomeCaptainGroup = -1;
static int gLoadedAwayCaptainGroup = -1;
static int gLoadedHomeSidekickGroup = -1;
static int gLoadedAwaySidekickGroup = -1;
static int gLoadedSurfaceGroup = -1;

class GameSceneManager;

AudioLoader TheAudioLoader;

bool AudioLoader::gbDisableAudio;
bool AudioLoader::g_BGM_Off;
bool AudioLoader::gbDisableCrowd;
bool AudioLoader::gbDisableReverb;
bool AudioLoader::gReverbOn;
static bool gbAsyncLoadEntireSampleFileIntoMemRequestMade;

Config g_FEStreamConfig(Config::ALLOCATE_HIGH);

extern SoundPropAccessor* gpBIRDOGRASSSoundPropAccessor;
extern SoundPropAccessor* gpBIRDOMETALSoundPropAccessor;
extern SoundPropAccessor* gpBIRDOCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpBIRDORUBBERSoundPropAccessor;
extern SoundPropAccessor* gpBIRDOWOODSoundPropAccessor;
extern SoundPropAccessor* gpDAISYGRASSSoundPropAccessor;
extern SoundPropAccessor* gpDAISYMETALSoundPropAccessor;
extern SoundPropAccessor* gpDAISYCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpDAISYRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpDAISYWOODSoundPropAccessor;
extern SoundPropAccessor* gpDKGRASSSoundPropAccessor;
extern SoundPropAccessor* gpDKMETALSoundPropAccessor;
extern SoundPropAccessor* gpDKCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpDKRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpDKWOODSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSGRASSSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSMETALSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSWOODSoundPropAccessor;
extern SoundPropAccessor* gpKOOPAGRASSSoundPropAccessor;
extern SoundPropAccessor* gpKOOPAMETALSoundPropAccessor;
extern SoundPropAccessor* gpKOOPACONCRETESoundPropAccessor;
extern SoundPropAccessor* gpKOOPARUBBERSoundPropAccessor;
extern SoundPropAccessor* gpKOOPAWOODSoundPropAccessor;
extern SoundPropAccessor* gpLUIGIGRASSSoundPropAccessor;
extern SoundPropAccessor* gpLUIGIMETALSoundPropAccessor;
extern SoundPropAccessor* gpLUIGICONCRETESoundPropAccessor;
extern SoundPropAccessor* gpLUIGIRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpLUIGIWOODSoundPropAccessor;
extern SoundPropAccessor* gpMARIOGRASSSoundPropAccessor;
extern SoundPropAccessor* gpMARIOMETALSoundPropAccessor;
extern SoundPropAccessor* gpMARIOCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpMARIORUBBERSoundPropAccessor;
extern SoundPropAccessor* gpMARIOWOODSoundPropAccessor;
extern SoundPropAccessor* gpPEACHGRASSSoundPropAccessor;
extern SoundPropAccessor* gpPEACHMETALSoundPropAccessor;
extern SoundPropAccessor* gpPEACHCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpPEACHRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpPEACHWOODSoundPropAccessor;
extern SoundPropAccessor* gpTOADGRASSSoundPropAccessor;
extern SoundPropAccessor* gpTOADMETALSoundPropAccessor;
extern SoundPropAccessor* gpTOADCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpTOADRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpTOADWOODSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIGRASSSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIMETALSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGICONCRETESoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIWOODSoundPropAccessor;
extern SoundPropAccessor* gpWARIOGRASSSoundPropAccessor;
extern SoundPropAccessor* gpWARIOMETALSoundPropAccessor;
extern SoundPropAccessor* gpWARIOCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpWARIORUBBERSoundPropAccessor;
extern SoundPropAccessor* gpWARIOWOODSoundPropAccessor;
extern SoundPropAccessor* gpYOSHIGRASSSoundPropAccessor;
extern SoundPropAccessor* gpYOSHIMETALSoundPropAccessor;
extern SoundPropAccessor* gpYOSHICONCRETESoundPropAccessor;
extern SoundPropAccessor* gpYOSHIRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpYOSHIWOODSoundPropAccessor;
extern SoundPropAccessor* gpSUPERGRASSSoundPropAccessor;
extern SoundPropAccessor* gpSUPERMETALSoundPropAccessor;
extern SoundPropAccessor* gpSUPERCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpSUPERRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpSUPERWOODSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERGRASSSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERMETALSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpBOWSERRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERWOODSoundPropAccessor;

SndGroupData sebringAudioGroups[47] = {
    { "GRPFE_Main_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPFE_Char_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPFE_Button_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPFE_Char_Nintendo_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPFE_Char_NLG_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPCrowd_Gen_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Gen_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Palace_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Pipeline_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Underground_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Konga_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Yoshi_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Super_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Mystery_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Grass_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Metal_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Concrete_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Rubber_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPStad_Wood_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Gen_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Critter_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Daisy_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_DK_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Luigi_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_MARIO_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Peach_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Waluigi_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Wario_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Yoshi_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Mystery_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Birdo_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Koopa_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Toad_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Ham_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPChar_Bowser_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Gen_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Bana_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Bomb_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Mush_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Shell_Freeze_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Shell_Green_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Shell_Red_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Shell_Spiny_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Star_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPPower_Up_Chain_Chomp_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "GRPHUD_In_Game_FE_SFX", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
    { "", 0xFFFF, -1, -1, SND_GROUP_LOAD_NOT_LOADED },
};

SoundPropAccessor* surfaceSoundPropTables[14][5] = {
    { gpBIRDOGRASSSoundPropAccessor, gpBIRDOMETALSoundPropAccessor, gpBIRDOCONCRETESoundPropAccessor, gpBIRDORUBBERSoundPropAccessor, gpBIRDOWOODSoundPropAccessor },
    { gpDAISYGRASSSoundPropAccessor, gpDAISYMETALSoundPropAccessor, gpDAISYCONCRETESoundPropAccessor, gpDAISYRUBBERSoundPropAccessor, gpDAISYWOODSoundPropAccessor },
    { gpDKGRASSSoundPropAccessor, gpDKMETALSoundPropAccessor, gpDKCONCRETESoundPropAccessor, gpDKRUBBERSoundPropAccessor, gpDKWOODSoundPropAccessor },
    { gpHAMBROSGRASSSoundPropAccessor, gpHAMBROSMETALSoundPropAccessor, gpHAMBROSCONCRETESoundPropAccessor, gpHAMBROSRUBBERSoundPropAccessor, gpHAMBROSWOODSoundPropAccessor },
    { gpKOOPAGRASSSoundPropAccessor, gpKOOPAMETALSoundPropAccessor, gpKOOPACONCRETESoundPropAccessor, gpKOOPARUBBERSoundPropAccessor, gpKOOPAWOODSoundPropAccessor },
    { gpLUIGIGRASSSoundPropAccessor, gpLUIGIMETALSoundPropAccessor, gpLUIGICONCRETESoundPropAccessor, gpLUIGIRUBBERSoundPropAccessor, gpLUIGIWOODSoundPropAccessor },
    { gpMARIOGRASSSoundPropAccessor, gpMARIOMETALSoundPropAccessor, gpMARIOCONCRETESoundPropAccessor, gpMARIORUBBERSoundPropAccessor, gpMARIOWOODSoundPropAccessor },
    { gpPEACHGRASSSoundPropAccessor, gpPEACHMETALSoundPropAccessor, gpPEACHCONCRETESoundPropAccessor, gpPEACHRUBBERSoundPropAccessor, gpPEACHWOODSoundPropAccessor },
    { gpTOADGRASSSoundPropAccessor, gpTOADMETALSoundPropAccessor, gpTOADCONCRETESoundPropAccessor, gpTOADRUBBERSoundPropAccessor, gpTOADWOODSoundPropAccessor },
    { gpWALUIGIGRASSSoundPropAccessor, gpWALUIGIMETALSoundPropAccessor, gpWALUIGICONCRETESoundPropAccessor, gpWALUIGIRUBBERSoundPropAccessor, gpWALUIGIWOODSoundPropAccessor },
    { gpWARIOGRASSSoundPropAccessor, gpWARIOMETALSoundPropAccessor, gpWARIOCONCRETESoundPropAccessor, gpWARIORUBBERSoundPropAccessor, gpWARIOWOODSoundPropAccessor },
    { gpYOSHIGRASSSoundPropAccessor, gpYOSHIMETALSoundPropAccessor, gpYOSHICONCRETESoundPropAccessor, gpYOSHIRUBBERSoundPropAccessor, gpYOSHIWOODSoundPropAccessor },
    { gpSUPERGRASSSoundPropAccessor, gpSUPERMETALSoundPropAccessor, gpSUPERCONCRETESoundPropAccessor, gpSUPERRUBBERSoundPropAccessor, gpSUPERWOODSoundPropAccessor },
    { gpBOWSERGRASSSoundPropAccessor, gpBOWSERMETALSoundPropAccessor, gpBOWSERCONCRETESoundPropAccessor, gpBOWSERRUBBERSoundPropAccessor, gpBOWSERWOODSoundPropAccessor },
};

AudioFileData AudioLoader::sebringAudioFileData = {
    "/audio/data/sebring.pool",
    "/audio/data/sebring.samp",
    "/audio/data/sebring.proj",
    "/audio/data/sebring.sdir",
    NULL,
    NULL,
    NULL,
    NULL,
    sebringAudioGroups,
    0x2E,
};

nlAVLTreeSlotPool<int, SoundStrToIDNode*, DefaultKeyCompare<int> > AudioLoader::gMusyXSoundDefineMap(0x438, 0);
nlAVLTreeSlotPool<int, SoundStrToIDNode*, DefaultKeyCompare<int> > AudioLoader::gCharSoundDefineMap(0xAF, 0);
nlAVLTreeSlotPool<int, SoundStrToIDNode*, DefaultKeyCompare<int> > AudioLoader::gWorldSoundDefineMap(0xDC, 0);

/**
 * Offset/Address/Size: 0x3F48 | 0x80147D14 | size: 0x14C
 */
void AudioLoader::SetupSoundDefinesAVLTree()
{
    unsigned long numSoundDefines = GetNumSoundDefines();

    for (int i = 0; i < numSoundDefines; i++)
    {
        SoundStrToIDNode* pNode = (SoundStrToIDNode*)nlMalloc(sizeof(SoundStrToIDNode), 8, false);
        SoundStrToIDNode* newNode = pNode;
        pNode->typeID = -1;
        pNode->typeStr = NULL;
        pNode->musyxStr = NULL;
        pNode->musyxID = -1;
        pNode->fVolume = 100.0f;
        pNode->fDelay = -1.0f;
        pNode->fVolReverb = 100.0f;
        pNode->volGrp = -1;
        pNode->sfxPriority = 0;
        pNode->uHashVal = 0;
        pNode->pSoundPropAccessor = NULL;
        pNode->bSoundPropTableReloaded = 0;
        pNode->pSoundProp = NULL;
        pNode->pOwner = NULL;
        pNode->lastVoiceID = -1;
        pNode->pLastEmitter = NULL;
        pNode->m_unk_0x40 = false;

        newNode->musyxID = SebringSoundDefines[i].musyxID;
        newNode->musyxStr = SebringSoundDefines[i].musyxStr;
        newNode->uHashVal = nlStringLowerHash(newNode->musyxStr);

        gMusyXSoundDefineMap.Add(newNode->uHashVal, newNode);
    }
}

namespace Audio
{
extern const char* gCharSoundTable[];
}

/**
 * Offset/Address/Size: 0x3DF4 | 0x80147BC0 | size: 0x154
 */
void AudioLoader::SetupCharSoundTypesAVLTree()
{
    const char* const* pSoundTable = Audio::gCharSoundTable;
    int i;

    i = 0;

    do
    {
        SoundStrToIDNode* pNode = (SoundStrToIDNode*)nlMalloc(sizeof(SoundStrToIDNode), 8, false);
        SoundStrToIDNode* newNode = pNode;
        pNode->typeID = -1;
        pNode->typeStr = NULL;
        pNode->musyxStr = NULL;
        pNode->musyxID = -1;
        pNode->fVolume = 100.0f;
        pNode->fDelay = -1.0f;
        pNode->fVolReverb = 100.0f;
        pNode->volGrp = -1;
        pNode->sfxPriority = 0;
        pNode->uHashVal = 0;
        pNode->pSoundPropAccessor = NULL;
        pNode->bSoundPropTableReloaded = 0;
        pNode->pSoundProp = NULL;
        pNode->pOwner = NULL;
        pNode->lastVoiceID = -1;
        pNode->pLastEmitter = NULL;
        pNode->m_unk_0x40 = false;

        newNode->typeID = i;
        newNode->typeStr = pSoundTable[i];
        newNode->uHashVal = nlStringLowerHash(newNode->typeStr);

        gCharSoundDefineMap.Add(newNode->uHashVal, newNode);

        i++;
    } while (i < 173);
}

/**
 * Offset/Address/Size: 0x3D40 | 0x80147B0C | size: 0xB4
 */
void AudioLoader::SetupWorldSoundTypesAVLTree()
{
    for (int i = 0; i < 211; i++)
    {
        SoundStrToIDNode* pNode = &Audio::gWorldSFXInfo[i];
        pNode->typeStr = Audio::gWorldSoundTable[i];
        pNode->uHashVal = nlStringLowerHash(pNode->typeStr);

        gWorldSoundDefineMap.Add(pNode->uHashVal, pNode);
    }
}

/**
 * Offset/Address/Size: 0x3C60 | 0x80147A2C | size: 0xE0
 */
unsigned long AudioLoader::GetSFXIDFromStr(const char* str, SoundStrToIDNode** ppNode)
{
    if (ppNode != NULL)
    {
        *ppNode = NULL;
    }

    SoundStrToIDNode** foundValue = NULL;
    unsigned long hash = nlStringLowerHash(str);
    bool found = AudioLoader::gMusyXSoundDefineMap.FindGet(hash, &foundValue);

    if (found)
    {
        if (ppNode != NULL)
        {
            *ppNode = *foundValue;
        }
        return (*foundValue)->musyxID;
    }
    return -1;
}

/**
 * Offset/Address/Size: 0x3BAC | 0x80147978 | size: 0xB4
 */
unsigned long AudioLoader::GetCharSFXTypeFromStr(const char* str)
{
    SoundStrToIDNode** ppNode = NULL;
    unsigned long key = nlStringLowerHash(str);
    bool found = AudioLoader::gCharSoundDefineMap.FindGet(key, &ppNode);
    if (found)
        return (*ppNode)->typeID;
    else
        return -1;
}

/**
 * Offset/Address/Size: 0x3AF8 | 0x801478C4 | size: 0xB4
 */
unsigned long AudioLoader::GetWorldSFXTypeFromStr(const char* str)
{
    SoundStrToIDNode** ppNode = NULL;
    unsigned long key = nlStringLowerHash(str);
    bool found = AudioLoader::gWorldSoundDefineMap.FindGet(key, &ppNode);
    if (found)
        return (*ppNode)->typeID;
    else
        return -1;
}

void AudioLoader::DeleteStrToIDTables()
{
    gMusyXSoundDefineMap.DeleteValues();
    gCharSoundDefineMap.DeleteValues();
    gWorldSoundDefineMap.Clear();
}

/**
 * Offset/Address/Size: 0x39F4 | 0x801477C0 | size: 0x104
 */
void AudioLoader::SetupSoundGroups()
{
    const char* szGroup = sebringAudioGroups[0].szGroupName;
    int i = 0;

    while ((u32)nlStrLen(szGroup) != 0)
    {
        unsigned long musyxID = AudioLoader::GetSFXIDFromStr(sebringAudioGroups[i].szGroupName, NULL);

        if ((u32)(musyxID + 0x10000) == 0xFFFF)
            break;

        sebringAudioGroups[i].groupID = (u16)musyxID;
        i++;
    }
}

/**
 * Offset/Address/Size: 0x35EC | 0x801473B8 | size: 0x408
 */
bool AudioLoader::ActivateDPL2(bool bEnableDPL2, bool bLoadSampleFile)
{
    bool dpl2Result;

    if ((bEnableDPL2 && !PlatAudio::gUsingDolbyProLogic2) || (!bEnableDPL2 && PlatAudio::gUsingDolbyProLogic2))
    {
        if (!gbDisableReverb)
        {
            if (gReverbOn)
            {
                nlPrintf("AudioLoader::ActivateDPL2(), shutting down reverb...\n");
                if (!Audio::ShutdownReverb())
                {
                    nlPrintf("AudioLoader::ActivateDPL2(), Audio::ShutdownReverb() returned false.\n");
                    return false;
                }
            }
        }

        // PORT: null when audio is disabled, nothing to command.
        if (g_pTrackManager != NULL) g_pTrackManager->StopAllTracks(0);

        AudioStreamTrack::TrackManagerBase* pTM = g_pTrackManager;
        pTM->StopAllTracks(0);

        pTM->m_FadeMgr.Clear();
        pTM->PurgeStreams();
        pTM->m_StreamPool.FreeBlocks();
        pTM->m_StreamDeleteList.m_Allocator.FreeBlocks();
        PlatAudio::ShutdownStreaming();
        Audio::Silence();
        Audio::UnloadWorldSFX();
        PlatAudio::UnloadAllSoundGroups(sebringAudioFileData);
    }
    else
    {
        return true;
    }

    if (bEnableDPL2)
    {
        dpl2Result = PlatAudio::ActivateDPL2();
    }
    else
    {
        dpl2Result = PlatAudio::DeactivateDPL2();
    }

    Audio::Shutdown();
    Audio::Initialize(bEnableDPL2);

    if (bEnableDPL2)
    {
        PlatAudio::SetOutputMode(MusyXOutputType_SURROUND);
    }

    Audio::LoadWorldSFX();

    if (!gbDisableAudio)
    {
        PlatAudio::SetupSoundBuffers(sebringAudioFileData, true);
    }

    bool onScene = false;
    BaseGameSceneManager* mgr = (BaseGameSceneManager*)nlSingleton<GameSceneManager>::s_pInstance;
    if (mgr != NULL)
    {
        BaseSceneHandler* handler = mgr->GetCurrentScene();

        if (handler != NULL)
        {
            onScene = (mgr->GetSceneType(handler) == 0x27);
        }
    }

    if (onScene)
    {
        if (!gbDisableAudio)
        {
            bool bAlreadyLoaded = false;
            if (sebringAudioGroups[2].uLoadOrder > -1 && sebringAudioGroups[2].stackEnum > -1)
            {
                bAlreadyLoaded = true;
            }

            if (!bAlreadyLoaded)
            {
                if (!gbDisableAudio)
                {
                    bool isInited;
                    if (gbDisableAudio)
                    {
                        isInited = false;
                    }
                    else
                    {
                        isInited = Audio::IsInited();
                    }

                    if (isInited)
                    {
                        PlatAudio::LoadSoundGroup(sebringAudioFileData, 2, 0, true);
                    }
                }
            }
        }
    }
    else
    {
        LoadPermanentSoundGroups(bLoadSampleFile);
    }

    bool bAlreadyLoaded = false;
    if (sebringAudioGroups[0].uLoadOrder > -1 && sebringAudioGroups[0].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (!bAlreadyLoaded)
    {
        if (!gbDisableAudio)
        {
            bool isInited;
            if (gbDisableAudio)
            {
                isInited = false;
            }
            else
            {
                isInited = Audio::IsInited();
            }

            if (isInited)
            {
                PlatAudio::LoadSoundGroup(sebringAudioFileData, 0, 1, !bLoadSampleFile);
            }
        }
    }

    bAlreadyLoaded = false;
    if (sebringAudioGroups[1].uLoadOrder > -1 && sebringAudioGroups[1].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (!bAlreadyLoaded)
    {
        if (!gbDisableAudio)
        {
            bool isInited;
            if (gbDisableAudio)
            {
                isInited = false;
            }
            else
            {
                isInited = Audio::IsInited();
            }

            if (isInited)
            {
                PlatAudio::LoadSoundGroup(sebringAudioFileData, 1, 1, !bLoadSampleFile);
            }
        }
    }

    ARQSetChunkSize(0x1000);
    Audio::InitStreaming();
    PlatAudio::ConfigureStreamBuffers(4);

    return dpl2Result;
}

/**
 * Offset/Address/Size: 0x356C | 0x80147338 | size: 0x80
 */
bool AudioLoader::StartLoad(LoadingManager* manager)
{
    if (gbDisableAudio)
    {
        return true;
    }

    if (gbDisableAudio)
    {
        return true;
    }

    bool isInited;
    if (gbDisableAudio)
    {
        isInited = false;
    }
    else
    {
        isInited = Audio::IsInited();
    }

    if (isInited)
    {
        return true;
    }

    FEAudio::BuildAnimAudioEventLookup();
    g_FEStreamConfig.LoadFromFile("audio/data/streams/FEStreams.txt");
    Audio::Initialize(false);
    return true;
}

/**
 * Offset/Address/Size: 0x34F8 | 0x801472C4 | size: 0x74
 */
bool AudioLoader::Initialize()
{
    if (gbDisableAudio)
    {
        return true;
    }

    bool isInited;
    if (gbDisableAudio)
    {
        isInited = false;
    }
    else
    {
        isInited = Audio::IsInited();
    }

    if (isInited)
    {
        return true;
    }

    FEAudio::BuildAnimAudioEventLookup();
    g_FEStreamConfig.LoadFromFile("audio/data/streams/FEStreams.txt");
    Audio::Initialize(false);
    return true;
}

/**
 * Offset/Address/Size: 0x34C4 | 0x80147290 | size: 0x34
 */
bool AudioLoader::IsInited()
{
    if (gbDisableAudio)
    {
        return false;
    }
    return Audio::IsInited();
}

static inline AudioStreamTrack::StreamTrack* GetTrackByName(const char* pTrackName)
{
    AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
    return pTrackMgr->GetTrack(nlStringLowerHash(pTrackName));
}

/**
 * Offset/Address/Size: 0x31D4 | 0x80146FA0 | size: 0x2F0
 */
void AudioLoader::StartFEStream(const char* pSoundName, bool bLoop, const char* pTrackName)
{
    if (gbDisableAudio || !gbStream)
    {
        return;
    }

    char settingPath[64];
    nlSNPrintf(settingPath, 64, "%s/%s", pSoundName, "Volume");
    float volume = GetConfigFloat(g_FEStreamConfig, settingPath, 0.0f);
    volume /= 100.0f;

    nlSNPrintf(settingPath, 64, "%s/%s", pSoundName, "FadeIn");
    unsigned long fadeIn = GetConfigInt(g_FEStreamConfig, settingPath, 0);
    unsigned long interruptFadeOut = GetConfigInt(g_FEStreamConfig, "InterruptFadeOut", 0);

    if (volume > 0.0f)
    {
        AudioStreamTrack::StreamTrack* track = GetTrackByName(pTrackName);
        track->PlayStream(nlStringLowerHash(pSoundName), volume, bLoop, fadeIn, interruptFadeOut, "", Audio::MasterVolume::VG_Music);
    }
    else
    {
        AudioStreamTrack::StreamTrack* track = GetTrackByName(pTrackName);
        track->Stop(interruptFadeOut);
    }
}

/**
 * Offset/Address/Size: 0x2ED4 | 0x80146CA0 | size: 0x300
 */
void AudioLoader::PlayFETitleMusicWithFade()
{
    if (gbDisableAudio)
    {
        return;
    }

    if (!gbStream)
    {
        return;
    }

    char settingPath[64];
    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Main_Title_With_Fade_In", "Volume");
    float volume = GetConfigFloat(g_FEStreamConfig, settingPath, 0.0f);
    volume /= 100.0f;

    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Main_Title_With_Fade_In", "FadeIn");
    unsigned long fadeIn = GetConfigInt(g_FEStreamConfig, settingPath, 0);
    unsigned long interruptFadeOut = GetConfigInt(g_FEStreamConfig, "InterruptFadeOut", 0);

    if (volume > 0.0f)
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
        track->PlayStream(nlStringLowerHash("FE_Main_Title_With_Fade_In"), volume, true, fadeIn, interruptFadeOut, "", Audio::MasterVolume::VG_Music);
    }
    else
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
        track->Stop(interruptFadeOut);
    }
}

/**
 * Offset/Address/Size: 0x2BD4 | 0x801469A0 | size: 0x300
 */
void AudioLoader::PlayFEMenuMusic()
{
    if (gbDisableAudio)
    {
        return;
    }

    if (!gbStream)
    {
        return;
    }

    char settingPath[64];
    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Main_Options", "Volume");
    float volume = GetConfigFloat(g_FEStreamConfig, settingPath, 0.0f);
    volume /= 100.0f;

    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Main_Options", "FadeIn");
    unsigned long fadeIn = GetConfigInt(g_FEStreamConfig, settingPath, 0);
    unsigned long interruptFadeOut = GetConfigInt(g_FEStreamConfig, "InterruptFadeOut", 0);

    if (volume > 0.0f)
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
        track->PlayStream(nlStringLowerHash("FE_Main_Options"), volume, true, fadeIn, interruptFadeOut, "", Audio::MasterVolume::VG_Music);
    }
    else
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
        track->Stop(interruptFadeOut);
    }
}

/**
 * Offset/Address/Size: 0x27F0 | 0x801465BC | size: 0x3E4
 */
void AudioLoader::PlayLoadLoopMusic()
{
    if (gbDisableAudio || !gbStream)
    {
        return;
    }

    AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
    // PORT: null when audio is disabled, nothing to ask.
    AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
    track->Stop(0);

    if (GetConfigBool(Config::Global(), "NoLoadLoop", false))
    {
        return;
    }

    if (gbDisableAudio)
    {
        return;
    }

    if (!gbStream)
    {
        return;
    }

    char settingPath[64];
    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Load_Loop_01", "Volume");
    float volume = GetConfigFloat(g_FEStreamConfig, settingPath, 0.0f);
    volume /= 100.0f;

    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Load_Loop_01", "FadeIn");
    unsigned long fadeIn = GetConfigInt(g_FEStreamConfig, settingPath, 0);
    unsigned long interruptFadeOut = GetConfigInt(g_FEStreamConfig, "InterruptFadeOut", 0);

    if (volume > 0.0f)
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
        track->PlayStream(nlStringLowerHash("FE_Load_Loop_01"), volume, true, fadeIn, interruptFadeOut, "", Audio::MasterVolume::VG_Music);
    }
    else
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
        track->Stop(interruptFadeOut);
    }
}

/**
 * Offset/Address/Size: 0x24E8 | 0x801462B4 | size: 0x308
 */
void AudioLoader::PlayPauseMenuMusic()
{
    if (gbDisableAudio)
    {
        return;
    }

    if (!gbStream)
    {
        return;
    }

    char settingPath[64];
    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Pause", "Volume");
    float volume = GetConfigFloat(g_FEStreamConfig, settingPath, 0.0f);
    volume /= 100.0f;

    nlSNPrintf(settingPath, 64, "%s/%s", "FE_Pause", "FadeIn");
    unsigned long fadeIn = GetConfigInt(g_FEStreamConfig, settingPath, 0);
    unsigned long interruptFadeOut = GetConfigInt(g_FEStreamConfig, "InterruptFadeOut", 0);

    if (volume > 0.0f)
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("Announcer")) : NULL;
        track->PlayStream(nlStringLowerHash("FE_Pause"), volume, true, fadeIn, interruptFadeOut, "", Audio::MasterVolume::VG_Music);
    }
    else
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        // PORT: null when audio is disabled, nothing to ask.
        AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("Announcer")) : NULL;
        track->Stop(interruptFadeOut);
    }
}

/**
 * Offset/Address/Size: 0x23D4 | 0x801461A0 | size: 0x114
 */
void AudioLoader::StopPauseMenuMusic()
{
    if (gbDisableAudio || !gbStream)
    {
        return;
    }

    AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
    // PORT: null when audio is disabled, nothing to ask.
    AudioStreamTrack::StreamTrack* track = pTrackMgr != NULL ? pTrackMgr->GetTrack(nlStringLowerHash("Announcer")) : NULL;
    s32 fadeOut = GetConfigInt(g_FEStreamConfig, "InterruptFadeOut", 0);
    track->Stop(fadeOut);
}

/**
 * Offset/Address/Size: 0x23B4 | 0x80146180 | size: 0x20
 */
void AudioLoader::StopStreaming()
{
    Audio::StopStreaming();
}

/**
 * Offset/Address/Size: 0x237C | 0x80146148 | size: 0x38
 */
void AudioLoader::SetupSoundBuffers()
{
    if (gbDisableAudio)
    {
        return;
    }

    PlatAudio::SetupSoundBuffers(sebringAudioFileData, true);
}

/**
 * Offset/Address/Size: 0x20E4 | 0x80145EB0 | size: 0x298
 */
void AudioLoader::LoadFE(bool bLoadSampleFile)
{
    if (gbDisableAudio)
    {
        return;
    }

    if (bLoadSampleFile)
    {
        gbAsyncLoadEntireSampleFileIntoMemRequestMade = true;
        PlatAudio::ReadEntireSampleFileIntoMemSync(sebringAudioFileData.szSampleFile);

        if (!gbDisableAudio)
        {
            bool bAlreadyLoaded = false;
            if (sebringAudioGroups[3].uLoadOrder > -1 && sebringAudioGroups[3].stackEnum > -1)
            {
                bAlreadyLoaded = true;
            }

            if (!bAlreadyLoaded)
            {
                if (!gbDisableAudio)
                {
                    bool isInited;
                    if (gbDisableAudio)
                    {
                        isInited = false;
                    }
                    else
                    {
                        isInited = Audio::IsInited();
                    }

                    if (isInited)
                    {
                        PlatAudio::LoadSoundGroup(sebringAudioFileData, 3, 1, false);
                    }
                }
            }
        }

        if (!gbDisableAudio)
        {
            bool bAlreadyLoaded = false;
            if (sebringAudioGroups[4].uLoadOrder > -1 && sebringAudioGroups[4].stackEnum > -1)
            {
                bAlreadyLoaded = true;
            }

            if (!bAlreadyLoaded)
            {
                if (!gbDisableAudio)
                {
                    bool isInited;
                    if (gbDisableAudio)
                    {
                        isInited = false;
                    }
                    else
                    {
                        isInited = Audio::IsInited();
                    }

                    if (isInited)
                    {
                        PlatAudio::LoadSoundGroup(sebringAudioFileData, 4, 1, false);
                    }
                }
            }
        }

        bool bAlreadyLoaded = false;
        if (sebringAudioGroups[0].uLoadOrder > -1 && sebringAudioGroups[0].stackEnum > -1)
        {
            bAlreadyLoaded = true;
        }

        if (!bAlreadyLoaded)
        {
            if (!gbDisableAudio)
            {
                bool isInited;
                if (gbDisableAudio)
                {
                    isInited = false;
                }
                else
                {
                    isInited = Audio::IsInited();
                }

                if (isInited)
                {
                    PlatAudio::LoadSoundGroup(sebringAudioFileData, 0, 1, false);
                }
            }
        }

        bAlreadyLoaded = false;
        if (sebringAudioGroups[1].uLoadOrder > -1 && sebringAudioGroups[1].stackEnum > -1)
        {
            bAlreadyLoaded = true;
        }

        if (!bAlreadyLoaded)
        {
            if (!gbDisableAudio)
            {
                bool isInited;
                if (gbDisableAudio)
                {
                    isInited = false;
                }
                else
                {
                    isInited = Audio::IsInited();
                }

                if (isInited)
                {
                    PlatAudio::LoadSoundGroup(sebringAudioFileData, 1, 1, false);
                }
            }
        }

        ARQSetChunkSize(0x1000);
        PlatAudio::PurgeSampleFileBuffer();
        gbAsyncLoadEntireSampleFileIntoMemRequestMade = false;
    }

    Audio::LoadWorldSFX();
    CrowdMood::SetCrowdVolume(0, 0);
    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->StopAllTracks(0);
    PlatAudio::ConfigureStreamBuffers(4);
    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->DestroyAllTracks();
    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->CreateTrack("FE", Audio::MasterVolume::VG_Music);
    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->CreateTrack("FE2", Audio::MasterVolume::VG_Music);
}

/**
 * Offset/Address/Size: 0x205C | 0x80145E28 | size: 0x88
 */
void AudioLoader::UnloadFE()
{
    if (gbDisableAudio)
    {
        return;
    }

    Audio::ClearFadeData();
    Audio::gWorldSFX.ShutdownPlaySet();
    Audio::gPowerupSFX.ShutdownPlaySet();
    Audio::gStadGenSFX.ShutdownPlaySet();
    Audio::gCrowdSFX.ShutdownPlaySet();
    SlotPoolBase::BaseFreeBlocks(&SFXPlaySet::m_TrackedSFXSlotPool, sizeof(SFXPlaySet));
    PlatAudio::UnloadAllSoundGroupsOnStack(sebringAudioFileData, 1);
    Audio::gWorldSFX.StopPlayingAllTrackedSFX();
}

/**
 * Offset/Address/Size: 0x1F54 | 0x80145D20 | size: 0x108
 */
void AudioLoader::LoadFEAudioData(bool bAsync)
{
    bool bAlreadyLoaded = false;
    if (sebringAudioGroups[0].uLoadOrder > -1 && sebringAudioGroups[0].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (!bAlreadyLoaded)
    {
        if (!gbDisableAudio)
        {
            bool isInited;
            if (gbDisableAudio)
            {
                isInited = false;
            }
            else
            {
                isInited = Audio::IsInited();
            }

            if (isInited)
            {
                PlatAudio::LoadSoundGroup(sebringAudioFileData, 0, 1, bAsync);
            }
        }
    }

    bAlreadyLoaded = false;
    if (sebringAudioGroups[1].uLoadOrder > -1 && sebringAudioGroups[1].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (!bAlreadyLoaded)
    {
        if (!gbDisableAudio)
        {
            bool isInited;
            if (gbDisableAudio)
            {
                isInited = false;
            }
            else
            {
                isInited = Audio::IsInited();
            }

            if (isInited)
            {
                PlatAudio::LoadSoundGroup(sebringAudioFileData, 1, 1, bAsync);
            }
        }
    }

    ARQSetChunkSize(0x1000);
}

/**
 * Offset/Address/Size: 0x1EB8 | 0x80145C84 | size: 0x9C
 */
void AudioLoader::LoadNintendoDialogueGroup(bool bAsync)
{
    if (gbDisableAudio)
        return;

    bool bAlreadyLoaded = false;
    if (sebringAudioGroups[3].uLoadOrder > -1 && sebringAudioGroups[3].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (bAlreadyLoaded)
        return;

    if (gbDisableAudio)
        return;

    bool isInited;
    if (gbDisableAudio)
    {
        isInited = false;
    }
    else
    {
        isInited = Audio::IsInited();
    }

    if (!isInited)
        return;

    PlatAudio::LoadSoundGroup(sebringAudioFileData, 3, 1, bAsync);
}

/**
 * Offset/Address/Size: 0x1E1C | 0x80145BE8 | size: 0x9C
 */
void AudioLoader::LoadNLGDialogueGroup(bool bAsync)
{
    if (gbDisableAudio)
        return;

    bool bAlreadyLoaded = false;
    if (sebringAudioGroups[4].uLoadOrder > -1 && sebringAudioGroups[4].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (bAlreadyLoaded)
        return;

    if (gbDisableAudio)
        return;

    if (!AudioLoader::IsInited())
        return;

    PlatAudio::LoadSoundGroup(sebringAudioFileData, 4, 1, bAsync);
}

/**
 * Offset/Address/Size: 0x1D8C | 0x80145B58 | size: 0x90
 */
void AudioLoader::LoadFEButtonSoundGroup()
{
    if (gbDisableAudio)
    {
        return;
    }

    bool alreadyLoaded = (sebringAudioGroups[2].uLoadOrder > -1) && (sebringAudioGroups[2].stackEnum > -1);

    if (alreadyLoaded)
    {
        return;
    }

    if (gbDisableAudio)
    {
        return;
    }

    bool isInited;
    if (gbDisableAudio)
    {
        isInited = false;
    }
    else
    {
        isInited = Audio::IsInited();
    }

    if (!isInited)
    {
        return;
    }

    PlatAudio::LoadSoundGroup(sebringAudioFileData, 2, 0, true);
}

static inline void LoadOnePermanentGroup(int groupIndex, bool bUseReadFromDiscCallback)
{
    bool bAlreadyLoaded = false;
    if (sebringAudioGroups[groupIndex].uLoadOrder > -1 && sebringAudioGroups[groupIndex].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (!bAlreadyLoaded)
    {
        if (!AudioLoader::gbDisableAudio)
        {
            bool isInited;
            if (AudioLoader::gbDisableAudio)
            {
                isInited = false;
            }
            else
            {
                isInited = Audio::IsInited();
            }

            if (isInited)
            {
                PlatAudio::LoadSoundGroup(AudioLoader::sebringAudioFileData, groupIndex, 0, bUseReadFromDiscCallback);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x1630 | 0x801453FC | size: 0x75C
 */
void AudioLoader::LoadPermanentSoundGroups(bool bLoadEntireSampleFileIntoMem)
{
    if (gbDisableAudio)
        return;

    if (!gbDisableAudio)
    {
        bool bAlreadyLoaded = false;
        if (sebringAudioGroups[2].uLoadOrder > -1 && sebringAudioGroups[2].stackEnum > -1)
        {
            bAlreadyLoaded = true;
        }

        if (!bAlreadyLoaded)
        {
            if (!gbDisableAudio)
            {
                bool isInited;
                if (gbDisableAudio)
                {
                    isInited = false;
                }
                else
                {
                    isInited = Audio::IsInited();
                }

                if (isInited)
                {
                    PlatAudio::LoadSoundGroup(sebringAudioFileData, 2, 0, true);
                }
            }
        }
    }

    bool bUseReadFromDiscCallback = !bLoadEntireSampleFileIntoMem;
    if (bUseReadFromDiscCallback)
    {
        if (PlatAudio::IsEntireSampleFileInMem() || gbAsyncLoadEntireSampleFileIntoMemRequestMade)
        {
            bUseReadFromDiscCallback = false;
        }
    }

    if (gbAsyncLoadEntireSampleFileIntoMemRequestMade)
    {
        while (!PlatAudio::IsEntireSampleFileInMem())
        {
            nlServiceFileSystem();
        }
    }

    LoadOnePermanentGroup(5, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(6, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(19, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(20, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(34, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(35, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(36, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(37, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(38, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(39, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(40, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(41, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(42, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(43, bUseReadFromDiscCallback);
    LoadOnePermanentGroup(44, bUseReadFromDiscCallback);

    ARQSetChunkSize(0x1000);
    PlatAudio::PurgeSampleFileBuffer();
    gbAsyncLoadEntireSampleFileIntoMemRequestMade = false;
}

/**
 * Offset/Address/Size: 0x14F4 | 0x801452C0 | size: 0x13C
 */
void AudioLoader::LoadInGame()
{
    if (gbDisableAudio)
    {
        return;
    }

    bool bLoaded = false;
    if (sebringAudioGroups[1].uLoadOrder > -1 && sebringAudioGroups[1].stackEnum > -1)
    {
        bLoaded = true;
    }

    if (bLoaded)
    {
        if (!PlatAudio::UnloadSoundGroup(sebringAudioFileData, 1))
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not unload SND_GROUP_FE_CHAR sound group from secondary sound stack.\n");
        }
    }

    bLoaded = false;
    if (sebringAudioGroups[0].uLoadOrder > -1 && sebringAudioGroups[0].stackEnum > -1)
    {
        bLoaded = true;
    }

    if (bLoaded)
    {
        if (!PlatAudio::UnloadSoundGroup(sebringAudioFileData, 0))
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not unload SND_GROUP_FE_MAIN sound group from secondary sound stack.\n");
        }
    }

    LoadPermanentSoundGroups(false);
    Audio::LoadInGameSFX();
    Audio::gWorldSFX.Init();
    Audio::gPowerupSFX.Init();
    Audio::gStadGenSFX.Init();

    if (!gbDisableCrowd)
    {
        Audio::gCrowdSFX.Init();
    }

    LoadInGameAudioData();
    Audio::LoadWorldSFX();
    ARQSetChunkSize(0x1000);
    SoundEventScript::CreateInstance();
}

/**
 * Offset/Address/Size: 0x130C | 0x801450D8 | size: 0x1E8
 */
void AudioLoader::UnloadInGame()
{
    if (gbDisableAudio)
    {
        return;
    }

    Audio::ClearFadeData();
    CrowdMood::SetCrowdVolume(0, 0);
    Audio::UnloadWorldSFX();
    Audio::UnloadInGameSFX();
    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->StopAllTracks(0);
    CrowdMood::Purge(false);
    Audio::DestroyPriorityStreams();
    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->DestroyAllTracks();

    AudioStreamTrack::TrackManagerBase* pTM = g_pTrackManager;
    pTM->StopAllTracks(0);

    pTM->m_FadeMgr.Clear();
    pTM->PurgeStreams();
    pTM->m_StreamPool.FreeBlocks();
    pTM->m_StreamDeleteList.m_Allocator.FreeBlocks();

    UnloadInGameAudioData();

    SoundEventScript::DestroyInstance();
    FEAudio::ResetRandomVoiceToggleSFX();
    Audio::Silence();
}

/**
 * Offset/Address/Size: 0x1170 | 0x80144F3C | size: 0x19C
 */
void AudioLoader::SetupPostPhysicsCameraLoad()
{
    if (AudioLoader::gbDisableAudio)
    {
        return;
    }

    AudioLoader::SetupInGameListener();
    AudioLoader::SetupCharStadiumSoundTable();
}

void AudioLoader::SetupInGameListener()
{
    if (!AudioLoader::gbDisableAudio && !Audio::IsListenerActive())
    {
        nlVector3 vDir = { { 0.0f, 0.0f, 1.0f } };
        nlVector3 vCameraTarg;
        nlVector3 vCameraPos;
        nlVector3 vNormHeading;
        nlVector3 vNormUp;

        cBaseCamera* camera = nlDLRingGetStart(cCameraManager::m_cameraStack);
        vCameraTarg = camera->GetTargetPosition();

        camera = nlDLRingGetStart(cCameraManager::m_cameraStack);
        vCameraPos = camera->GetCameraPosition();

        float dz;
        float dy;
        float dx;
        dy = vCameraTarg.y - vCameraPos.y;
        dx = vCameraTarg.x - vCameraPos.x;
        dz = vCameraTarg.z - vCameraPos.z;

        float recipLen = nlRecipSqrt(dx * dx + dy * dy + dz * dz, true);

        vNormHeading.x = recipLen * dx;
        vNormHeading.y = recipLen * dy;
        vNormHeading.z = recipLen * dz;

        static const nlVector3 kUpVec = { { 0.0f, 0.0f, 0.0f } };
        vNormUp = kUpVec;

        GameTweaks* tweaks = g_pGame->m_pGameTweaks;

        PlatAudio::Add3DSFXListener(
            &Audio::gListener,
            vCameraPos,
            vNormUp,
            vNormHeading,
            vDir,
            tweaks->fFrontAudibleSurrDist,
            tweaks->fBackAudibleSurrDist,
            1.0f,
            tweaks->fEmitterDistFromListenerMaxVol,
            false,
            tweaks->fSpeedOfSoundForDoppler);

        Audio::SetListenerActive(true);
    }
}

extern SoundPropAccessor* gpCRITTERWOODSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERWOODSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpBOWSERCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpCRITTERGRASSSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERGRASSSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERMETALSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERMETALSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERROBOTSoundPropAccessor;
extern SoundPropAccessor* gpSTADWOODSoundPropAccessor;
extern SoundPropAccessor* gpSTADKONGASoundPropAccessor;
extern SoundPropAccessor* gpSTADCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpSTADPIPESoundPropAccessor;
extern SoundPropAccessor* gpSTADGRASSSoundPropAccessor;
extern SoundPropAccessor* gpSTADPALACESoundPropAccessor;
extern SoundPropAccessor* gpSTADMETALSoundPropAccessor;
extern SoundPropAccessor* gpSTADUNDERSoundPropAccessor;
extern SoundPropAccessor* gpSTADCRATERSoundPropAccessor;
extern SoundPropAccessor* gpSTADRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpSTADBOWSERSoundPropAccessor;
extern SoundPropAccessor* gpSTADBATTLESoundPropAccessor;
extern eCharacterClass ConvertToCharacterClass(eTeamID);
extern eCharacterClass ConvertToCharacterClass(eSidekickID);

/**
 * Offset/Address/Size: 0xF90 | 0x80144D5C | size: 0x1E0
 */
static SoundPropAccessor* GetSoundPropTableFromPlayerStadium(eStadiumID stadiumId, eCharacterClass charClass)
{
    switch (stadiumId)
    {
    case STAD_DK_DAISY:
        if (charClass >= NUM_FIELDER_CLASSES && charClass < NUM_CHARACTER_CLASSES)
            return gpCRITTERWOODSoundPropAccessor;
        if (charClass == NUM_CHARACTER_CLASSES)
            return gpBOWSERWOODSoundPropAccessor;
        return surfaceSoundPropTables[charClass][4];
    case STAD_MARIO_STADIUM:
        if (charClass >= NUM_FIELDER_CLASSES && charClass < NUM_CHARACTER_CLASSES)
            return gpCRITTERCONCRETESoundPropAccessor;
        if (charClass == NUM_CHARACTER_CLASSES)
            return gpBOWSERCONCRETESoundPropAccessor;
        return surfaceSoundPropTables[charClass][2];
    case STAD_PEACH_TOAD_STADIUM:
        if (charClass >= NUM_FIELDER_CLASSES && charClass < NUM_CHARACTER_CLASSES)
            return gpCRITTERGRASSSoundPropAccessor;
        if (charClass == NUM_CHARACTER_CLASSES)
            return gpBOWSERGRASSSoundPropAccessor;
        return surfaceSoundPropTables[charClass][0];
    case STAD_WARIO_STADIUM:
        if (charClass >= NUM_FIELDER_CLASSES && charClass < NUM_CHARACTER_CLASSES)
            return gpCRITTERMETALSoundPropAccessor;
        if (charClass == NUM_CHARACTER_CLASSES)
            return gpBOWSERMETALSoundPropAccessor;
        return surfaceSoundPropTables[charClass][1];
    case STAD_YOSHI_STADIUM:
        if (charClass >= NUM_FIELDER_CLASSES && charClass < NUM_CHARACTER_CLASSES)
            return gpCRITTERGRASSSoundPropAccessor;
        if (charClass == NUM_CHARACTER_CLASSES)
            return gpBOWSERGRASSSoundPropAccessor;
        return surfaceSoundPropTables[charClass][0];
    case STAD_SUPER_STADIUM:
        if (charClass >= NUM_FIELDER_CLASSES && charClass < NUM_CHARACTER_CLASSES)
            return gpCRITTERRUBBERSoundPropAccessor;
        if (charClass == NUM_CHARACTER_CLASSES)
            return gpBOWSERRUBBERSoundPropAccessor;
        return surfaceSoundPropTables[charClass][3];
    case STAD_FORBIDDEN_DOME:
        if (charClass >= NUM_FIELDER_CLASSES && charClass < NUM_CHARACTER_CLASSES)
            return gpCRITTERCONCRETESoundPropAccessor;
        if (charClass == NUM_CHARACTER_CLASSES)
            return gpBOWSERCONCRETESoundPropAccessor;
        return surfaceSoundPropTables[charClass][2];
    default:
        return NULL;
    }
}

/**
 * Offset/Address/Size: 0xC88 | 0x80144A54 | size: 0x308
 */
void AudioLoader::SetupCharStadiumSoundTable()
{
    if (gbDisableAudio)
    {
        return;
    }

    eStadiumID stadium;
    stadium = nlSingleton<GameInfoManager>::Instance()->GetStadium();

    eCharacterClass homeCaptainClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetTeam(0));
    eCharacterClass awayCaptainClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetTeam(1));
    eCharacterClass homeSidekickClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetSidekick(0));
    eCharacterClass awaySidekickClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetSidekick(1));

    SoundPropAccessor* homeCaptainPropTable = GetSoundPropTableFromPlayerStadium(stadium, homeCaptainClass);
    SoundPropAccessor* awayCaptainPropTable = GetSoundPropTableFromPlayerStadium(stadium, awayCaptainClass);
    SoundPropAccessor* homeSidekickPropTable = GetSoundPropTableFromPlayerStadium(stadium, homeSidekickClass);
    SoundPropAccessor* awaySidekickPropTable = GetSoundPropTableFromPlayerStadium(stadium, awaySidekickClass);

    for (int team = 0; team < 2; team++)
    {
        cTeam* pTeam = g_pTeams[team];
        for (int player = 0; player < 5; player++)
        {
            cPlayer* pPlayer = pTeam->GetPlayer(player);
            if (pPlayer->m_eClassType == GOALIE)
            {
                SoundPropAccessor* goaliePropTable = GetSoundPropTableFromPlayerStadium(stadium, g_pTeams[team]->GetGoalie()->m_eCharacterClass);
                g_pTeams[team]->GetGoalie()->m_pCharacterSFX->SetSFX(goaliePropTable);
                if (((cCharacter*)g_pTeams[team]->GetCaptain())->m_eCharacterClass == MYSTERY)
                {
                    g_pTeams[team]->GetGoalie()->m_pCharacterSFX->SetSFX(gpCRITTERROBOTSoundPropAccessor);
                }
            }
            else if (pPlayer->IsCaptain())
            {
                if (team == 0)
                {
                    pPlayer->m_pCharacterSFX->SetSFX(homeCaptainPropTable);
                }
                else
                {
                    pPlayer->m_pCharacterSFX->SetSFX(awayCaptainPropTable);
                }
            }
            else
            {
                if (team == 0)
                {
                    pPlayer->m_pCharacterSFX->SetSFX(homeSidekickPropTable);
                }
                else
                {
                    pPlayer->m_pCharacterSFX->SetSFX(awaySidekickPropTable);
                }
            }
        }
    }

    if (((cCharacter*)g_pTeams[0]->GetCaptain())->m_eCharacterClass == MYSTERY || ((cCharacter*)g_pTeams[1]->GetCaptain())->m_eCharacterClass == MYSTERY)
    {
        bool bAlreadyLoaded = false;
        if (sebringAudioGroups[13].uLoadOrder > -1 && sebringAudioGroups[13].stackEnum > -1)
        {
            bAlreadyLoaded = true;
        }

        if (!bAlreadyLoaded)
        {
            bool loaded;
            if (gbDisableAudio)
            {
                loaded = true;
            }
            else
            {
                if (!AudioLoader::IsInited())
                {
                    loaded = false;
                }
                else
                {
                    loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, 13, 1, true);
                }
            }

            if (!loaded)
            {
                tDebugPrintManager::Print(DC_SOUND, "Could not load mystery stadium sound group onto secondary sound stack.\n");
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xC34 | 0x80144A00 | size: 0x54
 */
void AudioLoader::SetupBowserStadiumSoundTable(Bowser* bowser)
{
    eStadiumID stadium = nlSingleton<GameInfoManager>::Instance()->GetStadium();
    SoundPropAccessor* propTable = GetSoundPropTableFromPlayerStadium(stadium, (eCharacterClass)0x16);
    bowser->m_pCharacterSFX->SetSFX(propTable);
}

/**
 * Offset/Address/Size: 0x8E4 | 0x801446B0 | size: 0x350
 */
unsigned char AudioLoader::LoadStadiumSpecificSoundGroups(eStadiumID stadiumID)
{
    switch (stadiumID)
    {
    case STAD_DK_DAISY:
        gLoadedSurfaceGroup = 18;
        gLoadedStadiumGroup = 10;
        Audio::gStadGenSFX.SetSFX(gpSTADWOODSoundPropAccessor);
        Audio::gStadGenSFX.SetSFX(gpSTADKONGASoundPropAccessor);
        break;
    case STAD_MARIO_STADIUM:
        gLoadedSurfaceGroup = 16;
        gLoadedStadiumGroup = 8;
        Audio::gStadGenSFX.SetSFX(gpSTADCONCRETESoundPropAccessor);
        Audio::gStadGenSFX.SetSFX(gpSTADPIPESoundPropAccessor);
        break;
    case STAD_PEACH_TOAD_STADIUM:
        gLoadedSurfaceGroup = 14;
        gLoadedStadiumGroup = 7;
        Audio::gStadGenSFX.SetSFX(gpSTADGRASSSoundPropAccessor);
        Audio::gStadGenSFX.SetSFX(gpSTADPALACESoundPropAccessor);
        break;
    case STAD_WARIO_STADIUM:
        gLoadedSurfaceGroup = 15;
        gLoadedStadiumGroup = 9;
        Audio::gStadGenSFX.SetSFX(gpSTADMETALSoundPropAccessor);
        Audio::gStadGenSFX.SetSFX(gpSTADUNDERSoundPropAccessor);
        break;
    case STAD_YOSHI_STADIUM:
        gLoadedSurfaceGroup = 14;
        gLoadedStadiumGroup = 11;
        Audio::gStadGenSFX.SetSFX(gpSTADGRASSSoundPropAccessor);
        Audio::gStadGenSFX.SetSFX(gpSTADCRATERSoundPropAccessor);
        break;
    case STAD_SUPER_STADIUM:
        gLoadedSurfaceGroup = 17;
        gLoadedStadiumGroup = 12;
        Audio::gStadGenSFX.SetSFX(gpSTADRUBBERSoundPropAccessor);
        Audio::gStadGenSFX.SetSFX(gpSTADBOWSERSoundPropAccessor);
        break;
    case STAD_FORBIDDEN_DOME:
        gLoadedSurfaceGroup = 16;
        gLoadedStadiumGroup = 9;
        Audio::gStadGenSFX.SetSFX(gpSTADCONCRETESoundPropAccessor);
        Audio::gStadGenSFX.SetSFX(gpSTADBATTLESoundPropAccessor);
        break;
    default:
        tDebugPrintManager::Print(DC_SOUND, "A new stadium needs to be added to AudioLoader::LoadInGameAudioData()\n");
        return false;
    }

    s32 loadedGroup = gLoadedSurfaceGroup;

    if (AudioLoader::IsSoundGroupLoaded(loadedGroup, 1) == false)
    {
        bool loaded;
        if (gbDisableAudio)
        {
            loaded = true;
        }
        else
        {
            if (AudioLoader::IsInited() == false)
            {
                loaded = false;
            }
            else
            {
                loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, loadedGroup, 1, true);
            }
        }

        if (loaded == false)
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not load surface sound group %d onto secondary sound stack.\n", gLoadedSurfaceGroup);
            return false;
        }
    }

    loadedGroup = gLoadedStadiumGroup;
    if (AudioLoader::IsSoundGroupLoaded(loadedGroup, 1) == false)
    {
        bool loaded;
        if (gbDisableAudio)
        {
            loaded = true;
        }
        else
        {
            if (AudioLoader::IsInited() == false)
            {
                loaded = false;
            }
            else
            {
                loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, loadedGroup, 1, true);
            }
        }

        if (loaded == false)
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not load stadium sound group %d onto secondary sound stack.\n", gLoadedStadiumGroup);
            return false;
        }
    }

    return true;
}

namespace Audio
{
bool InitializeReverb(eStadiumID, unsigned char);
}

/**
 * Offset/Address/Size: 0x1F8 | 0x80143FC4 | size: 0x6EC
 */
bool AudioLoader::LoadInGameAudioData()
{
    bool bAlreadyLoaded;
    bool loaded;
    int loadedGroup;

    bAlreadyLoaded = false;
    if (sebringAudioGroups[45].uLoadOrder > -1 && sebringAudioGroups[45].stackEnum > -1)
    {
        bAlreadyLoaded = true;
    }

    if (bAlreadyLoaded == false)
    {
        if (gbDisableAudio)
        {
            loaded = true;
        }
        else
        {
            if (AudioLoader::IsInited() == false)
            {
                loaded = false;
            }
            else
            {
                loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, 45, 1, true);
            }
        }

        if (loaded == false)
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not load SND_GROUP_HUD_IN_GAME_FE sound group onto secondary sound stack.\n");
            return false;
        }
    }

    LoadStadiumSpecificSoundGroups(nlSingleton<GameInfoManager>::Instance()->GetStadium());

    if (!gbDisableReverb)
    {
        if (Audio::InitializeReverb(nlSingleton<GameInfoManager>::Instance()->GetStadium(), 0) == false)
        {
            nlPrintf("AudioLoader::LoadInGameAudioData(), could not initialize reverb!\n");
        }
        else
        {
            gReverbOn = true;
        }
    }

    eCharacterClass homeCaptainClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetTeam(0));
    eCharacterClass awayCaptainClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetTeam(1));

    int homeCaptainGroup;
    switch (homeCaptainClass)
    {
    case MARIO:
        homeCaptainGroup = 0x18;
        break;
    case DONKEYKONG:
        homeCaptainGroup = 0x16;
        break;
    case DAISY:
        homeCaptainGroup = 0x15;
        break;
    case LUIGI:
        homeCaptainGroup = 0x17;
        break;
    case WALUIGI:
        homeCaptainGroup = 0x1a;
        break;
    case PEACH:
        homeCaptainGroup = 0x19;
        break;
    case WARIO:
        homeCaptainGroup = 0x1b;
        break;
    case YOSHI:
        homeCaptainGroup = 0x1c;
        break;
    case HAMMERBROS:
        homeCaptainGroup = 0x21;
        break;
    case TOAD:
        homeCaptainGroup = 0x20;
        break;
    case BIRDO:
        homeCaptainGroup = 0x1e;
        break;
    case KOOPA:
        homeCaptainGroup = 0x1f;
        break;
    case MYSTERY:
        homeCaptainGroup = 0x1d;
        break;
    default:
        homeCaptainGroup = -1;
        break;
    }
    gLoadedHomeCaptainGroup = homeCaptainGroup;

    int awayCaptainGroup;
    switch (awayCaptainClass)
    {
    case MARIO:
        awayCaptainGroup = 0x18;
        break;
    case DONKEYKONG:
        awayCaptainGroup = 0x16;
        break;
    case DAISY:
        awayCaptainGroup = 0x15;
        break;
    case LUIGI:
        awayCaptainGroup = 0x17;
        break;
    case WALUIGI:
        awayCaptainGroup = 0x1a;
        break;
    case PEACH:
        awayCaptainGroup = 0x19;
        break;
    case WARIO:
        awayCaptainGroup = 0x1b;
        break;
    case YOSHI:
        awayCaptainGroup = 0x1c;
        break;
    case HAMMERBROS:
        awayCaptainGroup = 0x21;
        break;
    case TOAD:
        awayCaptainGroup = 0x20;
        break;
    case BIRDO:
        awayCaptainGroup = 0x1e;
        break;
    case KOOPA:
        awayCaptainGroup = 0x1f;
        break;
    case MYSTERY:
        awayCaptainGroup = 0x1d;
        break;
    default:
        awayCaptainGroup = -1;
        break;
    }
    gLoadedAwayCaptainGroup = awayCaptainGroup;

    if (homeCaptainGroup < 0 || awayCaptainGroup < 0)
    {
        tDebugPrintManager::Print(DC_SOUND, "A new captain needs to be added to GetCharacterSoundGroupFromCharClass()\n");
        return false;
    }

    loadedGroup = homeCaptainGroup;
    bAlreadyLoaded = AudioLoader::IsSoundGroupLoaded(loadedGroup, 1);

    if (bAlreadyLoaded == false)
    {
        if (gbDisableAudio)
        {
            loaded = true;
        }
        else
        {
            if (AudioLoader::IsInited() == false)
            {
                loaded = false;
            }
            else
            {
                loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, loadedGroup, 1, true);
            }
        }

        if (loaded == false)
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not load home captain sound group %d onto secondary sound stack.\n", gLoadedHomeCaptainGroup);
            return false;
        }
    }

    {
        const int awayGroup = gLoadedAwayCaptainGroup;
        bAlreadyLoaded = AudioLoader::IsSoundGroupLoaded(awayGroup, 1);

        if (bAlreadyLoaded == false)
        {
            if (gbDisableAudio)
            {
                loaded = true;
            }
            else
            {
                if (AudioLoader::IsInited() == false)
                {
                    loaded = false;
                }
                else
                {
                    loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, awayGroup, 1, true);
                }
            }

            if (loaded == false)
            {
                tDebugPrintManager::Print(DC_SOUND, "Could not load away captain sound group %d onto secondary sound stack.\n", gLoadedAwayCaptainGroup);
                return false;
            }
        }
    }

    eCharacterClass homeSidekickClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetSidekick(0));
    eCharacterClass awaySidekickClass = ConvertToCharacterClass(nlSingleton<GameInfoManager>::Instance()->GetSidekick(1));

    int homeSidekickGroup;
    switch (homeSidekickClass)
    {
    case MARIO:
        homeSidekickGroup = 0x18;
        break;
    case DONKEYKONG:
        homeSidekickGroup = 0x16;
        break;
    case DAISY:
        homeSidekickGroup = 0x15;
        break;
    case LUIGI:
        homeSidekickGroup = 0x17;
        break;
    case WALUIGI:
        homeSidekickGroup = 0x1a;
        break;
    case PEACH:
        homeSidekickGroup = 0x19;
        break;
    case WARIO:
        homeSidekickGroup = 0x1b;
        break;
    case YOSHI:
        homeSidekickGroup = 0x1c;
        break;
    case HAMMERBROS:
        homeSidekickGroup = 0x21;
        break;
    case TOAD:
        homeSidekickGroup = 0x20;
        break;
    case BIRDO:
        homeSidekickGroup = 0x1e;
        break;
    case KOOPA:
        homeSidekickGroup = 0x1f;
        break;
    case MYSTERY:
        homeSidekickGroup = 0x1d;
        break;
    default:
        homeSidekickGroup = -1;
        break;
    }
    gLoadedHomeSidekickGroup = homeSidekickGroup;

    int awaySidekickGroup;
    switch (awaySidekickClass)
    {
    case MARIO:
        awaySidekickGroup = 0x18;
        break;
    case DONKEYKONG:
        awaySidekickGroup = 0x16;
        break;
    case DAISY:
        awaySidekickGroup = 0x15;
        break;
    case LUIGI:
        awaySidekickGroup = 0x17;
        break;
    case WALUIGI:
        awaySidekickGroup = 0x1a;
        break;
    case PEACH:
        awaySidekickGroup = 0x19;
        break;
    case WARIO:
        awaySidekickGroup = 0x1b;
        break;
    case YOSHI:
        awaySidekickGroup = 0x1c;
        break;
    case HAMMERBROS:
        awaySidekickGroup = 0x21;
        break;
    case TOAD:
        awaySidekickGroup = 0x20;
        break;
    case BIRDO:
        awaySidekickGroup = 0x1e;
        break;
    case KOOPA:
        awaySidekickGroup = 0x1f;
        break;
    case MYSTERY:
        awaySidekickGroup = 0x1d;
        break;
    default:
        awaySidekickGroup = -1;
        break;
    }
    gLoadedAwaySidekickGroup = awaySidekickGroup;

    if (homeSidekickGroup < 0 || awaySidekickGroup < 0)
    {
        tDebugPrintManager::Print(DC_SOUND, "A new sidekick needs to be added to GetCharacterSoundGroupFromCharClass()\n");
        return false;
    }

    loadedGroup = homeSidekickGroup;
    bAlreadyLoaded = AudioLoader::IsSoundGroupLoaded(loadedGroup, 1);

    if (bAlreadyLoaded == false)
    {
        if (gbDisableAudio)
        {
            loaded = true;
        }
        else
        {
            if (AudioLoader::IsInited() == false)
            {
                loaded = false;
            }
            else
            {
                loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, loadedGroup, 1, true);
            }
        }

        if (loaded == false)
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not load home sidekick sound group %d onto secondary sound stack.\n", gLoadedHomeSidekickGroup);
            return false;
        }
    }

    {
        const int awayGroup = gLoadedAwaySidekickGroup;
        bAlreadyLoaded = AudioLoader::IsSoundGroupLoaded(awayGroup, 1);

        if (bAlreadyLoaded == false)
        {
            if (gbDisableAudio)
            {
                loaded = true;
            }
            else
            {
                if (AudioLoader::IsInited() == false)
                {
                    loaded = false;
                }
                else
                {
                    loaded = PlatAudio::LoadSoundGroup(sebringAudioFileData, awayGroup, 1, true);
                }
            }

            if (loaded == false)
            {
                tDebugPrintManager::Print(DC_SOUND, "Could not load away sidekick sound group %d onto secondary sound stack.\n", gLoadedAwaySidekickGroup);
                return false;
            }
        }
    }

    return true;
}

unsigned char AudioLoader::UnloadInGameAudioData()
{
    bool bResult = PlatAudio::UnloadAllSoundGroupsOnStack(sebringAudioFileData, 1);

    gLoadedAwaySidekickGroup = -1;
    gLoadedHomeSidekickGroup = -1;
    gLoadedAwayCaptainGroup = -1;
    gLoadedHomeCaptainGroup = -1;
    gLoadedSurfaceGroup = -1;
    gLoadedStadiumGroup = -1;

    if (!gbDisableReverb)
    {
        bResult = Audio::ShutdownReverb();
        if (!bResult)
        {
            nlPrintf("AudioLoader::UnloadInGameAudioData(), Audio::UpdateReverbSettingsToOff() returned false.\n");
        }
    }

    return bResult;
}

/**
 * Offset/Address/Size: 0x1A8 | 0x80143F74 | size: 0x50
 */
void AudioLoader::ReadEntireSampleFileIntoMem(bool sync)
{
    // PORT: the rest of this file gates on gbDisableAudio; this entry point did not.
    if (gbDisableAudio)
        return;

    gbAsyncLoadEntireSampleFileIntoMemRequestMade = true;

    if (sync)
    {
        PlatAudio::ReadEntireSampleFileIntoMemSync(sebringAudioFileData.szSampleFile);
    }
    else
    {
        PlatAudio::ReadEntireSampleFileIntoMem(sebringAudioFileData.szSampleFile);
    }
}

/**
 * Offset/Address/Size: 0x144 | 0x80143F10 | size: 0x64
 */
void AudioLoader::ResetForNewGame()
{
    bool isInited;

    if (gbDisableAudio)
    {
        isInited = false;
    }
    else
    {
        isInited = Audio::IsInited();
    }

    if (!isInited)
    {
        return;
    }

    if (!gbDisableCrowd)
    {
        CrowdMood::Init();
        CrowdMood::SetMood(CrowdMood::CM_Neutral, 0);
    }

    Audio::GetPriorityStream()->Reset();
    Audio::ResetForNewGame();
}

/**
 * Offset/Address/Size: 0x68 | 0x80143E34 | size: 0xDC
 */
void AudioLoader::ResetForRematch()
{
    bool isInited;

    if (gbDisableAudio)
    {
        isInited = false;
    }
    else
    {
        isInited = Audio::IsInited();
    }

    if (!isInited)
    {
        return;
    }

    CrowdMood::Purge(false);
    Audio::Silence();

    if (!gbDisableCrowd)
    {
        // PORT: null when audio is disabled, nothing to command.
        if (g_pTrackManager != NULL) g_pTrackManager->StopAllTracks(false);
        Audio::GetPriorityStream()->m_Track.Stop(0);
        Audio::GetPriorityStream()->Reset();
        CrowdMood::Init();
        CrowdMood::SetMood(CrowdMood::CM_Neutral, 0);
    }

    if (gbDisableAudio)
    {
        return;
    }

    if (gbDisableAudio)
    {
        isInited = false;
    }
    else
    {
        isInited = Audio::IsInited();
    }

    if (!isInited)
    {
        return;
    }

    if (!Audio::IsWorldSFXLoaded())
    {
        return;
    }

    Audio::ResetPauseStatus();
    nlSingleton<GameInfoManager>::Instance()->GetAudioOptions();
    CrowdMood::SetCrowdVolume(0x7f, 0);
}
/**
 * Offset/Address/Size: 0x0 | 0x80143DCC | size: 0x68
 */
void AudioLoader::InitCrowdFromStateTransition()
{
    if (gbDisableAudio || !(gbDisableAudio ? false : Audio::IsInited()))
    {
        return;
    }

    if (Audio::IsWorldSFXLoaded() == false)
    {
        return;
    }

    Audio::ResetPauseStatus();
    nlSingleton<GameInfoManager>::Instance()->GetAudioOptions();
    CrowdMood::SetCrowdVolume(0x7f, 0);
}
