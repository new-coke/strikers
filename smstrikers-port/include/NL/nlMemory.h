#ifndef _NLMEMORY_H_
#define _NLMEMORY_H_

#include "dolphin/os.h"
#include <stddef.h>

inline unsigned long KB(unsigned long size)
{
    return size << 10;
}

inline unsigned long MB(unsigned long size)
{
    return KB(KB(size));
}

// PORT: was a local definition of the two standard placement-new operators.
#include <new>

/*
this code snippets are from Cuyler / discord commuity - no yet integrating it, as I would need to refactors different
files to use it instead of the current new/malloc implementation.

void* operator new(size_t size) {
    return nlMalloc(size, 8, false);
}

void* operator new(size_t size, size_t alignment) {
    return nlMalloc(size, alignment, false);
}
*/

void nlFree(void* ptr);
void* nlMalloc(size_t size, unsigned int alignment, bool atEnd);
void* nlMalloc(size_t size);
void* operator new(size_t size);

inline void* operator new(size_t size, unsigned int alignment, bool atEnd)
{
    return nlMalloc(size, alignment, atEnd);
}
inline void* operator new[](size_t size, unsigned int alignment, bool atEnd)
{
    return nlMalloc(size, alignment, atEnd);
}
inline void* operator new[](size_t size, unsigned int alignment, bool atEnd, const char*)
{
    return nlMalloc(size, alignment, atEnd);
}
void operator delete[](void* ptr);
void operator delete(void* ptr);
unsigned int nlVirtualTotalFree();
unsigned int nlVirtualLargestBlock();
void nlVirtualFree(void* ptr);
void* nlVirtualAlloc(size_t size, bool bZero);
void nlInitMemory();

#ifdef __cplusplus
extern "C"
{
#endif

// PORT: was four local declarations of memcpy/memmove/memset/memcmp, guarded by `#ifndef memcpy`.
#include <string.h>

#ifdef __cplusplus
}
#endif

#endif // _NLMEMORY_H_
