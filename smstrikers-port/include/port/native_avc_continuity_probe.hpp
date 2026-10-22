#pragma once
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
// Absent, empty or "0" means OFF. Any other value selects an exclusive run label.
bool PortAvcContinuityProbeRequested() noexcept;
int PortRunAvcContinuityProbe() noexcept;
#endif
