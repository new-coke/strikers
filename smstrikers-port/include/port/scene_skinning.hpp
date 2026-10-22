#pragma once
#include <array>
#include <cstdint>

namespace port::render
{
// The native packet contract accepts up to 32 rigid bones. Slot zero remains
// unskinned; stitch index i selects slot i+1, or GX row 3*(i+1). The original
// userdata stores register 3*i-96 and glud_Skin loads it at register+99.
inline constexpr unsigned kSceneSkinBoneCount = 32;
inline constexpr unsigned kSceneBoneSlots = kSceneSkinBoneCount + 1;
inline constexpr unsigned kSceneBoneRows = kSceneBoneSlots * 3;
inline constexpr unsigned kSceneBoneFloats = kSceneBoneRows * 4;
inline constexpr int kSceneSkinMinRegister = -96;
inline constexpr int kSceneSkinMaxRegister = kSceneSkinMinRegister + 3 * int(kSceneSkinBoneCount - 1);
inline constexpr unsigned kSceneSkinMaxSelector = 3 * kSceneSkinBoneCount;
static_assert(kSceneSkinMaxSelector <= UINT8_MAX);
static_assert(kSceneSkinMaxRegister + 99 + 2 == int(kSceneBoneRows) - 1);
using SceneBonePalette = std::array<std::array<float, 4>, kSceneBoneRows>;
} // namespace port::render
