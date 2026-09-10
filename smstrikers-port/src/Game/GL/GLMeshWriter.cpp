#include "Game/GL/GLMeshWriter.h"

#include "NL/nlMemory.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glState.h"

static const int gl_stream_stride[15] = {
    12, 3, 4, 4, 4, 4, 4, 4, 4, 12, 12, 12, 1, 16, 16
};

/**
 * Offset/Address/Size: 0x510 | 0x801E04B0 | size: 0x58
 */
GLMeshWriterCore::GLMeshWriterCore()
{
    pModel = nullptr;
    stream[0].id = GLStream_Invalid;
    stream[1].id = GLStream_Invalid;
    stream[2].id = GLStream_Invalid;
    stream[3].id = GLStream_Invalid;
    stream[4].id = GLStream_Invalid;
    stream[5].id = GLStream_Invalid;
    stream[6].id = GLStream_Invalid;
    stream[7].id = GLStream_Invalid;
    stream[8].id = GLStream_Invalid;
    stream[9].id = GLStream_Invalid;
    stream[10].id = GLStream_Invalid;
    stream[11].id = GLStream_Invalid;
    stream[12].id = GLStream_Invalid;
    stream[13].id = GLStream_Invalid;
    stream[14].id = GLStream_Invalid;
}

/**
 * Offset/Address/Size: 0x4C8 | 0x801E0468 | size: 0x48
 */
GLMeshWriterCore::~GLMeshWriterCore()
{
}

/**
 * Offset/Address/Size: 0x134 | 0x801E00D4 | size: 0x394
 */
bool GLMeshWriterCore::Begin(int numVerts, eGLPrimitive prim, int numStreams, const eGLStream* pStreamIDs, bool bPermanent)
{
    int i;
    unsigned long size;
    maximumVerts = numVerts;
    currentIndex = 0;
    elementCount = 0;

    for (i = 0; i < numStreams; i++)
    {
        eGLStream id = pStreamIDs[i];
        stream[id].id = (u8)id;
        int stride = gl_stream_stride[id];
        size = (unsigned long)(numVerts * stride);
        stream[id].stride = (u8)stride;

        void* addr;
        if (bPermanent)
        {
            addr = glResourceAlloc(size, GLM_VertexData);
        }
        else
        {
            addr = glFrameAlloc(size, GLM_VertexData);
        }

        if (addr == NULL)
        {
            return false;
        }

        stream[id].address = (uintptr_t)addr;
        stream[id].beData = 0;   // PORT: host order, see glModelStream
        stream[id].dataSize = (u32)size;
    }

    glModel* model;
    if (bPermanent)
    {
        model = (glModel*)glResourceAlloc(sizeof(glModel), GLM_Header);
    }
    else
    {
        model = (glModel*)glFrameAlloc(sizeof(glModel), GLM_Header);
    }
    pModel = model;
    if (pModel == NULL)
    {
        return false;
    }
    memset(pModel, 0, sizeof(glModel));

    glModelPacket* pPacket;
    if (bPermanent)
    {
        pPacket = (glModelPacket*)glResourceAlloc(sizeof(glModelPacket), GLM_Header);
    }
    else
    {
        pPacket = (glModelPacket*)glFrameAlloc(sizeof(glModelPacket), GLM_Header);
    }
    if (pPacket == NULL)
    {
        return false;
    }
    memset(pPacket, 0, sizeof(glModelPacket));

    // PORT: 6 is glModelStream's size on disc; the host record is wider.
    size = (unsigned long)(numStreams * sizeof(glModelStream));
    glModelStream* pPktStreams;
    if (bPermanent)
    {
        pPktStreams = (glModelStream*)glResourceAlloc(size, GLM_Header);
    }
    else
    {
        pPktStreams = (glModelStream*)glFrameAlloc(size, GLM_Header);
    }
    if (pPktStreams == NULL)
    {
        return false;
    }
    memset(pPktStreams, 0, size);

    pModel->numPackets = 1;
    pModel->packets = pPacket;

    pPacket->numVertices = (unsigned short)numVerts;
    pPacket->primType = (unsigned char)prim;
    pPacket->numStreams = (unsigned char)numStreams;
    pPacket->streams = pPktStreams;
    pPacket->materialset = 0;
    glStateSave(pPacket->state);

    for (i = 0; i < numStreams; i++)
    {
        eGLStream id = pStreamIDs[i];
        pPktStreams[i].id = stream[id].id;
        pPktStreams[i].stride = stream[id].stride;
        pPktStreams[i].address = stream[id].address;
        // PORT: carry the endianness explicitly rather than leaning on the memset above.
        pPktStreams[i].beData = stream[id].beData;
        pPktStreams[i].dataSize = stream[id].dataSize;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x11C | 0x801E00BC | size: 0x18
 */
bool GLMeshWriterCore::End()
{
    return (currentIndex == maximumVerts);
}

/**
 * Offset/Address/Size: 0x114 | 0x801E00B4 | size: 0x8
 */
glModel* GLMeshWriterCore::GetModel()
{
    return pModel;
}

/**
 * Offset/Address/Size: 0xF0 | 0x801E0090 | size: 0x24
 */
void GLMeshWriterCore::Colour(const nlColour& c)
{
    u32* p = (u32*)stream[GLStream_Colour].address;  // PORT: nlColour is 4 bytes
    p[currentIndex] = *(const u32*)&c;
    elementCount += 1;
}

/**
 * Offset/Address/Size: 0xD0 | 0x801E0070 | size: 0x20
 */
void GLMeshWriterCore::ColourPlat(unsigned long rgba)
{
    u32* p = (u32*)stream[GLStream_Colour].address;  // PORT: nlColour is 4 bytes
    p[currentIndex] = rgba;
    elementCount += 1;
}

/**
 * Offset/Address/Size: 0xA0 | 0x801E0040 | size: 0x30
 */
void GLMeshWriterCore::Texcoord(const nlVector2& uv)
{
    char* base = (char*)stream[GLStream_Diffuse].address;
    *(nlVector2*)(base + currentIndex * 8) = uv;
    elementCount += 1;
}

/**
 * Offset/Address/Size: 0x54 | 0x801DFFF4 | size: 0x4C
 */
void GLMeshWriterCore::Vertex(const nlVector3& pos)
{
    char* base = (char*)stream[GLStream_Position].address;
    *(nlVector3*)(base + currentIndex * 12) = pos;
    elementCount += 1;
    elementCount = 0;
    currentIndex += 1;
}

/**
 * Offset/Address/Size: 0x0 | 0x801DFFA0 | size: 0x54
 */
void GLMeshWriterCore::Vertex(const nlVector4& pos)
{
    char* base = (char*)stream[GLStream_Position4].address;
    *(nlVector4*)(base + currentIndex * 16) = pos;
    elementCount += 1;
    elementCount = 0;
    currentIndex += 1;
}
