#include "NL/nlMath.h"
#include "Game/Core/mtRandom.h"

#include "math.h"

unsigned int nlDefaultSeed = 0x12345678;

struct coeffs
{
    float values[8];
};

struct nlACosConstants
{
    float radicandScale;
};

struct nlAngleConstants
{
    float unitsToRadians;
};

namespace
{
extern const nlAngleConstants nlAngleConstantsData;
}

static const volatile float nlHalfPi = M_PI / 2.0f;

/**
 * Offset/Address/Size: 0x0 | 0x801D1474 | size: 0x1D4
 */
float nlBezier(float* fControlPoints, int nNumPoints, float fMu)
{
    float oneMinusMu;
    float powVal;
    float result;
    float oneMinusMuToNMinusK;
    float muToK;
    float blend;
    float powTerm;
    int nFactorial;
    int kFactorial;
    int i;
    float* currentPoint;
    int nMinusKFactorial;
    int k;

    if (fMu == 1.0f)
    {
        return fControlPoints[nNumPoints - 1];
    }

    oneMinusMu = 1.0f - fMu;
    nFactorial = 1;
    kFactorial = 1;
    powVal = pow(oneMinusMu, (float)nNumPoints);
    oneMinusMuToNMinusK = powVal;
    muToK = 1.0f;

    for (i = 1; i <= nNumPoints; i++)
    {
        nFactorial *= i;
    }

    nMinusKFactorial = nFactorial;
    currentPoint = fControlPoints + 1;
    result = *fControlPoints * powVal;

    for (k = 1; k <= nNumPoints; k++)
    {
        kFactorial *= k;
        if (k != nNumPoints)
        {
            nMinusKFactorial /= (nNumPoints - k) + 1;
        }

        oneMinusMuToNMinusK /= oneMinusMu;
        muToK *= fMu;
        blend = (float)(nFactorial / (kFactorial * nMinusKFactorial));
        powTerm = muToK * oneMinusMuToNMinusK;
        blend = blend * powTerm;
        blend = blend * *currentPoint;
        result = result + blend;
        currentPoint += 1;
    }

    return result;
}

static inline void nlATanLookup(float value, float& result)
{
    coeffs low = {
        0.0f, 0.004072621f, 0.0178999677f, 0.0447405465f, 0.0844737813f, 0.134708926f, 0.192103297f, 0.253371507f
    };
    coeffs high = {
        0.997413278f, 0.964989364f, 0.910336077f, 0.839015484f, 0.759613633f, 0.679214358f, 0.602631152f, 0.532545328f
    };

    int index = (int)(8.0f * value);
    index = index <= 7 ? index : 7;
    result = value * high.values[index] + low.values[index];
}

static inline float nlATan(float x)
{
    float value = x;
    float sign;

    if (value > 1.0f)
    {
        sign = 1.0f / value;
        float result;
        nlATanLookup(sign, result);
        return nlHalfPi - result;
    }
    float result;
    nlATanLookup(value, result);
    return result;
}

/**
 * Offset/Address/Size: 0x1D4 | 0x801D1648 | size: 0x244
 */
float nlATan2f(float y, float x)
{
    f32 angle;

    if ((float)fabs(x) > 0.00001f)
    {
        angle = nlATan((float)fabs(y / x));
        if (y >= 0.0f)
        {
            if (x >= 0.0f)
                return angle;
            return 3.1415927f - angle;
        }
        if (x < 0.0f)
            return 3.1415927f + angle;
        return 6.2831855f - angle;
    }
    if (y > 0.0f)
        return 1.5707964f;
    return 4.712389f;
}

DECL_SECT(".sdata2")
static const nlACosConstants nlACosConstantsData = { 2.0f };

/**
 * Offset/Address/Size: 0x418 | 0x801D188C | size: 0x48
 */
float nlTan(unsigned short angle)
{
    return (float)tan(nlAngleConstantsData.unitsToRadians * angle);
}

namespace
{
DECL_SECT(".sdata2")
const nlAngleConstants nlAngleConstantsData = { 0.0000958738f };
} // namespace

static inline f32 nlACosMax(f32 a, f32 b)
{
    return (a >= b) ? a : b;
}

/**
 * Offset/Address/Size: 0x460 | 0x801D18D4 | size: 0x12C
 */
int nlACos(float x)
{
    u8 complement = (x < 0.0f);
    f32 y, sqrtVal, rad;

    x = 1.0f - (f32)fabs(x);

    y = x * (x * (x * (x * (0.015098966f * x + 0.005516444f) + 0.047654245f) + 0.16391061f) + 2.0002916f)
      + -0.000007239284f;

    rad = nlACosMax(nlACosConstantsData.radicandScale * x, 0.00001f);

    if (rad > 0.0f)
    {
        f32 t = (f32)__frsqrte(rad);
        f32 t2 = 0.5f * t * -(rad * (t * t) - 3.0f);
        f32 t3 = 0.5f * t2 * -(rad * (t2 * t2) - 3.0f);
        sqrtVal = 0.5f * t3 * -(rad * (t3 * t3) - 3.0f);
    }
    else
    {
        f32 zero = 0.0f;
        if (rad != zero)
        {
            sqrtVal = *(f32*)&__float_nan;
        }
        else
        {
            sqrtVal = *(f32*)&__float_huge;
        }
    }

    rad = y * sqrtVal;

    if (complement)
    {
        return (unsigned short)(s32)(10430.378f * (3.1415927f - rad));
    }
    return (unsigned short)(s32)(10430.378f * rad);
}

/**
 * Offset/Address/Size: 0x58C | 0x801D1A00 | size: 0x164
 */
void nlSinCos(float* presult_sin, float* presult_cos, unsigned short angle)
{
    float angle_rad = nlAngleConstantsData.unitsToRadians * (float)angle;
    float octants = 1.2732395f * angle_rad;
    int k = (int)octants;
    float oct_f = (float)k;
    float y = angle_rad - 0.7853982f * oct_f;

    float y_squared = y * y;
    float s0 = -0.00019516895f;
    float s1 = 0.008332208f;
    float s2 = -0.16666657f;
    float c0 = -0.0013400711f;
    float c1 = 0.04163633f;
    float s3 = 1.0f;
    float c2 = -0.49999395f;
    float c3 = 0.9999998f;

    float sin_poly = y * (y_squared * (y_squared * (s0 * y_squared + s1) + s2) + s3);
    float cos_poly = y_squared * (y_squared * (c0 * y_squared + c1) + c2) + c3;

    float result_cos;
    float result_sin;

    switch (k & 7)
    {
    case 0:
        result_sin = sin_poly;
        result_cos = cos_poly;
        break;
    case 1:
    {
        float c = 0.70710677f;
        float s_1 = c * sin_poly;
        float s_2 = c * cos_poly;
        result_sin = s_1 + s_2;
        result_cos = s_2 - s_1;
        break;
    }
    case 2:
        result_sin = cos_poly;
        result_cos = -sin_poly;
        break;
    case 3:
    {
        float a = 0.70710677f;
        float b = -0.70710677f;
        float t1 = a * cos_poly;
        float t2 = a * sin_poly;
        result_sin = b * sin_poly + t1;
        result_cos = b * cos_poly - t2;
        break;
    }
    case 4:
        result_sin = -sin_poly;
        result_cos = -cos_poly;
        break;
    case 5:
    {
        float c = -0.70710677f;
        float s_1 = c * sin_poly;
        float s_2 = c * cos_poly;
        result_sin = s_1 + s_2;
        result_cos = s_2 - s_1;
        break;
    }
    case 6:
        result_sin = -cos_poly;
        result_cos = sin_poly;
        break;
    case 7:
    {
        float a = -0.70710677f;
        float b = 0.70710677f;
        float t1 = a * cos_poly;
        float t2 = a * sin_poly;
        result_sin = b * sin_poly + t1;
        result_cos = b * cos_poly - t2;
        break;
    }
    }

    *presult_sin = result_sin;
    *presult_cos = result_cos;
}

/**
 * Offset/Address/Size: 0x6F0 | 0x801D1B64 | size: 0xA0
 */
float nlSin(unsigned short angle)
{
    float a = nlAngleConstantsData.unitsToRadians * (float)angle;
    float working_a = a;
    float flip_sign = 1.0f;

    if (a >= 4.7123889f) // 3*PI/2
    {
        working_a = a - 6.2831855f; // 2*PI
    }
    else if (a >= 1.5707964f) // PI/2
    {
        flip_sign = -flip_sign;
        working_a = a - 3.14159265f; // PI
    }

    // Taylor series
    float a_squared = working_a * working_a;
    float result = working_a * (1.0f + a_squared * (-0.16666667f + a_squared * (0.0083333337f + a_squared * (-0.0001984127f + a_squared * 0.0000027557319f))));

    return flip_sign * result;
}

/**
 * Offset/Address/Size: 0x790 | 0x801D1C04 | size: 0x70
 */
float nlRecipSqrt(float x, bool bAccurate)
{
    float zero = 0.0f;
    if (x > zero)
    {
        float y = __frsqrte(x);
        y = 0.5f * y * (3.0f - y * y * x);
        y = 0.5f * y * (3.0f - y * y * x);
        y = 0.5f * y * (3.0f - y * y * x);
        return y;
    }
    else if (x != zero)
    {
        return NAN;
    }
    else
    {
        return INFINITY;
    }
}

/**
 * Offset/Address/Size: 0x800 | 0x801D1C74 | size: 0xEC
 */
float nlSqrt(float x, bool bAccurate)
{
    if (x > 0.0f)
    {
        double guess = __frsqrte((double)x);
        guess = 0.5 * guess * (3.0 - guess * guess * x);
        if (bAccurate)
        {
            guess = 0.5 * guess * (3.0 - guess * guess * x);
            guess = 0.5 * guess * (3.0 - guess * guess * x);
        }
        return (float)(x * guess);
    }
    else if (x < 0.0)
    {
        return NAN;
    }
    else
    {
        // PORT: __fpclassifyf is Apple-only; fpclassify is C99 and everywhere.
        if (fpclassify(x) == FP_NAN)
        {
            return NAN;
        }
        return x;
    }
}

/**
 * Offset/Address/Size: 0x8EC | 0x801D1D60 | size: 0x84
 */
float nlRandomf(float fMin, float fMax, unsigned int* pSeed)
{
    unsigned int next;
    unsigned int temp;
    unsigned int mod;
    unsigned int seed;

    float range = fMax - fMin;
    float scale = (1.0f / 2147483647.0f) * range;
    mod = (seed = *pSeed) % 0x7FFFFFFFu;
    next = seed ^ 0x1d872b41;
    temp = next ^ (next >> 5);
    *pSeed = temp ^ (next ^ (temp << 0x1b));
    f32 fVal = scale * (f32)mod;
    return fMin + fVal;
}

/**
 * Offset/Address/Size: 0x970 | 0x801D1DE4 | size: 0x7C
 */
float nlRandomf(float fMax, unsigned int* pSeed)
{
    unsigned int next;
    unsigned int temp;
    unsigned int mod;
    unsigned int seed;

    mod = (seed = *pSeed) % 0x7FFFFFFFu;
    next = seed ^ 0x1d872b41;
    temp = next ^ (next >> 5);
    *pSeed = temp ^ (next ^ (temp << 0x1b));
    return (1.0f / 2147483647.0f) * fMax * (f32)mod;
}

/**
 * Offset/Address/Size: 0x9EC | 0x801D1E60 | size: 0x34
 */
uint nlRandom(uint range, uint* seed)
{
    uint next;
    uint temp;

    /* PORT: PowerPC's divwu does not trap on a zero divisor and x86's div does. */
    uint result = (range != 0) ? (*seed % range) : 0u;
    next = *seed ^ 0x1d872b41;
    temp = next ^ (next >> 5);
    *seed = temp ^ (next ^ (temp << 0x1b));
    return result;
}

/**
 * Offset/Address/Size: 0xA20 | 0x801D1E94 | size: 0x2C
 */
void nlSetRandomSeed(uint value, uint* seed)
{
    uint temp;
    uint next;

    *seed = value;

    next = *seed ^ 0x1d872b41;
    temp = next ^ (next >> 5);

    *seed = temp ^ (next ^ (temp << 0x1b));
}

/**
 * Offset/Address/Size: 0xA4C | 0x801D1EC0 | size: 0x24
 */
void nlInitRandom()
{
    seedMT(0x1105);
}

unsigned long nlMoreRandom()
{
    return randomMT();
}
