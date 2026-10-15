#pragma once
namespace port::startup {
#if defined(STRIKERS_VITA_NATIVE_RUNTIME) || defined(STRIKERS_BOOT_SCREEN_TEST)
bool begin() noexcept;
void show(const char* status, unsigned current=0, unsigned total=0) noexcept;
// Called after a real game swap; release only after scanout has replaced us.
bool retire_if_replaced() noexcept;
void shutdown() noexcept;
#else
inline bool begin() noexcept { return true; }
inline void show(const char*, unsigned=0, unsigned=0) noexcept {}
inline bool retire_if_replaced() noexcept { return true; }
inline void shutdown() noexcept {}
#endif
}
