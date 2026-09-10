#include "NL/nlSlotPool.h"
#include "NL/nlList.h"
#include "NL/nlMemory.h"

/**
 * Offset/Address/Size: 0x0 | 0x801D23F8 | size: 0xAC
 */
void SlotPoolBase::BaseAddNewBlock(SlotPoolBase* slotPool, unsigned int slotSize)
{
    u32 totalSlotMemory;
    u32 primarySlotCount;
    u32 numSlots;
    void* memoryBlock;

    primarySlotCount = slotPool->m_Delta;
    if ((primarySlotCount != 0) || (slotPool->m_BlockList == NULL))
    {
        numSlots = primarySlotCount;
        if (slotPool->m_BlockList == NULL)
        {
            numSlots = slotPool->m_Initial;
        }

        totalSlotMemory = slotSize * numSlots;
        // PORT: the trailing link is a pointer, not four bytes.
        memoryBlock = slotPool->m_AllocFn(totalSlotMemory + sizeof(SlotPoolBlock));
        nlListAddStart<SlotPoolBlock>(&slotPool->m_BlockList, (SlotPoolBlock*)((u8*)memoryBlock + totalSlotMemory), 0);

        for (u32 i = 0; i < numSlots; i++)
        {
            nlListAddStart<SlotPoolEntry>(&slotPool->m_FreeList, (SlotPoolEntry*)memoryBlock, 0);
            memoryBlock = (u8*)memoryBlock + slotSize;
        }
    }
}

/**
 * Offset/Address/Size: 0xAC | 0x801D24A4 | size: 0x90
 */
void SlotPoolBase::BaseFreeBlocks(SlotPoolBase* slotPool, unsigned int slotSize)
{
    SlotPoolBlock* nextBlock;
    SlotPoolBlock* currentBlock;
    s32 blockOffset;

    currentBlock = slotPool->m_BlockList;
    blockOffset = slotSize * slotPool->m_Delta;
    while (currentBlock != NULL)
    {
        nextBlock = currentBlock->next;
        if (nextBlock == NULL)
        {
            blockOffset = slotSize * slotPool->m_Initial;
        }
        slotPool->m_FreeFn((u8*)currentBlock - blockOffset);
        currentBlock = nextBlock;
    }
    slotPool->m_BlockList = NULL;
    slotPool->m_FreeList = NULL;
}

/**
 * Offset/Address/Size: 0x13C | 0x801D2534 | size: 0x3C
 */
SlotPoolBase::~SlotPoolBase()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x178 | 0x801D2570 | size: 0x30
 */
SlotPoolBase::SlotPoolBase()
{
    m_BlockList = 0;
    m_FreeList = 0;
    m_AllocFn = DefaultSlotPoolAllocator;
    m_FreeFn = DefaultSlotPoolFree;
    m_Initial = 0;
    m_Delta = 0;
}

/**
 * Offset/Address/Size: 0x1A8 | 0x801D25A0 | size: 0x20
 */
void DefaultSlotPoolFree(void* data)
{
    nlFree(data);
}

/**
 * Offset/Address/Size: 0x1C8 | 0x801D25C0 | size: 0x28
 */
void* DefaultSlotPoolAllocator(unsigned long size)
{
    return nlMalloc(size, 8, false);
}

//  */
// // nlListAddStart<SlotPoolEntry>(SlotPoolEntry**, SlotPoolEntry*, SlotPoolEntry**)
// void nlListAddStart(SlotPoolEntry**, SlotPoolEntry*, SlotPoolEntry**)
// {
// }

//  */
// // nlListAddStart<SlotPoolBlock>(SlotPoolBlock**, SlotPoolBlock*, SlotPoolBlock**)
// void nlListAddStart(SlotPoolBlock**, SlotPoolBlock*, SlotPoolBlock**)
// {
// }
