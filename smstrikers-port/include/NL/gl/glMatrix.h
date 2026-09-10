#ifndef _GLMATRIX_H_
#define _GLMATRIX_H_

#include "NL/nlMath.h"

void glMatrixLookAt(nlMatrix4& m, const nlVector3& peye, const nlVector3& pat, const nlVector3& vup);
void glMatrixPerspective(nlMatrix4& m, float fovRad, float aspect, float nearPlane, float farPlane);
void glMatrixOrthographicCentered(nlMatrix4& m, float width, float height, float nearPlane, float farPlane);
void glMatrixOrthographic(nlMatrix4& m, float width, float height);
void glSetMatrix(uintptr_t matrix, const nlMatrix4& m);
void glGetMatrix(uintptr_t matrix, nlMatrix4& m);
uintptr_t glAllocMatrix();
uintptr_t glGetIdentityMatrix();
void gl_MatrixStartup();

class GLMatrix
{
public:
    void Set(const nlMatrix4& m);
    void Get(nlMatrix4& m) const;

    nlMatrix4 matrix;
};

#endif // _GLMATRIX_H_
