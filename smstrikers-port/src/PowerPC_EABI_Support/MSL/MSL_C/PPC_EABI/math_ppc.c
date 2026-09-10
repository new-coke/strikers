#include "PowerPC_EABI_Support/MSL_C/PPC_EABI/math_ppc.h"
#include "fdlibm.h"

__declspec(weak) int __fpclassifyf(float x)
{
    switch ((*(int*)&x) & 0x7f800000)
    {
    case 0x7f800000:
        if ((*(int*)&x) & 0x007fffff)
            return 1;
        else
            return 2;
    case 0:
        if ((*(int*)&x) & 0x007fffff)
            return 5;
        else
            return 3;
    }
    return 4;
}

__declspec(weak) int __fpclassifyd(double x)
{
    switch (__HI(x) & 0x7ff00000)
    {
    case 0x7ff00000:
        if ((__HI(x) & 0x000fffff) || (__LO(x) & 0xffffffff))
            return 1;
        else
            return 2;
    case 0:
        if ((__HI(x) & 0x000fffff) || (__LO(x) & 0xffffffff))
            return 5;
        else
            return 3;
    }
    return 4;
}

__declspec(weak) double scalbn(double x, int n)
{
    return ldexp(x, n);
}

__declspec(weak) double scalbln(double x, long n)
{
    return ldexp(x, (int)n);
}

__declspec(weak) long double acosl(long double x)
{
    return acos(x);
}

__declspec(weak) long double asinl(long double x)
{
    return asin(x);
}

__declspec(weak) long double atanl(long double x)
{
    return atan(x);
}

__declspec(weak) long double atan2l(long double y, long double x)
{
    return atan2(y, x);
}

__declspec(weak) long double cosl(long double x)
{
    return cos(x);
}

__declspec(weak) long double sinl(long double x)
{
    return sin(x);
}

__declspec(weak) long double tanl(long double x)
{
    return tan(x);
}

__declspec(weak) long double coshl(long double x)
{
    return cosh(x);
}

__declspec(weak) long double sinhl(long double x)
{
    return sinh(x);
}

__declspec(weak) long double tanhl(long double x)
{
    return tanh(x);
}

__declspec(weak) long double acoshl(long double x)
{
    return acosh(x);
}

__declspec(weak) long double asinhl(long double x)
{
    return asinh(x);
}

__declspec(weak) long double atanhl(long double x)
{
    return atanh(x);
}

__declspec(weak) long double expl(long double x)
{
    return exp(x);
}

__declspec(weak) long double frexpl(long double x, int* exp)
{
    return frexp(x, exp);
}

__declspec(weak) long double ldexpl(long double x, int n)
{
    return ldexp(x, n);
}

__declspec(weak) long double logl(long double x)
{
    return log(x);
}

__declspec(weak) long double log10l(long double x)
{
    return log10(x);
}

__declspec(weak) long double modfl(long double x, long double* iptr)
{
    double i;
    long double f = modf(x, &i);
    *iptr = i;
    return f;
}

__declspec(weak) long double exp2l(long double x)
{
    return exp(x * 0.6931471805599453);
}

__declspec(weak) long double expm1l(long double x)
{
    return expm1(x);
}

__declspec(weak) long double log1pl(long double x)
{
    return log1p(x);
}

__declspec(weak) long double log2l(long double x)
{
    return log(x) * 1.4426950408889634;
}

__declspec(weak) long double logbl(long double x)
{
    return logb(x);
}

__declspec(weak) long double scalbnl(long double x, int n)
{
    return ldexp(x, n);
}

__declspec(weak) long double scalblnl(long double x, long n)
{
    return ldexp(x, (int)n);
}

__declspec(weak) long double fabsl(long double x)
{
    return __fabs(x);
}

__declspec(weak) long double powl(long double x, long double y)
{
    return pow(x, y);
}

__declspec(weak) long double sqrtl(long double x)
{
    return sqrt(x);
}

__declspec(weak) long double hypotl(long double x, long double y)
{
    return hypot(x, y);
}

__declspec(weak) long double erfl(long double x)
{
    return erf(x);
}

__declspec(weak) long double erfcl(long double x)
{
    return erfc(x);
}

__declspec(weak) long double gammal(long double x)
{
    return gamma(x);
}

__declspec(weak) long double lgammal(long double x)
{
    return lgamma(x);
}

__declspec(weak) long double nextafterl(long double x, long double y)
{
    return nextafter(x, y);
}

__declspec(weak) long double ceill(long double x)
{
    return ceil(x);
}

__declspec(weak) long double floorl(long double x)
{
    return floor(x);
}

__declspec(weak) long double nearbyintl(long double x)
{
    return nearbyint(x);
}

__declspec(weak) long double rintl(long double x)
{
    return rint(x);
}

__declspec(weak) long lrintl(long double x)
{
    return lrint(x);
}

__declspec(weak) long long llrintl(long double x)
{
    return llrint(x);
}

__declspec(weak) long double truncl(long double x)
{
    return trunc(x);
}

__declspec(weak) long double fmodl(long double x, long double y)
{
    return fmod(x, y);
}

__declspec(weak) long double remainderl(long double x, long double y)
{
    return remainder(x, y);
}

__declspec(weak) long double copysignl(long double x, long double y)
{
    return copysign(x, y);
}

__declspec(weak) long double remquol(long double x, long double y, int* quo)
{
    return remquo(x, y, quo);
}

__declspec(weak) long double fdiml(long double x, long double y)
{
    return fdim(x, y);
}

__declspec(weak) long double fmaxl(long double x, long double y)
{
    return fmax(x, y);
}

__declspec(weak) long double fminl(long double x, long double y)
{
    return fmin(x, y);
}

/**
 * Offset/Address/Size: 0x48 | 0x80239FBC | size: 0x24
 */
__declspec(weak) float acosf(float x)
{
    return acos(x);
}

__declspec(weak) float asinf(float x)
{
    return asin(x);
}

__declspec(weak) float atanf(float x)
{
    return atan(x);
}

__declspec(weak) float atan2f(float y, float x)
{
    return atan2(y, x);
}

__declspec(weak) float cosf(float x)
{
    return cos(x);
}

/**
 * Offset/Address/Size: 0x24 | 0x80239F98 | size: 0x24
 */
__declspec(weak) float sinf(float x)
{
    return sin(x);
}

/**
 * Offset/Address/Size: 0x0 | 0x80239F74 | size: 0x24
 */
__declspec(weak) float tanf(float x)
{
    return tan(x);
}

__declspec(weak) float coshf(float x)
{
    return cosh(x);
}

__declspec(weak) float sinhf(float x)
{
    return sinh(x);
}

__declspec(weak) float tanhf(float x)
{
    return tanh(x);
}

__declspec(weak) float expf(float x)
{
    return exp(x);
}

__declspec(weak) float frexpf(float x, int* exp)
{
    return frexp(x, exp);
}

__declspec(weak) float ldexpf(float x, int n)
{
    return ldexp(x, n);
}

__declspec(weak) float logf(float x)
{
    return log(x);
}

__declspec(weak) float log10f(float x)
{
    return log10(x);
}

__declspec(weak) float fabsf(float x)
{
    return __fabs(x);
}

__declspec(weak) float powf(float x, float y)
{
    return pow(x, y);
}

__declspec(weak) float ceilf(float x)
{
    return ceil(x);
}

__declspec(weak) float floorf(float x)
{
    return floor(x);
}

__declspec(weak) float fmodf(float x, float y)
{
    return fmod(x, y);
}

__declspec(weak) float log2f(float x)
{
    return log(x) * 1.4426950408889634;
}

__declspec(weak) double fabs(double x)
{
    return __fabs(x);
}

__declspec(weak) float acoshf(float x)
{
    return acosh(x);
}

__declspec(weak) float asinhf(float x)
{
    return asinh(x);
}

__declspec(weak) float atanhf(float x)
{
    return atanh(x);
}

__declspec(weak) float exp2f(float x)
{
    return exp(x * 0.6931471805599453);
}

__declspec(weak) float expm1f(float x)
{
    return expm1(x);
}

__declspec(weak) float log1pf(float x)
{
    return log1p(x);
}

__declspec(weak) float logbf(float x)
{
    return logb(x);
}

__declspec(weak) float scalbnf(float x, int n)
{
    return ldexp(x, n);
}

__declspec(weak) float scalblnf(float x, long n)
{
    return ldexp(x, (int)n);
}

__declspec(weak) float hypotf(float x, float y)
{
    return hypot(x, y);
}

__declspec(weak) float erff(float x)
{
    return erf(x);
}

__declspec(weak) float erfcf(float x)
{
    return erfc(x);
}

__declspec(weak) float gammaf(float x)
{
    return gamma(x);
}

__declspec(weak) float lgammaf(float x)
{
    return lgamma(x);
}

__declspec(weak) float nextafterf(float x, float y)
{
    return nextafter(x, y);
}

__declspec(weak) float nearbyintf(float x)
{
    return nearbyint(x);
}

__declspec(weak) float rintf(float x)
{
    return rint(x);
}

__declspec(weak) long lrintf(float x)
{
    return lrint(x);
}

__declspec(weak) long long llroundf(float x)
{
    return llround(x);
}

__declspec(weak) long long llrintf(float x)
{
    return llrint(x);
}

__declspec(weak) float truncf(float x)
{
    return trunc(x);
}

__declspec(weak) float remainderf(float x, float y)
{
    return remainder(x, y);
}

__declspec(weak) float copysignf(float x, float y)
{
    return copysign(x, y);
}

__declspec(weak) float remquof(float x, float y, int* quo)
{
    return remquo(x, y, quo);
}

__declspec(weak) float fdimf(float x, float y)
{
    return fdim(x, y);
}

__declspec(weak) float fmaxf(float x, float y)
{
    return fmax(x, y);
}

__declspec(weak) float fminf(float x, float y)
{
    return fmin(x, y);
}

__declspec(weak) double log2(double x)
{
    return log(x) * 1.4426950408889634;
}

__declspec(weak) double exp2(double x)
{
    return exp(x * 0.6931471805599453);
}
