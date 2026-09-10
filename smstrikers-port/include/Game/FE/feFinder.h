#ifndef _FEFINDER_H_
#define _FEFINDER_H_

#include "NL/nlString.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/tlInstance.h"

template <class T>
T* FindItemByHashID(T* list, unsigned long hashID)
{
    if (list == 0)
        return 0;
    T* curr = list->m_next;
    for (;;)
    {
        unsigned long id = curr->m_hash;
        T* next = curr->m_next;
        if (hashID == id)
            return curr;
        if (curr == list)
            break;
        curr = next;
    }
    return 0;
}

template <class T>
T* CastToSomeType(T*, void* pValue)
{
    return (T*)pValue;
}

static inline TLSlide* FEGetChildren(FEPresentation* p)
{
    return p->m_slides;
}

static inline TLInstance* FEGetChildren(TLSlide* p)
{
    return p->m_instances;
}

static inline TLInstance* FEGetChildren(TLInstance* p)
{
    return p->pChildren;
}

struct InlineHasher
{
    unsigned long m_Hash;
    InlineHasher() { }
    InlineHasher(unsigned long h)
        : m_Hash(h)
    {
    }
};

template <typename T, int N>
struct FEFinder
{
    template <typename U>
    inline static T* Find(U* pTopLevel, InlineHasher Level1, InlineHasher Level2 = 0, InlineHasher Level3 = 0, InlineHasher Level4 = 0, InlineHasher Level5 = 0, InlineHasher Level6 = 0);

    template <typename U>
    inline static T* _Find(U* pTopLevel, const unsigned long Level1, const unsigned long Level2,
        const unsigned long Level3, const unsigned long Level4, const unsigned long Level5, const unsigned long Level6);
};

template <typename T, int N>
template <typename U>
T* FEFinder<T, N>::Find(U* pTopLevel, InlineHasher Level1, InlineHasher Level2, InlineHasher Level3, InlineHasher Level4, InlineHasher Level5, InlineHasher Level6)
{
    return _Find(pTopLevel, Level1.m_Hash, Level2.m_Hash, Level3.m_Hash, Level4.m_Hash, Level5.m_Hash, Level6.m_Hash);
}

template <typename T, int N>
template <typename U>
T* FEFinder<T, N>::_Find(U* pTopLevel, const unsigned long Level1, const unsigned long Level2,
    const unsigned long Level3, const unsigned long Level4, const unsigned long Level5, const unsigned long Level6)
{
    void* pChild = FindItemByHashID(FEGetChildren(pTopLevel), Level1);
    if (pChild == 0)
        return 0;
    if (Level2 == 0)
        return (T*)pChild;
    return _Find(CastToSomeType(FEGetChildren(pTopLevel), pChild), Level2, Level3, Level4, Level5, Level6, 0);
}

#endif
