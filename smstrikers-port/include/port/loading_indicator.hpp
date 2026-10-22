#pragma once

#include <array>
#include <cstdint>

namespace port::loading
{
struct Request
{
    bool submit = false, begin = false, otherPosition = false, blackBackground = false;
    uint32_t targetFps = 60, waitRetraces = 0;
};

// glxSwapLoading consumes its one-time delay on the first call, including OFF.
// Save/Restore preserve the retail flag contract without a clock or allocation.
class State
{
  public:
    constexpr Request change(bool begin, bool other, uint32_t fps,
                             bool blackBackground = false) noexcept
    {
        const Request request{ true, begin, other, blackBackground, fps,
                               first_ && fps <= UINT32_MAX / 2 ? fps * 2 : 0 };
        first_ = false;
        enabled_ = begin;
        return request;
    }
    constexpr Request save(uint32_t fps) noexcept
    {
        saved_ = enabled_;
        return enabled_ ? change(false, false, fps) : Request{};
    }
    constexpr Request restore(uint32_t fps) noexcept
    {
        if (saved_)
            return change(true, false, fps, true);
        const bool stop = enabled_;
        enabled_ = false;
        // Restoring a saved OFF flag does not reset the retail delay/phase.
        return { stop, false, false, false, fps, 0 };
    }
    constexpr void reject() noexcept { enabled_ = false; }
    constexpr bool enabled() const noexcept { return enabled_; }

  private:
    bool first_ = true, enabled_ = false, saved_ = false;
};

constexpr uint32_t phase_retraces(uint32_t fps) noexcept { return fps == 50 ? 16 : 19; }

constexpr std::array<std::array<float, 4>, 3> rectangles(const Request& request) noexcept
{
    const bool pal = request.targetFps == 50;
    const float y = pal ? (request.otherPosition ? 418.f : 436.f)
                        : (request.otherPosition ? 366.f : 382.f);
    // These are fixed 640-wide XFB coordinates, independent of world aspect.
    // Native glPlat always reports 60 Hz / 448 lines; the PAL source used 542.
    const float scaleY = 544.f / (pal ? 542.f : 448.f);
    std::array<std::array<float, 4>, 3> result{};
    for (unsigned i = 0; i < result.size(); ++i)
        result[i] = { (288.f + 24.f * i) * 1.5f, y * scaleY, 48.f, 32.f * scaleY };
    return result;
}

inline constexpr uint16_t normalPixels[32][32] = {
#include "NL/glx/_ImageData.inc"
};
inline constexpr uint16_t selectedPixels[32][32] = {
#include "NL/glx/_SelectedImageData.inc"
};

using Operations = std::array<uint8_t, 32 * 32 * 4>;

// Each RGBA texel is an operation on a shared-chroma XFB pair, not a color:
// R=Y, G/B=filtered U/V, A=85*(writeY + 2*writeUV). An odd visible source pixel
// writes chroma for both pixels, including its transparent even neighbour.
constexpr Operations operations(const uint16_t (&pixels)[32][32]) noexcept
{
    Operations result{};
    for (unsigned y = 0; y < 32; ++y)
    {
        for (unsigned x = 0; x < 32; ++x)
        {
            const unsigned odd = x | 1u;
            const unsigned previous = odd >= 2 ? odd - 2 : 0;
            const auto component = [&](unsigned column, unsigned shift) {
                return unsigned((pixels[y][column] >> shift) & 15u) * 17u;
            };
            const bool writeY = (pixels[y][x] & 15u) != 0;
            const bool writeUV = (pixels[y][odd] & 15u) != 0;
            const unsigned at = (y * 32 + x) * 4;
            result[at] = uint8_t(component(x, 12));
            result[at + 1] = uint8_t((component(previous, 8) +
                2 * component(odd - 1, 8) + component(odd, 8)) / 4);
            result[at + 2] = uint8_t((component(previous, 4) +
                2 * component(odd - 1, 4) + component(odd, 4)) / 4);
            result[at + 3] = uint8_t(85 * (unsigned(writeY) + 2 * unsigned(writeUV)));
        }
    }
    return result;
}

inline constexpr auto normalOperations = operations(normalPixels);
inline constexpr auto selectedOperations = operations(selectedPixels);
}   // namespace port::loading
