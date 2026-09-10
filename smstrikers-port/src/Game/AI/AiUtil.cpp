#include "Game/AI/AiUtil.h"

#include "Game/Field.h"
#include "math.h"

char* g_sPowerupNames[9] = {
    "GreenShell",
    "RedShell",
    "SpinyShell",
    "FreezeShell",
    "Banana",
    "BoBomb",
    "ChainChomp",
    "Mushroom",
    "Star"
};

static f32 CANT_COLLIDE = *(f32*)__float_max;

/**
 * Offset/Address/Size: 0x1420 | 0x80006ECC | size: 0x90
 */
void MakePerpendicularPlane(const nlVector3& v3Position, unsigned short aNormalAngle, nlVector4& v4Plane, float fPlaneOffset)
{
    float fSin;
    float fCos;
    nlSinCos(&fSin, &fCos, aNormalAngle);

    v4Plane.x = fCos;
    v4Plane.y = fSin;
    v4Plane.z = 0.0f;

    float t0 = v3Position.y * fSin;
    float t1 = v3Position.x * fCos + t0;
    v4Plane.w = fPlaneOffset + t1;
}

/**
 * Offset/Address/Size: 0x1354 | 0x80006E00 | size: 0xCC
 */
void MakePerpendicularPlane(const nlVector3& v3Position, const nlVector3& v3Normal, nlVector4& v4Plane, float fPlaneOffset)
{
    f32 lenSq = const_cast<nlVector3&>(v3Normal).GetLengthSq3D();
    f32 invLen = nlRecipSqrt(lenSq, true);

    f32 normX = invLen * v3Normal.x;
    f32 normY = invLen * v3Normal.y;
    f32 normZ = invLen * v3Normal.z;
    v4Plane.x = normX;
    v4Plane.y = normY;
    v4Plane.z = normZ;

    v4Plane.w = fPlaneOffset + (v3Position.x * v4Plane.x + v3Position.y * v4Plane.y + v3Position.z * v4Plane.z);
}

/**
 * Offset/Address/Size: 0x1204 | 0x80006CB0 | size: 0x150
 */
bool IsPointInCone(const nlVector3& v3Point, const nlVector3& v3Pivot, const nlVector3& v3Plane1, const nlVector3& v3Plane2)
{
    f32 distSqA = const_cast<nlVector3&>(v3Plane1).CalculateDistanceSquared2D(v3Pivot);
    f32 distSqP = const_cast<nlVector3&>(v3Plane1).CalculateDistanceSquared2D(v3Point);
    f32 pointY = v3Point.y;
    f32 pivotY = v3Pivot.y;

    if (distSqP < distSqA)
    {
        f32 dirX = v3Pivot.x - v3Point.x;
        f32 zeroVal = 0.0f;
        f32 dirY = pivotY - pointY;

        f32 perpY = -dirY;
        f32 lenSq = perpY * perpY + dirX * dirX;
        f32 invLen = nlRecipSqrt(zeroVal + lenSq, true);

        nlVector4 v4Plane;
        nlVector3& v3Plane = *(nlVector3*)&v4Plane;
        v3Plane.x = invLen * perpY;
        v3Plane.y = invLen * dirX;
        v3Plane.z = invLen * zeroVal;
        v4Plane.w = zeroVal + (v3Pivot.x * v4Plane.x + v3Pivot.y * v4Plane.y + v3Pivot.z * v4Plane.z);

        f32 sideLeft = (v3Plane1.x * v4Plane.x + v3Plane1.y * v4Plane.y + v3Plane1.z * v4Plane.z) - v4Plane.w;
        f32 sideRight = (v3Plane2.x * v4Plane.x + v3Plane2.y * v4Plane.y + v3Plane2.z * v4Plane.z) - v4Plane.w;

        if (sideLeft * sideRight < zeroVal)
        {
            return true;
        }
    }

    return false;
}

#undef abs
extern "C" int abs(int n);

static inline s16 AngleDiff(u16 a, u16 b)
{
    return (s16)(a - b);
}

/**
 * Offset/Address/Size: 0x110C | 0x80006BB8 | size: 0xF8
 */
unsigned short SeekDirection(unsigned short aCurrent, unsigned short aDesired, float fSeekSpeed, float fFalloff, float fDeltaT)
{
    u16 current = aCurrent;
    s16 diff = AngleDiff(aDesired, current);
    signed short nDeltaDisplacement;

    if (diff != 0)
    {
        f64 dDiff = (f64)(s16)diff;
        f64 absDiffD = __fabs(dDiff);
        f32 absDiff = (f32)absDiffD;

        if (absDiff < fFalloff)
        {
            f32 fSeekCoefficient = fFalloff * fFalloff / fSeekSpeed;
            nDeltaDisplacement = (s16)(s32)(absDiff - fSeekCoefficient / (fDeltaT + fSeekCoefficient / absDiff));
        }
        else
        {
            nDeltaDisplacement = (s16)(s32)(fDeltaT * fSeekSpeed);
        }

        if (abs(diff) < (s16)nDeltaDisplacement)
        {
            return aDesired;
        }

        if (diff > 0)
        {
            return (u16)(current + (s16)nDeltaDisplacement);
        }
        else
        {
            return (u16)(current - (s16)nDeltaDisplacement);
        }
    }

    return aDesired;
}

/**
 * Offset/Address/Size: 0x10C0 | 0x80006B6C | size: 0x4C
 */
float SeekSpeed(float fCurrent, float fDesired, float fSeekAccel, float fSeekDecel, float fDeltaT)
{
    float seekRate = (fCurrent <= fDesired) ? fSeekAccel : fSeekDecel;
    float delta = seekRate * fDeltaT;

    if (fCurrent <= fDesired)
    {
        float newValue = fCurrent + delta;
        if (newValue > fDesired)
        {
            return fDesired;
        }
        return newValue;
    }

    float newValue = fCurrent - delta;
    if (newValue < fDesired)
    {
        return fDesired;
    }
    return newValue;
}

/**
 * Reading the components through a f32* keeps them out of the cross-statement
 * CSE web they would otherwise share with the dot product below, which is what
 * holds them in the volatile temp pool (f4/f5) instead of f10/f11.
 */
static inline f32 AiVelLenSq2D(const nlVector3& v)
{
    const f32* p = (const f32*)&v;
    return nlGetLengthSquared2D(p[0], p[1]);
}

/**
 * Offset/Address/Size: 0xFA4 | 0x80006A50 | size: 0x11C
 */
void CalcInterceptXY(const nlVector3& pos1, f32 speed1, f32 speed2, const nlVector3& pos2, const nlVector3& vel, int& count, f32* times)
{
    struct Terms
    {
        f32 limbSq;
        f32 speedSq;
        f32 distSq;
        f32 dot;
        f32 velSq;
    };
    Terms terms;

    nlVector3 delta;
    nlVec3Sub2D(delta, pos2, pos1);

    terms.limbSq = speed2 * speed2;
    terms.speedSq = speed1 * speed1;
    terms.distSq = delta.GetLengthSq2D();
    terms.dot = nlVec3DotProduct2D(vel, delta);
    terms.velSq = AiVelLenSq2D(vel);

    f32 a = terms.velSq - terms.speedSq;

    if (terms.distSq <= terms.limbSq)
    {
        count = 1;
        times[0] = 0.0f;
        return;
    }

    f32 b = 2.0f * (terms.dot - speed2 * speed1);
    f32 c = terms.distSq - terms.limbSq;

    int numRoots;
    f32 t[2];
    SolveQuadratic(a, b, c, numRoots, t[0], t[1]);

    count = 0;
    if (numRoots == 0)
    {
        return;
    }

    for (int i = 0; i < 2; i++)
    {
        f32 root = t[i];
        if (root > 0.0f)
        {
            times[count] = root;
            count++;
        }
    }
}

/**
 * Offset/Address/Size: 0xED0 | 0x8000697C | size: 0xD4
 */
bool ClipPositionToSidelines(nlVector3& position, float margin)
{
    bool wasClipped = false;

    float rightBound = cField::GetGoalLineX(1U) - margin;
    float topBound = cField::GetSidelineY(1U) - margin;

    if (position.x > rightBound)
    {
        position.x = rightBound;
        wasClipped = true;
    }
    else
    {
        float leftBound = -1.0f * rightBound;
        if (position.x < leftBound)
        {
            position.x = leftBound;
            wasClipped = true;
        }
    }

    float bottomBound = -1.0f * topBound;
    if (position.y < bottomBound)
    {
        position.y = bottomBound;
        wasClipped = true;
    }
    else if (position.y > topBound)
    {
        position.y = topBound;
        wasClipped = true;
    }

    return wasClipped;
}

/**
 * Offset/Address/Size: 0xDF4 | 0x800068A0 | size: 0xDC
 */
bool TestCollision(float rp, const nlVector3& p1, const nlVector3& p2, float rq, const nlVector3& q1, const nlVector3& q2)
{
    float combinedRadius = rp + rq;
    float radiusSq = combinedRadius * combinedRadius;

    // Check start positions
    nlVector3 diff1;
    nlVec3Sub(diff1, q1, p1);
    float distSq1 = nlGetLengthSquared3D(diff1.x, diff1.y, diff1.z);

    if (distSq1 <= radiusSq)
    {
        return true;
    }

    // Check end positions
    nlVector3 diff2;
    nlVec3Sub(diff2, q2, p2);
    float distSq2 = nlGetLengthSquared3D(diff2.x, diff2.y, diff2.z);

    if (distSq2 <= radiusSq)
    {
        return true;
    }

    // Do sweep test
    float t = SweepSpheres(rp, p1, p2, rq, q1, q2);
    if (t >= 0.0f && t <= 1.0f)
    {
        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0xD90 | 0x8000683C | size: 0x64
 */
float Exp(float k)
{
    float result = 1.0f;
    float negk = -k;
    float c0 = 0.24999869f;
    float c1 = 0.031257585f;
    float powk;
    float c2 = 0.0025913713f;
    float c3 = 0.000171562f;
    float c4 = 0.0000054302f;
    float c5 = 0.0000006906f;
    float t0;
    float t1;
    float t2;

    result = c0 * negk + result;
    powk = negk * negk;
    result = c1 * powk + result;
    powk *= negk;
    result = c2 * powk + result;
    powk *= negk;
    result = c3 * powk + result;
    powk *= negk;
    result = c4 * powk + result;
    powk *= negk;
    result = c5 * powk + result;

    t0 = result * result;
    t1 = result * t0;
    t2 = result * t1;

    return 1.0f / t2;
}

/**
 * Offset/Address/Size: 0xCE8 | 0x80006794 | size: 0xA8
 */
float GetClosingSpeed2D(const nlVector3& pos1, const nlVector3& vel1, const nlVector3& pos2, const nlVector3& vel2)
{
    float dx, dy;
    dy = pos2.y - pos1.y;
    dx = pos2.x - pos1.x;
    float distSq = dx * dx + dy * dy;
    float invDist = nlRecipSqrt(distSq, true);

    float normDx = invDist * dx;
    float normDy = invDist * dy;

    float vel1Proj = normDx * vel1.x + normDy * vel1.y;
    float vel2Proj = normDx * vel2.x + normDy * vel2.y;

    return vel1Proj - vel2Proj;
}

/**
 * Offset/Address/Size: 0xC0C | 0x800066B8 | size: 0xDC
 */
float GetClosingSpeed(const nlVector3& pos1, const nlVector3& vel1, const nlVector3& pos2, const nlVector3& vel2)
{
    float dx, dy, dz;
    dz = pos2.z - pos1.z;
    dy = pos2.y - pos1.y;
    dx = pos2.x - pos1.x;
    float distSq = dx * dx + dy * dy + dz * dz;
    float invDist = nlRecipSqrt(distSq, true);

    float normDx = invDist * dx;
    float normDy = invDist * dy;
    float normDz = invDist * dz;

    float vel1Proj = normDx * vel1.x + normDy * vel1.y + normDz * vel1.z;
    float vel2Proj = normDx * vel2.x + normDy * vel2.y + normDz * vel2.z;

    return vel1Proj - vel2Proj;
}

/**
 * Offset/Address/Size: 0xB74 | 0x80006620 | size: 0x98
 */
void GetLocalPoint(nlVector3& v3LocalPointOut, const nlVector3& v3WorldPointIn, const nlVector3& v3RefPosition, unsigned short aRefAngle)
{
    float fSin; // r1+0xC
    float fCos; // r1+0x8

    nlSinCos(&fSin, &fCos, aRefAngle);
    float dx = v3WorldPointIn.x - v3RefPosition.x;
    float dy = v3WorldPointIn.y - v3RefPosition.y;

    v3LocalPointOut.x = (fCos * dx) + (fSin * dy);
    v3LocalPointOut.y = (fCos * dy) - (fSin * dx);
    v3LocalPointOut.z = v3WorldPointIn.z;
}

/**
 * Offset/Address/Size: 0xABC | 0x80006568 | size: 0xB8
 */
void GetWorldPoint(nlVector3& v3WorldPointOut, const nlVector3& v3LocalPointIn, const nlVector3& v3RefPosition, unsigned short aRefAngle)
{
    f32 fSin;
    f32 fCos;

    float localX = v3LocalPointIn.x;
    float localY = v3LocalPointIn.y;

    nlSinCos(&fSin, &fCos, aRefAngle);

    v3WorldPointOut.x = v3RefPosition.x + ((fCos * localX) - (fSin * localY));
    v3WorldPointOut.y = v3RefPosition.y + ((fCos * localY) + (fSin * localX));
    v3WorldPointOut.z = v3LocalPointIn.z;
}

/**
 * Offset/Address/Size: 0xA24 | 0x800064D0 | size: 0x98
 */
void RotateVectorZAxis(nlVector3& v3Out, const nlVector3& v3In, unsigned short angle)
{
    f32 fSin;
    f32 fCos;

    f32 localX = v3In.x;
    f32 localY = v3In.y;

    nlSinCos(&fSin, &fCos, angle);

    v3Out.x = (fCos * localX) - (fSin * localY);
    v3Out.y = (fCos * localY) + (fSin * localX);
    v3Out.z = v3In.z;
}

/**
 * Offset/Address/Size: 0x814 | 0x800062C0 | size: 0x210
 */
void GetRotationBetweenVectors(nlQuaternion& quat, const nlVector3& v3Vec1, const nlVector3& v3Vec2)
{
    float cz;
    float cy;
    float cx;
    float fInvR1R2 = 1.0f / nlSqrt(v3Vec1.GetLengthSq3D() * v3Vec2.GetLengthSq3D(), true);
    float fCosAngle = fInvR1R2 * nlVec3DotProduct(v3Vec1, v3Vec2);

    if (fCosAngle > 0.99999f)
    {
        nlQuatIdentity(quat);
    }
    else if (fCosAngle < -0.99999f)
    {
        nlVector3 axis;
        axis.x = 1.0f;

        if (v3Vec1.x > v3Vec1.z || v3Vec1.y > v3Vec1.z)
        {
            axis.x = 0.0f;
            axis.z = 1.0f;
            axis.y = axis.x;
        }
        else
        {
            axis.y = 0.0f;
            axis.z = axis.y;
        }

        cx = axis.y * v3Vec1.z - axis.z * v3Vec1.y;
        cy = -axis.x * v3Vec1.z + axis.z * v3Vec1.x;
        cz = axis.x * v3Vec1.y - axis.y * v3Vec1.x;

        float invLen = nlRecipSqrt(cx * cx + cy * cy + cz * cz, true);

        quat.x = invLen * cx;
        quat.y = invLen * cy;
        quat.z = invLen * cz;
        quat.w = 0.0f;
    }
    else
    {
        float fMagic = nlSqrt((float)(2.0 * (1.0 + fCosAngle)), true);
        float fMultiplier = fInvR1R2 / fMagic;

        cx = v3Vec1.y * v3Vec2.z - v3Vec1.z * v3Vec2.y;
        cy = -v3Vec1.x * v3Vec2.z + v3Vec1.z * v3Vec2.x;
        cz = v3Vec1.x * v3Vec2.y - v3Vec1.y * v3Vec2.x;
        quat.w = 0.5f * fMagic;
        quat.x = cx * fMultiplier;
        quat.y = cy * fMultiplier;
        quat.z = cz * fMultiplier;
    }
}

/**
 * Offset/Address/Size: 0x70C | 0x800061B8 | size: 0x108
 */
void RotateVector(nlVector3& result, const nlVector3& v, nlQuaternion& q)
{
    f32 xz2;
    f32 R;
    f32 xw2;
    f32 P;
    f32 zz;
    f32 S;
    f32 G;
    f32 N;
    f32 z2;
    f32 C;
    f32 K;
    f32 y2;
    f32 O;
    f32 A;
    f32 H;
    f32 yw2;
    f32 xy2;
    f32 yz2;
    f32 vx;
    f32 I;
    f32 xx;
    f32 T;
    f32 J;
    f32 L;
    f32 F;
    f32 yy;
    f32 D;
    f32 M;
    f32 zw2;
    f32 vy;
    f32 vz;
    f32 ww;
    f32 E;
    f32 U;
    f32 x2;
    f32 B;

    xx = q.x * q.x;
    zz = q.z * q.z;
    ww = q.w * q.w;
    x2 = 2.0f * q.x;
    z2 = 2.0f * q.z;
    vy = v.y;
    y2 = 2.0f * q.y;
    vx = v.x;
    yy = q.y * q.y;

    A = ww - xx;
    B = xx + ww;
    vz = v.z;

    xy2 = x2 * q.y;
    zw2 = z2 * q.w;
    C = yy + A;
    D = B - yy;
    E = xy2 - zw2;
    F = C - zz;
    G = A - yy;
    xw2 = x2 * q.w;
    yz2 = y2 * q.z;
    xz2 = x2 * q.z;
    yw2 = y2 * q.w;
    H = xw2 + yz2;
    I = D - zz;
    J = vy * E;
    K = zw2 + xy2;
    L = vy * F;
    M = xz2 + yw2;
    N = vx * I + J;
    O = xz2 - yw2;
    P = vy * H;
    result.x = vz * M + N;
    R = yz2 - xw2;
    S = vx * K + L;
    T = zz + G;
    U = vx * O + P;
    result.y = vz * R + S;
    result.z = vz * T + U;
}

/**
 * Offset/Address/Size: 0x700 | 0x800061AC | size: 0xC
 */
float Interpolate(float fMin, float fMax, float fPercent)
{
    return (fPercent * (fMax - fMin)) + fMin;
}

/**
 * Offset/Address/Size: 0x670 | 0x8000611C | size: 0x90
 */
float InterpolateRangeClamped(float fResultMin, float fResultMax, float fInputMin, float fInputMax, float fInput)
{
    float range;

    if (fInputMin < fInputMax)
    {
        fInput = (fInput >= fInputMin) ? fInput : fInputMin;
        fInput = (fInput <= fInputMax) ? fInput : fInputMax;
    }
    else
    {
        fInput = (fInput >= fInputMax) ? fInput : fInputMax;
        fInput = (fInput <= fInputMin) ? fInput : fInputMin;
    }

    range = fInputMax - fInputMin;
    if (fabsf(range) < 0.00001f)
    {
        return fResultMax;
    }

    return fResultMin + ((fInput - fInputMin) / range) * (fResultMax - fResultMin);
}

/**
 * Offset/Address/Size: 0x5D0 | 0x8000607C | size: 0xA0
 */
float InterpolateRangeClamped(const nlVector2& outputRange, const nlVector2& inputRange, float value)
{
    float maxVal = inputRange.y;
    float minVal = inputRange.x;
    float outMax = outputRange.y;
    float outMin = outputRange.x;

    // Clamp value to input range
    if (minVal < maxVal)
    {
        value = (value >= minVal) ? value : minVal;
        value = (value <= maxVal) ? value : maxVal;
    }
    else
    {
        value = (value >= maxVal) ? value : maxVal;
        value = (value <= minVal) ? value : minVal;
    }

    float range = maxVal - minVal;
    if ((float)__fabs((double)range) < 0.00001f)
    {
        return outMax;
    }

    return (value - minVal) / range * (outMax - outMin) + outMin;
}

/**
 * Offset/Address/Size: 0x580 | 0x8000602C | size: 0x50
 */
float NormalizeVal(float fromVal, const nlVector2& fromExtrema)
{
    if (fromExtrema.y == fromExtrema.x)
        return 1.0f;

    float t = (fromVal - fromExtrema.x) / (fromExtrema.y - fromExtrema.x);

    float lo = (t >= 0.0f) ? t : 0.0f;
    float res = (lo <= 1.0f) ? lo : 1.0f;

    return res;
}

/**
 * Offset/Address/Size: 0x538 | 0x80005FE4 | size: 0x48
 */
float NormalizeVal(float fromVal, float fromMin, float fromMax)
{
    if (fromMin == fromMax)
        return 1.0f;

    float t = (fromVal - fromMin) / (fromMax - fromMin);

    float lo = (t >= 0.0f) ? t : 0.0f;
    float res = (lo <= 1.0f) ? lo : 1.0f;

    return res;
}

/**
 * Offset/Address/Size: 0x518 | 0x80005FC4 | size: 0x20
 */
float AIsgn(float fValue)
{
    if (fValue >= 0.0f)
    {
        return 1.0f;
    }
    return -1.0f;
}

/**
 * Offset/Address/Size: 0x2E4 | 0x80005D90 | size: 0x234
 */
nlVector3 GetClosestPointOnLineABFromPointC(const nlVector3& a, const nlVector3& b, const nlVector3& c)
{
    nlVector3 ac;
    nlVector3 ab;
    nlVec3Sub(ac, c, a);
    nlVec3Sub(ab, b, a);

    f32 dot = nlVec3DotProduct(ac, ab);
    f32 t = dot / ab.GetLengthSq3D();

    nlVector3 scaledAb;
    nlVec3Scale(scaledAb, ab, t);

    nlVector3 reconstructed;
    nlVec3Add(reconstructed, a, ac);

    nlVector3 offset;
    nlVec3Sub(offset, ac, scaledAb);

    nlVector3 projected;
    nlVec3Sub(projected, reconstructed, offset);

    nlVector3 toA;
    nlVector3 toB;
    nlVec3Sub(toA, a, projected);
    nlVec3Sub(toB, b, projected);

    const f32 fLenSq = ab.GetLengthSq3D();
    f32 lineLen = nlSqrt(fLenSq, true);
    f32 distASq = toA.GetLengthSq3D();
    f32 distA = nlSqrt(distASq, true);

    f32 lineLen2;
    f32 distBSq;
    f32 distB;
    if (distA > lineLen || (lineLen2 = nlSqrt(fLenSq, true), distBSq = toB.GetLengthSq3D(), distB = nlSqrt(distBSq, true), distB > lineLen2))
    {
        f32 distBSq2 = toB.GetLengthSq3D();
        f32 distB2 = nlSqrt(distBSq2, true);
        f32 distA2 = nlSqrt(distASq, true);
        if (distA2 < distB2)
        {
            return a;
        }
        return b;
    }
    return projected;
}

/**
 * Offset/Address/Size: 0x160 | 0x80005C0C | size: 0x184
 */
void SortToMinOrMaxTotalSum(unsigned int* result, float (*data)[4], bool findMin)
{
    f32 bestSum;

    if (findMin)
    {
        bestSum = 10000000000.0f;
    }
    else
    {
        bestSum = -10000000000.0f;
    }

    for (s32 i = 0; i < 4; i++)
    {
        f32 sum = data[0][i];
        for (s32 j = 0; j < 4; j++)
        {
            if (j == i)
                continue;
            f32 save2 = sum;
            sum += data[1][j];
            for (s32 k = 0; k < 4; k++)
            {
                if (k == j)
                    continue;
                if (k == i)
                    continue;
                f32 save3 = sum;
                sum += data[2][k];
                for (s32 l = 0; l < 4; l++)
                {
                    if (l == k)
                        continue;
                    if (l == j)
                        continue;
                    if (l == i)
                        continue;
                    f32 save4 = sum;
                    sum += data[3][l];
                    if ((findMin && sum < bestSum) || (!findMin && sum > bestSum))
                    {
                        result[0] = i;
                        result[1] = j;
                        result[2] = k;
                        result[3] = l;
                        bestSum = sum;
                    }
                    sum = save4;
                }
                sum = save3;
            }
            sum = save2;
        }
    }
}

/**
 * Offset/Address/Size: 0x134 | 0x80005BE0 | size: 0x2C
 */
char* GetPowerupName(int powerup)
{
    if ((powerup >= 0) && (powerup < 9))
    {
        return g_sPowerupNames[powerup];
    }
    return "Unknown";
}

/**
 * Offset/Address/Size: 0x0 | 0x80005AAC | size: 0x134
 */
nlVector3 GetClosestPointOnSidelines(const nlVector3& v3Position)
{
    nlVector3 v3WallPosition;
    f32 fSign;
    f32 fDistToGoalLine;
    f32 fDistToSideline;

    fDistToGoalLine = (f32)fabs(cField::GetGoalLineX(1U) - (f32)fabs(v3Position.x));
    fDistToSideline = (f32)fabs(cField::GetSidelineY(1U) - (f32)fabs(v3Position.y));

    if (fDistToGoalLine <= fDistToSideline)
    {
        v3WallPosition.y = v3Position.y;
        if (v3Position.x >= 0.0f)
        {
            fSign = 1.0f;
        }
        else
        {
            fSign = -1.0f;
        }
        f32 fGoalX = cField::GetGoalLineX(1U);
        v3WallPosition.x = fGoalX * fSign;
    }
    else
    {
        if (v3Position.y >= 0.0f)
        {
            fSign = 1.0f;
        }
        else
        {
            fSign = -1.0f;
        }
        f32 fSideY = cField::GetSidelineY(1U);
        v3WallPosition.y = fSideY * fSign;
        v3WallPosition.x = v3Position.x;
    }

    v3WallPosition.z = 0.0f;
    return v3WallPosition;
}
