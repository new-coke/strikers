// See include/port/launch.h for why these three settings live here rather than in main.cpp.

#include "port/launch.h"

#if defined(PORT_USE_AURORA)

#include "port/texfilter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>

namespace
{

// A boolean the way a person writes one. `fullscreen = yes` and `fullscreen = 1` are the same
// statement, and refusing the first is the kind of pedantry that makes a config file worse than a
// wrapper script.
int EnvBool(const char* name, int fallback)
{
    const char* v = getenv(name);
    if (v == NULL || *v == '\0')
        return fallback;
    if (strcmp(v, "0") == 0 || strcmp(v, "false") == 0 || strcmp(v, "no") == 0 ||
        strcmp(v, "off") == 0 || strcmp(v, "FALSE") == 0)
        return 0;
    return 1;
}

// The window icon: the game's own memory card icon assets/icon/MC_Icon.tpl is the 32x32 image the
// game writes into its save file so the GameCube's memory card screen has something to show: Mario
// with the ball.

#include "mc_icon.h"

constexpr int kIconScale = 2;
constexpr int kIconSize = MC_ICON_WIDTH * kIconScale;

unsigned char g_icon[kIconSize * kIconSize * 4];

void BuildIcon()
{
    for (int sy = 0; sy < MC_ICON_HEIGHT; sy++)
        for (int sx = 0; sx < MC_ICON_WIDTH; sx++)
        {
            const unsigned char* px = &kMcIconRGBA8[(sy * MC_ICON_WIDTH + sx) * 4];
            for (int dy = 0; dy < kIconScale; dy++)
                for (int dx = 0; dx < kIconScale; dx++)
                    memcpy(&g_icon[((sy * kIconScale + dy) * kIconSize + sx * kIconScale + dx) * 4], px, 4);
        }
}

// Aurora copies the AuroraConfig struct but not what its pointers point at (aurora.cpp keeps
// `g_config = *config`), and the icon is read later, when the window is created.
char g_cacheDir[1024];
char g_userDir[1024];

} // namespace

extern "C" void PortAuroraConfigure(AuroraConfig* cfg)
{
    if (cfg == NULL)
        return;

    // What the operating system calls this program.
    SDL_SetAppMetadata(cfg->appName != NULL ? cfg->appName : "Super Mario Strikers",
                       NULL, "org.smstrikers.port");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

    // Fullscreen at startup. F11 and the debug menu's System tab already toggle it at runtime
    // through SDL, but a player who wants fullscreen wants it before the game has drawn anything,
    // and AuroraConfig is the only place that can be asked for.
    cfg->startFullscreen = EnvBool("STRIKERS_FULLSCREEN", 0) != 0;

    // Let Aurora pick a software rasteriser when that is all there is.
    cfg->allowCpuAdapter = EnvBool("STRIKERS_ALLOW_CPU_ADAPTER", 0) != 0;

    // Default 0, and that is a decision rather than an oversight.
    cfg->pauseOnFocusLost = EnvBool("STRIKERS_PAUSE_ON_FOCUS_LOST", 0) != 0;

    // Anisotropic filtering. This is the *ceiling*, and it is the half of the control the GX enum
    // cannot express: glxSend picks GX_ANISO_4, which Aurora's wgpu_aniso() resolves to exactly
    // this number (GX_ANISO_2 would resolve to half of it).
    cfg->maxTextureAnisotropy = (uint16_t)PortTextureAniso();

    // STRIKERS_USER_DIR: the memory card somewhere other than the player's. Aurora derives the card
    // directory from userPath alone and formats an empty card where it finds none.
    {
        const char* dir = getenv("STRIKERS_USER_DIR");
        if (dir != NULL && *dir != '\0')
        {
            // The trailing separator is what SDL_GetPrefPath returns; imgui.cpp concatenates
            // "/imgui.ini" onto it by hand.
            const size_t n = strlen(dir);
            const int hasSep = n != 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\');
            snprintf(g_userDir, sizeof g_userDir, "%s%s", dir, hasSep ? "" : "/");
            // SDL_GetPrefPath creates what it returns and a path handed in here does not exist yet;
            // without this the card is formatted into a directory that is not there.
            SDL_CreateDirectory(g_userDir);
            cfg->userPath = g_userDir;
            fprintf(stderr, "[port] user dir: %s\n", g_userDir);
        }
    }

    // Aurora otherwise puts its pipeline and shader caches under SDL_GetPrefPath (on macOS,
    // ~/Library/Application Support/Super Mario Strikers).
    {
        const char* dir = getenv("STRIKERS_CACHE_DIR");
        if (dir != NULL && *dir != '\0')
        {
            snprintf(g_cacheDir, sizeof g_cacheDir, "%s", dir);
            // SDL_GetPrefPath creates what it returns; a path handed in here does not exist yet,
            // and sqlite3_open into a missing directory leaves Aurora running with no cache at all.
            SDL_CreateDirectory(g_cacheDir);
            cfg->cachePath = g_cacheDir;
        }
    }

    BuildIcon();
    cfg->iconRGBA8 = g_icon;
    cfg->iconWidth = kIconSize;
    cfg->iconHeight = kIconSize;

    // STRIKERS_DUMP_ICON=<path.ppm>: what the icon actually looks like.
    {
        const char* dump = getenv("STRIKERS_DUMP_ICON");
        if (dump != NULL && *dump != '\0')
        {
            FILE* f = fopen(dump, "wb");
            if (f != NULL)
            {
                fprintf(f, "P6\n%d %d\n255\n", kIconSize, kIconSize);
                for (int i = 0; i < kIconSize * kIconSize; i++)
                {
                    const unsigned char* p = &g_icon[i * 4];
                    for (int c = 0; c < 3; c++)
                    {
                        const int v = (p[c] * p[3] + 48 * (255 - p[3])) / 255;
                        fputc(v, f);
                    }
                }
                fclose(f);
                fprintf(stderr, "[port] icon -> %s\n", dump);
            }
        }
    }

    fprintf(stderr,
            "[port] window: fullscreen=%d pause_on_focus_lost=%d aniso=%ux "
            "icon=MC_Icon %dx%d",
            cfg->startFullscreen ? 1 : 0, cfg->pauseOnFocusLost ? 1 : 0,
            (unsigned)cfg->maxTextureAnisotropy, kIconSize, kIconSize);
    if (cfg->cachePath != NULL)
        fprintf(stderr, " cache=%s", cfg->cachePath);
    fprintf(stderr, "\n");
}

#endif // PORT_USE_AURORA
