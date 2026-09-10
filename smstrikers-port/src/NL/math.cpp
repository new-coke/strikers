#include "NL/nlMath.h"

/**
 * Offset/Address/Size: 0x540 | 0x801F083C | size: 0x88
 */
void nlCartesianToPolar(nlPolar& out, float x, float y)
{
    float lenSq = x * x + y * y;
    out.r = nlSqrt(lenSq, true);
    float angle = nlATan2f(y, x);
    out.a = (u16)(s32)(angle * 10430.378f);
}

/**
 * Offset/Address/Size: 0x4D4 | 0x801F07D0 | size: 0x6C
 */
void nlPolarToCartesian(float& x, float& y, unsigned short angle, float radius)
{
    float* x_ptr = &x;
    float* y_ptr = &y;
    nlSinCos(y_ptr, x_ptr, angle);
    *x_ptr *= radius;
    *y_ptr *= radius;
}

/**
 * Offset/Address/Size: 0x46C | 0x801F0768 | size: 0x68
 */
void nlPolarToCartesian(nlVector3& v, const nlPolar& polar)
{
    const volatile nlPolar& p = polar;
    f32 radius = p.r;
    nlSinCos(&v.y, &v.x, p.a);
    v.x *= radius;
    v.y *= radius;
}
/**
 * Offset/Address/Size: 0x3EC | 0x801F06E8 | size: 0x80
 */
void nlAddPolarToCartesian(nlVector3& result, const nlPolar& polar)
{
    const volatile nlPolar& p = polar;
    f32 radius = p.r;
    f32 cos_val, sin_val;
    nlSinCos(&sin_val, &cos_val, p.a);
    cos_val *= radius;
    sin_val *= radius;
    result.x += cos_val;
    result.y += sin_val;
}

/**
 * Offset/Address/Size: 0x364 | 0x801F0660 | size: 0x88
 */
void nlCartesianToPolar(nlPolar& out, const nlVector3& in)
{
    float x = in.x;
    float y = in.y;
    float lenSq = x * x + y * y;
    out.r = nlSqrt(lenSq, true);
    float angle = nlATan2f(y, x);
    out.a = (u16)(s32)(angle * 10430.378f);
}

/**
 * Offset/Address/Size: 0x2B8 | 0x801F05B4 | size: 0xAC
 */
void nlMakeQuat(nlQuaternion& out, const nlVector3& v3RotationAxis, float ang_rad)
{
    f32 cosHalfAngle;
    f32 sinHalfAngle;
    s32 halfAngleUnits;

    ang_rad *= 0.5f;
    halfAngleUnits = 10430.378f * ang_rad;

    cosHalfAngle = nlSin((u16)halfAngleUnits + 0x4000);
    sinHalfAngle = nlSin((u16)halfAngleUnits);

    out.x = v3RotationAxis.x * sinHalfAngle;
    out.y = v3RotationAxis.y * sinHalfAngle;
    out.z = v3RotationAxis.z * sinHalfAngle;
    out.w = cosHalfAngle;
}

/**
 * Offset/Address/Size: 0x268 | 0x801F0564 | size: 0x50
 */
void nlQuatNormalize(nlQuaternion& out, const nlQuaternion& in)
{
    float fLenSquared = nlQuatDot(in, in);
    float fOneOverSqrt = nlRecipSqrt(fLenSquared, true);
    nlQuatScale(out, in, fOneOverSqrt);
}
/**
 * Offset/Address/Size: 0x1E8 | 0x801F04E4 | size: 0x80
 */
void nlQuatInverse(nlQuaternion& out, const nlQuaternion& in)
{
    float fOneOverSqrt = nlRecipSqrt(nlQuatDot(in, in), true);

    out.x = -in.x * fOneOverSqrt;
    out.y = -in.y * fOneOverSqrt;
    out.z = -in.z * fOneOverSqrt;
    out.w = in.w * fOneOverSqrt;
}

/**
 * Offset/Address/Size: 0xA8 | 0x801F03A4 | size: 0x140
 */
void nlQuatNLerp(nlQuaternion& out, const nlQuaternion& q1, const nlQuaternion& q2, float t)
{
    float dot = nlQuatDot(q1, q2);
    if (dot > 0.0f)
    {
        out.x = t * (q2.x - q1.x) + q1.x;
        out.y = t * (q2.y - q1.y) + q1.y;
        out.z = t * (q2.z - q1.z) + q1.z;
        out.w = t * (q2.w - q1.w) + q1.w;
    }
    else
    {
        out.x = t * (-q2.x - q1.x) + q1.x;
        out.y = t * (-q2.y - q1.y) + q1.y;
        out.z = t * (-q2.z - q1.z) + q1.z;
        out.w = t * (-q2.w - q1.w) + q1.w;
    }
    float fOneOverSqrt = nlRecipSqrt(nlQuatDot(out, out), true);
    nlQuatScale(out, out, fOneOverSqrt);
}

/**
 * Offset/Address/Size: 0x0 | 0x801F02FC | size: 0xA8
 */
void nlInvertRotTransMatrix(nlMatrix4& out, const nlMatrix4& in)
{
    nlVector3 negResult;
    nlVector3 translation;
    ((u32*)&translation)[0] = *(u32*)&in.e2[3][0];
    ((u32*)&translation)[1] = *(u32*)&in.e2[3][1];
    ((u32*)&translation)[2] = *(u32*)&in.e2[3][2];

    nlTransposeMatrix(out, in);

    out.e2[2][3] = 0.0f;
    out.e2[1][3] = 0.0f;
    out.e2[0][3] = 0.0f;

    nlMultPosVectorMatrix(negResult, translation, out);

    nlVec3Set(negResult, -1.0f * negResult.x, -1.0f * negResult.y, -1.0f * negResult.z);

    out.e2[3][0] = negResult.x;
    out.e2[3][1] = negResult.y;
    out.e2[3][2] = negResult.z;
    out.e2[3][3] = 1.0f;
}

/**
 * Stub only for field order; unreferenced so the linker drops it.
 * Forces emission of specific constants/operations so the compiler lays out the related fields to match the original binary.
 */
void math_stub(float& v0, float& v1, float& v2, float& v3, float& v4)
{
    v0 = 0.0f;
    v1 = 1.0f;
    v2 = -1.0f;
    v3 = 0.5f;
    v4 = 10430.378f;
}
