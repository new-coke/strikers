#include "NL/nlMath.h"
#include "dolphin/mtx.h"

#define qr0 0

// Conversion factor from radians to 16-bit fixed-point format: 65536.0f / (2*pi) ~= 10430.378f
#define RAD_TO_FIXED16 10430.378f

/**
 * Offset/Address/Size: 0x800 | 0x801C437C | size: 0x20
 */
void nlMatrix4::SetIdentity()
{
    PSMTX44Identity(e2);
}

/**
 * Offset/Address/Size: 0x724 | 0x801C42A0 | size: 0xDC
 */
void nlMultMatrices(nlMatrix4& out, const nlMatrix4& a, const nlMatrix4& b)
{
    nlMatrix4 temp;

    if ((out.e2 == a.e2) || (out.e2 == b.e2))
    {
        PSMTX44Concat(a.e2, b.e2, temp.e2);
        out = temp;
        return;
    }
    PSMTX44Concat(a.e2, b.e2, out.e2);
}

/**
 * Offset/Address/Size: 0x658 | 0x801C41D4 | size: 0xCC
 */
void nlTransposeMatrix(nlMatrix4& out, const nlMatrix4& in)
{
    if (out.e2 == in.e2)
    {
        nlMatrix4 temp;
        PSMTX44Transpose(in.e2, temp.e2);
        out = temp;
        return;
    }
    PSMTX44Transpose(in.e2, out.e2);
}

/**
 * Offset/Address/Size: 0x5A4 | 0x801C4120 | size: 0xB4
 */
void nlInvertMatrix(nlMatrix4& out, const nlMatrix4& in)
{
    nlMatrix4 temp;
    C_MTX44Inverse(in.e2, temp.e2);
    out = temp;
}

/**
 * Offset/Address/Size: 0x548 | 0x801C40C4 | size: 0x5C
 */
void nlMultVectorMatrix(nlVector2& v_out, const nlVector2& v_in, const nlMatrix3& m)
{
    nlVector2 t;
    t.x = m.e[0] * v_in.x + m.e[3] * v_in.y + m.e[6];
    t.y = m.e[1] * v_in.x + m.e[4] * v_in.y + m.e[7];
    v_out = t;
}

/**
 * Offset/Address/Size: 0x4F4 | 0x801C4070 | size: 0x54
 */
#pragma scheduling off
void nlMultPosVectorMatrix(register nlVector3& result, register const nlVector3& pos, register const nlMatrix4& transformMatrix)
{
    // clang-format off
    const float x = pos.x;   // PORT: read pos before writing result, which aliases it at most call sites (SetupRotatedRectangle).
    const float y = pos.y;
    const float z = pos.z;
    for (int c = 0; c < 3; c++)
    {
        (&result.x)[c] = transformMatrix.e2[0][c] * x
                       + transformMatrix.e2[1][c] * y
                       + transformMatrix.e2[2][c] * z
                       + transformMatrix.e2[3][c];
    }
    // clang-format on
}
#pragma scheduling reset

/**
 * Offset/Address/Size: 0x428 | 0x801C3FA4 | size: 0xCC
 */
void nlMultVectorMatrix(nlVector4& out, const nlVector4& in, const nlMatrix4& m)
{
    nlVector4 temp;
    temp.x = m.e2[0][0] * in.x + m.e2[1][0] * in.y + m.e2[2][0] * in.z + m.e2[3][0] * in.w;
    temp.y = m.e2[0][1] * in.x + m.e2[1][1] * in.y + m.e2[2][1] * in.z + m.e2[3][1] * in.w;
    temp.z = m.e2[0][2] * in.x + m.e2[1][2] * in.y + m.e2[2][2] * in.z + m.e2[3][2] * in.w;
    temp.w = m.e2[0][3] * in.x + m.e2[1][3] * in.y + m.e2[2][3] * in.z + m.e2[3][3] * in.w;
    out = temp;
}

/**
 * Offset/Address/Size: 0x3E4 | 0x801C3F60 | size: 0x44
 */
#pragma scheduling off
void nlMultDirVectorMatrix(register nlVector3& result, register const nlVector3& direction, register const nlMatrix4& transformMatrix)
{
    // clang-format off
    const float x = direction.x;   // PORT: read before write, as in nlMultPosVectorMatrix.
    const float y = direction.y;
    const float z = direction.z;
    for (int c = 0; c < 3; c++)
    {
        (&result.x)[c] = transformMatrix.e2[0][c] * x
                       + transformMatrix.e2[1][c] * y
                       + transformMatrix.e2[2][c] * z;
    }
    // clang-format on
}
#pragma scheduling reset

/**
 * Offset/Address/Size: 0x36C | 0x801C3EE8 | size: 0x78
 */
void nlMakeRotationMatrixX(nlMatrix4& out, float theta)
{
    f32 sn;
    f32 cs;

    nlSinCos(&sn, &cs, (short)(RAD_TO_FIXED16 * theta));
    PSMTX44Identity(out.e2);
    out.e2[1][1] = cs;            // cos(theta) at offset 0x14
    out.e2[1][2] = sn;            // sin(theta) at offset 0x18
    out.e2[2][1] = -out.e2[1][2]; // -sin(theta) at offset 0x24
    out.e2[2][2] = out.e2[1][1];  // cos(theta) at offset 0x28
}

/**
 * Offset/Address/Size: 0x2F0 | 0x801C3E6C | size: 0x7C
 */
void nlMakeRotationMatrixY(nlMatrix4& out, float theta)
{
    f32 sn;
    f32 cs;

    nlSinCos(&sn, &cs, (short)(RAD_TO_FIXED16 * theta));
    PSMTX44Identity(out.e2);
    out.e2[0][0] = cs;            // cos(theta) at offset 0x00
    out.e2[0][2] = -sn;           // sin(theta) at offset 0x08
    out.e2[2][0] = -out.e2[0][2]; // -sin(theta) at offset 0x18
    out.e2[2][2] = out.e2[0][0];  // cos(theta) at offset 0x20
}
/**
 * Offset/Address/Size: 0x278 | 0x801C3DF4 | size: 0x78
 */
void nlMakeRotationMatrixZ(nlMatrix4& out, float theta)
{
    f32 sn;
    f32 cs;

    nlSinCos(&sn, &cs, (short)(RAD_TO_FIXED16 * theta));
    PSMTX44Identity(out.e2);

    out.e2[0][0] = cs;            // cos(theta) at offset 0x00
    out.e2[0][1] = sn;            // sin(theta) at offset 0x04
    out.e2[1][0] = -out.e2[0][1]; // -sin(theta) at offset 0x10
    out.e2[1][1] = out.e2[0][0];  // cos(theta) at offset 0x14
}

/**
 * Offset/Address/Size: 0x1DC | 0x801C3D58 | size: 0x9C
 */
void nlMakeRotationMatrixZ(nlMatrix3& out, float theta)
{
    f32 sn;
    f32 cs;

    nlSinCos(&sn, &cs, (short)(10430.378f * theta));

    out.SetIdentity();

    out.e[0] = cs;
    out.e[1] = sn;
    out.e[3] = -out.e[1];
    out.e[4] = out.e[0];
}

/**
 * Offset/Address/Size: 0x20 | 0x801C3B9C | size: 0x1BC
 */
void nlMakeRotationMatrixEulerAngles(nlMatrix4& m, float pitch, float yaw, float roll)
{
    float zero = 0.0f;
    float one = 1.0f;
    f32 sinRoll;
    f32 cosRoll;
    f32 sinYaw;
    f32 cosYaw;
    f32 sinPitch;
    f32 cosPitch;

    nlSinCos(&sinRoll, &cosRoll, (short)(RAD_TO_FIXED16 * roll));
    nlSinCos(&sinYaw, &cosYaw, (short)(RAD_TO_FIXED16 * yaw));
    nlSinCos(&sinPitch, &cosPitch, (short)(RAD_TO_FIXED16 * pitch));

    m.e2[2][3] = zero;
    m.e2[1][3] = zero;
    m.e2[0][3] = zero;
    m.e2[3][2] = zero;
    m.e2[3][1] = zero;
    m.e2[3][0] = zero;
    m.e2[3][3] = one;
    m.e2[0][0] = cosYaw * cosRoll;
    m.e2[0][1] = cosYaw * sinRoll;
    m.e2[0][2] = -sinYaw;
    m.e2[1][0] = (cosRoll * (sinPitch * sinYaw)) - (cosPitch * sinRoll);
    m.e2[1][1] = (sinRoll * (sinPitch * sinYaw)) + (cosPitch * cosRoll);
    m.e2[1][2] = cosYaw * sinPitch;
    m.e2[2][0] = (cosRoll * (cosPitch * sinYaw)) + (sinPitch * sinRoll);
    m.e2[2][1] = (sinRoll * (cosPitch * sinYaw)) - (sinPitch * cosRoll);
    m.e2[2][2] = cosYaw * cosPitch;
}

/**
 * Offset/Address/Size: 0x0 | 0x801C3B7C | size: 0x20
 */
void nlMakeScaleMatrix(nlMatrix4& m, float sx, float sy, float sz)
{
    PSMTX44Scale(m.e2, sx, sy, sz);
}

/**
 * Stub only for field order; unreferenced so the linker drops it.
 * Forces emission of specific constants/operations so the compiler lays out the related fields to match the original binary.
 */
void platvmath_stub(float& sx, float& sy, float& sz)
{
    sy = 1.0f;
    sx = 0.0f;
    sz = RAD_TO_FIXED16;
}
