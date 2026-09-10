
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

int PortBenchEnabled(void);

// Drive the AI-vs-AI demo rather than sit on the title; STRIKERS_BENCHMARK=fe measures the front
// end without it.
int PortBenchWantsDemo(void);

double PortBenchRunSeconds(void);

double PortBenchElapsed(void);

// From cGame's update while a match is live. Frames before the first call are counted but not
// recorded, keeping loading spikes out of the tail, and the run clock restarts here.
void PortBenchMatchActive(void);

void PortBenchFrameBegin(void);
void PortBenchAfterTasks(void);
void PortBenchFrameEnd(void);

void PortBenchAddSleep(unsigned long long ns);

void PortBenchReport(void);

// The demo path picks its stadium at random, so two runs may measure different content; the build
// type is labelled for the same reason.

void PortBenchSetLabel(const char* key, const char* value);

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
} PortBenchLive;

void PortBenchGetLive(PortBenchLive* out);

size_t PortBenchGetHistory(float* busyMs, float* frameMs, size_t cap);

#ifdef __cplusplus
}
#endif

#endif // _PORT_BENCHMARK_H_
