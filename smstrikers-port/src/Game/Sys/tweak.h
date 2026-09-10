#ifndef _TWEAK_H_
#define _TWEAK_H_

#include "NL/nlColour.h"

enum eTweakType
{
    TWEAK_Int = 0,
    TWEAK_Enum = 1,
    TWEAK_Bool = 2,
    TWEAK_Float = 3,
    TWEAK_Short = 4,
    TWEAK_Byte = 5,
    TWEAK_Func = 6,
    TWEAK_ParmFunc = 7,
    TWEAK_MembFunc = 8,
    TWEAK_Colour = 9,
    TWEAK_FloatColour = 10,
    TWEAK_GraphFloat = 11,
    TWEAK_GraphInt = 12,
    TWEAK_GraphFloatFunc = 13,
    TWEAK_GraphIntFunc = 14,
    TWEAK_Title = 15,
    TWEAK_Num = 16,
};

struct Tweakable
{
    eTweakType type;
    void* pointer;
    const char* name;
    float minimum;
    float maximum;
    float increment;
    const char* const* enum_names;
}; // total size: 0x1C

void DrawTextRectangle(int view, float vx0, float vy0, float vx1, float vy1, float depth, const nlColour& colour, bool expand8);

#endif // _TWEAK_H_
