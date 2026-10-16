#include "port/native_match_transform.hpp"
#include "NL/glx/glxMatrix.h"
#include "NL/platvmath.h"
#include "dolphin/mtx.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

// Required by the unused rotation helpers in the real platvmath unit.
void nlSinCos(float* s, float* c, unsigned short angle)
{
    const float radians = float(angle) * (6.283185307179586f / 65536.f);
    *s = std::sin(radians);
    *c = std::cos(radians);
}
float nlRecipSqrt(float value, bool) { return 1.f / std::sqrt(value); }

using ClipVector = std::array<float, 4>;
static ClipVector shader_clip(const std::array<float, 16>& m, const ClipVector& v)
{
    ClipVector out{};
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned col = 0; col < 4; ++col)
            out[row] += m[col * 4 + row] * v[col];
    return out;
}
static ClipVector original_clip(const nlMatrix4& modelView, const nlMatrix4& projection,
                                const ClipVector& v)
{
    // Independent oracle: actual GLX conversion followed by GX's six
    // projection coefficients, without using the native composition helper.
    Mtx model{};
    Mtx44 p{};
    glxCopyMatrix(model, modelView);
    glxCopyMatrix(p, projection);
    float eye[3]{};
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 4; ++col)
            eye[row] += model[row][col] * v[col];
    if (p[3][2] == -1.f)
        return { p[0][0] * eye[0] + p[0][2] * eye[2], p[1][1] * eye[1] + p[1][2] * eye[2],
                 p[2][2] * eye[2] + p[2][3] * v[3], -eye[2] };
    return { p[0][0] * eye[0] + p[0][3] * v[3], p[1][1] * eye[1] + p[1][3] * v[3],
             p[2][2] * eye[2] + p[2][3] * v[3], v[3] };
}
static void check(const nlMatrix4& modelView, const nlMatrix4& projection, const ClipVector& vertex)
{
    const auto native =
        shader_clip(port::directmatch::compose_glx_mvp(modelView.e, projection.e), vertex);
    const auto oracle = original_clip(modelView, projection, vertex);
    for (unsigned n = 0; n < 4; ++n)
        assert(std::abs(native[n] - oracle[n]) <= .0001f * std::max(1.f, std::abs(oracle[n])));
}
int main()
{
    nlMatrix4 model{}, view{}, mv{}, projection{};
    model.SetIdentity();
    view.SetIdentity();
    C_MTXPerspective(projection.e2, 60.f, 960.f / 544.f, 8.f, 1000.f);
    // Regression: near must map to z/w=-1, far to 0, and w=-eye.z.
    const auto m = port::directmatch::compose_glx_mvp(model.e, projection.e);
    const auto near = shader_clip(m, { 0, 0, -8, 1 }), far = shader_clip(m, { 0, 0, -1000, 1 });
    assert(std::abs(near[2] / near[3] + 1.f) < .00001f);
    assert(std::abs(far[2] / far[3]) < .00001f && far[3] == 1000.f);
    nlMatrix4 old{};
    nlMultMatrices(old, model, projection);
    std::array<float, 16> oldBytes{};
    std::copy(old.e, old.e + 16, oldBytes.begin());
    assert(std::abs(shader_clip(oldBytes, { 0, 0, -8, 1 })[2] /
                        shader_clip(oldBytes, { 0, 0, -8, 1 })[3] +
                    1.f) > .1f);
    unsigned checks = 0;
    for (unsigned i = 0; i < 128; ++i)
    {
        model.SetIdentity();
        view.SetIdentity();
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
            {
                model.e2[row][col] = float(int((i + row * 7 + col * 11) % 23) - 11) / 8.f;
                view.e2[row][col] = float(int((i * 3 + row * 5 + col * 13) % 31) - 15) / 16.f;
            }
        model.e2[3][0] = float(i) - 64.f;
        model.e2[3][1] = -3.f;
        model.e2[3][2] = -42.f;
        view.e2[3][0] = -5.f;
        view.e2[3][1] = 13.f;
        view.e2[3][2] = -100.f;
        nlMultMatrices(mv, model, view);
        for (unsigned kind = 0; kind < 3; ++kind)
        {
            if (kind == 0)
                C_MTXPerspective(projection.e2, 60.f, 1.7f, 1.f, 1000.f);
            if (kind == 1)
                C_MTXFrustum(projection.e2, 3.f, -2.f, -4.f, 5.f, 1.f, 500.f);
            if (kind == 2)
                C_MTXOrtho(projection.e2, 100.f, -40.f, -60.f, 140.f, .5f, 300.f);
            for (const ClipVector& v : { ClipVector{ 0, 0, 0, 1 }, ClipVector{ 4, -7, -13, 1 },
                                         ClipVector{ -12, 5, 8, 1 } })
            {
                check(mv, projection, v);
                ++checks;
            }
        }
    }
    // GLX/GX ignores unused projection coefficients and affine bottom row.
    C_MTXPerspective(projection.e2, 60.f, 1.7f, 1.f, 1000.f);
    projection.e2[0][1] = projection.e2[3][3] = std::numeric_limits<float>::quiet_NaN();
    mv.e2[0][3] = std::numeric_limits<float>::quiet_NaN();
    check(mv, projection, { 4, -7, -13, 1 });
    std::printf("native transform: %u GLX/GX oracle comparisons and near/far regression passed\n",
                checks + 1);
}
