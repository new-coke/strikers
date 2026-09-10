#ifndef GAME_INVENTORY_H
#define GAME_INVENTORY_H

#include "Game/SAnim.h"
#include "port/endian.h"
#include "NL/nlFile.h"
#include "NL/nlList.h"
#include "NL/nlString.h"

template <typename T>
class cInventory
{
public:
    cInventory()
        : m_nItemCount(0)
    {
    }

    void ParseChunks(nlChunk* chunk, nlChunk* end)
    {
        // PORT: the file is big-endian and each item converts its own subtree, so the walk reads the header rather than trusting it.
        while (chunk != end && chunk != NULL)
        {
            const unsigned long uSize = port_be32(&chunk->m_Size);
            if (T::IsValidChunkID(port_be32(&chunk->m_ID) & 0x80FFFFFF))
            {
                T* item = T::Initialize(chunk);
                m_lItemList.AddStart(item);
                m_nItemCount++;
            }
            else
            {
                nlPrintf("Warning: inventory encountered an unknown chunk type\n");
            }
            chunk = (nlChunk*)((unsigned char*)chunk + uSize + sizeof(nlChunk));
        }
    }

    void AddFile(char* memory, unsigned long length)
    {
        m_lMemList.AddStart(memory);
        ParseChunks((nlChunk*)memory, (nlChunk*)(memory + length));
    }

    void AddFile(char* filename)
    {
        unsigned long length;
        char* memory = (char*)nlLoadEntireFile(filename, &length, 0x20, AllocateStart);
        if (memory == NULL)
        {
            // PORT: upstream walked from null; say which file instead.
            nlPrintf("Warning: inventory could not load \"%s\"\n", filename);
            return;
        }
        m_lMemList.AddStart(memory);
        ParseChunks((nlChunk*)memory, (nlChunk*)(memory + length));
    }

    nlListIterator<T*> Begin()
    {
        return m_lItemList.Begin();
    }

    T* Find(unsigned int hashID)
    {
        for (nlListIterator<T*> iterator = Begin(); iterator.IsValid(); iterator.Next())
        {
            if (hashID == iterator.Current()->GetHashID())
            {
                return iterator.Current();
            }
        }
        return NULL;
    }

    T* Find(char* name)
    {
        return Find((unsigned int)nlStringHash(name));
    }

    T* Find(int index)
    {
        int i = 0;
        for (nlListIterator<T*> iterator = Begin(); iterator.IsValid(); iterator.Next())
        {
            if (i == index)
            {
                return iterator.Current();
            }
            i++;
        }
        return NULL;
    }

    ~cInventory();

    void Clear();

private:
    /* 0x0 */ nlListContainer<T*> m_lItemList;
    /* 0xC */ nlListContainer<typename T::MemType> m_lMemList;
    /* 0x18 */ int m_nItemCount;
}; // total size: 0x1C

template <typename T>
inline cInventory<T>::~cInventory()
{
    Clear();
}

template <typename T>
inline void cInventory<T>::Clear()
{
    for (nlListIterator<T*> iterator = m_lItemList.Begin();
         iterator.IsValid(); iterator.Next())
    {
        iterator.Current()->Destroy();
    }

    m_lItemList.Clear();

    while (m_lMemList.m_Head != NULL)
    {
        ListEntry<char*>* first = m_lMemList.RemoveStart();
        void* mesh;
        if (&mesh != NULL)
        {
            mesh = first->entry;
        }
        ::operator delete(first);
        ::operator delete(mesh);
    }

    m_nItemCount = 0;
}

#endif // GAME_INVENTORY_H
