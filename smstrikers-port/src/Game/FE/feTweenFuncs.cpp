#include "Game/FE/feTweenFuncs.h"

#include "math.h"
#include "NL/nlMath.h"

namespace TweenFunctions
{

/**
 * Offset/Address/Size: 0x104 | 0x800A2FC8 | size: 0xF4
 */
float easeinelastic(float t, float b, float c, float d)
{
    float p;

    if (t == 0.0f)
    {
        return b;
    }

    t = t / d;
    if (t == 1.0f)
    {
        return b + c;
    }

    t = t - 1.0f;
    p = (float)(0.3 * (double)d);
    u16 idx = (u16)(s32)(10430.378f * ((6.2831855f * ((t * d) - p / 4.0f)) / p));
    float sinv = nlSin(idx);
    double pow_d = pow(2.0, (double)(10.0f * t));
    double prod = ((double)c) * pow_d;
    return (float)(-((prod * (double)sinv) - (double)b));
}

/**
 * Offset/Address/Size: 0x10 | 0x800A2ED4 | size: 0xF4
 */
float easeoutelastic(float t, float b, float c, float d)
{
    float p;
    float sinv;
    double prod;

    if (t == 0.0f)
    {
        return b;
    }
    t = t / d;
    if (t == 1.0f)
    {
        return b + c;
    }

    p = (float)(0.3 * (double)d);
    sinv = nlSin((u16)(s32)(10430.378f * ((6.2831855f * ((t * d) - p / 4.0f)) / p)));
    prod = c * pow(2.0, -10.0f * t);
    return (float)(b + ((prod * sinv) + c));
}

/**
 * Offset/Address/Size: 0x0 | 0x800A2EC4 | size: 0x10
 */
float linear(float t, float b, float c, float d)
{
    return b + (c * t) / d;
}

} // namespace TweenFunctions
