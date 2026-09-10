// Host stand-in for the Video Interface. VI is the GameCube's scanout engine: it owns the
// framebuffer address, the TV format, and the retrace interrupt that paces the whole game.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dolphin/types.h"
#include "port/host.h"
#include "port/benchmark.h"
#include "port/determinism.h"
#include "port/framerate.h"
#include "port/morphwatch.h"

typedef void (*VIRetraceCallback)(u32 retraceCount);

// NTSC field rate. The game is hard-locked to this: 23 VIWaitForRetrace calls across the tree and
// no frame-rate decoupling anywhere.
#define VI_FIELD_NS 16683333ull

// The fallback frame period: 120 Hz. Used only when the display's refresh rate is unavailable,
// headless, STRIKERS_AURORA=OFF, or a mode that reports no rate.
#define PORT_FALLBACK_FRAME_NS 8333333ull

// How far above the display's refresh rate the limiter sits while vsync is also pacing, so the two
// never fight over which one ends the frame.
#define PORT_VSYNC_HEADROOM 1.05

static VIRetraceCallback s_pre_cb;
static VIRetraceCallback s_post_cb;
static u32 s_retrace_count;
static void* s_next_fb;
static BOOL s_black;

static u64 now_ns(void) { return port_monotonic_ns(); }

#if !defined(PORT_USE_AURORA)
void VIInit(void)
{
    s_retrace_count = 0;
}
#endif

// The frame rate cap, in Hz. 0 means uncapped.
static double s_displayHz;
static int s_displayVsync;

// The derived period, latched at file scope so PortSetDisplayRefresh can drop it: the limiter can
// be asked for a period before the display is known, and a latch that cannot clear pins the
// fallback.
static u64 s_period;
static int s_periodValid;

// A rate the debug menu asked for, in Hz; 0 for uncapped, negative for "no override, follow the env
// and the display".
static double s_limitOverride = -1.0;

static u64 frame_period_ns(void)
{
    if (!s_periodValid)
    {
        // getenv once, not once per frame: it takes a lock and walks environ, and this is on the
        // frame path.
        static const char* s_env;
        static int s_envRead;
        if (!s_envRead)
        {
            s_envRead = 1;
            s_env = getenv("STRIKERS_FPS_LIMIT");
        }

        const char* why;
        s_periodValid = 1;
        if (s_limitOverride >= 0.0)
        {
            s_period = (s_limitOverride > 0.0) ? (u64)(1000000000.0 / s_limitOverride) : 0ull;
            why = "debug command";
        }
        else if (s_env != NULL && *s_env != '\0')
        {
            // An explicit request is taken exactly, no display, no margin.
            const double hz = atof(s_env);
            s_period = (hz > 0.0) ? (u64)(1000000000.0 / hz) : 0ull;
            why = "STRIKERS_FPS_LIMIT";
        }
        else if (s_displayHz > 0.0)
        {
            const double hz = s_displayVsync
                                  ? s_displayHz * PORT_VSYNC_HEADROOM
                                  : s_displayHz;
            s_period = (u64)(1000000000.0 / hz);
            why = s_displayVsync ? "display, +5% for vsync" : "display";
        }
        else
        {
            s_period = PORT_FALLBACK_FRAME_NS;
            why = "fallback, display unknown";
        }

        // Once per derivation and never per frame; this is how a headless run reads back what a
        // switch did.
        if (s_period != 0)
            fprintf(stderr, "[limiter] %.2f Hz (%s)\n",
                    1000000000.0 / (double)s_period, why);
        else
            fprintf(stderr, "[limiter] uncapped (%s)\n", why);
    }
    return s_period;
}

void PortSetFrameLimit(double hz)
{
    s_limitOverride = hz;
    s_periodValid = 0;
}

void PortFrameLimitInfo(double* limitHz, double* displayHz, int* vsync, int* overridden)
{
    const u64 period = frame_period_ns();
    if (limitHz != NULL)
        *limitHz = (period != 0) ? 1000000000.0 / (double)period : 0.0;
    if (displayHz != NULL)
        *displayHz = s_displayHz;
    if (vsync != NULL)
        *vsync = s_displayVsync;
    if (overridden != NULL)
        *overridden = (s_limitOverride >= 0.0);
}

void PortSetDisplayRefresh(double hz, int vsync)
{
    // Range-checked rather than trusted: an absurd mode would otherwise become the frame limiter.
    // The ceiling is 2000 because SDL reports a float, and a panel at exactly 1000 must not fall
    // back to 120 Hz.
    s_displayHz = (hz >= 20.0 && hz <= 2000.0) ? hz : 0.0;
    s_displayVsync = (vsync != 0);
    s_periodValid = 0;
}

// The virtual clock. See include/port/determinism.h for why it exists.
static double fixed_dt_ms(void)
{
    static double s_ms;
    static int s_read;

    if (!s_read)
    {
        const char* env = getenv("STRIKERS_FIXED_DT");
        s_read = 1;
        s_ms = 0.0;
        if (env != NULL && *env != '\0')
        {
            const double v = atof(env);
            if (v == 1.0)
            {
                // Follow the limiter, but only where the limiter is a number someone chose, never
                // where it came from the display.
                const char* lim = getenv("STRIKERS_FPS_LIMIT");
                u64 period = PORT_FALLBACK_FRAME_NS;
                if (lim != NULL && *lim != '\0')
                {
                    const double hz = atof(lim);
                    period = (hz > 0.0) ? (u64)(1000000000.0 / hz) : 0ull;
                }
                s_ms = (period != 0) ? (double)period / 1e6 : (double)VI_FIELD_NS / 1e6;
            }
            else if (v > 0.0)
            {
                s_ms = v;
            }
        }
    }
    return s_ms;
}

int PortFixedTimestep(void) { return fixed_dt_ms() > 0.0; }

unsigned int PortVirtualTicker(void)
{
    // OSGetTick counts bus clocks at 40.5MHz; nlGetTickerDifference converts back through
    // __OSBusClock, so the units have to match OSGetTick's or every delta in the game comes out
    // scaled.
    const double ticksPerFrame = fixed_dt_ms() * 40500.0;   // 40.5e6 / 1000
    return (unsigned int)(u64)((double)s_retrace_count * ticksPerFrame);
}

int PortFixedSeed(unsigned int* out)
{
    static unsigned int s_seed;
    static int s_have;
    static int s_read;

    if (!s_read)
    {
        const char* env = getenv("STRIKERS_SEED");
        s_read = 1;
        if (env != NULL && *env != '\0')
        {
            s_seed = (unsigned int)strtoul(env, NULL, 0);
            s_have = 1;
        }
    }
    if (s_have && out != NULL)
        *out = s_seed;
    return s_have;
}

// Waiting for a deadline: every host sleeps late by about a fifth of the request, so sleep four
// fifths of what is left and look again, then poll the last hundred microseconds with a yield.
// Stateless on purpose.
#define WAIT_SPIN_FLOOR_NS 100000ull   // under this, sleep and wake cost more than they save
#define WAIT_REQUEST_MIN_NS 20000ull   // a request shorter than this is all leeway

static void wait_until(u64 deadline)
{
    for (;;)
    {
        const u64 t = now_ns();
        if (t >= deadline)
            return;
        const u64 remaining = deadline - t;
        if (remaining <= WAIT_SPIN_FLOOR_NS)
            break;
        const u64 req = remaining - remaining / 5 - 30000ull;
        if (req < WAIT_REQUEST_MIN_NS)
            break;
        port_sleep_ns(req);
    }

    while (now_ns() < deadline)
        port_yield();
}

void VIWaitForRetrace(void)
{
    // Wait for the next field boundary, then run the callbacks the console would have run from the
    // retrace interrupt.
    const u64 period = frame_period_ns();

    if (period != 0)
    {
        static u64 next_deadline;
        u64 t = now_ns();
        if (next_deadline == 0 || t > next_deadline + period * 4)
            next_deadline = t;                   // first call, or badly behind
        next_deadline += period;

        if (next_deadline > t)
        {
            // Report what the wait *actually* cost, not what was asked for.
            u64 before = now_ns();
            wait_until(next_deadline);
            PortBenchAddSleep(now_ns() - before);
        }
    }

    // The callbacks run either way. They are what the console's retrace interrupt drove, controller
    // sampling among them, and skipping them when uncapped would stop input rather than speed
    // anything up.
    s_retrace_count++;

    // Once-per-frame scan for the cSAnim morph-field overwrite; a no-op unless STRIKERS_WATCH_MORPH
    // is set.
    PortMorphWatchPoll((unsigned long)s_retrace_count);

    if (s_pre_cb)
        s_pre_cb(s_retrace_count);
    if (s_post_cb)
        s_post_cb(s_retrace_count);
}

// The frame budget the port runs to. benchmark.c divides by this rather than the console field
// period: at a 120Hz default, measuring headroom against 16.683ms overstates it twofold. 0 when
// uncapped.
unsigned long long PortFramePeriodNs(void) { return frame_period_ns(); }

u32 VIGetRetraceCount(void) { return s_retrace_count; }

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback prev = s_pre_cb;
    s_pre_cb = cb;
    return prev;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback prev = s_post_cb;
    s_post_cb = cb;
    return prev;
}

#if !defined(PORT_USE_AURORA)
void VIConfigure(const void* rm) { (void)rm; }
#endif
#if !defined(PORT_USE_AURORA)
void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height)
{
    (void)xOrg; (void)yOrg; (void)width; (void)height;
}
#endif
#if !defined(PORT_USE_AURORA)
void VIFlush(void) {}
#endif
void VISetNextFrameBuffer(void* fb) { s_next_fb = fb; }
void VISetBlack(BOOL black) { s_black = black; }

#if !defined(PORT_USE_AURORA)
u32 VIGetTvFormat(void) { return 0; }   // VI_NTSC
#endif
u32 VIGetDTVStatus(void) { return 0; }  // no component cable
