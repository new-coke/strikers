#include "Game/AI/Fuzzy.h"

#include "Game/AI/FilteredRandom.h"
static inline float GetRandomFloat()
{
    static FilteredRandomReal randgen;
    return randgen.genrand();
}

/**
 * Offset/Address/Size: 0x1E4 | 0x8003C494 | size: 0x5C
 */
float GenerateFilteredRandom()
{
    return GetRandomFloat();
}

/**
 * Offset/Address/Size: 0xF8 | 0x8003C3A8 | size: 0xEC
 */
float RandomChance(float fChance)
{
    return FGREATER(fChance, GetRandomFloat());
}

static inline float sub_f(float a, float b)
{
    return a - b;
}

static inline float div_f(float a, float b)
{
    return a / b;
}

/**
 * Offset/Address/Size: 0x7C | 0x8003C32C | size: 0x7C
 */
float FGREATER(float f1, float f2)
{
    float fDelta = sub_f(f1, f2);
    float fScore = 0.0f;

    if (fDelta > 0.0f)
    {
        f2 = sub_f(1.0f, f2);
        f2 = (f2 >= f1) ? f2 : f1;
        f2 = (f2 <= 0.5f) ? f2 : 0.5f;

        f1 = fDelta / f2;
        f1 = (f1 >= 0.0f) ? f1 : 0.0f;
        fScore = (f1 <= 1.0f) ? f1 : 1.0f;
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x0 | 0x8003C2B0 | size: 0x7C
 */
float FLESS(float f1, float f2)
{
    float fDelta = sub_f(f2, f1);
    float fScore = 0.0f;

    if (fDelta > 0.0f)
    {
        f1 = sub_f(1.0f, f1);
        f1 = (f1 >= f2) ? f1 : f2;
        f1 = (f1 <= 0.5f) ? f1 : 0.5f;

        f1 = div_f(fDelta, f1);
        f1 = (f1 >= 0.0f) ? f1 : 0.0f;
        fScore = (f1 <= 1.0f) ? f1 : 1.0f;
    }

    return fScore;
}
