#pragma once
#include <array>
#include <cstdint>

namespace port::render
{
enum class SceneAlphaProgram : uint8_t { Standard, Dirt, Crowd, Movie };
enum class SceneAlphaTail : uint8_t
{
    DoublePrevious, Previous, PreviousTimesRaster, PreviousPlusTexture,
    PreviousPlusKonstSquared, Texture, PreviousPlusInverseKonstTexture,
    Zero, KonstSquared
};

// GLX updates only the alpha selectors/inputs named by a texture configuration.
// Keep that small amount of state on the packet producer; shader uniforms are
// resolved snapshots and remain independent of deferred submission order.
struct SceneAlphaState
{
    enum Selector : uint8_t { One, Diffuse, Shadow };
    std::array<Selector, 3> selectors{ One, One, One };
    // GXInit clears stage 2's input bits to APREV; the first GLX configuration
    // restores ADD/scale-one/clamp, giving clamp(PREV + PREV) until overwritten.
    SceneAlphaTail tail = SceneAlphaTail::DoublePrevious;
    SceneAlphaProgram program = SceneAlphaProgram::Standard;
    bool stage1RasterColor = false;
    uint8_t pending = 0;

    constexpr void observe(unsigned long flags, unsigned mask, SceneAlphaProgram next) noexcept
    {
        if (pending & 1u)
            stage1RasterColor = true;
        if (pending & 2u)
            flags |= 0x80u;
        pending = 0;
        if (flags & 2u)
        {
            if (program == SceneAlphaProgram::Movie && next != SceneAlphaProgram::Movie)
                selectors[1] = One;
            program = next;
        }
        if (!(flags & 0x80u))
            return;
        stage1RasterColor = false;
        if (program == SceneAlphaProgram::Crowd)
            return;
        switch (mask)
        {
        case 3:
            selectors[0] = selectors[1] = Diffuse;
            tail = SceneAlphaTail::PreviousTimesRaster;
            break;
        case 7:
            selectors[0] = Diffuse;
            selectors[1] = program == SceneAlphaProgram::Movie ? Shadow : Diffuse;
            tail = program == SceneAlphaProgram::Movie ? SceneAlphaTail::PreviousPlusTexture
                                                       : SceneAlphaTail::PreviousTimesRaster;
            break;
        case 19:
            selectors[0] = selectors[1] = Diffuse;
            tail = SceneAlphaTail::Previous;
            break;
        case 13: case 25:
            tail = SceneAlphaTail::Previous;
            break;
        case 35:
            if (program == SceneAlphaProgram::Dirt)
            {
                tail = SceneAlphaTail::Texture;
                break;
            }
            [[fallthrough]];
        case 39: case 43:
            selectors[1] = selectors[2] = Diffuse;
            tail = SceneAlphaTail::PreviousPlusInverseKonstTexture;
            break;
        case 41: tail = SceneAlphaTail::PreviousPlusKonstSquared; break;
        case 49: case 57: tail = SceneAlphaTail::Zero; break;
        case 53: tail = SceneAlphaTail::KonstSquared; break;
        default: break;
        }
    }

    constexpr void note_draw(unsigned mask, bool constantColor, bool alphaOverride,
                              bool environmentDiffuse) noexcept
    {
        // ConstantColour's cleanup changes stage 1 even on masks where the
        // draw override is unused. EnvDiffuse separately forces the next
        // configuration rebuild. Apply both before the next packet's flags.
        pending = uint8_t(constantColor && !alphaOverride && mask != 1 ? 1u : 0u) |
                  uint8_t(environmentDiffuse && (mask & 16u) ? 2u : 0u);
    }

    constexpr std::array<float, 4> resolve(float diffuse, float shadow) const noexcept
    {
        const std::array<float, 3> constants{ 1.f, diffuse, shadow };
        return { constants[selectors[0]], constants[selectors[1]], constants[selectors[2]],
                 float(tail) };
    }
};
} // namespace port::render
