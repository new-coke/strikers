#include "NL/nlMemory.h"
#include <stdlib.h>
#include "NL/MemAlloc.h"

#include <types.h>

#include "dolphin/os.h"
#include "dolphin/pad.h"
#include "dolphin/dvd.h"
#include "dolphin/vm/VM.h"
#include "dolphin/vi/vifuncs.h"

static u8 s_MemoryInitialized = 0;

MemoryAllocator StandardAllocator;
MemoryAllocator VirtualAllocator;

/**
 * Offset/Address/Size: 0x0 | 0x801D1EE4 | size: 0x40
 */
void nlFree(void* ptr)
{
    if (((uintptr_t)ptr & 0x80000000) == 0)
    {
        VirtualAllocator.Free(ptr);
    }
    else
    {
        StandardAllocator.Free(ptr);
    }
}

/**
 * Offset/Address/Size: 0x40 | 0x801D1F24 | size: 0x64
 */
void* nlMalloc(size_t size, unsigned int alignment, bool atEnd)
{
    if (s_MemoryInitialized == 0)
    {
        nlInitMemory();
    }
    return StandardAllocator.Allocate(size, alignment, atEnd);
}

/**
 * Offset/Address/Size: 0xA4 | 0x801D1F88 | size: 0x4C
 */
void* nlMalloc(size_t size)
{
    if (s_MemoryInitialized == 0)
    {
        nlInitMemory();
    }
    return StandardAllocator.Allocate(size, 8, false);
}

/**
 * Offset/Address/Size: 0xF0 | 0x801D1FD4 | size: 0x4C
 */
void* operator new(size_t size)
{
    // PORT: the host heap, not nlMalloc, see the note in tools/vendor.py.
    void* p = malloc(size ? size : 1);
    if (p == NULL)
    {
        OSReport("nlMemory: out of memory allocating %lu bytes\n", size);
        abort();
    }
    return p;
}

/**
 * Offset/Address/Size: 0x13C | 0x801D2020 | size: 0x40
 */
void operator delete[](void* ptr)
{
    nlFree(ptr);
}

/**
 * Offset/Address/Size: 0x17C | 0x801D2060 | size: 0x40
 */
void operator delete(void* ptr)
{
    nlFree(ptr);
}

/**
 * Offset/Address/Size: 0x1BC | 0x801D20A0 | size: 0x24
 */
unsigned int nlVirtualTotalFree()
{
    return VirtualAllocator.TotalFreeMemory();
}

/**
 * Offset/Address/Size: 0x1E0 | 0x801D20C4 | size: 0x24
 */
unsigned int nlVirtualLargestBlock()
{
    return VirtualAllocator.LargestFreeBlock();
}

/**
 * Offset/Address/Size: 0x204 | 0x801D20E8 | size: 0x28
 */
void nlVirtualFree(void* ptr)
{
    VirtualAllocator.Free(ptr);
}

/**
 * Offset/Address/Size: 0x22C | 0x801D2110 | size: 0x30
 */
void* nlVirtualAlloc(size_t size, bool bZero)
{
    return VirtualAllocator.Allocate(size, 0x20, bZero);
}

/**
 * Offset/Address/Size: 0x25C | 0x801D2140 | size: 0x1B8
 */
extern "C" void* VMPortGetWindow(size_t*);  // src/platform/vm.c

void nlInitMemory()
{
    if (s_MemoryInitialized == 0)
    {
        s_MemoryInitialized = 1;
        VMInit(0x100000, 0x700000, 0x900000);
        VMAlloc(0x7E000000, 0x900000);
        DVDInit();
        VIInit();
        PADInit();

        void* arenaLo = OSGetArenaLo();
        void* arenaHi = OSGetArenaHi();
        arenaLo = OSInitAlloc(arenaLo, arenaHi, 1);
        OSSetArenaLo(arenaLo);

        uintptr_t alignedLo = ((uintptr_t)arenaLo + 0x1F) & ~0x1F;
        arenaLo = (void*)((uintptr_t)arenaHi & ~0x1F);
        uintptr_t heapSize = (uintptr_t)arenaLo - alignedLo;

        s32 heap = OSCreateHeap((void*)alignedLo, arenaLo);
        OSSetCurrentHeap(heap);
        OSSetArenaLo(arenaLo);

        void* ptr = OSAllocFromHeap(__OSCurrHeap, heapSize - 0x40000);
        u32 i;
        for (i = 0; i < heapSize - 0x40000; i++)
        {
            ((s8*)ptr)[i] = -0x33;
        }

        StandardAllocator.Initialize(ptr, heapSize - 0x40000);
        // PORT: 0x7E000000 is the console's MMU window; use the host allocation that src/platform/vm.c made for VMAlloc.
        {
            size_t vmSize = 0;
            void* vmBase = VMPortGetWindow(&vmSize);
            VirtualAllocator.Initialize(vmBase, (u32)vmSize);
        }
        OSReport("After nlInitMemory\n");
        OSReport("Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
        OSReport("Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());
    }
}
