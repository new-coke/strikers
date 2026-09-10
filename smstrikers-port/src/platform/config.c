// See include/port/config.h for what this is and why it works by writing to the environment rather
// than by being consulted.

#include "port/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/firstrun.h"
#include "port/host.h"

// open_log moves stderr onto a file by descriptor, so the file is open before stderr is given up.
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define port_dup2 _dup2
#define port_fileno _fileno
#else
#include <unistd.h>
#define port_dup2 dup2
#define port_fileno fileno
#endif

#define PORT_CONFIG_NAME "strikers.ini"
#define PORT_CONFIG_EXAMPLE "strikers.ini.example"
#define PORT_CONFIG_PREFIX "STRIKERS_"

static char s_path[1024];
static int s_loaded;
// Remembered, not recomputed, because PortConfigLoad is now called from more than one place:
// whoever asks second still gets the honest count rather than a 0 that reads as "there was no
// file".
static int s_applied;

const char* PortConfigPath(void) { return s_path[0] != '\0' ? s_path : NULL; }

static int exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

// Trim ASCII whitespace in place, returning the new start.
static char* trim(char* s)
{
    char* end;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    if (*s == '\0')
        return s;
    end = s + strlen(s);
    while (end > s)
    {
        const char c = end[-1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            break;
        end--;
    }
    *end = '\0';
    return s;
}

// `key` as the environment spells it: upper-cased, with STRIKERS_ supplied if the file left it off.
static int env_name(const char* key, char* out, size_t size)
{
    size_t i;
    size_t n = strlen(key);
    int prefixed;

    if (n == 0)
        return -1;

    // Case-insensitive prefix test, so `strikers_msaa` is not double-prefixed.
    prefixed = 1;
    for (i = 0; i < sizeof(PORT_CONFIG_PREFIX) - 1; i++)
    {
        char a = key[i];
        char b = PORT_CONFIG_PREFIX[i];
        if (a >= 'a' && a <= 'z')
            a = (char)(a - 'a' + 'A');
        if (i >= n || a != b)
        {
            prefixed = 0;
            break;
        }
    }

    if (prefixed)
    {
        if (n + 1 > size)
            return -1;
        memcpy(out, key, n + 1);
    }
    else
    {
        const size_t p = sizeof(PORT_CONFIG_PREFIX) - 1;
        if (p + n + 1 > size)
            return -1;
        memcpy(out, PORT_CONFIG_PREFIX, p);
        memcpy(out + p, key, n + 1);
    }

    for (i = 0; out[i] != '\0'; i++)
    {
        if (out[i] >= 'a' && out[i] <= 'z')
            out[i] = (char)(out[i] - 'a' + 'A');
    }
    return 0;
}

static int load_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    char line[1024];
    int applied = 0;

    if (f == NULL)
        return -1;

    while (fgets(line, (int)sizeof line, f) != NULL)
    {
        char name[256];
        char* p = trim(line);
        char* eq;
        char* key;
        char* val;
        size_t vlen;

        if (*p == '\0' || *p == '#' || *p == ';')
            continue;
        // A section header organises the file; nothing here needs its name.
        if (*p == '[')
            continue;

        eq = strchr(p, '=');
        if (eq == NULL)
            continue;
        *eq = '\0';
        key = trim(p);
        val = trim(eq + 1);

        // Strip one layer of matching quotes, so a value with meaningful trailing space can be
        // written.
        vlen = strlen(val);
        if (vlen >= 2 && ((val[0] == '"' && val[vlen - 1] == '"') ||
                          (val[0] == '\'' && val[vlen - 1] == '\'')))
        {
            val[vlen - 1] = '\0';
            val++;
        }

        if (*key == '\0')
            continue;
        if (env_name(key, name, sizeof name) != 0)
            continue;
        if (port_setenv_default(name, val) == 0)
            applied++;
    }

    fclose(f);
    return applied;
}

static int load_config(void);
static int seed_config(const char* dir, char* path, size_t size);

// STRIKERS_LOG / the `log` key: send the run's diagnostics to a file.
// Borrow the parent's console on Windows: the executable is linked for the GUI subsystem so a
// double-click opens no cmd window, which leaves the same binary run from a terminal, from CI or
// under a test harness with nowhere to write. AttachConsole takes a console the parent already
// owns and fails when there is none, so it can never create one; the streams are re-pointed by
// hand because attaching leaves the CRT's inherited handles invalid.
static void attach_parent_console(void)
{
#ifdef _WIN32
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    (void)freopen("CONOUT$", "w", stdout);
    (void)freopen("CONOUT$", "w", stderr);
#endif
}

static void open_log(void)
{
    const char* v = getenv("STRIKERS_LOG");
    char path[1024];

    if (v == NULL || *v == '\0' || strcmp(v, "0") == 0)
        return;

    if (strcmp(v, "1") == 0)
    {
        char dir[1024];
        if (port_executable_dir(dir, sizeof dir) == 0)
            snprintf(path, sizeof path, "%s/strikers-log.txt", dir);
        else
            snprintf(path, sizeof path, "strikers-log.txt");
    }
    else
    {
        snprintf(path, sizeof path, "%s", v);
    }

    fprintf(stderr, "[port] logging to %s\n", path);
    fflush(stderr);
    // Open the destination first: freopen closes the stream it is given before trying the new
    // path, so a bad path would leave stderr closed and the message below unseen.
    {
        FILE* f = fopen(path, "w");
        if (f == NULL)
        {
            fprintf(stderr, "[port] STRIKERS_LOG=%s: could not open %s for "
                            "writing; logging to stderr as before\n", v, path);
            return;
        }
        if (port_dup2(port_fileno(f), port_fileno(stderr)) < 0)
        {
            // No descriptor behind stderr (a Windows GUI process has none); freopen is safe now
            // the path is known to open.
            fclose(f);
            if (freopen(path, "w", stderr) == NULL)
                return;
        }
        else
        {
            fclose(f);
        }
    }
    // Unbuffered, because the run this is most wanted for is the one that ends in a crash, and a
    // buffered last few hundred lines is exactly the part that would be missing.
    setvbuf(stderr, NULL, _IONBF, 0);
}

int PortConfigLoad(void)
{
    if (s_loaded)
        return s_applied;

    attach_parent_console();
    s_applied = load_config();
    // After the file, not before: `log` is a key like any other, and a player asked to turn logging
    // on will put it in strikers.ini rather than set an environment variable.
    open_log();
    return s_applied;
}

static int load_config(void)
{
    char dir[1024];
    int haveDir;

    s_loaded = 1;

    {
        const char* explicitPath = getenv("STRIKERS_CONFIG");
        if (explicitPath != NULL && *explicitPath != '\0')
        {
            // An explicit path is not silently skipped when it is missing, unlike the two searched
            // locations, naming one is a statement that it should be there, and the typo is worth
            // reporting.
            if (!exists(explicitPath))
            {
                fprintf(stderr, "[port] STRIKERS_CONFIG=%s: no such file\n",
                        explicitPath);
                return -1;
            }
            snprintf(s_path, sizeof s_path, "%s", explicitPath);
            return load_file(s_path);
        }
    }

    haveDir = port_executable_dir(dir, sizeof dir) == 0;
    if (haveDir)
    {
        snprintf(s_path, sizeof s_path, "%s/%s", dir, PORT_CONFIG_NAME);
        if (exists(s_path))
            return load_file(s_path);
    }

    snprintf(s_path, sizeof s_path, "%s", PORT_CONFIG_NAME);
    if (exists(s_path))
        return load_file(s_path);

    // Not "there is no file", but "nobody has ever been asked".
    if (haveDir && PortFirstRunSettings())
    {
        snprintf(s_path, sizeof s_path, "%s/%s", dir, PORT_CONFIG_NAME);
        if (exists(s_path))
            return load_file(s_path);
        // The window was closed without saving. Write the file anyway, so the question is asked
        // once rather than at every launch; see firstrun.h for why the file's existence is the
        // marker and what is written.
        if (seed_config(dir, s_path, sizeof s_path) == 0)
            return load_file(s_path);
    }

    s_path[0] = '\0';
    return 0;
}

// Create strikers.ini in `dir` from strikers.ini.example beside it, writing the path into `path`.
static int seed_config(const char* dir, char* path, size_t size)
{
    char examplePath[1024];
    FILE* src;
    FILE* dst;

    snprintf(path, size, "%s/%s", dir, PORT_CONFIG_NAME);

    dst = fopen(path, "wb");
    if (dst == NULL)
        return -1;

    snprintf(examplePath, sizeof examplePath, "%s/%s", dir, PORT_CONFIG_EXAMPLE);
    src = fopen(examplePath, "rb");
    if (src != NULL)
    {
        char buf[4096];
        size_t n;
        int ok = 1;
        while ((n = fread(buf, 1, sizeof buf, src)) > 0)
        {
            if (fwrite(buf, 1, n, dst) != n)
            {
                ok = 0;
                break;
            }
        }
        fclose(src);
        fclose(dst);
        if (ok)
            return 0;
        remove(path);
        return -1;
    }

    // No example beside the game. Two lines, because an empty file says nothing about why it is
    // there and this one has to survive being found by someone who did not create it.
    fprintf(dst, "# %s: settings for the Super Mario Strikers native port.\n"
                 "# Written on the first run. Every setting has a default; see\n"
                 "# %s if the release shipped one, or run the settings\n"
                 "# application beside this file.\n",
            PORT_CONFIG_NAME, PORT_CONFIG_EXAMPLE);
    if (fclose(dst) != 0)
    {
        remove(path);
        return -1;
    }
    return 0;
}
