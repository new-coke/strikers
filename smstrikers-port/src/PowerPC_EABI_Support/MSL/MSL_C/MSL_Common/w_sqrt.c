#include "fdlibm.h"

/* 8036CA54-8036CA74 367394 0020+00 0/0 8/8 1/1 .text            sqrt */
/**
 * Offset/Address/Size: 0x0 | 0x80239FE0 | size: 0x20
 */
double sqrt(double x)
{
    return __ieee754_sqrt(x);
}
