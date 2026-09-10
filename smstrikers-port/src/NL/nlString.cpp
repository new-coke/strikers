#include "NL/nlString.h"
#include <string.h>
#include <dolphin/os/OSCache.h>

/**
 * Offset/Address/Size: 0x0 | 0x801D2638 | size: 0x48
 */
void nlStrToWcs(const char* str, unsigned short* wstr, unsigned long maxLen)
{
    unsigned short* dest;
    unsigned long remaining;

    remaining = maxLen;
    dest = wstr;

    while (remaining-- != 0 && (*dest = (s16)*str) != 0)
    {
        dest++;
        str++;
    }
    wstr[maxLen - 1] = 0;
}

/**
 * Offset/Address/Size: 0x48 | 0x801D2680 | size: 0x140
 */
void nlZeroMemory(void* p, unsigned long numBytes)
{
    if (numBytes >= 0x80)
    {
        uintptr_t addr = (uintptr_t)p;
        uintptr_t endr = addr + numBytes;
        uintptr_t startAddr = (addr + 0x1F) & ~0x1F;
        uintptr_t endAddr = endr & ~0x1F;

        // Zero leading unaligned bytes
        while (addr < startAddr)
        {
            *(unsigned char*)addr = 0;
            addr++;
        }

        // DCZeroRange for aligned middle
        if (endAddr > startAddr)
        {
            DCZeroRange((void*)startAddr, endAddr - startAddr);
        }

        // Zero trailing unaligned bytes
        while (endAddr < endr)
        {
            *(unsigned char*)endAddr = 0;
            endAddr++;
        }
    }
    else
    {
        memset(p, 0, numBytes);
    }
}

/**
 * Offset/Address/Size: 0x188 | 0x801D27C0 | size: 0x64
 */
u32 nlStringLowerHash(const char* str)
{
    u32 h = (u32)-1;
    while (*str)
    {
        h += h << 5;
        h += nlToLower<unsigned char>(*(unsigned char*)str++);
    }
    return h;
}

/**
 * Offset/Address/Size: 0x1EC | 0x801D2824 | size: 0x30
 */
u32 nlStringHash(const char* str)
{
    u32 h = -1;
    while (*str)
    {
        h += (h << 5);
        h += (unsigned char)*str++;
    }
    return h;
}
