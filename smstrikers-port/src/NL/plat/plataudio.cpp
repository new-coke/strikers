#include "NL/plat/plataudio.h"
#include "NL/nlMemory.h"
#include "NL/nlFile.h"
#include "NL/nlString.h"
#include "NL/nlTicker.h"
#include "NL/nlFileGC.h"
#include "NL/gl/glMemory.h"
#include "Game/Sys/debug.h"
#include "Game/Audio/AudioLoader.h"
#include "types.h"
#include <dolphin/ar.h>

extern "C" u32 sndStackGetAvailableSampleMemory(unsigned long id);
extern "C" u32 sndStackSetCurrent(u32 id);
extern "C" u32 sndStackGetSize(void);
extern "C" unsigned long sndStackAdd(void* buffer, u32 aramAddr, u32 size);
extern "C" u32 sndStackGetARAMAddressRange(u32 id, u32* start, u32* end);

/* PORT: src/platform/musyx_data.cpp. */
extern "C" void PortMusyxSwapProj(void* buf, unsigned long size);
extern "C" void PortMusyxSwapPool(void* buf, unsigned long size);
extern "C" void* PortMusyxConvertSDir(const void* buf, unsigned long size, unsigned long* outSize);

namespace PlatAudio
{
static u32 gPrimaryStackSize = 0x44C000U - (u32)ARGetBaseAddress();
bool gUsingDolbyProLogic2 = false;
} // namespace PlatAudio

static u32 aramMemArray[2];
// PORT: these four hold a pointer.
static uintptr_t gAuxAEffectSettings;
static uintptr_t gAuxBEffectSettings;
static uintptr_t gDPL2AuxAEffectSettings;
static uintptr_t gDPL2AuxBEffectSettings;
static s32 gAuxAEffect;
static s32 gAuxBEffect;
static s32 gDPL2AuxAEffect;
static s32 gDPL2AuxBEffect;
static bool gAreSoundBuffersSetup;
static void* gpEntireSampleFileBufferFirstHalf;
static void* gpEntireSampleFileBufferSecondHalf;
static unsigned long gEntireSampleFileFirstHalfAllocSize;
static unsigned long gEntireSampleFileSecondHalfAllocSize;
static void* gpEntireSampleFileMRAMXferBuffer;
static u64 gEntireSampleMarker;
static u8 gAllowSyncReadsPastLoadedData;

#include <dolphin/ai.h>
#include <dolphin/arq.h>

SFXEmitter gEmitters[64];

struct _struct_stack_list_0x10
{
    /* 0x00 */ u32* workMem;         /* inferred */
    /* 0x04 */ unsigned long id;     /* stack id for sndStack functions */
    /* 0x08 */ s32 stackSize;        /* inferred */
    /* 0x0C */ u32 numGroupsOnStack; /* inferred */
};

static struct _struct_stack_list_0x10 stack_list[2] = {
    { NULL, 0xFFFFFFFEU, (s32)PlatAudio::gPrimaryStackSize, 0U },
    { NULL, 0xFFFFFFFFU, 0x2B4000, 0U },
};

static const SND_HOOKS sndHooks = { musyXAlloc, musyXFree };

// Field names and layout from the retail DWARF. Only referenced by the MIDI song helpers, which the
// linker dead-strips; kept at global scope so those helpers mangle as retail does (R12MidiFileData).
struct MidiFileData
{
    /* 0x00 */ char* szMidiFile;
    /* 0x04 */ unsigned short songGroup;
    /* 0x08 */ unsigned char* song_buffer;
    /* 0x0C */ unsigned long songID;
    /* 0x10 */ unsigned long seqID;
    /* 0x14 */ float seqVolume;
    /* 0x18 */ float seqTempo;
    /* 0x1C */ unsigned char bPaused;
}; // total size: 0x20

struct EffectSettings
{
};

namespace PlatAudio
{

/**
 * Offset/Address/Size: 0x0 | 0x801C47FC | size: 0x8
 */
u32 GetSndIDError()
{
    return -1;
}

/**
 * Offset/Address/Size: 0x8 | 0x801C4804 | size: 0x30
 */
bool IsSFXPlaying(unsigned long uVoiceID)
{
    SND_VOICEID result = sndFXCheck(uVoiceID);
    return result != -1;
}

/**
 * Offset/Address/Size: 0x38 | 0x801C4834 | size: 0xC0
 */
void PlatAudio::InitEmitter(unsigned long index)
{
    gEmitters[index].bKeepTrack = true;
    gEmitters[index].soundType = (unsigned long)-1;
    gEmitters[index].fTimeStamp = -1.0f;
    gEmitters[index].bIsStopping = false;
    gEmitters[index].bInUse = false;
    gEmitters[index].bIsFilterOn = false;
    gEmitters[index].m_unk_0x5F = false;
    gEmitters[index].pPhysObj = NULL;
    gEmitters[index].pOwner = NULL;
    gEmitters[index].pos.pvPos = NULL;
    gEmitters[index].dir.pvDir = NULL;
    gEmitters[index].pos.vPos.x = 0.0f;
    gEmitters[index].pos.vPos.y = 0.0f;
    gEmitters[index].pos.vPos.z = 0.0f;
    gEmitters[index].dir.vDir.x = 0.0f;
    gEmitters[index].dir.vDir.y = 0.0f;
    gEmitters[index].dir.vDir.z = 0.0f;
    gEmitters[index].posUpdateMethod = NONE;
    if (gEmitters[index].pMIDIControllerInfo != NULL)
    {
        if (gEmitters[index].pMIDIControllerInfo->paraArray != NULL)
            delete[] (char*)gEmitters[index].pMIDIControllerInfo->paraArray;
        delete gEmitters[index].pMIDIControllerInfo;
    }
    gEmitters[index].pMIDIControllerInfo = NULL;
}

/**
 * Offset/Address/Size: 0xF8 | 0x801C48F4 | size: 0x20
 */
bool RemoveEmitter(SFXEmitter* pSFXEmitter)
{
    return sndRemoveEmitter((SND_EMITTER*)pSFXEmitter);
}

/**
 * Offset/Address/Size: 0x118 | 0x801C4914 | size: 0x30
 */
bool RemoveEmitter(unsigned long index)
{
    return sndRemoveEmitter((SND_EMITTER*)&gEmitters[index]);
}

/**
 * Offset/Address/Size: 0x148 | 0x801C4944 | size: 0x14
 */
SFXEmitter* GetSFXEmitter(unsigned long index)
{
    return &gEmitters[index];
}

/**
 * Offset/Address/Size: 0x15C | 0x801C4958 | size: 0x35C
 */
SFXEmitter* GetFreeEmitter(unsigned long& index)
{
    int i;

    index = 0;
    for (i = 0; i < 64; i++)
    {
        if (!sndCheckEmitter((SND_EMITTER*)&gEmitters[i]) && sndFXCheck(sndEmitterVoiceID((SND_EMITTER*)&gEmitters[i])) == -1
            && !gEmitters[i].bIsStopping && !gEmitters[i].bInUse)
        {
            int emitterOffset = i * sizeof(SFXEmitter);
            sndRemoveEmitter((SND_EMITTER*)((u8*)gEmitters + emitterOffset));
            gEmitters[i].bInUse = true;
            index = i;
            InitEmitter(i);
            break;
        }
    }

    if (i == 64)
    {
        int minIndex;
        i = 1;
        minIndex = 0;
        float min = gEmitters[0].fTimeStamp;
        while (i < 64)
        {
            if (gEmitters[i].fTimeStamp < min)
            {
                min = gEmitters[i].fTimeStamp;
                minIndex = i;
            }
            i++;
        }

        int emitterOffset = minIndex * sizeof(SFXEmitter);
        sndRemoveEmitter((SND_EMITTER*)((u8*)gEmitters + emitterOffset));
        gEmitters[minIndex].bInUse = true;
        index = minIndex;
        InitEmitter(minIndex);
        tDebugPrintManager::Print(DC_SOUND, "Audio::GetFreeEmitter(): Ran out of free emitters, killing oldest one...\n");
    }

    return &gEmitters[index];
}

/**
 * Offset/Address/Size: 0x4B8 | 0x801C4CB4 | size: 0x20
 */
SND_VOICEID GetEmitterVoiceID(SFXEmitter* pSFXEmitter)
{
    return sndEmitterVoiceID((SND_EMITTER*)pSFXEmitter);
}

/**
 * Offset/Address/Size: 0x4D8 | 0x801C4CD4 | size: 0x20
 */
bool IsEmitterActive(SFXEmitter* pSFXEmitter)
{
    return sndCheckEmitter((SND_EMITTER*)pSFXEmitter);
}

/**
 * Offset/Address/Size: 0x4F8 | 0x801C4CF4 | size: 0x90
 */
void Update3DSFXEmitter(SFXEmitter* pSFXEmitter, const nlVector3& position, const nlVector3& direction, float maxVol)
{
    float adj;
    float rounded = 127.0f * maxVol;

    SND_FVECTOR svPos;
    svPos.x = position.x;
    svPos.y = position.y;
    svPos.z = position.z;

    SND_FVECTOR svDir;
    svDir.x = direction.x;
    svDir.y = direction.y;
    svDir.z = direction.z;

    if (rounded < 0.0f)
    {
        adj = -0.5f;
    }
    else
    {
        adj = 0.5f;
    }
    rounded += adj;
    sndUpdateEmitter(&pSFXEmitter->emitter, &svPos, &svDir, (u8)(s32)rounded, NULL);
}

/**
 * Offset/Address/Size: 0x588 | 0x801C4D84 | size: 0x2E4
 */
unsigned long Add3DSFXEmitter(const EmitterStartInfo& info)
{
    SND_FVECTOR svPos;
    SND_FVECTOR svDir;
    SFXEmitter* pSFXEmitter;
    unsigned long flags;
    unsigned long numPara;
    int currParaIndex;
    SND_PARAMETER_INFO* pParaInfo;
    SND_PARAMETER* pParaArray;
    SND_PARAMETER_INFO tempParaInfo;
    SND_PARAMETER tempParaArray[4];

    svPos.x = info.position.x;
    svPos.y = info.position.y;
    svPos.z = info.position.z;

    svDir.x = info.direction.x;
    svDir.y = info.direction.y;
    svDir.z = info.direction.z;

    pSFXEmitter = info.pSFXEmitter;
    flags = 0;

    if (pSFXEmitter == NULL)
    {
        flags = 0;
    }
    else
    {
        if (info.bContinuous)
        {
            flags |= SND_EMITTER_CONTINUOUS;
        }

        if (info.bRestartable)
        {
            flags |= SND_EMITTER_RESTARTABLE;
        }

        if (info.bPausable)
        {
            flags |= SND_EMITTER_PAUSABLE;
        }

        if (info.bUseDoppler)
        {
            flags |= SND_EMITTER_DOPPLERFX;
        }

        if (info.bHardStart)
        {
            flags |= SND_EMITTER_HARDSTART;
        }
    }

    numPara = 0;
    currParaIndex = 0;
    pParaInfo = NULL;
    pParaArray = NULL;

    if (info.fVolReverb != 100.0f)
    {
        numPara = 1;
    }

    if (info.pitch != 0x2000)
    {
        numPara += 1;
    }

    if (info.bActivateFilter)
    {
        numPara += 2;
    }

    if (numPara != 0)
    {
        if (pSFXEmitter == NULL)
        {
            pParaInfo = &tempParaInfo;
            pParaArray = tempParaArray;
        }
        else
        {
            void* alloc = nlMalloc(8, 8, false);
            pParaInfo = (SND_PARAMETER_INFO*)alloc;
            pParaArray = (SND_PARAMETER*)nlMalloc(numPara * sizeof(SND_PARAMETER), 8, false);
        }

        pParaInfo->numPara = numPara;
        pParaInfo->paraArray = pParaArray;
    }

    if (info.fVolReverb != 100.0f)
    {
        float reverbVol = info.fVolReverb;
        float volScale = 127.0f;
        float rounded = 0.0f;
        pParaArray[0].ctrl = 0x5B;
        reverbVol = volScale * reverbVol;
        if (reverbVol < rounded)
        {
            rounded = -0.5f;
        }
        else
        {
            rounded = 0.5f;
        }
        rounded = reverbVol + rounded;

        currParaIndex = 1;
        pParaArray[0].paraData.value7 = (u8)(s32)rounded;
    }

    if (info.pitch != 0x2000)
    {
        if (info.pitch != 0x2000)
        {
            tDebugPrintManager::Print(DC_SOUND, "emitter started with pitch %d\n", info.pitch);
        }

        pParaArray[currParaIndex].ctrl = 0x80;
        pParaArray[currParaIndex].paraData.value14 = info.pitch;
        currParaIndex += 1;
    }

    if (info.bActivateFilter)
    {
        unsigned long paraIndex = currParaIndex;
        unsigned long freq = info.filterFreq;

        pParaArray[paraIndex].ctrl = 0x4F;
        pParaArray[paraIndex].paraData.value14 = 0x2000;

        if (freq > 0x3FFF)
        {
            freq = 0x3FFF;
        }

        paraIndex = currParaIndex + 1;
        pParaArray[paraIndex].ctrl = 1;
        pParaArray[paraIndex].paraData.value14 = (u16)freq;

        if (pSFXEmitter != NULL)
        {
            pSFXEmitter->bIsFilterOn = true;
        }
    }

    if (pSFXEmitter != NULL)
    {
        pSFXEmitter->pMIDIControllerInfo = pParaInfo;
    }

    {
        float adj;
        float rounded = 127.0f * info.minVol;
        if (rounded < 0.0f)
        {
            adj = -0.5f;
        }
        else
        {
            adj = 0.5f;
        }
        rounded += adj;

        u8 minVol = (u8)(s32)rounded;

        rounded = 127.0f * info.maxVol;
        if (rounded < 0.0f)
        {
            adj = -0.5f;
        }
        else
        {
            adj = 0.5f;
        }
        rounded += adj;

        return sndAddEmitter2StudioPara((SND_EMITTER*)pSFXEmitter, &svPos, &svDir, info.maxDist, info.comp, flags, (u16)info.uSFXID, (u8)(s32)rounded, minVol, 0, pParaInfo);
    }
}

/**
 * Offset/Address/Size: 0x86C | 0x801C5068 | size: 0x20
 */
void Remove3DSFXListener(SND_LISTENER* pListener)
{
    sndRemoveListener(pListener);
}

/**
 * Offset/Address/Size: 0x88C | 0x801C5088 | size: 0xC8
 */
void Update3DSFXListener(SND_LISTENER* pListener, const nlVector3& position, const nlVector3& direction, const nlVector3& heading, const nlVector3& up, float overallEmitterVol)
{
    SND_FVECTOR svPos;
    svPos.x = position.x;
    svPos.y = position.y;
    svPos.z = position.z;

    SND_FVECTOR svDir;
    svDir.x = direction.x;
    svDir.y = direction.y;
    svDir.z = direction.z;

    SND_FVECTOR svHeading;
    svHeading.x = heading.x;
    svHeading.y = heading.y;
    svHeading.z = heading.z;

    SND_FVECTOR svUp;
    svUp.x = up.x;
    svUp.y = up.y;
    svUp.z = up.z;

    f32 rounded = 127.0f * overallEmitterVol;
    f32 adj;
    if (rounded < 0.0f)
    {
        adj = -0.5f;
    }
    else
    {
        adj = 0.5f;
    }
    rounded += adj;

    sndUpdateListener(pListener, &svPos, &svDir, &svHeading, &svUp, (u8)(s32)rounded, NULL);
}

/**
 * Offset/Address/Size: 0x954 | 0x801C5150 | size: 0x10C
 */
void Add3DSFXListener(SND_LISTENER* pListener, const nlVector3& position, const nlVector3& direction, const nlVector3& heading, const nlVector3& up, float frontAudibleDist, float backAudibleDist, float overallEmitterVol, float volPosOffset, bool bUseDoppler, float fSpeedOfSound)
{
    SND_FVECTOR svPos;
    svPos.x = position.x;
    svPos.y = position.y;
    svPos.z = position.z;

    SND_FVECTOR svDir;
    svDir.x = direction.x;
    svDir.y = direction.y;
    svDir.z = direction.z;

    SND_FVECTOR svHeading;
    svHeading.x = heading.x;
    svHeading.y = heading.y;
    svHeading.z = heading.z;

    SND_FVECTOR svUp;
    svUp.x = up.x;
    svUp.y = up.y;
    svUp.z = up.z;

    f32 rounded = 127.0f * overallEmitterVol;
    f32 adj;
    if (rounded < 0.0f)
    {
        adj = -0.5f;
    }
    else
    {
        adj = 0.5f;
    }
    rounded += adj;

    u32 flags = bUseDoppler ? 1 : 0;

    sndAddListenerEx(pListener, &svPos, &svDir, &svHeading, &svUp, frontAudibleDist, backAudibleDist, fSpeedOfSound, volPosOffset, flags, (u8)(s32)rounded, NULL);
}

/**
 * Offset/Address/Size: 0xA60 | 0x801C525C | size: 0x68
 */
bool SetPitchBendOnSFX(SND_VOICEID uVoiceID, u16 pitch)
{
    if (uVoiceID == -1 || sndFXCheck(uVoiceID) == -1)
    {
        return true;
    }
    return sndFXCtrl14(uVoiceID, 0x80, pitch);
}

/**
 * Offset/Address/Size: 0xAC8 | 0x801C52C4 | size: 0x7C
 */
bool SetFilterFreqOnSFX(SND_VOICEID uVoiceID, u16 value)
{
    u16 freq = value;
    if (value > 0x3FFFU)
    {
        freq = 0x3FFF;
    }

    if (uVoiceID == -1 || sndFXCheck(uVoiceID) == -1)
    {
        return true;
    }
    return sndFXCtrl14(uVoiceID, 0x1, freq);
}

/**
 * Offset/Address/Size: 0xB44 | 0x801C5340 | size: 0x20
 */
bool SetMIDIControllerVal14Bit(SND_VOICEID uVoiceID, u8 ctrl, u16 value)
{
    return sndFXCtrl14(uVoiceID, ctrl, value);
}

/**
 * Offset/Address/Size: 0xB64 | 0x801C5360 | size: 0x58
 */
void SetVolGroupVolume(u8 volGroup, float fVol, u16 fadeTime)
{
    f32 scaledVol;
    f32 adj;

    scaledVol = 127.0f * fVol;
    if (scaledVol < 0.0f)
    {
        adj = -0.5f;
    }
    else
    {
        adj = 0.5f;
    }
    sndVolume((s8)(scaledVol + adj), fadeTime, volGroup);
}

/**
 * Offset/Address/Size: 0xBBC | 0x801C53B8 | size: 0x24
 */
bool SetSFXVolumeGroup(u32 uSFXID, u8 volGroup)
{
    return sndFXAssignVolGroup2FXId((SND_FXID)uSFXID, volGroup);
}

/**
 * Offset/Address/Size: 0xBE0 | 0x801C53DC | size: 0x68
 */
bool SetSFXReverbVol(SND_VOICEID uVoiceID, float fVol)
{
    int roundedVol;

    if (fVol == 100.0f)
    {
        fVol = 0.0f; // Fallback
    }

    float vol = 127.0f * fVol;
    vol += (vol < 0.0f ? -0.5f : 0.5f);
    roundedVol = vol;
    return sndFXCtrl(uVoiceID, 0x5B, roundedVol);
}

/**
 * Offset/Address/Size: 0xC48 | 0x801C5444 | size: 0x88
 */
void SetSFXVolume(unsigned long uVoiceID, float fVolume)
{
    fVolume = (fVolume >= 0.0f) ? fVolume : 0.0f;
    fVolume = (fVolume <= 1.0f) ? fVolume : 1.0f;

    float rounded = 127.0f * fVolume;
    float adj = (rounded < 0.0f) ? -0.5f : 0.5f;
    rounded = rounded + adj;
    sndFXCtrl(uVoiceID, 7, (u8)(s32)rounded);
}

/**
 * Offset/Address/Size: 0xCD0 | 0x801C54CC | size: 0x48
 */
bool StopSFX(unsigned long uVoiceID)
{
    if (uVoiceID == 0xFFFFFFFF)
    {
        return false;
    }
    bool bKeyOffSet = sndFXKeyOff(uVoiceID);
    if (bKeyOffSet)
    {
        return true;
    }
    return bKeyOffSet;
}

/**
 * Offset/Address/Size: 0xD18 | 0x801C5514 | size: 0x244
 */
unsigned long PlaySFX(const SFXStartInfo& info)
{
    const SFXStartInfo* pInfo = &info;
    u8 uVolume;
    u8 uPan;
    unsigned long numPara;
    int currParaIndex;
    SND_PARAMETER* pParaArray;
    SND_PARAMETER_INFO tempParaInfo;
    SND_PARAMETER tempParaArray[4];

    float vol = pInfo->fVolume;
    if (100.0f == vol)
    {
        uVolume = 0xFF;
    }
    else if (vol > 1.0f)
    {
        uVolume = 0x7F;
    }
    else if (vol < 0.0f)
    {
        uVolume = 0;
    }
    else
    {
        float rounded;
        vol = 127.0f * vol;
        if (vol < 0.0f)
        {
            rounded = -0.5f;
        }
        else
        {
            rounded = 0.5f;
        }
        rounded = vol + rounded;
        uVolume = (u8)(s32)rounded;
    }

    if (100.0f == pInfo->fPan)
    {
        uPan = 0xFF;
    }
    else
    {
        float f3 = 0.5f;
        float scaledPan = 127.0f * (f3 * (1.0f + pInfo->fPan));
        if (scaledPan < 0.0f)
        {
            f3 = -0.5f;
        }
        float rounded = scaledPan + f3;
        uPan = (u8)(s32)rounded;
    }

    numPara = 0;
    currParaIndex = 0;
    pParaArray = NULL;
    tempParaInfo.numPara = 0;
    tempParaInfo.paraArray = NULL;

    if (100.0f != pInfo->fVolReverb)
    {
        numPara = 1;
    }

    if (pInfo->uPitchBend != 0x2000)
    {
        numPara += 1;
    }

    if (pInfo->bActivateFilter)
    {
        numPara += 2;
    }

    if (numPara != 0)
    {
        pParaArray = tempParaArray;
        tempParaInfo.numPara = numPara;
        tempParaInfo.paraArray = pParaArray;
    }

    if (100.0f != pInfo->fVolReverb)
    {
        float rounded;
        float scaledReverbVol = 127.0f * pInfo->fVolReverb;
        pParaArray[0].ctrl = 0x5B;
        if (scaledReverbVol < 0.0f)
        {
            rounded = -0.5f;
        }
        else
        {
            rounded = 0.5f;
        }
        rounded = scaledReverbVol + rounded;
        currParaIndex = 1;
        pParaArray[0].paraData.value7 = (u8)(s32)rounded;
    }

    if (pInfo->uPitchBend != 0x2000)
    {
        if (pInfo->uPitchBend != 0x2000)
        {
            tDebugPrintManager::Print(DC_SOUND, "emitter started with pitch %d\n", pInfo->uPitchBend);
        }

        {
            unsigned long paraIndex = currParaIndex;
            pParaArray[paraIndex].ctrl = 0x80;
            pParaArray[paraIndex].paraData.value14 = pInfo->uPitchBend;
            currParaIndex = paraIndex + 1;
        }
    }

    if (pInfo->bActivateFilter)
    {
        int paraIndex = currParaIndex;
        unsigned long freq = pInfo->filterFreq;

        pParaArray[paraIndex].ctrl = 0x4F;
        pParaArray[paraIndex].paraData.value14 = 0x2000;

        if (freq > 0x3FFF)
        {
            freq = 0x3FFF;
        }

        paraIndex = currParaIndex + 1;
        pParaArray[paraIndex].ctrl = 1;
        pParaArray[paraIndex].paraData.value14 = (u16)freq;
    }

    return sndFXStartParaInfo((u16)pInfo->uSFXID, uVolume, uPan, 0, &tempParaInfo);
}

/**
 * Dead-stripped by the linker in retail. The MAP lists a family of MIDI song helpers as UNUSED in
 * plataudio.o at this point in the file - SetMidiTempo, SetMidiVolume, StopMidiSong, ResumeMidiSong,
 * PauseMidiSong, XFadeMidiSong, PlayMidiSong, UnloadMidiSong and LoadMidiSong (0x80 bytes) - none of
 * which anything calls. They are compiled into the object and then discarded at link time, so they
 * contribute nothing to the DOL.
 *
 * They still matter for the link: string literals are interned at parse time, so "Failed to open
 * file %s\n" claims its .data slot here (@1555 in the retail object) - ahead of every literal used
 * by UnloadAllSoundGroupsOnStack and everything after it. Without this, the literal is first seen in
 * SetupSoundBuffers, lands at the end of .data instead of at 0x8C, and shifts eight strings plus the
 * @l halves of every instruction that references them.
 *
 * Only the failure path of LoadMidiSong is reconstructed - enough to anchor the literal. The bodies
 * are unrecoverable: dead-stripped code leaves no disassembly to match against.
 */
static bool LoadMidiSong(MidiFileData& fileData)
{
    tDebugPrintManager::Print(DC_SOUND, "Failed to open file %s\n", fileData.szMidiFile);
    return false;
}

static inline void ResetSoundGroup(SndGroupData& group)
{
    group.stackEnum = -1;
    group.uLoadOrder = -1;
    group.loadType = SND_GROUP_LOAD_NOT_LOADED;
}

/**
 * Offset/Address/Size: 0xF5C | 0x801C5758 | size: 0x120
 */
bool UnloadAllSoundGroupsOnStack(AudioFileData& fileData, unsigned long stackEnum)
{
    int i;

    if (!(unsigned char)sndStackSetCurrent(stack_list[stackEnum].id))
    {
        tDebugPrintManager::Print(DC_SOUND, "sndStackSetCurrent() failed on sound stack ID %d\n", stack_list[stackEnum].id);
        return 0;
    }

    for (i = 0; (unsigned long)i < stack_list[stackEnum].numGroupsOnStack; i++)
    {
        if (!sndPopGroup())
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not unload sound group %d because numGroupsOnStack count is incorrect.\n", stackEnum);
            return 0;
        }
        PrintSoundStackInfo();
    }

    stack_list[stackEnum].numGroupsOnStack = 0;

    i = 0;
    while (i < fileData.numSoundGroups)
    {
        if (stackEnum == (unsigned long)fileData.soundGroups[i].stackEnum)
        {
            ResetSoundGroup(fileData.soundGroups[i]);
        }
        i++;
    }

    return true;
}

static inline bool UnloadTopSoundGroupOnStack(AudioFileData& fileData, unsigned long stackEnum)
{
    int i;

    if (!(unsigned char)sndStackSetCurrent(stack_list[stackEnum].id))
    {
        tDebugPrintManager::Print(DC_SOUND, "sndStackSetCurrent() failed on sound stack ID %d\n", stack_list[stackEnum].id);
        return false;
    }

    for (i = 0; (unsigned long)i < stack_list[stackEnum].numGroupsOnStack; i++)
    {
        if (!sndPopGroup())
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not unload sound group %d because numGroupsOnStack count is incorrect.\n", stackEnum);
            return false;
        }
        PrintSoundStackInfo();
    }

    stack_list[stackEnum].numGroupsOnStack = 0;

    i = 0;
    while (i < fileData.numSoundGroups)
    {
        if (stackEnum == (unsigned long)fileData.soundGroups[i].stackEnum)
        {
            ResetSoundGroup(fileData.soundGroups[i]);
        }
        i++;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x107C | 0x801C5878 | size: 0x150
 */
bool UnloadAllSoundGroups(AudioFileData& fileData)
{
    int i;

    sndSilence();

    for (i = 1; i >= 0; i--)
    {
        UnloadTopSoundGroupOnStack(fileData, i);
    }

    i = 0;
    while (i < fileData.numSoundGroups)
    {
        ResetSoundGroup(fileData.soundGroups[i]);
        i++;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x11CC | 0x801C59C8 | size: 0x184
 */
bool UnloadSoundGroup(AudioFileData& fileData, unsigned long groupEnum)
{
    u32 uTickStart = nlGetTicker();

    if ((unsigned long)fileData.soundGroups[groupEnum].uLoadOrder == stack_list[fileData.soundGroups[groupEnum].stackEnum].numGroupsOnStack - 1)
    {
        if (!(unsigned char)sndStackSetCurrent(stack_list[fileData.soundGroups[groupEnum].stackEnum].id))
        {
            tDebugPrintManager::Print(DC_SOUND, "sndStackSetCurrent() failed on sound stack ID %d\n", stack_list[fileData.soundGroups[groupEnum].stackEnum].id);
            return false;
        }

        if (!(unsigned char)sndPopGroup())
        {
            tDebugPrintManager::Print(DC_SOUND, "Could not unload sound group %s for some unknown reason.\n", fileData.soundGroups[groupEnum].szGroupName);
            return false;
        }

        stack_list[fileData.soundGroups[groupEnum].stackEnum].numGroupsOnStack--;

        u32 uTickEnd = nlGetTicker();
        f32 fTime = nlGetTickerDifference(uTickStart, uTickEnd) / 1000.0f;

        tDebugPrintManager::Print(DC_SOUND, "Popping sound group %s from sound stack ID %d: %0.3f seconds\n", fileData.soundGroups[groupEnum].szGroupName, stack_list[fileData.soundGroups[groupEnum].stackEnum].id, fTime);
        PrintSoundStackInfo();

        SndGroupData* grp = &fileData.soundGroups[groupEnum];
        grp->stackEnum = -1;
        grp->uLoadOrder = -1;
        grp->loadType = (LoadType)0;

        return true;
    }

    tDebugPrintManager::Print(DC_SOUND, "Could not unload sound group %s because it isn't on top.\n", fileData.soundGroups[groupEnum].szGroupName);
    return false;
}

/**
 * Offset/Address/Size: 0x1350 | 0x801C5B4C | size: 0x328
 */
bool LoadSoundGroup(AudioFileData& fileData, unsigned long groupEnum, unsigned long stackEnum, bool bUseARAMStreamCallback)
{
    unsigned long uTickStart;
    unsigned short groupID = fileData.soundGroups[groupEnum].groupID;
    uTickStart = nlGetTicker();

    if (!(unsigned char)sndStackSetCurrent(stack_list[stackEnum].id))
    {
        tDebugPrintManager::Print(DC_SOUND, "sndStackSetCurrent() failed on sound stack ID %d\n", stack_list[stackEnum].id);
        return false;
    }

    ARAMTransferHelper* pTransferHelperLoadFromDisc = NULL;
    ARAMTransferHelperLoadEntireFile* pTransferHelperLoadEntireFile = NULL;

    if (bUseARAMStreamCallback)
    {
        pTransferHelperLoadFromDisc = (ARAMTransferHelper*)nlMalloc(sizeof(ARAMTransferHelper), 0x20, true);
        if (pTransferHelperLoadFromDisc != NULL)
        {
            const char* szFile = fileData.szSampleFile;
            pTransferHelperLoadFromDisc->m_pARAMXferBlockBaseAddress = NULL;
            pTransferHelperLoadFromDisc->m_uCachedDataOffset = -1;
            pTransferHelperLoadFromDisc->m_pDiskCacheBaseAddress = NULL;
            ARAMTransferHelper::m_szFileName = szFile;

            pTransferHelperLoadFromDisc->m_pDiskCacheBaseAddress = (unsigned char*)nlMalloc(0x20000, 0x20, true);
            pTransferHelperLoadFromDisc->m_pARAMXferBlockBaseAddress = (unsigned char*)nlMalloc(0x20000, 0x20, true);

            ARAMTransferHelper::m_pFile = nlOpen(ARAMTransferHelper::m_szFileName);
            {
                unsigned int allocSize;
                pTransferHelperLoadFromDisc->m_uFileSize = nlFileSize(ARAMTransferHelper::m_pFile, &allocSize);
            }
            nlClose(ARAMTransferHelper::m_pFile);
            ARAMTransferHelper::m_pFile = NULL;

            tDebugPrintManager::Print(DC_SOUND, "ARAMTransferHelper: Sample file size is %d\n", pTransferHelperLoadFromDisc->m_uFileSize);

            ARAMTransferHelper::m_pARAMHelper = pTransferHelperLoadFromDisc;
        }

        ARQSetChunkSize(0x20000);
        sndSetSampleDataUploadCallback(ARAMTransferHelper::sndPushGroupCallback, 0x20000);
    }
    else
    {
        pTransferHelperLoadEntireFile = (ARAMTransferHelperLoadEntireFile*)nlMalloc(sizeof(ARAMTransferHelperLoadEntireFile), 0x20, true);
        if (pTransferHelperLoadEntireFile != NULL)
        {
            const char* szFile = fileData.szSampleFile;
            pTransferHelperLoadEntireFile->m_pARAMXferBlockBaseAddress = NULL;
            ARAMTransferHelperLoadEntireFile::m_szFileName = szFile;
            pTransferHelperLoadEntireFile->m_pARAMXferBlockBaseAddress = (unsigned char*)nlMalloc(0x20000, 0x20, true);
            ARAMTransferHelperLoadEntireFile::m_pARAMHelper = pTransferHelperLoadEntireFile;
        }

        ARQSetChunkSize(0x20000);
        sndSetSampleDataUploadCallback(ARAMTransferHelperLoadEntireFile::sndPushGroupCallback, 0x20000);
    }

    if (!(unsigned char)sndPushGroup(fileData.proj_buffer, groupID, 0, fileData.sdir_buffer, fileData.pool_buffer))
    {
        tDebugPrintManager::Print(DC_SOUND, "sndPushGroup() failed on sound group %d\n", groupID);
        return false;
    }

    fileData.soundGroups[groupEnum].stackEnum = stackEnum;

    int loadType = 1;
    unsigned long uLoadOrder = stack_list[stackEnum].numGroupsOnStack;
    stack_list[stackEnum].numGroupsOnStack = uLoadOrder + 1;
    fileData.soundGroups[groupEnum].uLoadOrder = uLoadOrder;

    if (pTransferHelperLoadFromDisc != NULL)
    {
        loadType = 2;
    }
    fileData.soundGroups[groupEnum].loadType = (LoadType)loadType;

    if (pTransferHelperLoadFromDisc != NULL)
    {
        if (pTransferHelperLoadFromDisc != NULL)
        {
            if (ARAMTransferHelper::m_bFileOpened)
            {
                nlClose(ARAMTransferHelper::m_pFile);
                ARAMTransferHelper::m_pFile = NULL;
                ARAMTransferHelper::m_bFileOpened = 0;
            }

            if (pTransferHelperLoadFromDisc->m_pARAMXferBlockBaseAddress != NULL)
            {
                delete[] pTransferHelperLoadFromDisc->m_pARAMXferBlockBaseAddress;
                pTransferHelperLoadFromDisc->m_pARAMXferBlockBaseAddress = NULL;
            }

            if (pTransferHelperLoadFromDisc->m_pDiskCacheBaseAddress != NULL)
            {
                delete[] pTransferHelperLoadFromDisc->m_pDiskCacheBaseAddress;
                pTransferHelperLoadFromDisc->m_pDiskCacheBaseAddress = NULL;
            }

            ARAMTransferHelper::m_pARAMHelper = NULL;
            delete pTransferHelperLoadFromDisc;
        }
    }

    if (pTransferHelperLoadEntireFile != NULL)
    {
        if (pTransferHelperLoadEntireFile != NULL)
        {
            if (pTransferHelperLoadEntireFile->m_pARAMXferBlockBaseAddress != NULL)
            {
                delete[] pTransferHelperLoadEntireFile->m_pARAMXferBlockBaseAddress;
                pTransferHelperLoadEntireFile->m_pARAMXferBlockBaseAddress = NULL;
            }

            if (ARAMTransferHelperLoadEntireFile::s_pFile != NULL)
            {
                nlClose(ARAMTransferHelperLoadEntireFile::s_pFile);
            }

            ARAMTransferHelperLoadEntireFile::m_pARAMHelper = NULL;
            delete pTransferHelperLoadEntireFile;
        }
    }

    {
        unsigned long uTickEnd = nlGetTicker();
        float fTime = nlGetTickerDifference(uTickStart, uTickEnd) / 1000.0f;
        nlPrintf("Pushed sound group %s onto sound stack ID %d: %0.3f seconds\n", fileData.soundGroups[groupEnum].szGroupName, stack_list[stackEnum].id, fTime);
    }

    PrintSoundStackInfo();
    return true;
}

/**
 * Offset/Address/Size: 0x1678 | 0x801C5E74 | size: 0x16C
 */
void SetupSoundBuffers(AudioFileData& fileData, bool bStream)
{
    unsigned long uPoolReadLength;
    unsigned long uProjReadLength;
    unsigned long uSdirReadLength;
    char* szFileName;
    unsigned char* pBuffer;

    u32 uTickStart = nlGetTicker();

    if (!fileData.pool_buffer)
    {
        szFileName = fileData.szPoolFile;
        pBuffer = (unsigned char*)nlLoadEntireFile(szFileName, &uPoolReadLength, 0x20, AllocateStart);
        if (!pBuffer)
            tDebugPrintManager::Print(DC_SOUND, "Failed to open file %s\n", szFileName);
        /* PORT: the disc is big-endian and MusyX reads this as structs. */
        if (pBuffer)
            PortMusyxSwapPool(pBuffer, uPoolReadLength);
        fileData.pool_buffer = pBuffer;
    }

    if (!fileData.proj_buffer)
    {
        szFileName = fileData.szProjectFile;
        pBuffer = (unsigned char*)nlLoadEntireFile(szFileName, &uProjReadLength, 0x20, AllocateStart);
        if (!pBuffer)
            tDebugPrintManager::Print(DC_SOUND, "Failed to open file %s\n", szFileName);
        if (pBuffer)
            PortMusyxSwapProj(pBuffer, uProjReadLength);
        fileData.proj_buffer = pBuffer;
    }

    if (!fileData.sdir_buffer)
    {
        szFileName = fileData.szDirFile;
        pBuffer = (unsigned char*)nlLoadEntireFile(szFileName, &uSdirReadLength, 0x20, AllocateStart);
        if (!pBuffer)
            tDebugPrintManager::Print(DC_SOUND, "Failed to open file %s\n", szFileName);
        if (pBuffer)
        {
            /* PORT: rebuilt rather than swapped in place, SDIR_DATA carries a pointer and is wider here than on the disc. */
            unsigned long uConvertedLength;
            unsigned char* pConverted = (unsigned char*)PortMusyxConvertSDir(pBuffer, uSdirReadLength, &uConvertedLength);
            if (pConverted)
            {
                nlFree(pBuffer);
                pBuffer = pConverted;
            }
        }
        fileData.sdir_buffer = pBuffer;
    }

    gAreSoundBuffersSetup = 1;

    u32 uTickEnd = nlGetTicker();
    f32 fTime = nlGetTickerDifference(uTickStart, uTickEnd);

    if (!bStream)
    {
        tDebugPrintManager::Print(DC_SOUND, "Immediate Audio load: %0.3f seconds\n", fTime / 1000.0f);
    }
    else
    {
        tDebugPrintManager::Print(DC_SOUND, "Setting up MusyX buffers and stream load callback: %0.3f seconds\n", fTime / 1000.0f);
    }
}

/**
 * Offset/Address/Size: 0x17E4 | 0x801C5FE0 | size: 0x20
 */
void StopAllSound()
{
    sndSilence();
}

/**
 * Offset/Address/Size: 0x1804 | 0x801C6000 | size: 0x78
 */
void Shutdown()
{
    sndQuit();

    u32 length;
    for (int i = 0; i < 2; i++)
    {
        ARFree(&length);
        delete stack_list[i].workMem;
        stack_list[i].workMem = NULL;
    }

    AIReset();
    ARQReset();
    ARReset();
}

/**
 * Offset/Address/Size: 0x187C | 0x801C6078 | size: 0x11C
 */
bool Initialize(bool bUseDPL2)
{
    tDebugPrintManager::Print(DC_SOUND, "GameCube Platform Audio Initialized\n");

    SND_HOOKS hooks = sndHooks;

    ARInit(aramMemArray, 2);
    ARQInit();
    AIInit(NULL);

    ARAlloc(gPrimaryStackSize);

    sndSetHooks(&hooks);

    u32 flags = 0;
    flags |= 0x2;
    if (bUseDPL2)
    {
        flags |= 0x1;
    }

    if (!sndIsInstalled())
    {
        sndInit(0x40, 0, 0x40, 1, flags, gPrimaryStackSize);
    }

    _struct_stack_list_0x10* pStack = &stack_list[1];
    u32 aramBase = ARAlloc(pStack->stackSize);
    u32 workMemSize = sndStackGetSize();
    u32* workMemPtr = (u32*)nlMalloc(workMemSize, 8, false);
    pStack->id = sndStackAdd(workMemPtr, aramBase, pStack->stackSize);
    pStack->workMem = workMemPtr;

    sndVolume(0x7F, 0, 0xFF);
    sndOutputMode(SND_OUTPUTMODE_STEREO);
    PrintSoundStackInfo();

    return true;
}

/**
 * Offset/Address/Size: 0x1998 | 0x801C6194 | size: 0x74
 */
void PurgeSampleFileBuffer()
{
    if (gpEntireSampleFileBufferFirstHalf != NULL)
    {
        glResourceRelease(gEntireSampleMarker);
        gEntireSampleMarker = 0;
        gpEntireSampleFileBufferFirstHalf = NULL;
    }

    if (gpEntireSampleFileBufferSecondHalf != NULL)
    {
        nlVirtualFree(gpEntireSampleFileBufferSecondHalf);
        gpEntireSampleFileBufferSecondHalf = NULL;
        nlFree(gpEntireSampleFileMRAMXferBuffer);
        gpEntireSampleFileMRAMXferBuffer = NULL;
    }

    gAllowSyncReadsPastLoadedData = 0;
}

/**
 * Offset/Address/Size: 0x1A0C | 0x801C6208 | size: 0x14
 */
bool IsEntireSampleFileInMem()
{
    // PORT: with audio off the buffer is never filled, and callers that wait for it never stop waiting.
    if (AudioLoader::gbDisableAudio)
        return true;

    // PORT: when the file fitted in one piece there is no second buffer to become non-NULL.
    if (gEntireSampleFileSecondHalfAllocSize == 0)
        return gpEntireSampleFileBufferFirstHalf != NULL;

    return gpEntireSampleFileBufferSecondHalf != NULL;
}

/**
 * Offset/Address/Size: 0x1A20 | 0x801C621C | size: 0x124
 */
unsigned char ReadEntireSampleFileIntoMemSync(const char* sampleFile)
{
    ARAMTransferHelperLoadEntireFile::s_pFile = nlOpen(sampleFile);
    if (ARAMTransferHelperLoadEntireFile::s_pFile == NULL)
    {
        tDebugPrintManager::Print(DC_SOUND, "nlLoadEntireFileAsync() call inside ReadEntireSampleFileIntoMemSync() failed!\n");
        return 0;
    }

    gAllowSyncReadsPastLoadedData = 1;
    unsigned int allocSize;
    unsigned int fileSize = nlFileSize(ARAMTransferHelperLoadEntireFile::s_pFile, &allocSize);
    if (fileSize != 0)
    {
        gEntireSampleFileFirstHalfAllocSize = glx_GetFreeMemory() - 0x400;
        unsigned long firstHalfSize = (gEntireSampleFileFirstHalfAllocSize <= fileSize) ? gEntireSampleFileFirstHalfAllocSize : fileSize;
        gEntireSampleFileFirstHalfAllocSize = firstHalfSize;

        gEntireSampleMarker = glResourceMark();
        gpEntireSampleFileBufferFirstHalf = glResourceAlloc(gEntireSampleFileFirstHalfAllocSize, GLM_TextureData);
        *(u32*)gpEntireSampleFileBufferFirstHalf = 0;
        nlRead(ARAMTransferHelperLoadEntireFile::s_pFile, gpEntireSampleFileBufferFirstHalf, gEntireSampleFileFirstHalfAllocSize);

        fileSize -= gEntireSampleFileFirstHalfAllocSize;
        unsigned int virtualFree = nlVirtualLargestBlock() - 0x400;
        if (virtualFree <= fileSize)
        {
            fileSize = virtualFree;
        }
        gEntireSampleFileSecondHalfAllocSize = fileSize;
        if (fileSize != 0)
        {
            gpEntireSampleFileBufferSecondHalf = nlVirtualAlloc(fileSize, true);
            *(u32*)gpEntireSampleFileBufferSecondHalf = 0;
            nlReadToVirtualMemory(ARAMTransferHelperLoadEntireFile::s_pFile, gpEntireSampleFileBufferSecondHalf, gEntireSampleFileSecondHalfAllocSize, 0x80000);
        }
    }

    nlClose(ARAMTransferHelperLoadEntireFile::s_pFile);
    ARAMTransferHelperLoadEntireFile::s_pFile = NULL;
    return 1;
}

/**
 * Offset/Address/Size: 0x1B44 | 0x801C6340 | size: 0x100
 */
unsigned char ReadEntireSampleFileIntoMem(const char* sampleFile)
{
    unsigned int allocSize;

    ARAMTransferHelperLoadEntireFile::s_pFile = nlOpen(sampleFile);
    if (ARAMTransferHelperLoadEntireFile::s_pFile == NULL)
    {
        tDebugPrintManager::Print(DC_SOUND, "nlLoadEntireFileAsync() call inside LoadSoundGroup() failed!\n");
        return 0;
    }

    unsigned long SecondHalfSize = nlFileSize(ARAMTransferHelperLoadEntireFile::s_pFile, &allocSize);
    if (SecondHalfSize != 0)
    {
        // PORT: clamp to the file, as ReadEntireSampleFileIntoMemSync already does twenty lines up.
        gEntireSampleFileFirstHalfAllocSize = glx_GetFreeMemory() - 0x400;
        if (gEntireSampleFileFirstHalfAllocSize > SecondHalfSize)
            gEntireSampleFileFirstHalfAllocSize = SecondHalfSize;
        gEntireSampleMarker = glResourceMark();

        void* buffer = glResourceAlloc(gEntireSampleFileFirstHalfAllocSize, GLM_TextureData);
        *(u32*)buffer = 0;
        nlReadAsync(ARAMTransferHelperLoadEntireFile::s_pFile, buffer, gEntireSampleFileFirstHalfAllocSize, ARAMTransferHelperLoadEntireFile::LoadEntireFileCallback, 0);

        // PORT: the size first; with the file already in main memory there is no second piece to transfer.
        gEntireSampleFileSecondHalfAllocSize = SecondHalfSize - gEntireSampleFileFirstHalfAllocSize;
        if (gEntireSampleFileSecondHalfAllocSize == 0)
        {
            gpEntireSampleFileBufferSecondHalf = NULL;
            return 1;
        }
        gpEntireSampleFileMRAMXferBuffer = nlMalloc(0x80000, 0x20, true);
        void* vBuffer = nlVirtualAlloc(gEntireSampleFileSecondHalfAllocSize, true);
        *(u32*)vBuffer = 0;
        nlReadAsyncToVirtualMemory(ARAMTransferHelperLoadEntireFile::s_pFile, vBuffer, gEntireSampleFileSecondHalfAllocSize, ARAMTransferHelperLoadEntireFile::LoadEntireFileCallback, 1, 0x80000, gpEntireSampleFileMRAMXferBuffer);
    }

    return 1;
}

} // namespace PlatAudio

/**
 * Offset/Address/Size: 0x1C44 | 0x801C6440 | size: 0x54
 */
void ARAMTransferHelperLoadEntireFile::LoadEntireFileCallback(nlFile* pFile, void* buffer, unsigned int size, uintptr_t uParam)
{
    unsigned int AllocSize;
    if (uParam == 0)
    {
        gpEntireSampleFileBufferFirstHalf = (void*)((char*)buffer - size);
        // PORT: when the file fitted in one piece no second read was issued, so finish here.
        if (gEntireSampleFileSecondHalfAllocSize == 0)
        {
            ARAMTransferHelperLoadEntireFile::m_uFileSize = nlFileSize(pFile, &AllocSize);
            nlClose(ARAMTransferHelperLoadEntireFile::s_pFile);
            ARAMTransferHelperLoadEntireFile::s_pFile = NULL;
        }
    }
    else
    {
        gpEntireSampleFileBufferSecondHalf = (void*)((char*)buffer - size);
        ARAMTransferHelperLoadEntireFile::m_uFileSize = nlFileSize(pFile, &AllocSize);
        nlClose(ARAMTransferHelperLoadEntireFile::s_pFile);
        ARAMTransferHelperLoadEntireFile::s_pFile = NULL;
    }
}

/**
 * Offset/Address/Size: 0x1C98 | 0x801C6494 | size: 0x13C
 */
void* ARAMTransferHelperLoadEntireFile::sndPushGroupCallback(u32 uOffset, u32 uSize)
{
    unsigned long uRequestedSize = uSize;
    unsigned char* pARAMBlock = ARAMTransferHelperLoadEntireFile::m_pARAMHelper->m_pARAMXferBlockBaseAddress;

    while (uRequestedSize != 0)
    {
        unsigned long uCopySize = uRequestedSize < 0x20000 ? uRequestedSize : (unsigned long)0x20000;

        if (uOffset > gEntireSampleFileFirstHalfAllocSize + gEntireSampleFileSecondHalfAllocSize)
        {
            nlRead(ARAMTransferHelperLoadEntireFile::s_pFile, pARAMBlock, uCopySize);
        }
        else
        {
            unsigned long totalBufSize = gEntireSampleFileFirstHalfAllocSize + gEntireSampleFileSecondHalfAllocSize;
            if (uOffset + uCopySize > totalBufSize)
            {
                unsigned long firstCopySize = totalBufSize - uOffset;
                unsigned char* pSrc = (unsigned char*)gpEntireSampleFileBufferSecondHalf + (uOffset - gEntireSampleFileFirstHalfAllocSize);
                memcpy(pARAMBlock, pSrc, firstCopySize);
                nlRead(ARAMTransferHelperLoadEntireFile::s_pFile, pARAMBlock + firstCopySize, uCopySize - firstCopySize);
            }
        }

        {
            unsigned long firstHalfSize = gEntireSampleFileFirstHalfAllocSize;
            if (uOffset > firstHalfSize)
            {
                unsigned char* pSrc = (unsigned char*)gpEntireSampleFileBufferSecondHalf + (uOffset - firstHalfSize);
                memcpy(pARAMBlock, pSrc, uCopySize);
            }
            else if (uOffset + uCopySize > firstHalfSize)
            {
                unsigned long firstHalfCopySize = firstHalfSize - uOffset;
                memcpy(pARAMBlock, (unsigned char*)gpEntireSampleFileBufferFirstHalf + uOffset, firstHalfCopySize);
                memcpy(pARAMBlock + firstHalfCopySize, gpEntireSampleFileBufferSecondHalf, uCopySize - firstHalfCopySize);
            }
            else
            {
                memcpy(pARAMBlock, (unsigned char*)gpEntireSampleFileBufferFirstHalf + uOffset, uCopySize);
            }
        }

        uRequestedSize -= uCopySize;
        uOffset += uCopySize;
        pARAMBlock += uCopySize;
    }

    return ARAMTransferHelperLoadEntireFile::m_pARAMHelper->m_pARAMXferBlockBaseAddress;
}

/**
 * Offset/Address/Size: 0x1DD4 | 0x801C65D0 | size: 0x148
 */
void* ARAMTransferHelper::sndPushGroupCallback(u32 uOffset, u32 uSize)
{
    unsigned long uRequestedSize = uSize;
    unsigned char* pARAMBlock = ARAMTransferHelper::m_pARAMHelper->m_pARAMXferBlockBaseAddress;
    unsigned long uCurrentOffset = uOffset;

    while (uRequestedSize != 0)
    {
        if (uCurrentOffset >= ARAMTransferHelper::m_pARAMHelper->m_uCachedDataOffset && uCurrentOffset < ARAMTransferHelper::m_pARAMHelper->m_uCachedDataOffset + 0x20000)
        {
            if (!ARAMTransferHelper::m_bFileOpened)
            {
                ARAMTransferHelper::m_pFile = nlOpen(ARAMTransferHelper::m_szFileName);
                ARAMTransferHelper::m_bFileOpened = 1;
            }

            unsigned long uOffsetInBlock = uCurrentOffset - ARAMTransferHelper::m_pARAMHelper->m_uCachedDataOffset;
            unsigned long uRemainingInCache = 0x20000 - uOffsetInBlock;
            unsigned long uCopySize = uRequestedSize;
            if (uRemainingInCache <= uRequestedSize)
                uCopySize = uRemainingInCache;

            memcpy(pARAMBlock, ARAMTransferHelper::m_pARAMHelper->m_pDiskCacheBaseAddress + uOffsetInBlock, uCopySize);
            uRequestedSize -= uCopySize;
            uCurrentOffset += uCopySize;
            pARAMBlock += uCopySize;
        }
        else
        {
            if (!ARAMTransferHelper::m_bFileOpened)
            {
                ARAMTransferHelper::m_pFile = nlOpen(ARAMTransferHelper::m_szFileName);
                ARAMTransferHelper::m_bFileOpened = 1;
            }

            unsigned long uSeekPosition = uCurrentOffset & ~0x1FFFF;
            nlSeek(ARAMTransferHelper::m_pFile, uSeekPosition, 0);

            nlFile* pFile = ARAMTransferHelper::m_pFile;
            unsigned long uFileDataRemaining = ARAMTransferHelper::m_pARAMHelper->m_uFileSize - uSeekPosition;
            unsigned char* pDiskCache = ARAMTransferHelper::m_pARAMHelper->m_pDiskCacheBaseAddress;
            unsigned long uReadSize = uFileDataRemaining < 0x20000 ? uFileDataRemaining : (unsigned long)0x20000;
            nlRead(pFile, pDiskCache, uReadSize);
            ARAMTransferHelper::m_pARAMHelper->m_uCachedDataOffset = uSeekPosition;
        }
    }

    return ARAMTransferHelper::m_pARAMHelper->m_pARAMXferBlockBaseAddress;
}

namespace PlatAudio
{

/**
 * Offset/Address/Size: 0x1F1C | 0x801C6718 | size: 0x10C
 */
bool UpdateAuxEffectA(MusyXEffectType type, void* auxEffectSettings)
{
    bool result;

    switch (type)
    {
    case MUSYX_EFFECT_NONE:
        return true;
    case MUSYX_EFFECT_REVERB:
        result = sndAuxCallbackUpdateSettingsReverbSTD((SND_AUX_REVERBSTD*)auxEffectSettings);
        if (!result)
        {
            nlPrintf("UpdateAuxEffect: MUSYX_EFFECT_REVERB passed in, sndAuxCallbackUpdateSettingsReverbSTD() return is FALSE.\n");
            return false;
        }
        break;
    case MUSYX_EFFECT_REVERB_HI:
        result = sndAuxCallbackUpdateSettingsReverbHI((SND_AUX_REVERBHI*)auxEffectSettings);
        if (!result)
        {
            nlPrintf("UpdateAuxEffect: MUSYX_EFFECT_REVERB_HI passed in, sndAuxCallbackUpdateSettingsReverbHI() return is NULL.\n");
            return false;
        }
        break;
    case MUSYX_EFFECT_CHORUS:
        result = sndAuxCallbackUpdateSettingsChorus((SND_AUX_CHORUS*)auxEffectSettings);
        if (!result)
        {
            nlPrintf("UpdateAuxEffect: MUSYX_EFFECT_CHORUS passed in, sndAuxCallbackUpdateSettingsChorus() return is NULL.\n");
            return false;
        }
        break;
    case MUSYX_EFFECT_DELAY:
        result = sndAuxCallbackUpdateSettingsDelay((SND_AUX_DELAY*)auxEffectSettings);
        if (!result)
        {
            nlPrintf("UpdateAuxEffect: MUSYX_EFFECT_DELAY passed in, sndAuxCallbackUpdateSettingsDelay() return is NULL.\n");
            return false;
        }
        break;
    default:
        nlPrintf("UpdateAuxEffect: Unaccounted-for case.\n");
        return false;
    }

    return true;
}

} // namespace PlatAudio

static inline void (*InitAuxEffect(MusyXEffectType auxEffect, void* auxEffectSettings))(u8 reason, SND_AUX_INFO* info, void* user)
{
    void (*callback)(u8 reason, SND_AUX_INFO* info, void* user);

    switch (auxEffect)
    {
    case MUSYX_EFFECT_NONE:
        callback = NULL;
        nlPrintf("InitAuxEffect: MUSYX_EFFECT_NONE passed in, callback return is NULL.\n");
        break;
    case MUSYX_EFFECT_REVERB:
        callback = sndAuxCallbackReverbSTD;
        if (!sndAuxCallbackPrepareReverbSTD((SND_AUX_REVERBSTD*)auxEffectSettings))
        {
            nlPrintf("InitAuxEffect: MUSYX_EFFECT_REVERB passed in, callback return is NULL.\n");
            callback = NULL;
        }
        break;
    case MUSYX_EFFECT_REVERB_HI:
        callback = sndAuxCallbackReverbHI;
        if (!sndAuxCallbackPrepareReverbHI((SND_AUX_REVERBHI*)auxEffectSettings))
        {
            nlPrintf("InitAuxEffect: MUSYX_EFFECT_REVERB_HI passed in, callback return is NULL.\n");
            callback = NULL;
        }
        break;
    case MUSYX_EFFECT_CHORUS:
        callback = sndAuxCallbackChorus;
        if (!sndAuxCallbackPrepareChorus((SND_AUX_CHORUS*)auxEffectSettings))
        {
            nlPrintf("InitAuxEffect: MUSYX_EFFECT_CHORUS passed in, callback return is NULL.\n");
            callback = NULL;
        }
        break;
    case MUSYX_EFFECT_DELAY:
        callback = sndAuxCallbackDelay;
        if (!sndAuxCallbackPrepareDelay((SND_AUX_DELAY*)auxEffectSettings))
        {
            nlPrintf("InitAuxEffect: MUSYX_EFFECT_DELAY passed in, callback return is NULL.\n");
            callback = NULL;
        }
        break;
    default:
        nlPrintf("InitAuxEffect: Unaccounted-for case.\n");
        {
            void (*defaultCallback)(u8 reason, SND_AUX_INFO* info, void* user);
            callback = defaultCallback;
        }
        break;
    }

    return callback;
}

/**
 * Offset/Address/Size: 0x2028 | 0x801C6824 | size: 0x244
 */
static bool AddAuxEffect(MusyXEffectType type, void* auxEffectSettings, bool bA, unsigned char studio)
{
    FORCE_DONT_INLINE;
    if ((bA == 0) && (PlatAudio::gUsingDolbyProLogic2) && (type != 0))
    {
        return false;
    }

    void* pAuxEffectSettings;
    MusyXEffectType* pAuxEffect;
    void (*callback)(u8 reason, SND_AUX_INFO* info, void* user);

    if (PlatAudio::gUsingDolbyProLogic2)
    {
        if (bA)
        {
            pAuxEffectSettings = &gDPL2AuxAEffectSettings;
            pAuxEffect = (MusyXEffectType*)&gDPL2AuxAEffect;
        }
        else
        {
            pAuxEffectSettings = &gDPL2AuxBEffectSettings;
            pAuxEffect = (MusyXEffectType*)&gDPL2AuxBEffect;
        }
    }
    else if (bA)
    {
        pAuxEffectSettings = &gAuxAEffectSettings;
        pAuxEffect = (MusyXEffectType*)&gAuxAEffect;
    }
    else
    {
        pAuxEffectSettings = &gAuxBEffectSettings;
        pAuxEffect = (MusyXEffectType*)&gAuxBEffect;
    }

    callback = InitAuxEffect(type, auxEffectSettings);

    if (callback == NULL)
    {
        nlPrintf("PlatAudio::SetAuxEffects(), callback is NULL. Should not happen unless values are invalid.\n");
        return false;
    }

    *pAuxEffect = type;
    *(void**)pAuxEffectSettings = auxEffectSettings;

    if (bA)
    {
        sndSetAuxProcessingCallbacks(studio, callback, *(void**)pAuxEffectSettings, 0xFF, 0, NULL, NULL, 0xFF, 0);
    }
    else
    {
        sndSetAuxProcessingCallbacks(studio, NULL, NULL, 0xFF, 0, callback, *(void**)pAuxEffectSettings, 0xFF, 0);
    }

    return true;
}

namespace PlatAudio
{

/**
 * Offset/Address/Size: 0x226C | 0x801C6A68 | size: 0x28
 */
bool AddAuxEffectA(MusyXEffectType type, void* auxEffectSettings, unsigned char studio)
{
    return AddAuxEffect(type, auxEffectSettings, true, studio);
}

/**
 * Offset/Address/Size: 0x2294 | 0x801C6A90 | size: 0x21C
 */
bool ShutdownAuxEffectA()
{
    if (gUsingDolbyProLogic2)
    {
        s32 auxEffect = gDPL2AuxAEffect;
        uintptr_t auxEffectSettings = gDPL2AuxAEffectSettings;

        if (auxEffect == 0)
        {
            nlPrintf("PlatAudio::ShutdownAuxEffect() trying to shutdown with MUSYX_EFFECT_NONE.\n");
            return true;
        }

        sndSetAuxProcessingCallbacks(0, NULL, NULL, 0xFF, 0, NULL, NULL, 0xFF, 0);

        switch (auxEffect)
        {
        case MUSYX_EFFECT_REVERB:
            if (!sndAuxCallbackShutdownReverbSTD((SND_AUX_REVERBSTD*)auxEffectSettings))
                return false;
            break;
        case MUSYX_EFFECT_REVERB_HI:
            if (!sndAuxCallbackShutdownReverbHI((SND_AUX_REVERBHI*)auxEffectSettings))
            {
                nlPrintf("sndAuxCallbackShutdownReverbHI() returned false.\n");
                return false;
            }
            break;
        case MUSYX_EFFECT_CHORUS:
            if (!sndAuxCallbackShutdownChorus((SND_AUX_CHORUS*)auxEffectSettings))
                return false;
            break;
        case MUSYX_EFFECT_DELAY:
            if (!sndAuxCallbackShutdownDelay((SND_AUX_DELAY*)auxEffectSettings))
                return false;
            break;
        }

        return true;
    }
    else
    {
        s32 auxEffect = gAuxAEffect;
        uintptr_t auxEffectSettings = gAuxAEffectSettings;

        if (auxEffect == 0)
        {
            nlPrintf("PlatAudio::ShutdownAuxEffect() trying to shutdown with MUSYX_EFFECT_NONE.\n");
            return true;
        }

        sndSetAuxProcessingCallbacks(0, NULL, NULL, 0xFF, 0, NULL, NULL, 0xFF, 0);

        switch (auxEffect)
        {
        case MUSYX_EFFECT_REVERB:
            if (!sndAuxCallbackShutdownReverbSTD((SND_AUX_REVERBSTD*)auxEffectSettings))
                return false;
            break;
        case MUSYX_EFFECT_REVERB_HI:
            if (!sndAuxCallbackShutdownReverbHI((SND_AUX_REVERBHI*)auxEffectSettings))
            {
                nlPrintf("sndAuxCallbackShutdownReverbHI() returned false.\n");
                return false;
            }
            break;
        case MUSYX_EFFECT_CHORUS:
            if (!sndAuxCallbackShutdownChorus((SND_AUX_CHORUS*)auxEffectSettings))
                return false;
            break;
        case MUSYX_EFFECT_DELAY:
            if (!sndAuxCallbackShutdownDelay((SND_AUX_DELAY*)auxEffectSettings))
                return false;
            break;
        }

        return true;
    }
}

/**
 * Offset/Address/Size: 0x24B0 | 0x801C6CAC | size: 0x24
 */
bool DeactivateDPL2()
{
    if (gUsingDolbyProLogic2 == false)
    {
        return true;
    }
    gUsingDolbyProLogic2 = false;
    return true;
}

/**
 * Offset/Address/Size: 0x24D4 | 0x801C6CD0 | size: 0x24
 */
bool ActivateDPL2()
{
    if (gUsingDolbyProLogic2)
    {
        return true;
    }
    gUsingDolbyProLogic2 = true;
    return true;
}

/**
 * Offset/Address/Size: 0x24F8 | 0x801C6CF4 | size: 0x60
 */
void SetOutputMode(MusyXOutputType output)
{
    switch (output)
    {
    case MusyXOutputType_MONO:
        sndOutputMode(SND_OUTPUTMODE_MONO);
        return;
    case MusyXOutputType_STEREO:
        sndOutputMode(SND_OUTPUTMODE_STEREO);
        return;
    case MusyXOutputType_SURROUND:
        sndOutputMode(SND_OUTPUTMODE_SURROUND);
        return;
    }
}

} // namespace PlatAudio

/**
 * Offset/Address/Size: 0x2558 | 0x801C6D54 | size: 0x278
 */
void PrintSoundStackInfo()
{
    static u32 prevAvailPrimaryStackSampleMem = PlatAudio::gPrimaryStackSize - 0x500;
    static u32 prevAvailSecondaryStackSampleMem = 0x2B4000;
    static bool bRunOnce = true;

    for (int i = 0; i < 2; i++)
    {
        u32 uAvailSampleMem = sndStackGetAvailableSampleMemory(stack_list[i].id);
        nlPrintf("Available sample memory in sound stack ID %d: %d\n", stack_list[i].id, uAvailSampleMem);

        if (bRunOnce)
        {
            u32 start;
            u32 end;
            if ((unsigned char)sndStackGetARAMAddressRange(stack_list[i].id, &start, &end))
            {
                tDebugPrintManager::Print(DC_SOUND, "ARAM address range for sound stack ID %d: %d (start) to %d (end)\n", stack_list[i].id, start, end);
            }
            else
            {
                tDebugPrintManager::Print(DC_SOUND, "Could not get ARAM address range for stack ID %d!\n", stack_list[i].id);
            }
        }

        if (stack_list[i].id == 0xFFFFFFFE)
        {
            if (uAvailSampleMem < prevAvailPrimaryStackSampleMem)
            {
                tDebugPrintManager::Print(DC_SOUND, "Primary sound stack ARAM used: %d\n", prevAvailPrimaryStackSampleMem - uAvailSampleMem);
                prevAvailPrimaryStackSampleMem = uAvailSampleMem;
            }
            else if (uAvailSampleMem > prevAvailPrimaryStackSampleMem)
            {
                tDebugPrintManager::Print(DC_SOUND, "Primary sound stack ARAM freed: %d\n", uAvailSampleMem - prevAvailPrimaryStackSampleMem);
                prevAvailPrimaryStackSampleMem = uAvailSampleMem;
            }
            else if (uAvailSampleMem == prevAvailPrimaryStackSampleMem)
            {
                tDebugPrintManager::Print(DC_SOUND, "Primary sound stack ARAM used/freed: 0\n");
            }

            if (uAvailSampleMem == PlatAudio::gPrimaryStackSize - 0x500)
            {
                prevAvailPrimaryStackSampleMem = PlatAudio::gPrimaryStackSize - 0x500;
                tDebugPrintManager::Print(DC_SOUND, "Primary sound stack is now empty.\n");
            }
        }
        else
        {
            if (uAvailSampleMem < prevAvailSecondaryStackSampleMem)
            {
                tDebugPrintManager::Print(DC_SOUND, "Secondary sound stack ARAM used: %d\n", prevAvailSecondaryStackSampleMem - uAvailSampleMem);
                prevAvailSecondaryStackSampleMem = uAvailSampleMem;
            }
            else if (uAvailSampleMem > prevAvailSecondaryStackSampleMem)
            {
                tDebugPrintManager::Print(DC_SOUND, "Secondary sound stack ARAM freed: %d\n", uAvailSampleMem - prevAvailSecondaryStackSampleMem);
                prevAvailSecondaryStackSampleMem = uAvailSampleMem;
            }
            else if (uAvailSampleMem == prevAvailSecondaryStackSampleMem)
            {
                tDebugPrintManager::Print(DC_SOUND, "Secondary sound stack ARAM used/freed: 0\n");
            }

            if (uAvailSampleMem == 0x2B4000)
            {
                prevAvailSecondaryStackSampleMem = 0x2B4000;
                tDebugPrintManager::Print(DC_SOUND, "Secondary sound stack is now empty.\n");
            }
        }
    }

    bRunOnce = false;
}

/**
 * Offset/Address/Size: 0x27D0 | 0x801C6FCC | size: 0x74
 */
void PrintAvailableARAMMemory()
{
    for (int i = 0; i < 2; i++)
    {
        u32 uAvailSampleMem = sndStackGetAvailableSampleMemory(stack_list[i].id);
        tDebugPrintManager::Print(DC_MEMORY, "Free Aram: %u\n", uAvailSampleMem);
    }
}

/**
 * Offset/Address/Size: 0x2844 | 0x801C7040 | size: 0x20
 */
void musyXFree(void* addr)
{
    nlFree(addr);
}

/**
 * Offset/Address/Size: 0x2864 | 0x801C7060 | size: 0x28
 */
void* musyXAlloc(size_t size)
{
    return nlMalloc(size, 0x20, false);
}

ARAMTransferHelper* ARAMTransferHelper::m_pARAMHelper;
unsigned char ARAMTransferHelper::m_bFileOpened;
nlFile* ARAMTransferHelper::m_pFile;
const char* ARAMTransferHelper::m_szFileName;

ARAMTransferHelperLoadEntireFile* ARAMTransferHelperLoadEntireFile::m_pARAMHelper;
const char* ARAMTransferHelperLoadEntireFile::m_szFileName;
u32 ARAMTransferHelperLoadEntireFile::m_uFileSize;
nlFile* ARAMTransferHelperLoadEntireFile::s_pFile;
