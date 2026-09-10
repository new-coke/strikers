#ifndef _NLCOLOUR_H_
#define _NLCOLOUR_H_

#include "types.h"

struct nlColour_
{
    u8 rgba[4];
};

struct nlColour
{
    unsigned char c[4];

    bool operator==(const nlColour& other) const
    {
        return *(u32*)&c[0] == *(u32*)&other.c[0];
    }

    bool operator!=(const nlColour& other) const
    {
        return !(*this == other);
    }

    nlColour& operator=(const nlColour& other)
    {
        *(u32*)&c[0] = *(u32*)&other.c[0];
        return *this;
    }
};

inline void nlColourSet(nlColour& c0, u8 _r, u8 _g, u8 _b, u8 _a)
{
    c0.c[0] = _r;
    c0.c[1] = _g;
    c0.c[2] = _b;
    c0.c[3] = _a;
}

inline void nlColourSet(nlColour& c0, u8 _r, u8 _g, u8 _b)
{
    c0.c[0] = _r;
    c0.c[1] = _g;
    c0.c[2] = _b;
}

inline void nlColourSeReversed(nlColour& c0, u8 _r, u8 _g, u8 _b, u8 _a)
{
    c0.c[3] = _a;
    c0.c[2] = _b;
    c0.c[1] = _g;
    c0.c[0] = _r;
}

struct nlFloatColour
{
    float c[4]; // offset 0x0, size 0x10
}; // total size: 0x10

inline u8 nlFloatColourToByte(float input)
{
    int value = (int)(input * 255.5f);
    if (value < 0)
    {
        return 0;
    }
    else if (value > 255)
    {
        return 255;
    }
    return (u8)value;
}

inline void nlFloatColourSet(nlFloatColour& c0, float _r, float _g, float _b, float _a)
{
    c0.c[0] = _r;
    c0.c[1] = _g;
    c0.c[2] = _b;
    c0.c[3] = _a;
}

inline void ConvertColour(nlColour& out, const nlFloatColour& in)
{
    out.c[0] = (s32)(in.c[0] * 255.0f);
    out.c[1] = (s32)(in.c[1] * 255.0f);
    out.c[2] = (s32)(in.c[2] * 255.0f);
    out.c[3] = (s32)(in.c[3] * 255.0f);
}

#endif // _NLCOLOUR_H_
