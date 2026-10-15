#pragma once
#include <cstdint>

namespace port
{
// The intro owns the render snapshot throughout NIS and the transition to play.
constexpr bool nis_controls_render_snapshot(uint32_t current, uint32_t previous) noexcept
{
    return current == 0x100 || (previous == 0x100 && current == 1);
}
}
