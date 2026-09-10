#include "arith.h"

/**
 * Offset/Address/Size: 0x0 | 0x8022F8B4 | size: 0x10
 */
int abs(int n)
{
    if (n < 0)
        return (-n);
    else
        return (n);
}

long labs(long n)
{
    if (n < 0)
        return (-n);
    else
        return (n);
}

long long llabs(long long n)
{
    if (n < 0)
        return (-n);
    else
        return (n);
}
