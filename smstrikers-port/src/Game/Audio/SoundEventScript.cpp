#include "Game/Audio/SoundEventScript.h"

#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/Audio/PriorityStream.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/Sys/audio.h"
#include "NL/nlDebug.h"

SoundEventScript* SoundEventScript::pInstance;

static inline unsigned long PriorityStreamToStreamId(int stream)
{
    switch (stream)
    {
    case 0:
        return 0xE38B5407;
    case 1:
        return 0x436E3953;
    case 2:
        return 0x57CB5A12;
    case 3:
        return 0x09451A58;
    case 4:
        return 0xA207B1AE;
    default:
        return 0xFFFFFFFF;
    }
}

static inline void PlayPriorityStreamParam(int stream, float volume, unsigned long looping, unsigned long fadeIn, unsigned long existingFadeOut, const char* param)
{
    Audio::GetPriorityStream()->PlayStream(PriorityStreamToStreamId(stream), volume, (bool)looping, existingFadeOut, fadeIn, param);
}

static inline void PlayStreamParam(unsigned long TrackName, unsigned long StreamId, float Volume, unsigned long Looping, unsigned long FadeIn, unsigned long ExistingFadeOut, const char* StreamParam)
{
    Audio::MasterVolume::VOLUME_GROUP volGroup;
    AudioStreamTrack::StreamTrack* track = AudioStreamTrack::TrackManagerBase::Get()->GetTrack(TrackName);

    switch (TrackName)
    {
    case 0x05A165C0:
        Audio::MasterVolume::GetVolume(Audio::MasterVolume::VG_Music);
        volGroup = Audio::MasterVolume::VG_Music;
        break;
    case 0xC25BA8E8:
        Audio::MasterVolume::GetVoiceVolume();
        volGroup = Audio::MasterVolume::VG_Voice;
        break;
    case 0x78ABFED1:
        Audio::MasterVolume::GetVolume(Audio::MasterVolume::VG_SFX);
        AudioStreamTrack::TrackManagerBase* mgr = AudioStreamTrack::TrackManagerBase::Get();
        track = mgr->GetTrack(nlStringLowerHash("Music"));
        volGroup = Audio::MasterVolume::VG_SFX;
        break;
    }

    track->PlayStream(StreamId, Volume, Looping == 1, ExistingFadeOut, FadeIn, StreamParam, volGroup);
}

static inline void PlayStream(unsigned long TrackName, unsigned long StreamId, float Volume, unsigned long Looping, unsigned long FadeIn, unsigned long ExistingFadeOut)
{
    PlayStreamParam(TrackName, StreamId, Volume, Looping, FadeIn, ExistingFadeOut, NULL);
}

static inline void PlayRandomBooBig(float delay, float fVol, int priority)
{
    if (Audio::gCrowdSFX.IsInited())
    {
        if (fVol >= 0.0f)
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_BOO_BIG, fVol, delay, priority, 0.0f);
        }
        else
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_BOO_BIG, 100.0f, delay, priority, 0.0f);
        }
    }
}

static inline void PlayRandomBooSmall(float delay, float fVol, int priority)
{
    if (Audio::gCrowdSFX.IsInited())
    {
        if (fVol >= 0.0f)
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_BOO_SMALL, fVol, delay, priority, 0.0f);
        }
        else
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_BOO_SMALL, 100.0f, delay, priority, 0.0f);
        }
    }
}

static inline void PlayRandomYeahSmall(float delay, float fVol, int priority)
{
    if (Audio::gCrowdSFX.IsInited())
    {
        if (fVol >= 0.0f)
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_YEAH_SMALL, fVol, delay, priority, 0.0f);
        }
        else
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_YEAH_SMALL, 100.0f, delay, priority, 0.0f);
        }
    }
}

static inline void PlayRandomOhSmall(float delay, float fVol, int priority)
{
    if (Audio::gCrowdSFX.IsInited())
    {
        if (fVol >= 0.0f)
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_OH_SMALL, fVol, delay, priority, 0.0f);
        }
        else
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_OH_SMALL, 100.0f, delay, priority, 0.0f);
        }
    }
}

static inline void PlayRandomOhBig(float delay, float fVol, int priority)
{
    if (Audio::gCrowdSFX.IsInited())
    {
        if (fVol >= 0.0f)
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_OH_BIG, fVol, delay, priority, 0.0f);
        }
        else
        {
            Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_OH_BIG, 100.0f, delay, priority, 0.0f);
        }
    }
}

/**
 * Offset/Address/Size: 0x1A0 | 0x80153FD8 | size: 0x96C
 */
void SoundEventScript::DoFunctionCall(unsigned int func)
{
    switch (func)
    {
    case 0:
    {
        m_SP--;
        uintptr_t uIntensity = *m_SP;
        m_SP--;
        CrowdMood::CROWD_MOOD mood = (CrowdMood::CROWD_MOOD)*m_SP;
        CrowdMood::AdjustMood(mood, uIntensity);
        break;
    }
    case 1:
    {
        m_SP--;
        bool enabled = *m_SP != 0;
        CrowdMood::EnableCrowdDecay(enabled);
        break;
    }
    case 2:
    {
        CrowdMood::InitiateFastCrowdTransition();
        break;
    }
    case 3:
    {
        m_SP--;
        m_SP--;
        break;
    }
    case 4:
    {
        m_SP--;
        uintptr_t fadeIn = *m_SP;
        m_SP--;
        uintptr_t fadeOut = *m_SP;
        m_SP--;
        uintptr_t loop = *m_SP;
        m_SP--;
        float vol = *(float*)m_SP;
        m_SP--;
        uintptr_t streamSelect = *m_SP;

        PlayPriorityStreamParam(streamSelect, vol, loop, fadeIn, fadeOut, NULL);
        break;
    }
    case 5:
    {
        m_SP--;
        const char* name = (const char*)*m_SP;
        m_SP--;
        uintptr_t fadeIn = *m_SP;
        m_SP--;
        uintptr_t fadeOut = *m_SP;
        m_SP--;
        uintptr_t loop = *m_SP;
        m_SP--;
        float vol = *(float*)m_SP;
        m_SP--;
        uintptr_t streamSelect = *m_SP;

        PlayPriorityStreamParam(streamSelect, vol, loop, fadeIn, fadeOut, name);
        break;
    }
    case 6:
    {
        m_SP--;
        uintptr_t range = *m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        float fDelay = *(float*)m_SP;
        PlayRandomBooBig(fDelay, fVol, range);
        break;
    }
    case 7:
    {
        m_SP--;
        uintptr_t range = *m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        float fDelay = *(float*)m_SP;
        PlayRandomBooSmall(fDelay, fVol, range);
        break;
    }
    case 8:
    {
        m_SP--;
        uintptr_t range = *m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        float fDelay = *(float*)m_SP;
        PlayRandomOhBig(fDelay, fVol, range);
        break;
    }
    case 9:
    {
        m_SP--;
        uintptr_t range = *m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        float fDelay = *(float*)m_SP;
        PlayRandomOhSmall(fDelay, fVol, range);
        break;
    }
    case 10:
    {
        m_SP--;
        uintptr_t range = *m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        float fDelay = *(float*)m_SP;
        PlayRandomYeahSmall(fDelay, fVol, range);
        break;
    }
    case 11:
    {
        SoundEventData data;
        m_SP--;
        uintptr_t eventPrio = *m_SP;
        m_SP--;
        const char* sfxType = (const char*)*m_SP;
        m_SP--;
        float fDelay = *(float*)m_SP;
        data.eventName = mCurrentFunction;
        data.eventPriority = eventPrio;
        Audio::PlaySFXEventFromScript(data, sfxType, 100.0f, fDelay);
        break;
    }
    case 12:
    {
        SoundEventData data;
        m_SP--;
        uintptr_t eventPrio = *m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        const char* sfxType = (const char*)*m_SP;
        m_SP--;
        float fDelay = *(float*)m_SP;
        data.eventName = mCurrentFunction;
        data.eventPriority = eventPrio;
        Audio::PlaySFXEventFromScript(data, sfxType, fVol, fDelay);
        break;
    }
    case 13:
    {
        unsigned long fadeIn;
        unsigned long fadeOut;
        unsigned long streamId;
        unsigned long loop;
        float vol;

        m_SP--;
        fadeIn = *m_SP;
        m_SP--;
        fadeOut = *m_SP;
        m_SP--;
        loop = *m_SP;
        m_SP--;
        vol = *(float*)m_SP;
        m_SP--;
        streamId = *m_SP;
        uintptr_t trackHash = Pop();

        PlayStream(trackHash, streamId, vol, loop, fadeIn, fadeOut);
        break;
    }
    case 14:
    {
        unsigned long streamId;
        const char* name;
        unsigned long fadeIn;
        unsigned long fadeOut;
        unsigned long loop;
        float vol;

        m_SP--;
        name = (const char*)*m_SP;
        m_SP--;
        fadeIn = *m_SP;
        m_SP--;
        fadeOut = *m_SP;
        m_SP--;
        loop = *m_SP;
        m_SP--;
        vol = *(float*)m_SP;
        m_SP--;
        streamId = *m_SP;
        uintptr_t trackHash = Pop();

        PlayStreamParam(trackHash, streamId, vol, loop, fadeIn, fadeOut, name);
        break;
    }
    case 15:
    {
        m_SP--;
        uintptr_t arg2 = *m_SP;
        m_SP--;
        CrowdMood::CROWD_MOOD mood = (CrowdMood::CROWD_MOOD)*m_SP;
        CrowdMood::SetMood(mood, arg2);
        break;
    }
    case 16:
    {
        m_SP--;
        uintptr_t arg2 = *m_SP;
        m_SP--;
        uintptr_t streamSelect = *m_SP;

        unsigned long streamId;
        switch (streamSelect)
        {
        case 0:
            streamId = 0xE38B5407;
            break;
        case 1:
            streamId = 0x436E3953;
            break;
        case 2:
            streamId = 0x57CB5A12;
            break;
        case 3:
            streamId = 0x09451A58;
            break;
        case 4:
            streamId = 0xA207B1AE;
            break;
        default:
            streamId = 0xFFFFFFFF;
            break;
        }
        Audio::GetPriorityStream()->Stop(streamId, arg2);
        break;
    }
    case 17:
    {
        m_SP--;
        uintptr_t streamId = *m_SP;
        uintptr_t trackHash = Pop();
        AudioStreamTrack::TrackManagerBase::Get()->GetTrack(trackHash)->Stop(streamId);
        break;
    }
    case 18:
    {
        m_SP--;
        const char* sfxName = (const char*)*m_SP;
        m_SP--;
        Audio::StopWorldSFXbyStr(sfxName);
        break;
    }
    default:
        nlBreak();
        break;
    }
}

/**
 * Offset/Address/Size: 0x118 | 0x80153F50 | size: 0x88
 */
void SoundEventScript::CreateInstance()
{
    pInstance = new (nlMalloc(sizeof(SoundEventScript), 8, 0)) SoundEventScript();
}

/**
 * Offset/Address/Size: 0xC8 | 0x80153F00 | size: 0x50
 */
void SoundEventScript::DestroyInstance()
{
    delete[] pInstance->pByteCode;
    delete pInstance;
    pInstance = NULL;
}

/**
 * Offset/Address/Size: 0xC0 | 0x80153EF8 | size: 0x8
 */
SoundEventScript& SoundEventScript::Instance()
{
    return *pInstance;
}

/**
 * Offset/Address/Size: 0x60 | 0x80153E98 | size: 0x60
 */
void SoundEventScript::Call(const char* functionName)
{
    if (AudioLoader::gbDisableAudio == false)
    {
        nlStrNCpy<char>(mCurrentFunction, functionName, 0x40);
        InterpreterCore::CallFunction(nlStringHash(functionName));
    }
}
