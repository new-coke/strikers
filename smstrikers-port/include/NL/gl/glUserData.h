#ifndef _GLUSERDATA_H_
#define _GLUSERDATA_H_

#include "types.h"
#include "NL/gl/glStateBundle.h" // Include the new header

enum eGLUserData
{
    GLUD_Skin = 0,
    GLUD_Light = 1,
    GLUD_ShadowVolume = 2,
    GLUD_Specular = 3,
    GLUD_ShadowBuffer = 4,
    GLUD_Translucent = 5,
    GLUD_EnvDiffuse = 6,
    GLUD_ConstantColour = 7,
    GLUD_Ambient = 8,
    GLUD_Diffuse = 9,
    GLUD_DirectionalLight = 10,
    GLUD_Viewport = 11,
    GLUD_Scissor = 12,
    GLUD_NoRasterizedAlpha = 13,
    GLUD_MobileDiffuse = 14,
    GLUD_NoFog = 15,
    GLUD_CoPlanar = 16,
    GLUD_OnePassFresnel = 17,
    GLUD_Num = 18,
};

struct GLViewportUserData
{
    /* 0x00 */ u16 x;
    /* 0x02 */ u16 y;
    /* 0x04 */ u16 w;
    /* 0x06 */ u16 h;
    /* 0x08 */ uintptr_t view;         // PORT: a matrix handle
    /* 0x0C */ uintptr_t projection;   // PORT: a matrix handle
}; // total size: 0x10

#pragma pack(push)
#pragma pack(1)
struct glModelStream
{
    /* 0x0 */ uintptr_t address;
    /* 0x4 */ u8 id;
    /* 0x5 */ u8 stride;
    // PORT: host-only. 1 = the array is big-endian, i.e. it lives inside a model file.
    u8 beData;
    // PORT: host-only. How many bytes of array there are at `address`, or 0 if nobody knows.
    u32 dataSize;
}; // total size: 0x6 on disc

struct glModelPacket // size: 0x4A
{
    /* 0x00 */ uintptr_t userData;
    /* 0x04 */ uintptr_t indexBuffer;
    /* 0x08 */ u16 numVertices;
    /* 0x0A */ u8 primType;
    /* 0x0B */ u8 numStreams;
    /* 0x0C */ glModelStream* streams;
    /* 0x10 */ glStateBundle state;
    /* 0x46 */ u32 materialset;
}; // total size: 0x4A
#pragma pack(pop)

bool glUserHasType(eGLUserData type, const glModelPacket* pPacket);
void glUserDetach(eGLUserData type, glModelPacket* pPacket);
void glUserDup(glModelPacket* pDest, const glModelPacket* pSrc, bool bPerm);
void glUserAttach(const void* pUserData, glModelPacket* pPacket, bool bPerm);
void* glUserGetData(const void* pUserData);
void* glUserAlloc(eGLUserData type, unsigned long size, bool bPerm);

#endif // _GLUSERDATA_H_
