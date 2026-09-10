#ifndef _GLXMEMORY_H_
#define _GLXMEMORY_H_

#include "NL/nlMath.h"

enum eGLMemory
{
    GLM_Header = 0,
    GLM_Matrix = 1,
    GLM_IndexData = 2,
    GLM_VertexData = 3,
    GLM_TextureData = 4,
    GLM_Target = 5,
    GLM_Num = 6,
};

void glplatSetMatrix(uintptr_t matrix, const nlMatrix4& m);
void glplatGetMatrix(uintptr_t matrix, nlMatrix4& m);
void glplatFrameAllocNextFrame();
void* glplatFrameAlloc(unsigned long size, eGLMemory memType);
void glplatResourceRelease(unsigned long long marker);
unsigned long long glplatResourceMark();
void* glplatResourceAlloc(unsigned long size, eGLMemory memType);
bool glxInitMemory();
void glx_FreeMemory1(const char* filename);
void glx_FreeMemory0();
u32 glx_GetFreeMemory();

class GLXMemoryInfo
{
public:
    inline GLXMemoryInfo();
    void Clear();
    u32 GetTotal() const;
    u32 GetTexDiff() const;
    void Print(unsigned long level) const;

    /* 0x00 */ unsigned long m_uBytes[6];
    /* 0x18 */ unsigned long m_uTexBundle;
}; // total size: 0x1C

#endif // _GLXMEMORY_H_
