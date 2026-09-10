// Fails if include/port/prelude.h is not force-included; a check inside it cannot say so.

#if !defined(PORT_PRELUDE_H)
#error "include/port/prelude.h is not being force-included, so the compile contract (signed char, 16-bit wchar_t, host checks) is not in effect. See target_compile_options(port_flags) in CMakeLists.txt."
#endif

// Clang's Windows intrin.h declares __nop(void); this catches a prelude macro of that name.
#if defined(_WIN32)
#include <intrin.h>
#endif

// ISO C does not allow an empty translation unit.
const char port_prelude_check[] = "prelude present";
