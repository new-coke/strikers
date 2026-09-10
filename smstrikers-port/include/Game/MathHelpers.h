#ifndef _MATHHELPERS_H_
#define _MATHHELPERS_H_

#include "types.h"
#include "NL/nlMath.h"

/*
 * Converts a 2D direction vector to a u16 angle (0x10000 = 360 deg)
 * offset: angle offset to add (0x4000 = 90 deg, 0x8000 = 180 deg, etc.)
 */
static inline u16 nlVector3ToAngle(const nlVector3& dir, u16 offset = 0)
{
    return (u16)((u32)(u16)(s32)(10430.378f * nlATan2f(dir.y, dir.x)) + offset);
}

/*
 * Converts radians to a u16 angle (0x10000 = 360 deg)
 */
static inline u16 RadToAng16(f32 r)
{
    return (u16)(s32)(10430.378f * r);
}

/*
 * Returns the maximum of two float values
 */
static inline float nlMaxEquals(float a, float b)
{
    return (a >= b) ? a : b;
}

/*
 * Returns the minimum of two float values
 */
static inline float nlMinEquals(float a, float b)
{
    return (a <= b) ? a : b;
}

/*
 * Calculates the absolute value of a signed 16-bit integer (returns s32)
 */
static inline s32 abs_s16(s16 x)
{
    if (x < 0)
    {
        return -x;
    }
    return x;
}

/*
 * Calculates the squared distance between two 3D positions
 */
inline float CalculateDistanceSquared(const nlVector3& pos1, const nlVector3& pos2)
{
    nlVector3 delta;
    nlVec3Sub(delta, pos1, pos2);
    return nlGetLengthSquared3D(delta.x, delta.y, delta.z);
}

/*
 * Tests if the squared distance between two positions is less than a threshold
 */
inline float TestDistanceSquaredLessThan(const nlVector3& pos1, const nlVector3& pos2, float threshold)
{
    nlVector3 delta;
    nlVec3Sub(delta, pos1, pos2);
    return nlGetLengthSquared3D(delta.x, delta.y, delta.z) < (threshold * threshold);
}

/*
 * Absolute value of a signed 16-bit angle delta, as an unsigned 16-bit angle
 */
static inline u16 abs_ang16(s16 x)
{
    return (u16)(x < 0 ? -x : x);
}

#endif // _MATHHELPERS_H_
