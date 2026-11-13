#include "port/texture_packs.h"

#if !defined(PORT_USE_AURORA)

extern "C" void PortTexturesInit(const char*) {}
extern "C" void PortTexturesReload(void) {}
extern "C" const char* PortTexturesFolder(int, int*) { return 0; }
extern "C" int PortTextureDumpEnabled(void) { return 0; }
extern "C" void PortTextureDumpEnable(int) {}
extern "C" const char* PortTextureDumpDir(void) { return ""; }
extern "C" unsigned PortTextureDumpWritten(void) { return 0; }
extern "C" void PortTextureDumpFrom(const char*) {}
extern "C" void PortTextureDumpExpect(const void*, const char*) {}
extern "C" void PortTextureDumpFromBuffer(const void*) {}
extern "C" void PortTextureDumpSkip(int) {}
extern "C" void PortTextureCreated(const GXTexObj*, const GXTlutObj*) {}

#else

#include "port/host.h"
#include "port/mods.h"
#include "port/region.h"

#include <aurora/replacement.h>
#include <SDL3/SDL_filesystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

namespace
{

struct Folder
{
    std::string path;
    int priority;
    int registrations;
};

std::string g_userPath;
std::vector<Folder> g_folders;
bool g_onePack;

bool g_dumping;
std::string g_dumpDir;
std::string g_dumpFrom;
int g_dumpSkip;
unsigned g_dumpWritten;

struct Expected
{
    const void* buffer;
    std::string name;
};
Expected g_expected[8];
unsigned g_expectedNext;

std::string TrimSeparators(std::string path)
{
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    return path;
}

bool IsDirectory(const std::string& path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

bool Off(const char* v)
{
    return strcmp(v, "0") == 0 || strcmp(v, "off") == 0 || strcmp(v, "no") == 0 ||
           strcmp(v, "false") == 0;
}

void AddFolder(const std::string& path, int priority, bool named)
{
    const std::string dir = TrimSeparators(path);
    for (size_t i = 0; i < g_folders.size(); i++)
    {
        if (g_folders[i].path == dir)
        {
            g_folders[i].priority = priority;
            return;
        }
    }
    if (!IsDirectory(dir))
    {
        if (named)
            fprintf(stderr, "[port] textures: %s is not a folder\n", dir.c_str());
        return;
    }
    Folder f;
    f.path = dir;
    f.priority = priority;
    f.registrations = 0;
    g_folders.push_back(f);
}

void FindFolders()
{
    g_folders.clear();
    const char* named = getenv("STRIKERS_TEXTURES");
    if (named != NULL && *named != '\0' && Off(named))
        return;

    std::vector<std::string> roots;
    for (int root = 0; root < PORT_MODS_ROOT_COUNT; root++)
    {
        char dir[1024];
        if (PortModsFolder(root, g_userPath.c_str(), "textures", dir, sizeof dir))
            roots.push_back(dir);
    }
    if (named != NULL && *named != '\0')
        roots.push_back(TrimSeparators(named));

    const char* pack = getenv("STRIKERS_TEXTURE_PACK");
    g_onePack = pack != NULL && *pack != '\0';
    for (size_t i = 0; i < roots.size(); i++)
    {
        const bool mustExist = !g_onePack && named != NULL && i + 1 == roots.size();
        AddFolder(g_onePack ? roots[i] + "/" + pack : roots[i], (int)i, mustExist);
    }
    if (g_onePack && g_folders.empty())
        fprintf(stderr, "[port] textures: no pack named %s in the textures folders\n", pack);
}

void LoadFolders()
{
    for (size_t i = 0; i < g_folders.size(); i++)
    {
        Folder& f = g_folders[i];
        f.registrations = (int)aurora_replacement_load(f.path.c_str(), f.priority, g_onePack ? 1 : 0);
        fprintf(stderr, "[port] textures: %d from %s\n", f.registrations, f.path.c_str());
    }
}

void SetDumpDir(const char* v)
{
    if (v != NULL && *v != '\0' && strcmp(v, "1") != 0)
        g_dumpDir = TrimSeparators(v);
    else
        g_dumpDir = TrimSeparators(g_userPath) + "/texture_dumps";
    g_dumpDir += "/";
    g_dumpDir += port_disc_game_code();
    g_dumpDir += port_disc_maker_code();
}

} // namespace

extern "C" void PortTexturesInit(const char* userPath)
{
    g_userPath = userPath != NULL ? userPath : "";

#if defined(__SWITCH__) || defined(__ANDROID__)
    // Handheld builds share RAM with the rest of the system.
    unsigned long cacheMb = 512;
#else
    unsigned long cacheMb = 4096;
#endif
    const char* mb = getenv("STRIKERS_TEXTURE_CACHE_MB");
    if (mb != NULL && *mb != '\0')
    {
        char* end = NULL;
        const unsigned long n = strtoul(mb, &end, 10);
        if (end != mb && *end == '\0' && n >= 64)
            cacheMb = n;
        else
            fprintf(stderr, "[port] textures: texture_cache_mb = %s is not a number of MB from 64 up; "
                            "using %lu\n", mb, cacheMb);
    }
    aurora_replacement_set_cache_budget((uint64_t)cacheMb << 20);
#if defined(__SWITCH__)
    // Its slow CPU would otherwise show the disc texture for seconds before the pack's.
    aurora_replacement_set_wait(1);
    // Its heap has no room to spare for a pack bigger than the budget, or for reloading what was evicted.
    aurora_replacement_set_strict_budget(1);
#endif

    FindFolders();
    LoadFolders();
    if (g_folders.empty())
        fprintf(stderr, "[port] textures: no pack folder; mods/textures/ beside the game or in %s\n",
                g_userPath.c_str());
    else
        fprintf(stderr, "[port] textures: up to %lu MB of them loaded at once\n", cacheMb);

    const char* dump = getenv("STRIKERS_TEXTURE_DUMP");
    if (dump != NULL && *dump != '\0' && !Off(dump))
        PortTextureDumpEnable(1);
}

extern "C" void PortTexturesReload(void)
{
    aurora_replacement_clear();
    FindFolders();
    LoadFolders();
}

extern "C" const char* PortTexturesFolder(int index, int* registrations)
{
    if (index < 0 || (size_t)index >= g_folders.size())
        return NULL;
    if (registrations != NULL)
        *registrations = g_folders[index].registrations;
    return g_folders[index].path.c_str();
}

extern "C" int PortTextureDumpEnabled(void) { return g_dumping ? 1 : 0; }

extern "C" void PortTextureDumpEnable(int on)
{
    g_dumping = on != 0;
    if (!g_dumping)
        return;
    if (g_dumpDir.empty())
    {
        const char* v = getenv("STRIKERS_TEXTURE_DUMP");
        SetDumpDir(v != NULL && !Off(v) ? v : NULL);
    }
    fprintf(stderr, "[port] textures: dumping to %s\n", g_dumpDir.c_str());
}

extern "C" const char* PortTextureDumpDir(void) { return g_dumpDir.c_str(); }

extern "C" unsigned PortTextureDumpWritten(void) { return g_dumpWritten; }

extern "C" void PortTextureDumpFrom(const char* name)
{
    if (name != NULL)
        g_dumpFrom = name;
    else
        g_dumpFrom.clear();
}

extern "C" void PortTextureDumpExpect(const void* buffer, const char* name)
{
    if (buffer == NULL || name == NULL)
        return;
    Expected& e = g_expected[g_expectedNext++ % (sizeof g_expected / sizeof g_expected[0])];
    e.buffer = buffer;
    e.name = name;
}

extern "C" void PortTextureDumpFromBuffer(const void* buffer)
{
    for (size_t i = 0; i < sizeof g_expected / sizeof g_expected[0]; i++)
    {
        if (buffer != NULL && g_expected[i].buffer == buffer)
        {
            PortTextureDumpFrom(g_expected[i].name.c_str());
            return;
        }
    }
    PortTextureDumpFrom(NULL);
}

extern "C" void PortTextureDumpSkip(int skip) { g_dumpSkip += skip ? 1 : -1; }

extern "C" void PortTextureCreated(const GXTexObj* obj, const GXTlutObj* tlut)
{
#if defined(__SWITCH__)
    if (!g_folders.empty())
        aurora_replacement_prefetch(obj, tlut);
#endif
    if (!g_dumping || g_dumpSkip > 0)
        return;
    // Menu resources, fonts and loading screens arrive by hash, with no file name to sort them by.
    const std::string dir = g_dumpDir + "/" + (g_dumpFrom.empty() ? std::string("other") : g_dumpFrom);
    if (aurora_replacement_dump(obj, tlut, dir.c_str()) > 0)
        g_dumpWritten++;
}

#endif // PORT_USE_AURORA
