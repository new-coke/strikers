// Host stand-ins for the audio DMA (AI) and auxiliary RAM (AR/ARQ) hardware.

#include <stdint.h>
#include <stdlib.h>

#include "dolphin/ai.h"
#include "dolphin/types.h"

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

void AIInit(u8* stack) { (void)stack; }
void AIReset(void) {}
void AIResetStreamSampleCount(void) {}

AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
    AIDCallback prev = s_ai_cb;
    s_ai_cb = callback;
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

void AIPortRunDMACallback(void)
{
    if (s_ai_cb != NULL)
    {
        s_ai_cb();
    }
}
