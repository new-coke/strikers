#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Owner-thread diagnostic hook. The caller supplies storage and the same
// monotonic clock used by its frame timeline; no hook means no measurements.
typedef uint64_t (*PortVglTimelineClock)(void);
typedef struct PortVglPresentProfile
{
    uint64_t addEntryNs, gcNs;
    uint32_t addEntryCalls, gcCalls, addEntryErrors, gcErrors;
    int32_t lastAddEntryResult, lastGcResult;
} PortVglPresentProfile;
void vglSetPresentProfile(PortVglPresentProfile*, PortVglTimelineClock);

#ifdef __cplusplus
}
#endif
