// Host implementations of the Dolphin OS calls the game makes.

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dolphin/types.h"
#include "port/host.h"

// A synthetic range: the game compares against the arena bounds and never dereferences them.

#define PORT_ARENA_SIZE (192u * 1024u * 1024u)

#if defined(PORT_USE_AURORA)
// Aurora's SDK headers reach the console's low-memory globals as offsets from OSBaseAddress.
#define PORT_MEM1_SIZE (24u * 1024u * 1024u)   // retail MEM1

uintptr_t OSBaseAddress = 0;

__attribute__((constructor)) static void port_init_mem1(void)
{
    void* mem1 = calloc(1, PORT_MEM1_SIZE);
    if (mem1 == NULL)
        return;
    OSBaseAddress = (uintptr_t)mem1;

    // Retail GameCube boot-info values.
    *(u32*)(OSBaseAddress + 0x0028) = PORT_MEM1_SIZE;   // __OSPhysicalMemSize
    *(u32*)(OSBaseAddress + 0x00F0) = PORT_MEM1_SIZE;   // __OSSimulatedMemSize
    *(u32*)(OSBaseAddress + 0x00F8) = 162000000u;       // __OSBusClock
    *(u32*)(OSBaseAddress + 0x00FC) = 486000000u;       // __OSCoreClock
}
#endif // PORT_USE_AURORA

void OSReport(const char* msg, ...);   // defined below

static void port_note_exit(void)
{
    fprintf(stderr, "[port] process exiting normally (not a crash)\n");
}

static u8* s_arena_lo;
static u8* s_arena_hi;
int __OSCurrHeap = -1;

static void ensure_arena(void)
{
    if (s_arena_lo == NULL)
    {
        s_arena_lo = (u8*)malloc(PORT_ARENA_SIZE);
        s_arena_hi = s_arena_lo ? s_arena_lo + PORT_ARENA_SIZE : NULL;
    }
}

void* OSGetArenaLo(void) { ensure_arena(); return s_arena_lo; }
void* OSGetArenaHi(void) { ensure_arena(); return s_arena_hi; }
void OSSetArenaLo(void* newLo) { s_arena_lo = (u8*)newLo; }

void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps)
{
    (void)arenaEnd;
    (void)maxHeaps;
    return arenaStart;
}

int OSCreateHeap(void* start, void* end)
{
    (void)start;
    (void)end;
    // OSAllocFromHeap ignores the handle, so heaps only need distinct non-negative ids.
    static int next = 0;
    return next++;
}

int OSSetCurrentHeap(int heap)
{
    int prev = __OSCurrHeap;
    __OSCurrHeap = heap;
    return prev;
}

void* OSAllocFromHeap(int heap, u32 size)
{
    (void)heap;
    // 32-byte alignment, as the console allocator gave callers wanting DMA-ready buffers.
    return port_aligned_alloc(32, size ? size : 1);
}

void OSFreeToHeap(int heap, void* ptr)
{
    (void)heap;
    // Must pair with port_aligned_alloc: on Windows free() would get an interior pointer.
    port_aligned_free(ptr);
}

// DCZeroRange is the one cache call with an observable side effect callers depend on.
void DCFlushRange(void* addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCFlushRangeNoSync(void* addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCStoreRange(void* addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCStoreRangeNoSync(void* addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCInvalidateRange(void* addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCZeroRange(void* addr, u32 nBytes) { memset(addr, 0, nBytes); }
void LCEnable(void) {}
void LCDisable(void) {}
void PPCSync(void) { __sync_synchronize(); }

// The return is the previous enable state, which callers pass back to OSRestoreInterrupts.
static BOOL s_interrupts_enabled = TRUE;

BOOL OSDisableInterrupts(void)
{
    BOOL prev = s_interrupts_enabled;
    s_interrupts_enabled = FALSE;
    return prev;
}

BOOL OSEnableInterrupts(void)
{
    BOOL prev = s_interrupts_enabled;
    s_interrupts_enabled = TRUE;
    return prev;
}

BOOL OSRestoreInterrupts(BOOL level)
{
    BOOL prev = s_interrupts_enabled;
    s_interrupts_enabled = level;
    return prev;
}

void OSYieldThread(void) {}

// The console's timer rate, kept so the game's OSTicksTo* conversions stay correct.
#define OS_TIMER_CLOCK 40500000ull

static u64 host_ticks(void)
{
    u64 ns = port_monotonic_ns();
    return ns / 1000000000ull * OS_TIMER_CLOCK
         + ns % 1000000000ull * OS_TIMER_CLOCK / 1000000000ull;
}

u64 OSGetTime(void) { return host_ticks(); }
u32 OSGetTick(void) { return (u32)host_ticks(); }

typedef struct OSCalendarTime OSCalendarTime;

void OSTicksToCalendarTime(u64 ticks, OSCalendarTime* td)
{
    // Consecutive ints: sec, min, hour, mday, mon, year, wday, yday, msec, usec.
    int* f = (int*)td;
    time_t now = time(NULL);
    struct tm tmv;
    memset(&tmv, 0, sizeof tmv);
    port_localtime(now, &tmv);
    f[0] = tmv.tm_sec;
    f[1] = tmv.tm_min;
    f[2] = tmv.tm_hour;
    f[3] = tmv.tm_mday;
    f[4] = tmv.tm_mon;
    f[5] = tmv.tm_year + 1900;
    f[6] = tmv.tm_wday;
    f[7] = tmv.tm_yday;
    f[8] = (int)((ticks % OS_TIMER_CLOCK) * 1000 / OS_TIMER_CLOCK);
    f[9] = 0;
}

u32 OSGetConsoleType(void) { return 0x00000001u; } // retail production unit

// STRIKERS_LANGUAGE, the IPL setting a European GameCube kept in SRAM.
u8 OSGetLanguage(void)
{
    static const struct { const char* name; u8 value; } kLanguages[] = {
        {"english", 0}, {"eng", 0}, {"uk", 0}, {"en", 0},
        {"german", 1},  {"deu", 1}, {"de", 1}, {"ger", 1},
        {"french", 2},  {"fra", 2}, {"fr", 2},
        {"spanish", 3}, {"esp", 3}, {"es", 3}, {"spa", 3},
        {"italian", 4}, {"ita", 4}, {"it", 4},
    };
    const char* v = getenv("STRIKERS_LANGUAGE");
    size_t i;

    if (v == NULL || *v == '\0')
        return 0;   // UK English, which is the console's own default

    if (*v >= '0' && *v <= '9')
    {
        unsigned long n = strtoul(v, NULL, 10);
        return n <= 4 ? (u8)n : (u8)0;
    }

    for (i = 0; i < sizeof kLanguages / sizeof kLanguages[0]; i++)
        if (strcmpi(v, kLanguages[i].name) == 0)
            return kLanguages[i].value;

    // Named something this does not know. Not fatal, a typo in a config file should not stop the
    // game, but silent would be worse, because the symptom is "the language setting does nothing".
    fprintf(stderr,
            "[port] STRIKERS_LANGUAGE=%s not recognised; using UK English. "
            "Try english, german, french, spanish, italian, or 0-4.\n", v);
    return 0;
}
u32 OSGetSoundMode(void) { return 1; }             // stereo
void OSSetSoundMode(u32 mode) { (void)mode; }
u32 OSGetProgressiveMode(void) { return 0; }
void OSSetProgressiveMode(u32 on) { (void)on; }
u32 OSGetEuRgb60Mode(void) { return 0; }
void OSSetEuRgb60Mode(u32 on) { (void)on; }
u32 OSGetResetCode(void) { return 0; }
BOOL OSGetResetButtonState(void) { return FALSE; }

void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu)
{
    (void)forceMenu;
    // On console this reboots into the IPL; here the window would just disappear.
    OSReport("OSResetSystem(reset=%d, code=%u): exiting\n", reset,
             (unsigned)resetCode);
    fflush(NULL);
    exit(0);
}

// So a run that leaves through exit() rather than a signal still says it was not a crash.
__attribute__((constructor)) static void port_install_exit_note(void)
{
    atexit(port_note_exit);
}

void OSClearStack(u8 val) { (void)val; }

u32 __OSFpscrEnableBits = 0;

typedef void (*OSErrorHandler)(u16, void*, ...);
OSErrorHandler OSSetErrorHandler(u16 error, OSErrorHandler handler)
{
    (void)error;
    (void)handler;
    return NULL;
}

u8 __OSReport_disable = 0;
u8 __OSReport_enable = 1;
u8 __OSReport_Error_disable = 0;
u8 __OSReport_System_disable = 0;
u8 __OSReport_Warning_disable = 0;

void OSReport(const char* msg, ...)
{
    if (__OSReport_disable)
        return;
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
}
