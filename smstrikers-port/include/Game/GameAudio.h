#ifndef _GAMEAUDIO_H_
#define _GAMEAUDIO_H_

#include "types.h"
#include "Game/Sys/audio.h"
#include "NL/plat/plataudio.h"

#include "NL/nlMath.h"
#include "NL/nlDLRing.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlSlotPool.h"

enum eClassType
{
    GAME = 0,
    CHAR = 1,
    WORLD = 2,
};

// Forward declarations
namespace Audio
{
struct SoundAttributes;
}

struct SFXPlaySet
{
    /* 0x00 */ unsigned long type;
    /* 0x04 */ unsigned long voiceID;
    /* 0x08 */ unsigned char bIs3D;
    /* 0x0C */ SFXEmitter* emitter;
    /* 0x10 */ float delay;
    /* 0x14 */ float timeStamp;
    /* 0x18 */ int sfxPriority;
    /* 0x1C */ int groupPriority;
    /* 0x20 */ unsigned short filterFreq;
    /* 0x22 */ unsigned short pitch;

    static SlotPool<SFXPlaySet> m_TrackedSFXSlotPool;
}; // total size: 0x24

class SoundPropAccessor
{
public:
    ~SoundPropAccessor();

    virtual SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual SoundProperties* GetSoundPropTable();
    virtual int GetNumSFX();
    virtual char* GetSoundPropTableName();
    virtual char* GetHTMLFileName();
    virtual unsigned char IsUsingOrigTable() const;
    virtual void SetSoundPropTable(SoundProperties* pNewTable);
    virtual void ResetSoundPropTable();

    /* 0x4 */ SoundProperties* mpSoundProp;
    /* 0x8 */ unsigned char mbIsReloaded;
}; // total size: 0xC

class cGameSFX
{
public:
    enum StopFlag
    {
        SFX_STOP_ALL = 0,
        SFX_STOP_FIRST = 1,
        SFX_STOP_OLDEST = 2,
    };

    cGameSFX();
    virtual ~cGameSFX();

    virtual void Init();
    virtual void DeInit();
    void SetupPlaySet();
    void ShutdownPlaySet();
    virtual void SetSFX(SoundPropAccessor* pSoundPropAccessor);
    void CheckTypeMap(SoundPropAccessor* pSoundPropAccessor) const;
    void ResetSFX();
    float GetSFXVol(const Audio::SoundAttributes& sfxData) const;
    float GetSFXVol(unsigned long type) const;
    float GetSFXVolReverb(const Audio::SoundAttributes& sfxData) const;
    float GetSFXVolReverb(unsigned long type) const;
    int GetVolGroup(unsigned long type) const;
    int GetSFXPriority(unsigned long type) const;
    bool IsKeepingTrackOf(unsigned long type, SFXPlaySet** pGrabTrackedSFX);
    bool InitiateCallbackOnAllTrackedSFX(
        bool (*pTrackedSFXCallback)(SFXPlaySet*, unsigned long, cGameSFX*),
        unsigned long param,
        bool (*pTrackedSFXTypeCallback)(unsigned long, cGameSFX*));
    bool ActivateFilterOnAllTrackedSFX(bool bOn);
    bool SetFilterFreqOnAllTrackedSFX(unsigned short freq);
    bool SetPitchBendOnAllDialogueSFX(unsigned short pitch);
    bool CheckForHigherPrioritySFX(int priority);
    bool KillLowerPrioritySFX(int priority);
    virtual uintptr_t Play(Audio::SoundAttributes& sfxData);
    virtual eClassType GetClassType() const { return meClassType; }
    SFXPlaySet* KeepTrack(SFXEmitter* pSFXEmitter, const Audio::SoundAttributes& sfxData, unsigned long uVoiceID);
    void Stop(unsigned long type, cGameSFX::StopFlag stopFlag);
    void StopEmitter(SFXEmitter* pSFXEmitter, unsigned long type);
    bool StopTrackedSFX(SFXPlaySet* pSFXPlaySet);
    bool StopTrackedSFX(nlDLListIterator<SFXPlaySet*>* pIter);
    void StopPlayingAllTrackedSFX();
    void UpdateGroupFilterStatusOnSFX(SFXPlaySet* pTrackedSFX);
    void UpdateGroupPitchStatusOnSFX(SFXPlaySet* pTrackedSFX);
    void UpdateAllTrackedSFX(float fDeltaT);
    SFXPlaySet* RemoveTrackedSFX(unsigned long position);
    SFXPlaySet* RemoveTrackedSFX(nlDLListIterator<SFXPlaySet*>* pIter);
    unsigned long GetSFXID(unsigned long type) const;
    void SetSFXInfo(unsigned long type, unsigned long ID, float fVol, float fVolReverb, int volGroup, int sfxPriority);
    SoundPropAccessor* GetSoundPropAccessor(unsigned long type);
    // PORT: mpSFX is null until Init(), which is skipped when audio is not inited.
    const SoundStrToIDNode& GetSFXInfo(unsigned long type) const
    {
        if (mpSFX == NULL || type >= mNumSFXTypes)
        {
            static const SoundStrToIDNode kNoSFX = {};
            return kNoSFX;
        }
        return mpSFX[type];
    }
    bool IsInited() const { return mbInited; }
    unsigned short GetGroupFilterFreq() const { return muGroupFilterFreq; }
    unsigned short GetGroupPitch() const { return muGroupPitch; }

protected:
    /* 0x04 */ bool mbInited;
    /* 0x08 */ unsigned long mNumSFX;
    /* 0x0C */ unsigned long mNumSFXTypes;
    /* 0x10 */ SoundStrToIDNode* mpSFX;
    /* 0x14 */ nlDLListContainer<SFXPlaySet*> mpCurPlaySet;
    /* 0x1C */ bool bCurPlaySetIsValid;
    /* 0x20 */ float mfTrackedSFXCheckInterval;
    /* 0x24 */ const char** mpSoundStrTable;
    /* 0x28 */ eClassType meClassType;
    /* 0x2C */ bool mbGroupFilterOn;
    /* 0x2E */ unsigned short muGroupFilterFreq;
    /* 0x30 */ unsigned short muGroupPitch;
}; // total size: 0x34

#endif // _GAMEAUDIO_H_
