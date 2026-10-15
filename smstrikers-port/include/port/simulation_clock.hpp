#pragma once
#include <cstdint>

namespace port
{
// Sample once per task pass, before any task runs. Rendering cost is included
// in the next interval; no task's position in the list changes game time.
class SimulationFrameClock
{
    uint64_t previous_ = 0;
    bool initialized_ = false;
public:
    void reset() noexcept { initialized_ = false; }
    float sample(uint64_t now) noexcept
    {
        const auto elapsed = initialized_ && now >= previous_ ? now - previous_ : 0;
        previous_ = now;
        initialized_ = true;
        // Treat long suspend/debugger/loading stalls as discontinuities.
        // Ordinary rendering hitches retain their full elapsed duration.
        return elapsed <= 2000000000ull ? float(double(elapsed) * 1.e-9) : 0.f;
    }
};
inline bool simulation_step_budget(unsigned completed) noexcept { return completed < 16; }
} // namespace port
