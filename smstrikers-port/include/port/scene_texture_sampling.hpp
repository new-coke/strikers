#pragma once
#include <cstdint>

namespace port::render
{
struct SceneTextureSampling
{
    uint8_t wrapU, wrapV, linear;
};

// GLX clamps the light-ramp lookup (BumpLocal, semantic 5) independently
// of the material's stored wrap bits. Its generated lighting coordinates
// include 0 and 1; repeating blends the opposite ends of the ramp there.
constexpr SceneTextureSampling scene_texture_sampling(unsigned semantic,
                                                      unsigned wrap,
                                                      bool linear) noexcept
{
    return { uint8_t(semantic == 5 || wrap >= 2),
             uint8_t(semantic == 5 || (wrap & 1)), uint8_t(linear) };
}
} // namespace port::render
