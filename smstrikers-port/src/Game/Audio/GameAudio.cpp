#include "NL/nlstring_tmpl.h"
#include "Game/GameAudio.h"
#include "NL/nlWare.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Game.h"
#include "Game/Physics/PhysicsObject.h"
#include "Game/Sys/debug.h"
#include "NL/nlTask.h"
#include "types.h"

SlotPool<SFXPlaySet> SFXPlaySet::m_TrackedSFXSlotPool(0x32, 0x10);

static const nlVector3 kZeroVector = { 0.0f, 0.0f, 0.0f };

static bool TrackedSFXPitchFreqTypeCheckCallback(unsigned long type, cGameSFX* pGameSFX);
static bool TrackedSFXFilterFreqTypeCheckCallback(unsigned long type, cGameSFX* pGameSFX);
static bool TrackedSFXPriorityCallback(SFXPlaySet* pSFXPlaySet, unsigned long param, cGameSFX* pGameSFX);

inline bool IsVolGrpInRange(unsigned long sfxID, cGameSFX* pGameSFX)
{
    const int volGrp = pGameSFX->GetVolGroup(sfxID);
    return (volGrp >= 5 && volGrp <= 19);
}

/**
 * Offset/Address/Size: 0x2660 | 0x80153BA4 | size: 0x4C
 */
cGameSFX::cGameSFX()
    : mbInited(false)
    , mNumSFX(0)
    , mNumSFXTypes(0)
    , mpSFX(NULL)
    , mpCurPlaySet()
    , bCurPlaySetIsValid(false)
    , mfTrackedSFXCheckInterval(0.0f)
    , mpSoundStrTable(NULL)
    , meClassType(GAME)
    , mbGroupFilterOn(false)
    , muGroupFilterFreq(0)
    , muGroupPitch(0x2000)
{
}

/**
 * Offset/Address/Size: 0x25B0 | 0x80153AF4 | size: 0xB0
 */
cGameSFX::~cGameSFX()
{
    DeInit();
}

/**
 * Offset/Address/Size: 0x25A0 | 0x80153AE4 | size: 0x10
 */
void cGameSFX::Init()
{
    mbInited = true;
    bCurPlaySetIsValid = true;
}

/**
 * Offset/Address/Size: 0x2554 | 0x80153A98 | size: 0x4C
 */
void cGameSFX::DeInit()
{
    ShutdownPlaySet();
    mpSFX = NULL;
    mpSoundStrTable = 0;
    mbGroupFilterOn = false;
    muGroupFilterFreq = 0;
    muGroupPitch = 0x2000;
    mbInited = false;
}

void cGameSFX::SetupPlaySet()
{
    bCurPlaySetIsValid = true;
}

/**
 * Offset/Address/Size: 0x24E8 | 0x80153A2C | size: 0x6C
 */
void cGameSFX::ShutdownPlaySet()
{
    FORCE_DONT_INLINE;
    bCurPlaySetIsValid = false;
    StopPlayingAllTrackedSFX();

    mpCurPlaySet.Clear();
}

/**
 * Offset/Address/Size: 0x22D0 | 0x80153814 | size: 0x218
 */
void cGameSFX::SetSFX(SoundPropAccessor* pSoundPropAccessor)
{
    if (!mbInited)
    {
        nlPrintf("Error: cGameSFX::SetSFX(): Sound class not initialized\n");
    }

    if (mNumSFX == 0)
    {
        mNumSFX = pSoundPropAccessor->GetNumSFX();
    }
    else
    {
        mNumSFX += pSoundPropAccessor->GetNumSFX();
    }

    for (unsigned long i = 1; i < mNumSFXTypes; i++)
    {
        for (int j = 0; j < pSoundPropAccessor->GetNumSFX(); j++)
        {
            const SoundProperties* pProp = pSoundPropAccessor->GetSoundProperty(j);

            if (nlStrCmp(mpSoundStrTable[i], pProp->typeStr) == 0)
            {
                eClassType classType = GetClassType();

                if (classType == WORLD)
                {
                    AudioLoader::GetWorldSFXTypeFromStr(pProp->typeStr);
                }
                else if (classType == CHAR)
                {
                    AudioLoader::GetCharSFXTypeFromStr(pProp->typeStr);
                }

                SoundStrToIDNode* pNode = NULL;
                unsigned long musyxID = AudioLoader::GetSFXIDFromStr(pProp->musyxStr, &pNode);

                if (classType == CHAR)
                {
                    mpSFX[i].typeID = i;
                    mpSFX[i].typeStr = pProp->typeStr;
                }

                mpSFX[i].musyxStr = pProp->musyxStr;
                mpSFX[i].musyxID = musyxID;
                mpSFX[i].fVolume = pProp->fVolume;
                mpSFX[i].fDelay = pProp->fDelay;
                mpSFX[i].fVolReverb = pProp->fVolReverb;
                mpSFX[i].volGrp = pProp->volumeGroup;
                mpSFX[i].sfxPriority = pProp->priority;
                mpSFX[i].pSoundPropAccessor = pSoundPropAccessor;
                mpSFX[i].pSoundProp = pProp;
                mpSFX[i].pOwner = this;

                break;
            }
        }
    }
}

void cGameSFX::CheckTypeMap(SoundPropAccessor* pSoundPropAccessor) const
{
    for (int i = 0; i < pSoundPropAccessor->GetNumSFX(); i++)
    {
        const SoundProperties* pProp = pSoundPropAccessor->GetSoundProperty(i);
        int j;
        for (j = 1; j < mNumSFXTypes; j++)
        {
            if (nlStrCmp(mpSoundStrTable[j], pProp->typeStr) == 0)
            {
                eClassType classType = GetClassType();
                if (classType == WORLD)
                {
                    AudioLoader::GetWorldSFXTypeFromStr(pProp->typeStr);
                }
                else if (classType == CHAR)
                {
                    AudioLoader::GetCharSFXTypeFromStr(pProp->typeStr);
                }
                break;
            }
        }

        if (j == mNumSFXTypes)
        {
            return;
        }
    }
}

void cGameSFX::ResetSFX()
{
    mNumSFX = 0;

    for (int i = 0; i < mNumSFXTypes; i++)
    {
        mpSFX[i].musyxID = (unsigned long)-1;
        mpSFX[i].fVolume = 100.0f;
        mpSFX[i].fDelay = -1.0f;
        mpSFX[i].fVolReverb = 100.0f;
        mpSFX[i].volGrp = -1;
        mpSFX[i].sfxPriority = 0;
    }
}

/**
 * Offset/Address/Size: 0x22BC | 0x80153800 | size: 0x14
 */
float cGameSFX::GetSFXVol(unsigned long type) const
{
    return mpSFX[type].fVolume;
}

/**
 * Offset/Address/Size: 0x22A8 | 0x801537EC | size: 0x14
 */
float cGameSFX::GetSFXVolReverb(unsigned long type) const
{
    return mpSFX[type].fVolReverb;
}

float cGameSFX::GetSFXVol(const Audio::SoundAttributes& sfxData) const
{
    SoundStrToIDNode sfxInfo = mpSFX[sfxData.mu_Type];
    if (sfxData.mf_Volume != 100.0f)
    {
        return sfxData.mf_Volume;
    }

    return sfxInfo.fVolume;
}

float cGameSFX::GetSFXVolReverb(const Audio::SoundAttributes& sfxData) const
{
    SoundStrToIDNode* sfxEntry = mpSFX + sfxData.mu_Type;
    SoundStrToIDNode sfxInfo = *sfxEntry;
    if (sfxData.mf_VolReverb != 100.0f)
    {
        return sfxData.mf_VolReverb;
    }

    return sfxInfo.fVolReverb;
}

int cGameSFX::GetVolGroup(unsigned long type) const
{
    return mpSFX[type].volGrp;
}

int cGameSFX::GetSFXPriority(unsigned long type) const
{
    return mpSFX[type].sfxPriority;
}

/**
 * Offset/Address/Size: 0x2158 | 0x8015369C | size: 0x150
 */
bool cGameSFX::IsKeepingTrackOf(unsigned long type, SFXPlaySet** pGrabTrackedSFX)
{
    if (pGrabTrackedSFX != NULL)
    {
        *pGrabTrackedSFX = NULL;
    }

    if (!bCurPlaySetIsValid)
    {
        return false;
    }

    nlDLListIterator<SFXPlaySet*> iterator = mpCurPlaySet.Begin();

    while (iterator.hasNext())
    {
        SFXPlaySet* pTrackedSFX = *iterator;

        if (pTrackedSFX->type == type)
        {
            if (pTrackedSFX->bIs3D && pTrackedSFX->emitter != NULL && Audio::IsEmitterActive(pTrackedSFX->emitter))
            {
                pTrackedSFX->voiceID = Audio::GetEmitterVoiceID(pTrackedSFX->emitter);
            }

            if ((pTrackedSFX->delay < 0.0f && pTrackedSFX->bIs3D && pTrackedSFX->voiceID == (unsigned long)Audio::GetSndIDError()) || Audio::IsSFXPlaying(pTrackedSFX->voiceID))
            {
                if (pGrabTrackedSFX != NULL)
                {
                    *pGrabTrackedSFX = pTrackedSFX;
                }
                return true;
            }

            if (pTrackedSFX->delay >= 0.0f)
            {
                if (pGrabTrackedSFX != NULL)
                {
                    *pGrabTrackedSFX = pTrackedSFX;
                }
                return true;
            }
        }

        iterator.next();
    }

    return false;
}

bool cGameSFX::InitiateCallbackOnAllTrackedSFX(
    bool (*pTrackedSFXCallback)(SFXPlaySet*, unsigned long, cGameSFX*),
    unsigned long param,
    bool (*pTrackedSFXTypeCallback)(unsigned long, cGameSFX*))
{
    if (!bCurPlaySetIsValid)
    {
        return true;
    }

    nlDLListIterator<SFXPlaySet*> iterator = mpCurPlaySet.Begin();

    while (iterator.hasNext())
    {
        SFXPlaySet* pTrackedSFX = *iterator;
        iterator.next();

        if (pTrackedSFX->type == (unsigned long)-1)
        {
            continue;
        }

        if (pTrackedSFXTypeCallback != NULL && !pTrackedSFXTypeCallback(pTrackedSFX->type, this))
        {
            continue;
        }

        pTrackedSFXCallback(pTrackedSFX, param, this);
    }

    return true;
}

static bool TrackedSFXFilterActivateCallback(SFXPlaySet* pSFXPlaySet, unsigned long param, cGameSFX*)
{
    bool bOn = false;
    if ((unsigned char)param == true)
    {
        bOn = true;
    }
    bool bResult = true;

    if (pSFXPlaySet->bIs3D)
    {
        if (!Audio::IsEmitterActive(pSFXPlaySet->emitter))
        {
            return bResult;
        }
        pSFXPlaySet->voiceID = Audio::GetEmitterVoiceID(pSFXPlaySet->emitter);
    }

    if (pSFXPlaySet->voiceID != Audio::GetSndIDError())
    {
        if (Audio::IsSFXPlaying(pSFXPlaySet->voiceID))
        {
            return Audio::ActivateFilterOnSFX(pSFXPlaySet->voiceID, bOn);
        }
    }

    return bResult;
}

static bool TrackedSFXFilterFreqCallback(SFXPlaySet* pSFXPlaySet, unsigned long param, cGameSFX*)
{
    if (pSFXPlaySet->bIs3D)
    {
        if (!Audio::IsEmitterActive(pSFXPlaySet->emitter))
        {
            return true;
        }
        pSFXPlaySet->voiceID = Audio::GetEmitterVoiceID(pSFXPlaySet->emitter);
    }

    if (pSFXPlaySet->voiceID == Audio::GetSndIDError())
    {
        return true;
    }

    if (!Audio::IsSFXPlaying(pSFXPlaySet->voiceID))
    {
        return true;
    }

    if (pSFXPlaySet->filterFreq != (unsigned short)param)
    {
        pSFXPlaySet->filterFreq = (unsigned short)param;
        Audio::SetFilterFreqOnSFX(pSFXPlaySet->voiceID, (unsigned short)param);
    }

    return true;
}

static bool TrackedSFXPitchFreqCallback(SFXPlaySet* pSFXPlaySet, unsigned long param, cGameSFX*)
{
    if (pSFXPlaySet->bIs3D)
    {
        if (!Audio::IsEmitterActive(pSFXPlaySet->emitter))
        {
            return true;
        }
        pSFXPlaySet->voiceID = Audio::GetEmitterVoiceID(pSFXPlaySet->emitter);
    }

    if (pSFXPlaySet->voiceID == Audio::GetSndIDError())
    {
        return true;
    }

    if (!Audio::IsSFXPlaying(pSFXPlaySet->voiceID))
    {
        return true;
    }

    if (pSFXPlaySet->pitch != (unsigned short)param)
    {
        pSFXPlaySet->pitch = (unsigned short)param;
        Audio::SetPitchBendOnSFX(pSFXPlaySet->voiceID, (unsigned short)param);
    }

    return true;
}

/**
 * Offset/Address/Size: 0x2128 | 0x8015366C | size: 0x30
 */
static bool TrackedSFXPitchFreqTypeCheckCallback(unsigned long type, cGameSFX* pGameSFX)
{
    const int volGrp = pGameSFX->GetVolGroup(type);
    return (bool)(volGrp >= 5 && volGrp <= 19);
}

/**
 * Offset/Address/Size: 0x208C | 0x801535D0 | size: 0x9C
 */
static bool TrackedSFXFilterFreqTypeCheckCallback(unsigned long type, cGameSFX* pGameSFX)
{
    if (pGameSFX->GetClassType() == WORLD && type == 0xBB)
    {
        return false;
    }

    if (IsVolGrpInRange(type, pGameSFX))
    {
        return false;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x1F60 | 0x801534A4 | size: 0x12C
 */
bool cGameSFX::ActivateFilterOnAllTrackedSFX(bool bOn)
{
    unsigned long param = (bool)bOn;
    bool bResult = InitiateCallbackOnAllTrackedSFX(TrackedSFXFilterActivateCallback, param, NULL);
    if (bOn)
    {
        mbGroupFilterOn = true;
    }
    else
    {
        mbGroupFilterOn = false;
    }
    return bResult;
}

/**
 * Offset/Address/Size: 0x1DCC | 0x80153310 | size: 0x194
 */
bool cGameSFX::SetFilterFreqOnAllTrackedSFX(unsigned short freq)
{
    if (freq > 0x3FFF)
    {
        freq = 0x3FFF;
    }

    bool result = InitiateCallbackOnAllTrackedSFX(TrackedSFXFilterFreqCallback, freq, TrackedSFXFilterFreqTypeCheckCallback);
    muGroupFilterFreq = freq;
    return result;
}

/**
 * Offset/Address/Size: 0x1C78 | 0x801531BC | size: 0x154
 */
bool cGameSFX::SetPitchBendOnAllDialogueSFX(unsigned short pitch)
{
    if (pitch > 0x3FFF)
    {
        pitch = 0x3FFF;
    }

    bool result = InitiateCallbackOnAllTrackedSFX(TrackedSFXPitchFreqCallback, pitch, TrackedSFXPitchFreqTypeCheckCallback);
    muGroupPitch = pitch;
    return result;
}

bool cGameSFX::CheckForHigherPrioritySFX(int priority)
{
    if (!bCurPlaySetIsValid)
    {
        return false;
    }

    nlDLListIterator<SFXPlaySet*> iterator = mpCurPlaySet.Begin();
    while (iterator.hasNext())
    {
        SFXPlaySet* pTrackedSFX = *iterator;
        if (pTrackedSFX->type != (unsigned long)-1)
        {
            int trackedPriority = GetSFXPriority(pTrackedSFX->type);
            if (trackedPriority > 0 && trackedPriority < priority)
            {
                if (pTrackedSFX->bIs3D)
                {
                    if (pTrackedSFX->emitter != NULL && Audio::IsEmitterActive(pTrackedSFX->emitter))
                    {
                        pTrackedSFX->voiceID = Audio::GetEmitterVoiceID(pTrackedSFX->emitter);
                        return true;
                    }
                }
                else if (Audio::IsSFXPlaying(pTrackedSFX->voiceID))
                {
                    return true;
                }
            }
        }
        iterator.next();
    }

    return false;
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x1BD0 | 0x80153114 | size: 0xA8
 */
static bool TrackedSFXPriorityCallback(SFXPlaySet* pSFXPlaySet, unsigned long param, cGameSFX* pGameSFX)
{
    if (pSFXPlaySet->bIs3D != 0)
    {
        if (Audio::IsEmitterActive(pSFXPlaySet->emitter) && pSFXPlaySet->sfxPriority > param)
        {
            return pGameSFX->StopTrackedSFX(pSFXPlaySet);
        }
    }
    else
    {
        if (Audio::IsSFXPlaying(pSFXPlaySet->voiceID) && pSFXPlaySet->sfxPriority > param)
        {
            return pGameSFX->StopTrackedSFX(pSFXPlaySet);
        }
    }
    return true;
}
#pragma dont_inline reset

bool cGameSFX::KillLowerPrioritySFX(int priority)
{
    return InitiateCallbackOnAllTrackedSFX(TrackedSFXPriorityCallback, priority, NULL);
}

/**
 * Offset/Address/Size: 0x1104 | 0x80152648 | size: 0xACC
 */
uintptr_t cGameSFX::Play(Audio::SoundAttributes& sfxData)
{
    // PORT: uintptr_t holds (uintptr_t)pSFXEmitter when mf_ReturnEmitterOnPlay is set, and unsigned long is four bytes on Windows.
    uintptr_t voiceID;
    SFXEmitter* pSFXEmitter;
    unsigned long emitterIndex;
    SFXPlaySet* pTrackedSFX;
    float currTime;
    float fVolume;
    float fVolReverb;
    int volGrp;
    EmitterStartInfo info;
    pSFXEmitter = NULL;
    emitterIndex = Audio::GetSndIDError();

    if (!Audio::IsInited() || !this->mbInited)
    {
        return Audio::GetSndIDError();
    }

    unsigned long sfxID = GetSFXID(sfxData.mu_Type);

    if (!sfxData.m_unk_0x7B && g_pGame != NULL && g_pGame->mbCaptainShotToScoreOn && nlTaskManager::m_pInstance->m_CurrState != 1)
    {
        if (sfxData.me_ClassType == WORLD)
        {
            if (sfxData.mu_Type != 0xB5 && sfxData.mu_Type != 0xBC && sfxData.mu_Type != 0xBD && sfxData.mu_Type != 0x94
                && sfxData.mu_Type != 0x95 && sfxData.mu_Type != 0x96 && sfxData.mu_Type != 0x92 && sfxData.mu_Type != 0x93
                && sfxData.mu_Type != 0xCA && sfxData.mu_Type != 0x1C)
            {
                return Audio::GetSndIDError();
            }
        }
        else if (sfxData.me_ClassType == CHAR)
        {
            if (sfxData.mu_Type != 0x4F && sfxData.mu_Type != 0x4E && sfxData.mu_Type != 0x4F && sfxData.mu_Type != 0x50
                && sfxData.mu_Type != 0x3B && sfxData.mu_Type != 0x3A && sfxData.mu_Type != 0x3C && sfxData.mu_Type != 0x62
                && sfxData.mu_Type != 0x5D)
            {
                return Audio::GetSndIDError();
            }
        }
    }

    pTrackedSFX = NULL;

    if (sfxData.mb_NoPhasingFilter)
    {
        if (IsKeepingTrackOf(sfxData.mu_Type, &pTrackedSFX))
        {
            currTime = Audio::GetAudioTimer();

            float repeatThreshold = 0.01f;
            if (currTime - pTrackedSFX->timeStamp < repeatThreshold && sfxData.mf_DelayTime == pTrackedSFX->delay)
            {
                tDebugPrintManager::Print(DC_SOUND, "SFX Repeat Filter prevented a duplicate instance of type %d playing at %0.2f\n", sfxData.mu_Type, currTime);
                tDebugPrintManager::Print(
                    DC_SOUND, "Last instance of type %d played at %0.2f. Threshold time is %0.2f\n", sfxData.mu_Type, pTrackedSFX->timeStamp, repeatThreshold);
                return Audio::GetSndIDError();
            }
        }
    }

    {
        fVolume = this->GetSFXVol(sfxData);
        fVolReverb = this->GetSFXVolReverb(sfxData);

        if (fVolReverb == 0.0f)
        {
            fVolReverb = 100.0f;
        }

        fVolume += sfxData.mf_VolAdjustment;

        volGrp = GetVolGroup(sfxData.mu_Type);
        if (volGrp > -1)
        {
            Audio::SetSFXVolumeGroup(sfxID, volGrp);
        }

        if (sfxData.mi_GroupPriority >= 0)
        {
            if (CheckForHigherPrioritySFX(sfxData.mi_GroupPriority))
            {
                return Audio::GetSndIDError();
            }

            KillLowerPrioritySFX(sfxData.mi_GroupPriority);
        }

        sfxData.mi_SFXPriority = GetSFXPriority(sfxData.mu_Type);
    }

    if (sfxData.mi_SFXPriority > 0 && sfxData.mf_DelayTime < 0.0f)
    {
        if (CheckForHigherPrioritySFX(sfxData.mi_SFXPriority))
        {
            return Audio::GetSndIDError();
        }

        KillLowerPrioritySFX(sfxData.mi_SFXPriority);
    }

    sfxData.mp_OwnerSFX = this;
    {
        bool bInRange = (volGrp >= 5 && volGrp <= 19);
        if (bInRange)
        {
            sfxData.mu_Pitch = 0x2000;
        }
        else
        {
            sfxData.mb_FilterOn = false;
            sfxData.mu_FilterFreq = 0;
        }
    }

    if (sfxData.mf_DelayTime != -1.0f)
    {
        return Audio::AddDelayedSFX(sfxData, sfxID, fVolume, fVolReverb, this);
    }

    if (!sfxData.mb_Is3D)
    {
        voiceID = Audio::PlaySFXbyID(sfxData, sfxID, fVolume, fVolReverb, volGrp);
        this->mpSFX[sfxData.mu_Type].lastVoiceID = voiceID;
        return voiceID;
    }

    nlVector3 vPos;
    nlVector3 vDir;

    vDir = kZeroVector;

    if (sfxData.posUpdateMethod == PHYSOBJ)
    {
        sfxData.mp_PhysObj->GetPosition(&vPos);
    }
    else if (sfxData.posUpdateMethod == PTRS_TO_VECTORS)
    {
        vPos = *sfxData.pos.pvPos;
    }
    else if (sfxData.posUpdateMethod == VECTORS)
    {
        vPos = sfxData.pos.vPos;
    }

    if (sfxData.mb_Update3DContinuously)
    {
        pSFXEmitter = Audio::GetFreeEmitter(emitterIndex);
        if (pSFXEmitter == NULL)
        {
            return Audio::GetSndIDError();
        }

        if (sfxData.posUpdateMethod == PHYSOBJ)
        {
            if (sfxData.mp_PhysObj->m_bodyID != NULL)
            {
                sfxData.mp_PhysObj->GetLinearVelocity(&vDir);
            }
            pSFXEmitter->pPhysObj = sfxData.mp_PhysObj;
        }
        else if (sfxData.posUpdateMethod == PTRS_TO_VECTORS)
        {
            vDir = *sfxData.dir.pvDir;
            pSFXEmitter->pos.pvPos = sfxData.pos.pvPos;
            pSFXEmitter->dir.pvDir = sfxData.dir.pvDir;
        }
        else if (sfxData.posUpdateMethod == VECTORS)
        {
            vDir = sfxData.dir.vDir;
            pSFXEmitter->pos.vPos = vPos;
            pSFXEmitter->dir.vDir = vDir;
        }

        pSFXEmitter->posUpdateMethod = sfxData.posUpdateMethod;
        pSFXEmitter->soundType = sfxData.mu_Type;
        pSFXEmitter->pOwner = this;
    }

    info.pSFXEmitter = NULL;
    info.uSFXID = (unsigned long)-1;
    info.groupID = 0;
    info.position.x = 0.0f;
    info.position.y = 0.0f;
    info.position.z = 0.0f;
    info.direction.x = 0.0f;
    info.direction.y = 0.0f;
    info.direction.z = 0.0f;
    info.maxDist = 100.0f;
    info.comp = 0.0f;
    info.minVol = 0.0f;
    info.maxVol = 1.0f;
    info.fVolReverb = 100.0f;
    info.bContinuous = true;
    info.bRestartable = false;
    info.bPausable = false;
    info.bUseDoppler = false;
    info.bHardStart = true;
    info.bActivateFilter = false;
    info.filterFreq = 0;
    info.pitch = 0x2000;

    SFXEmitter* pInfoEmitter = sfxData.mb_Update3DContinuously ? pSFXEmitter : NULL;
    info.pSFXEmitter = pInfoEmitter;
    info.uSFXID = sfxID;
    info.groupID = sfxData.mi_EmitterGroup;
    info.position = vPos;
    info.direction = vDir;
    info.maxDist = g_pGame->m_pGameTweaks->fMaxAudibleEmitterDistance;
    info.comp = g_pGame->m_pGameTweaks->fEmitterVolToDistanceValue;
    info.minVol = 0.0f;
    info.maxVol = fVolume;
    info.fVolReverb = fVolReverb;
    info.bContinuous = sfxData.mb_Update3DContinuously ? 1 : 0;
    info.bHardStart = sfxData.m_unk_0x7C;

    {
        bool bInRange = (volGrp >= 5 && volGrp <= 19);
        if (bInRange)
        {
            info.pitch = 0x2000;
        }
        else
        {
            info.bActivateFilter = false;
            info.filterFreq = 0;
        }
    }

    voiceID = Audio::Add3DSFXEmitter(info);

    if (sfxData.mb_Update3DContinuously)
    {
        pSFXEmitter->fTimeStamp = Audio::GetAudioTimer();
    }

    this->mpSFX[sfxData.mu_Type].pLastEmitter = pSFXEmitter;

    if (sfxData.mf_ReturnEmitterOnPlay)
    {
        voiceID = (uintptr_t)pSFXEmitter;
    }

    if (sfxData.mb_KeepTrack)
    {
        this->KeepTrack(pSFXEmitter, sfxData, voiceID);
    }

    return voiceID;
}

/**
 * Offset/Address/Size: 0xF00 | 0x80152444 | size: 0x204
 */
SFXPlaySet* cGameSFX::KeepTrack(SFXEmitter* pSFXEmitter, const Audio::SoundAttributes& sfxData, unsigned long uVoiceID)
{
    if (!bCurPlaySetIsValid)
    {
        return NULL;
    }

    if (sfxData.mb_Is3D && !sfxData.mb_Update3DContinuously)
    {
        return NULL;
    }

    SFXPlaySet* slot = NULL;

    SFXPlaySet::m_TrackedSFXSlotPool.Allocate(slot);

    slot->type = (unsigned long)-1;
    slot->voiceID = Audio::GetSndIDError();
    slot->bIs3D = 0;
    slot->emitter = NULL;
    slot->delay = -1.0f;
    slot->timeStamp = -1.0f;
    slot->sfxPriority = 0;
    slot->groupPriority = -1;
    slot->filterFreq = 0;
    slot->pitch = 0x2000;
    slot->type = sfxData.mu_Type;
    if (!sfxData.mf_ReturnEmitterOnPlay)
    {
        slot->voiceID = uVoiceID;
    }

    slot->delay = sfxData.mf_DelayTime;
    if (sfxData.mb_Is3D)
    {
        slot->bIs3D = 1;
    }
    else
    {
        slot->bIs3D = 0;
    }

    slot->timeStamp = Audio::GetAudioTimer();

    if (sfxData.mb_Is3D && sfxData.mb_Update3DContinuously && -1.0f == sfxData.mf_DelayTime)
    {
        slot->emitter = pSFXEmitter;
        pSFXEmitter->bKeepTrack = true;
        if (sfxData.posUpdateMethod == PHYSOBJ)
        {
            pSFXEmitter->pPhysObj = sfxData.mp_PhysObj;
        }
        else if (sfxData.posUpdateMethod == PTRS_TO_VECTORS)
        {
            pSFXEmitter->pos.pvPos = sfxData.pos.pvPos;
            pSFXEmitter->dir.pvDir = sfxData.dir.pvDir;
        }
        else if (sfxData.posUpdateMethod == VECTORS)
        {
            pSFXEmitter->pos.vPos = sfxData.pos.vPos;
            pSFXEmitter->dir.vDir = sfxData.dir.vDir;
        }
    }

    mpCurPlaySet.AddStart(slot);

    return slot;
}

/**
 * Offset/Address/Size: 0xDD4 | 0x80152318 | size: 0x12C
 */
void cGameSFX::Stop(unsigned long type, cGameSFX::StopFlag stopFlag)
{
    if (!bCurPlaySetIsValid)
    {
        return;
    }

    f32 fOldestTimeStamp = -1.0f;

    nlDLListIterator<SFXPlaySet*> iterToOldestSFX = mpCurPlaySet.Begin();
    nlDLListIterator<SFXPlaySet*> iterator = mpCurPlaySet.Begin();

    while (iterator.hasNext())
    {
        SFXPlaySet* pTrackedSFX = *iterator;
        nlDLListIterator<SFXPlaySet*> iterToCurrentItem = iterator;
        iterator.next();

        if (pTrackedSFX->type != type)
        {
            continue;
        }

        if (stopFlag == SFX_STOP_OLDEST)
        {
            if (fOldestTimeStamp != -1.0f && !(pTrackedSFX->timeStamp < fOldestTimeStamp))
            {
                continue;
            }
            fOldestTimeStamp = pTrackedSFX->timeStamp;
            iterToOldestSFX = iterToCurrentItem;
            continue;
        }

        StopTrackedSFX(&iterToCurrentItem);

        if (stopFlag != SFX_STOP_ALL)
        {
            break;
        }
    }

    if (stopFlag == SFX_STOP_OLDEST && fOldestTimeStamp != -1.0f)
    {
        StopTrackedSFX(&iterToOldestSFX);
    }
}

/**
 * Offset/Address/Size: 0xC84 | 0x801521C8 | size: 0x150
 */
void cGameSFX::StopEmitter(SFXEmitter* pSFXEmitter, unsigned long type)
{
    if (pSFXEmitter == NULL)
    {
        return;
    }

    for (s32 i = 0; i < 0x40; i++)
    {
        SFXEmitter* pEmitter = Audio::GetEmitter(i);
        if (pEmitter == NULL || pSFXEmitter != pEmitter)
        {
            continue;
        }

        if ((type != 0 && pSFXEmitter->soundType != type) || pSFXEmitter->pOwner != this)
        {
            tDebugPrintManager::Print(DC_SOUND, "cGameSFX::StopEmitter(): types don't match!\n");
            return;
        }

        pSFXEmitter->bIsStopping = true;
        unsigned char bResult = Audio::Remove3DSFXEmitter(pSFXEmitter);
        if (!bResult)
        {
            return;
        }

        pSFXEmitter->bKeepTrack = true;
        pSFXEmitter->Init();
        pSFXEmitter->pos.vPos.x = 0.0f;
        pSFXEmitter->pos.vPos.y = 0.0f;
        pSFXEmitter->pos.vPos.z = 0.0f;
        pSFXEmitter->dir.vDir.x = 0.0f;
        pSFXEmitter->dir.vDir.y = 0.0f;
        pSFXEmitter->dir.vDir.z = 0.0f;
        pSFXEmitter->posUpdateMethod = (PosUpdateMethod)0;

        if (pSFXEmitter->pMIDIControllerInfo != NULL)
        {
            if (pSFXEmitter->pMIDIControllerInfo->paraArray != NULL)
            {
                delete[] pSFXEmitter->pMIDIControllerInfo->paraArray;
            }
            delete pSFXEmitter->pMIDIControllerInfo;
        }
        pSFXEmitter->pMIDIControllerInfo = NULL;
        return;
    }
}

/**
 * Offset/Address/Size: 0xBC0 | 0x80152104 | size: 0xC4
 */
bool cGameSFX::StopTrackedSFX(SFXPlaySet* pSFXPlaySet)
{
    FORCE_DONT_INLINE;
    if (!bCurPlaySetIsValid)
    {
        return true;
    }

    nlDLListIterator<SFXPlaySet*> iter = mpCurPlaySet.Begin();
    for (; iter.hasNext(); iter.next())
    {
        if (pSFXPlaySet == *iter)
        {
            return StopTrackedSFX(&iter);
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x974 | 0x80151EB8 | size: 0x24C
 */
bool cGameSFX::StopTrackedSFX(nlDLListIterator<SFXPlaySet*>* pIter)
{
    SFXPlaySet* pTrackedSFX = **pIter;
    SFXEmitter* pSFXEmitter;

    if (pTrackedSFX->delay >= 0.0f)
    {
        int index = Audio::IsDelayedCharSFX(pTrackedSFX->type, this);
        if (index != -1)
        {
            Audio::RemoveDelayedSFX(index);
            return true;
        }
    }
    else
    {
        if (pTrackedSFX->bIs3D != 0)
        {
            pSFXEmitter = pTrackedSFX->emitter;
            if (pSFXEmitter != NULL && pSFXEmitter->soundType == pTrackedSFX->type && pSFXEmitter->pOwner == this)
            {
                bool bResult = Audio::Remove3DSFXEmitter(pSFXEmitter);
                pSFXEmitter->bIsStopping = true;

                if (bResult)
                {
                    if (!Audio::IsEmitterActive(pSFXEmitter))
                    {
                        pSFXEmitter->bKeepTrack = true;
                        pSFXEmitter->Init();
                        pSFXEmitter->pos.vPos.x = 0.0f;
                        pSFXEmitter->pos.vPos.y = 0.0f;
                        pSFXEmitter->pos.vPos.z = 0.0f;
                        pSFXEmitter->dir.vDir.x = 0.0f;
                        pSFXEmitter->dir.vDir.y = 0.0f;
                        pSFXEmitter->dir.vDir.z = 0.0f;
                        pSFXEmitter->posUpdateMethod = NONE;

                        if (pSFXEmitter->pMIDIControllerInfo != NULL)
                        {
                            if (pSFXEmitter->pMIDIControllerInfo->paraArray != NULL)
                            {
                                delete[] pSFXEmitter->pMIDIControllerInfo->paraArray;
                            }
                            delete pSFXEmitter->pMIDIControllerInfo;
                        }
                        pSFXEmitter->pMIDIControllerInfo = NULL;
                    }
                }
            }
            else if (pTrackedSFX->voiceID != Audio::GetSndIDError())
            {
                Audio::StopSFX(pTrackedSFX->voiceID);
            }
            else
            {
                nlPrintf("Stopping stop sound type %d\n which no longer has a valid emitter.\n", pTrackedSFX->type);
            }
        }
        else
        {
            Audio::StopSFX(pTrackedSFX->voiceID);
        }
    }

    RemoveTrackedSFX(pIter);
    return true;
}

/**
 * Offset/Address/Size: 0x8D8 | 0x80151E1C | size: 0x9C
 */
void cGameSFX::StopPlayingAllTrackedSFX()
{
    FORCE_DONT_INLINE;
    if (!bCurPlaySetIsValid)
    {
        return;
    }

    for (nlDLListIterator<SFXPlaySet*> iterator = mpCurPlaySet.Begin(); iterator.hasNext();)
    {
        nlDLListIterator<SFXPlaySet*> iterToCurrentItem = iterator;
        iterator.next();
        StopTrackedSFX(&iterToCurrentItem);
    }
}

void cGameSFX::UpdateGroupFilterStatusOnSFX(SFXPlaySet* pTrackedSFX)
{
    bool bResult = TrackedSFXFilterFreqTypeCheckCallback(pTrackedSFX->type, this);

    if (!bResult)
    {
        return;
    }

    if (mbGroupFilterOn)
    {
        if (pTrackedSFX->bIs3D)
        {
            if (Audio::IsEmitterActive(pTrackedSFX->emitter))
            {
                pTrackedSFX->voiceID = Audio::GetEmitterVoiceID(pTrackedSFX->emitter);
            }
        }

        Audio::ActivateFilterOnSFX(pTrackedSFX->voiceID, true);

        if (pTrackedSFX->filterFreq != muGroupFilterFreq)
        {
            bResult = Audio::SetFilterFreqOnSFX(pTrackedSFX->voiceID, muGroupFilterFreq);
            if (!bResult)
            {
                tDebugPrintManager::Print(
                    DC_SOUND,
                    "SetFilterFreqOnSFX() returned false on type %d, voice ID %d, with frequency %d.\n",
                    pTrackedSFX->type,
                    pTrackedSFX->voiceID,
                    muGroupFilterFreq);
            }
            pTrackedSFX->filterFreq = muGroupFilterFreq;
        }
    }
    else
    {
        muGroupFilterFreq = 0;
        Audio::ActivateFilterOnSFX(pTrackedSFX->voiceID, false);
        Audio::SetFilterFreqOnSFX(pTrackedSFX->voiceID, muGroupFilterFreq);
        pTrackedSFX->filterFreq = muGroupFilterFreq;
    }
}

void cGameSFX::UpdateGroupPitchStatusOnSFX(SFXPlaySet* pTrackedSFX)
{
    bool bResult = TrackedSFXPitchFreqTypeCheckCallback(pTrackedSFX->type, this);

    if (!bResult)
    {
        return;
    }

    bResult = Audio::SetPitchBendOnSFX(pTrackedSFX->voiceID, muGroupPitch);
    if (!bResult)
    {
        tDebugPrintManager::Print(
            DC_SOUND,
            "SetPitchBendOnSFX() returned false on type %d, voice ID %d, with pitch %d.\n",
            pTrackedSFX->type,
            pTrackedSFX->voiceID,
            muGroupPitch);
    }

    pTrackedSFX->pitch = muGroupPitch;
}

/**
 * Offset/Address/Size: 0x0 | 0x80151544 | size: 0x8D8
 */
void cGameSFX::UpdateAllTrackedSFX(float fDeltaT)
{
    float currTime;

    if (!bCurPlaySetIsValid)
    {
        return;
    }

    nlDLListIterator<SFXPlaySet*> iterator = mpCurPlaySet.Begin();

    while (iterator.hasNext())
    {
        SFXPlaySet* pTrackedSFX = *iterator;
        nlDLListIterator<SFXPlaySet*> iterToCurrentItem = iterator;
        iterator.next();

        if (pTrackedSFX->type == (unsigned long)-1)
        {
            RemoveTrackedSFX(&iterToCurrentItem);
            continue;
        }

        currTime = Audio::GetAudioTimer();

        if (pTrackedSFX->delay >= 0.0f)
        {
            if (currTime > pTrackedSFX->timeStamp + pTrackedSFX->delay + 15.0f)
            {
                RemoveTrackedSFX(&iterToCurrentItem);
            }
            continue;
        }

        if (pTrackedSFX->bIs3D)
        {
            if (pTrackedSFX->emitter != NULL)
            {
                if (pTrackedSFX->type != pTrackedSFX->emitter->soundType || pTrackedSFX->emitter->pOwner != this)
                {
                    RemoveTrackedSFX(&iterToCurrentItem);
                    continue;
                }

                if (Audio::IsEmitterActive(pTrackedSFX->emitter))
                {
                    unsigned long sndIDError = Audio::GetSndIDError();
                    unsigned long emitterVoiceID = Audio::GetEmitterVoiceID(pTrackedSFX->emitter);
                    if (emitterVoiceID != sndIDError)
                    {
                        if (g_pGame != NULL)
                        {
                            pTrackedSFX->voiceID = Audio::GetEmitterVoiceID(pTrackedSFX->emitter);
                            UpdateGroupFilterStatusOnSFX(pTrackedSFX);
                            UpdateGroupPitchStatusOnSFX(pTrackedSFX);
                        }
                        continue;
                    }
                }

                if (!Audio::IsEmitterActive(pTrackedSFX->emitter) && pTrackedSFX->timeStamp > -1.0f && currTime > pTrackedSFX->timeStamp + 0.1f)
                {
                    UpdateGroupFilterStatusOnSFX(pTrackedSFX);
                    UpdateGroupPitchStatusOnSFX(pTrackedSFX);

                    SFXEmitter* pSFXEmitter = pTrackedSFX->emitter;
                    pSFXEmitter->bKeepTrack = true;
                    pSFXEmitter->Init();
                    pSFXEmitter->pos.vPos.x = 0.0f;
                    pSFXEmitter->pos.vPos.y = 0.0f;
                    pSFXEmitter->pos.vPos.z = 0.0f;
                    pSFXEmitter->dir.vDir.x = 0.0f;
                    pSFXEmitter->dir.vDir.y = 0.0f;
                    pSFXEmitter->dir.vDir.z = 0.0f;
                    pSFXEmitter->posUpdateMethod = NONE;

                    if (pSFXEmitter->pMIDIControllerInfo != NULL)
                    {
                        if (pSFXEmitter->pMIDIControllerInfo->paraArray != NULL)
                        {
                            delete[] pSFXEmitter->pMIDIControllerInfo->paraArray;
                        }
                        delete pSFXEmitter->pMIDIControllerInfo;
                    }
                    pSFXEmitter->pMIDIControllerInfo = NULL;
                    RemoveTrackedSFX(&iterToCurrentItem);
                }

                continue;
            }

            if (pTrackedSFX->voiceID != Audio::GetSndIDError())
            {
                if (!Audio::IsSFXPlaying(pTrackedSFX->voiceID))
                {
                    RemoveTrackedSFX(&iterToCurrentItem);
                }
                continue;
            }

            nlPrintf("Could not get valid emitter or voice ID to check sound type %d\n", pTrackedSFX->type);
            continue;
        }

        if (pTrackedSFX->voiceID != Audio::GetSndIDError())
        {
            if (!Audio::IsSFXPlaying(pTrackedSFX->voiceID))
            {
                RemoveTrackedSFX(&iterToCurrentItem);
            }
            continue;
        }

        RemoveTrackedSFX(&iterToCurrentItem);
        nlPrintf("Could not get valid emitter or voice ID to check sound type %d\n", pTrackedSFX->type);
    }
}

SFXPlaySet* cGameSFX::RemoveTrackedSFX(unsigned long position)
{
    nlDLListIterator<SFXPlaySet*> iterator = mpCurPlaySet.Begin();
    while (position != 0 && iterator.hasNext())
    {
        iterator.next();
        position--;
    }

    SFXPlaySet* pTrackedSFX = mpCurPlaySet.RemoveEntry(iterator.CurrentEntry());
    pTrackedSFX->type = (unsigned long)-1;
    pTrackedSFX->voiceID = Audio::GetSndIDError();
    pTrackedSFX->bIs3D = false;
    pTrackedSFX->emitter = NULL;
    pTrackedSFX->delay = -1.0f;
    pTrackedSFX->timeStamp = -1.0f;
    pTrackedSFX->sfxPriority = 0;
    pTrackedSFX->groupPriority = -1;
    pTrackedSFX->filterFreq = 0;
    pTrackedSFX->pitch = 0x2000;
    SFXPlaySet::m_TrackedSFXSlotPool.Free(pTrackedSFX);
    return pTrackedSFX;
}

SFXPlaySet* cGameSFX::RemoveTrackedSFX(nlDLListIterator<SFXPlaySet*>* pIter)
{
    SFXPlaySet* pTrackedSFX = **pIter;
    SFXPlaySet* pNextTrackedSFX = NULL;
    mpCurPlaySet.Remove(pIter);
    if (pIter->hasNext())
    {
        pNextTrackedSFX = **pIter;
    }
    pTrackedSFX->type = (unsigned long)-1;
    pTrackedSFX->voiceID = Audio::GetSndIDError();
    pTrackedSFX->bIs3D = false;
    pTrackedSFX->emitter = NULL;
    pTrackedSFX->delay = -1.0f;
    pTrackedSFX->timeStamp = -1.0f;
    pTrackedSFX->sfxPriority = 0;
    pTrackedSFX->groupPriority = -1;
    pTrackedSFX->filterFreq = 0;
    pTrackedSFX->pitch = 0x2000;
    SFXPlaySet::m_TrackedSFXSlotPool.Free(pTrackedSFX);
    return pNextTrackedSFX;
}

unsigned long cGameSFX::GetSFXID(unsigned long type) const
{
    return mpSFX[type].musyxID;
}

void cGameSFX::SetSFXInfo(unsigned long type, unsigned long ID, float fVol, float fVolReverb, int volGroup, int sfxPriority)
{
    mpSFX[type].typeID = type;
    mpSFX[type].musyxID = ID;
    mpSFX[type].fVolume = fVol;
    mpSFX[type].fVolReverb = fVolReverb;
    mpSFX[type].volGrp = volGroup;
    mpSFX[type].sfxPriority = sfxPriority;
    mpSFX[type].pOwner = this;
}

SoundPropAccessor* cGameSFX::GetSoundPropAccessor(unsigned long type)
{
    return mpSFX[type].pSoundPropAccessor;
}
