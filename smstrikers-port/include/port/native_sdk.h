#pragma once
#include <dolphin/gx/GXStruct.h>
#include <dolphin/pad.h>
#ifdef __cplusplus
extern "C" {
#endif
void VILockAspectRatio(int numerator,int denominator);
void VIUnlockAspectRatio(void);
void GXWaitDrawDone(void);
void GXVitaClearEfb(void);
#ifdef __cplusplus
}
#endif
