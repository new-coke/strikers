#include "NL/gl/glMemory.h"

/**
 * Offset/Address/Size: 0x0 | 0x801D8FBC | size: 0x20
 */
void glResourceRelease(unsigned long long marker)
{
    glplatResourceRelease(marker);
}

/**
 * Offset/Address/Size: 0x20 | 0x801D8FDC | size: 0x20
 */
unsigned long long glResourceMark()
{
    return glplatResourceMark();
}

/**
 * Offset/Address/Size: 0x40 | 0x801D8FFC | size: 0x20
 */
void* glFrameAlloc(unsigned long size, eGLMemory memType)
{
    return glplatFrameAlloc(size, memType);
}

/**
 * Offset/Address/Size: 0x60 | 0x801D901C | size: 0x20
 */
void* glResourceAlloc(unsigned long size, eGLMemory memType)
{
    return glplatResourceAlloc(size, memType);
}
