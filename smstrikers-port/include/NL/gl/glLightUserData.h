#ifndef NL_GL_GLLIGHTUSERDATA_H
#define NL_GL_GLLIGHTUSERDATA_H

#include "NL/nlMath.h"
#include "NL/nlColour.h"

struct GLSpecularUserData
{
    /* 0x0 */ nlVector3 worldDirection;
    /* 0x0C */ nlFloatColour colour;
    /* 0x1C */ float intensity;
    /* 0x20 */ float exponent;
}; // total size: 0x24

struct GLDirectionalLightUserData
{
    /* 0x0 */ nlVector3 direction;
    /* 0xC */ nlFloatColour colour;
}; // total size: 0x1C

struct GLLightUserData
{
    /* 0x00 */ nlVector3 worldPosition;
    /* 0x0C */ nlFloatColour colour;
    /* 0x1C */ float intensity;
    /* 0x20 */ float innerRadius;
    /* 0x24 */ float outerRadius;
}; // total size: 0x28

#endif // NL_GL_GLLIGHTUSERDATA_H
