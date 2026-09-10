#include "NL/nlPrint.h"

/**
 * Offset/Address/Size: 0x0 | 0x801D22F8 | size: 0x48
 */
int nlVSNPrintf(char* buffer, unsigned long size, const char* format, va_list args)
{
    int result = vsnprintf(buffer, size - 1, format, args);
    buffer[size - 1] = '\0';
    return result;
}

/**
 * Offset/Address/Size: 0x48 | 0x801D2340 | size: 0xB8
 */
int nlSNPrintf(char* buffer, unsigned long size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size - 1, format, args);
    va_end(args);
    buffer[size - 1] = '\0';
    return result;
}
