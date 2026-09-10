#include "PowerPC_EABI_Support/MetroTRK/trk.h"
#include "dolphin/ar.h"

extern u32 _db_stack_addr;

#define EXCEPTIONMASK_ADDR 0x80000044

static u32 lc_base;

static u32 TRK_ISR_OFFSETS[15] = { PPC_SystemReset,
    PPC_MachineCheck,
    PPC_DataStorage,
    PPC_InstructionStorage,
    PPC_ExternalInterrupt,
    PPC_Alignment,
    PPC_Program,
    PPC_FloatingPointUnavaiable,
    PPC_Decrementer,
    PPC_SystemCall,
    PPC_Trace,
    PPC_PerformanceMonitor,
    PPC_InstructionAddressBreakpoint,
    PPC_SystemManagementInterrupt,
    PPC_ThermalManagementInterrupt };

void __TRK_copy_vectors(void);
/**
 * Offset/Address/Size: 0x0 | 0x80005088 | size: 0x2C
 */
__declspec(section ".init") void __TRK_reset(void)
{
    OSResetSystem(FALSE, 0, FALSE);
}

/**
 * Offset/Address/Size: 0x61C | 0x8022991C | size: 0x20
 */
void EnableMetroTRKInterrupts(void)
{
    EnableEXI2Interrupts();
}

/**
 * Offset/Address/Size: 0x5C4 | 0x802298C4 | size: 0x58
 */
u32 TRKTargetTranslate(u32 address)
{
    if (address >= lc_base)
    {

        if ((address < lc_base + 0x4000) && ((gTRKCPUState.Extended1.DBAT3U & 3) != 0))
        {
            return address;
        }
    }
    if ((address >= 0x7E000000) && (address <= 0x80000000))
    {
        return address;
    }
    return address & 0x3FFFFFFF | 0x80000000;
}

extern u8 gTRKInterruptVectorTable[];

void TRK_copy_vector(u32 offset)
{
    void* destPtr = (void*)TRKTargetTranslate(offset);
    TRK_memcpy(destPtr, (void*)(gTRKInterruptVectorTable + offset), 0x100);
    TRK_flush_cache(destPtr, 0x100);
}

/**
 * Offset/Address/Size: 0x498 | 0x80229798 | size: 0x12C
 */
void __TRK_copy_vectors(void)
{
    u32 exceptionMaskAddress = lc_base;
    u32* isrOffsets;
    int vectorIndex;
    u32 exceptionMask;

    if (exceptionMaskAddress <= 0x44 && exceptionMaskAddress + 0x4000 > 0x44
        && gTRKCPUState.Extended1.DBAT3U & 3)
    {
        exceptionMaskAddress = 0x44;
    }
    else
    {
        exceptionMaskAddress = EXCEPTIONMASK_ADDR;
    }

    vectorIndex = 0;
    exceptionMask = *(u32*)exceptionMaskAddress;
    isrOffsets = TRK_ISR_OFFSETS;

    do
    {
        if ((exceptionMask & (1 << vectorIndex)) && vectorIndex != 4)
        {
            TRK_copy_vector(isrOffsets[vectorIndex]);
        }

        vectorIndex++;
    } while (vectorIndex <= 14);
}

/**
 * Offset/Address/Size: 0x44C | 0x8022974C | size: 0x4C
 */
DSError TRKInitializeTarget()
{
    gTRKState.isStopped = TRUE;
    gTRKState.msr = __TRK_get_MSR();
    lc_base = 0xE0000000;
    return DS_NoError;
}

#define __dcbi(a, b) asm { dcbi a, b }

void TRK__read_aram(register int mainMemoryAddress, u32 aramAddress, void* length)
{
    u32 dmaStatus;
    register int offset;
    u16 interruptStatus;
    u32 aramStart;
    u32 dmaSize;

    if ((size_t)aramAddress < 0x4000 || aramAddress + *(u32*)length > 0x8000000)
    {
        return;
    }

    aramStart = aramAddress & ~0x1F;
    dmaSize = *(u32*)length + (aramAddress & 0x1F);
    dmaSize = OSRoundUp32B(dmaSize);

    for (offset = 0; offset < dmaSize; offset += 0x20)
    {
        __dcbi(offset, mainMemoryAddress);
    }

    do
    {
        dmaStatus = ARGetDMAStatus();
    } while (dmaStatus);

    interruptStatus = __ARGetInterruptStatus();
    __ARClearInterrupt();

    ARStartDMA(1, mainMemoryAddress, aramStart, dmaSize);

    while (!__ARGetInterruptStatus())
    {
    }

    if (!interruptStatus)
    {
        __ARClearInterrupt();
    }
}

static void __read_aram_1block(int mainMemoryAddress, u32 aramAddress)
{
    DCBlockInvalidate((void*)mainMemoryAddress);
    __ARClearInterrupt();
    ARStartDMA(1, mainMemoryAddress, aramAddress, 0x20);

    while (!__ARGetInterruptStatus())
    {
    }
}

void TRK__write_aram(int mainMemoryAddress, u32 aramAddress, void* length)
{
    u8 block[32] ALIGN_DECL(32);
    u32 dmaStatus;
    register u32 blockAddress;
    u32 aramStart;
    u32 dmaSize;
    u16 interruptStatus;
    u32 address;
    int offset;
    u32 cacheOffset;

    if ((size_t)aramAddress < 0x4000 || aramAddress + *(u32*)length > 0x8000000)
    {
        return;
    }

    aramStart = aramAddress & ~0x1f;
    offset = 0;
    dmaSize = *(u32*)length + (aramAddress & 0x1f);
    dmaSize = OSRoundUp32B(dmaSize);

    for (cacheOffset = 0; cacheOffset < dmaSize; cacheOffset += 0x20)
    {
        __dcbf((void*)offset, mainMemoryAddress);
        offset += 0x20;
    }

    do
    {
        dmaStatus = ARGetDMAStatus();
    } while (dmaStatus);

    interruptStatus = __ARGetInterruptStatus();
    address = 0x8000000;

    offset = aramAddress & 0x1f;
    if (offset)
    {
        address = aramStart;
        blockAddress = (u32)block;
        __dcbi(r0, blockAddress);
        __ARClearInterrupt();

        ARStartDMA(1, blockAddress, aramStart, 0x20);

        while (!__ARGetInterruptStatus())
        {
        }

        TRK_memcpy((void*)mainMemoryAddress, block, offset);
        __dcbf((void*)mainMemoryAddress, 0);
    }

    aramAddress += *(u32*)length;
    offset = aramAddress & 0x1f;
    if (offset)
    {
        u32 aramEnd = aramAddress & ~0x1F;
        if (aramEnd != address)
        {
            blockAddress = (u32)block;
            __dcbi(r0, blockAddress);
            __ARClearInterrupt();
            ARStartDMA(1, blockAddress, aramEnd, 0x20);

            while (!__ARGetInterruptStatus())
            {
            }
        }
        address = mainMemoryAddress + aramAddress;
        TRK_memcpy((void*)address, (void*)(block + offset), 0x20 - offset);

        __dcbf((void*)address, 0);
    }
    __sync();
    __ARClearInterrupt();
    ARStartDMA(0, mainMemoryAddress, aramStart, dmaSize);
    if (!interruptStatus)
    {
        while (!__ARGetInterruptStatus())
        {
        }

        __ARClearInterrupt();
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80229300 | size: 0x98
 */
asm void InitMetroTRK()
{
    // clang-format off
    nofralloc
	addi r1, r1, -4
	stw r3, 0(r1)
	lis r3, gTRKCPUState@h
	ori r3, r3, gTRKCPUState@l
	stmw r0, ProcessorState_PPC.Default.GPR(r3)
	lwz r4, 0(r1)
	addi r1, r1, 4
	stw r1, ProcessorState_PPC.Default.GPR[1](r3)
	stw r4, ProcessorState_PPC.Default.GPR[3](r3)
	mflr r4
	stw r4, ProcessorState_PPC.Default.LR(r3)
	stw r4, ProcessorState_PPC.Default.PC(r3)
	mfcr r4
	stw r4, ProcessorState_PPC.Default.CR(r3)
	mfmsr r4
	ori r3, r4, (1 << (31 - 16))
	xori r3, r3, (1 << (31 - 16))
	mtmsr r3
	mtsrr1 r4
	bl TRKSaveExtended1Block
	lis r3, gTRKCPUState@h
	ori r3, r3, gTRKCPUState@l
	lmw r0, ProcessorState_PPC.Default.GPR(r3)
	li r0, 0
	mtspr  0x3f2, r0
	mtspr  0x3f5, r0
	lis r1, _db_stack_addr@h
	ori r1, r1, _db_stack_addr@l
	mr r3, r5
	bl InitMetroTRKCommTable
	cmpwi r3, 1
	bne initCommTableSuccess
	lwz r4, ProcessorState_PPC.Default.LR(r3)
	mtlr r4
	lmw r0, ProcessorState_PPC.Default.GPR(r3)
	blr
initCommTableSuccess:
	b TRK_main
	blr
    // clang-format on
}

/**
 * Offset/Address/Size: 0x98 | 0x80229398 | size: 0x94
 */
asm void InitMetroTRK_BBA()
{
    // clang-format off
    nofralloc
	addi r1, r1, -4
	stw r3, 0(r1)
	lis r3, gTRKCPUState@h
	ori r3, r3, gTRKCPUState@l
	stmw r0, ProcessorState_PPC.Default.GPR(r3)
	lwz r4, 0(r1)
	addi r1, r1, 4
	stw r1, ProcessorState_PPC.Default.GPR[1](r3)
	stw r4, ProcessorState_PPC.Default.GPR[3](r3)
	mflr r4
	stw r4, ProcessorState_PPC.Default.LR(r3)
	stw r4, ProcessorState_PPC.Default.PC(r3)
	mfcr r4
	stw r4, ProcessorState_PPC.Default.CR(r3)
	mfmsr r4
	ori r3, r4, (1 << (31 - 16))
	mtmsr r3
	mtsrr1 r4
	bl TRKSaveExtended1Block
	lis r3, gTRKCPUState@h
	ori r3, r3, gTRKCPUState@l
	lmw r0, ProcessorState_PPC.Default.GPR(r3)
	li r0, 0
	mtspr  0x3f2, r0
	mtspr 0x3f5, r0
	lis r1, _db_stack_addr@h
	ori r1, r1, _db_stack_addr@l
	li r3, 2
	bl InitMetroTRKCommTable
	cmpwi r3, 1
	bne initCommTableSuccess
	lwz r4, ProcessorState_PPC.Default.LR(r3)
	mtlr r4
	lmw r0, ProcessorState_PPC.Default.GPR(r3)
	blr
initCommTableSuccess:
	b TRK_main
	blr
    // clang-format on
}
