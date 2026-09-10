#ifndef _GLMEMORY_H_
#define _GLMEMORY_H_

#include "NL/glx/glxMemory.h"

void glResourceRelease(unsigned long long marker);
unsigned long long glResourceMark();
void* glFrameAlloc(unsigned long size, eGLMemory memType);
void* glResourceAlloc(unsigned long size, eGLMemory memType);

#endif // _GLMEMORY_H_
