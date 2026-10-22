#pragma once

class Event;

namespace port::control_trace
{
// Main-thread diagnostics only. Call-site guards avoid evaluating world data OFF.
extern bool enabled;
void begin_match(bool rematch, bool straight_to_kickoff) noexcept;
void end_match() noexcept;
void after_fixed_update(float simulation_time) noexcept;
void observe_event(Event* event) noexcept;
}
