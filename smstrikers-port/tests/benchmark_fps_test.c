#include "port/benchmark.h"
#include "port/config.h"
#include "port/host.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static int diagnostics, fpsOverlay;
static unsigned int clockCalls;
static unsigned long long clockNs = 1000000000ull;

int PortDiagnosticsEnabled(void) { return diagnostics; }
int PortFpsOverlayEnabled(void) { return fpsOverlay; }
unsigned long long port_monotonic_ns(void) { ++clockCalls; return clockNs; }

static void run_case(int mode)
{
    const pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0)
    {
        diagnostics = mode == 0;
        fpsOverlay = mode != 2;
        unsigned long long window[256] = {0}, sum = 0;
        CHECK(PortBenchGetFps() == 0.0);
        PortBenchInit();
        PortBenchRendererGameplayStart();
        PortBenchSetPlayActive(1);
        for (unsigned int i = 0; i < 800; ++i)
        {
            const unsigned long long workNs = (10000ull + i % 37 * 100ull) * 1000ull;
            const unsigned long long preSleepNs = 1000000ull, acquireNs = 2000000ull;
            PortBenchAddPreFrameSleep(preSleepNs);
            PortBenchFrameBegin();
            PortBenchAddAcquire(acquireNs);
            clockNs += workNs / 2;
            PortBenchAfterTasks();
            PortBenchAddSleep(500000ull);
            clockNs += workNs - workNs / 2;
            PortBenchFrameEnd();
            if (mode != 2)
            {
                // Independently calculate the rolling rate, including pacing and acquire.
                sum -= window[i % 256];
                window[i % 256] = (workNs + preSleepNs + acquireNs) / 1000ull;
                sum += window[i % 256];
                const unsigned int count = i < 256 ? i + 1 : 256;
                const double expected = count * 1e6 / (double)sum;
                CHECK(fabs(PortBenchGetFps() - expected) < 1e-9);
            }
            else
                CHECK(PortBenchGetFps() == 0.0);
        }
        CHECK(clockCalls == (mode == 0 ? 2400u : mode == 1 ? 1600u : 0u));
        const unsigned int before = clockCalls;
        PortBenchLive live;
        PortBenchGetLive(&live);
        CHECK(live.fps == PortBenchGetFps());
        CHECK(live.matchActive && live.playActive && live.frames==800 && live.playFrames==800);
        CHECK(clockCalls == before); // Reading FPS does not sample another clock.
        if (!diagnostics)
            CHECK(live.busyP95Ms == 0.0);
        exit(0);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void)
{
    for (int mode = 0; mode < 3; ++mode)
        run_case(mode);
    puts("FPS: rolling window wrap, pacing/acquire included, 2 clock reads with FPS only and 0 with everything OFF");
    return 0;
}
