// What the frame limiter caps to: vi.c sleeps to an absolute deadline in place of the console's 60
// Hz field rate, and the period comes from the display main() found the window on.

// With vsync on the limiter is held slightly above the refresh rate, because two pacers in series
// at the same rate but a different phase drift, and each time the deadline lands just after a
// vblank the frame waits nearly a whole extra period.

// STRIKERS_FPS_LIMIT overrides, in Hz or 0 for unlimited, and is taken exactly.

#ifndef PORT_FRAMERATE_H
#define PORT_FRAMERATE_H

#ifdef __cplusplus
extern "C" {
#endif

// Tell the limiter what the display does: `hz` is the refresh rate, 0 when unknown (outside
// 20..2000 Hz counts as unknown), `vsync` non-zero when present paces too. Each call re-derives the
// period.
void PortSetDisplayRefresh(double hz, int vsync);

// A rate in Hz, 0 for uncapped, or negative to follow STRIKERS_FPS_LIMIT again.
void PortSetFrameLimit(double hz);

void PortFrameLimitInfo(double* limitHz, double* displayHz, int* vsync, int* overridden);

// Sleeps to the deadline VIWaitForRetrace deferred. Call it at the top of the frame ahead of the event pump.
void PortLimiterFlush(void);

// Which clock a task steps by this frame and which it records for the next; the two differ only on the frame the clock changes.
enum
{
    PORT_TASK_CLOCK_HOST = 0,       // step by the host clock, record the host clock
    PORT_TASK_CLOCK_DISPLAY = 1,    // step by the display clock, record the display clock
    PORT_TASK_CLOCK_ENTERING = 2,   // step by the host clock, record the display clock
    PORT_TASK_CLOCK_LEAVING = 3,    // step by the display clock, record the host clock
};

// Once per RunAllTasks: the display clock in OSGetTick units, stepping whole display periods while present waits for the vblank.
void PortNativeRetrace(void);
int PortTaskClockFrame(unsigned int* ticks);

// The clock a task added mid-frame records: returns 1 with the display clock in ticks, or 0 for the host clock.
int PortTaskClockCurrent(unsigned int* ticks);

// 1 on, 0 off, negative to follow STRIKERS_DT_SNAP again (default on).
void PortSetTaskClockSnap(int on);

#ifdef __cplusplus
}
#endif

#endif // PORT_FRAMERATE_H
