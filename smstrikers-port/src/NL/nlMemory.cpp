#include "port/native_boot_screen.hpp"
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
    // PORT: malloc, not nlMalloc: a system library may free this without calling the game's operator delete.
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
extern "C" void* port_region_reserve(size_t, size_t);  // src/platform/memalloc.cpp
extern "C" void port_region_stats(size_t*, size_t*);   // src/platform/memalloc.cpp

void nlInitMemory()
{
    if (s_MemoryInitialized == 0)
    {
        s_MemoryInitialized = 1;
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
        // Static game constructors can mount the archive before main(). This
        // runs after the SDK thread runtime is ready and before that disk I/O.
        (void)port::startup::begin();
#endif
        VMInit(0x100000, 0x700000, 0x900000);
        VMAlloc(0x7E000000, 0x900000);
        DVDInit();
        VIInit();
        PADInit();

#if defined(STRIKERS_VITA)
        // The Vita port owns one large game-memory slab. VMInit has already
        // carved its paging window from the front; give the rest directly to
        // StandardAllocator instead of allocating a second ~48 MiB newlib
        // heap block just to hand it straight back to the game's allocator.
        size_t regionUsed = 0;
        size_t regionTotal = 0;
        port_region_stats(&regionUsed, &regionTotal);
        const size_t guard = 0x40000;
        const size_t standardSize =
            regionTotal > regionUsed + guard ? regionTotal - regionUsed - guard : 0;
        void* standardBase =
            standardSize != 0 ? port_region_reserve(standardSize, 32) : NULL;

        size_t vmSize = 0;
        void* vmBase = VMPortGetWindow(&vmSize);
        if (standardBase == NULL || standardSize == 0 || vmBase == NULL || vmSize == 0)
        {
            OSReport("[vita] nlInitMemory allocation failed: std=%p/%u vm=%p/%u region=%u/%u\n",
                     standardBase, (unsigned int)standardSize,
                     vmBase, (unsigned int)vmSize,
                     (unsigned int)regionUsed, (unsigned int)regionTotal);
            abort();
        }

        StandardAllocator.Initialize(standardBase, (u32)standardSize);
        VirtualAllocator.Initialize(vmBase, (u32)vmSize);
#else
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
#endif
        OSReport("After nlInitMemory\n");
        OSReport("Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
        OSReport("Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());
    }
}
