#include "NL/glx/glxFont.h"
#include "NL/glx/glxTexture.h"
#include "NL/nlMemory.h"

/**
 * Offset/Address/Size: 0x0 | 0x801B64D8 | size: 0x90
 */
bool glplatCreateFont(unsigned long width, unsigned long height, const unsigned short* data, uintptr_t handle)
{
    PlatTexture* pTex = glx_CreatePlatTexture();
    pTex->Create(width, height, GXTex_RGB5A3, 1, true, false);
    memcpy(pTex->m_LinearData, data, height * (width * 2));
    pTex->Swizzle(true);
    pTex->Prepare();
    glx_AddTex(handle, pTex);
    return true;
}
