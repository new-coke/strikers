// See include/port/config.h for what this is and why it works by writing to the environment rather
// than by being consulted.

#include "port/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define PORT_CONFIG_PREFIX "STRIKERS_"

static char s_path[1024];
static int s_loaded;
// Remembered, not recomputed, because PortConfigLoad is now called from more than one place:
// whoever asks second still gets the honest count rather than a 0 that reads as "there was no
// file".
static int s_applied;

const char* PortConfigPath(void) { return s_path[0] != '\0' ? s_path : NULL; }

int PortConfigDir(char* buf, size_t size)
{
    const char* slash = strrchr(s_path, '/');
#ifdef _WIN32
    const char* back = strrchr(s_path, '\\');
    if (back != NULL && (slash == NULL || back > slash))
        slash = back;
#endif
    if (slash == NULL)
        return port_executable_dir(buf, size);
    if ((size_t)(slash - s_path) >= size)
        return -1;
    snprintf(buf, size, "%.*s", (int)(slash - s_path), s_path);
    return 0;
}

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

// Case-insensitive test for `prefix` at the head of `key`.
static int has_prefix(const char* key, size_t n, const char* prefix)
{
    size_t i;
    const size_t p = strlen(prefix);

    for (i = 0; i < p; i++)
    {
        char a = key[i];
        if (a >= 'a' && a <= 'z')
            a = (char)(a - 'a' + 'A');
        if (i >= n || a != prefix[i])
            return 0;
    }
    return 1;
}

// `key` as the environment spells it: upper-cased, with STRIKERS_ supplied if the file left it off.
static int env_name(const char* key, char* out, size_t size)
{
    size_t i;
    size_t n = strlen(key);
    int prefixed;

    if (n == 0)
        return -1;

    prefixed = has_prefix(key, n, PORT_CONFIG_PREFIX);
#if defined(__SWITCH__)
    // Allow Aurora settings in strikers.ini without adding the STRIKERS_ prefix.
    prefixed = prefixed || has_prefix(key, n, "AURORA_");
#endif

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

// A relative path in the file is taken from the game folder, as the settings app reads it.
static int is_path_key(const char* name)
{
    return strcmp(name, "STRIKERS_DATA") == 0 || strcmp(name, "STRIKERS_TEXTURES") == 0 ||
           strcmp(name, "STRIKERS_TEXTURE_DUMP") == 0 || strcmp(name, "STRIKERS_USER_DIR") == 0 ||
           strcmp(name, "STRIKERS_CACHE_DIR") == 0;
}

static int load_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    char line[1024];
    char dir[1024];
    char full[2048];
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
        if (is_path_key(name) && val[0] != '\0' && val[0] != '/' && val[0] != '\\' &&
            val[1] != ':' && strcmp(val, "0") != 0 && strcmp(val, "1") != 0 &&
            port_executable_dir(dir, sizeof dir) == 0)
        {
            snprintf(full, sizeof full, "%s/%s", dir, val);
            val = full;
        }
        if (port_setenv_default(name, val) == 0)
            applied++;
    }

    fclose(f);
    return applied;
}

static int load_config(void);

// AttachConsole never creates a console and leaves the CRT's handles invalid, so they are reopened.
static void attach_parent_console(void)
{
#ifdef _WIN32
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    (void)freopen("CONOUT$", "w", stdout);
    (void)freopen("CONOUT$", "w", stderr);
#endif
}

#if defined(__SWITCH__)
int PortSwitchLogToFile(const char* path);
#endif

static void open_log(void)
{
    const char* v = getenv("STRIKERS_LOG");
    char path[1024];

    if (v == NULL || *v == '\0' || strcmp(v, "0") == 0)
        return;

    if (strcmp(v, "console") == 0)
    {
        attach_parent_console();
        return;
    }

    if (strcmp(v, "1") == 0)
    {
        char dir[1024];
        char previous[1100];
        if (PortConfigDir(dir, sizeof dir) != 0)
            snprintf(dir, sizeof dir, ".");
        snprintf(path, sizeof path, "%s/strikers-log.txt", dir);
        // A relaunch after a crash would otherwise overwrite the crash's log.
        snprintf(previous, sizeof previous, "%s/strikers-log-previous.txt", dir);
        remove(previous);
        rename(path, previous);
    }
    else
    {
        snprintf(path, sizeof path, "%s", v);
    }

#if defined(__SWITCH__)
    // Share one file descriptor for both streams, or keep the existing nxlink connection.
    (void)PortSwitchLogToFile(path);
    return;
#endif
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

// portable = 1 defaults user_dir and cache_dir to user/ and cache/ beside the game.
static void portable_folders(void)
{
    char dir[1024];
    char path[1100];
    const char* v = getenv("STRIKERS_PORTABLE");

    if (v == NULL || *v == '\0' || strcmp(v, "0") == 0 || strcmp(v, "false") == 0 ||
        strcmp(v, "no") == 0 || strcmp(v, "off") == 0 || strcmp(v, "FALSE") == 0)
        return;
    if (port_executable_dir(dir, sizeof dir) != 0)
        return;
    snprintf(path, sizeof path, "%s/user", dir);
    port_setenv_default("STRIKERS_USER_DIR", path);
    snprintf(path, sizeof path, "%s/cache", dir);
    port_setenv_default("STRIKERS_CACHE_DIR", path);
}

int PortConfigLoad(void)
{
    if (s_loaded)
        return s_applied;

    s_applied = load_config();
    portable_folders();
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

    s_path[0] = '\0';
    return 0;
}
