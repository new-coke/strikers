#include "Game/Physics/NetMeshEdge.h"

#include <string.h>

#include "port/endian.h"

/**
 * Offset/Address/Size: 0x114 | 0x80130138 | size: 0x20
 * NetMeshModelLoader::NetMeshVertex::GetPosition() const
 */
const nlVector3* NetMeshModelLoader::NetMeshVertex::GetPosition() const
{
    // PORT: was hand-indexed at the on-disc offsets.
    const glModelStream& s = mpPacket->streams[0];
    const u8* p = (const u8*)s.address + s.stride * mIndex;

    if (s.beData)
    {
        mHostPosition.x = port_bef32(p + 0);
        mHostPosition.y = port_bef32(p + 4);
        mHostPosition.z = port_bef32(p + 8);
    }
    else
    {
        memcpy(&mHostPosition, p, sizeof(nlVector3));
    }
    return &mHostPosition;
}

/**
 * Offset/Address/Size: 0x74 | 0x80130098 | size: 0xA0
 * NetMeshModelLoader::NetMeshVertex::GetNormal(nlVector3&) const
 */
void NetMeshModelLoader::NetMeshVertex::GetNormal(nlVector3& normal) const
{
    float f = 0.015625f;

    // PORT: streams[1]; single bytes, so endianness does not arise.
    const glModelStream& s = mpPacket->streams[1];
    const s8* base = (const s8*)s.address;
    u8 stride = s.stride;

    s8 normalX = (base + stride * mIndex)[0];
    s8 normalY = (base + stride * mIndex)[1];
    s8 normalZ = (base + stride * mIndex)[2];

    normal.x = (float)normalX * f;
    normal.y = (float)normalY * f;
    normal.z = (float)normalZ * f;
}

/**
 * Offset/Address/Size: 0x0 | 0x80130024 | size: 0x74
 * NetMeshModelLoader::NetMeshVertex::GetTextureCoord(nlVector2&) const
 */
void NetMeshModelLoader::NetMeshVertex::GetTextureCoord(nlVector2& txtCoord) const
{
    // PORT: streams[3], and 16-bit fixed point stored big-endian.
    const glModelStream& s = mpPacket->streams[3];
    const u8* p = (const u8*)s.address + s.stride * mIndex;

    s16 texCoordX = s.beData ? (s16)port_be16(p + 0) : ((const s16*)p)[0];
    s16 texCoordY = s.beData ? (s16)port_be16(p + 2) : ((const s16*)p)[1];

    float f = 0.0009765625f;

    txtCoord.x = (float)texCoordX * f;
    txtCoord.y = (float)texCoordY * f;
}
