#include "port/config.h"
#include "port/host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define DEFAULTS_BEGIN "# --- STRIKERS VALIDATED DEFAULTS BEGIN ---"
#define DEFAULTS_END "# --- STRIKERS VALIDATED DEFAULTS END ---"
#define DEFAULT_CONFIG_KEY_COUNT 67

static char s_data_dir[1024];
static const char* s_default_template;

int port_executable_dir(char* out, size_t size)
{
    const size_t n = strlen(s_data_dir) + 1;
    if (n > size)
        return -1;
    memcpy(out, s_data_dir, n);
    return 0;
}

int port_setenv_default(const char* name, const char* value)
{
    return getenv(name) ? 1 : setenv(name, value, 1);
}

static void write_ini(const char* path, const char* profile, const char* disable)
{
    FILE* f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "gxm_shader_profile = %s\n", profile);
    fprintf(f, "gxm_disable = %s\n", disable);
    fprintf(f, "vita_test_match = 0\n");
    fprintf(f, "vita_debug_menu = 0\n");
    fprintf(f, "gxm_native_gpu_static = 0\n");
    fprintf(f, "native_asset_archive = ux0:data/strikersVita/custom-native.psarc\n");
    fprintf(f, "gxm_native_model_draw = 2\n");
    fprintf(f, "gxm_native_model_cache = 1\n");
    fprintf(f, "gxm_native_model_uniform_build = 1\n");
    fprintf(f, "gxm_native_model_prepared = 1\n");
    fprintf(f, "gxm_native_model_early = 1\n");
    fprintf(f, "gxm_raster_uniform_reuse = 1\n");
    fprintf(f, "gxm_raster_uniform_reuse = 0\n");
    fprintf(f, "gxm_native_pn_palette = 1\n");
    fprintf(f, "gxm_native_pn_palette = 0\n");
    fprintf(f, "gxm_internal_resolution = 640x480\n");
    fprintf(f, "gxm_internal_resolution = 960x544\n");
    fprintf(f, "diagnostics = 0\n");
    fprintf(f, "fps_overlay = 0\n");
    CHECK(fclose(f) == 0);
}

static int file_contains(const char* path, const char* needle)
{
    FILE* f = fopen(path, "rb");
    char buf[8192];
    size_t n;
    CHECK(f != NULL);
    n = fread(buf, 1, sizeof buf - 1, f);
    CHECK(ferror(f) == 0);
    CHECK(fclose(f) == 0);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

static unsigned int file_count(const char* path, const char* needle)
{
    FILE* f = fopen(path, "rb");
    char buf[8192];
    const char* p;
    unsigned int count = 0;
    size_t n;
    CHECK(f != NULL);
    n = fread(buf, 1, sizeof buf - 1, f);
    CHECK(ferror(f) == 0);
    CHECK(fclose(f) == 0);
    buf[n] = '\0';
    p = buf;
    while ((p = strstr(p, needle)) != NULL)
    {
        count++;
        p += strlen(needle);
    }
    return count;
}

static void check_default_template(const char* path)
{
    FILE* actual = fopen(path, "rb");
    FILE* expected = fopen(s_default_template, "rb");
    int a, e;
    CHECK(actual != NULL && expected != NULL);
    do
    {
        a = fgetc(actual);
        e = fgetc(expected);
        CHECK(a == e);
    } while (a != EOF);
    CHECK(!ferror(actual) && !ferror(expected));
    CHECK(fclose(actual) == 0 && fclose(expected) == 0);
}

static void write_stale_managed_ini(const char* path)
{
    FILE* f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "# preserved user override\n");
    fprintf(f, "gxm_shader_profile = CONTROL\n");
    fprintf(f, "%s\n", DEFAULTS_BEGIN);
    fprintf(f, "# profile_revision = stale\n");
    fprintf(f, "cpu_mhz = 111\n");
    fprintf(f, "gxm_disable = 0x9999\n");
    fprintf(f, "%s\n", DEFAULTS_END);
    fprintf(f, "gpu_mhz = 333\n");
    CHECK(fclose(f) == 0);
}

static void run_case(const char* root, int mode)
{
    const pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0)
    {
        char user_ini[1200];
        CHECK(chdir(root) == 0);
        unsetenv("STRIKERS_CONFIG");
        unsetenv("STRIKERS_GXM_SHADER_PROFILE");
        unsetenv("STRIKERS_GXM_DISABLE");
        unsetenv("STRIKERS_DIAGNOSTICS");
        unsetenv("STRIKERS_FPS_OVERLAY");
        unsetenv("STRIKERS_GXM_INTERNAL_RESOLUTION");

        snprintf(user_ini, sizeof user_ini, "%s/strikers.ini", s_data_dir);
        if (mode == 1)
            write_ini(user_ini, "CONTROL", "0x1234");
        else if (mode == 2)
            write_stale_managed_ini(user_ini);

        const int applied = PortConfigLoad();
        CHECK(strcmp(getenv("STRIKERS_GXM_SHADER_PROFILE"), mode == 0 ? "WARM" : "CONTROL") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_DISABLE"), mode == 1 ? "0x1234" : "0x8") == 0);
        CHECK(PortDiagnosticsEnabled() == 0);
        CHECK(PortFpsOverlayEnabled() == 0);
        if (mode == 1)
        {
            CHECK(applied == DEFAULT_CONFIG_KEY_COUNT);
            CHECK(strcmp(PortConfigPath(), user_ini) == 0);
            CHECK(file_contains(user_ini, "gxm_shader_profile = CONTROL"));
            CHECK(file_contains(user_ini, "gxm_disable = 0x1234"));
            CHECK(file_contains(user_ini, "gxm_disable = 0x8"));
        }
        else if (mode == 2)
        {
            CHECK(applied == DEFAULT_CONFIG_KEY_COUNT);
            CHECK(strcmp(PortConfigPath(), user_ini) == 0);
            CHECK(strcmp(getenv("STRIKERS_CPU_MHZ"), "500") == 0);
            CHECK(strcmp(getenv("STRIKERS_GPU_MHZ"), "333") == 0);
            CHECK(file_contains(user_ini, "gxm_shader_profile = CONTROL"));
            CHECK(file_contains(user_ini, "gpu_mhz = 333"));
            CHECK(file_contains(user_ini, "cpu_mhz = 500"));
            CHECK(!file_contains(user_ini, "cpu_mhz = 111"));
            CHECK(!file_contains(user_ini, "gxm_disable = 0x9999"));
            CHECK(!file_contains(user_ini, "profile_revision = stale"));
        }
        else
        {
            struct stat info;
            CHECK(applied == DEFAULT_CONFIG_KEY_COUNT);
            CHECK(strcmp(PortConfigPath(), user_ini) == 0);
            CHECK(stat(user_ini, &info) == 0 && info.st_size > 0);
            check_default_template(user_ini);
            CHECK(strcmp(getenv("STRIKERS_CPU_MHZ"), "500") == 0);
            CHECK(strcmp(getenv("STRIKERS_GPU_MHZ"), "222") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_FRAGMENT_PREPARE_CACHE"), "1") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_UNIFORM_DELTA_UPLOAD"), "0") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_A4_FRAGMENT_OPT"), "0") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_XF_EQUAL_MATRIX_WRITES"), "0") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_TEV_DECODED_WRITE_GATE"), "0") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_PREPARED_DL"), "0") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_GPU_STATIC"), "1") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_DRAW_REPLAY"), "0") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_FIXED_UNIFORM_POOL"), "1") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_GEOMETRY_PREFLIGHT"), "1") == 0);
            CHECK(strcmp(getenv("STRIKERS_GXM_LOCAL_DRAW_BATCHING"), "0") == 0);
            CHECK(strcmp(getenv("STRIKERS_VITA_CORE3_MAX_TOTAL_PCT"), "70") == 0);
        }
        CHECK(strcmp(getenv("STRIKERS_VITA_FRAMESKIP"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_DEBUG_MENU"), mode == 1 ? "0" : "1") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_TEST_MATCH"), mode == 1 ? "0" : "1") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_VIEW_DRAW_CAPTURE"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_VIEW_DRAW_PAYLOADS"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_XF_EQUAL_POS_WRITES"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_RASTER_UNIFORM_REUSE"), mode == 1 ? "1" : "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_PN_PALETTE"), mode == 1 ? "1" : "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_UNIFORM_DELTA_UPLOAD"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_A4_FRAGMENT_OPT"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_XF_EQUAL_MATRIX_WRITES"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_TEV_DECODED_WRITE_GATE"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_MODEL_CACHE"), mode == 1 ? "1" : "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_MODEL_UNIFORM_BUILD"), mode == 1 ? "1" : "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_MODEL_PREPARED"), mode == 1 ? "1" : "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_MODEL_EARLY"), mode == 1 ? "1" : "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_INTERNAL_RESOLUTION"), mode == 1 ? "640x480" : "auto") == 0);
        {
            unsigned int width = 123, height = 456;
            CHECK(PortConfigVitaRenderSize(&width, &height) == (mode == 1 ? 1 : 0));
            CHECK(width == (mode == 1 ? 640u : 123u));
            CHECK(height == (mode == 1 ? 480u : 456u));
            CHECK(setenv("STRIKERS_GXM_INTERNAL_RESOLUTION", "960x544", 1) == 0);
            CHECK(PortConfigVitaRenderSize(&width, &height) == 1 && width == 960 && height == 544);
            CHECK(setenv("STRIKERS_GXM_INTERNAL_RESOLUTION", "640x481", 1) == 0);
            CHECK(PortConfigVitaRenderSize(&width, &height) == -1 && width == 960 && height == 544);
            CHECK(PortConfigVitaRenderSize(NULL, &height) == -1);
        }
        CHECK(strcmp(getenv("STRIKERS_GXM_PREPARED_DL"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_GPU_STATIC"), mode == 1 ? "0" : "1") == 0);
        CHECK(strcmp(getenv("STRIKERS_NATIVE_ASSET_ARCHIVE"), mode == 1
            ? "ux0:data/strikersVita/custom-native.psarc"
            : "ux0:data/strikersVita/sms-native-gxm-static.psarc") == 0);
        CHECK(strcmp(getenv("STRIKERS_FRAME_CAPTURE"), "ux0:data/strikersVita/r3a_quiet_gx.csv") == 0);
        CHECK(strcmp(getenv("STRIKERS_FRAME_CAPTURE_MATCH_ONLY"), "1") == 0);
        CHECK(strcmp(getenv("STRIKERS_FRAME_CAPTURE_PLAY_ONLY"), "1") == 0);
        CHECK(strcmp(getenv("STRIKERS_FRAME_CAPTURE_SKIP"), "600") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_VIEW_DRAW_SKIP"), "600") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_VIEW_DRAW_FRAMES"), "12") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_VIEW_DRAW_CAPACITY"), "8192") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_DRAW_REPLAY"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_MODEL_DRAW"), mode == 1 ? "2" : "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_MODEL_CENSUS"), "0") == 0);
        if (mode == 1)
            CHECK(file_contains(user_ini, "vita_debug_menu = 0"));
        CHECK(strcmp(getenv("STRIKERS_FIXED_DT"), "0") == 0);
        CHECK(strcmp(getenv("STRIKERS_GXM_NATIVE_ASSETS"), "1") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_NATIVE_AUDIO"), "1") == 0);
        CHECK(strcmp(getenv("STRIKERS_VITA_NATIVE_VIDEO"), "1") == 0);
        // diagnostics=0 deliberately removes the profiling env switch after loading.
        CHECK(getenv("STRIKERS_VITA_PACKET_PROFILE_PERIOD") == NULL);
        CHECK(file_contains(user_ini, "vita_packet_profile_period = 0"));
        CHECK(strcmp(getenv("STRIKERS_ASSET_ARCHIVE"), "ux0:data/strikersVita/sms.psarc") == 0);
        CHECK(file_count(user_ini, DEFAULTS_BEGIN) == 1);
        CHECK(file_count(user_ini, DEFAULTS_END) == 1);
        CHECK(file_contains(user_ini, "profile_revision = 2026-10-11-current-gxm-baseline"));
        exit(0);
    }

    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    s_default_template = argv[1];
    char root[] = "/tmp/strikers-vita-config-test-XXXXXX";
    char data[1200];
    CHECK(mkdtemp(root) != NULL);
    snprintf(data, sizeof data, "%s/data", root);
    CHECK(mkdir(data, 0700) == 0);
    snprintf(s_data_dir, sizeof s_data_dir, "%s", data);

    CHECK(chdir(root) == 0);

    run_case(root, 0);
    {
        char generated[1200];
        struct stat before, after;
        snprintf(generated, sizeof generated, "%s/strikers.ini", data);
        CHECK(stat(generated, &before) == 0);
        run_case(root, 0);
        CHECK(stat(generated, &after) == 0);
        // A fresh process loading the same profile must keep the existing
        // file, instead of creating a temporary file and replacing the INI.
        CHECK(before.st_ino == after.st_ino && before.st_size == after.st_size);
        CHECK(unlink(generated) == 0);
    }
    run_case(root, 1);
    {
        char generated[1200];
        snprintf(generated, sizeof generated, "%s/strikers.ini", data);
        CHECK(unlink(generated) == 0);
    }
    run_case(root, 2);

    char user_ini[1200];
    snprintf(user_ini, sizeof user_ini, "%s/strikers.ini", data);
    unlink(user_ini);
    CHECK(rmdir(data) == 0);
    CHECK(chdir("/") == 0);
    CHECK(rmdir(root) == 0);
    puts("vita default config: device baseline template, first-run generation, no redundant rewrite, managed refresh and user overrides");
    return 0;
}
