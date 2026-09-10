#ifndef _LIGHTOBJECT_H_
#define _LIGHTOBJECT_H_

#include "NL/nlMath.h"
#include "NL/nlColour.h"

class LightObject
{
public:
    /* 0x00 */ unsigned long m_uHashID;
    /* 0x04 */ nlVector3 m_worldPosition;
    /* 0x10 */ float m_fIntensity;
    /* 0x14 */ float m_fFarAttenuationStart;
    /* 0x18 */ float m_fFarAttenuationEnd;
    /* 0x1C */ nlFloatColour m_colour;
    /* 0x2C */ unsigned long m_bit;
    /* 0x30 */ float m_fRadiusSquared;
    /* 0x34 */ unsigned long m_emitFlags;
}; // total size: 0x38

#endif // _LIGHTOBJECT_H_
