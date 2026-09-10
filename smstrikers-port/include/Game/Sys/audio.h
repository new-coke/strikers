#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "types.h"
#include "NL/nlMath.h"
#include "NL/plat/plataudio.h"

#include "Game/Render/Nis.h"

namespace AudioStreamTrack
{
class TrackManagerBase;
}
extern AudioStreamTrack::TrackManagerBase* g_pTrackManager;

class PhysicsObject;
class cGameSFX;

struct SoundEventData
{
    const char* eventName; // offset 0x0, size 0x4
    int eventPriority;     // offset 0x4, size 0x4
}; // total size: 0x8

namespace Audio
{

struct SoundAttributes
{
    u32 me_ClassType;             // offset 0x0, size 0x4
    u32 mu_Type;                  // offset 0x4, size 0x4
    u32 mu_SfxID;                 // offset 0x8, size 0x4
    u32 mu_VoiceID;               // offset 0xC, size 0x4
    float mf_Volume;              // offset 0x10, size 0x4
    float mf_VolReverb;           // offset 0x14, size 0x4
    float mf_Attenuate;           // offset 0x18, size 0x4
    float mf_VolAdjustment;       // offset 0x1C, size 0x4
    float mf_Panning;             // offset 0x20, size 0x4
    float mf_DelayTime;           // offset 0x24, size 0x4
    float mf_DebugTimer;          // offset 0x28, size 0x4
    bool mb_Is3D;                 // offset 0x2C, size 0x1
    bool mb_IsPlaying;            // offset 0x2D, size 0x1
    bool mb_KeepTrack;            // offset 0x2E, size 0x1
    bool mb_HasCutoff;            // offset 0x2F, size 0x1
    bool mb_Update3DContinuously; // offset 0x30, size 0x1
    bool mb_Pausable;             // offset 0x31, size 0x1
    bool mb_Restartable;          // offset 0x32, size 0x1
    bool mb_UseDoppler;           // offset 0x33, size 0x1
    bool mf_ReturnEmitterOnPlay;  // offset 0x34, size 0x1
    float mf_CutoffTime;          // offset 0x38, size 0x4
    cGameSFX* mp_OwnerSFX;        // offset 0x3C, size 0x4
    PhysicsObject* mp_PhysObj;    // offset 0x40, size 0x4
    union
    {
        const class nlVector3* pvPos; // offset 0x0, size 0x4
        class nlVector3 vPos;         // offset 0x0, size 0xC
    } pos;                            // offset 0x44, size 0xC
    union
    {
        const class nlVector3* pvDir; // offset 0x0, size 0x4
        class nlVector3 vDir;         // offset 0x0, size 0xC
    } dir;                            // offset 0x50, size 0xC
    PosUpdateMethod posUpdateMethod;  // offset 0x5C, size 0x4
    const char* ms_EventName;         // offset 0x60, size 0x4
    int mi_SFXPriority;               // offset 0x64, size 0x4
    int mi_GroupPriority;             // offset 0x68, size 0x4
    int mi_VolGroup;                  // offset 0x6C, size 0x4
    int mi_EmitterGroup;              // offset 0x70, size 0x4
    bool mb_FilterOn;                 // offset 0x74, size 0x1
    unsigned short mu_FilterFreq;     // offset 0x76, size 0x2
    unsigned short mu_Pitch;          // offset 0x78, size 0x2
    bool mb_NoPhasingFilter;          // offset 0x7A, size 0x1
    /* 0x7B */ bool m_unk_0x7B;
    /* 0x7C */ bool m_unk_0x7C;

    // /* 0x00 */ s32 m_unk_0x00;
    // /* 0x04 */ u32 m_soundType;
    // /* 0x08 */ s32 m_unk_0x08;
    // /* 0x0C */ s32 m_unk_0x0C;
    // /* 0x10 */ f32 m_unk_0x10;
    // /* 0x14 */ f32 m_unk_0x14;
    // /* 0x18 */ f32 m_unk_0x18;
    // /* 0x1C */ f32 m_unk_0x1C;
    // /* 0x20 */ f32 m_unk_0x20;
    // /* 0x24 */ f32 m_unk_0x24;
    // /* 0x28 */ f32 m_unk_0x28;
    // /* 0x2C */ bool m_unk_0x2C;
    // /* 0x2D */ bool m_unk_0x2D;
    // /* 0x2E */ bool m_unk_0x2E;
    // /* 0x2F */ bool m_unk_0x2F;
    // /* 0x30 */ bool m_unk_0x30;
    // /* 0x31 */ bool m_unk_0x31;
    // /* 0x32 */ bool m_unk_0x32;
    // /* 0x33 */ bool m_unk_0x33;
    // /* 0x34 */ bool m_unk_0x34;
    // /* 0x35 */ char m_pad_0x35[3];
    // /* 0x38 */ f32 m_unk_0x38;
    // /* 0x3C */ s32 m_unk_0x3C;
    // /* 0x40 */ PhysicsObject* m_physicsObject;

    // union
    // {
    //     nlVector3 m_vec_0x44;
    //     const nlVector3* m_vecPtr_0x44;
    // };
    // union
    // {
    //     nlVector3 m_vec_0x50;
    //     const nlVector3* m_vecPtr_0x50;
    // };

    // /* 0x5C */ s32 m_unk_0x5C;
    // /* 0x60 */ s32 m_unk_0x60;
    // /* 0x64 */ s32 m_unk_0x64;
    // /* 0x68 */ s32 m_unk_0x68;
    // /* 0x6C */ s32 m_unk_0x6C;
    // /* 0x70 */ s32 m_unk_0x70;
    // /* 0x74 */ s8 m_unk_0x74;
    // /* 0x75 */ char m_pad_0x75[1];
    // /* 0x76 */ s16 m_unk_0x76;
    // /* 0x78 */ s16 m_unk_0x78;
    // /* 0x7A */ s8 m_unk_0x7A;
    // /* 0x7B */ s8 m_unk_0x7B;
    // /* 0x7C */ s8 m_unk_0x7C;

    void UseStationaryPosVector(const nlVector3&position);
    void UseVectors(const nlVector3&p, const nlVector3&d);
    void UseVectorPtrs(const nlVector3*v1, const nlVector3*v2);
    void UsePhysObj(PhysicsObject*obj);
    void SetSoundType(unsigned long soundType, bool bIs3D);
    void Init();
}; // total size: 0x7C

namespace MasterVolume
{

enum VOLUME_GROUP
{
    VG_Special = 0,
    VG_Music = 1,
    VG_SFX = 2,
    VG_Voice = 3,
};

float GetVoiceVolume();
void SetVoiceVolume(float volume, int time);
void SetVolume(VOLUME_GROUP group, float volume);
float GetVolume(VOLUME_GROUP group);

}; // namespace MasterVolume

void FadeFilterFromCurrentToZero();
void FadeFilterToFullStrength();
void PitchBend(float currentVal, float fadeToVal, float fadeDuration, float fadeTimeStart);
void FadeFilter(float currentVal, float fadeToVal, float fadeDuration, float fadeTimeStart);
void ClearFadeData();
bool IsEmitterActive(SFXEmitter*emitter);
u32 GetEmitterVoiceID(SFXEmitter*emitter);
bool Remove3DSFXEmitter(SFXEmitter*emitter);
unsigned long Add3DSFXEmitter(const EmitterStartInfo&emitterStartInfo);
SFXEmitter* GetFreeEmitter(unsigned long&id);
SFXEmitter* GetEmitter(unsigned long id);
extern SND_LISTENER gListener;
extern unsigned long uSFXVolume;
void SetListenerActive(bool active);
bool IsListenerActive();
void SetOutputMode(MusyXOutputType outputType);
bool SetPitchBendOnSFX(unsigned long uVoiceID, unsigned short pitch);
bool SetFilterFreqOnSFX(unsigned long uVoiceID, unsigned short freq);
bool ActivateFilterOnSFX(unsigned long uVoiceID, bool bOn);
void SetPitchBendOnAllDialogueSFX(unsigned short pitch);
void ActivateFilterOnAllCurrentSFX(bool bOn);
void SetVolGroupVolume(int volGroup, float fVol, int fadeTime);
void SetSFXVolumeGroup(unsigned long uSFXID, int volGroup);
void SetSFXVolume(unsigned long voiceID, float volume);
void Update3DSFXEmitters();
void UpdateFades(float fDeltaT);
void Update(float fDeltaT);
int GetSndIDError();
bool IsSFXPlaying(unsigned long sfxID);
bool StopSFX(unsigned long sfxID);
unsigned long PlaySFXEventFromScript(const SoundEventData&sfxEventData, const char*szSFXType, float fVol, float fDelay);
void StopCharSFXbyStr(const char* szSFXType, NisCharacterClass charIdentifier);
void StopWorldSFXbyStr(const char* szSFXType);
uintptr_t PlayCharSFXbyStr(const char* szSFXType, NisCharacterClass charIdentifier, float fVol, float fDelay, bool bIs3D, bool bKeepTrack, const nlVector3* pInitialPosVector, const nlVector3* pInitialDirVector, unsigned long* pType);
uintptr_t PlayWorldSFXbyStr(const char* szSFXType, float fVol, float fDelay, bool bIs3D, bool bKeepTrack, const nlVector3* pInitialPosVector, const nlVector3* pInitialDirVector, unsigned long* pType);
void RemoveDelayedSFX(unsigned long index);
int IsDelayedCharSFX(unsigned long sfxType, cGameSFX* pOwner);
int AddDelayedSFX(const Audio::SoundAttributes&sfxData, unsigned long uSFXID, float volume, float delay, cGameSFX*pOwnerSFX);
unsigned long PlaySFXbyID(const Audio::SoundAttributes&attrs, unsigned long sfxID, float fVol, float fRevVol, int volGroup);
unsigned long PlaySFX(const SFXStartInfo&info);
float GetAudioTimer();
void Shutdown();
void Silence();
void ResetForNewGame();
void ResetPauseStatus();
void UnloadWorldSFX();
bool IsWorldSFXLoaded();
void LoadWorldSFX();
void UnloadInGameSFX();
void LoadInGameSFX();
bool IsInited();
bool Initialize(bool bInit);
bool ShutdownReverb();
// void InitializeReverb(eStadiumID, unsigned char);

// eCharSFX enum is defined in CharacterAudio.h

}; // namespace Audio

// class cGameSFX
// {
// public:
//     void GetClassType() const;
// };

// class nlBSearch<nlSortedSlot<AudioStreamTrack
// {
// public:
//     void StreamTrack, 3>::EntryLookup<AudioStreamTrack::StreamTrack>, unsigned long>(const unsigned long&,
//     nlSortedSlot<AudioStreamTrack::StreamTrack, 3>::EntryLookup<AudioStreamTrack::StreamTrack>*, int);
// };

// class nlWalkDLRing<DLListEntry<AudioStreamTrack
// {
// public:
//     void TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
//     DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
//     DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>*, void
//     (DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*));
//     void StreamTrack::QUEUED_STREAM>, DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
//     nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>,
//     4>>>(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*, DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
//     nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>, 4>>*, void
//     (DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
//     nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>,
//     4>>::*)(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*)); void TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
//     WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
//     AudioStreamTrack::TrackManagerBase::FadeManager>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
//     WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>, AudioStreamTrack::TrackManagerBase::FadeManager>*,
//     void (WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
//     AudioStreamTrack::TrackManagerBase::FadeManager>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*));
// };

// class nlWalkDLRing<DLListEntry<GCAudioStreaming
// {
// public:
//     void StereoAudioStream*>, DLListContainerBase<GCAudioStreaming::StereoAudioStream*,
//     BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>>(DLListEntry<GCAudioStreaming::StereoAudioStream*>*,
//     DLListContainerBase<GCAudioStreaming::StereoAudioStream*, BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>*, void
//     (DLListContainerBase<GCAudioStreaming::StereoAudioStream*,
//     BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>::*)(DLListEntry<GCAudioStreaming::StereoAudioStream*>*));
// };

// class nlDLRingIsEnd<DLListEntry<AudioStreamTrack
// {
// public:
//     void
//     TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*);
// };

// class nlDLRingGetStart<DLListEntry<AudioStreamTrack
// {
// public:
//     void StreamTrack::QUEUED_STREAM>>(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*);
//     void
//     TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*);
// };

// class AudioStreamTrack
// {
// public:
//     void TrackManagerBase::~TrackManagerBase();
//     void TrackManager<3>::~TrackManager();
//     void TrackManager<3>::OnMasterVolumeChange(Audio::MasterVolume::VOLUME_GROUP);
//     void TrackManager<3>::GetTrack(unsigned long);
//     void TrackManager<3>::StopAllTracks(unsigned long);
//     void TrackManager<3>::Update(float);
//     void TrackManager<3>::DestroyAllTracks();
//     void TrackManager<3>::CreateTrack(const char*, Audio::MasterVolume::VOLUME_GROUP);
// };

// class nlStaticSortedSlot<AudioStreamTrack
// {
// public:
//     void StreamTrack, 3>::ExpandLookup();
//     void StreamTrack, 3>::FreeLookup();
//     void StreamTrack, 3>::FreeEntry(AudioStreamTrack::StreamTrack*);
//     void StreamTrack, 3>::GetNewEntry();
// };

// class nlWalkRing<DLListEntry<GCAudioStreaming
// {
// public:
//     void StereoAudioStream*>, DLListContainerBase<GCAudioStreaming::StereoAudioStream*,
//     BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>>(DLListEntry<GCAudioStreaming::StereoAudioStream*>*,
//     DLListContainerBase<GCAudioStreaming::StereoAudioStream*, BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>*, void
//     (DLListContainerBase<GCAudioStreaming::StereoAudioStream*,
//     BasicSlotPool<DLListEntry<GCAudioStreaming::StereoAudioStream*>>>::*)(DLListEntry<GCAudioStreaming::StereoAudioStream*>*));
// };

// class nlWalkRing<DLListEntry<AudioStreamTrack
// {
// public:
//     void TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
//     DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
//     DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>*, void
//     (DLListContainerBase<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     BasicSlotPool<DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>>>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*));
//     void TrackManagerBase::FadeManager::STREAM_FADE_CTRL>, WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
//     AudioStreamTrack::TrackManagerBase::FadeManager>>(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*,
//     WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>, AudioStreamTrack::TrackManagerBase::FadeManager>*,
//     void (WalkHelper<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL,
//     DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>,
//     AudioStreamTrack::TrackManagerBase::FadeManager>::*)(DLListEntry<AudioStreamTrack::TrackManagerBase::FadeManager::STREAM_FADE_CTRL>*));
//     void StreamTrack::QUEUED_STREAM>, DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
//     nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>,
//     4>>>(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*, DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
//     nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>, 4>>*, void
//     (DLListContainerBase<AudioStreamTrack::StreamTrack::QUEUED_STREAM,
//     nlStaticArrayAllocator<DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>,
//     4>>::*)(DLListEntry<AudioStreamTrack::StreamTrack::QUEUED_STREAM>*));
// };

#endif // _AUDIO_H_
