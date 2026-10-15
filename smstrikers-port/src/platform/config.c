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
#if defined(PORT_VITA)
#define PORT_VITA_DEFAULTS_BEGIN "# --- STRIKERS VALIDATED DEFAULTS BEGIN ---"
#define PORT_VITA_DEFAULTS_END "# --- STRIKERS VALIDATED DEFAULTS END ---"
#define PORT_VITA_DEFAULTS_REVISION "2026-10-15-vitagl-unified"

typedef struct VitaDefaultConfigEntry
{
    const char* key;
    const char* value;
} VitaDefaultConfigEntry;

// Single source of truth for the selected Vita runtime baseline. Generate the
// INI from this table when missing and refresh an outdated managed block;
// leave an already current file untouched to avoid redundant storage writes.
static const VitaDefaultConfigEntry s_vita_defaults[] = {
    { "diagnostics", "0" },
    { "fps_overlay", "0" },
    { "vita_test_match", "0" },
    { "vita_frameskip", "0" },
    { "vita_debug_menu", "0" },
    { "seed", "0x53545249" },
    { "fixed_dt", "0" },
    { "frame_capture", "" },
    { "frame_capture_frames", "0" },
    { "frame_capture_match_only", "1" },
    { "frame_capture_play_only", "1" },
    { "frame_capture_skip", "600" },
    { "vita_snapshot_match_frame", "0" },
    { "vita_snapshot_play_frame", "0" },
    { "log", "ux0:data/strikersVita/vitagl.log" },
    { "asset_archive", "ux0:data/strikersVita/sms.psarc" },
    { "vita_native_audio", "1" },
    { "vita_native_video", "1" },
    { "task_profile", "0" },
    { "asset_io_profile", "0" },
    { "vita_view_draw_capture", "0" },
    { "benchmark", "0" },
    { "cpu_mhz", "500" },
    { "gpu_mhz", "222" },
    { "vitagl_timing", "0" },
};
#endif

static char s_path[1024];
static int s_loaded;
// Remembered, not recomputed, because PortConfigLoad is now called from more than one place:
// whoever asks second still gets the honest count rather than a 0 that reads as "there was no
// file".
static int s_applied;
static int s_diagnostics = 1;
static int s_fpsOverlay = 1;

int PortDiagnosticsEnabled(void) { return s_diagnostics; }
int PortFpsOverlayEnabled(void) { return s_fpsOverlay; }
void PortSetFpsOverlayEnabled(int enabled) { s_fpsOverlay = enabled != 0; }

static void configure_diagnostics(void)
{
    const char* value = getenv("STRIKERS_DIAGNOSTICS");
    s_diagnostics = value == NULL || strcmp(value, "0") != 0;
    const char* fps = getenv("STRIKERS_FPS_OVERLAY");
    s_fpsOverlay = fps == NULL || strcmp(fps, "0") != 0;
    if (s_diagnostics)
        return;

    // Several older probes test presence, so setting them to "0" would still
    // run their dumps/timers. Remove only diagnostic selectors from this
    // process; the INI remains intact for the next diagnostics=1 launch.
    static const char* const selectors[] = {
        "STRIKERS_TASK_PROFILE", "STRIKERS_TASK_PROFILE_FRAMES", "STRIKERS_TASK_PROFILE_BUFFERED",
        "STRIKERS_VITA_PACKET_PROFILE_PERIOD", "STRIKERS_VITA_PROFILE", "STRIKERS_VITA_PROFILE_PATH",
        "STRIKERS_AURORA_DIAGNOSTICS", "STRIKERS_PROFILE_VERTEX_PHASES",
        "STRIKERS_VITA_TEXTURE_DIAGNOSTICS", "STRIKERS_VITA_3D_DIAGNOSTICS",
        "STRIKERS_PROBE_SKIN", "STRIKERS_PROBE_TEX", "STRIKERS_PROBE_PAD",
        "STRIKERS_PROBE_ARRAY", "STRIKERS_PROBE_DRAW", "STRIKERS_PROBE_OBJ",
        "STRIKERS_PROBE_DVD", "STRIKERS_PROBE_TEXDUMP", "STRIKERS_PROBE_AI",
        "STRIKERS_PROBE_CHARDRAW", "STRIKERS_PROBE_WIDEN", "STRIKERS_PROBE_PAIRS", "STRIKERS_PROBE_GK",
        "STRIKERS_LOG", "STRIKERS_LOG_AUDIO", "STRIKERS_LOG_SCENES", "STRIKERS_LOG_MATCH",
        "STRIKERS_LOG_FOG", "STRIKERS_LOG_FRUSTUM", "STRIKERS_LOG_NIS", "STRIKERS_LOG_BUNDLES",
        "STRIKERS_LOG_EVENTS", "STRIKERS_AUDIO_DUMP", "STRIKERS_TEXTURE_DUMP",
        "STRIKERS_DUMP_FEN", "STRIKERS_DUMP_ICON", "STRIKERS_CAPTURE", "STRIKERS_CAPTURE_FRAME",
        "STRIKERS_VIDEO_CAPTURE", "STRIKERS_CONFIG_PROBE", "STRIKERS_WATCH_MORPH_TRAP",
        "STRIKERS_VITA_SNAPSHOT_3D_FRAME", "STRIKERS_VITA_SNAPSHOT_PATH",
        "STRIKERS_WATCH_MORPH", "STRIKERS_WATCH_MORPH_PROT",
        "STRIKERS_WATCH_MORPH_PROT_FROM", "STRIKERS_WATCH_MORPH_PROT_TO",
    };
    for (size_t i = 0; i < sizeof selectors / sizeof selectors[0]; ++i)
    {
#ifdef _WIN32
        (void)_putenv_s(selectors[i], "");
#else
        (void)unsetenv(selectors[i]);
#endif
    }
}

const char* PortConfigPath(void) { return s_path[0] != '\0' ? s_path : NULL; }



static int exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

#if defined(PORT_VITA)
static int line_is_marker(const char* line, const char* marker)
{
    const size_t n = strlen(marker);
    while (*line == ' ' || *line == '\t')
        line++;
    if (strncmp(line, marker, n) != 0)
        return 0;
    line += n;
    return *line == '\0' || *line == '\r' || *line == '\n';
}

static int write_vita_default_block(FILE* f)
{
    if (fprintf(f, "%s\n", PORT_VITA_DEFAULTS_BEGIN) < 0 ||
        fprintf(f, "# profile_revision = %s\n", PORT_VITA_DEFAULTS_REVISION) < 0 ||
        fprintf(f, "# Managed automatically. Put custom overrides outside this block.\n") < 0)
        return -1;
    for (size_t i = 0; i < sizeof s_vita_defaults / sizeof s_vita_defaults[0]; ++i)
    {
        if (fprintf(f, "%s = %s\n", s_vita_defaults[i].key, s_vita_defaults[i].value) < 0)
            return -1;
    }
    return fprintf(f, "%s\n", PORT_VITA_DEFAULTS_END) < 0 ? -1 : 0;
}

static int vita_default_block_current(const char* path)
{
    FILE* in = fopen(path, "rb");
    char line[1024], expected[256];
    int found = 0, valid = 0;
    if (in == NULL)
        return 0;
    while (fgets(line, sizeof line, in) != NULL)
    {
        if (line_is_marker(line, PORT_VITA_DEFAULTS_BEGIN))
        {
            found = strcmp(line, PORT_VITA_DEFAULTS_BEGIN "\n") == 0;
            break;
        }
    }
    if (!found)
        goto done;
    snprintf(expected, sizeof expected, "# profile_revision = %s\n", PORT_VITA_DEFAULTS_REVISION);
    if (fgets(line, sizeof line, in) == NULL || strcmp(line, expected) != 0 ||
        fgets(line, sizeof line, in) == NULL ||
        strcmp(line, "# Managed automatically. Put custom overrides outside this block.\n") != 0)
        goto done;
    for (size_t i = 0; i < sizeof s_vita_defaults / sizeof s_vita_defaults[0]; ++i)
    {
        snprintf(expected, sizeof expected, "%s = %s\n", s_vita_defaults[i].key, s_vita_defaults[i].value);
        if (fgets(line, sizeof line, in) == NULL || strcmp(line, expected) != 0)
            goto done;
    }
    // Overrides after the managed block must still move before defaults. Only
    // an exact current block at EOF is safe to keep without rewriting.
    if (fgets(line, sizeof line, in) != NULL && strcmp(line, PORT_VITA_DEFAULTS_END "\n") == 0 &&
        fgetc(in) == EOF && !ferror(in))
        valid = 1;
done:
    fclose(in);
    return valid;
}

static int refresh_vita_default_config(const char* path)
{
    char temp[1100];
    char line[1024];
    FILE* in;
    FILE* out;
    int managed = 0;
    int copied = 0;

    if (vita_default_block_current(path))
        return 0;
    in = fopen(path, "rb");
    if (snprintf(temp, sizeof temp, "%s.tmp", path) >= (int)sizeof temp)
        return -1;
    remove(temp);
    out = fopen(temp, "wb");
    if (out == NULL)
    {
        if (in != NULL)
            fclose(in);
        return -1;
    }

    // Preserve every user-owned line and drop the old managed block. Lines
    // after a previous block are moved before the regenerated defaults, so
    // they remain effective overrides with the loader's first-value-wins rule.
    if (in != NULL)
    {
        while (fgets(line, (int)sizeof line, in) != NULL)
        {
            if (line_is_marker(line, PORT_VITA_DEFAULTS_BEGIN))
            {
                managed = 1;
                continue;
            }
            if (managed)
            {
                if (line_is_marker(line, PORT_VITA_DEFAULTS_END))
                    managed = 0;
                continue;
            }
            if (fputs(line, out) == EOF)
            {
                fclose(in);
                fclose(out);
                remove(temp);
                return -1;
            }
            copied = 1;
        }
        fclose(in);
    }

    if (copied && fputc('\n', out) == EOF)
    {
        fclose(out);
        remove(temp);
        return -1;
    }
    {
        const int writeFailed = write_vita_default_block(out) != 0;
        const int closeFailed = fclose(out) != 0;
        if (writeFailed || closeFailed)
        {
            remove(temp);
            return -1;
        }
    }
    if (rename(temp, path) != 0)
    {
        remove(temp);
        return -1;
    }
    return 0;
}
#endif

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
    if (!PortDiagnosticsEnabled())
        return;
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
        if (port_executable_dir(dir, sizeof dir) == 0)
            snprintf(path, sizeof path, "%s/strikers-log.txt", dir);
        else
            snprintf(path, sizeof path, "strikers-log.txt");
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
#if defined(PORT_VITA)
        fclose(f);
        if (freopen(path, "w", stderr) == NULL)
            return;
#else
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
#endif
    }
    // Unbuffered, because the run this is most wanted for is the one that ends in a crash, and a
    // buffered last few hundred lines is exactly the part that would be missing.
    setvbuf(stderr, NULL, _IONBF, 0);
}

int PortConfigLoad(void)
{
    if (s_loaded)
        return s_applied;

    s_applied = load_config();
    configure_diagnostics();
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
#if defined(PORT_VITA)
        if (refresh_vita_default_config(s_path) == 0)
            return load_file(s_path);
        fprintf(stderr, "[port] could not refresh default Vita config at %s\n", s_path);
        return -1;
#else
        if (exists(s_path))
            return load_file(s_path);
#endif
    }

    snprintf(s_path, sizeof s_path, "%s", PORT_CONFIG_NAME);
    if (exists(s_path))
        return load_file(s_path);

    s_path[0] = '\0';
    return 0;
}
