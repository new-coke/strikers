#ifndef _PLATAUDIO_H_
#define _PLATAUDIO_H_

#include "types.h"
#include "NL/nlMath.h"
#include "musyx/musyx.h"

// Forward declarations
class PhysicsObject;
class cGameSFX;

// PosUpdateMethod enum - defined here to avoid circular dependency with audio.h
enum PosUpdateMethod
{
    NONE = 0,
    PHYSOBJ = 1,
    VECTORS = 2,
    PTRS_TO_VECTORS = 3,
};

enum MusyXEffectType
{
    MUSYX_EFFECT_NONE = 0,
    MUSYX_EFFECT_REVERB = 1,
    MUSYX_EFFECT_REVERB_HI = 2,
    MUSYX_EFFECT_CHORUS = 3,
    MUSYX_EFFECT_DELAY = 4,
};

enum MusyXOutputType
{
    MusyXOutputType_MONO = 0,
    MusyXOutputType_STEREO,
    MusyXOutputType_SURROUND
};

void PrintSoundStackInfo();
void musyXFree(void* addr);
void* musyXAlloc(size_t size);

struct AudioFileData;
class nlFile;

class SFXStartInfo
{
public:
    /* 0x00 */ unsigned long uSFXID;
    /* 0x04 */ float fVolume;
    /* 0x08 */ float fPan;
    /* 0x0C */ float fVolReverb;
    /* 0x10 */ unsigned char uSurroundPan;
    /* 0x11 */ unsigned char _pad1;
    /* 0x12 */ unsigned short uPitchBend;
    /* 0x14 */ unsigned short uModulation;
    /* 0x16 */ unsigned short uDoppler;
    /* 0x18 */ unsigned char bActivateFilter;
    /* 0x19 */ unsigned char _pad2;
    /* 0x1A */ unsigned short filterFreq;
}; // total size: 0x1C

// Forward declarations for types used in SoundStrToIDNode
class SoundPropAccessor;

struct SoundProperties
{
    /* 0x00 */ const char* typeStr;
    /* 0x04 */ const char* musyxStr;
    /* 0x08 */ float fVolume;
    /* 0x0C */ float fDelay;
    /* 0x10 */ float fVolReverb;
    /* 0x14 */ int volumeGroup;
    /* 0x18 */ int priority;
}; // total size: 0x1C

struct SFXEmitter
{
    /* 0x00 */ SND_EMITTER emitter;
    /* 0x50 */ bool bKeepTrack;
    /* 0x54 */ unsigned long soundType;
    /* 0x58 */ float fTimeStamp;
    /* 0x5C */ bool bIsStopping;
    /* 0x5D */ bool bInUse;
    /* 0x5E */ bool bIsFilterOn;
    /* 0x5F */ bool m_unk_0x5F;
    /* 0x60 */ PhysicsObject* pPhysObj;
    /* 0x64 */ void* pOwner;
    /* 0x68 */ union
    {
        const nlVector3* pvPos;
        nlVector3 vPos;
    } pos;
    /* 0x74 */ union
    {
        const nlVector3* pvDir;
        nlVector3 vDir;
    } dir;
    /* 0x80 */ PosUpdateMethod posUpdateMethod;
    /* 0x84 */ SND_PARAMETER_INFO* pMIDIControllerInfo;

    inline void Init()
    {
        soundType = (unsigned long)-1;
        fTimeStamp = -1.0f;
        bIsStopping = 0;
        bInUse = 0;
        bIsFilterOn = 0;
        m_unk_0x5F = 0;
        pPhysObj = NULL;
        pOwner = NULL;
        pos.pvPos = NULL;
        dir.pvDir = NULL;
    }

}; // total size: 0x88

struct SoundStrToIDNode
{
    /*  0x00 */ unsigned long typeID;
    /*  0x04 */ const char* typeStr;
    /*  0x08 */ const char* musyxStr;
    /*  0x0C */ unsigned long musyxID;
    /*  0x10 */ float fVolume;
    /*  0x14 */ float fDelay;
    /*  0x18 */ float fVolReverb;
    /*  0x1C */ int volGrp;
    /*  0x20 */ int sfxPriority;
    /*  0x24 */ unsigned long uHashVal;
    /*  0x28 */ SoundPropAccessor* pSoundPropAccessor;
    /*  0x2C */ unsigned char bSoundPropTableReloaded;
    /*  0x30 */ const struct SoundProperties* pSoundProp;
    /*  0x34 */ cGameSFX* pOwner;
    /*  0x38 */ unsigned long lastVoiceID;
    /*  0x3C */ SFXEmitter* pLastEmitter;
    /*  0x40 */ bool m_unk_0x40;
}; // total size: 0x44

struct EmitterStartInfo
{
    /* 0x00 */ SFXEmitter* pSFXEmitter;
    /* 0x04 */ unsigned long uSFXID;
    /* 0x08 */ unsigned long groupID;
    /* 0x0C */ nlVector3 position;
    /* 0x18 */ nlVector3 direction;
    /* 0x24 */ float maxDist;
    /* 0x28 */ float comp;
    /* 0x2C */ float minVol;
    /* 0x30 */ float maxVol;
    /* 0x34 */ float fVolReverb;
    /* 0x38 */ unsigned char bContinuous;
    /* 0x39 */ unsigned char bRestartable;
    /* 0x3A */ unsigned char bPausable;
    /* 0x3B */ unsigned char bUseDoppler;
    /* 0x3C */ unsigned char bHardStart;
    /* 0x3D */ unsigned char bActivateFilter;
    /* 0x3E */ unsigned short filterFreq;
    /* 0x40 */ unsigned short pitch;
}; // total size: 0x44

namespace PlatAudio
{

extern bool gUsingDolbyProLogic2;

// Retail mangles these as `__9PlatAudio`, which MWCC produces identically for a namespace and for
// a class, so a namespace reproduces the symbols exactly. Note that means `static` here would give
// internal linkage rather than the static-member linkage the original had - InitEmitter is called
// from audio.cpp and AudioEventHandler.cpp and must stay externally visible.
u32 GetSndIDError();
bool IsSFXPlaying(unsigned long uVoiceID);
void InitEmitter(unsigned long index);
bool RemoveEmitter(SFXEmitter* pSFXEmitter);
bool RemoveEmitter(unsigned long index);
SFXEmitter* GetSFXEmitter(unsigned long index);
SFXEmitter* GetFreeEmitter(unsigned long& index);
SND_VOICEID GetEmitterVoiceID(SFXEmitter* pSFXEmitter);
bool IsEmitterActive(SFXEmitter* pSFXEmitter);
void Update3DSFXEmitter(SFXEmitter* pSFXEmitter, const nlVector3& position, const nlVector3& direction, float maxVol);
unsigned long Add3DSFXEmitter(const EmitterStartInfo& info);
void Remove3DSFXListener(SND_LISTENER* pListener);
void Update3DSFXListener(SND_LISTENER* pListener, const nlVector3& position, const nlVector3& direction, const nlVector3& heading, const nlVector3& up, float overallEmitterVol);
void Add3DSFXListener(SND_LISTENER* pListener, const nlVector3& position, const nlVector3& direction, const nlVector3& heading, const nlVector3& up, float frontAudibleDist, float backAudibleDist, float overallEmitterVol, float volPosOffset,
    bool bUseDoppler, float fSpeedOfSound);
bool SetPitchBendOnSFX(SND_VOICEID uVoiceID, u16 pitch);
bool SetFilterFreqOnSFX(SND_VOICEID uVoiceID, u16 value);
bool SetMIDIControllerVal14Bit(SND_VOICEID uVoiceID, u8 ctrl, u16 value);
void SetVolGroupVolume(u8 volGroup, float fVol, u16 fadeTime);
bool SetSFXVolumeGroup(u32 uSFXID, u8 volGroup);
bool SetSFXReverbVol(unsigned long uVoiceID, float fVol);
void SetSFXVolume(unsigned long uVoiceID, float fVolume);
bool StopSFX(unsigned long uVoiceID);
unsigned long PlaySFX(const SFXStartInfo& info);
bool UnloadAllSoundGroupsOnStack(AudioFileData& fileData, unsigned long stackEnum);
bool UnloadAllSoundGroups(AudioFileData& fileData);
bool UnloadSoundGroup(AudioFileData& fileData, unsigned long groupEnum);
bool LoadSoundGroup(AudioFileData& fileData, unsigned long groupEnum, unsigned long stackEnum, bool bUseARAMStreamCallback);
void SetupSoundBuffers(AudioFileData& fileData, bool bStream);
void StopAllSound();
void Shutdown();
bool Initialize(bool bUseDPL2);
void PurgeSampleFileBuffer();
bool IsEntireSampleFileInMem();
unsigned char ReadEntireSampleFileIntoMemSync(const char* sampleFile);
unsigned char ReadEntireSampleFileIntoMem(const char* sampleFile);
bool UpdateAuxEffectA(MusyXEffectType type, void* auxEffectSettings);
bool AddAuxEffectA(MusyXEffectType type, void* auxEffectSettings, unsigned char studio);
bool ShutdownAuxEffectA();
bool DeactivateDPL2();
bool ActivateDPL2();
void SetOutputMode(MusyXOutputType output);
} // namespace PlatAudio

class ARAMTransferHelperLoadEntireFile
{
public:
    static void LoadEntireFileCallback(nlFile* pFile, void* buffer, unsigned int size, uintptr_t uParam);
    static void* sndPushGroupCallback(u32 uOffset, u32 uSize);

    unsigned char* m_pARAMXferBlockBaseAddress; // offset 0x0

    static u32 m_uFileSize;
    static nlFile* s_pFile;
    static ARAMTransferHelperLoadEntireFile* m_pARAMHelper;
    static const char* m_szFileName;
};

class ARAMTransferHelper
{
public:
    static void* sndPushGroupCallback(u32 uOffset, u32 uSize);

    unsigned char* m_pARAMXferBlockBaseAddress; // offset 0x0
    unsigned long m_uCachedDataOffset;          // offset 0x4
    unsigned char* m_pDiskCacheBaseAddress;     // offset 0x8
    unsigned long m_uFileSize;                  // offset 0xC

    static ARAMTransferHelper* m_pARAMHelper;
    static unsigned char m_bFileOpened;
    static nlFile* m_pFile;
    static const char* m_szFileName;
};

#endif // _PLATAUDIO_H_
