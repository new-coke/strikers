#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "dolphin/si.h"
#include "dolphin/os.h"
extern "C" {
// Native PAD reads SceCtrl every host frame; retain the requested retail poll
// interval as metadata without starting the GameCube serial interface.
static u32 samplingMilliseconds = 0;
void SISetSamplingRate(u32 milliseconds) { samplingMilliseconds = milliseconds; }
}
#endif
