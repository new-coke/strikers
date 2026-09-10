#include "Game/Core/mtRandom.h"

#include "types.h"

static unsigned long mt[624];
static int mti = 0x271;

unsigned long randomMT()
{
    unsigned long y;
    static unsigned long mag01[2] = { 0x0UL, 0x9908b0dfUL };

    if (mti >= 624)
    {
        int kk;

        if (mti == 625)
            seedMT(5489UL);

        for (kk = 0; kk < 624 - 397; kk++)
        {
            y = (mt[kk] & 0x80000000UL) | (mt[kk + 1] & 0x7fffffffUL);
            mt[kk] = mt[kk + 397] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (; kk < 623; kk++)
        {
            y = (mt[kk] & 0x80000000UL) | (mt[kk + 1] & 0x7fffffffUL);
            mt[kk] = mt[kk + (397 - 624)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[623] & 0x80000000UL) | (mt[0] & 0x7fffffffUL);
        mt[623] = mt[396] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mti = 0;
    }

    y = mt[mti++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

/**
 * Offset/Address/Size: 0x0 | 0x801CDFB8 | size: 0x168
 */
void seedMT(unsigned long seed)
{
    mt[0] = seed;

    for (mti = 1; mti < 624; ++mti)
    {
        mt[mti] = 1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti;
    }
}
