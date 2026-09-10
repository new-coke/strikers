#ifndef _NLARRAYALLOCATOR_H_
#define _NLARRAYALLOCATOR_H_

#include "NL/nlDLRing.h"

template <typename T>
class nlArrayAllocator
{
public:
    nlArrayAllocator()
    {
    }

    nlArrayAllocator(T* mem, unsigned long count)
    {
        Init(mem, count);
    }

    void Init(T* mem, unsigned long count)
    {
        m_pFree = mem;
        for (unsigned long i = 0; i < count - 1; ++i)
        {
            *(T**)(mem + i) = mem + i + 1;
        }
        *(T**)(mem + count - 1) = 0;
    }

    T* Allocate()
    {
        T* entry = m_pFree;
        if (entry == 0)
            return 0;
        m_pFree = *(T**)entry;
        return entry;
    }

    void DeleteEntry(T* entry)
    {
        *(T**)entry = m_pFree;
        m_pFree = entry;
    }

    /* 0x0 */ T* m_pFree; // size 0x4
}; // total size: 0x4

template <typename T, int N>
class nlStaticArrayAllocator : public nlArrayAllocator<T>
{
public:
    nlStaticArrayAllocator()
    {
        this->m_pFree = 0;
        this->Init(reinterpret_cast<T*>(m_Memory), N);
    }

    /* 0x4 */ unsigned char m_Memory[sizeof(T) * N]; // offset 0x4
}; // total size: 0x4 + sizeof(T) * N

#endif // _NLARRAYALLOCATOR_H_
