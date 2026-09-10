#include "NL/nlEndian.h"

#include "types.h"

/**
 * Offset/Address/Size: 0x0 | 0x801D2C38 | size: 0x2C
 */
void nlSwapEndian(unsigned short in, unsigned short* out)
{
    *out = 0;
    *out |= (in >> 8U) & 0xFF;
    *out |= (in << 8) & 0xFF00;
}
