#ifndef _GLDRAW2_H_
#define _GLDRAW2_H_

#include "types.h"

#include "NL/nlColour.h"
#include "NL/gl/glView.h"

class glPoly2
{
public:
    bool Attach(eGLView view, int layer, uintptr_t* pMatrixHandle, unsigned long programHandle);
    void FullCoverage(const nlColour& col, float z);
    void SetupRectangle(float x, float y, float w, float h, float z);
    void SetupRotatedRectangle(float cx, float cy, float w, float h, float angle, float z);
    void SetColour(const nlColour& col);

    /* 0x00 */ nlVector2 m_pos[4];
    /* 0x20 */ nlVector2 m_uv[4];
    /* 0x40 */ nlColour m_colour[4];
    /* 0x50 */ float depth;
}; // total size: 0x54

bool glAttachPoly2(eGLView view, unsigned long numPolys, glPoly2* pPolys, uintptr_t* pMatrixHandle, const void* pUserData);

#endif // _GLDRAW2_H_
