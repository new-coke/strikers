#include "NL/nlDebugFile.h"

#include "file_io.h"
#include "char_io.h"

/**
 * Offset/Address/Size: 0x0 | 0x801CE954 | size: 0x20
 */
void nlCloseFileDebug(void* fp)
{
    fclose((FILE*)fp);
}

/**
 * Offset/Address/Size: 0x20 | 0x801CE974 | size: 0x20
 */
void nlFlushFileDebug(void* fp)
{
    fflush((FILE*)fp);
}

/**
 * Offset/Address/Size: 0x40 | 0x801CE994 | size: 0x50
 */
void nlWriteLineDebug(void* file, const char* buffer, bool flush)
{
    fputs(buffer, (FILE*)file);
    if (flush != 0)
    {
        fflush((FILE*)file);
    }
}

/**
 * Offset/Address/Size: 0x90 | 0x801CE9E4 | size: 0x4C
 */
void* nlOpenFileDebug(const char* fileName, bool bBinary, bool bAppend)
{
    if (bBinary != 0)
    {
        return (void*)fopen(fileName, "wb");
    }
    if (bAppend != 0)
    {
        return (void*)fopen(fileName, "awt");
    }
    return (void*)fopen(fileName, "wt");
}
