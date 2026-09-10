// See include/port/firstrun.h for what this is and when it declines to do it.

#include "port/firstrun.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/host.h"

// The three shapes the settings application ships in, in the order they are looked for.
static const char* const kCandidates[] = {
#if defined(_WIN32)
    "strikers-settings.exe",
#elif defined(__APPLE__)
    "strikers-settings.app/Contents/MacOS/strikers-settings",
    "strikers-settings",
#else
    "strikers-settings",
#endif
    NULL,
};

static int exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

// A boolean the way a person writes one, matching src/platform/launch.cpp's: `first_run = no` and
// `first_run = 0` are the same statement.
static int env_bool(const char* name, int fallback)
{
    const char* v = getenv(name);
    if (v == NULL || *v == '\0')
        return fallback;
    if (strcmp(v, "0") == 0 || strcmp(v, "no") == 0 || strcmp(v, "off") == 0 ||
        strcmp(v, "false") == 0 || strcmp(v, "FALSE") == 0)
        return 0;
    return 1;
}

// The settings application's path, or 0 if there is not one to run.
static int find_settings(char* out, size_t size)
{
    char dir[1024];
    size_t i;

    {
        const char* named = getenv("STRIKERS_SETTINGS_EXE");
        if (named != NULL && *named != '\0')
        {
            if (!exists(named))
            {
                // Named and missing, unlike the searched locations below: the typo is the whole
                // content of the failure and is worth saying.
                fprintf(stderr, "[port] STRIKERS_SETTINGS_EXE=%s: no such file\n",
                        named);
                return 0;
            }
            snprintf(out, size, "%s", named);
            return 1;
        }
    }

    if (port_executable_dir(dir, sizeof dir) != 0)
        return 0;

    for (i = 0; kCandidates[i] != NULL; i++)
    {
        snprintf(out, size, "%s/%s", dir, kCandidates[i]);
        if (exists(out))
            return 1;
    }

    out[0] = '\0';
    return 0;
}

int PortFirstRunSettings(void)
{
    char exe[1024];
    // --first-run is what stops the window's Play button starting a second copy of a game that is
    // already running and waiting for it.
    static const char* const kArgs[] = { "--first-run", NULL };
    int status = 0;
    int ran;

    if (!env_bool("STRIKERS_FIRST_RUN", 1))
        return 0;

    {
        const char* explicitPath = getenv("STRIKERS_CONFIG");
        if (explicitPath != NULL && *explicitPath != '\0')
            return 0;
    }

    if (!find_settings(exe, sizeof exe))
        return 0;

    fprintf(stderr, "[port] first run: %s\n", exe);
    fflush(stderr);

    ran = port_run_wait(exe, kArgs, &status);

    // Neither of these is fatal, and neither returns 1; so neither writes the file that would stop
    // the question being asked again.
    if (ran < 0)
    {
        fprintf(stderr, "[port] first run: could not start %s; continuing with "
                        "the defaults\n", exe);
        return 0;
    }
    if (ran > 0)
    {
        fprintf(stderr, "[port] first run: %s did not exit normally; continuing "
                        "with the defaults\n", exe);
        return 0;
    }

    return 1;
}
