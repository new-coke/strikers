#ifndef PORT_MSL_MALLOC_H
#define PORT_MSL_MALLOC_H
// Port shim for MSL's <malloc.h>: malloc and friends come from <stdlib.h> and src/platform/msl_arena.cpp implements the arena helpers.
#include <stdlib.h>
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
