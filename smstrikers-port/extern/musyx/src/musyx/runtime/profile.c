#include "musyx/platform.h"
#if MUSYX_PLATFORM == MUSYX_DOLPHIN
#include "dolphin/PPCArch.h"
#include "musyx/musyx.h"

typedef struct SND_PROFILE_DATA {
  // total size: 0x68
  unsigned long loadStores; // offset 0x0, size 0x4
  unsigned long missCycles; // offset 0x4, size 0x4
  unsigned long cycles; // offset 0x8, size 0x4
  unsigned long instructions; // offset 0xC, size 0x4
  unsigned long lastLoadStores; // offset 0x10, size 0x4
  unsigned long lastMissCycles; // offset 0x14, size 0x4
  unsigned long lastCycles; // offset 0x18, size 0x4
  unsigned long lastInstructions; // offset 0x1C, size 0x4
  unsigned long peekLoadStores; // offset 0x20, size 0x4
  unsigned long peekMissCycles; // offset 0x24, size 0x4
  unsigned long peekCycles; // offset 0x28, size 0x4
  unsigned long peekInstructions; // offset 0x2C, size 0x4
  unsigned long _loadStores; // offset 0x30, size 0x4
  unsigned long _missCycles; // offset 0x34, size 0x4
  unsigned long _cycles; // offset 0x38, size 0x4
  unsigned long _instructions; // offset 0x3C, size 0x4
  float avgLoadStores; // offset 0x40, size 0x4
  float avgMissCycles; // offset 0x44, size 0x4
  float avgCycles; // offset 0x48, size 0x4
  float avgInstructions; // offset 0x4C, size 0x4
  float sumLoadStores; // offset 0x50, size 0x4
  float sumMissCycles; // offset 0x54, size 0x4
  float sumCycles; // offset 0x58, size 0x4
  float sumInstructions; // offset 0x5C, size 0x4
  unsigned long cnt; // offset 0x60, size 0x4
  unsigned long paused; // offset 0x64, size 0x4
} SND_PROFILE_DATA;

typedef struct SND_PROFILE_INFO {
  // total size: 0x274
  SND_PROFILE_DATA dspCtrl; // offset 0x0, size 0x68
  SND_PROFILE_DATA auxProcessing; // offset 0x68, size 0x68
  SND_PROFILE_DATA sequencer; // offset 0xD0, size 0x68
  SND_PROFILE_DATA synthesizer; // offset 0x138, size 0x68
  SND_PROFILE_DATA emitters; // offset 0x1A0, size 0x68
  SND_PROFILE_DATA streaming; // offset 0x208, size 0x68
  unsigned char numMusicVoices; // offset 0x270, size 0x1
  unsigned char numSFXVoices; // offset 0x271, size 0x1
} SND_PROFILE_INFO;
typedef void (* SND_PROF_USERCALLBACK)(struct SND_PROFILE_INFO *); // size: 0x4, address: 0x0


SND_PROF_USERCALLBACK sndProfUserCallback = NULL;

#if 0
void sndProfSetCallback(SND_PROF_USERCALLBACK callback) { sndProfUserCallback = callback; }

void sndProfUpdateMisc(SND_PROFILE_INFO* info) {
#ifdef MUSYX_DEBUG
  info->numMusicVoices = voiceMusicRunning;
  info->numSFXVoices = voiceFxRunning;
#endif
}

void sndProfResetPMC(SND_PROFILE_DATA* info) {
#if MUSYX_DEBUG
  PPCMtpmc1(0);
  PPCMtpmc2(0);
  PPCMtpmc3(0);
  PPCMtpmc4(0);
  info->sumLoadStores = info->loadStores = 0;
  info->sumMissCycles = info->missCycles = 0;
  info->sumCycles = info->cycles = 0;
  info->sumInstructions = info->instructions = 0;
  info->peekLoadStores = 0;
  info->peekMissCycles = 0;
  info->peekCycles = 0;
  info->peekInstructions = 0;
  info->cnt = 0;
  info->paused = 1;
#endif
}
void sndProfStartPMC(SND_PROFILE_DATA* info) {
#if MUSYX_DEBUG

  PPCMtmmcr0(0);
  PPCMtmmcr1(0);
  info->paused = 0;
  info->_loadStores = PPCMfpmc2();
  info->_missCycles = PPCMfpmc3();
  info->_cycles = PPCMfpmc4();
  info->_instructions = PPCMfpmc1();
  PPCMtmmcr1(0x78400000);
  PPCMtmmcr0(0xc08b);
#endif
}

void sndProfPausePMC(SND_PROFILE_DATA* info) {
#if MUSYX_DEBUG

  PPCMtmmcr0(0);
  PPCMtmmcr1(0);
  info->loadStores += PPCMfpmc2() - info->_loadStores;
  info->missCycles += PPCMfpmc3() - info->_missCycles;
  info->cycles += PPCMfpmc4() - info->_cycles;
  info->instructions += PPCMfpmc1() - info->_instructions;
  info->paused = 1;
  PPCMtmmcr1(0x78400000);
  PPCMtmmcr0(0xc08b);
#endif
}

void sndProfStopPMC(SND_PROFILE_DATA* info) {
#if MUSYX_DEBUG
  PPCMtmmcr0(0);
  PPCMtmmcr1(0);
  if (info->paused == 0) {
    info->loadStores = info->loadStores + (PPCMfpmc2() - info->_loadStores);
    info->missCycles = info->missCycles + (PPCMfpmc3() - info->_missCycles);
    info->cycles = info->cycles + (PPCMfpmc4() - info->_cycles);
    info->instructions = info->instructions + (PPCMfpmc1() - info->_instructions);
    info->paused = 1;
  }
  info->cnt = info->cnt + 1;
  info->sumLoadStores += info->loadStores;
  info->sumMissCycles += info->missCycles;

  info->sumCycles += info->cycles;
  info->sumInstructions += info->instructions;
  info->avgLoadStores = info->sumLoadStores / info->cnt;
  info->avgMissCycles = info->sumMissCycles / info->cnt;
  info->avgCycles = info->sumCycles / info->cnt;
  info->avgInstructions = info->sumInstructions / info->cnt;
  info->lastLoadStores = info->loadStores;
  info->lastMissCycles = info->missCycles;
  info->lastCycles = info->cycles;
  info->lastInstructions = info->instructions;
  if (info->loadStores > info->peekLoadStores) {
    info->peekLoadStores = info->loadStores;
  }
  if (info->missCycles > info->peekMissCycles) {
    info->peekMissCycles = info->missCycles;
  }
  if (info->cycles > info->peekCycles) {
    info->peekCycles = info->cycles;
  }
  if (info->instructions > info->peekInstructions) {
    info->peekInstructions = info->instructions;
  }
  info->loadStores = 0;
  info->missCycles = 0;
  info->cycles = 0;
  info->instructions = 0;
  PPCMtmmcr1(0x78400000);
  PPCMtmmcr0(0xc08b);
#endif
}
#endif

#endif