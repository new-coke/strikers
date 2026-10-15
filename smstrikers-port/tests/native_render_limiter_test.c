#define _POSIX_C_SOURCE 200809L
#include "port/framerate.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned long long now = 1000000000ull, slept = 0;
unsigned long long port_monotonic_ns(void) { return now; }
void port_sleep_ns(unsigned long long ns) { now += ns; slept += ns; }
void port_sleep_until_ns(unsigned long long target) { if (target > now) port_sleep_ns(target-now); }
void port_yield(void) { ++now; }
void port_tighten_timer_slack(void) {}
int PortDiagnosticsEnabled(void) { return 0; }
int PortFpsOverlayEnabled(void) { return 0; }
int PortUnderGamescope(void) { return 0; }
void PortMorphWatchPoll(unsigned long frame) { (void)frame; }
void PortBenchAddSleep(unsigned long long ns) { (void)ns; }
void PortBenchAddPreFrameSleep(unsigned long long ns) { (void)ns; }

int main(void) {
    assert(setenv("STRIKERS_FPS_LIMIT","20",1)==0);
    PortSetDisplayRefresh(60,1);
    PortNativeRetrace(); PortLimiterFlush();
    const unsigned long long start=now;
    for(unsigned i=0;i<100;++i) {
        now += 20000000ull; // 20 ms work: wait only the remaining 30 ms.
        PortNativeRetrace(); PortLimiterFlush();
    }
    assert(now-start==5000000000ull);
    unsigned long long before=now, priorSleep=slept;
    now += 125000000ull; // Below the cap: presentation already used the budget.
    PortNativeRetrace(); PortLimiterFlush();
    assert(now-before==125000000ull && slept==priorSleep);
    before=now;
    now += 125000000ull;
    PortNativeRetrace(); PortLimiterFlush();
    assert(now-before==125000000ull && slept==priorSleep);
    puts("Native render limiter: 20 FPS pacing, elapsed-time budget and no added wait below target passed");
}
