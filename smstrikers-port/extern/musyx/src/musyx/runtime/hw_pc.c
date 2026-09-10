#include "musyx/platform.h"

// TODO: Finish implementation, rename platform specific functions
// TODO: Use macros to alias platform specific calls, or ifdef for each?

#if MUSY_TARGET == MUSY_TARGET_PC
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "musyx/assert.h"
#include "musyx/hardware.h"
#include "musyx/sal.h"
#include <string.h>

static volatile u32 oldState = 0;
static volatile u16 hwIrqLevel = 0;
static volatile u32 salDspInitIsDone = 0;
static volatile u64 salLastTick = 0;
static volatile u32 salLogicActive = 0;
static volatile u32 salLogicIsWaiting = 0;
static volatile u32 salDspIsDone = 0;
void* salAIBufferBase = NULL;
static u8 salAIBufferIndex = 0;
static SND_SOME_CALLBACK userCallback = NULL;

#define DMA_BUFFER_LEN 0x280

#ifdef _WIN32
HANDLE globalMutex;
HANDLE globalInterrupt;
#else
pthread_mutex_t globalMutex;
pthread_mutex_t globalInterrupt;
#endif

u32 salGetStartDelay();
static void callUserCallback() {
  if (salLogicActive) {
    return;
  }
  salLogicActive = 1;
  // OSEnableInterrupts();
  userCallback();
  // OSDisableInterrupts();
  salLogicActive = 0;
}

/* PORT: the AI layer; src/platform/ai_ar.c, declared here rather than
   included, because this target's include path is MusyX's own and does not
   carry the SDK headers. The widths are include/dolphin/ai.h's: an AI address
   is a host pointer here, not a physical one. */
typedef void (*AIDCallback)();
extern AIDCallback AIRegisterDMACallback(AIDCallback callback);
extern void AIInitDMA(uintptr_t start_addr, u32 length);
extern void* AIPortDMADest(void);
extern void AIPortRunDMACallback(void);

void salCallback() {
  salAIBufferIndex = (salAIBufferIndex + 1) % 4;
  /* PORT: queue the slot that has just been rendered, as hw_dolphin.c does.
     Nothing consumed this before, the host read the ring directly, and
     THPSimple reads it back through AIGetDMAStartAddr to mix a movie over. */
  AIInitDMA((uintptr_t)salAIBufferBase + (salAIBufferIndex * DMA_BUFFER_LEN),
            DMA_BUFFER_LEN);
  salLastTick = 0; // OSGetTick();
  if (salDspIsDone) {
    callUserCallback();
  } else {
    salLogicIsWaiting = 1;
  }
}

void dspInitCallback() {
  salDspIsDone = TRUE;
  salDspInitIsDone = TRUE;
}

void dspResumeCallback() {
  salDspIsDone = TRUE;
  if (salLogicIsWaiting) {
    salLogicIsWaiting = FALSE;
    callUserCallback();
  }
}

bool salInitAi(SND_SOME_CALLBACK callback, u32 unk, u32* outFreq) {
  if ((salAIBufferBase = salMalloc(DMA_BUFFER_LEN * 4)) != NULL) {
    memset(salAIBufferBase, 0, DMA_BUFFER_LEN * 4);
    // DCFlushRange(salAIBufferBase, DMA_BUFFER_LEN * 4);
    salAIBufferIndex = TRUE;
    salLogicIsWaiting = FALSE;
    salDspIsDone = TRUE;
    salLogicActive = FALSE;
    userCallback = callback;
    /* PORT: register, so THPSimple can displace this and chain it for the
       length of a movie. */
    AIRegisterDMACallback(salCallback);
    AIInitDMA((uintptr_t)salAIBufferBase + (salAIBufferIndex * DMA_BUFFER_LEN),
              DMA_BUFFER_LEN);
    synthInfo.numSamples = 0x20;
    *outFreq = 32000;
    MUSY_DEBUG("MusyX AI interface initialized.\n");
    return TRUE;
  }

  return FALSE;
}

/* PORT: the host transport. src/platform/audio_out.cpp opens an SDL3 stream and pulls. */
extern int PortAudioStart(void);
extern void PortAudioStop(void);

/* `int` rather than `bool`: musyx.h typedefs bool to unsigned long. */
bool salStartAi() { return PortAudioStart() ? TRUE : FALSE; }

/* PORT: calling this advances the engine. Returns the buffer AI DMA would be playing, then
   runs one boundary, which runs the whole engine for 5 ms. Both halves go
   through the AI layer so a movie can wrap it: while one plays, the registered
   callback is THPSimple's and the buffer named is its mix over what MusyX
   rendered. What plays at a boundary is what was queued before it. */
unsigned int salPortBufferBytes(void) { return DMA_BUFFER_LEN; }

void* salPortNextBuffer(void) {
  void* played;

  if (salAIBufferBase == NULL) {
    return NULL;
  }
  played = AIPortDMADest();
  AIPortRunDMACallback();
  return played;
}

bool salExitAi() {
  /* PORT: stop the device before freeing the ring it reads out of, and take
     the callback back off the AI layer with it, hw_dolphin.c's line. */
  PortAudioStop();
  AIRegisterDMACallback(NULL);
  salFree(salAIBufferBase);
  return TRUE;
}

void* salAiGetDest() {
  u8 index; // r31
  index = (salAIBufferIndex + 2) % 4;
  /* PORT: return the slot rather than NULL, hw_dolphin.c's line. AI plays
     the buffer two behind the one being rendered, so this is where the mixer
     writes. Harmless while salBuildCommandList is a stub and required the
     moment it is not. */
  return (void*)((u8*)salAIBufferBase + (index * DMA_BUFFER_LEN));
}

bool salInitDsp(u32 arg0) { return TRUE; }

/* PORT: FALSE, not false; musyx.h only includes <stdbool.h> on C11 and up
   and these objects compile as gnu99, so the literal is undeclared on any
   host whose libc does not pull it in. */
bool salExitDsp() { return FALSE; }

void salStartDsp(u16* cmdList) {}

/* PORT: the software mixer, in place of a command list this target cannot build and a DSP it does not have. */
extern void PortAudioMixRender(s16* dest);

void salCtrlDsp(s16* dest) {
  /* salBuildCommandList(dest, salGetStartDelay());
     salStartDsp(dspCmdList); */
  PortAudioMixRender(dest);
}

u32 salGetStartDelay() { return 0; }

void hwInitIrq() {
  // oldState = OSDisableInterrupts();
  hwIrqLevel = 1;
#ifdef _WIN32
  globalMutex = CreateMutex(NULL, FALSE, NULL);
#elif defined(__linux__) && !defined(__ANDROID__)
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ROBUST);
  pthread_mutex_init(&globalMutex, &attr);
#else
  // TODO
#endif
}

void hwExitIrq() {}

void hwEnableIrq() {
  if (--hwIrqLevel == 0) {
    // OSRestoreInterrupts(oldState);
  }
}

void hwDisableIrq() {
  if ((hwIrqLevel++) == 0) {
    // oldState = OSDisableInterrupts();
  }
}

void hwIRQEnterCritical() {
#ifdef _WIN32
  DWORD waitResult = WaitForSingleObject(globalMutex, INFINITE);
#else
  pthread_mutex_lock(&globalMutex);
#endif
}

void hwIRQLeaveCritical() {
#ifdef _WIN32
  ReleaseMutex(globalMutex);
#else
  pthread_mutex_unlock(&globalMutex);
#endif
}
#endif
