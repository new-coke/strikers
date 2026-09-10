// Duplicating front-end scene content: no .fen under art/fe was authored with a spare menu row, so
// a row is added by copying one.

#include "port/fe_clone.h"

#include <cstddef>
#include <cstring>
#include <new>

#include "Game/FE/tlSlide.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/tlImageInstance.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/tlComponent.h"
#include "Game/FE/feAnimation.h"
#include "NL/nlString.h"

// From its own header: the size parameter is size_t, a different type from `unsigned long` on
// Windows, and the mangled name differs with it.
#include "NL/nlMemory.h"

namespace
{

struct Tracked
{
    void* p;
    Tracked* next;
};

Tracked* s_tracked = nullptr;

struct Pair
{
    const TLInstance* src;
    TLInstance* dst;
};

const unsigned kMaxPairs = 512;
Pair s_pairs[kMaxPairs];
unsigned s_pairCount = 0;

struct SlidePair
{
    const TLSlide* src;
    TLSlide* dst;
};

const unsigned kMaxSlides = 128;
SlidePair s_slides[kMaxSlides];
unsigned s_slideCount = 0;

bool s_overflowed = false;

void* track(void* p)
{
    if (p == nullptr)
        return nullptr;
    Tracked* t = (Tracked*)nlMalloc(sizeof(Tracked), 8, false);
    if (t == nullptr)
        return p;   // the copy still works; it just cannot be reclaimed
    t->p = p;
    t->next = s_tracked;
    s_tracked = t;
    return p;
}

void record(const TLInstance* src, TLInstance* dst)
{
    if (s_pairCount >= kMaxPairs)
    {
        s_overflowed = true;
        return;
    }
    s_pairs[s_pairCount].src = src;
    s_pairs[s_pairCount].dst = dst;
    s_pairCount++;
}

TLInstance* mapped(const TLInstance* src)
{
    for (unsigned i = 0; i < s_pairCount; i++)
        if (s_pairs[i].src == src)
            return s_pairs[i].dst;
    return nullptr;
}

// Only FEAnimation has a vtable, so a copy is a memcpy.
size_t size_for(eTimeLineAssetType type)
{
    switch (type)
    {
    case TLAT_IMAGE:     return sizeof(TLImageInstance);
    case TLAT_TEXT:      return sizeof(TLTextInstance);
    case TLAT_COMPONENT: return sizeof(TLComponentInstance);
    default:             return sizeof(TLInstance);
    }
}

TLInstance* clone_instance(const TLInstance* src);
TLSlide* clone_slide(const TLSlide* src);

TLInstance* clone_ring(const TLInstance* head)
{
    if (head == nullptr)
        return nullptr;

    TLInstance* first = nullptr;
    TLInstance* prev = nullptr;
    const TLInstance* it = head;
    do
    {
        TLInstance* copy = clone_instance(it);
        if (copy == nullptr)
            break;
        if (first == nullptr)
        {
            first = copy;
        }
        else
        {
            prev->m_next = copy;
            copy->m_prev = prev;
        }
        prev = copy;
        it = it->m_next;
    } while (it != nullptr && it != head);

    if (first != nullptr && prev != nullptr)
    {
        prev->m_next = first;
        first->m_prev = prev;
    }
    return first;
}

TLComponent* clone_component(const TLComponent* src)
{
    if (src == nullptr)
        return nullptr;

    TLComponent* dst = (TLComponent*)track(nlMalloc(sizeof(TLComponent), 8, false));
    if (dst == nullptr)
        return nullptr;
    std::memcpy((void*)dst, (const void*)src, sizeof(TLComponent));

    dst->next = nullptr;
    dst->prev = nullptr;

    const TLSlide* s = src->pChildren;
    const TLSlide* head = s;
    TLSlide* first = nullptr;
    TLSlide* prev = nullptr;
    while (s != nullptr)
    {
        TLSlide* copy = clone_slide(s);
        if (copy == nullptr)
            break;
        if (first == nullptr)
        {
            first = copy;
        }
        else
        {
            prev->m_next = copy;
            copy->m_prev = prev;
        }
        prev = copy;
        s = s->m_next;
        if (s == head)
            break;
    }
    if (first != nullptr && prev != nullptr)
    {
        prev->m_next = first;
        first->m_prev = prev;
    }

    dst->pChildren = first;
        // Start on the slide the source is on: a highlighted row stays highlighted.
    dst->m_pActiveSlide = first;
    if (src->m_pActiveSlide != nullptr && first != nullptr)
    {
        const TLSlide* s2 = src->pChildren;
        TLSlide* d2 = first;
        while (s2 != nullptr && d2 != nullptr)
        {
            if (s2 == src->m_pActiveSlide)
            {
                dst->m_pActiveSlide = d2;
                break;
            }
            s2 = s2->m_next;
            d2 = d2->m_next;
            if (s2 == src->pChildren)
                break;
        }
    }
    return dst;
}

TLSlide* clone_slide(const TLSlide* src)
{
    if (src == nullptr)
        return nullptr;

    TLSlide* dst = (TLSlide*)track(nlMalloc(sizeof(TLSlide), 8, false));
    if (dst == nullptr)
        return nullptr;
    std::memcpy((void*)dst, (const void*)src, sizeof(TLSlide));
    dst->m_next = nullptr;
    dst->m_prev = nullptr;
    dst->m_instances = clone_ring(src->m_instances);
    dst->m_animations = nullptr;

    if (s_slideCount < kMaxSlides)
    {
        s_slides[s_slideCount].src = src;
        s_slides[s_slideCount].dst = dst;
        s_slideCount++;
    }
    else
    {
        s_overflowed = true;
    }
    return dst;
}

TLInstance* clone_instance(const TLInstance* src)
{
    if (src == nullptr)
        return nullptr;

    const size_t bytes = size_for(src->m_type);
    TLInstance* dst = (TLInstance*)track(nlMalloc(bytes, 8, false));
    if (dst == nullptr)
        return nullptr;

    std::memcpy((void*)dst, (const void*)src, bytes);
    dst->m_next = nullptr;
    dst->m_prev = nullptr;
    record(src, dst);

    dst->pChildren = clone_ring(src->pChildren);
    if (src->m_type == TLAT_COMPONENT)
        dst->m_component = clone_component(src->m_component);
    return dst;
}

void clone_animations()
{
    for (unsigned i = 0; i < s_slideCount; i++)
    {
        const FEAnimation* a = s_slides[i].src->m_animations;
        const FEAnimation* head = a;
        FEAnimation* first = nullptr;
        FEAnimation* prev = nullptr;

        while (a != nullptr)
        {
            void* mem = track(nlMalloc(sizeof(FEAnimation), 8, false));
            if (mem == nullptr)
                break;
            FEAnimation* copy = new (mem) FEAnimation();
            copy->m_cast_type = a->m_cast_type;
            copy->pad12[0] = a->pad12[0];
            copy->pad12[1] = a->pad12[1];
            copy->m_type = a->m_type;
            copy->m_DLRingHead = a->m_DLRingHead;

            TLInstance* target = mapped(a->m_pTLInstanceTarget);
            copy->m_pTLInstanceTarget = target != nullptr ? target : a->m_pTLInstanceTarget;

            if (first == nullptr)
            {
                first = copy;
            }
            else
            {
                prev->m_next = copy;
                copy->m_prev = prev;
            }
            prev = copy;

            a = a->m_next;
            if (a == head)
                break;
        }

        if (first != nullptr && prev != nullptr)
        {
            prev->m_next = first;
            first->m_prev = prev;
        }
        s_slides[i].dst->m_animations = first;
    }
}

}   // namespace

extern "C" TLInstance* PortFEClone(const TLInstance* src, const char* newName)
{
    if (src == nullptr)
        return nullptr;

    s_pairCount = 0;
    s_slideCount = 0;
    s_overflowed = false;

    TLInstance* dst = clone_instance(src);
    if (dst == nullptr)
        return nullptr;
    clone_animations();

    if (newName != nullptr)
    {
        std::strncpy(dst->m_szName, newName, sizeof dst->m_szName - 1);
        dst->m_szName[sizeof dst->m_szName - 1] = '\0';
        // FEFinder matches on the hash.
        dst->m_hash = nlStringLowerHash(dst->m_szName);
    }
    return dst;
}

extern "C" int PortFECloneOverflowed(void) { return s_overflowed ? 1 : 0; }

extern "C" void PortFEInsertAfter(TLInstance* after, TLInstance* node)
{
    if (after == nullptr || node == nullptr)
        return;
    node->m_next = after->m_next;
    node->m_prev = after;
    if (after->m_next != nullptr)
        after->m_next->m_prev = node;
    after->m_next = node;
}

extern "C" void PortFESetPosition(TLInstance* node, float x, float y, float z)
{
    if (node == nullptr)
        return;
    node->m_overloadedAttributes.v3Position.f.x = x;
    node->m_overloadedAttributes.v3Position.f.y = y;
    node->m_overloadedAttributes.v3Position.f.z = z;
}

extern "C" TLInstance* PortFEFindChild(TLInstance* parent, const char* name)
{
    if (parent == nullptr || name == nullptr)
        return nullptr;
    const unsigned long want = nlStringLowerHash(name);
    TLInstance* it = parent->pChildren;
    TLInstance* head = it;
    while (it != nullptr)
    {
        if (it->m_hash == want)
            return it;
        TLInstance* deeper = PortFEFindChild(it, name);
        if (deeper != nullptr)
            return deeper;
        it = it->m_next;
        if (it == head)
            break;
    }
    return nullptr;
}

extern "C" unsigned long PortFECloneCount(void)
{
    unsigned long n = 0;
    for (Tracked* t = s_tracked; t != nullptr; t = t->next)
        n++;
    return n;
}

// The copies are not inside the package FEScene frees with one nlFree.
extern "C" void PortFEReleaseClones(void)
{
    Tracked* t = s_tracked;
    while (t != nullptr)
    {
        Tracked* next = t->next;
        nlFree(t->p);
        nlFree(t);
        t = next;
    }
    s_tracked = nullptr;
    s_pairCount = 0;
    s_slideCount = 0;
}
