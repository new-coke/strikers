#include "musyx/hardware.h"

void* salMalloc(size_t len) { return salHooks.malloc(len); }

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
void* salMallocPhysical(size_t len) { return salHooks.mallocPhysical(len); }
#endif

void salFree(void* addr) { salHooks.free(addr); }
