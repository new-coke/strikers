#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace port::render
{
// Owned range registers, separate from the base fog enable. The table encoding
// follows the retained GX SDK; pair order and /64 below are Dolphin compatibility
// choices, not independently verified GX raster behaviour.
inline constexpr std::array<float, 12> kFogRangeDefaultTable{
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};
struct FogRangeState
{
    std::array<uint16_t, 10> raw{};
    std::array<float, 12> table = kFogRangeDefaultTable;
    bool enabled = false;
    bool valid = true;
};

// gxProjection[row * 4 + column] is the original nlMatrix4::e2 view projection,
// before GL conversion, model concatenation, or a packet's viewport override.
// Commit only a complete table. Failure leaves the caller's owned data intact.
inline bool make_fog_range_state(const std::array<float, 16>& gxProjection,
                                 FogRangeState& out) noexcept
{
    for (float value : gxProjection)
        if (!std::isfinite(value)) return false;
    if (gxProjection[0] == 0.f) return false;
    float nearZ, sideX;
    if (gxProjection[15] == 0.f)
    {
        const float denominator = gxProjection[10] - 1.f;
        if (!std::isfinite(denominator) || denominator == 0.f) return false;
        nearZ = gxProjection[11] / denominator;
        if (!std::isfinite(nearZ)) return false;
        sideX = nearZ / gxProjection[0];
    }
    else
    {
        sideX = 1.f / gxProjection[0];
        nearZ = 1.73205f * sideX;
    }
    if (!std::isfinite(sideX) || !std::isfinite(nearZ)) return false;
    const float nearSquared = nearZ * nearZ;
    if (!std::isfinite(nearSquared) || !(nearSquared > 0.f)) return false;

    FogRangeState next;
    // Preserve the literal SDK sequence and original fixed width 640.
    const float iw = 2.f / 640.f;
    for (unsigned i = 0; i < next.raw.size(); ++i)
    {
        float xi = float((i + 1) << 5);
        xi *= iw;
        if (!std::isfinite(xi)) return false;
        xi *= sideX;
        if (!std::isfinite(xi)) return false;
        const float xiSquared = xi * xi;
        if (!std::isfinite(xiSquared)) return false;
        const float ratio = xiSquared / nearSquared;
        if (!std::isfinite(ratio)) return false;
        const float argument = 1.f + ratio;
        if (!std::isfinite(argument) || !(argument > 0.f)) return false;
        const float rangeVal = std::sqrt(argument);
        const float encoded = 256.f * rangeVal;
        // Compare in double: float(UINT32_MAX) rounds up outside uint32's domain.
        if (!std::isfinite(encoded) || encoded < 0.f ||
            double(encoded) > double(std::numeric_limits<uint32_t>::max())) return false;
        next.raw[i] = uint16_t(uint32_t(encoded) & 0xFFFu);
        if (!next.raw[i]) return false; // The proposed fragment kernel divides by K.
    }
    for (unsigned i = 0; i < next.raw.size(); ++i)
        next.table[i] = float(next.raw[i ^ 1u]) / 64.f;
    next.table[10] = next.table[11] = next.table[9];
    next.enabled = true;
    out = next;
    return true;
}

// Use this exact rounding in both material preparation and glViewport. Zero
// remains a representable GL extent when range fog is disabled; an enabled
// range requires a strictly positive rounded width. Guard before lround/cast.
inline bool rounded_viewport_width(float value, int& width) noexcept
{
    if (!std::isfinite(value) ||
        double(value) <= double(std::numeric_limits<int>::min()) - .5 ||
        double(value) >= double(std::numeric_limits<int>::max()) + .5) return false;
    width = int(std::lround(value));
    return true;
}

inline bool fog_range_controls(float start, float end, int viewportWidth,
                                float globalCenter, std::array<float, 4>& out) noexcept
{
    if (viewportWidth <= 0 || !std::isfinite(globalCenter) ||
        !std::isfinite(start) || !std::isfinite(end)) return false;
    const float span = end - start;
    if (!std::isfinite(span)) return false;
    const float range = span > .001f ? span : .001f;
    const float startOverRange = start / range;
    const float scale = 2.f / float(viewportWidth);
    if (!std::isfinite(startOverRange) || !std::isfinite(scale) || !(scale > 0.f)) return false;
    out = { 1.f, scale, globalCenter, startOverRange };
    return true;
}
} // namespace port::render
