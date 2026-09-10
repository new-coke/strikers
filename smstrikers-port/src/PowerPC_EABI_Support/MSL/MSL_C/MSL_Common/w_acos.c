#include "fdlibm.h"

/* 8036C6E0-8036C700 367020 0020+00 0/0 2/2 3/3 .text            acos */
/**
 * Offset/Address/Size: 0x0 | 0x80239CF0 | size: 0x20
 */
double acos(double x)
{
    return __ieee754_acos(x);
}
