#ifndef _VM_H_
#define _VM_H_

#include "types.h"
#include <stdint.h> // for uintptr_t
#include <stddef.h> // for size_t

#ifdef __cplusplus
extern "C" {
#endif

void VMInit(uintptr_t baseAddr, size_t initialCommitSize, uintptr_t limitAddr);
void VMAlloc(uintptr_t address, size_t size);

typedef void (*VMLogStatsCallback)(u32 faultAddr, u32 mainAddr, u32 pageIndex, u32 elapsed, u32 wroteBack);
void VMSetLogStatsCallback(VMLogStatsCallback cb);

#ifdef __cplusplus
}
#endif

#endif // _VM_H_
