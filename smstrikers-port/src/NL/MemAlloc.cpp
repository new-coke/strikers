// Include order is load-bearing; do not sort these lines. MWCC emits one .text
// section per code-contributing header, ordered by parse position, and this TU
// reproduces the original's four blocks (config/G4QE01/splits.txt):
//
//   0x664  MemAlloc.cpp   the seven MemoryAllocator/Callback functions
//   0x054  nlWare.h       weak nlPrintf; discarded at link (AnimInventory.cpp
//                         owns the surviving copy at 0x8000749C), which is why
//                         the split target records this block as zero-length
//   0x148  nlDLRing.h     nlWalkDLRing<> instantiations + nlDLRing* helpers
//   0x0c0  nlRing.h       nlWalkRing<> instantiations
//
// Parsing nlDLRing.h after MemAlloc.h splits its 0x148 block in two and reverses
// the halves, which reorders the .text contribution and breaks the DOL SHA1 --
// while objdiff and the per-unit report both still report 100%.
#include "NL/nlWare.h"
#include "NL/nlDebug.h"
#include "NL/nlDLRing.h"
#include "NL/MemAlloc.h"

// Built with `-inline deferred`: MWCC emits .text in REVERSE source order, so the
// functions below are declared last-to-first relative to their addresses. Parse-time
// .data (the ptmf constants) still follows source order, which is what pins
// TotalFreeMemCallback's ptmf ahead of LargestFreeBlockCallback's.

/**
 * Offset/Address/Size: 0x534 | 0x801CDC80 | size: 0x130
 */
void MemoryAllocator::Free(void* p)
{
    FreeBlockList* block;
    MemoryAllocator* self;
    FreeBlockList* start;
    FreeBlockList* iter;
    FreeBlockList* next;
    FreeBlockList* prev;
    s32 size;
    s32 header;
    s32 offset;

    if (p == NULL)
    {
        return;
    }

    block = (FreeBlockList*)((char*)p - 4);
    self = this;
    header = *(u32*)block;
    size = header & 0x3FFFFFFF;
    size += 3;
    size &= 0xFFFFFFFC;
    if (header & 0x40000000)
    {
        size += *(u32*)((char*)p + size);
    }

    size += 4;
    if (header & 0x80000000)
    {
        offset = *(u32*)((char*)block - 4);
        block = (FreeBlockList*)((char*)block - offset);
        size += offset;
    }

    block->m_size = size;
    start = nlDLRingGetStart<FreeBlockList>(self->m_free_block_list);
    if ((start > block) || (start == NULL))
    {
        nlDLRingAddStart<FreeBlockList>(&self->m_free_block_list, block);
    }
    else
    {
        iter = start->m_next;
        while (iter != start)
        {
            if (iter > block)
            {
                break;
            }
            iter = iter->m_next;
        }
        nlDLRingInsert<FreeBlockList>(&self->m_free_block_list, iter->m_prev, block);
    }

    next = block->m_next;
    if (next > block)
    {
        size = block->m_size;
        if (((char*)block + size) == (char*)next)
        {
            block->m_size = size + next->m_size;
            nlDLRingRemove<FreeBlockList>(&self->m_free_block_list, next);
        }
    }

    prev = block->m_prev;
    if (prev < block)
    {
        size = prev->m_size;
        if (((char*)prev + size) == (char*)block)
        {
            prev->m_size = size + block->m_size;
            nlDLRingRemove<FreeBlockList>(&self->m_free_block_list, block);
        }
    }
}

/**
 * Offset/Address/Size: 0x1D8 | 0x801CD924 | size: 0x35C
 */
void* MemoryAllocator::Allocate(unsigned long size, unsigned int alignment, bool fromEnd)
{
    void* result;
    uintptr_t offset;   // holds an address in the fromEnd branch
    unsigned int requestSize = size;

    if (alignment < 4)
        alignment = 4;
    if (requestSize < sizeof(FreeBlockList))
        requestSize = sizeof(FreeBlockList);

    if (fromEnd)
    {
        FreeBlockList* cur;
        FreeBlockList* end = nlDLRingGetEnd<FreeBlockList>(m_free_block_list);
        cur = end;
        u32 alignedSize = (requestSize + 3) & ~3u;
        uintptr_t alignMask = ~(uintptr_t)(alignment - 1);
        u32 savedSize = requestSize;
        u32 blockSize;

        do
        {
            blockSize = cur->m_size;
            if (blockSize > alignedSize)
            {
                uintptr_t endAddr = (uintptr_t)cur + blockSize;
                uintptr_t delta = endAddr - alignedSize;
                offset = delta & alignMask;
                requestSize = (endAddr - offset) + 4;
                if (requestSize <= blockSize)
                    break;
            }
            cur = cur->m_next;
            if (cur == end)
            {
                nlPrintf("Total Free Memory: %d\n", TotalFreeMemory());
                nlPrintf("Largest Free Block: %d\n", LargestFreeBlock());
                nlBreak();
            }
        } while (true);

        {
            u32 remaining = blockSize - requestSize;
            alignment = 4;
            if (remaining > sizeof(FreeBlockList))
            {
                cur->m_size = remaining;
            }
            else
            {
                alignment = remaining + 4;
                nlDLRingRemove<FreeBlockList>(&m_free_block_list, cur);
            }

            u32 suffixBase = requestSize - alignedSize;
            u32 header = savedSize;
            void* allocPtr;
            u32 suffixGap;
            suffixGap = suffixBase - 4;
            allocPtr = (char*)(offset - alignment) + alignment;
            if (alignment > 4)
            {
                header = savedSize | 0x80000000;
                *(u32*)((char*)allocPtr - 8) = alignment - 4;
            }
            u8* blockEnd = (u8*)allocPtr + savedSize;
            if (suffixGap != 0)
            {
                header |= 0x40000000;
                *(u32*)(((uintptr_t)blockEnd + 3) & ~3u) = suffixGap;
            }
            *(u32*)((char*)allocPtr - 4) = header;
            result = allocPtr;
        }
    }
    else
    {
        u32 savedSize;
        uintptr_t alignMask;
        u32 alignedSize;
        FreeBlockList* cur;
        FreeBlockList* start;
        uintptr_t alignedStart;
        u32 blockSize;

        start = nlDLRingGetStart<FreeBlockList>(m_free_block_list);
        cur = start;
        alignedSize = (requestSize + 3) & ~3u;
        alignMask = ~(uintptr_t)(alignment - 1);
        savedSize = requestSize;

        do
        {
            blockSize = cur->m_size;
            if (blockSize > alignedSize)
            {
                alignedStart = (uintptr_t)cur + alignment;
                alignedStart = alignMask & (alignedStart + 3);
                requestSize = alignedStart - (uintptr_t)cur;
                offset = requestSize + alignedSize;
                if (offset <= blockSize)
                    break;
            }
            cur = cur->m_next;
            if (cur == start)
            {
                nlPrintf("Total Free Memory: %d\n", TotalFreeMemory());
                nlPrintf("Largest Free Block: %d\n", LargestFreeBlock());
                nlBreak();
            }
        } while (true);

        {
            FreeBlockList* prev = cur->m_prev;
            nlDLRingRemove<FreeBlockList>(&m_free_block_list, cur);
            uintptr_t remaining = cur->m_size - offset;
            if (remaining > sizeof(FreeBlockList))
            {
                FreeBlockList* newFree = (FreeBlockList*)((char*)cur + offset);
                newFree->m_size = remaining;
                if (m_free_block_list == NULL || cur == start)
                {
                    nlDLRingAddStart<FreeBlockList>(&m_free_block_list, newFree);
                }
                else
                {
                    nlDLRingInsert<FreeBlockList>(&m_free_block_list, prev, newFree);
                }
                cur->m_size = offset;
            }

            u32 currentBlockSize = cur->m_size;
            u32 header = savedSize;
            void* allocPtr = (void*)((char*)cur + requestSize);
            uintptr_t suffixSize = currentBlockSize - offset;
            if (requestSize > 4)
            {
                header = savedSize | 0x80000000;
                *(u32*)((char*)allocPtr - 8) = requestSize - 4;
            }
            u8* blockEnd = (u8*)allocPtr + savedSize;
            if (suffixSize != 0)
            {
                header |= 0x40000000;
                *(u32*)(((uintptr_t)blockEnd + 3) & ~3u) = suffixSize;
            }
            *(u32*)((char*)allocPtr - 4) = header;
            result = allocPtr;
        }
    }

    return result;
}

/**
 * Offset/Address/Size: 0xE0 | 0x801CD82C | size: 0xF8
 */
void MemoryAllocator::Initialize(void* memory, unsigned int size)
{
    FreeBlockList* start;
    FreeBlockList* iter;
    FreeBlockList* next;
    FreeBlockList* prev;
    u32 blockSize;

    m_free_block_list = NULL;
    ((FreeBlockList*)memory)->m_size = size;
    start = nlDLRingGetStart<FreeBlockList>(m_free_block_list);
    if ((start > (FreeBlockList*)memory) || (start == NULL))
    {
        nlDLRingAddStart<FreeBlockList>(&m_free_block_list, (FreeBlockList*)memory);
    }
    else
    {
        iter = start->m_next;
        while (iter != start)
        {
            if (iter > (FreeBlockList*)memory)
            {
                break;
            }

            iter = iter->m_next;
        }

        nlDLRingInsert<FreeBlockList>(&m_free_block_list, iter->m_prev, (FreeBlockList*)memory);
    }

    next = ((FreeBlockList*)memory)->m_next;
    if (next > (FreeBlockList*)memory)
    {
        blockSize = ((FreeBlockList*)memory)->m_size;
        if ((FreeBlockList*)((u8*)memory + blockSize) == next)
        {
            ((FreeBlockList*)memory)->m_size = blockSize + next->m_size;
            nlDLRingRemove<FreeBlockList>(&m_free_block_list, next);
        }
    }

    prev = ((FreeBlockList*)memory)->m_prev;
    if (prev < (FreeBlockList*)memory)
    {
        blockSize = prev->m_size;
        if ((FreeBlockList*)((u8*)prev + blockSize) == (FreeBlockList*)memory)
        {
            prev->m_size = blockSize + ((FreeBlockList*)memory)->m_size;
            nlDLRingRemove<FreeBlockList>(&m_free_block_list, (FreeBlockList*)memory);
        }
    }
}

class TotalFreeMemCallback
{
public:
    /**
     * Offset/Address/Size: 0xCC | 0x801CD818 | size: 0x14
     */
    void Callback(FreeBlockList* block)
    {
        size = size + block->m_size;
    }

    /* 0x0 */ u32 size;
};

/**
 * Offset/Address/Size: 0x74 | 0x801CD7C0 | size: 0x58
 */
unsigned int MemoryAllocator::TotalFreeMemory()
{
    TotalFreeMemCallback callback;
    callback.size = 0;
    nlWalkDLRing<FreeBlockList, TotalFreeMemCallback>(m_free_block_list, &callback, &TotalFreeMemCallback::Callback);
    return callback.size;
}

class LargestFreeBlockCallback
{
public:
    /**
     * Offset/Address/Size: 0x58 | 0x801CD7A4 | size: 0x1C
     */
    void Callback(FreeBlockList* block)
    {
        u32 blockSize;
        u32 maxSize;

        maxSize = largest;
        blockSize = block->m_size;
        if (blockSize >= maxSize)
        {
            maxSize = blockSize;
        }
        largest = maxSize;
    }

    /* 0x0 */ u32 largest;
};

/**
 * Offset/Address/Size: 0x0 | 0x801CD74C | size: 0x58
 */
unsigned int MemoryAllocator::LargestFreeBlock()
{
    LargestFreeBlockCallback callback;
    callback.largest = 0;
    nlWalkDLRing<FreeBlockList, LargestFreeBlockCallback>(m_free_block_list, &callback, &LargestFreeBlockCallback::Callback);
    return callback.largest;
}
