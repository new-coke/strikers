#ifndef _CHARACTERPHYSICSELEMENT_H_
#define _CHARACTERPHYSICSELEMENT_H_

#include "NL/nlMath.h"

struct CharacterPhysicsElement
{
    /* 0x00 */ nlMatrix4 matLocalToParent;
    /* 0x40 */ s8 szName[32];
    /* 0x60 */ u32 uHashID;
    /* 0x64 */ s8 szParentName[32];
    /* 0x84 */ u32 uParentHashID;
    /* 0x88 */ u32 uPrimitiveType;
    /* 0x8C */ f32 fWidth;
    /* 0x90 */ f32 fLength;
    /* 0x94 */ f32 fHeight;
    /* 0x98 */ f32 fRadius;
    /* 0x9C */ u32 uReserved;
}; // total size: 0xA0

class CharacterPhysicsData
{
public:
    virtual ~CharacterPhysicsData() { delete[] pPhysicsElements; }

    /* 0x04 */ u32 physicsElementCount;
    /* 0x08 */ CharacterPhysicsElement* pPhysicsElements;
}; // total size: 0xC

bool LoadCharacterPhysicsElements(const char* szPhysicsElementsFilename, CharacterPhysicsData* pPhysicsData);

#endif // _CHARACTERPHYSICSELEMENT_H_
