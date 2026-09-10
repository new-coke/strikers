#include "Game/SAnim/AnimRetargeter.h"

#include <string.h>

#include "NL/nlMemory.h"
#include "NL/nlWare.h"
#include "dolphin/os.h"
#include "port/endian.h"
extern "C" unsigned long port_bmd_swap_headers(void*, unsigned long);

static inline AnimRetarget* GetAnimRetargetWithSignature_ARL(AnimRetargetList* list, const cSAnim* anim)
{
    intptr_t offset;
    AnimRetarget* p;
    AnimRetarget* result = NULL;
    offset = (intptr_t)result;

    for (long i = list->m_NumAnimRetargets; i > 0; i--)
    {
        p = (AnimRetarget*)((char*)list->m_pAnimRetarget + offset);
        if (anim->m_nHierarchySignature == p->m_TargetHierarchySignature)
        {
            result = p;
            break;
        }
        offset += sizeof(AnimRetarget);
    }

    return result;
}

static inline void* GetChunkData_ARL(nlChunk* chunk)
{
    u32 alignField = chunk->m_ID & 0x7F000000;

    if (((-alignField) | alignField) >> 31)
    {
        alignField = 1u << (alignField >> 24);
        uintptr_t result = (uintptr_t)chunk + alignField;
        result = (result + 7) & ~(uintptr_t)(alignField - 1);
        return (void*)result;
    }

    return (void*)((u8*)chunk + 8);
}

/**
 * Offset/Address/Size: 0x48 | 0x801EFFD8 | size: 0x10C
 */
AnimRetargetList* AnimRetargetList::Initialize(nlChunk* chunkData)
{
    // PORT: host order first, the extent read big-endian; a refused tree would be walked out of bounds.
    if (port_bmd_swap_headers(chunkData, port_be32(&chunkData->m_Size) + 8) == 0)
    {
        OSReport("Error: retarget chunk is not a well-formed chunk tree\n");
        return NULL;
    }

    nlChunk* chunk = (nlChunk*)((u8*)chunkData + 8);

    // PORT: allocated, not overlaid, AnimRetargetList is 0x10 on disc and wider here, and it gets written through below.
    AnimRetargetList* data =
        (AnimRetargetList*)nlMalloc(sizeof(AnimRetargetList), 8, false);
    memset(data, 0, sizeof(AnimRetargetList));
    {
        const u8* disc = (const u8*)GetChunkData_ARL(chunk);
        data->m_uHashID = port_be32(disc + 0x04);
        data->m_NumAnimRetargets = (long)(s32)port_be32(disc + 0x08);
    }

    nlChunk* nextChunk = (nlChunk*)((u8*)chunk + chunk->m_Size + 0x10);

    // PORT: AnimRetarget is 0xC on disc, two words and a pointer, so the array is rebuilt rather than pointed at.
    {
        const u8* disc = (const u8*)GetChunkData_ARL(nextChunk);
        const s32 n = (s32)data->m_NumAnimRetargets;
        data->m_pAnimRetarget = (AnimRetarget*)nlMalloc(
            (n > 0 ? n : 1) * sizeof(AnimRetarget), 8, false);
        for (s32 j = 0; j < n; j++)
        {
            data->m_pAnimRetarget[j].m_TargetHierarchySignature =
                port_be32(disc + j * 0xC + 0x00);
            data->m_pAnimRetarget[j].m_NumBones =
                (long)(s32)port_be32(disc + j * 0xC + 0x04);
            data->m_pAnimRetarget[j].m_pMap = NULL;
        }
    }

    nlChunk* mapChunk;
    s32 i = 0;

    while (i < data->m_NumAnimRetargets)
    {
        mapChunk = (nlChunk*)((u8*)nextChunk + nextChunk->m_Size + 8);
        nextChunk = mapChunk;
        signed short* nextMap = (signed short*)GetChunkData_ARL(mapChunk);

        // PORT: the map stays where it is, 16 bits either way, but its contents are big-endian.
        port_be16_array(nextMap, mapChunk->m_Size / 2);
        data->m_pAnimRetarget[i].m_pMap = nextMap;
        i++;
    }

    return data;
}

/**
 * Offset/Address/Size: 0x0 | 0x801EFF90 | size: 0x48
 */
AnimRetarget* AnimRetargetList::GetAnimRetargetWithSignature(const cSAnim* anim)
{
    return GetAnimRetargetWithSignature_ARL(this, anim);
}
