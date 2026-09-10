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

int port_run_wait(const char* exe, const char* const* args, int* exitCode)
{
    // One buffer, built by hand, because CreateProcess takes a command line rather than an argv and
    // the child's CRT splits it back apart under rules that are not "separate on spaces".
    char cmd[4096];
    size_t used = 0;
    size_t i;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD status = 0;

    if (exe == NULL || *exe == '\0')
        return -1;

    {
        const int n = _snprintf_s(cmd, sizeof cmd, _TRUNCATE, "\"%s\"", exe);
        if (n < 0)
            return -1;
        used = (size_t)n;
    }
    for (i = 0; args != NULL && args[i] != NULL; i++)
    {
        const int n = _snprintf_s(cmd + used, sizeof cmd - used, _TRUNCATE,
                                  " \"%s\"", args[i]);
        if (n < 0)
            return -1;
        used += (size_t)n;
    }

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);

    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &status))
        status = 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exitCode != NULL)
        *exitCode = (int)status;
    return 0;
}

#else

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
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
    // No portable answer here, and the two spellings share nothing: Darwin hands back a path that
    // may be relative or contain symlinks and must be resolved, Linux exposes the resolved path as
    // a symlink in /proc.
#if defined(__APPLE__)
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

int port_run_wait(const char* exe, const char* const* args, int* exitCode)
{
    // fork/exec rather than system(): system() hands the string to /bin/sh, which would split a
    // path on its spaces and read a $ or a; in one as punctuation.
    char* argv[64];
    const size_t cap = sizeof argv / sizeof argv[0];
    size_t n = 0;
    size_t i;
    pid_t pid;
    int status = 0;

    if (exe == NULL || *exe == '\0')
        return -1;

    // execv's prototype is char* const[] for historical reasons and does not write through them;
    // the cast is the standard one. argv[0] is the program, which is why `args` does not repeat it.
    argv[n++] = (char*)exe;
    for (i = 0; args != NULL && args[i] != NULL && n + 1 < cap; i++)
        argv[n++] = (char*)args[i];
    argv[n] = NULL;

    pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0)
    {
        execv(exe, argv);
        // _exit, not exit: the child shares the parent's stdio buffers and atexit handlers, and
        // running either would flush and unwind state the parent still owns.
        _exit(127);
    }

    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
            return -1;
    }

    if (!WIFEXITED(status))
        return 1;
    if (exitCode != NULL)
        *exitCode = WEXITSTATUS(status);
    // 127 is the child's own "execv failed" above, and is indistinguishable from a program that
    // genuinely exited 127.
    return WEXITSTATUS(status) == 127 ? -1 : 0;
}

#endif
