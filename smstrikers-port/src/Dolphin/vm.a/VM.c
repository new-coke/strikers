#include <stddef.h>
#include <stdint.h>

#include <dolphin/ar.h>
#include <dolphin/os.h>
#include <dolphin/types.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/vm/VM.h>

typedef void (*VMLogStatsCallback)(u32 faultAddr, u32 mainAddr, u32 pageIndex, u32 elapsed, u32 wroteBack);
typedef void (*VMSwapPageInCallback)(u32 virtualAddr);

static u32 g_vmInitialized;
static VMLogStatsCallback g_cbLogStats;
static u32 g_vmNumPagesInMRAM;
static u32 g_vmSizeVMARAM;
static u32 g_vmBaseVMMainMemory;
static u32 g_vmSizeVMMainMemory;

static u32 g_vmBaseVMARAM = 0x4000;

void VMBASEInit(VMSwapPageInCallback cb);
void VMBASESetPageTableEntry(u32 virtualAddr, u32 physicalAddr, u32 mramPage);
void VMBASEClearPageTableEntry(u32 virtualAddr, u32 mramPage);
u32 VMBASEGetVirtualAddrFromPageInMRAM(u32 mramPage);
BOOL VMBASEIsPageDirty(u32 virtualAddr);

void VMBASEQuit(void);
void VMBASESetPageDirty(u32 virtualAddr, BOOL dirty);

BOOL VMStoreOnePage(void);
void VMStoreAllPages(void);

void __VMAllocVirtualToARAMLUT(void);
void __VMAllocARAMToVirtualLUT(void);
void __VMAllocMRAMSwapSpace(void);
u32 __VMGetPhysicalAddrOfPageInMRAM(u32 mramPage);
u32 __VMGetMRAMBase(void);
void __VMSwapPageIn(u32 faultAddr);
void __VMSwapPageOut(u32 virtualPage);
u32 __VMGetPageToReplace(void);
void __VMSetARAMPageAsDirty(u32 virtualPage);
u32 __VMTranslateVMPageToARAMPage(u32 virtualPage);
BOOL __VMIsARAMPageDirty(u32 virtualPage);
BOOL __VMDoesMappingExist(u32 virtualPage);
void __VMMappingErrorAlert(u32 virtualPage);

/**
 * Offset/Address/Size: 0x0 | 0x8025F240 | size: 0x9C
 */
void VMInit(uintptr_t baseAddr, size_t initialCommitSize, uintptr_t limitAddr)
{
    BOOL oldInterrupts;

    if ((s32)g_vmInitialized != 0)
    {
        return;
    }

    oldInterrupts = OSDisableInterrupts();
    g_vmInitialized = 1;

    g_vmBaseVMARAM = (u32)initialCommitSize;
    g_vmSizeVMARAM = (u32)limitAddr;
    g_vmSizeVMMainMemory = (u32)baseAddr;
    g_vmNumPagesInMRAM = (u32)baseAddr >> 12;

    VMBASEInit(__VMSwapPageIn);
    __VMAllocVirtualToARAMLUT();
    __VMAllocARAMToVirtualLUT();
    __VMAllocMRAMSwapSpace();

    OSRestoreInterrupts(oldInterrupts);
}

BOOL VMIsInitialized(void)
{
    return g_vmInitialized;
}

void VMQuit(void)
{
    BOOL oldInterrupts;

    if ((s32)g_vmInitialized == 0)
    {
        return;
    }

    oldInterrupts = OSDisableInterrupts();
    VMStoreAllPages();
    VMBASEQuit();
    OSSetArenaLo((void*)g_vmBaseVMMainMemory);
    g_vmInitialized = 0;
    g_vmSizeVMARAM = 0;
    g_vmSizeVMMainMemory = 0;
    g_vmNumPagesInMRAM = 0;
    g_vmBaseVMMainMemory = 0;
    OSRestoreInterrupts(oldInterrupts);
}

void VMResizeARAM(u32 aramBase, u32 aramSize)
{
    g_vmBaseVMARAM = aramBase;
    g_vmSizeVMARAM = aramSize;
    __VMAllocARAMToVirtualLUT();
}

/**
 * Offset/Address/Size: 0x9C | 0x8025F2DC | size: 0x8
 */
void VMSetLogStatsCallback(VMLogStatsCallback cb)
{
    g_cbLogStats = cb;
}

u32 __VMGetPhysicalAddrOfPageInMRAM(u32 mramPage)
{
    return g_vmBaseVMMainMemory + (mramPage << 12);
}

/**
 * Offset/Address/Size: 0xA4 | 0x8025F2E4 | size: 0x8
 */
u32 __VMGetNumPagesInMRAM(void)
{
    return g_vmNumPagesInMRAM;
}

/**
 * Offset/Address/Size: 0xAC | 0x8025F2EC | size: 0x8
 */
u32 VMGetARAMSize(void)
{
    return g_vmSizeVMARAM;
}

/**
 * Offset/Address/Size: 0xB4 | 0x8025F2F4 | size: 0x8
 */
u32 VMGetARAMBase(void)
{
    return g_vmBaseVMARAM;
}

u32 __VMGetMRAMBase(void)
{
    return g_vmBaseVMMainMemory;
}

/**
 * Offset/Address/Size: 0xBC | 0x8025F2FC | size: 0x30
 */
void __VMAllocMRAMSwapSpace(void)
{
    g_vmBaseVMMainMemory = (u32)OSGetArenaLo();
    OSSetArenaLo((void*)(g_vmBaseVMMainMemory + g_vmSizeVMMainMemory));
}

/**
 * Offset/Address/Size: 0xEC | 0x8025F32C | size: 0x1FC
 */
void __VMSwapPageIn(u32 faultAddr)
{
    u32 startTicks;
    u32 virtualPage;
    u32 mainAddr;
    u32 wroteBack;
    u32 pageIndex;
    s32 hadARInterrupt;
    BOOL oldInterrupts;
    u32 previousVirtualPage;

    startTicks = (u32)OSTicksToMicroseconds(OSGetTime());
    virtualPage = faultAddr & ~0xFFF;
    wroteBack = 0;

    pageIndex = __VMGetPageToReplace();
    mainAddr = g_vmBaseVMMainMemory + (pageIndex << 12);
    previousVirtualPage = VMBASEGetVirtualAddrFromPageInMRAM(pageIndex);

    oldInterrupts = OSDisableInterrupts();
    while (ARGetDMAStatus() != 0)
    {
    }
    hadARInterrupt = __ARGetInterruptStatus();

    if (previousVirtualPage != 0)
    {
        if (VMBASEIsPageDirty(previousVirtualPage))
        {
            __VMSetARAMPageAsDirty(previousVirtualPage);
            wroteBack = 1;
            DCFlushRange((void*)mainAddr, 0x1000);
            ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, mainAddr, __VMTranslateVMPageToARAMPage(previousVirtualPage), 0x1000);
            while (ARGetDMAStatus() != 0)
            {
            }
        }
        VMBASEClearPageTableEntry(previousVirtualPage, pageIndex);
    }

    if (__VMIsARAMPageDirty(virtualPage))
    {
        ARStartDMA(ARAM_DIR_ARAM_TO_MRAM, mainAddr, __VMTranslateVMPageToARAMPage(virtualPage), 0x1000);
        while (ARGetDMAStatus() != 0)
        {
        }
        DCInvalidateRange((void*)mainAddr, 0x1000);
        ICInvalidateRange((void*)mainAddr, 0x1000);
    }
    else if (!__VMDoesMappingExist(virtualPage))
    {
        __VMMappingErrorAlert(virtualPage);
    }

    if (hadARInterrupt == 0)
    {
        __ARClearInterrupt();
    }

    VMBASESetPageTableEntry(virtualPage, mainAddr, pageIndex);
    OSRestoreInterrupts(oldInterrupts);

    if (g_cbLogStats != NULL)
    {
        g_cbLogStats(faultAddr, mainAddr, pageIndex, (u32)OSTicksToMicroseconds(OSGetTime()) - startTicks, wroteBack);
    }
}

void VMStoreAllPages(void)
{
    while (VMStoreOnePage())
    {
    }
}

BOOL VMStoreOnePage(void)
{
    static u32 nextPageToCheck;
    u32 virtualPage;
    u32 mainAddr;
    BOOL oldInterrupts;
    s32 hadARInterrupt;

    oldInterrupts = OSDisableInterrupts();

    while (nextPageToCheck < g_vmNumPagesInMRAM)
    {
        virtualPage = VMBASEGetVirtualAddrFromPageInMRAM(nextPageToCheck);

        if (virtualPage != 0 && VMBASEIsPageDirty(virtualPage))
        {
            mainAddr = g_vmBaseVMMainMemory + (nextPageToCheck << 12);
            while (ARGetDMAStatus() != 0)
            {
            }
            hadARInterrupt = __ARGetInterruptStatus();
            __VMSetARAMPageAsDirty(virtualPage);
            DCFlushRange((void*)mainAddr, 0x1000);
            ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, mainAddr, __VMTranslateVMPageToARAMPage(virtualPage), 0x1000);
            while (ARGetDMAStatus() != 0)
            {
            }
            if (hadARInterrupt == 0)
            {
                __ARClearInterrupt();
            }
            VMBASESetPageDirty(virtualPage, FALSE);
            nextPageToCheck++;
            OSRestoreInterrupts(oldInterrupts);
            return TRUE;
        }
        nextPageToCheck++;
    }

    nextPageToCheck = 0;
    OSRestoreInterrupts(oldInterrupts);
    return FALSE;
}
