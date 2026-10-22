// Host stand-ins for the audio DMA (AI) and auxiliary RAM (AR/ARQ) hardware.

#include <stdint.h>
#include <stdlib.h>

#include "dolphin/ai.h"
#include "dolphin/types.h"
#include "musyx/port_audio_watchdog.h"

#if defined(STRIKERS_VITA_AUDIO_THREAD)
#include <pthread.h>
#endif

// ARAM ARInit hands the library a stack of chunk addresses; the game then calls ARAlloc to carve
// the space up.

#define ARAM_SIZE (16u * 1024u * 1024u)   // retail ARAM
#define ARAM_BASE 0x00000000u

static u8* s_aram;
static u32 s_aram_used;

u32 ARInit(u32* stack_index_addr, u32 num_entries)
{
    (void)stack_index_addr;
    (void)num_entries;
    if (s_aram == NULL)
        s_aram = (u8*)calloc(ARAM_SIZE, 1);
    s_aram_used = 0;
    return ARAM_BASE;
}

void ARReset(void) { s_aram_used = 0; }

u32 ARGetBaseAddress(void) { return ARAM_BASE; }

u32 ARAlloc(u32 length)
{
    u32 at = s_aram_used;
    length = (length + 31u) & ~31u;           // 32-byte aligned, as on console
    if ((u64)at + length > ARAM_SIZE)
        return 0;
    s_aram_used = at + length;
    return ARAM_BASE + at;
}

u32 ARFree(u32* length)
{
    // The console API frees back to the high-water mark rather than per block.
    if (length)
        *length = ARAM_SIZE - s_aram_used;
    s_aram_used = 0;
    return ARAM_BASE;
}

// Lets the audio backend turn an ARAM handle into something it can read.
void* ARPortResolve(u32 aram_addr)
{
    if (s_aram == NULL || aram_addr < ARAM_BASE)
        return NULL;
    u32 off = aram_addr - ARAM_BASE;
    return off < ARAM_SIZE ? s_aram + off : NULL;
}

// ARQ, the DMA queue between ARAM and main memory.
// ---------------------------------------------------------------------------.

static u32 s_arq_chunk = 4096;

void ARQInit(void) {}
void ARQReset(void) {}
void ARQSetChunkSize(u32 size) { s_arq_chunk = size; }

// AI, audio interface DMA.

static AIDCallback s_ai_cb;
static uintptr_t s_ai_dma_addr;
static u32 s_ai_dma_len;

#if defined(STRIKERS_VITA_AUDIO_THREAD)
static pthread_once_t s_ai_callback_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t s_ai_callback_mutex;

static void ai_callback_mutex_init(void)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&s_ai_callback_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

void AIPortLockCallbacks(void)
{
    pthread_once(&s_ai_callback_once, ai_callback_mutex_init);
    pthread_mutex_lock(&s_ai_callback_mutex);
}

void AIPortUnlockCallbacks(void)
{
    pthread_mutex_unlock(&s_ai_callback_mutex);
}
#else
void AIPortLockCallbacks(void) {}
void AIPortUnlockCallbacks(void) {}
#endif

void AIInit(u8* stack) { (void)stack; }
void AIReset(void) {}
void AIResetStreamSampleCount(void) {}

AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
#if defined(STRIKERS_VITA_AUDIO_THREAD)
    AIPortLockCallbacks();
#endif
    AIDCallback prev = s_ai_cb;
    s_ai_cb = callback;
#if defined(STRIKERS_VITA_AUDIO_THREAD)
    AIPortUnlockCallbacks();
#endif
    return prev;
}

void AIInitDMA(uintptr_t start_addr, u32 length)
{
    s_ai_dma_addr = start_addr;
    s_ai_dma_len = length;
}

void AIStartDMA(void) {}

uintptr_t AIGetDMAStartAddr(void) { return s_ai_dma_addr; }
u32 AIGetDSPSampleRate(void) { return 1; }   // 1 = 48 kHz, as on retail

// Called from hw_pc.c's salPortNextBuffer, once per buffer.
void* AIPortDMADest(void) { return (void*)s_ai_dma_addr; }

// Atomically observe the buffer that was active at this DMA boundary and then
// run the callback that schedules the next one.  THPSimple swaps both the AI
// callback and DMA state while holding this same recursive mutex; keeping the
// read + callback in one critical section prevents pairing an old DMA address
// with a newly-installed movie callback (or vice versa).
void* AIPortAdvanceDMA(void)
{
    const unsigned int previous = PortAudioWatchdogMark(PORT_AUDIO_WD_AI_CALLBACK | PORT_AUDIO_WD_WAIT);
#if defined(STRIKERS_VITA_AUDIO_THREAD)
    AIPortLockCallbacks();
#endif
    PortAudioWatchdogMark(PORT_AUDIO_WD_AI_CALLBACK);
    void* played = (void*)s_ai_dma_addr;
    if (s_ai_cb != NULL)
    {
        s_ai_cb();
    }
#if defined(STRIKERS_VITA_AUDIO_THREAD)
    AIPortUnlockCallbacks();
#endif
    PortAudioWatchdogMark(previous);
    return played;
}

void AIPortRunDMACallback(void)
{
#if defined(STRIKERS_VITA_AUDIO_THREAD)
    AIPortLockCallbacks();
#endif
    if (s_ai_cb != NULL)
    {
        s_ai_cb();
    }
#if defined(STRIKERS_VITA_AUDIO_THREAD)
    AIPortUnlockCallbacks();
#endif
}
