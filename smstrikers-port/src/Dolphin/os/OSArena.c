#include <dolphin.h>
#include <dolphin/os.h>

#define ROUND(n, a) (((u32)(n) + (a) - 1) & ~((a) - 1))
#define TRUNC(n, a) (((u32)(n)) & ~((a) - 1))

static void* __OSArenaHi;
static void* __OSArenaLo = (void*)-1;

/**
 * Offset/Address/Size: 0x0 | 0x802546CC | size: 0x8
 */
void* OSGetArenaHi(void)
{
    ASSERTMSGLINE(55, (u32)__OSArenaLo != -1, "OSGetArenaHi(): OSInit() must be called in advance.");
    ASSERTMSGLINE(57, (u32)__OSArenaLo <= (u32)__OSArenaHi, "OSGetArenaHi(): invalid arena (hi < lo).");
    return __OSArenaHi;
}

/**
 * Offset/Address/Size: 0x8 | 0x802546D4 | size: 0x8
 */
void* OSGetArenaLo(void)
{
    ASSERTMSGLINE(73, (u32)__OSArenaLo != -1, "OSGetArenaLo(): OSInit() must be called in advance.");
    ASSERTMSGLINE(75, (u32)__OSArenaLo <= (u32)__OSArenaHi, "OSGetArenaLo(): invalid arena (hi < lo).");
    return __OSArenaLo;
}

/**
 * Offset/Address/Size: 0x10 | 0x802546DC | size: 0x8
 */
void OSSetArenaHi(void* newHi)
{
    __OSArenaHi = newHi;
}

/**
 * Offset/Address/Size: 0x18 | 0x802546E4 | size: 0x8
 */
void OSSetArenaLo(void* newLo)
{
    __OSArenaLo = newLo;
}

/**
 * Offset/Address/Size: 0x20 | 0x802546EC | size: 0x2C
 */
void* OSAllocFromArenaLo(u32 size, u32 align)
{
    void* ptr;
    u8* arenaLo;

    ptr = OSGetArenaLo();
    arenaLo = ptr = (void*)ROUND(ptr, align);
    arenaLo += size;
    arenaLo = (u8*)ROUND(arenaLo, align);
    OSSetArenaLo(arenaLo);
    return ptr;
}

void* OSAllocFromArenaHi(u32 size, u32 align)
{
    void* ptr;
    u8* arenaHi;

    arenaHi = OSGetArenaHi();
    arenaHi = (u8*)TRUNC(arenaHi, align);
    arenaHi -= size;
    arenaHi = ptr = (void*)TRUNC(arenaHi, align);
    OSSetArenaHi(arenaHi);
    return ptr;
}
