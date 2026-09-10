#include "NL/gl/glTexture.h"
#include "NL/glx/glxTexture.h"

/**
 * Offset/Address/Size: 0x0 | 0x801DF160 | size: 0x20
 */
void glTextureReplace(uintptr_t texture, const void* buffer, unsigned long length)
{
    glplatTextureReplace(texture, buffer, length);
}

/**
 * Offset/Address/Size: 0x20 | 0x801DF180 | size: 0x20
 */
void glTextureAdd(uintptr_t texture, const void* buffer, unsigned long length)
{
    glplatTextureAdd(texture, buffer, length);
}

/**
 * Offset/Address/Size: 0x40 | 0x801DF1A0 | size: 0x20
 */
int glTextureGetNumBits(int component)
{
    return glplatTextureGetNumBits(component);
}

/**
 * Offset/Address/Size: 0x60 | 0x801DF1C0 | size: 0x20
 */
u32 glTextureGetHeight()
{
    return glplatTextureGetHeight();
}

/**
 * Offset/Address/Size: 0x80 | 0x801DF1E0 | size: 0x20
 */
u32 glTextureGetWidth()
{
    return glplatTextureGetWidth();
}

/**
 * Offset/Address/Size: 0xA0 | 0x801DF200 | size: 0x20
 */
bool glTextureLoad(uintptr_t texture)
{
    return glplatTextureLoad(texture);
}
