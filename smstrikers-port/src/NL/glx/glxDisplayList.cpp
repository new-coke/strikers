#include "NL/glx/glxDisplayList.h"
#include "NL/gl/glMemory.h"
#include "NL/nlString.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/PPCArch.h"
#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_match_runtime.hpp"
#include "NL/glx/glxMemory.h"
#endif

#define DISPLAY_LIST_HEADER 0xBA7EF00D

static const unsigned char opcodes[6] = {
    0x90,
    0x98,
    0xA0,
    0x80,
    0xA8,
    0xB0,
};

/**
 * Offset/Address/Size: 0x0 | 0x801C1E5C | size: 0x2A8
 */
DisplayList* dlMakeDisplayList(const glModelPacket* packet, bool permanent)
{
    const u32 numStreams = packet->numStreams;
    const u32 numVertices = packet->numVertices;
    bool hasColor = false;
    for (u32 j = 0; j < numStreams; j++)
    {
        // PORT: was `i += 6` with `[i + 4]`, glModelStream's on-disc layout.
        if (packet->streams[j].id == 0x0C)
        {
            hasColor = true;
            break;
        }
    }

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    const u32 size=(numVertices*sizeof(u16)+31u)&~31u;
    u8* p=(u8*)(permanent?glResourceAlloc(size,GLM_VertexData):glFrameAlloc(size,GLM_VertexData));
    memcpy(p,(const void*)packet->indexBuffer,numVertices*sizeof(u16));
    memset(p+numVertices*sizeof(u16),0,size-numVertices*sizeof(u16));
#else
    // The color stream is one direct byte; all other attributes repeat the
    // same big-endian index. The stream layout is constant for the entire DL.
    const u32 indexedStreams = numStreams - (hasColor ? 1u : 0u);
    const u32 actualSize = 3 + numVertices * (indexedStreams * 2 + (hasColor ? 1u : 0u));
    const u32 size = (actualSize + 0x1F) & ~0x1F;
    u8* p = (u8*)(permanent ? glResourceAlloc(size, GLM_VertexData) : glFrameAlloc(size, GLM_VertexData));

    nlZeroMemory(p + actualSize, size - actualSize);

    u8* p8 = p + 3;
    const u16* pInd = (const u16*)packet->indexBuffer;

    p[0] = opcodes[packet->primType];
    // The source indices are host order; the FIFO header and attributes are BE.
    p[1] = (u8)(packet->numVertices >> 8);
    p[2] = (u8)(packet->numVertices & 0xFF);
    for (u32 i = 0; i < numVertices; i++)
    {
        if (hasColor)
            *p8++ = 0xFF;
        const u16 index = indexedStreams != 0 ? pInd[i] : 0;
        const u8 high = (u8)(index >> 8);
        const u8 low = (u8)index;
        for (u32 j = 0; j < indexedStreams; j++)
        {
            *p8++ = high;
            *p8++ = low;
        }
    }

#endif
    DisplayList* pList = (DisplayList*)(permanent ? glResourceAlloc(sizeof(DisplayList), GLM_Header)
                                                : glFrameAlloc(sizeof(DisplayList), GLM_Header));
    pList->magic = DISPLAY_LIST_HEADER;
    pList->list = p;
    pList->size = size;
    pList->indices[0] = numStreams;
    pList->indices[1] = hasColor;

    DCFlushRangeNoSync(pList->list, pList->size);
    PPCSync();

#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_RUNTIME)
    if (permanent)
        port::directmatch::on_display_list_created(
            packet, reinterpret_cast<uintptr_t>(pList),
            glx_GetResourceMarkerLevel());
#endif


    return pList;
}

/**
 * Offset/Address/Size: 0x2A8 | 0x801C2104 | size: 0x30
 */
u32 dlGetSize(uintptr_t addr)
{
    const DisplayList* dl = dlGetStruct(addr);
    return dl != NULL ? dl->size : 0;
}

/**
 * Offset/Address/Size: 0x2D8 | 0x801C2134 | size: 0x30
 */
void* dlGetDisplayList(uintptr_t addr)
{
    const DisplayList* dl = dlGetStruct(addr);
    return dl != NULL ? dl->list : NULL;
}

/**
 * Offset/Address/Size: 0x308 | 0x801C2164 | size: 0x38
 */
bool dlIsDisplayList(uintptr_t addr)
{
    return dlGetStruct(addr) != NULL;
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
