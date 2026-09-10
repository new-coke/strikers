#ifndef _FELIBOBJECT_H_
#define _FELIBOBJECT_H_

#include "types.h"

#include "NL/nlColour.h"
#include "NL/nlMath.h"

class feVector3
{
public:
    void GetNLVector3(nlVector3& out) const
    {
        out.x = f.x;
        out.y = f.y;
        out.z = f.z;
    }

    union
    {               // inferred
        float e[3]; // offset 0x0, size 0xC
        struct
        {            // inferred
            float x; // offset 0x0, size 0x4
            float y; // offset 0x4, size 0x4
            float z; // offset 0x8, size 0x4
        } f;
    };
}; // total size: 0xC

enum eFELibObjectType
{
    FEOT_UNKNOWN = -1,
    FEOT_LAYER = 0,
    FEOT_IMAGE = 1,
    FEOT_TEXT = 2,
    FEOT_COMPONENT = 3,
    FEOT_GROUP = 4,
    FEOT_OBJECT_TYPE_ANY = -2147483648,
};

struct FELibObjectAttributes
{
    /* 0x00 */ feVector3 v3Position; // offset 0x0, size 0xC
    /* 0x0C */ feVector3 v3Rotation; // offset 0xC, size 0xC
    /* 0x18 */ feVector3 v3Scale;    // offset 0x18, size 0xC
    /* 0x24 */ feVector3 v3Pivot;    // offset 0x24, size 0xC
    /* 0x30 */ bool bVisible;        // offset 0x30, size 0x1
    /* 0x31 */ nlColour colour;      // offset 0x31, size 0x4
}; // total size: 0x38

class FELibObject
{
public:
    feVector3& GetScale() const;
    feVector3& GetRotation() const;
    feVector3& GetPosition() const;
    nlColour& GetColour() const;

    /* 0x00 */ FELibObject* next;
    /* 0x04 */ FELibObject* prev;
    /* 0x08 */ FELibObjectAttributes m_attributes; // offset 0x8, size 0x38
    /* 0x40 */ unsigned long m_hashID;
    /* 0x44 */ char m_szName[32];
    /* 0x64 */ eFELibObjectType m_type;
}; // total size: 0x68

#endif // _FELIBOBJECT_H_
