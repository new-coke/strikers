// Replacement for the Dolphin VM (virtual memory) library.

#include <stdint.h>
#include <stdlib.h>

#include "dolphin/types.h"

extern void* port_region_reserve(size_t size, size_t align);  // src/platform/memalloc.cpp

typedef void (*VMLogStatsCallback)(unsigned long faultAddr,
                                   unsigned long mainAddr,
                                   unsigned long pageIndex,
                                   unsigned long elapsed,
                                   int wroteBack);

static void* s_window;
static size_t s_window_size;
static VMLogStatsCallback s_log_cb;

// nlMemory.cpp asks for 9 MB, what the console reserved at 0x7E000000; but there the window was
// demand-paged, the MMU evicting to ARAM and re-reading from disc as the match moved. Nothing here
// does.
#define PORT_VM_MIN_SIZE (128u * 1024u * 1024u)

void VMInit(uintptr_t baseAddr, size_t initialCommitSize, uintptr_t limitAddr)
{
    // baseAddr/limitAddr describe the console's MEM1 paging window.
    (void)baseAddr;
    size_t want = initialCommitSize > limitAddr ? initialCommitSize
                                                : (size_t)limitAddr;
    {
        const size_t least = PORT_VM_MIN_SIZE;
        if (want < least)
            want = least;
    }
    if (want > s_window_size)
    {
        s_window = NULL;
        s_window_size = 0;
        // From the port's own region: the game asks "is this pointer mine?" by range-testing that
        // region, and matrices handed out of this window have to answer yes.
        void* p = port_region_reserve(want, 32);
        if (p != NULL)
        {
            s_window = p;
            s_window_size = want;
        }
    }
}

void VMAlloc(uintptr_t address, size_t size)
{
    // `address` is the console-side window base (0x7E000000).
    (void)address;
    if (size > s_window_size)
        VMInit(0, size, 0);
}

void VMSetLogStatsCallback(VMLogStatsCallback cb)
{
    // Kept so the game can install its handler; never invoked, because nothing here ever takes a
    // page fault.
    s_log_cb = cb;
}

// Exposed for the port's own allocator shim to sit on top of.
void* VMPortGetWindow(size_t* size)
{
    if (size)
        *size = s_window_size;
    return s_window;
}
