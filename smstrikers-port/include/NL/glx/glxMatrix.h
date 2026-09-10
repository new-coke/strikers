#ifndef _GLXMATRIX_H_
#define _GLXMATRIX_H_

#include "NL/nlMath.h"

void glxCopyMatrix(float (&mo)[3][4], const nlMatrix4& mi);
void glxCopyMatrix(float (&mo)[4][4], const nlMatrix4& mi);
void glplatMatrixLookAt(nlMatrix4& m, const nlVector3& eye, const nlVector3& at, const nlVector3& up);
void glplatMatrixPerspective(nlMatrix4& matrix, float fovY, float aspect, float near, float far);
void glplatMatrixOrthographicCentered(nlMatrix4& matrix, float width, float height, float near, float far);
void glplatMatrixOrthographic(nlMatrix4& matrix, float width, float height);

#endif // _GLXMATRIX_H_
