#include "Game/GL/GLSkinMesh.h"
#include "dolphin/mtx.h"
#include "string.h"

/**
 * Offset/Address/Size: 0xD8 | 0x801E05E0 | size: 0x64
 */
void SkinMatrix::Set(const nlMatrix4& in)
{
    m[0][0] = in.e2[0][0];
    m[0][1] = in.e2[1][0];
    m[0][2] = in.e2[2][0];
    m[0][3] = in.e2[3][0];

    m[1][0] = in.e2[0][1];
    m[1][1] = in.e2[1][1];
    m[1][2] = in.e2[2][1];
    m[1][3] = in.e2[3][1];

    m[2][0] = in.e2[0][2];
    m[2][1] = in.e2[1][2];
    m[2][2] = in.e2[2][2];
    m[2][3] = in.e2[3][2];
}

/**
 * Offset/Address/Size: 0x5C | 0x801E0564 | size: 0x7C
 */
void SkinMatrix::Get(nlMatrix4& out) const
{
    out.e2[0][0] = m[0][0];
    out.e2[1][0] = m[0][1];
    out.e2[2][0] = m[0][2];
    out.e2[3][0] = m[0][3];

    out.e2[0][1] = m[1][0];
    out.e2[1][1] = m[1][1];
    out.e2[2][1] = m[1][2];
    out.e2[3][1] = m[1][3];

    out.e2[0][2] = m[2][0];
    out.e2[1][2] = m[2][1];
    out.e2[2][2] = m[2][2];
    out.e2[3][2] = m[2][3];

    out.e2[0][3] = 0.0f;
    out.e2[1][3] = 0.0f;
    out.e2[2][3] = 0.0f;
    out.e2[3][3] = 1.0f;
}

/**
 * Offset/Address/Size: 0x2C | 0x801E0534 | size: 0x30
 */
void SkinMatrix::Get4x3(float* out) const
{
    memcpy(out, m, 12 * sizeof(float));
}

/**
 * Offset/Address/Size: 0x0 | 0x801E0508 | size: 0x2C
 */
void nlMultMatrices(SkinMatrix& out, const SkinMatrix& a, const SkinMatrix& b)
{
    PSMTXConcat(reinterpret_cast<CMtxP>(b.m), reinterpret_cast<CMtxP>(a.m), reinterpret_cast<MtxPtr>(out.m));
}
