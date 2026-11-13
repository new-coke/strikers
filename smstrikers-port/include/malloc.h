#ifndef PORT_MSL_MALLOC_H
#define PORT_MSL_MALLOC_H
// Port shim for MSL's <malloc.h>: malloc and friends come from <stdlib.h> and src/platform/msl_arena.cpp implements the arena helpers.
//
// This header shadows the C library's own <malloc.h> wherever the platform has one, and on Android
// that matters: bionic's <stdlib.h> includes <malloc.h> on its way through, so this file is read in
// the middle of <stdlib.h> rather than after it. Reaching back for <stdlib.h> from here re-enters
// it at a point where ldiv_t and lldiv_t are not declared yet, and libc++'s <stdlib.h> then fails on
// its own div() overloads. So on Android: the real <malloc.h>, which declares malloc and free just
// as <stdlib.h> would, and nothing that can re-enter.
#if defined(__ANDROID__)
#include_next <malloc.h>
#else
#include <stdlib.h>
#endif
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void MallocInit(void* base, size_t len);
void MallocCleanup(void);
int  MallocIsInitalized(void);
void GetFreeArena(size_t* max_size, size_t* free_size, size_t* alloc_size);
int  CheckArena(void);
void DisplayArena(void);
#ifdef __cplusplus
}
#endif
#endif // PORT_MSL_MALLOC_H
