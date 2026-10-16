// Frame-time accounting for benchmark runs.

#include "port/benchmark.h"
#include "port/host.h"
#include "port/config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#if defined(PORT_VITA)
#include <psp2/power.h>
#endif

static int s_read;
// Opt-in finite capture independent of diagnostics/overlay. No per-frame I/O:
// fixed storage is written once after the requested sample, with capture I/O
// excluded from that sample. Startup, menus and gameplay remain identifiable.
#define CAPTURE_LIMIT 8192
typedef struct FrameCapture { unsigned int frameUs, tasksUs, presentUs, sleepUs; unsigned long matchFrame; int match; } FrameCapture;
static FrameCapture* s_capture;
static size_t s_captureCount, s_captureGoal;
static char s_capturePath[1024];
static PortBenchCaptureState s_captureState;
static int s_captureError;
static int s_captureMatchOnly;
static size_t s_captureSkip;
static int s_capturePlayOnly, s_playActive;
static unsigned long s_playFrameCounter;

static int capture_active(void) { return s_captureState == PORT_CAPTURE_COLLECTING; }
static void finish_capture(void)
{
    // Stop sampling before output, including on failure. Keep all samples for
    // an explicit retry; never retry I/O from the game's frame loop.
    s_captureState = PORT_CAPTURE_ERROR;
    errno = 0;
    FILE* f = fopen(s_capturePath, "wb");
    if (f == NULL) {s_captureError=errno ? errno : EIO;return;}
    int failed = 0;
#define CAPTURE_WRITE(...) do { if (!failed && fprintf(f, __VA_ARGS__) < 0) failed=errno ? errno : EIO; } while (0)
    CAPTURE_WRITE("# strikers-frame-capture-v1 diagnostics=%d fps_overlay=%d\n", PortDiagnosticsEnabled(), PortFpsOverlayEnabled());
    CAPTURE_WRITE("# play_only=%d counter=%s\n", s_capturePlayOnly,
                  s_capturePlayOnly ? "live_play" : "renderer_scene");
#if defined(PORT_VITA)
    CAPTURE_WRITE("# actual_cpu_mhz=%d actual_gpu_mhz=%d actual_bus_mhz=%d actual_xbar_mhz=%d\n",
        scePowerGetArmClockFrequency(),scePowerGetGpuClockFrequency(),scePowerGetBusClockFrequency(),scePowerGetGpuXbarClockFrequency());
#endif
    CAPTURE_WRITE("sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n");
    for (size_t i=0;i<s_captureCount && !failed;++i) {
        const FrameCapture* r=&s_capture[i];
        // Vita's small newlib printf does not implement the z length modifier.
        CAPTURE_WRITE("%lu,%d,%lu,%u,%u,%u,%u\n",(unsigned long)i,r->match,r->matchFrame,r->frameUs,r->tasksUs,r->presentUs,r->sleepUs);
    }
#undef CAPTURE_WRITE
    if (ferror(f) && !failed) failed=errno ? errno : EIO;
    if (fclose(f) != 0 && !failed) failed=errno ? errno : EIO;
    s_captureError=failed;
    if (failed) return;
    s_captureState=PORT_CAPTURE_COMPLETE;
    free(s_capture);s_capture=NULL;
}

void PortBenchGetCaptureStatus(PortBenchCaptureStatus* out)
{
    if (out) *out=(PortBenchCaptureStatus){s_captureState,s_captureCount,s_captureGoal,s_captureSkip,s_captureError};
}
int PortBenchRetryCapture(void)
{
    if (s_captureState != PORT_CAPTURE_ERROR || !s_capture || s_captureCount != s_captureGoal)
        return 0;
    finish_capture();
    return s_captureState == PORT_CAPTURE_COMPLETE;
}

static unsigned long long s_t0;   // start of the current frame
static unsigned long long s_tTasks;
static unsigned long long s_sleepThisFrame;
static unsigned long long s_acquireThisFrame;   // blocked in aurora_begin_frame
static unsigned long long s_preSleepThisFrame;  // limiter sleep deferred to the top of the frame
static int s_matchActive;

// Minimal frame timing remains available for the FPS overlay when diagnostics
// are OFF. Both OFF suppress optional timing collection entirely.
#define LIVE_WINDOW 256

static unsigned int s_liveBusyUs[LIVE_WINDOW];
static unsigned int s_liveFrameUs[LIVE_WINDOW];
static size_t s_liveNext;
static size_t s_liveFilled;
static unsigned long long s_liveFrameSumUs;
static unsigned int s_lastBusyUs, s_lastPresentUs, s_lastFrameUs, s_lastSleepUs;
static unsigned long s_matchFrameCounter;
static unsigned int s_worstUs;
static unsigned long s_worstFrame;

static PortBenchRendererStats s_rendererStats;
static unsigned long long s_rendererGameplayStartCompiles;
static unsigned long long s_rendererGameplayStartBlocked;
static unsigned long long s_rendererGameplayFinalCompiles;
static unsigned long long s_rendererGameplayFinalBlocked;
static unsigned long long s_rendererGameplayStartQueueUs;
static unsigned long long s_rendererGameplayStartQueueSamples;
static unsigned long long s_rendererGameplayStartQueueBlocked;
static unsigned long long s_rendererGameplayFinalQueueUs;
static unsigned long long s_rendererGameplayFinalQueueSamples;
static unsigned long long s_rendererGameplayFinalQueueBlocked;
static int s_rendererGameplayBaselineValid;
static int s_rendererGameplayEnded;
static unsigned int s_worldCullTested;
static unsigned int s_worldCullDropped;

static void update_gameplay_display_queue_stats(void)
{
    unsigned long long totalUs;
    unsigned long long samples;
    unsigned long long blocked;

    if (!s_rendererGameplayBaselineValid)
        return;

    totalUs = s_rendererGameplayEnded ? s_rendererGameplayFinalQueueUs
                                      : s_rendererStats.displayQueueTotalUs;
    samples = s_rendererGameplayEnded ? s_rendererGameplayFinalQueueSamples
                                      : s_rendererStats.displayQueueSamples;
    blocked = s_rendererGameplayEnded ? s_rendererGameplayFinalQueueBlocked
                                      : s_rendererStats.displayQueueBlockedSamples;

    totalUs = totalUs >= s_rendererGameplayStartQueueUs
                  ? totalUs - s_rendererGameplayStartQueueUs
                  : 0;
    samples = samples >= s_rendererGameplayStartQueueSamples
                  ? samples - s_rendererGameplayStartQueueSamples
                  : 0;
    blocked = blocked >= s_rendererGameplayStartQueueBlocked
                  ? blocked - s_rendererGameplayStartQueueBlocked
                  : 0;

    s_rendererStats.displayQueueAverageUs = samples ? totalUs / samples : 0;
    s_rendererStats.displayQueueBlockedPercent =
        samples ? (unsigned int)((blocked * 100ull) / samples) : 0;
}

static unsigned long long s_counterLastFrame = ~0ull;
static unsigned long long s_counterFrames, s_counterScenes, s_counterDepthLoad, s_counterDepthStore,
    s_counterDepthless, s_counterFinish, s_counterScissorFree;

void PortBenchSetRendererStats(const PortBenchRendererStats* stats)
{
    if (stats == NULL)
        return;
    if (s_rendererGameplayBaselineValid && !s_rendererGameplayEnded && stats->frameIndex != s_counterLastFrame)
    {
        s_counterLastFrame = stats->frameIndex;
        s_counterFrames++;
        s_counterScenes += stats->nativeSceneCount;
        s_counterDepthLoad += stats->nativeDepthLoadScenes;
        s_counterDepthStore += stats->nativeDepthStoreScenes;
        s_counterDepthless += stats->nativeDepthlessScenes;
        s_counterFinish += stats->nativeFinishCalls;
        s_counterScissorFree += stats->nativeScissorFreeDraws;
    }
    s_rendererStats = *stats;
    s_rendererStats.gameplayCounterFrames = s_counterFrames;
    s_rendererStats.gameplayScenes = s_counterScenes;
    s_rendererStats.gameplayDepthLoadScenes = s_counterDepthLoad;
    s_rendererStats.gameplayDepthStoreScenes = s_counterDepthStore;
    s_rendererStats.gameplayDepthlessScenes = s_counterDepthless;
    s_rendererStats.gameplayFinishCalls = s_counterFinish;
    s_rendererStats.gameplayScissorFreeDraws = s_counterScissorFree;
    s_rendererStats.worldCullTested = s_worldCullTested;
    s_rendererStats.worldCullDropped = s_worldCullDropped;
    if (s_rendererGameplayBaselineValid)
    {
        s_rendererStats.shaderRuntimeCompilesAtGameplayStart = s_rendererGameplayStartCompiles;
        if (s_rendererGameplayEnded)
        {
            s_rendererStats.shaderRuntimeCompilesDuringGameplay = s_rendererGameplayFinalCompiles;
            s_rendererStats.shaderCompileBlockedMissesDuringGameplay = s_rendererGameplayFinalBlocked;
        }
        else
        {
            s_rendererStats.shaderRuntimeCompilesDuringGameplay =
                stats->shaderRuntimeCompiles >= s_rendererGameplayStartCompiles
                    ? stats->shaderRuntimeCompiles - s_rendererGameplayStartCompiles
                    : 0;
            s_rendererStats.shaderCompileBlockedMissesDuringGameplay =
                stats->shaderCompileBlockedMisses >= s_rendererGameplayStartBlocked
                    ? stats->shaderCompileBlockedMisses - s_rendererGameplayStartBlocked
                    : 0;
        }
        update_gameplay_display_queue_stats();
    }
}

void PortBenchRendererGameplayStart(void)
{
    s_matchActive = 1;
    s_matchFrameCounter = 0;
    s_worstUs = 0;
    s_worstFrame = 0;
    s_counterLastFrame = ~0ull;
    s_counterFrames = s_counterScenes = s_counterDepthLoad = s_counterDepthStore = 0;
    s_counterDepthless = s_counterFinish = s_counterScissorFree = 0;
    s_rendererGameplayStartCompiles = s_rendererStats.shaderRuntimeCompiles;
    s_rendererGameplayStartBlocked = s_rendererStats.shaderCompileBlockedMisses;
    s_rendererGameplayStartQueueUs = s_rendererStats.displayQueueTotalUs;
    s_rendererGameplayStartQueueSamples = s_rendererStats.displayQueueSamples;
    s_rendererGameplayStartQueueBlocked = s_rendererStats.displayQueueBlockedSamples;
    s_rendererGameplayBaselineValid = s_rendererStats.valid;
    s_rendererGameplayEnded = 0;
    s_rendererGameplayFinalCompiles = 0;
    s_rendererGameplayFinalBlocked = 0;
    s_rendererGameplayFinalQueueUs = 0;
    s_rendererGameplayFinalQueueSamples = 0;
    s_rendererGameplayFinalQueueBlocked = 0;
    if (s_rendererGameplayBaselineValid)
    {
        s_rendererStats.shaderRuntimeCompilesAtGameplayStart = s_rendererGameplayStartCompiles;
        s_rendererStats.shaderRuntimeCompilesDuringGameplay = 0;
        s_rendererStats.shaderCompileBlockedMissesDuringGameplay = 0;
        s_rendererStats.displayQueueAverageUs = 0;
        s_rendererStats.displayQueueBlockedPercent = 0;
    }
}

void PortBenchRendererGameplayEnd(void)
{
    s_matchActive = 0;
    if (!s_rendererGameplayBaselineValid)
        return;
    s_rendererGameplayFinalCompiles =
        s_rendererStats.shaderRuntimeCompiles >= s_rendererGameplayStartCompiles
            ? s_rendererStats.shaderRuntimeCompiles - s_rendererGameplayStartCompiles
            : 0;
    s_rendererGameplayFinalBlocked =
        s_rendererStats.shaderCompileBlockedMisses >= s_rendererGameplayStartBlocked
            ? s_rendererStats.shaderCompileBlockedMisses - s_rendererGameplayStartBlocked
            : 0;
    s_rendererGameplayFinalQueueUs = s_rendererStats.displayQueueTotalUs;
    s_rendererGameplayFinalQueueSamples = s_rendererStats.displayQueueSamples;
    s_rendererGameplayFinalQueueBlocked = s_rendererStats.displayQueueBlockedSamples;
    s_rendererStats.shaderRuntimeCompilesDuringGameplay = s_rendererGameplayFinalCompiles;
    s_rendererStats.shaderCompileBlockedMissesDuringGameplay = s_rendererGameplayFinalBlocked;
    s_rendererGameplayEnded = 1;
    update_gameplay_display_queue_stats();
}

void PortBenchGetRendererStats(PortBenchRendererStats* out)
{
    if (out != NULL)
        *out = s_rendererStats;
}

void PortBenchSetCullStats(unsigned int tested, unsigned int dropped)
{
    s_worldCullTested = tested;
    s_worldCullDropped = dropped;
    s_rendererStats.worldCullTested = tested;
    s_rendererStats.worldCullDropped = dropped;
}

void PortBenchInit(void)
{
    if (s_read)
        return;
    s_read = 1;
    const char* path=getenv("STRIKERS_FRAME_CAPTURE");
    const char* count=getenv("STRIKERS_FRAME_CAPTURE_FRAMES");
    if(path && *path && strlen(path)<sizeof s_capturePath && count && *count) {
        char* end=NULL;
        const unsigned long n=strtoul(count,&end,10);
        if(end && !*end && n && n<=CAPTURE_LIMIT) {
            s_captureGoal=n;
            s_capture=calloc(n,sizeof(*s_capture));
            if(s_capture){strcpy(s_capturePath,path);s_captureState=PORT_CAPTURE_COLLECTING;}
            else {s_captureState=PORT_CAPTURE_ERROR;s_captureError=ENOMEM;}
        }
    }
    const char* only=getenv("STRIKERS_FRAME_CAPTURE_MATCH_ONLY");
    s_captureMatchOnly=only&&strcmp(only,"1")==0;
    const char* play=getenv("STRIKERS_FRAME_CAPTURE_PLAY_ONLY");
    s_capturePlayOnly=play&&strcmp(play,"1")==0;
    const char* skip=getenv("STRIKERS_FRAME_CAPTURE_SKIP");
    if(skip&&*skip){char* end=NULL;unsigned long n=strtoul(skip,&end,10);if(end&&!*end&&n<=100000)s_captureSkip=n;}
}

void PortBenchSetPlayActive(int active)
{
    s_playActive = active != 0;
}

void PortBenchFrameBegin(void)
{
    if (!PortDiagnosticsEnabled() && !PortFpsOverlayEnabled() && !capture_active())
        return;
    s_t0 = port_monotonic_ns();
    s_sleepThisFrame = 0;
    s_acquireThisFrame = 0;
}

void PortBenchAddPreFrameSleep(unsigned long long ns)
{
    s_preSleepThisFrame += ns;
}

void PortBenchAddAcquire(unsigned long long ns)
{
    s_acquireThisFrame = ns;
}

void PortBenchAfterTasks(void)
{
    s_tTasks = (PortDiagnosticsEnabled() || capture_active()) ? port_monotonic_ns() : s_t0;
}

double PortBenchGetFps(void)
{
    return s_liveFrameSumUs ? (double)s_liveFilled * 1e6 / (double)s_liveFrameSumUs : 0.0;
}

void PortBenchAddSleep(unsigned long long ns)
{
    s_sleepThisFrame += ns;
}

size_t PortBenchGetHistory(float* busyMs, float* frameMs, size_t cap)
{
    size_t n = s_liveFilled, i, start;
    if (n > cap)
        n = cap;
    // Oldest first: the ring's next slot is the oldest entry once it is full.
    start = (s_liveFilled < LIVE_WINDOW) ? 0 : s_liveNext;
    if (s_liveFilled > cap)
        start = (start + (s_liveFilled - cap)) % LIVE_WINDOW;
    for (i = 0; i < n; i++)
    {
        size_t k = (start + i) % LIVE_WINDOW;
        if (busyMs != NULL)
            busyMs[i] = (float)s_liveBusyUs[k] / 1000.0f;
        if (frameMs != NULL)
            frameMs[i] = (float)s_liveFrameUs[k] / 1000.0f;
    }
    return n;
}

void PortBenchGetLive(PortBenchLive* out)
{
    size_t n, i;

    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));

    out->busyMs = (double)s_lastBusyUs / 1000.0;
    out->presentMs = (double)s_lastPresentUs / 1000.0;
    out->frameMs = (double)s_lastFrameUs / 1000.0;
    out->sleepMs = (double)s_lastSleepUs / 1000.0;
    out->worstMs = (double)s_worstUs / 1000.0;
    out->worstFrame = s_worstFrame;
    out->frames = s_matchFrameCounter;
    out->matchActive = s_matchActive;
    out->playActive = s_playActive;
    out->playFrames = s_playFrameCounter;

    n = s_liveFilled;
    if (n == 0)
        return;

    out->fps = PortBenchGetFps();
    if (!PortDiagnosticsEnabled())
        return;

    // p95 of the window, by partial selection into the scratch the report already owns. n is 256,
    // so an insertion pass is cheaper than qsort and does not allocate, this runs every frame the
    // overlay is drawn.
    {
        unsigned int win[LIVE_WINDOW];
        size_t j;
        memcpy(win, s_liveBusyUs, n * sizeof(unsigned int));
        for (i = 1; i < n; i++)
        {
            unsigned int v = win[i];
            for (j = i; j > 0 && win[j - 1] > v; j--)
                win[j] = win[j - 1];
            win[j] = v;
        }
        out->busyP95Ms = (double)win[(size_t)((double)n * 0.95)] / 1000.0;
    }
}

void PortBenchFrameEnd(void)
{
    if (!PortDiagnosticsEnabled() && !PortFpsOverlayEnabled() && !capture_active())
    {
        // Logical frame selection also serves one-shot screenshots with the
        // diagnostics gate off. It needs no timing or renderer collection.
        if (s_matchActive) ++s_matchFrameCounter;
        if (s_playActive) ++s_playFrameCounter;
        return;
    }
    const unsigned long long end = port_monotonic_ns();
    const unsigned long long tasks = s_tTasks - s_t0;
    const unsigned long long present = end - s_tTasks;
    // Includes the acquire wait before PortBenchFrameBegin.
    const unsigned long long preSleep = s_preSleepThisFrame;
    s_preSleepThisFrame = 0;
    const unsigned long long frame = (end - s_t0) + s_acquireThisFrame + preSleep;
    // The frame limiter sleeps inside the tasks phase, so tasks time is not all work.
    unsigned long long busy = (tasks > s_sleepThisFrame) ? tasks - s_sleepThisFrame : 0;

    // Maintain the FPS window without sorting or renderer snapshots.
    s_lastBusyUs = (unsigned int)(busy / 1000ull);
    s_lastPresentUs = (unsigned int)(present / 1000ull);
    s_lastFrameUs = (unsigned int)(frame / 1000ull);
    s_lastSleepUs = (unsigned int)((s_sleepThisFrame + preSleep) / 1000ull);
    s_liveBusyUs[s_liveNext] = s_lastBusyUs;
    s_liveFrameSumUs -= s_liveFrameUs[s_liveNext];
    s_liveFrameUs[s_liveNext] = s_lastFrameUs;
    s_liveFrameSumUs += s_lastFrameUs;
    s_liveNext = (s_liveNext + 1) % LIVE_WINDOW;
    if (s_liveFilled < LIVE_WINDOW)
        s_liveFilled++;
    if (s_matchActive)
    {
        if (s_lastFrameUs > s_worstUs)
        {
            s_worstUs = s_lastFrameUs;
            s_worstFrame = s_matchFrameCounter;
        }
        ++s_matchFrameCounter;
    }
    if(s_playActive) ++s_playFrameCounter;
    const int captureEligible=(!s_captureMatchOnly||s_matchActive)&&(!s_capturePlayOnly||s_playActive);
    if(capture_active() && captureEligible && s_captureSkip) {
        --s_captureSkip;
    } else if(capture_active() && captureEligible) {
        FrameCapture* r=&s_capture[s_captureCount++];
        r->frameUs=s_lastFrameUs;r->tasksUs=s_lastBusyUs;r->presentUs=s_lastPresentUs;r->sleepUs=s_lastSleepUs;
        r->match=s_matchActive;
        r->matchFrame=s_capturePlayOnly?s_playFrameCounter-1:(s_matchActive?s_matchFrameCounter-1:0);
        if(s_captureCount==s_captureGoal)finish_capture();
    }
}
