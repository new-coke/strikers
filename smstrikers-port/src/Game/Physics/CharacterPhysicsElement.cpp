#include "Game/Physics/CharacterPhysicsElement.h"
#include "NL/nlWare.h"
#include "dolphin/os.h"
extern "C" unsigned long port_phys_swap(void*, unsigned long);

#include "NL/nlFile.h"
#include "NL/nlMemory.h"

#include "Game/SAnim.h"

static inline void* nlGetChunkData(nlChunk* chunk)
{
    u32 alignField = chunk->m_ID & 0x7F000000;
    u32 isAligned = ((-alignField) | alignField) >> 31;
    if (isAligned != 0)
    {
        u32 alignment = 1u << (alignField >> 24);
        uintptr_t ptr = (uintptr_t)chunk;
        ptr += alignment;
        ptr += 7;
        return (void*)(ptr & ~(uintptr_t)(alignment - 1));
    }
    return (void*)((u8*)chunk + 8);
}

static inline nlChunk* nlGetNextChunk(nlChunk* chunk)
{
    return (nlChunk*)((u8*)chunk + chunk->m_Size + 8);
}

static inline void CopyPhysicsElements(CharacterPhysicsData* pPhysicsData, CharacterPhysicsElement* pSrc)
{
    u32 n;
    for (n = 0; n < pPhysicsData->physicsElementCount; n++)
    {
        pPhysicsData->pPhysicsElements[n] = pSrc[n];
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801FE13C | size: 0x2AC
 */
bool LoadCharacterPhysicsElements(const char* szPhysicsElementsFilename, CharacterPhysicsData* pPhysicsData)
{
    nlChunk* outerChunk;
    nlChunk* endChunk;

    unsigned long nFileSize;
    u8* pFileData = (u8*)nlLoadEntireFile(szPhysicsElementsFilename, &nFileSize, 0x20, AllocateStart);
    if (pFileData == 0)
    {
        return false;
    }

    // PORT: big-endian; a file wld_endian.c refuses would be walked out of bounds.
    if (port_phys_swap(pFileData, nFileSize) == 0)
    {
        OSReport("Error: '%s' is not a well-formed physics file\n", szPhysicsElementsFilename);
        delete pFileData;
        return false;
    }

    u32 dataSize = *((u32*)(pFileData + 4));
    outerChunk = (nlChunk*)(pFileData + 8);
    endChunk = (nlChunk*)(pFileData + dataSize + 8);

    while (outerChunk < endChunk)
    {
        s32 chunkID = (s32)outerChunk->m_ID;
        s32 chunkType = chunkID & 0x80FFFFFF;

        switch (chunkType)
        {
        case 0x0001D001:
        {
            pPhysicsData->physicsElementCount = *(u32*)nlGetChunkData(outerChunk);
            pPhysicsData->pPhysicsElements = (CharacterPhysicsElement*)nlMalloc(pPhysicsData->physicsElementCount * sizeof(CharacterPhysicsElement), 8, false);
            break;
        }

        case 0x0001D002:
        {
            CopyPhysicsElements(pPhysicsData, (CharacterPhysicsElement*)nlGetChunkData(outerChunk));
            break;
        }
        }

        outerChunk = nlGetNextChunk(outerChunk);
    }

    delete pFileData;
    return true;
}
