#pragma once

#include <array>
#include <span>

namespace port::directmatch {

// GLX stores the affine model/view matrix in NL's column layout, but passes
// its projection unchanged to GXSetProjection in Dolphin's row layout.
// Reconstruct the six projection coefficients used by that path, then
// compose a column-layout MVP for the native shader. Arbitrary unused entries
// in the projection must not enter the transform.
inline std::array<float,16> compose_glx_mvp(std::span<const float,16> modelView,
                                         std::span<const float,16> projection) noexcept {
    const bool perspective = projection[14] == -1.f;
    std::array<float,16> p{};
    p[0] = projection[0];
    p[5] = projection[5];
    p[10] = projection[10];
    p[14] = projection[11];
    if (perspective) {
        p[8] = projection[2];
        p[9] = projection[6];
        p[11] = -1.f;
    } else {
        p[12] = projection[3];
        p[13] = projection[7];
        p[15] = 1.f;
    }
    std::array<float,16> out{};
    for (unsigned column=0; column<4; ++column)
        for (unsigned row=0; row<4; ++row)
            for (unsigned k=0; k<3; ++k)
                out[column*4+row] += p[k*4+row] * modelView[column*4+k];
    // GXLoadPosMtxImm uploads only 3x4: the affine bottom row is implicit.
    for (unsigned row=0; row<4; ++row) out[12+row] += p[12+row];
    return out;
}

} // namespace port::directmatch
