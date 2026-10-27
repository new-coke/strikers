#include "port/config.h"
#include "port/host.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "vita_config_test_io.h"
static char directory[1024];
int port_executable_dir(char* out, size_t n)
{
    if (strlen(directory) >= n)
        return -1;
    strcpy(out, directory);
    return 0;
}
int port_setenv_default(const char* k, const char* v) { return getenv(k) ? 1 : setenv(k, v, 1); }
int main()
{
    strcpy(directory, "/tmp/strikers-vitagl-config-XXXXXX");
    assert(mkdtemp(directory));
    assert(chdir(directory) == 0);
    char path[1200];
    snprintf(path, sizeof path, "%s/strikers.ini", directory);
    for (unsigned mode = 0; mode < 3; ++mode)
    {
        pid_t p = fork();
        assert(p >= 0);
        if (!p)
        {
            unsetenv("STRIKERS_CONFIG");
            if (mode == 1)
            {
                FILE* f = fopen(path, "w");
                assert(f);
                fputs(
                    "vita_test_match=1\nvita_playstation_glyphs=0\nframe_capture_frames=30\n# --- STRIKERS VALIDATED DEFAULTS "
                    "BEGIN ---\n# profile_revision = stale\ntask_profile=1\ngxm_disable=0x8\n# --- "
                    "STRIKERS VALIDATED DEFAULTS END ---\n",
                    f);
                assert(!fclose(f));
            }
            assert(PortConfigLoad() > 0);
            assert(!strcmp(getenv("STRIKERS_VITA_TEST_MATCH"), mode ? "1" : "0"));
            assert(!strcmp(getenv("STRIKERS_FRAME_CAPTURE_FRAMES"), mode ? "30" : "0"));
            assert(!strcmp(getenv("STRIKERS_BENCHMARK"), "0"));
            assert(!getenv("STRIKERS_NATIVE_ASSET_ARCHIVE"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_TIMING"), "0"));
            assert(!strcmp(getenv("STRIKERS_VITA_VIDEO_PLAYER"), "avplayer"));
            assert(!strcmp(getenv("STRIKERS_VITA_VIDEO_ARCHIVE"), ""));
            assert(!strcmp(getenv("STRIKERS_FRAME_CAPTURE_SCENE"), ""));
            assert(!strcmp(getenv("STRIKERS_VITA_PLAYSTATION_GLYPHS"), mode ? "0" : "1"));
            assert(!strcmp(getenv("STRIKERS_VITA_ASYNC_RENDERER"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITA_FRAMESKIP"), "0"));
            // The performance profile is on by default.
            assert(!strcmp(getenv("STRIKERS_VITAGL_BC1_DIRECT"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_RENDER_SCALE"), "72"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_TEXTURE_HANDLE_CACHE"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITA_ASYNC_MODELS"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITA_DOF"), "0"));
            assert(!strcmp(getenv("STRIKERS_VITA_VSYNC"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITA_CINEMATIC_30"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_DYNAMIC_SCALE"), "64"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_FOG_RANGE"), "0"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_DISPLAY_DEPTH_STORE"), "0"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_AFFINE_UV"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITAGL_VERTEX_FOG"), "1"));
            assert(!strcmp(getenv("STRIKERS_VITA_ASYNC_SEND"), "1"));
            assert(!strcmp(getenv("STRIKERS_GFX_RESOURCE_CACHED"), "1"));
            assert(!getenv("STRIKERS_GXM_DISABLE"));
            assert(!PortDiagnosticsEnabled() && PortFpsOverlayEnabled());
            FILE* f = fopen(path, "r");
            char text[4096];
            size_t n = fread(text, 1, sizeof text - 1, f);
            assert(!fclose(f));
            text[n] = 0;
            assert(strstr(text, "profile_revision = 2026-10-27-vita-performance-10"));
            assert(!strstr(text, "gxm_") && !strstr(text, "aurora"));
            assert(!strstr(text, "task_profile=1"));
            config_assert_closed();
            exit(0);
        }
        int status;
        assert(waitpid(p, &status, 0) == p && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    assert(!unlink(path));
    snprintf(path, sizeof path, "%s/strikers.ini.vita-defaults.bak", directory);
    assert(!unlink(path));
    assert(!chdir("/tmp"));
    assert(!rmdir(directory));
    puts("VitaGL config: profiling/capture off, INI-controlled test match, managed-block migration "
         "and repeated load passed");
}
