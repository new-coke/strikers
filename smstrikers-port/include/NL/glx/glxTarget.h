#ifndef _GLXTARGET_H_
#define _GLXTARGET_H_

#include "NL/glx/glxGX.h"

void glPlatGrabFrameBufferToTexture(unsigned long texture, unsigned int destWidth, unsigned int destHeight, unsigned int srcLeft, unsigned int srcTop, unsigned int srcWidth, unsigned int srcHeight);
void glx_UpdateWarble();
void glx_DOFUpdate(float startDist);
void glx_DOFGrab();
void glx_ShadowTextureGrab();
void glx_ClearZBuffer();
void glx_OffsetGrab();
void glx_ColourGrab();
void glx_ShadowGrab();
void glxPostInitTargets();
void glxInitTargets();
bool glx_GetSharedLock();
void glx_UnlockSharedMemory();
void glx_LockSharedMemory();
u32 glx_GetSharedMemorySize();
u32 glx_GetSharedMemory();

#endif // _GLXTARGET_H_
