#ifndef PORT_AURORA_COMPAT_H
#define PORT_AURORA_COMPAT_H
// SDK entry points Aurora does not provide, defined in aurora_compat.c and compiled only into the
// Aurora build.

#if defined(PORT_USE_AURORA)

#include <dolphin/types.h>
#include <dolphin/gx.h>

#ifdef __cplusplus
extern "C" {
#endif


// Aurora has GXDrawDone, which flushes and waits, and GXSetDrawDone, which only signals; this is
// the wait-only half.
void GXWaitDrawDone(void);


// Poll rate in lines from VSync. Aurora samples from its own SDL pump, so this is bookkeeping.
void SISetSamplingRate(u32 samplingRate);


typedef void (*PADSamplingCallback)(void);

// Once per sampled frame; Aurora's PAD has no such hook, so the frame loop calls it.
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback);

void GXClearGPMetric(void);
void GXSetGPMetric(u32 perf0, u32 perf1);
void GXReadGPMetric(u32* cnt0, u32* cnt1);

void GXInitFogAdjTable(void* table, u16 width, const f32 projmtx[4][4]);

// Render modes Aurora does not define, populated at startup from its equivalents.
extern GXRenderModeObj GXNtsc480Prog;
extern GXRenderModeObj GXEurgb60Hz480IntDf;


// Aurora's card module defines CARDFormatAsync but its card.h does not declare it; a definition
// here would be a duplicate symbol.
typedef void (*CARDCallbackCompat)(s32 chan, s32 result);
s32 CARDFormatAsync(s32 chan, CARDCallbackCompat callback);

#ifdef __cplusplus
}
#endif

#endif // PORT_USE_AURORA
#endif // PORT_AURORA_COMPAT_H
