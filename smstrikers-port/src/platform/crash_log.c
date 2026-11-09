#include "port/crash_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "port/config.h"
#include "strikers_version.h"

#if defined(_WIN32)
#include <io.h>
#define port_sync(fd) _commit(fd)
#define port_fno(f) _fileno(f)
#else
#include <unistd.h>
#define port_sync(fd) fsync(fd)
#define port_fno(f) fileno(f)
#endif

// Enough lines to show a pattern, few enough that a stuck caller cannot fill the card.
#define PORT_CRASH_LOG_MAX 16

static FILE* s_file;
static int s_opened;
static unsigned int s_written;
static unsigned int s_suppressed;

static void crash_log_path(char* buf, size_t size)
{
    char dir[1024];
    char stamp[32] = "";
    const time_t now = time(NULL);
    const struct tm* local = localtime(&now);

    if (local != NULL)
        strftime(stamp, sizeof stamp, "-%Y-%m-%d_%H-%M-%S", local);

    if (PortConfigDir(dir, sizeof dir) == 0)
        snprintf(buf, size, "%s/strikers-crash-log%s.txt", dir, stamp);
    else
        snprintf(buf, size, "strikers-crash-log%s.txt", stamp);
}

static int crash_log_sync(void)
{
    int fd;

    if (fflush(s_file) != 0)
        return -1;

    // Horizon keeps the write in its own cache otherwise, and a process that dies takes it.
    fd = port_fno(s_file);
    if (fd >= 0 && port_sync(fd) != 0)
        return -1;
    return 0;
}

static void crash_log_open(void)
{
    char path[1088];

    s_opened = 1;
    crash_log_path(path, sizeof path);

    s_file = fopen(path, "w");
    if (s_file == NULL)
        return;

    fprintf(s_file, "strikers %s %s\n", STRIKERS_VERSION_STR, PORT_BUILD_TYPE);
    crash_log_sync();
}

int PortCrashLog(const char* fmt, ...)
{
    va_list args;

    if (!s_opened)
        crash_log_open();
    if (s_file == NULL)
        return -1;

    if (s_written >= PORT_CRASH_LOG_MAX)
    {
        ++s_suppressed;
        // At each power of two, so the last line bounds the count within a factor of two.
        if ((s_suppressed & (s_suppressed - 1u)) == 0)
        {
            fprintf(s_file, "further lines suppressed: %u\n", s_suppressed);
            crash_log_sync();
        }
        return -1;
    }

    va_start(args, fmt);
    vfprintf(s_file, fmt, args);
    va_end(args);
    ++s_written;
    return crash_log_sync();
}
