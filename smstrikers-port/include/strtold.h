#ifndef PORT_MSL_STRTOLD_H
#define PORT_MSL_STRTOLD_H
// Port shim for MSL's <strtold.h>: host stdlib provides strtol/atof.
#include <stdlib.h>
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif
f128 __strtold(int max_width, int (*ReadProc)(void*, int, int), void* ReadProcArg, int* chars_scanned, int* overflow);
#ifdef __cplusplus
}
#endif
#endif // PORT_MSL_STRTOLD_H
