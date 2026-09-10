#ifndef _GCSWIZZLER_H_
#define _GCSWIZZLER_H_

#include "types.h"
#include "NL/glx/glxTexture.h"

u32 GCTextureSize(eGXTextureFormat format, int width, int height, int numLevels, unsigned long texhandle);
void GCSwizzle(void* pSwizzledData, const void* pLinearData, unsigned short width, unsigned short height, eGXTextureFormat format, bool bEndianSwap);

#endif // _GCSWIZZLER_H_
