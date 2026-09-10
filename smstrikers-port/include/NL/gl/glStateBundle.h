#ifndef _GLSTATEBUNDLE_H_
#define _GLSTATEBUNDLE_H_

#include "types.h"
#include <stdint.h>

#pragma pack(push)
#pragma pack(1)
struct glStateBundle
{
    /* 0x00 */ unsigned long long texturestate; // size 0x8
    /* 0x08 */ u32 materialstate;     // size 0x4
    /* 0x0C */ u32 program;           // size 0x4
    /* 0x10 */ u32 raster;            // size 0x4
    /* 0x14 */ uintptr_t matrix;   // PORT: a GLMatrix address                          // size 0x4
    /* 0x18 */ uintptr_t texture[6];   // PORT: id or PlatTexture*, see glx_GetTex        // size 0x18 (18, 1C, 20, 24, 28, 2C)
    /* 0x30 */ unsigned char texconfig;         // size 0x1
    /* 0x31 */ unsigned char pad;               // size 0x1
    /* 0x32 */ u32 userStateKey;      // size 0x4
}; // total size: 0x36

struct gl_StateBitfield
{
    /* 0x00 */ s32 startBit;
    /* 0x04 */ s32 numBits;
}; // total size: 0x8

#pragma pack(pop)

#endif // _GLSTATEBUNDLE_H_
