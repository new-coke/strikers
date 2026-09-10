#ifndef _GLTEXTURE_H_
#define _GLTEXTURE_H_

#include "types.h"

void glTextureReplace(uintptr_t texture, const void* buffer, unsigned long length);
void glTextureAdd(uintptr_t texture, const void* buffer, unsigned long length);
int glTextureGetNumBits(int component);
u32 glTextureGetHeight();
u32 glTextureGetWidth();
bool glTextureLoad(uintptr_t texture);

#endif // _GLTEXTURE_H_
