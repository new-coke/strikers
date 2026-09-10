#include "Game/GL/gluMeshWriter.h"

#include "dolphin/PPCArch.h"
#include "dolphin/os/OSCache.h"

static const int gl_stream_stride[15] = {
    12, 3, 4, 4, 4, 4, 4, 4, 4, 12, 12, 12, 1, 16, 16
};

/**
 * Offset/Address/Size: 0x19C | 0x801B5AB4 | size: 0x90
 */
bool GLMeshWriter::End()
{
    if (currentIndex != maximumVerts)
        return false;

    for (int i = 0; i < GLStream_Num; ++i)
    {
        if (stream[i].id != GLStream_Invalid)
        {
            void* addr = (void*)stream[i].address;
            unsigned long nBytes = (unsigned long)maximumVerts * (unsigned long)stream[i].stride;
            DCStoreRangeNoSync(addr, nBytes);
        }
    }

    PPCSync();
    return true;
}

/**
 * Offset/Address/Size: 0x84 | 0x801B599C | size: 0x118
 */
void GLMeshWriter::Normal(const nlVector3& n)
{
    if (gl_stream_stride[GLStream_Normal] == 3)
    {
        s8* p = (s8*)(stream[GLStream_Normal].address + (currentIndex * 3));
        const float len = nlRecipSqrt((n.x * n.x) + (n.y * n.y) + (n.z * n.z), false);

        float nx = len * n.x;
        float ny = len * n.y;
        float nz = len * n.z;

        p[0] = (s8)(64.0f * nx);
        p[1] = (s8)(64.0f * ny);
        p[2] = (s8)(64.0f * nz);
    }
    else
    {
        *(nlVector3*)(stream[GLStream_Normal].address + (currentIndex * sizeof(nlVector3))) = n;
    }
    elementCount++;
}

/**
 * Offset/Address/Size: 0x28 | 0x801B5940 | size: 0x5C
 */
void GLMeshWriter::Texcoord(const nlVector2& uv)
{
    uintptr_t base = stream[GLStream_Diffuse].address;
    unsigned long offset = (unsigned long)currentIndex << 2;

    short* p = (short*)((unsigned char*)base + offset);
    p[0] = 1024.0f * uv.x;
    p[1] = 1024.0f * uv.y;

    elementCount++;
}

/**
 * Offset/Address/Size: 0x0 | 0x801B5918 | size: 0x28
 */
void GLMeshWriter::Texcoord(short u, short v)
{
    uintptr_t base = stream[GLStream_Diffuse].address;
    unsigned long offset = (unsigned long)currentIndex << 2;

    short* p = (short*)((unsigned char*)base + offset);
    p[0] = u;
    p[1] = v;

    elementCount++;
}
