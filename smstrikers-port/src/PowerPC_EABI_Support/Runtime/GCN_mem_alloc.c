#include "dolphin/os.h"

static void InitDefaultHeap(void)
{
    void* arenaLo;
    void* arenaHi;

    OSReport("GCN_Mem_Alloc.c : InitDefaultHeap. No Heap Available\n");
    OSReport("Metrowerks CW runtime library initializing default heap\n");

    arenaLo = OSGetArenaLo();
    arenaHi = OSGetArenaHi();

    arenaLo = OSInitAlloc(arenaLo, arenaHi, 1);
    OSSetArenaLo(arenaLo);

    arenaLo = (void*)OSRoundUp32B(arenaLo);
    arenaHi = (void*)OSRoundDown32B(arenaHi);

    OSSetCurrentHeap(OSCreateHeap(arenaLo, arenaHi));
    OSSetArenaLo(arenaLo = arenaHi);
}

/**
 * Offset/Address/Size: 0x0 | 0x8023AD78 | size: 0xB8
 */
void __sys_free(void* p)
{
    if (__OSCurrHeap == -1)
    {
        InitDefaultHeap();
    }

    OSFreeToHeap(__OSCurrHeap, p);
}

/**
 * Offset/Address/Size: 0xB8 | 0x8023AE30 | size: 0xB8
 */
void* __sys_alloc(u32 size)
{
    if (__OSCurrHeap == -1)
    {
        InitDefaultHeap();
    }

    return OSAllocFromHeap(__OSCurrHeap, size);
}
