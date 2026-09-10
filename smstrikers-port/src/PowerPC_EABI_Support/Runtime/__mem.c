#include "PowerPC_EABI_Support/Runtime/__mem.h"

/**
 * Offset/Address/Size: 0x0 | 0x800050B4 | size: 0x30
 */
__declspec(section ".init") void* memset(void* dst, int val, size_t n)
{
    __fill_mem(dst, val, n);

    return dst;
}

/**
 * Offset/Address/Size: 0x30 | 0x800050E4 | size: 0xB8
 */
__declspec(section ".init") void __fill_mem(void* dst, int val, size_t n)
{
    u32 v = (u8)val;
    u32 i;

    ((u8*)dst) = ((u8*)dst) - 1;

    if (n >= 32)
    {
        i = (~(u32)dst) & 3;

        if (i)
        {
            n -= i;

            do
            {
                *++(((u8*)dst)) = v;
            } while (--i);
        }

        if (v)
            v |= v << 24 | v << 16 | v << 8;

        ((u32*)dst) = ((u32*)(((u8*)dst) + 1)) - 1;

        i = n >> 5;

        if (i)
        {
            do
            {
                *++((u32*)dst) = v;
                *++((u32*)dst) = v;
                *++((u32*)dst) = v;
                *++((u32*)dst) = v;
                *++((u32*)dst) = v;
                *++((u32*)dst) = v;
                *++((u32*)dst) = v;
                *++((u32*)dst) = v;
            } while (--i);
        }

        i = (n & 31) >> 2;

        if (i)
        {
            do
            {
                *++((u32*)dst) = v;
            } while (--i);
        }

        ((u8*)dst) = ((u8*)(((u32*)dst) + 1)) - 1;

        n &= 3;
    }

    if (n)
        do
        {
            *++((u8*)dst) = v;
        } while (--n);

    return;
}

/**
 * Offset/Address/Size: 0xE8 | 0x8000519C | size: 0x50
 */
__declspec(section ".init") void* memcpy(void* dst, const void* src, size_t n)
{
    const u8* s;
    u8* d;

    if ((u32)src >= (u32)dst)
    {
        s = (const u8*)src - 1;
        d = (u8*)dst - 1;
        n++;
        while (--n != 0)
        {
            *++d = *++s;
        }
    }
    else
    {
        s = (const u8*)src + n;
        d = (u8*)dst + n;
        n++;
        while (--n != 0)
        {
            *--d = *--s;
        }
    }
    return dst;
}
