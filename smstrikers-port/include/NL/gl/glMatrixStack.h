#ifndef _GLMATRIXSTACK_H_
#define _GLMATRIXSTACK_H_

#include "NL/nlMath.h"

class GLMatrixStack
{
public:
    void GetInverse(nlMatrix4& m, bool bForce);
    void GetTop(nlMatrix4& m);
    void PopMatrix();
    void PushMatrix();
    void MultMatrixLocal(const nlMatrix4& m);
    void MultMatrix(const nlMatrix4& m);
    void LoadMatrix(const nlMatrix4& m);
    void LoadIdentity();
    ~GLMatrixStack();
    GLMatrixStack(int nLevels);

    /* 0x00 */ nlMatrix4 top;
    /* 0x40 */ nlMatrix4 inverse;
    /* 0x80 */ nlMatrix4* stack;
    /* 0x84 */ bool bDirtyInverse;
    /* 0x88 */ int maxStackDepth;
    /* 0x8C */ int stackDepth;
}; // total size: 0x90

#endif // _GLMATRIXSTACK_H_
