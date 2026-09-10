#ifndef _MEMALLOC_H_
#define _MEMALLOC_H_

#include "types.h"

struct FreeBlockList
{
    /* 0x0 */ FreeBlockList* m_next;
    /* 0x4 */ FreeBlockList* m_prev;
    /* 0x8 */ u32 m_size;
};

class MemoryAllocator
{
public:
    unsigned int LargestFreeBlock();
    unsigned int TotalFreeMemory();
    void Initialize(void* memory, unsigned int size);
    void* Allocate(unsigned long size, unsigned int alignment, bool fromEnd);
    void Free(void* p);

    /* 0x0 */ FreeBlockList* m_free_block_list;
};

extern MemoryAllocator StandardAllocator;
extern MemoryAllocator VirtualAllocator;

template <typename T, typename CallbackType>
void nlWalkDLRing(T* head, CallbackType* callback, void (CallbackType::*callbackFunc)(T*));

#endif // _MEMALLOC_H_
