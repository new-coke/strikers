// Host stand-in for the Video Interface. VI is the GameCube's scanout engine: it owns the
// framebuffer address, the TV format, and the retrace interrupt that paces the whole game.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dolphin/types.h"
#include "port/host.h"
#include "port/benchmark.h"
#include "port/config.h"
#include "port/determinism.h"
#include "port/framerate.h"
#include "port/morphwatch.h"
#include "port/steamdeck.h"

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
            // Under gamescope the acquire rarely blocks, so this limiter paces and runs at the panel rate.
            const int underGamescope = PortUnderGamescope();
            double hz = s_displayHz;
            if (underGamescope)
            {
                // Xwayland reports a loose rate, 59.81 for a 59.999 Hz panel, and a limiter under it repeats frames.
                const double rounded = (double)(long)(hz + 0.5);
                if (hz - rounded < 0.5 && rounded - hz < 0.5)
                    hz = rounded;
            }
            else if (s_displayVsync)
            {
                hz *= PORT_VSYNC_HEADROOM;
            }
            s_period = (u64)(1000000000.0 / hz);
            why = !s_displayVsync ? "display"
                  : underGamescope ? "display, exact: gamescope paces by this limiter"
                                   : "display, +5% for vsync";
        }
        else
        {
            s_period = PORT_FALLBACK_FRAME_NS;
            why = "fallback, display unknown";
        }

        // Once per derivation and never per frame; this is how a headless run reads back what a
        // switch did.
        if (PortDiagnosticsEnabled())
        {
            if (s_period != 0)
                fprintf(stderr, "[limiter] %.2f Hz (%s)\n",
                        1000000000.0 / (double)s_period, why);
            else
                fprintf(stderr, "[limiter] uncapped (%s)\n", why);
        }
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

unsigned long long PortVirtualTickerWide(void)
{
    // OSGetTick counts bus clocks at 40.5MHz; nlGetTickerDifference converts back through
    // __OSBusClock, so the units have to match OSGetTick's or every delta in the game comes out
    // scaled.
    const double ticksPerFrame = fixed_dt_ms() * 40500.0;   // 40.5e6 / 1000
    return (u64)((double)s_retrace_count * ticksPerFrame);
}

unsigned int PortVirtualTicker(void)
{
    return (unsigned int)PortVirtualTickerWide();
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
#if defined(__linux__)
    // Tight timer slack makes one absolute sleep to just short of the deadline accurate on Linux.
    static int s_slackSet;
    if (!s_slackSet)
    {
        s_slackSet = 1;
        port_tighten_timer_slack();
    }
    {
        const u64 t = now_ns();
        if (deadline > t + WAIT_SPIN_FLOOR_NS)
            port_sleep_until_ns(deadline - WAIT_SPIN_FLOOR_NS);
    }
#else
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
#endif

    while (now_ns() < deadline)
        port_yield();
}

// Unless present waits for the vblank, the limiter sleeps at the top of the next frame so input is read after it.
// A vblank wait would add it back in present and make step lengths alternate. STRIKERS_LIMITER_DEFER=1 or 0 overrides.
static u64 s_deferredDeadline;

static int limiter_defers(void)
{
    static int s_forced = -2;
    if (s_forced == -2)
    {
        const char* e = getenv("STRIKERS_LIMITER_DEFER");
        s_forced = (e == NULL || *e == '\0') ? -1 : (atoi(e) != 0);
    }
    if (s_forced >= 0)
        return s_forced;
    return !s_displayVsync;
}

void PortLimiterFlush(void)
{
    const u64 deadline = s_deferredDeadline;
    if (deadline == 0)
        return;
    s_deferredDeadline = 0;
    {
        const u64 before = now_ns();
        if (deadline > before)
            wait_until(deadline);
        if (PortDiagnosticsEnabled() || PortFpsOverlayEnabled())
            PortBenchAddPreFrameSleep(now_ns() - before);
    }
}

void VIWaitForRetrace(void)
{
    // Wait for the next field boundary, then run the callbacks the console would have run from the
    // retrace interrupt.
    const u64 period = frame_period_ns();

    if (period != 0)
    {
        static u64 next_deadline;
        if (s_deferredDeadline != 0)
        {
            // A second wait in one frame comes from a loop that expects to block.
            const u64 pending = s_deferredDeadline;
            const u64 before = now_ns();
            s_deferredDeadline = 0;
            if (pending > before)
                wait_until(pending);
            if (PortDiagnosticsEnabled())
                PortBenchAddSleep(now_ns() - before);
        }
        u64 t = now_ns();
        if (next_deadline == 0)
        {
            next_deadline = t;                   // first call: no wait
        }
        else if (t > next_deadline + period)
        {
            // Behind by a period, as when vsync ends frames slower than the cap: land on `t` after the +=.
            next_deadline = t - period;
        }
        next_deadline += period;

        if (next_deadline > t)
        {
            if (limiter_defers())
            {
                s_deferredDeadline = next_deadline;
            }
            else
            {
                // Report what the wait *actually* cost, not what was asked for.
                u64 before = now_ns();
                wait_until(next_deadline);
                if (PortDiagnosticsEnabled())
                    PortBenchAddSleep(now_ns() - before);
            }
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

// The clock tasks step by while vsync paces the frame: a frame stays on screen for whole display periods.
#define PACE_LAG 0.5            // periods the clock sits behind the host clock, so it is never ahead
#define PACE_GAIN 0.02          // share of the phase error corrected each frame, which keeps corrections under 10 microseconds
#define PACE_OFF_GRID 0.25      // a frame this many periods from the clock's grid is off it
#define PACE_SHARE_ALPHA 0.005  // the off-grid share is averaged over a few hundred frames
#define PACE_SHARE_ON 0.2       // trusted below this share: vblank-locked frames leave the grid only on a hitch, and VRR frames about half the time
#define PACE_SHARE_OFF 0.35

static int s_snapOverride = -1;
static double s_paceHz;         // the rate the clock was started at; 0 when stopped
static double s_paceNs;
static u64 s_paceLastNow;
static double s_paceShare;
static int s_paceTrusted;
static int s_paceActive;
static unsigned int s_paceTicks;

static int snap_wanted(void)
{
    static int s_env = -1;
    if (s_snapOverride >= 0)
        return s_snapOverride;
    if (s_env < 0)
    {
        const char* e = getenv("STRIKERS_DT_SNAP");
        s_env = (e == NULL || *e == '\0') ? 1 : (atoi(e) != 0);
    }
    return s_env;
}

void PortSetTaskClockSnap(int on)
{
    s_snapOverride = (on < 0) ? -1 : (on != 0);
    s_paceHz = 0.0;
}

// OSGetTick's units: 40.5 MHz.
static unsigned int ticks_from_ns(u64 ns)
{
    return (unsigned int)(ns / 1000000000ull * 40500000ull + ns % 1000000000ull * 40500000ull / 1000000000ull);
}

static void pace_report(int trusted, const char* why)
{
    if (!PortDiagnosticsEnabled())
        return;
    if (trusted)
        fprintf(stderr, "[limiter] task steps: whole display periods (%.2f Hz)\n", s_paceHz);
    else
        fprintf(stderr, "[limiter] task steps: host clock (%s)\n", why);
}

int PortTaskClockFrame(unsigned int* ticks)
{
    const u64 now = now_ns();
    const int wanted = !PortFixedTimestep() && snap_wanted() && s_displayHz > 0.0 && s_displayVsync;
    const u64 sinceLast = now - s_paceLastNow;
    s_paceLastNow = now;

    if (s_paceActive && (!wanted || s_paceHz != s_displayHz))
    {
        // Carried forward by host time for the frame that leaves it, so no task's step goes backwards or jumps.
        s_paceNs += (double)sinceLast;
        s_paceTicks = ticks_from_ns((u64)s_paceNs);
        s_paceActive = 0;
        s_paceHz = 0.0;
        pace_report(0, !snap_wanted() ? "STRIKERS_DT_SNAP=0"
                       : wanted       ? "the display changed"
                                      : "present does not wait for the vblank");
        if (ticks != NULL)
            *ticks = s_paceTicks;
        return PORT_TASK_CLOCK_LEAVING;
    }
    if (!wanted)
    {
        s_paceHz = 0.0;
        return PORT_TASK_CLOCK_HOST;
    }

    const double period = 1000000000.0 / s_displayHz;
    if (s_paceHz != s_displayHz)
    {
        s_paceHz = s_displayHz;
        s_paceNs = (double)now - PACE_LAG * period;
        // Untrusted until a second or two of frames lands on the grid, so VRR never starts snapped.
        s_paceShare = 0.5;
        s_paceTrusted = 0;
    }
    else
    {
        const double target = (double)now - PACE_LAG * period;
        double whole = floor((target - s_paceNs) / period + 0.5);
        if (whole < 1.0)
            whole = 1.0;
        double next = s_paceNs + whole * period;
        const double err = target - next;
        next += err * PACE_GAIN;
        if (next > (double)now)
            next = (double)now;

        s_paceShare += PACE_SHARE_ALPHA * ((fabs(err) > PACE_OFF_GRID * period ? 1.0 : 0.0) - s_paceShare);
        s_paceNs = next;

        s_paceTrusted = s_paceTrusted ? (s_paceShare <= PACE_SHARE_OFF) : (s_paceShare <= PACE_SHARE_ON);
    }

    s_paceTicks = ticks_from_ns((u64)s_paceNs);
    if (ticks != NULL)
        *ticks = s_paceTicks;
    if (s_paceTrusted == s_paceActive)
        return s_paceActive ? PORT_TASK_CLOCK_DISPLAY : PORT_TASK_CLOCK_HOST;
    s_paceActive = s_paceTrusted;
    pace_report(s_paceActive, "frame intervals are not whole display periods");
    return s_paceActive ? PORT_TASK_CLOCK_ENTERING : PORT_TASK_CLOCK_LEAVING;
}

int PortTaskClockCurrent(unsigned int* ticks)
{
    if (s_paceActive && ticks != NULL)
        *ticks = s_paceTicks;
    return s_paceActive;
}

// The frame budget the port runs to. benchmark.c divides by this rather than the console field
// period: at a 120Hz default, measuring headroom against 16.683ms overstates it twofold. 0 when
// uncapped.
unsigned long long PortFramePeriodNs(void) { return frame_period_ns(); }

// The native display queue owns the physical wait. Advance the game/test
// retrace exactly once for each submitted frame without a second vblank wait.
void PortNativeRetrace(void)
{
    // VitaGL already paces physical presentation. Honour an explicit slower
    // render cap by waiting before the next input/task pass, without dropping
    // frames or changing the simulation clock.
    static u64 nextDeadline;
    static int explicitLimit = -1;
    if (explicitLimit < 0)
    {
        const char* value = getenv("STRIKERS_FPS_LIMIT");
        explicitLimit = value != NULL && *value != '\0';
    }
    if (explicitLimit || s_limitOverride >= 0.0)
    {
        const u64 period = frame_period_ns();
        const u64 now = now_ns();
        if (period != 0)
        {
            if (nextDeadline == 0) nextDeadline = now;
            else if (now > nextDeadline + period) nextDeadline = now - period;
            nextDeadline += period;
            if (nextDeadline > now) s_deferredDeadline = nextDeadline;
        }
        else nextDeadline = 0;
    }
    ++s_retrace_count;
    if(s_pre_cb)s_pre_cb(s_retrace_count);
    if(s_post_cb)s_post_cb(s_retrace_count);
}

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
