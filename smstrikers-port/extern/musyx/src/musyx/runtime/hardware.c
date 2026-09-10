#include "musyx/platform.h"

#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
#include "dolphin/os/OSCache.h"
#endif

#include "musyx/musyx.h"
#include "musyx/hardware.h"
#include "math.h"
#include "float.h"
#include "musyx/assert.h"
#include "musyx/s3d.h"
#include "musyx/sal.h"
#include "musyx/seq.h"
#include "musyx/snd.h"
#include "musyx/stream.h"
#include "musyx/synth.h"

static volatile const u16 itdOffTab[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,  7,  7,  8,  8,  8,
    9,  9,  9,  10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 16, 16, 17,
    17, 17, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 22, 22, 22, 23, 23, 23, 24, 24, 24, 25,
    25, 25, 26, 26, 26, 27, 27, 27, 28, 28, 28, 28, 29, 29, 29, 29, 29, 30, 30, 30, 30, 30,
    31, 31, 31, 31, 31, 31, 31, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
};

// SND_PROFILE_INFO prof;

u8 salFrame;
u8 salAuxFrame;
u8 salNumVoices;
u8 salMaxStudioNum;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
SND_HOOKS salHooks;
#else
SND_HOOKS_EX salHooks;
#endif
u8 salTimeOffset;
void hwSetTimeOffset(u8 offset);

static void snd_handle_irq() {
  u8 r; // r31
  u8 i; // r30
  u8 v; // r29
  if (sndActive == 0) {
    return;
  }

  streamCorrectLoops();
  hwIRQEnterCritical();
  // sndProfStartPCM(&prof.dspCtrl);
  salCtrlDsp(salAiGetDest());
  // sndProfStopPMC(&prof.dspCtrl);
  hwIRQLeaveCritical();
  hwIRQEnterCritical();
  // sndProfStartPCM(&prof.auxProcessing);
  salHandleAuxProcessing();
  // sndProfStopPMC(&prof.auxProcessing);
  hwIRQLeaveCritical();
  hwIRQEnterCritical();
  salFrame ^= 1;
  salAuxFrame = (salAuxFrame + 1) % 3;

  for (v = 0; v < salNumVoices; ++v) {
    for (i = 0; i < 5; ++i) {
      dspVoice[v].changed[i] = 0;
    }
  }

  // sndProfStartPMC(&prof.sequencer);
  // sndProfPausePMC(&prof.sequencer);
  // sndProfStartPMC(&prof.synthesizer);
  // sndProfPausePMC(&prof.synthesizer);
  hwIRQLeaveCritical();
  for (r = 0; r < 5; ++r) {
    hwIRQEnterCritical();
    hwSetTimeOffset(r);
    // sndProfStartPMC(&prof.sequencer);
    seqHandle(256);
    // sndProfPausePMC(&prof.sequencer);
    // sndProfStartPMC(&prof.synthesizer);
    synthHandle(256);
    // sndProfPausePMC(&prof.synthesizer);
    hwIRQLeaveCritical();
  }

  // sndProfStopPMC(&prof.sequencer);
  // sndProfStopPMC(&prof.synthesizer);
  hwIRQEnterCritical();
  hwSetTimeOffset(0);
  // sndProfStartPMC(&prof.emitters);
  s3dHandle();
  // sndProfStopPMC(&prof.emitters);
  hwIRQLeaveCritical();
  hwIRQEnterCritical();
  // sndProfStartPMC(&prof.streaming);
  streamHandle();
  // sndProfStopPMC(&prof.streaming);
  hwIRQLeaveCritical();
  hwIRQEnterCritical();
  vsSampleUpdates();
  hwIRQLeaveCritical();
  // sndProfUpdateMisc(&prof);

  // if (sndProfUserCallback) {
  //   sndProfUserCallback(&prof);
  // }
}

s32 hwInit(u32* frq, u16 numVoices, u16 numStudios, u32 flags) {
  MUSY_DEBUG("Entering hwInit()\n\n");
  hwInitIrq();
  salFrame = 0;
  salAuxFrame = 0;
  salMessageCallback = NULL;
  if (salInitAi(snd_handle_irq, flags, frq) != 0) {
    MUSY_DEBUG("salInitAi() is done.\n\n");
    if (salInitDspCtrl(numVoices, numStudios, (flags & 1) != 0) != 0) {
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
      if (flags & 0x8) {
        hwEnableCompressor();
      }
#endif
      MUSY_DEBUG("salInitDspCtrl() is done.\n\n");
      if (salInitDsp(flags)) {
        MUSY_DEBUG("salInitDsp() is done.\n\n");
        hwEnableIrq();
        MUSY_DEBUG("Starting AI DMA...\n\n");
        salStartAi();
        MUSY_DEBUG("hwInit() done.\n\n");
        return 0;
      }
      MUSY_DEBUG("Could not initialize DSP.\n");
    } else {
      MUSY_DEBUG("Could not initialize DSP control logic.\n");
    }
  } else {
    MUSY_DEBUG("Could not initialize AI.\n");
  }
  return -1;
}

void hwExit() {
  hwDisableIrq();
  salExitDsp();
  salExitDspCtrl();
  salExitAi();
  hwEnableIrq();
  hwExitIrq();
}

void hwSetTimeOffset(u8 offset) { salTimeOffset = offset; }

u8 hwGetTimeOffset() { return salTimeOffset; }

u32 hwIsActive(u32 v) { return dspVoice[v].state != 0; }
u32 hwGlobalActivity() { return 0; }

void hwSetMesgCallback(SND_MESSAGE_CALLBACK callback) { salMessageCallback = callback; }

void hwSetPriority(u32 v, u32 prio) { dspVoice[v].prio = prio; }

void hwInitSamplePlayback(u32 v, u16 smpID, void* newsmp, u32 set_defadsr, u32 prio,
                          u32 callbackUserValue, u32 setSRC, u8 itdMode) {
  unsigned char i;  // r30
  unsigned long bf; // r29
  bf = 0;
  for (i = 0; i <= salTimeOffset; ++i) {
    bf |= dspVoice[v].changed[i] & 0x20;
    dspVoice[v].changed[i] = 0;
  }

  dspVoice[v].changed[0] = bf;
  dspVoice[v].prio = prio;
  dspVoice[v].mesgCallBackUserValue = callbackUserValue;
  dspVoice[v].flags = 0;
  dspVoice[v].smp_id = smpID;
  dspVoice[v].smp_info = *(SAMPLE_INFO*)newsmp;

  if (set_defadsr != 0) {
    dspVoice[v].adsr.mode = 0;
    dspVoice[v].adsr.data.dls.aTime = 0;
    dspVoice[v].adsr.data.dls.dTime = 0;
    dspVoice[v].adsr.data.dls.sLevel = 0x7FFF;
    dspVoice[v].adsr.data.dls.rTime = 0;
  }

  dspVoice[v].lastUpdate.pitch = 0xff;
  dspVoice[v].lastUpdate.vol = 0xff;
  dspVoice[v].lastUpdate.volA = 0xff;
  dspVoice[v].lastUpdate.volB = 0xff;

  if (setSRC != 0) {
    hwSetSRCType(v, 0);
    hwSetPolyPhaseFilter(v, 1);
  }

  hwSetITDMode(v, itdMode);
}

void hwBreak(s32 vid) {
  if (dspVoice[vid].state == 1 && salTimeOffset == 0) {
    dspVoice[vid].startupBreak = 1;
  }

  dspVoice[vid].changed[salTimeOffset] |= 0x20;
}

void hwSetADSR(u32 v, void* _adsr, u8 mode) {
  u32 sl;                              // r29
  ADSR_INFO* adsr = (ADSR_INFO*)_adsr; // r30

  switch (mode) {
  case 0: {
    dspVoice[v].adsr.mode = 0;
    dspVoice[v].adsr.data.linear.aTime = adsr->data.linear.atime;
    dspVoice[v].adsr.data.linear.dTime = adsr->data.linear.dtime;
    sl = adsr->data.linear.slevel << 3;
    if (sl > 0x7fff) {
      sl = 0x7fff;
    }

    dspVoice[v].adsr.data.linear.sLevel = sl;
    dspVoice[v].adsr.data.linear.rTime = adsr->data.linear.rtime;
    break;
  }
  case 1:
  case 2:
    dspVoice[v].adsr.mode = 1;
    dspVoice[v].adsr.data.dls.aMode = 0;
    if (mode == 1) {
      dspVoice[v].adsr.data.dls.aTime = adsrConvertTimeCents(adsr->data.dls.atime) & 0xFFFF;
      dspVoice[v].adsr.data.dls.dTime = adsrConvertTimeCents(adsr->data.dls.dtime) & 0xFFFF;

      sl = adsr->data.dls.slevel >> 2;
      if (sl > 0x3ff) {
        sl = 0x3ff;
      }

      dspVoice[v].adsr.data.dls.sLevel = 193 - dspScale2IndexTab[sl];
    } else {
      dspVoice[v].adsr.data.dls.aTime = adsr->data.dls.atime & 0xFFFF;
      dspVoice[v].adsr.data.dls.dTime = adsr->data.dls.dtime & 0xFFFF;
      dspVoice[v].adsr.data.dls.sLevel = adsr->data.dls.slevel;
    }

    dspVoice[v].adsr.data.dls.rTime = adsr->data.dls.rtime;
  }

  dspVoice[v].changed[0] |= 0x10;
}

void hwSetVirtualSampleLoopBuffer(u32 voice, void* addr, u32 len) {
  dspVoice[voice].vSampleInfo.loopBufferAddr = addr;
  dspVoice[voice].vSampleInfo.loopBufferLength = len;
}

u32 hwGetVirtualSampleState(u32 voice) { return dspVoice[voice].vSampleInfo.inLoopBuffer; }

u8 hwGetSampleType(u32 voice) { return dspVoice[voice].smp_info.compType; }

u16 hwGetSampleID(u32 voice) { return dspVoice[voice].smp_id; }

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
void* hwGetSampleExtraData(u32 voice) { return dspVoice[voice].smp_info.extraData; }
#endif

void hwSetStreamLoopPS(u32 voice, u8 ps) { dspVoice[voice].streamLoopPS = ps; }

void hwStart(u32 v, u8 studio) {
  dspVoice[v].singleOffset = salTimeOffset;
  salActivateVoice(&dspVoice[v], studio);
}

void hwKeyOff(u32 v) { dspVoice[v].changed[salTimeOffset] |= 0x40; }

void hwSetPitch(u32 v, u16 speed) {
  DSPvoice* dsp_vptr = &dspVoice[v];

  if (speed >= 0x4000) {
    speed = 0x3fff;
  }

  if (dsp_vptr->lastUpdate.pitch != 0xff &&
      dsp_vptr->pitch[dsp_vptr->lastUpdate.pitch] == speed * 16) {
    return;
  }

  dsp_vptr->pitch[salTimeOffset] = speed * 16;
  dsp_vptr->changed[salTimeOffset] |= 8;
  dsp_vptr->lastUpdate.pitch = salTimeOffset;
}

void hwSetSRCType(u32 v, u8 salSRCType) {
  static u16 dspSRCType[3] = {0, 1, 2};
  struct DSPvoice* dsp_vptr = &dspVoice[v];
  dsp_vptr->srcTypeSelect = dspSRCType[salSRCType];
  dsp_vptr->changed[0] |= 0x100;
}

void hwSetPolyPhaseFilter(u32 v, u8 salCoefSel) {
  static u16 dspCoefSel[3] = {0, 1, 2};
  DSPvoice* dsp_vptr = &dspVoice[v];
  dsp_vptr->srcCoefSelect = dspCoefSel[salCoefSel];
  dsp_vptr->changed[0] |= 0x80;
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
void hwLowPassFrqToCoef(u32 frq, u16* _a0, u16* _b1) {
  float c;  // f30
  float b1; // f31

  c = 2.0f - sndCos(((float)(2 * M_PI) * frq) / 32000.0f);
  b1 = c > 1.0f ? (sndSqrt((c * c) - 1.0f) - c) : -1.0f;
  b1 = CLAMP(b1, -1.0f, 1.0f);
  *_a0 = 32768.0f * (1.0f + b1);
  *_b1 = 32768.0f * -b1;
}

void hwSetFilter(u32 v, u8 mode, u16 coefA, u16 coefB) {
  struct DSPvoice* dsp_vptr = &dspVoice[v]; // r31
  if (dsp_vptr->filter.on == 0) {
    if (mode == 1) {
      dsp_vptr->filter.on = 1;
      dsp_vptr->filter.coefA = coefA;
      dsp_vptr->filter.coefB = coefB;
      dsp_vptr->changed[0] |= 0xC00;
    }
  } else {
    if (mode == 1) {
      dsp_vptr->filter.coefA = coefA;
      dsp_vptr->filter.coefB = coefB;
      dsp_vptr->changed[0] |= 0x800;
      return;
    }
    dsp_vptr->filter.on = 0;
    dsp_vptr->changed[0] |= 0x400;
  }
}
#endif

static void SetupITD(DSPvoice* dsp_vptr, u8 pan) {
  dsp_vptr->itdShiftL = itdOffTab[pan];
  dsp_vptr->itdShiftR = 32 - itdOffTab[pan];
  dsp_vptr->changed[0] |= 0x200;
}

void hwSetITDMode(u32 v, u8 mode) {
  if (!mode) {
    dspVoice[v].flags |= 0x80000000;
    dspVoice[v].itdShiftL = 16;
    dspVoice[v].itdShiftR = 16;
    return;
  }
  dspVoice[v].flags &= ~0x80000000;
}

#define hwGetITDMode(dsp_vptr) (dsp_vptr->flags & 0x80000000)

void hwSetVolume(u32 v, u8 table, float vol, u32 pan, u32 span, float auxa, float auxb) {
  SAL_VOLINFO vi;                    // r1+0x24
  u16 il;                            // r30
  u16 ir;                            // r29
  u16 is;                            // r28
  DSPvoice* dsp_vptr = &dspVoice[v]; // r31
  if (vol >= 1.f) {
    vol = 1.f;
  }

  if (auxa >= 1.f) {
    auxa = 1.f;
  }

  if (auxb >= 1.f) {
    auxb = 1.f;
  }

  salCalcVolume(table, &vi, vol, pan, span, auxa, auxb, hwGetITDMode(dsp_vptr) != 0,
                dspStudio[dsp_vptr->studio].type == SND_STUDIO_TYPE_DPL2);

  il = 32767.f * vi.volL;
  ir = 32767.f * vi.volR;
  is = 32767.f * vi.volS;

  if (dsp_vptr->lastUpdate.vol == 0xff || dsp_vptr->volL != il || dsp_vptr->volR != ir ||
      dsp_vptr->volS != is) {
    dsp_vptr->volL = il;
    dsp_vptr->volR = ir;
    dsp_vptr->volS = is;
    dsp_vptr->changed[0] |= 1;
    dsp_vptr->lastUpdate.vol = 0;
  }

  il = 32767.f * vi.volAuxAL;
  ir = 32767.f * vi.volAuxAR;
  is = 32767.f * vi.volAuxAS;

  if (dsp_vptr->lastUpdate.volA == 0xff || dsp_vptr->volLa != il || dsp_vptr->volRa != ir ||
      dsp_vptr->volSa != is) {
    dsp_vptr->volLa = il;
    dsp_vptr->volRa = ir;
    dsp_vptr->volSa = is;
    dsp_vptr->changed[0] |= 2;
    dsp_vptr->lastUpdate.volA = 0;
  }

  il = 32767.f * vi.volAuxBL;
  ir = 32767.f * vi.volAuxBR;
  is = 32767.f * vi.volAuxBS;

  if (dsp_vptr->lastUpdate.volB == 0xff || dsp_vptr->volLb != il || dsp_vptr->volRb != ir ||
      dsp_vptr->volSb != is) {
    dsp_vptr->volLb = il;
    dsp_vptr->volRb = ir;
    dsp_vptr->volSb = is;
    dsp_vptr->changed[0] |= 4;
    dsp_vptr->lastUpdate.volB = 0;
  }

  if (hwGetITDMode(dsp_vptr)) {
    SetupITD(dsp_vptr, (pan >> 16));
  }
}

void hwOff(s32 vid) { salDeactivateVoice(&dspVoice[vid]); }

void hwSetAUXProcessingCallbacks(u8 studio, SND_AUX_CALLBACK auxA, void* userA,
                                 SND_AUX_CALLBACK auxB, void* userB) {
  dspStudio[studio].auxAHandler = auxA;
  dspStudio[studio].auxAUser = userA;
  dspStudio[studio].auxBHandler = auxB;
  dspStudio[studio].auxBUser = userB;
}

void hwActivateStudio(u8 studio, bool isMaster, SND_STUDIO_TYPE type) {
  salActivateStudio(studio, isMaster, type);
}

void hwDeactivateStudio(u8 studio) { salDeactivateStudio(studio); }

void hwChangeStudioMix(u8 studio, u32 isMaster) { dspStudio[studio].isMaster = isMaster; }

bool hwIsStudioActive(u8 studio) { return dspStudio[studio].state == 1; }

bool hwAddInput(u8 studio, SND_STUDIO_INPUT* in_desc) {
  return salAddStudioInput(&dspStudio[studio], in_desc);
}

bool hwRemoveInput(u8 studio, SND_STUDIO_INPUT* in_desc) {
  return salRemoveStudioInput(&dspStudio[studio], in_desc);
}

void hwChangeStudio(u32 v, u8 studio) { salReconnectVoice(&dspVoice[v], studio); }

u32 hwGetPos(u32 v) {
  unsigned long pos; // r31
  unsigned long off; // r30
  if (dspVoice[v].state != 2) {
    return 0;
  }

  switch (dspVoice[v].smp_info.compType) {
  case 0:
  case 1:
  case 4:
  case 5:
    pos = ((dspVoice[v].currentAddr - (u32)dspVoice[v].smp_info.addr * 2) / 16) * 14;
    off = dspVoice[v].currentAddr & 0xf;
    if (off >= 2) {
      pos += off - 2;
    }
    break;
  case 3:
    pos = dspVoice[v].currentAddr - (u32)dspVoice[v].smp_info.addr;
    break;
  case 2:
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
  case 6:
#endif
    pos = dspVoice[v].currentAddr - ((u32)dspVoice[v].smp_info.addr / 2);
    break;
  }

  return pos;
}

void hwFlushStream(void* base, u32 offset, u32 bytes, u8 hwStreamHandle, void (*callback)(size_t),
                   u32 user) {
  size_t aram; // r28
  size_t mram; // r29
  size_t len;
  aram = aramGetStreamBufferAddress(hwStreamHandle, &len);
  bytes += (offset & 31);
  offset &= ~31;
  bytes = (bytes + 31) & ~31;
  /* PORT: uintptr_t, not u32, base is the game's MRAM stream buffer. */
  mram = (uintptr_t)base + offset;
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  DCStoreRange((void*)mram, bytes);
#endif
  // TODO: Platform specific audio memory handling
  aramUploadData((void*)mram, aram + offset, bytes, 1, callback, user);
}

void hwPrepareStreamBuffer() {}
u8 hwInitStream(u32 len) {
  // TODO: Platform specific audio memory handling
  return aramAllocateStreamBuffer(len);
}

void hwExitStream(u8 id) {
  // TODO: Platform specific audio memory handling
  aramFreeStreamBuffer(id);
}

void* hwGetStreamPlayBuffer(u8 hwStreamHandle) {
  // TODO: Platform specific audio memory handling
  return (void*)aramGetStreamBufferAddress(hwStreamHandle, NULL);
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
u32 hwGetStreamARAMAddr(u8 hwStreamHandle) {
  u32 len; // r1+0xC
  return aramGetStreamBufferAddress(hwStreamHandle, &len);
}
#endif

void* hwTransAddr(void* samples) { return samples; }

u32 hwFrq2Pitch(u32 frq) { return (frq * 4096.f) / synthInfo.mixFrq; }

void hwInitSampleMem(u32 baseAddr, u32 length) {
#line 940
  MUSY_ASSERT(baseAddr == 0x00000000);
  // TODO: Platform specific audio memory handling
  aramInit(length);
}

void hwExitSampleMem() { aramExit(); }

static u32 convert_length(u32 len, u8 type) {
  switch (type) {
  case 0:
  case 1:
  case 4:
  case 5:
    len = (((u32)((len + 13) / 14))) * 8;
    break;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
  case 6:
#endif
  case 2:
    len *= 2;
    break;
  }
  return len;
}

// TODO: Platform specific audio memory handling
void hwSaveSample(void* header, void* data
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                  ,
                  ARAMInfo* aramInfo
#endif
) {
#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
  u32 len = ((u32*)*((u32*)header))[1] & 0xFFFFFF;
  u8 type = ((u32*)*((u32*)header))[1] >> 0x18;
  len = convert_length(len, type);
  *((u32*)data) = (u32)aramStoreData((void*)*((u32*)data), len
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                                     ,
                                     aramInfo
#endif
  );
#else
  /* PORT: the same three lines with the pointers read at their own width. */
  {
    u32 hdr1 = ((u32*)(*(void**)header))[1];
    u32 len = convert_length(hdr1 & 0xFFFFFF, (u8)(hdr1 >> 0x18));
    *(void**)data = aramStoreData(*(void**)data, len
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                                  ,
                                  aramInfo
#endif
    );
  }
#endif
}

// TODO: Platform specific audio memory handling
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
u32 hwGetAvailableSampleMemory(ARAMInfo* ai) { return aramGetAvailableBytes(ai); }
#endif

// TODO: Platform specific audio memory handling
void hwSetSaveSampleCallback(ARAMUploadCallback callback, unsigned long chunckSize) {
  aramSetUploadCallback(callback, chunckSize);
}

// TODO: Platform specific audio memory handling
void hwRemoveSample(void* header, void* data
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                    ,
                    ARAMInfo* aramInfo
#endif
) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(1, 5, 3)
  u32 len = (((u32*)header))[1] & 0xFFFFFF;
  u8 type = (((u32*)header))[1] >> 0x18;
  len = convert_length(len, type);
#else
  u8 type = (((u32*)header))[1] >> 0x18;
  u32 len = convert_length((((u32*)header))[1] & 0xFFFFFF, type);
#endif
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  aramRemoveData(data, len);
#else
  aramRemoveData(data, len, aramInfo);
#endif
}

// TODO: Platform specific audio memory handling
void hwSyncSampleMem() { aramSyncTransferQueue(); }

void hwFrameDone() {}

void sndSetHooks(SND_HOOKS* hooks) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
  salHooks = *hooks;
#else
  salHooks.malloc = hooks->malloc;
  salHooks.mallocPhysical = hooks->malloc;
  salHooks.free = hooks->free;
#endif
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
void sndSetHooksEx(SND_HOOKS_EX* hooks) {
  salHooks.malloc = hooks->malloc;
  salHooks.mallocPhysical = hooks->mallocPhysical;
  salHooks.free = hooks->free;
}
#endif
void hwEnableHRTF() {
  if (dspHRTFOn != FALSE) {
    return;
  }
  dspHRTFOn = TRUE;
  salInitHRTFBuffer();
}
void hwDisableHRTF() { dspHRTFOn = FALSE; }

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
void hwEnableCompressor() { dspCompressorOn = TRUE; }

void hwDisableCompressor() { dspCompressorOn = FALSE; }
#endif

u32 hwGetVirtualSampleID(u32 v) {
  if (dspVoice[v].state == 0) {
    return 0xFFFFFFFF;
  }

  return dspVoice[v].virtualSampleID;
}

bool hwVoiceInStartup(u32 v) { return dspVoice[v].state == 1; }
