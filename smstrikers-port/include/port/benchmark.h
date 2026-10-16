
// Wall-clock frame rate cannot measure headroom: paced by VIWaitForRetrace and presented through a
// swapchain, a machine that keeps up reports the refresh rate whatever the CPU is doing.

// So this measures where the frame's time goes: `busy`, the tasks phase minus the limiter's own
// sleep, is what decides whether a slower machine holds the rate.

#ifndef _PORT_BENCHMARK_H_
#define _PORT_BENCHMARK_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void PortBenchInit(void);

typedef enum PortBenchCaptureState {
    PORT_CAPTURE_IDLE, PORT_CAPTURE_COLLECTING, PORT_CAPTURE_COMPLETE, PORT_CAPTURE_ERROR
} PortBenchCaptureState;
typedef struct PortBenchCaptureStatus {
    PortBenchCaptureState state;
    size_t samples, requested, warmupRemaining;
    int errorCode; /* errno; zero unless allocation or output failed */
} PortBenchCaptureStatus;
/* Read-only RAM status; neither function is called automatically per frame. */
void PortBenchGetCaptureStatus(PortBenchCaptureStatus* out);
/* Retry a fully collected capture after output failure, using the same path. */
int PortBenchRetryCapture(void);

void PortBenchFrameBegin(void);
void PortBenchAfterTasks(void);
void PortBenchFrameEnd(void);
// Game state, separate from renderer loading/scene boundaries. Only live play
// and overtime qualify; intros, replays and post-goal scenes do not.
void PortBenchSetPlayActive(int active);

void PortBenchAddSleep(unsigned long long ns);

// Time blocked in aurora_begin_frame on the swapchain acquire; counted in the frame total, not busy.
void PortBenchAddAcquire(unsigned long long ns);

// The deferred limiter sleep before the frame begins; it counts toward the frame total and the sleep time.
void PortBenchAddPreFrameSleep(unsigned long long ns);

// A rolling window rather than the whole run, because a run-length mean stops moving after a
// minute.

typedef struct PortBenchLive
{
    double busyMs;        // most recent frame
    double presentMs;
    double frameMs;
    double sleepMs;
    double busyP95Ms;     // over the rolling window
    double fps;           // over the rolling window
    double worstMs;       // worst frame total seen this run, and when
    unsigned long worstFrame;
    unsigned long frames;      // recorded frames (match only)
    int matchActive;
    int playActive;
    unsigned long playFrames;   // monotonic across renderer scene transitions
} PortBenchLive;

void PortBenchGetLive(PortBenchLive* out);
// Rolling full-frame rate without percentiles, sorting, renderer snapshots or I/O.
double PortBenchGetFps(void);

size_t PortBenchGetHistory(float* busyMs, float* frameMs, size_t cap);

// Renderer-side counters are sampled by the Vita host loop and kept in memory.
// This deliberately does not log or write files from gameplay.
typedef struct PortBenchRendererStats
{
    int valid;
    int shaderRuntimeCompilationEnabled;
    unsigned long long shaderRuntimeCompiles;
    unsigned long long shaderRuntimeCompileUs;
    unsigned long long shaderCompileBlockedMisses;
    unsigned int shaderDiskCacheHits;
    unsigned int shaderDiskCacheMisses;
    size_t pipelineEntries;
    size_t pipelineBudget;
    unsigned long long pipelineEvictions;
    size_t registeredStageCount;
    size_t sharedVertexProgramCount;
    size_t sharedFragmentProgramCount;
    unsigned long long stageRegistrationCreates;
    unsigned long long stageRegistrationReuses;
    unsigned long long sharedVertexProgramCreates;
    unsigned long long sharedVertexProgramReuses;
    unsigned long long sharedVertexProgramEvictions;
    unsigned long long sharedFragmentProgramCreates;
    unsigned long long sharedFragmentProgramReuses;
    unsigned long long sharedFragmentProgramEvictions;

    unsigned long long frameUs;
    unsigned long long rendererCpuFrameUs;
    unsigned long long displayQueueLastUs;
    unsigned long long displayQueueMaxUs;
    unsigned long long displayQueueTotalUs;
    unsigned long long displayQueueAverageUs;
    unsigned long long displayQueueSamples;
    unsigned long long displayQueueBlockedSamples;
    unsigned int displayQueueBlockedPercent;
    int gpuBackpressureLikely;
    int nativeTimingsSampled;
    unsigned long long nativePipelineUs;
    unsigned long long nativeTextureUs;
    unsigned long long nativeDrawUs;
    unsigned int nativeSceneCount;
    unsigned int nativeEfbCopies;
    /* gxm-optimization testbed: per-frame native counters (last frame) and
     * their sums over the measured gameplay window. */
    unsigned long long frameIndex;
    unsigned int nativeDepthLoadScenes;
    unsigned int nativeDepthStoreScenes;
    unsigned int nativeDepthlessScenes;
    unsigned int nativeFinishCalls;
    unsigned int nativeScissorFreeDraws;
    unsigned long long gameplayCounterFrames;
    unsigned long long gameplayScenes;
    unsigned long long gameplayDepthLoadScenes;
    unsigned long long gameplayDepthStoreScenes;
    unsigned long long gameplayDepthlessScenes;
    unsigned long long gameplayFinishCalls;
    unsigned long long gameplayScissorFreeDraws;
    unsigned long long nativeEfbEndSceneUs;
    unsigned long long nativeEfbTransferSubmitUs;
    unsigned long long nativeEfbTransferWaitUs;
    unsigned long long nativeEfbCpuFixupUs;
    unsigned long long drawFrontendUs;
    unsigned long long stateTranslateUs;
    unsigned long long vertexDecodeUs;
    unsigned long long vertexTransformUs;
    unsigned long long textureResolveUs;
    unsigned long long pipelineResolveUs;
    unsigned long long commandBuildUs;
    unsigned long long submitUs;
    unsigned long long bufferUploadUs;
    unsigned long long streamWaitUs;
    unsigned long long vertexPackUs;
    unsigned long long geometryCacheUs;
    unsigned long long geometryKeyUs;
    unsigned long long geometryValidateUs;
    unsigned long long efbCopyUs;
    unsigned long long presentUs;
    unsigned long long draws;
    unsigned long long vertices;
    unsigned long long triangles;
    unsigned long long pipelineHits;
    unsigned long long pipelineMisses;
    unsigned long long textureHits;
    unsigned long long textureMisses;
    unsigned long long textureUploads;
    unsigned long long textureUploadBytes;
    unsigned long long arenaOverflows;

    unsigned long long staticGeometryHits;
    unsigned long long staticGeometryMisses;
    unsigned long long staticGeometryLookupFallbacks;
    size_t staticGeometryBytes;
    size_t staticGeometryEntries;
    unsigned int worldCullTested;
    unsigned int worldCullDropped;

    // Baseline/deltas captured at the loading -> gameplay boundary.
    unsigned long long shaderRuntimeCompilesAtGameplayStart;
    unsigned long long shaderRuntimeCompilesDuringGameplay;
    unsigned long long shaderCompileBlockedMissesDuringGameplay;
} PortBenchRendererStats;

void PortBenchSetRendererStats(const PortBenchRendererStats* stats);
void PortBenchRendererGameplayStart(void);
void PortBenchRendererGameplayEnd(void);
void PortBenchGetRendererStats(PortBenchRendererStats* out);
void PortBenchSetCullStats(unsigned int tested, unsigned int dropped);

#ifdef __cplusplus
}
#endif

#endif // _PORT_BENCHMARK_H_
