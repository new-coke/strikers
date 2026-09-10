#include "NL/glx/glxDisplayList.h"
#include "NL/gl/glMemory.h"
#include "NL/nlString.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/PPCArch.h"

#define DISPLAY_LIST_HEADER 0xBA7EF00D

static unsigned char opcodes[6] = {
    0x90,
    0x98,
    0xA0,
    0x80,
    0xA8,
    0xB0,
};

struct DisplayListEx
{
    /* 0x00 */ u32 m_header;
    /* 0x04 */ void* m_displayList;
    /* 0x08 */ u32 m_size;
    /* 0x0C */ u16 m_numStreams;
    /* 0x0E */ u16 m_hasColorStream;
};

/**
 * Offset/Address/Size: 0x0 | 0x801C1E5C | size: 0x2A8
 */
DisplayList* dlMakeDisplayList(const glModelPacket* packet, bool permanent)
{
    DisplayList* pList;
    u32 actualSize;
    u32 size;
    u8* p;
    u32 i;
    u32 j;
    u8 bStitch;

    u32 numStreams = packet->numStreams;

    bStitch = 0;
    i = 0;
    for (j = 0; j < numStreams; j++)
    {
        // PORT: was `i += 6` with `[i + 4]`, glModelStream's on-disc layout.
        if (packet->streams[j].id == 0x0C)
        {
            bStitch = 1;
            break;
        }
    }

    if (bStitch)
    {
        u16 numVertices = packet->numVertices;
        actualSize = numVertices * ((numStreams - 1) << 1) + 3;
        actualSize += numVertices;
    }
    else
    {
        actualSize = packet->numVertices * (numStreams << 1) + 3;
    }

    size = (actualSize + 0x1F) & ~0x1F;
    if (permanent)
    {
        p = (u8*)glResourceAlloc(size, GLM_VertexData);
    }
    else
    {
        p = (u8*)glFrameAlloc(size, GLM_VertexData);
    }

    nlZeroMemory(p + actualSize, size - actualSize);

    u8* p8 = p + 3;
    u8 hasColor = bStitch;
    u16* pInd = (u16*)packet->indexBuffer;

    p[0] = opcodes[packet->primType];
    // PORT: the FIFO is a big-endian byte stream; the indices copied below come from the file already in that order.
    p[1] = (u8)(packet->numVertices >> 8);
    p[2] = (u8)(packet->numVertices & 0xFF);
    i = 0;

    // TODO: Remove the const cast once a clean source form preserves the exact register allocation.
    while (i < ((glModelPacket*)packet)->numVertices)
    {
        // PORT: the FIFO is big-endian and *pInd is host order.
        if (hasColor)
        {
            *p8++ = 0xFF;
            for (j = 0; j < packet->numStreams - 1; j++)
            {
                p8[0] = (u8)(*pInd >> 8);
                p8[1] = (u8)(*pInd & 0xFF);
                p8 += 2;
            }
        }
        else
        {
            for (j = 0; j < packet->numStreams; j++)
            {
                p8[0] = (u8)(*pInd >> 8);
                p8[1] = (u8)(*pInd & 0xFF);
                p8 += 2;
            }
        }

        i++;
        pInd++;
    }

    if (permanent)
    {
        pList = (DisplayList*)glResourceAlloc(sizeof(DisplayList), GLM_Header);
    }
    else
    {
        // PORT: 0x10 is the console sizeof.
        pList = (DisplayList*)glFrameAlloc(sizeof(DisplayList), GLM_Header);
    }

    DisplayListEx* pListEx = (DisplayListEx*)pList;
    pListEx->m_header = DISPLAY_LIST_HEADER;
    pListEx->m_displayList = p;
    pListEx->m_size = size;
    pListEx->m_numStreams = packet->numStreams;
    pListEx->m_hasColorStream = bStitch != 0;

    DCFlushRangeNoSync(pList->list, pList->size);
    PPCSync();

    return pList;
}

/**
 * Offset/Address/Size: 0x2A8 | 0x801C2104 | size: 0x30
 */
u32 dlGetSize(uintptr_t addr)
{
    DisplayList* dl = (DisplayList*)addr;

    if (dl == NULL)
    {
        dl = NULL;
    }
    else
    {
        dl = (dl->magic == DISPLAY_LIST_HEADER) ? dl : NULL;
    }

    return dl->size;
}

/**
 * Offset/Address/Size: 0x2D8 | 0x801C2134 | size: 0x30
 */
void* dlGetDisplayList(uintptr_t addr)
{
    DisplayList* dl = (DisplayList*)addr;

    if (dl == NULL)
    {
        dl = NULL;
    }
    else
    {
        dl = (dl->magic == DISPLAY_LIST_HEADER) ? dl : NULL;
    }

    return dl->list;
}

/**
 * Offset/Address/Size: 0x308 | 0x801C2164 | size: 0x38
 */
bool dlIsDisplayList(uintptr_t addr)
{
    DisplayList* dl = (DisplayList*)addr;

    if (dl == NULL)
    {
        dl = NULL;
    }
    else
    {
        dl = (dl->magic == DISPLAY_LIST_HEADER) ? dl : NULL;
    }

    return dl != NULL;
}

/**
 * Offset/Address/Size: 0x340 | 0x801C219C | size: 0x28
 */
DisplayList* dlGetStruct(uintptr_t addr)
{
    DisplayList* dl = (DisplayList*)addr;

    if (dl == NULL)
    {
        return NULL;
    }

    if (dl->magic != DISPLAY_LIST_HEADER)
    {
        return NULL;
    }

    return dl;
}
