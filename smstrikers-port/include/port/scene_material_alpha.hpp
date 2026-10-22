#pragma once
#include "port/scene_alpha_state.hpp"
#include <algorithm>

namespace port::render
{
constexpr float scene_material_konst(float level) noexcept
{
    return float(int(255.5f * std::clamp(level, 0.f, 1.f))) / 255.f;
}

constexpr float scene_alpha_tail_minimum(SceneAlphaTail tail, float previous, float texture,
                                         float raster, float konst) noexcept
{
    float value = 0;
    switch (tail)
    {
    case SceneAlphaTail::DoublePrevious: value = 2.f * previous; break;
    case SceneAlphaTail::Previous: value = previous; break;
    case SceneAlphaTail::PreviousTimesRaster: value = previous * raster; break;
    case SceneAlphaTail::PreviousPlusTexture: value = previous + texture; break;
    case SceneAlphaTail::PreviousPlusKonstSquared: value = previous + konst * konst; break;
    case SceneAlphaTail::Texture: value = texture; break;
    case SceneAlphaTail::PreviousPlusInverseKonstTexture:
        value = previous + texture * (1.f - konst); break;
    case SceneAlphaTail::Zero: break;
    case SceneAlphaTail::KonstSquared: value = konst * konst; break;
    }
    return std::clamp(value, 0.f, 1.f);
}

// Inputs are normalized texture/vertex minima and the already-resolved vertex
// alpha override. A zero bound keeps alpha testing for an unknown program.
constexpr float scene_material_minimum_alpha(unsigned mask, float raster,
                                              float diffuse, float detail, float blend,
                                              float alphaOverride, bool dirt,
                                              const std::array<float, 4>& state,
                                              float shadow, float gloss,
                                              float secondaryRaster = 0.f) noexcept
{
    const float mixed = detail * (1.f - blend) + diffuse * blend;
    switch (mask)
    {
    case 0: return raster * state[0];
    case 1: case 5: case 9: case 13: case 17: case 19: case 25:
        return raster * diffuse;
    case 3: case 7: return raster * mixed;
    case 21:
        return scene_alpha_tail_minimum(SceneAlphaTail(unsigned(state[3])), raster * diffuse,
                                         gloss, secondaryRaster, state[2]);
    case 33: case 49: return diffuse * (alphaOverride >= 0.f ? alphaOverride : state[1]);
    case 35: return dirt ? diffuse : mixed;
    case 37:
        return scene_alpha_tail_minimum(SceneAlphaTail(unsigned(state[3])), state[0] * state[0],
                                         shadow, 0.f, state[2]);
    case 39: return mixed * (alphaOverride >= 0.f ? alphaOverride : 0.f);
    case 41:
        return std::clamp(diffuse * (alphaOverride >= 0.f ? alphaOverride : state[1]) +
                              state[2] * state[2], 0.f, 1.f);
    case 43: return mixed;
    case 53: case 57: return diffuse * state[1];
    default: return 0.f;
    }
}
} // namespace port::render
