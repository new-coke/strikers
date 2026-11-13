// The one place the port branches on the host OS.

#include "port/host.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <malloc.h>
// stdio for _snprintf_s and stdlib for getenv, both used below.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long long port_monotonic_ns(void)
{
    // The frequency is fixed for the lifetime of the process, and since Windows 7
    // QueryPerformanceCounter is a cheap userspace read.
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    // Split the division so a counter running for weeks cannot overflow the multiply: whole seconds
    // first, then the remainder scaled to nanoseconds.
    return (unsigned long long)(now.QuadPart / freq.QuadPart) * 1000000000ull
         + (unsigned long long)(now.QuadPart % freq.QuadPart) * 1000000000ull
               / (unsigned long long)freq.QuadPart;
}

void port_sleep_ns(unsigned long long ns)
{
    static HANDLE timer;
    static int tried;
    LARGE_INTEGER due;

    if (ns == 0)
        return;

    if (!tried)
    {
        tried = 1;
        timer = CreateWaitableTimerExW(NULL, NULL,
                                       CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                       TIMER_ALL_ACCESS);
    }

    if (timer != NULL)
    {
        // Negative means relative, in 100ns units.
        due.QuadPart = -(LONGLONG)(ns / 100ull);
        if (due.QuadPart == 0)
            due.QuadPart = -1;
        if (SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE))
        {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }

    Sleep((DWORD)(ns / 1000000ull));
}

void port_sleep_until_ns(unsigned long long deadline_ns)
{
    const unsigned long long now = port_monotonic_ns();
    if (deadline_ns > now)
        port_sleep_ns(deadline_ns - now);
}

void port_tighten_timer_slack(void) {}

void port_yield(void)
{
    // SwitchToThread yields only to a thread on the same processor and returns at once when there
    // is none: give way if anyone is waiting, never sleep.
    SwitchToThread();
}

void* port_aligned_alloc(size_t alignment, size_t size)
{
    return _aligned_malloc(size, alignment);
}

void port_aligned_free(void* ptr)
{
    _aligned_free(ptr);
}

int port_localtime(time_t when, struct tm* out)
{
    // localtime_s, and note the argument order is the reverse of POSIX's localtime_r.
    return localtime_s(out, &when) == 0 ? 0 : -1;
}

int port_executable_dir(char* buf, size_t size)
{
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)size);
    if (n == 0 || n >= size)
        return -1;
    // Strip the file name. Both separators are legal in a Win32 path and a path built by hand can
    // carry either, so look for the last of each.
    char* slash = strrchr(buf, '\\');
    char* fwd = strrchr(buf, '/');
    if (fwd != NULL && (slash == NULL || fwd > slash))
        slash = fwd;
    if (slash == NULL)
        return -1;
    *slash = '\0';
    return 0;
}

int port_scan_dir(const char* path,
                  void (*visit)(void* user, const char* name),
                  void* user)
{
    char pattern[1024];
    WIN32_FIND_DATAA find;
    HANDLE h;

    // FindFirstFile wants a wildcard path.
    if (_snprintf_s(pattern, sizeof pattern, _TRUNCATE, "%s\\*", path) < 0)
        return -1;
    h = FindFirstFileA(pattern, &find);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    do
    {
        if (find.cFileName[0] == '.' &&
            (find.cFileName[1] == '\0' ||
             (find.cFileName[1] == '.' && find.cFileName[2] == '\0')))
            continue;
        visit(user, find.cFileName);
    } while (FindNextFileA(h, &find));
    FindClose(h);
    return 0;
}

int port_setenv_default(const char* name, const char* value)
{
    // _putenv_s, not setenv: setenv is POSIX and absent here.
    if (getenv(name) != NULL)
        return 1;
    return _putenv_s(name, value) == 0 ? 0 : -1;
}

#else

#include <dirent.h>
#include <limits.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#if defined(__linux__)
#include <errno.h>
#include <sys/prctl.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

unsigned long long port_monotonic_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ull
         + (unsigned long long)ts.tv_nsec;
}

void port_sleep_ns(unsigned long long ns)
{
    struct timespec req;
    req.tv_sec = (time_t)(ns / 1000000000ull);
    req.tv_nsec = (long)(ns % 1000000000ull);
    nanosleep(&req, NULL);
}

void port_sleep_until_ns(unsigned long long deadline_ns)
{
#if defined(__linux__)
    struct timespec ts;
    ts.tv_sec = (time_t)(deadline_ns / 1000000000ull);
    ts.tv_nsec = (long)(deadline_ns % 1000000000ull);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR)
    {
    }
#else
    const unsigned long long now = port_monotonic_ns();
    if (deadline_ns > now)
        port_sleep_ns(deadline_ns - now);
#endif
}

void port_tighten_timer_slack(void)
{
#if defined(__linux__)
    // 20 us, so the frame limiter can sleep once to just short of its deadline.
    prctl(PR_SET_TIMERSLACK, 20000ul, 0ul, 0ul, 0ul);
#endif
}

void port_yield(void)
{
    sched_yield();
}

void* port_aligned_alloc(size_t alignment, size_t size)
{
    // Not C11 aligned_alloc: that requires size to be a multiple of alignment, and the callers here
    // pass whatever the game asked for.
    void* p = NULL;
    if (posix_memalign(&p, alignment, size) != 0)
        return NULL;
    return p;
}

void port_aligned_free(void* ptr)
{
    free(ptr);
}

int port_localtime(time_t when, struct tm* out)
{
    return localtime_r(&when, out) != NULL ? 0 : -1;
}

int port_executable_dir(char* buf, size_t size)
{
    // No portable answer here, and the spellings share nothing: Darwin hands back a path that may
    // be relative or contain symlinks and must be resolved, Linux exposes the resolved path as a
    // symlink in /proc, and Android has no executable of its own to point at. /proc/self/exe there
    // is /system/bin/app_process64, the zygote every application on the device is forked from: the
    // same directory for all of them, and holding nothing of ours.
    //
    // So on Android the answer is supplied rather than discovered. The activity puts the app's
    // private files directory in STRIKERS_HOME before it loads this library, and that directory is
    // what "beside the game" means there; everything downstream, strikers.ini and the log and the
    // button prompt artwork and the files/ scan in DVDInit, then works unchanged.
#if defined(__ANDROID__)
    {
        const char* home = getenv("STRIKERS_HOME");
        size_t n;
        if (home == NULL || home[0] == 0)
            return -1;
        n = strlen(home);
        if (n >= size)
            return -1;
        memcpy(buf, home, n + 1);
        // No trailing separator, as the contract above says, so that the callers' "%s/%s" is right.
        while (n > 1 && buf[n - 1] == '/')
        {
            n--;
            buf[n] = 0;
        }
        return 0;
    }
#elif defined(__APPLE__)
    char raw[4096];
    uint32_t rawSize = (uint32_t)sizeof raw;
    if (_NSGetExecutablePath(raw, &rawSize) != 0)
        return -1;
    char resolved[4096];
    if (realpath(raw, resolved) == NULL)
        return -1;
    if (strlen(resolved) >= size)
        return -1;
    strcpy(buf, resolved);
#else
    ssize_t n = readlink("/proc/self/exe", buf, size - 1);
    if (n <= 0 || (size_t)n >= size - 1)
        return -1;
    buf[n] = '\0';
#endif
    char* slash = strrchr(buf, '/');
    if (slash == NULL)
        return -1;
    *slash = '\0';
    return 0;
}

int port_scan_dir(const char* path,
                  void (*visit)(void* user, const char* name),
                  void* user)
{
    DIR* d = opendir(path);
    struct dirent* e;
    if (d == NULL)
        return -1;
    while ((e = readdir(d)) != NULL)
    {
        if (e->d_name[0] == '.' &&
            (e->d_name[1] == '\0' ||
             (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;
        visit(user, e->d_name);
    }
    closedir(d);
    return 0;
}

int port_setenv_default(const char* name, const char* value)
{
    if (getenv(name) != NULL)
        return 1;
    return setenv(name, value, 0) == 0 ? 0 : -1;
}

#endif

int port_docked(void) { return -1; }
