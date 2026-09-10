#include "NL/glx/glxMatrix.h"
#include "math.h"

#include <stddef.h>

extern "C"
{
    void* memcpy(void* dest, const void* src, size_t num);
}

/**
 * Offset/Address/Size: 0x0 | 0x801B6568 | size: 0x64
 */
void glxCopyMatrix(float (&mo)[3][4], const nlMatrix4& mi)
{
    // Row 0: copy from nlMatrix4 row 0 to target row 0
    mo[0][0] = mi.e2[0][0]; // offset 0x00
    mo[0][1] = mi.e2[1][0]; // offset 0x04
    mo[0][2] = mi.e2[2][0]; // offset 0x08
    mo[0][3] = mi.e2[3][0]; // offset 0x0C

    // Row 1: copy from nlMatrix4 row 1 to target row 1
    mo[1][0] = mi.e2[0][1]; // offset 0x10
    mo[1][1] = mi.e2[1][1]; // offset 0x14
    mo[1][2] = mi.e2[2][1]; // offset 0x18
    mo[1][3] = mi.e2[3][1]; // offset 0x1C

    // Row 2: copy from nlMatrix4 row 2 to target row 2
    mo[2][0] = mi.e2[0][2]; // offset 0x20
    mo[2][1] = mi.e2[1][2]; // offset 0x24
    mo[2][2] = mi.e2[2][2]; // offset 0x28
    mo[2][3] = mi.e2[3][2]; // offset 0x2C
}

/**
 * Offset/Address/Size: 0x64 | 0x801B65CC | size: 0x24
 */
void glxCopyMatrix(float (&mo)[4][4], const nlMatrix4& mi)
{
    memcpy(mo, mi.e2, sizeof(mi.e2));
}

/**
 * Offset/Address/Size: 0x88 | 0x801B65F0 | size: 0x1CC
 */
void glplatMatrixLookAt(nlMatrix4& m, const nlVector3& eye, const nlVector3& at, const nlVector3& up)
{
    nlVector3 side;
    nlVector3 view;
    nlVector3 cameraUp;
    float x;
    float y;
    float z;
    float inverseLength = nlRecipSqrt(
        nlGetLengthSquared3D(
            x = eye.x - at.x,
            y = eye.y - at.y,
            z = eye.z - at.z),
        true);
    float upZ = up.z;
    float upY = up.y;
    float forwardX = inverseLength * x;
    float upX = up.x;
    float forwardY = inverseLength * y;
    float forwardZ = inverseLength * z;

    float negUpX = -upX;
    float upZForwardX = upZ * forwardX;
    float upZForwardY = upZ * forwardY;
    float upYForwardX = upY * forwardX;
    x = upY * forwardZ - upZForwardY;
    y = negUpX * forwardZ + upZForwardX;
    z = upX * forwardY - upYForwardX;

    inverseLength = nlRecipSqrt(z * z + (x * x + y * y), true);

    side.x = inverseLength * x;
    side.y = inverseLength * y;
    side.z = inverseLength * z;

    view.x = forwardX;
    view.y = forwardY;
    view.z = forwardZ;

    float negForwardX = -view.x;
    float cameraUpX = (view.y * side.z) - (view.z * side.y);
    nlVec3Set(cameraUp,
        cameraUpX,
        (negForwardX * side.z) + (view.z * side.x),
        (view.x * side.y) - (view.y * side.x));

    m.SetColumn_(0, side);
    m.e2[3][0] = -nlVec3DotProduct(side, eye);
    m.SetColumn_(1, cameraUp);
    m.e2[3][1] = -nlVec3DotProduct(cameraUp, eye);
    m.SetColumn_(2, view);
    m.e2[3][2] = -nlVec3DotProduct(view, eye);
    m.e2[0][3] = 0.0f;
    m.e2[1][3] = 0.0f;
    m.e2[2][3] = 0.0f;
    m.e2[3][3] = 1.0f;
}

/**
 * Offset/Address/Size: 0x254 | 0x801B67BC | size: 0xB8
 */
void glplatMatrixPerspective(nlMatrix4& matrix, float fovY, float aspect, float near, float far)
{
    f32 temp_f2 = tan(0.5f * fovY);
    f32 atanVal = (f32)atan((1.0f / aspect) / (1.0f / temp_f2));
    C_MTXPerspective(matrix.e2, (2.f * atanVal * 180.0f) / 3.1415927f, aspect, near, far);
}

/**
 * Offset/Address/Size: 0x30C | 0x801B6874 | size: 0x48
 */
void glplatMatrixOrthographicCentered(nlMatrix4& matrix, float width, float height, float near, float far)
{
    float half = 0.5f;
    C_MTXOrtho(matrix.e2, height * half, -height * half, -width * half, width * half, near, far);
}

/**
 * Offset/Address/Size: 0x354 | 0x801B68BC | size: 0x6C
 */
void glplatMatrixOrthographic(nlMatrix4& matrix, float width, float height)
{
    static float fNear = 0.0f;
    static float fFar = 16777215.0f;
    C_MTXOrtho(matrix.e2, 0.f, height, 0.f, width, fNear, fFar);
}
