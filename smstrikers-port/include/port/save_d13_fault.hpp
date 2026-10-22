#pragma once
// Offline proposal: enabled only in a separately reviewed diagnostic D13 build.
#if !defined(STRIKERS_SAVE_D13_FAULT) || STRIKERS_SAVE_D13_FAULT != 1
#error "save_d13_fault.hpp must not enter the ordinary build"
#endif
#if !defined(PORT_VITA) || !defined(STRIKERS_VITA_NATIVE_RUNTIME)
#error "D13 is specific to the real native Vita backend"
#endif

namespace port::save_d13
{
void initialize() noexcept;
bool fail_replacement(unsigned long slot, bool fileNull, unsigned long logical,
                      unsigned long capacity, const void* owner, const void* alias,
                      unsigned long oldCapacity) noexcept;
void note_create() noexcept;
void note_save_callback(long result) noexcept;
void note_action(bool retry) noexcept;
void observe_ui(bool present, bool prompt, bool ready, bool locked, bool visible,
                bool busy, bool started, bool finished, bool continued, bool success,
                int type, int next, long result) noexcept;
void sample_frontend() noexcept; // Defined beside the real private SHSaveLoad flags.
void drain(unsigned long frame) noexcept;
void shutdown(unsigned long frame) noexcept;
}
