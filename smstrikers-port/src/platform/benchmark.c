// Frame-time accounting for benchmark runs.

#include "port/benchmark.h"
#include "port/host.h"

// The frame budget the limiter is running to; see src/platform/vi.c.
unsigned long long PortFramePeriodNs(void);

// Aurora's GPU frame time. Weak so a build without Aurora still links; see the note in
// extern/aurora/lib/aurora.cpp on why this is not a timestamp query.
__attribute__((weak)) void aurora_gpu_frame_time(unsigned long long*, unsigned long long*,
                                                 unsigned long long*, unsigned long long*);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Per-frame samples, in microseconds. Kept rather than averaged: one missed frame in fifty averages
// well and stutters visibly. 262144 frames is about 73 minutes at 60Hz; past that it stops and says
// so.
#define BENCH_CAP 262144

static int s_enabled;
static int s_wantsDemo;
static int s_read;
static double s_intervalSec;      // 0 = summary at exit only
static double s_runSeconds;       // 0 = run until stopped

static unsigned long long s_t0;   // start of the current frame
static unsigned long long s_tTasks;
static unsigned long long s_sleepThisFrame;
static unsigned long long s_runStart;
static unsigned long long s_lastReport;

static unsigned int s_busyUs[BENCH_CAP];
static unsigned int s_presentUs[BENCH_CAP];
static unsigned int s_frameUs[BENCH_CAP];
static unsigned long long s_sleepUsTotal;
static size_t s_count;
static size_t s_dropped;
static size_t s_skipped;          // frames before the match started
static int s_matchActive;
static int s_skipKickoff;         // discard the frame the match starts on

// Always-on frame timing, for the overlay. Two clock reads a frame, so they are collected whether
// or not STRIKERS_BENCHMARK is set: watching a spike during ordinary play is the case that matters.
#define LIVE_WINDOW 256

static unsigned int s_liveBusyUs[LIVE_WINDOW];
static unsigned int s_liveFrameUs[LIVE_WINDOW];
static size_t s_liveNext;
static size_t s_liveFilled;
static unsigned int s_lastBusyUs, s_lastPresentUs, s_lastFrameUs, s_lastSleepUs;
static unsigned long s_frameCounter;
static unsigned int s_worstUs;
static unsigned long s_worstFrame;

// Labels. Fixed slots rather than a map: there are four of them, they are set once, and a benchmark
// is not the place for an allocator.
#define LABEL_MAX 8
#define LABEL_LEN 64
static char s_labelKey[LABEL_MAX][LABEL_LEN];
static char s_labelVal[LABEL_MAX][LABEL_LEN];
static size_t s_labelCount;

static void copy_label(char* dst, const char* src)
{
    size_t i = 0;
    if (src == NULL)
        src = "";
    for (; src[i] != '\0' && i < LABEL_LEN - 1; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

void PortBenchSetLabel(const char* key, const char* value)
{
    size_t i;
    if (key == NULL)
        return;
    for (i = 0; i < s_labelCount; i++)
    {
        if (strcmp(s_labelKey[i], key) == 0)
        {
            copy_label(s_labelVal[i], value);
            return;
        }
    }
    if (s_labelCount >= LABEL_MAX)
        return;                      // a lost label is not worth a failed run
    copy_label(s_labelKey[s_labelCount], key);
    copy_label(s_labelVal[s_labelCount], value);
    s_labelCount++;
}

void PortBenchInit(void)
{
    if (s_read)
        return;
    s_read = 1;

    const char* env = getenv("STRIKERS_BENCHMARK");
    if (env == NULL || *env == '\0' || strcmp(env, "0") == 0)
        return;

    s_enabled = 1;

    // "fe" measures the front end, which is the one case where driving the demo match would defeat
    // the point.
    s_wantsDemo = (strcmp(env, "fe") != 0);

    // A number greater than 1 is a reporting interval in seconds, so a long run says something
    // before it ends; a headless soak that only reported at exit would be silent for exactly as
    // long as it ran.
    double v = atof(env);
    s_intervalSec = (v > 1.0) ? v : 0.0;

    const char* secs = getenv("STRIKERS_BENCHMARK_SECONDS");
    s_runSeconds = (secs != NULL) ? atof(secs) : 0.0;

    s_runStart = port_monotonic_ns();
    s_lastReport = s_runStart;
}

// PORT_BUILD_TYPE comes from CMake. If it is ever missing, say so rather than guessing: "unknown"
// is a useful thing to read in a summary and a silent default is not.
#ifndef PORT_BUILD_TYPE
#define PORT_BUILD_TYPE "unknown"
#endif

int PortBenchEnabled(void) { return s_enabled; }
int PortBenchWantsDemo(void) { return s_enabled && s_wantsDemo; }
double PortBenchRunSeconds(void) { return s_runSeconds; }

void PortBenchMatchActive(void)
{
    if (!s_enabled || s_matchActive)
        return;
    // First frame of the match: throw away everything measured on the way in and restart the clock,
    // so the summary describes the match alone.
    s_matchActive = 1;
    // Discard the frame this fires on as well: cGame's update calls this partway through the first
    // frame of the match, and the rest of that frame is the transition finishing, ~140ms of `busy`
    // against a 16.6ms median.
    s_skipKickoff = 1;
    s_skipped = s_count + s_dropped;
    s_count = 0;
    s_dropped = 0;
    s_sleepUsTotal = 0;
    s_runStart = port_monotonic_ns();
    s_lastReport = s_runStart;
}

double PortBenchElapsed(void)
{
    if (!s_enabled)
        return 0.0;
    // Before the match, report 0 so STRIKERS_BENCHMARK_SECONDS cannot expire during the loading
    // screen and summarise a run that never played.
    if (!s_matchActive)
        return 0.0;
    return (double)(port_monotonic_ns() - s_runStart) / 1e9;
}

void PortBenchFrameBegin(void)
{
    s_t0 = port_monotonic_ns();
    s_sleepThisFrame = 0;
}

void PortBenchAfterTasks(void)
{
    s_tTasks = port_monotonic_ns();
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
    double sumFrame = 0.0;

    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));

    out->busyMs = (double)s_lastBusyUs / 1000.0;
    out->presentMs = (double)s_lastPresentUs / 1000.0;
    out->frameMs = (double)s_lastFrameUs / 1000.0;
    out->sleepMs = (double)s_lastSleepUs / 1000.0;
    out->worstMs = (double)s_worstUs / 1000.0;
    out->worstFrame = s_worstFrame;
    out->frames = (unsigned long)s_count;
    out->matchActive = s_matchActive;

    n = s_liveFilled;
    if (n == 0)
        return;

    for (i = 0; i < n; i++)
        sumFrame += (double)s_liveFrameUs[i];
    if (sumFrame > 0.0)
        out->fps = (double)n / (sumFrame / 1e6);

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
    const unsigned long long end = port_monotonic_ns();
    const unsigned long long tasks = s_tTasks - s_t0;
    const unsigned long long present = end - s_tTasks;
    const unsigned long long frame = end - s_t0;

    // The frame limiter sleeps inside the tasks phase, so tasks time is not all work.
    unsigned long long busy = (tasks > s_sleepThisFrame) ? tasks - s_sleepThisFrame : 0;

    // The overlay's view. Always collected, see the note by LIVE_WINDOW, and deliberately before
    // the s_enabled test below, because the overlay is most wanted during ordinary play rather than
    // during a benchmark.
    s_lastBusyUs = (unsigned int)(busy / 1000ull);
    s_lastPresentUs = (unsigned int)(present / 1000ull);
    s_lastFrameUs = (unsigned int)(frame / 1000ull);
    s_lastSleepUs = (unsigned int)(s_sleepThisFrame / 1000ull);
    s_frameCounter++;

    s_liveBusyUs[s_liveNext] = s_lastBusyUs;
    s_liveFrameUs[s_liveNext] = s_lastFrameUs;
    s_liveNext = (s_liveNext + 1) % LIVE_WINDOW;
    if (s_liveFilled < LIVE_WINDOW)
        s_liveFilled++;

    if (!s_enabled)
        return;

    // Count, but do not record, anything before the match: see PortBenchMatchActive.
    if (!s_matchActive)
    {
        s_count++;
        return;
    }

    if (s_skipKickoff)
    {
        s_skipKickoff = 0;
        s_skipped++;
        return;
    }

    // The worst frame of the run, and which frame it was.
    if (s_lastFrameUs > s_worstUs)
    {
        s_worstUs = s_lastFrameUs;
        s_worstFrame = (unsigned long)s_count;   // frame index within the match
    }

    if (s_count < BENCH_CAP)
    {
        s_busyUs[s_count] = (unsigned int)(busy / 1000ull);
        s_presentUs[s_count] = (unsigned int)(present / 1000ull);
        s_frameUs[s_count] = (unsigned int)(frame / 1000ull);
        s_count++;
    }
    else
    {
        s_dropped++;
    }
    s_sleepUsTotal += s_sleepThisFrame / 1000ull;

    if (s_intervalSec > 0.0)
    {
        double since = (double)(end - s_lastReport) / 1e9;
        if (since >= s_intervalSec)
        {
            PortBenchReport();
            s_lastReport = end;
        }
    }
}

static int cmp_u32(const void* a, const void* b)
{
    unsigned int x = *(const unsigned int*)a;
    unsigned int y = *(const unsigned int*)b;
    return (x > y) - (x < y);
}

// Percentiles need the samples sorted, and the samples are also the record of the run, so sort a
// copy.
static unsigned int* s_scratch;

static void stats(const unsigned int* src, size_t n,
                  double* mean, double* p50, double* p95, double* p99, double* max)
{
    *mean = *p50 = *p95 = *p99 = *max = 0.0;
    if (n == 0)
        return;
    if (s_scratch == NULL)
        s_scratch = (unsigned int*)malloc(sizeof(unsigned int) * BENCH_CAP);
    if (s_scratch == NULL)
        return;

    memcpy(s_scratch, src, n * sizeof(unsigned int));
    qsort(s_scratch, n, sizeof(unsigned int), cmp_u32);

    double sum = 0.0;
    for (size_t i = 0; i < n; i++)
        sum += (double)s_scratch[i];

    *mean = (sum / (double)n) / 1000.0;
    *p50 = (double)s_scratch[n / 2] / 1000.0;
    *p95 = (double)s_scratch[(size_t)((double)n * 0.95)] / 1000.0;
    *p99 = (double)s_scratch[(size_t)((double)n * 0.99)] / 1000.0;
    *max = (double)s_scratch[n - 1] / 1000.0;
}

static void write_csv(void);

void PortBenchReport(void)
{
    if (!s_enabled)
        return;

    // Saying so is the point: a run that never reached a match has measured the loading screen, and
    // reporting its numbers as if they were a match is how a benchmark starts reporting the loading
    // screen.
    if (!s_matchActive)
    {
        fprintf(stderr,
                "\n=== strikers benchmark ===\n"
                "no match was reached; %zu frames of front end discarded and "
                "nothing measured.\n"
                "The demo match starts about a minute in, give it longer, or "
                "set STRIKERS_BENCHMARK=fe\nto measure the front end "
                "deliberately.\n\n",
                s_count);
        return;
    }
    if (s_count == 0)
        return;

    const double wall = (double)(port_monotonic_ns() - s_runStart) / 1e9;
    double bMean, bP50, bP95, bP99, bMax;
    double pMean, pP50, pP95, pP99, pMax;
    double fMean, fP50, fP95, fP99, fMax;

    stats(s_busyUs, s_count, &bMean, &bP50, &bP95, &bP99, &bMax);
    stats(s_presentUs, s_count, &pMean, &pP50, &pP95, &pP99, &pMax);
    stats(s_frameUs, s_count, &fMean, &fP50, &fP95, &fP99, &fMax);

    const double sleepMean = ((double)s_sleepUsTotal / (double)s_count) / 1000.0;

    // What this run measured, printed before the numbers because it decides whether they compare
    // with anyone else's: the demo picks its stadium at random, and the build type matters.
    fprintf(stderr, "\n=== strikers benchmark ==============================================\n");
    fprintf(stderr, "  build %s", PORT_BUILD_TYPE);
    {
        size_t li;
        for (li = 0; li < s_labelCount; li++)
            fprintf(stderr, "   %s %s", s_labelKey[li], s_labelVal[li]);
    }
    fprintf(stderr, "\n");

    fprintf(stderr,
        "match frames %zu over %.1fs   %.2f fps presented\n"
        "(%zu earlier frames, boot, title, loading; discarded)\n"
        "                     mean     p50     p95     p99     max   (ms)\n"
        "  cpu busy        %7.3f %7.3f %7.3f %7.3f %7.3f\n"
        "  present/drain   %7.3f %7.3f %7.3f %7.3f %7.3f\n"
        "  frame total     %7.3f %7.3f %7.3f %7.3f %7.3f\n"
        "  limiter sleep   %7.3f (idle)\n",
        s_count, wall, (double)s_count / (wall > 0.0 ? wall : 1.0), s_skipped,
        bMean, bP50, bP95, bP99, bMax,
        pMean, pP50, pP95, pP99, pMax,
        fMean, fP50, fP95, fP99, fMax,
        sleepMean);

    // Headroom: how much slower a machine could be and still hold the frame rate it runs to.
    double field = 1000.0 / 59.94;
    int uncapped = 0;
    {
        const unsigned long long periodNs = PortFramePeriodNs();
        if (periodNs > 0)
            field = (double)periodNs / 1e6;
        else
            uncapped = 1;
    }
    if (bP95 > 0.0)
        fprintf(stderr,
            "  headroom        %.1fx at p95  (%.3fms of a %.3fms %s)\n",
            field / bP95, bP95, field,
            uncapped ? "console field, run is uncapped" : "frame budget");

    // GPU time, from Aurora. Weakly declared: a non-Aurora build has no implementation and should
    // print nothing rather than fail to link.
    {
        unsigned long long lastNs = 0, meanNs = 0, maxNs = 0, count = 0;
        if (aurora_gpu_frame_time != NULL)
        {
            aurora_gpu_frame_time(&lastNs, &meanNs, &maxNs, &count);
            if (count > 0)
                fprintf(stderr,
                        "  queue latency   %7.3f mean %7.3f max  over %llu submissions\n"
                        "                  (NOT GPU execution time on Metal, it does not\n"
                        "                   respond to GPU load)\n",
                        (double)meanNs / 1e6, (double)maxNs / 1e6, count);
        }
    }

    // Where the worst frame was, as well as how bad.
    fprintf(stderr, "  worst frame     %7.3f at frame %lu\n",
            (double)s_worstUs / 1000.0, s_worstFrame);

    if (s_dropped)
        fprintf(stderr, "  (%zu frames past the %d-frame sample cap not counted)\n",
                s_dropped, BENCH_CAP);
    fprintf(stderr,
        "=====================================================================\n\n");

    write_csv();
}

// Per-frame record, for a baseline or a spike hunt.
static void write_csv(void)
{
    const char* path;
    FILE* f;
    size_t i;

    path = getenv("STRIKERS_BENCH_RECORD");
    if (path == NULL || *path == '\0')
        return;

    f = fopen(path, "w");
    if (f == NULL)
    {
        fprintf(stderr, "[bench] could not write %s\n", path);
        return;
    }

    fprintf(f, "# build=%s", PORT_BUILD_TYPE);
    for (i = 0; i < s_labelCount; i++)
        fprintf(f, " %s=%s", s_labelKey[i], s_labelVal[i]);
    fprintf(f, "\nframe,busy_us,present_us,frame_us\n");
    for (i = 0; i < s_count; i++)
        fprintf(f, "%zu,%u,%u,%u\n", i, s_busyUs[i], s_presentUs[i], s_frameUs[i]);
    fclose(f);
    fprintf(stderr, "[bench] wrote %zu frames to %s\n", s_count, path);
}
