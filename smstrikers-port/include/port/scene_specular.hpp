#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace port::render
{
struct SceneSpecularLight
{
    std::array<float, 4> direction{}, color{};
};

// glx_LoadSpecular normalizes the incoming direction and applies the game's
// 1.25 intensity factor before loading the specular colour channel.
inline SceneSpecularLight scene_specular_light(std::array<float, 4> direction,
                                               const float* color, float intensity,
                                               float exponent) noexcept
{
    SceneSpecularLight result;
    const float length2 = direction[0] * direction[0] + direction[1] * direction[1] +
                          direction[2] * direction[2];
    if (!(length2 > 0) || !std::isfinite(length2) || !std::isfinite(exponent) ||
        !std::isfinite(intensity))
        return result;
    const float reciprocal = 1.f / std::sqrt(length2);
    for (unsigned i = 0; i < 3; ++i)
        result.direction[i] = direction[i] * reciprocal;
    result.direction[3] = std::max(0.f, exponent);
    for (unsigned i = 0; i < 4; ++i)
        result.color[i] = std::isfinite(color[i]) ?
            std::clamp(color[i] * intensity * 1.25f, 0.f, 1.f) : 0.f;
    return result;
}
} // namespace port::render
