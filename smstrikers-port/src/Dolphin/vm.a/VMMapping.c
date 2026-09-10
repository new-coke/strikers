#include <stdio.h>

#include <dolphin/PPCArch.h>
#include <dolphin/types.h>

static u32* g_baseARAMtoVM = 0;
static u32* g_baseVMtoARAM = 0;
static u32 g_totalAllocatedVM = 0;

void* OSGetArenaLo(void);
void OSSetArenaLo(void* newLo);
u32 VMGetARAMBase(void);
u32 VMGetARAMSize(void);
void __VMMappingErrorAlert(u32 virtualPage);

static inline u32 VMToARAMOffset(u32 virtualAddr)
{
    return (virtualAddr >> 10) & 0x1FFC;
}

#pragma optimize_for_size off
/**
 * Offset/Address/Size: 0x0 | 0x8025F83C | size: 0x100
 */
BOOL VMAlloc(u32 address, u32 size)
{
    static u32 g_nextARAMPageToCheck;
    u32 i;
    u32 startAramPage = VMGetARAMBase() >> 12;
    u32 endAramPage = startAramPage + (VMGetARAMSize() >> 12);

    if (g_nextARAMPageToCheck < startAramPage)
    {
        g_nextARAMPageToCheck = startAramPage;
    }

    if ((g_totalAllocatedVM + size) > VMGetARAMSize())
    {
        return FALSE;
    }

    for (i = 0; i < size; i += 0x1000)
    {
        u32 virtualPage = address + i;

        while (TRUE)
        {
            u32 next = g_nextARAMPageToCheck + 1;
            g_nextARAMPageToCheck = next;
            if (next >= endAramPage)
            {
                g_nextARAMPageToCheck = startAramPage;
            }

            if (g_baseARAMtoVM[g_nextARAMPageToCheck] == 0)
            {
                break;
            }
        }

        g_baseARAMtoVM[g_nextARAMPageToCheck] = virtualPage;
        g_baseVMtoARAM[((virtualPage >> 10) & 0x7FFC) >> 2] = g_nextARAMPageToCheck << 12;

        g_totalAllocatedVM += 0x1000;
    }

    return TRUE;
}
#pragma optimize_for_size reset

/**
 * Offset/Address/Size: 0x100 | 0x8025F93C | size: 0x40
 */
u32 __VMTranslateVMPageToARAMPage(u32 virtualPage)
{
    u32 aramPage = g_baseVMtoARAM[(virtualPage >> 12) & 0x1FFF] & 0x7FFFFFFF;

    if (aramPage != 0)
    {
        return aramPage;
    }

    __VMMappingErrorAlert(virtualPage);
    return 0;
}

/**
 * Offset/Address/Size: 0x140 | 0x8025F97C | size: 0x20
 *
 * `#pragma optimize_for_size off` selects the neg/or/srwi nonzero-test
 * sequence; under the unit's -O4,s default MWCC emits the shorter
 * subic/subfe form, which diverges from the target.
 */
#pragma optimize_for_size off
BOOL __VMDoesMappingExist(u32 virtualPage)
{
    return (g_baseVMtoARAM[(virtualPage >> 12) & 0x1FFF] & 0x7FFFFFFF) != 0;
}
#pragma optimize_for_size reset

/**
 * Offset/Address/Size: 0x160 | 0x8025F99C | size: 0x38
 */
void __VMMappingErrorAlert(u32 virtualPage)
{
    char msg[0x408];
    sprintf(msg, "Virtual address (%x) has not been allocated. Call VMAlloc on virtual address ranges before using them.", virtualPage);
    PPCHalt();
}

/**
 * Offset/Address/Size: 0x198 | 0x8025F9D4 | size: 0x18
 */
void __VMSetARAMPageAsDirty(u32 virtualPage)
{
    g_baseVMtoARAM[(virtualPage >> 12) & 0x1FFF] |= 0x80000000;
}

/**
 * Offset/Address/Size: 0x1B0 | 0x8025F9EC | size: 0x14
 */
BOOL __VMIsARAMPageDirty(u32 virtualPage)
{
    return g_baseVMtoARAM[(virtualPage >> 12) & 0x1FFF] >> 31;
}

/**
 * Offset/Address/Size: 0x1C4 | 0x8025FA00 | size: 0xA8
 *
 * The LUT clear is a byte-offset loop that writes 0 straight into the global
 * base pointer (`g_baseVMtoARAM[offset]`), re-reading the global on every store.
 * `#pragma optimize_for_size off` (the -O4,s default is restored below) lets MWCC
 * unroll it 8x AND hoist the fill 0 into a held register (r4) with the byte
 * offset in r5 -- matching the target. Under -O4,s the fill is rematerialized
 * inside the loop instead. See the sibling VMBase.c invalidate helpers, which
 * use the same idiom under speed optimization.
 *
 * The `u8* lo` temp is deliberate: storing the global from `lo` but computing
 * the OSSetArenaLo argument by reading `g_baseVMtoARAM` back is what makes MWCC
 * hold the arena base in r4 (`mr r4, r3`) for the 0x8000 bump.
 */
#pragma optimize_for_size off
void __VMAllocVirtualToARAMLUT(void)
{
    u32 offset;
    u8* lo = (u8*)OSGetArenaLo();

    g_baseVMtoARAM = (u32*)lo;
    OSSetArenaLo((void*)((u8*)g_baseVMtoARAM + 0x8000));

    for (offset = 0; offset < 0x8000; offset += 4)
    {
        *(u32*)((u8*)g_baseVMtoARAM + offset) = 0;
    }
}

/**
 * Offset/Address/Size: 0x26C | 0x8025FAA8 | size: 0xA0
 *
 * The 0x4000 arena bump is a single addi, so the prologue keeps the base in r3
 * (no `mr`) unlike the 0x8000 case above.
 */
void __VMAllocARAMToVirtualLUT(void)
{
    u32 offset;

    g_baseARAMtoVM = (u32*)OSGetArenaLo();
    OSSetArenaLo((void*)((u8*)g_baseARAMtoVM + 0x4000));

    for (offset = 0; offset < 0x4000; offset += 4)
    {
        *(u32*)((u8*)g_baseARAMtoVM + offset) = 0;
    }
}
#pragma optimize_for_size reset
