#ifndef _NLSORTEDSLOT_H_
#define _NLSORTEDSLOT_H_

#include "NL/nlArrayAllocator.h"

template <typename T, int N = 0>
class nlSortedSlot
{
public:
    template <typename U>
    struct EntryLookup
    {
        /* 0x0 */ unsigned long hash;
        /* 0x4 */ U* pEntry;

        operator unsigned long() const { return hash; }

    }; // total size: 0x8

    nlSortedSlot()
        : m_pEntryLookup(0)
        , m_EntryCount(0)
        , m_LookupAllocated(0)
    {
    }

    virtual T* GetNewEntry() = 0;
    virtual void FreeEntry(T*) = 0;
    virtual void ExpandLookup() = 0;
    virtual void FreeLookup() = 0;

    T* AddEntry(const unsigned long& id)
    {
        T* newEntry = GetNewEntry();
        if (m_EntryCount == m_LookupAllocated)
        {
            ExpandLookup();
        }

        long high;
        long low = -1;
        long middle;
        unsigned long count = m_EntryCount;
        high = count;

        while (high - low > 1)
        {
            middle = (high + low) >> 1;
            if (m_pEntryLookup[middle].hash > id)
            {
                high = middle;
            }
            else
            {
                low = middle;
            }
        }

        long insertPos = high = low + 1;
        while (count != (unsigned long)insertPos)
        {
            unsigned long previous = count - 1;
            // PORT: assign the record. EntryLookup is 16 bytes here, not 8, so a 64-bit copy moves the hash and leaves the pointer behind.
            m_pEntryLookup[count] = m_pEntryLookup[previous];
            count = previous;
        }

        m_pEntryLookup[insertPos].hash = id;
        m_pEntryLookup[insertPos].pEntry = newEntry;
        m_EntryCount = m_EntryCount + 1;
        return newEntry;
    }

    EntryLookup<T>* FindEntry(T* entry)
    {
        for (long i = 0; (unsigned long)i < m_EntryCount; i++)
        {
            if (m_pEntryLookup[i].pEntry == entry)
            {
                return &m_pEntryLookup[i];
            }
        }
        return NULL;
    }

    void DeleteEntry(T* entry)
    {
        EntryLookup<T>* lookup = FindEntry(entry);
        FreeEntry(entry);

        unsigned long total = m_EntryCount;
        long index = lookup - m_pEntryLookup;
        while ((unsigned long)index != total)
        {
            long next = index + 1;
            // PORT: assign the record. EntryLookup is 16 bytes here, not 8, so a 64-bit copy moves the hash and leaves the pointer behind.
            m_pEntryLookup[index] = m_pEntryLookup[next];
            index = next;
        }
        m_EntryCount = m_EntryCount - 1;
    }

    void DeleteEntry(EntryLookup<T>* lookup)
    {
        FreeEntry(lookup->pEntry);

        unsigned long total = m_EntryCount;
        long index = lookup - m_pEntryLookup;
        while ((unsigned long)index != total)
        {
            long next = index + 1;
            // PORT: assign the record. EntryLookup is 16 bytes here, not 8, so a 64-bit copy moves the hash and leaves the pointer behind.
            m_pEntryLookup[index] = m_pEntryLookup[next];
            index = next;
        }
        m_EntryCount = m_EntryCount - 1;
    }

    void Clear()
    {
        for (unsigned long i = 0; i < m_EntryCount; i++)
        {
            FreeEntry(m_pEntryLookup[i].pEntry);
        }

        FreeLookup();
        m_EntryCount = 0;
    }

    unsigned long GetEntryCount() const { return m_EntryCount; }
    /* 0x04 */ EntryLookup<T>* m_pEntryLookup;
    /* 0x08 */ unsigned long m_EntryCount;
    /* 0x0C */ unsigned long m_LookupAllocated;
}; // total size: 0x10

template <typename T, int N>
class nlStaticSortedSlot : public nlSortedSlot<T, N>
{
public:
    nlStaticSortedSlot()
    {
        this->m_ArrayAllocator.Init(reinterpret_cast<T*>(m_EntryData), N);
    }

    virtual T* GetNewEntry();
    virtual void FreeEntry(T*);
    virtual void ExpandLookup();
    virtual void FreeLookup();

    /* 0x10 */ unsigned char m_EntryData[sizeof(T) * N];
    /* 0x2C */ typename nlSortedSlot<T, N>::template EntryLookup<T> m_LookupData[N];
    /* 0x64 */ nlArrayAllocator<T> m_ArrayAllocator;
}; // total size: depends on T, N

template <typename T, int N>
void nlStaticSortedSlot<T, N>::ExpandLookup()
{
    this->m_pEntryLookup = m_LookupData;
    this->m_LookupAllocated = N;
}

template <typename T, int N>
void nlStaticSortedSlot<T, N>::FreeLookup()
{
    this->m_LookupAllocated = 0;
}

template <typename T, int N>
void nlStaticSortedSlot<T, N>::FreeEntry(T* entry)
{
    m_ArrayAllocator.DeleteEntry(entry);
}

template <typename T, int N>
T* nlStaticSortedSlot<T, N>::GetNewEntry()
{
    return m_ArrayAllocator.Allocate();
}

#endif // _NLSORTEDSLOT_H_
