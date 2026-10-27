// DVD backed by an extracted disc filesystem, or by a disc image.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "dolphin/types.h"
#include "port/config.h"
#include "port/disc.h"
#include "port/asset_archive.h"
#include "port/fatal.h"
#include "port/host.h"
#include "port/disc_reader.h"
#include "port/region.h"

void OSReport(const char* msg, ...);

// Defined below; DVDInit uses it to check that the directory it found is this game's disc rather
// than some other directory that happens to have files in it.
s32 DVDConvertPathToEntrynum(const char* pathPtr);

// Defined near DVDGetCurrentDiskID, below.
static void load_disk_id(void);
static const u8* disk_id_bytes(void);

// MSVC's <sys/stat.h> has the S_IFDIR bit but not the POSIX test macro.
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

// The 32-byte disc header: game code (4), maker (2), disk/version, flags, and the game name.
static u8 s_disk_id[32];
// Whether that read succeeded, so the fallback below is a decision and not an accident.
static int s_disk_id_read;
// Whether the id was chosen rather than read, for the DVD: disc line.
static int s_disk_id_guessed;

typedef struct DVDCommandBlock DVDCommandBlock;
typedef void (*DVDCBCallback)(s32 result, DVDCommandBlock* block);

struct DVDCommandBlock
{
    DVDCommandBlock* next;
    DVDCommandBlock* prev;
    u32 command;
    s32 state;
    u32 offset;
    u32 length;
    void* addr;
    u32 currTransferSize;
    u32 transferredSize;
    void* id;
    DVDCBCallback callback;
    void* userData;
};

typedef struct DVDFileInfo DVDFileInfo;
typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

struct DVDFileInfo
{
    DVDCommandBlock cb;
    u32 startAddr;
    u32 length;
    DVDCallback callback;
};

#define DVD_STATE_END 0
#define DVD_STATE_BUSY 1
#define DVD_STATE_FATAL_ERROR (-1)

typedef struct
{
    char* path;   // path as the game spells it, lowercased, '/'-separated
    char* host;   // full host path, or NULL when the bytes are in an image
    u32 offset;   // image byte offset or PSARC entry index; unused for a host file
    u32 length;
} DvdEntry;

static DvdEntry* s_entries;
static int s_count;
static int s_cap;
static char s_root[1024];
// Non-NULL when the data is a disc image rather than a directory.
static PortDisc* s_disc;
static PortAssetArchive* s_archive;

static int is_fen_mapping_probe(const char* path)
{
    return path != NULL
        && (strcmpi(path, "art/fe/popup_menu.fen") == 0
            || strcmpi(path, "art/fe/saving_loading.fen") == 0);
}

static u32 fen_mapping_fingerprint(const DvdEntry* entry)
{
    // FNV-1a over the complete file.  This is diagnostic-only and runs for two
    // small FEN files during DVDInit, so favour a deterministic identity over a
    // sample that could accidentally match the common package header.
    unsigned char buffer[4096];
    u32 hash = 2166136261u;
    u32 pos = 0;

    while (pos < entry->length)
    {
        size_t want = entry->length - pos;
        if (want > sizeof buffer)
            want = sizeof buffer;

        long got;
        if (entry->host != NULL)
        {
            FILE* f = fopen(entry->host, "rb");
            if (f == NULL)
                return 0;
            if (fseek(f, (long)pos, SEEK_SET) != 0)
            {
                fclose(f);
                return 0;
            }
            got = (long)fread(buffer, 1, want, f);
            fclose(f);
        }
        else if (s_archive != NULL)
        {
            got = port_asset_archive_read(s_archive, entry->offset, buffer, want, pos);
        }
        else
        {
            got = port_disc_read(s_disc, buffer, want,
                                 (unsigned long long)entry->offset + pos);
        }

        if (got != (long)want)
            return 0;
        for (size_t i = 0; i < want; ++i)
        {
            hash ^= buffer[i];
            hash *= 16777619u;
        }
        pos += (u32)want;
    }
    return hash;
}

static void log_fen_mapping_probes(void)
{
    static const char* const kPaths[] = {
        "art/fe/popup_menu.fen",
        "art/fe/saving_loading.fen",
    };

    for (unsigned p = 0; p < sizeof kPaths / sizeof kPaths[0]; ++p)
    {
        int found = -1;
        for (int i = 0; i < s_count; ++i)
        {
            if (strcmpi(s_entries[i].path, kPaths[p]) == 0)
            {
                found = i;
                break;
            }
        }

        if (found < 0)
        {
            OSReport("[dvd-fen] missing %s\n", kPaths[p]);
            continue;
        }

        const DvdEntry* e = &s_entries[found];
        OSReport("[dvd-fen] map path=%s idx=%d off=%#x len=%u fnv=%08x source=%s\n",
                 e->path, found, (unsigned)e->offset, (unsigned)e->length,
                 (unsigned)fen_mapping_fingerprint(e),
                 e->host != NULL ? "host" : s_archive != NULL ? "PSARC" : "image");
    }
}

static char* dup_lower(const char* s)
{
    size_t n = strlen(s);
    char* out = (char*)malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        out[i] = (char)((s[i] >= 'A' && s[i] <= 'Z') ? s[i] - 'A' + 'a' : s[i]);
    out[n] = '\0';
    return out;
}

static void add_entry(const char* rel, const char* host, u32 offset, u32 length)
{
    if (s_count == s_cap)
    {
        s_cap = s_cap ? s_cap * 2 : 256;
        s_entries = (DvdEntry*)realloc(s_entries, (size_t)s_cap * sizeof(DvdEntry));
    }
    s_entries[s_count].path = dup_lower(rel);
    s_entries[s_count].host = host != NULL ? strdup(host) : NULL;
    s_entries[s_count].offset = offset;
    s_entries[s_count].length = length;
    s_count++;
}

// Directory iteration goes through port_scan_dir: <dirent.h> is POSIX and Windows has none.
static void scan(const char* hostDir, const char* relDir);

struct ScanCtx
{
    const char* hostDir;
    const char* relDir;
};

static void visit_entry(void* user, const char* name)
{
    const struct ScanCtx* ctx = (const struct ScanCtx*)user;
    char host[1024], rel[1024];
    struct stat st;

    // Skip every dotfile: an extracted disc copied about on macOS collects .DS_Store, and the
    // console's filesystem had no such thing, so anything starting with a dot is not disc content.
    if (name[0] == '.')
        return;

    snprintf(host, sizeof host, "%s/%s", ctx->hostDir, name);
    snprintf(rel, sizeof rel, "%s%s%s", ctx->relDir, *ctx->relDir ? "/" : "", name);
    if (stat(host, &st) != 0)
        return;
    if (S_ISDIR(st.st_mode))
        scan(host, rel);
    else
        add_entry(rel, host, 0, (u32)st.st_size);
}

static void scan(const char* hostDir, const char* relDir)
{
    struct ScanCtx ctx;
    ctx.hostDir = hostDir;
    ctx.relDir = relDir;
    port_scan_dir(hostDir, visit_entry, &ctx);
}

// The image path: the same table, built from the disc's own FST.

static void image_fatal(const char* path, const char* text)
{
    char msg[4096];
    snprintf(msg, sizeof msg, "%s\n\n  Image: %s\n", text, path);
    port_fatal("Super Mario Strikers: unusable disc image", msg);
}

static int index_visit(void* user, const char* path, unsigned offset, unsigned length,
                       int isDir)
{
    (void)user;
    if (!isDir)
        add_entry(path, NULL, offset, length);
    return 0;
}

static void index_image_fst(const char* path)
{
    char err[2048];

    // The disc id, from the image itself. An extracted folder reads these same 32 bytes out of
    // sys/boot.bin, which is a copy of them, so region handling is identical either way
    // (include/port/region.h).
    if (port_disc_read(s_disc, s_disk_id, sizeof s_disk_id, 0) != (long)sizeof s_disk_id)
        image_fatal(path, "That image is too short to hold a disc header.");
    s_disk_id_read = 1;

    if (port_disc_walk(s_disc, index_visit, NULL, err, sizeof err) != 0)
        image_fatal(path, err);

    // A plain ISO can still have a perfectly readable header/FST while the
    // actual file has been truncated or "shrunk" after the table was written.
    // In that case every entry name/size looks valid, but higher-offset files
    // silently read as EOF. Validate the furthest byte referenced by the FST
    // once, here, so the failure is attributed to the image rather than to an
    // unrelated parser much later in boot.
    if (s_count > 0)
    {
        unsigned long long maxEnd = 0;
        const DvdEntry* last = NULL;
        unsigned char byte;
        int i;

        for (i = 0; i < s_count; ++i)
        {
            const unsigned long long end = (unsigned long long)s_entries[i].offset
                                           + (unsigned long long)s_entries[i].length;
            if (end > maxEnd)
            {
                maxEnd = end;
                last = &s_entries[i];
            }
        }

        if (last != NULL && maxEnd != 0
            && port_disc_read(s_disc, &byte, 1, maxEnd - 1) != 1)
        {
            char msg[2048];
            snprintf(msg, sizeof msg,
                     "The disc image is incomplete or has been trimmed without rebuilding its "
                     "file table.\n\nThe FST says '%s' ends at disc offset 0x%llX, but that byte "
                     "cannot be read from the image.\n\nRe-copy a complete ISO/GCM, or use the "
                     "disc's extracted files folder instead. Do not continue with this image: "
                     "missing bytes become empty game assets and cause unrelated crashes later.",
                     last->path, maxEnd);
            image_fatal(path, msg);
        }
    }
}

// Open `path` as a disc image and index it.
static void open_image(const char* path)
{
    char err[2048];

    s_disc = port_disc_open(path, err, sizeof err);
    if (s_disc == NULL)
        image_fatal(path, err);

    snprintf(s_root, sizeof s_root, "%s", path);
    index_image_fst(path);

    // An image whose FST names no files would leave s_disc set and s_count 0, so DVDInit would scan
    // a directory into the same table, whose entries then carry host paths while reads take the
    // image branch.
    if (s_count == 0)
        image_fatal(path, "That image contains no files at all.");
}

// The archive keeps the extracted layout: files/... plus sys/boot.bin.
// Entry indices stay internal; DVDOpen and callbacks retain their SDK contract.
static void open_archive(const char* path)
{
    char err[2048];
    s_archive = port_asset_archive_open(path, err, sizeof err);
    if (s_archive == NULL) image_fatal(path, err);
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    {
        static const unsigned char expected[16] = {'S','N','P','A','C','K','0','1',1,0,0,0,1,0,0,0};
        unsigned char version[16];
        const int entry=port_asset_archive_find(s_archive,"sys/strikers-native.bin");
        if(entry<0 || port_asset_archive_size(s_archive,(unsigned)entry)!=sizeof version ||
           port_asset_archive_read(s_archive,(unsigned)entry,version,sizeof version,0)!=sizeof version ||
           memcmp(version,expected,sizeof version))
            image_fatal(path,"This Vita build requires the converted native PSARC. Regenerate sms.psarc with the Strikers native graphics compiler.");
    }
#endif
    int boot = port_asset_archive_find(s_archive, "sys/boot.bin");
    if (boot < 0 || port_asset_archive_read(s_archive, (unsigned)boot, s_disk_id,
                                          sizeof s_disk_id, 0) != sizeof s_disk_id)
        image_fatal(path, "The asset archive has no readable sys/boot.bin disc identity.");
    s_disk_id_read = 1;
    snprintf(s_root, sizeof s_root, "%s", path);
    for (unsigned i = 0; i < port_asset_archive_count(s_archive); ++i)
    {
        const char* name = port_asset_archive_path(s_archive, i);
        if (strncmp(name, "files/", 6) != 0) continue;
        uint64_t size = port_asset_archive_size(s_archive, i);
        if (size > UINT32_MAX) image_fatal(path, "An archive file exceeds the DVD 32-bit size limit.");
        add_entry(name + 6, NULL, i, (u32)size);
    }
    if (s_count == 0) image_fatal(path, "The asset archive contains no files/ game data.");
}

static void open_data_file(const char* path)
{
    unsigned char magic[4];
    FILE* f = fopen(path, "rb");
    int archive = f && fread(magic, 1, sizeof magic, f) == sizeof magic &&
                  memcmp(magic, "PSAR", 4) == 0;
    if (f) fclose(f);
    if (archive) open_archive(path);
    else open_image(path);
}

// A path names an image if it is a file rather than a directory.
static int is_regular_file(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0 && !S_ISDIR(st.st_mode);
}

// Pick a disc image out of the directory beside the executable, so dropping one next to the game is
// as good as unpacking a files/ folder there.
struct PickCtx
{
    const char* dir;
    char best[1024];
};

static void pick_image(void* user, const char* name)
{
    struct PickCtx* ctx = (struct PickCtx*)user;
    if (name[0] == '.' || !port_disc_looks_like_image(name))
        return;
    if (ctx->best[0] == '\0' || strcmpi(name, ctx->best) < 0)
        snprintf(ctx->best, sizeof ctx->best, "%s", name);
}

// One file that is on every copy of this disc and on nothing else a person is likely to point
// STRIKERS_DATA at.
#define DVD_SENTINEL "common.ini"

// The paths tried, in the order tried, so the message can name them.
#define DVD_MAX_TRIED 9

void DVDInit(void)
{
    char tried[DVD_MAX_TRIED][1024];
    const char* triedWhy[DVD_MAX_TRIED];
    int nTried = 0;

    if (s_count)
        return;

    // PORT: strikers.ini has to be in the environment before the first getenv here, and main() is
    // too late; this runs *before* main.
    PortConfigLoad();

    // Explicit opt-in takes precedence over the Vita's canonical sms.iso.
    const char* archive = getenv("STRIKERS_ASSET_ARCHIVE");
    if (archive != NULL && *archive != '\0')
    {
        snprintf(tried[nTried], sizeof tried[0], "%s", archive);
        triedWhy[nTried++] = "asset_archive in strikers.ini";
        open_archive(archive);
    }

#if defined(PORT_VITA)
    // Vita release layout: prefer the user's own GameCube image at the fixed
    // application-data path.  Keeping one canonical filename avoids scanning
    // ux0 and makes startup deterministic regardless of other disc images on
    // the memory card.
    {
        static const char vitaIso[] = "ux0:data/strikersVita/sms.iso";
        snprintf(tried[nTried], sizeof tried[0], "%s", vitaIso);
        triedWhy[nTried++] = "the Vita sms.iso game disc";
        if (s_count == 0 && is_regular_file(vitaIso))
            open_image(vitaIso);
    }

    // Development fallback: an extracted disc tree copied to
    // ux0:data/strikersVita/game/{sys,files}. Require common.ini before scanning
    // so a half-copied folder is ignored.
    if (s_count == 0)
    {
        char dir[1024];
        char sentinel[1200];
        if (port_executable_dir(dir, sizeof dir) == 0)
        {
            snprintf(s_root, sizeof s_root, "%s/game/files", dir);
            snprintf(sentinel, sizeof sentinel, "%s/%s", s_root, DVD_SENTINEL);
            snprintf(tried[nTried], sizeof tried[0], "%s", s_root);
            triedWhy[nTried++] = "the Vita extracted-disc folder";
            if (is_regular_file(sentinel))
                scan(s_root, "");
        }
    }
#endif

    const char* env = getenv("STRIKERS_DATA");
    const char* why = "STRIKERS_DATA / the 'data' key in strikers.ini";
    if (s_count == 0 && env != NULL && *env != '\0')
    {
        snprintf(s_root, sizeof s_root, "%s", env);
        snprintf(tried[nTried], sizeof tried[0], "%s", s_root);
        triedWhy[nTried++] = why;
        if (is_regular_file(s_root))
            open_data_file(s_root);   // never returns if it is not usable data
        else
            scan(s_root, "");
    }

    // Beside the executable, which is what an unpacked release archive looks like: strikers(.exe)
    // and a files/ directory, nothing to configure.
    if (s_count == 0)
    {
        char dir[1024];
        if (port_executable_dir(dir, sizeof dir) == 0)
        {
            snprintf(s_root, sizeof s_root, "%s/files", dir);
            snprintf(tried[nTried], sizeof tried[0], "%s", s_root);
            triedWhy[nTried++] = "beside the game";
            scan(s_root, "");
        }
    }

    // A disc image beside the game, which is the release archive again with the step of extracting
    // it left out.
    if (s_count == 0)
    {
        char dir[1024];
        struct PickCtx ctx;
        ctx.best[0] = '\0';
        if (port_executable_dir(dir, sizeof dir) == 0)
        {
            ctx.dir = dir;
            port_scan_dir(dir, pick_image, &ctx);
            if (ctx.best[0] != '\0')
            {
                snprintf(s_root, sizeof s_root, "%s/%s", dir, ctx.best);
                snprintf(tried[nTried], sizeof tried[0], "%s", s_root);
                triedWhy[nTried++] = "a disc image beside the game";
                open_image(s_root);
            }
        }
    }

    // Last: data/<disc id>/files under the port, so running the binary straight out of a build
    // directory keeps working.
    if (s_count == 0)
    {
        // Any of the three discs will run; this is only which one a developer working from a
        // checkout gets by default.
        snprintf(s_root, sizeof s_root, "%s",
                 "data/G4QE01/files");
        snprintf(tried[nTried], sizeof tried[0], "%s", s_root);
        triedWhy[nTried++] = "a source checkout";
        scan(s_root, "");
    }

    if (s_archive != NULL)
        fprintf(stderr, "[port] DVD: %d files in %s (PSARC 1.4)\n", s_count, s_root);
    else if (s_disc != NULL)
        fprintf(stderr, "[port] DVD: %d files in %s (%s image)\n", s_count,
                s_root, port_disc_format(s_disc));
    else
        fprintf(stderr, "[port] DVD: %d files under %s\n", s_count, s_root);

    // The boot save-flow opens popup_menu.fen immediately after
    // saving_loading.fen.  A hardware crash proved that the popup scene was
    // receiving the save/load graph, so print the authoritative index identity
    // before either scene can mutate runtime state.
    log_fen_mapping_probes();

    // PORT: fatal rather than zero files and a printed warning; launched from a file manager there
    // is no console to read.
    if (s_count == 0)
    {
        char msg[4096];
        int n = 0;
        int i;
        n += snprintf(msg + n, sizeof msg - (size_t)n,
                      "The game data was not found.\n\n"
                      "Super Mario Strikers needs its own GameCube disc: "
                      "either a disc image\n(.iso, .gcm, .ciso or .gcz) or the "
                      "`files` folder of an extracted one\n(1416 files, about "
                      "618 MB). No game data is shipped with this port; "
                      "supply\nyour own copy.\n\n"
                      "Looked in, in order:\n");
        for (i = 0; i < nTried && n < (int)sizeof msg; i++)
            n += snprintf(msg + n, sizeof msg - (size_t)n,
                          "  %d. %s\n     (%s)\n", i + 1, tried[i], triedWhy[i]);
        if (n < (int)sizeof msg)
            snprintf(msg + n, sizeof msg - (size_t)n,
                     "\nTo fix this, either:\n"
                     "  * choose it on the Game tab of the settings app beside "
                     "the game, or\n"
                     "  * put your disc image, or the extracted disc's `files` "
                     "folder, next to the\n    game, or\n"
                     "  * set the `data` key in strikers.ini (beside the game) "
                     "to either of those, or\n"
                     "  * set the STRIKERS_DATA environment variable to it.\n");
        port_fatal("Super Mario Strikers: game data not found", msg);
    }

    // Files, but not this game's files. A folder of holiday photos scans perfectly well and then
    // dies a long way from here, in the DVD layer or an allocator, on a header that was never a
    // header.
    if (DVDConvertPathToEntrynum(DVD_SENTINEL) < 0)
    {
        char msg[4096];
        snprintf(msg, sizeof msg,
                 "That %s is not the Super Mario Strikers disc.\n\n"
                 "Found %d files in:\n  %s\n\n"
                 "...but no `%s`, which is on every copy of this game.%s\n",
                 s_disc != NULL ? "disc image" : "folder",
                 s_count, s_root, DVD_SENTINEL,
                 s_disc != NULL
                     ? " It is a readable disc image\nof some other game."
                     : " The path is probably\npointing one level too high or "
                       "too low: it must be the `files` folder itself,\nthe one "
                       "that contains `common.ini`, `art` and `audio`.");
        port_fatal("Super Mario Strikers: wrong game data", msg);
    }

    load_disk_id();

    // Say which disc this is, always. It is one line, it is the first thing worth knowing about a
    // run that behaves oddly, and every region-specific decision in the game now follows from it
    // (include/port/region.h).
    {
        static const char* const kNames[3] = { "USA", "Europe", "Japan" };
        fprintf(stderr, "[port] DVD: disc %.6s (%s%s)\n",
                (const char*)disk_id_bytes(), kNames[port_region()],
                s_disk_id_guessed ? ", not read from the data" : "");
    }

    // This game's files, but not this game's disc.
    if (memcmp(disk_id_bytes(), "G4Q", 3) != 0)
    {
        char msg[4096];
        char found[7];
        memcpy(found, disk_id_bytes(), 6);
        found[6] = '\0';
        snprintf(msg, sizeof msg,
                 "The disc header %s is not Super Mario "
                 "Strikers.\n\n"
                 "  Game data:  %s\n"
                 "  It says the disc is: %s\n\n"
                 "Every release of this game has a code beginning G4Q "
                 "(G4QE01 USA, G4QP01\nEurope, G4QJ01 Japan) and this port "
                 "runs any of them. A `files` folder from\none game beside "
                 "another game's `sys` is the usual cause.\n",
                 s_disc != NULL ? "in this image" : "beside this data",
                 s_root, found);
        port_fatal("Super Mario Strikers: wrong disc", msg);
    }
}

// STRIKERS_PROBE_DVD: name every open and every read, with the bytes asked for against the bytes
// produced.
static int probe_dvd(void)
{
    static int s_on = -1;
    if (s_on < 0)
    {
        const char* e = getenv("STRIKERS_PROBE_DVD");
        s_on = (e != NULL && *e != '\0' && *e != '0') ? 1 : 0;
    }
    return s_on;
}

s32 DVDConvertPathToEntrynum(const char* pathPtr)
{
    if (!pathPtr)
        return -1;
    // The game spells paths with mixed case and a leading '/' in places; the index is lowercased,
    // so compare case-insensitively and skip any root.
    while (*pathPtr == '/')
        pathPtr++;
    s32 found = -1;
    for (int i = 0; i < s_count; i++)
        // strcmpi, not strcasecmp: prelude.h maps it to the host spelling. strcasecmp is POSIX and
        // absent on Windows, where the same function is _stricmp in <string.h>.
        if (strcmpi(s_entries[i].path, pathPtr) == 0)
        {
            found = i;
            break;
        }
    return found;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo)
{
    if (entrynum < 0 || entrynum >= s_count || !fileInfo)
        return FALSE;
    if (is_fen_mapping_probe(s_entries[entrynum].path))
    {
        OSReport("[dvd-fen] open path=%s idx=%d off=%#x len=%u\n",
                 s_entries[entrynum].path, (int)entrynum,
                 (unsigned)s_entries[entrynum].offset,
                 (unsigned)s_entries[entrynum].length);
    }
    // Under STRIKERS_LOG_AUDIO, name every stream file opened, whichever route resolved it.
    {
        static int s_log = -1;
        if (s_log < 0)
        {
            const char* e = getenv("STRIKERS_LOG_AUDIO");
            s_log = (e != NULL && *e != '\0') ? 1 : 0;
        }
        if (s_log)
        {
            const char* path = s_entries[entrynum].path;
            size_t n = strlen(path);
            if ((n > 5 && strcmpi(path + n - 5, ".idsp") == 0) || (n > 4 && strcmpi(path + n - 4, ".dsp") == 0))
            {
                /* port_monotonic_ns, not clock_gettime: CLOCK_MONOTONIC is POSIX and the UCRT
                   has neither. port/host.h exists for this family. */
                OSReport("[port] dvd: stream open %s @%lums\n", path,
                         (unsigned long)(port_monotonic_ns() / 1000000ull));
            }
        }
    }
    if (probe_dvd())
        OSReport("[port] dvd: open %s (%u bytes)\n", s_entries[entrynum].path,
                 (unsigned)s_entries[entrynum].length);
    memset(fileInfo, 0, sizeof *fileInfo);
    fileInfo->startAddr = (u32)entrynum;   // index, not a disc offset
    fileInfo->length = s_entries[entrynum].length;
    fileInfo->cb.state = DVD_STATE_END;
    return TRUE;
}

BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo)
{
    return DVDFastOpen(DVDConvertPathToEntrynum(fileName), fileInfo);
}


static s32 dvd_read_raw(const DvdEntry* e, void* addr, s32 length, s32 offset);
#if defined(PORT_VITA)
extern void strikers_native_prepare_memory_write(const void* address, size_t bytes);
extern void strikers_native_notify_memory_write(const void* address, size_t bytes);
#endif
/* Disc reads replace guest memory behind the CPU. Publish them like a
 * DCStore so Aurora's display-list shadows and stable-geometry revisions
 * never keep serving the previous contents of a reused buffer. */
#if defined(__SWITCH__) || defined(PORT_VITA)
#define DVD_READER_THREAD 1
#else
#define DVD_READER_THREAD 0
#endif
static s32 dvd_read(const DvdEntry* e, void* addr, s32 length, s32 offset)
{
#if defined(PORT_VITA)
    if (addr != NULL && length > 0)
        strikers_native_prepare_memory_write(addr, (size_t)length);
#endif
    const s32 got = dvd_read_raw(e, addr, length, offset);
#if defined(PORT_VITA)
    if (got > 0)
        strikers_native_notify_memory_write(addr, (size_t)got);
#endif
    return got;
}

#if DVD_READER_THREAD
// Pending reads share a fixed-size pool across open files.
#define DVD_PENDING_MAX 24
typedef struct DvdPending
{
    const DvdEntry* entry;
    DVDFileInfo* fileInfo;
    void* addr;
    s32 length;
    s32 offset;
    DVDCallback callback;
    int32_t inUse;
} DvdPending;

static DvdPending s_pending[DVD_PENDING_MAX];

// Reads report busy on their first poll, before GameCubeReadAsync advances the file position.
#define DVD_OWED_MAX 32
// Game thread only, so no lock.
static const DVDCommandBlock* s_owedBusy[DVD_OWED_MAX];

static void owe_busy_poll(const DVDCommandBlock* block)
{
    int i;
    for (i = 0; i < DVD_OWED_MAX; i++)
    {
        if (s_owedBusy[i] == block)
            return;
    }
    for (i = 0; i < DVD_OWED_MAX; i++)
    {
        if (s_owedBusy[i] == NULL)
        {
            s_owedBusy[i] = block;
            return;
        }
    }
    OSReport("[port] DVD: more than %d reads owed a busy poll; one completes a poll early\n",
             DVD_OWED_MAX);
}

static int take_busy_poll(const DVDCommandBlock* block)
{
    int i;
    for (i = 0; i < DVD_OWED_MAX; i++)
    {
        if (s_owedBusy[i] == block)
        {
            s_owedBusy[i] = NULL;
            return 1;
        }
    }
    return 0;
}

static s32 pending_read_now(const DvdEntry* e, void* addr, s32 length, s32 offset);
static void pending_run(void* ctx);

// Only the game thread claims slots and only the reader frees them.
static DvdPending* pending_acquire(void)
{
    int i;
    for (i = 0; i < DVD_PENDING_MAX; i++)
    {
        if (!port_load_acquire_i32(&s_pending[i].inUse))
        {
            s_pending[i].inUse = 1;
            return &s_pending[i];
        }
    }
    return NULL;
}
#endif

BOOL DVDClose(DVDFileInfo* fileInfo)
{
#if DVD_READER_THREAD
    int i;
    if (fileInfo == NULL)
        return TRUE;
    // Wait for this file's queued reads before the caller can free the command block and buffer.
    for (i = 0; i < DVD_PENDING_MAX; i++)
    {
        while (s_pending[i].fileInfo == fileInfo && port_load_acquire_i32(&s_pending[i].inUse))
            port_yield();
    }
    take_busy_poll(&fileInfo->cb);
    fileInfo->cb.state = DVD_STATE_END;
    return TRUE;
#else
    if (fileInfo)
        fileInfo->cb.state = DVD_STATE_END;
    return TRUE;
#endif
}

s32 DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                     DVDCallback callback, s32 prio)
{
    (void)prio;
    if (!fileInfo || fileInfo->startAddr >= (u32)s_count)
        return FALSE;
    const DvdEntry* e = &s_entries[fileInfo->startAddr];

    if (is_fen_mapping_probe(e->path))
    {
        if (s_archive != NULL)
            OSReport("[dvd-fen] read path=%s idx=%u file_off=%d len=%d archive_idx=%u\n",
                     e->path, (unsigned)fileInfo->startAddr, (int)offset, (int)length,
                     (unsigned)e->offset);
        else
            OSReport("[dvd-fen] read path=%s idx=%u file_off=%d len=%d iso_off=%#llx\n",
                     e->path, (unsigned)fileInfo->startAddr, (int)offset, (int)length,
                     (unsigned long long)e->offset + (offset >= 0 ? (unsigned)offset : 0));
    }

    // The SDK contract is boolean: TRUE means the command was accepted and its
    // completion state is authoritative. Never report END for a failed/short
    // host read, otherwise upper layers will advance their destination pointer
    // and consume uninitialised (often zero-filled) memory as valid disc data.
    if (addr == NULL || length <= 0 || offset < 0)
    {
        OSReport("[port] DVDReadAsyncPrio: refusing read of %s: "
                 "addr=%p length=%d offset=%d\n",
                 e->host != NULL ? e->host : e->path, addr, (int)length,
                 (int)offset);
        fileInfo->cb.transferredSize = 0;
        fileInfo->cb.state = DVD_STATE_FATAL_ERROR;
        if (callback)
            callback(-1, fileInfo);
        return FALSE;
    }

    const u32 at = (u32)offset;
    const u32 available = at < e->length ? e->length - at : 0;
    const u32 expected = (u32)length < available ? (u32)length : available;

    // Reads that start at/past EOF cannot satisfy a non-empty DVD request.
    // A final 32-byte alignment read may extend past EOF, but it always starts
    // before EOF and therefore has expected > 0.
    if (expected == 0)
    {
        OSReport("[port] DVDReadAsyncPrio: read starts at/past EOF: %s "
                 "offset=%d length=%d file_length=%u\n",
                 e->host != NULL ? e->host : e->path, (int)offset, (int)length,
                 (unsigned)e->length);
        fileInfo->cb.transferredSize = 0;
        fileInfo->cb.state = DVD_STATE_FATAL_ERROR;
        if (callback)
            callback(-1, fileInfo);
        return FALSE;
    }

#if DVD_READER_THREAD
    // Read synchronously once the entire image is in RAM.
    if (s_disc != NULL && port_disc_in_memory(s_disc))
    {
        const s32 got = pending_read_now(e, addr, length, offset);
        fileInfo->cb.transferredSize = got > 0 ? (u32)got : 0;
        fileInfo->cb.state = DVD_STATE_END;
        owe_busy_poll(&fileInfo->cb);
        if (callback)
            callback(got, fileInfo);
        return TRUE;
    }

    // Queue the read on the reader thread; with every slot busy, wait rather than share the file.
    DvdPending* job;
    while ((job = pending_acquire()) == NULL)
        port_yield();

    job->entry = e;
    job->addr = addr;
    job->length = length;
    job->offset = offset;
    job->callback = callback;
    job->fileInfo = fileInfo;
    fileInfo->cb.transferredSize = 0;
    fileInfo->cb.state = DVD_STATE_BUSY;
    owe_busy_poll(&fileInfo->cb);
    PortDiscQueue(pending_run, job);
    return TRUE;
#else
    const s32 got = dvd_read(e, addr, length, offset);

    if (probe_dvd())
        OSReport("[port] dvd: read %s off=%d len=%d -> %d cb=%d\n", e->path,
                 (int)offset, (int)length, (int)got, callback != NULL);

    fileInfo->cb.transferredSize = got > 0 ? (u32)got : 0;
    if (got != (s32)expected)
    {
        OSReport("[port] DVDReadAsyncPrio: short/failed read %s off=%d "
                 "requested=%d expected=%u got=%d\n",
                 e->host != NULL ? e->host : e->path, (int)offset,
                 (int)length, (unsigned)expected, (int)got);
        fileInfo->cb.state = DVD_STATE_FATAL_ERROR;
        if (callback)
            callback(-1, fileInfo);
        return FALSE;
    }
    if (callback)
    {
        // A caller that asked for a callback gets it here, inline; nothing in this tree does, and
        // there is no later point from which to fire it.
        fileInfo->cb.state = DVD_STATE_END;
        callback(got, fileInfo);
    }
    else
    {
        // Done, but busy to the first poll. See the header comment.
        fileInfo->cb.state = DVD_STATE_BUSY;
    }
    return TRUE;
#endif
}

static s32 dvd_read_raw(const DvdEntry* e, void* addr, s32 length, s32 offset)
{
    if (e == NULL || addr == NULL || length < 0 || offset < 0)
        return -1;
    if (length == 0 || (u32)offset >= e->length)
        return 0;

    const u32 at = (u32)offset;
    const u32 remaining = e->length - at;
    const u32 requested = (u32)length;
    const u32 expected = requested < remaining ? requested : remaining;
    s32 got = -1;
    if (s_archive != NULL)
    {
        got = (s32)port_asset_archive_read(s_archive, e->offset, addr, expected, at);
    }
    else if (s_disc != NULL)
    {
        // Do not cross into the following file in an image. The GameCube code
        // may round its final transfer to 32 bytes, so only the bytes remaining
        // in this logical file are required for that final transfer.
        got = (s32)port_disc_read(s_disc, addr, expected,
                                  (unsigned long long)e->offset + at);
    }
    else
    {
        FILE* f = fopen(e->host, "rb");
        if (f)
        {
            if (fseek(f, (long)at, SEEK_SET) == 0)
                got = (s32)fread(addr, 1, expected, f);
            fclose(f);
        }
    }
    return got;
}

#if DVD_READER_THREAD
static s32 pending_read_now(const DvdEntry* e, void* addr, s32 length, s32 offset)
{
    const s32 got = dvd_read(e, addr, length, offset);
    if (probe_dvd())
        OSReport("[port] dvd: read %s off=%d len=%d -> %d\n", e->path,
                 (int)offset, (int)length, (int)got);
    return got;
}

static void pending_run(void* ctx)
{
    DvdPending* job = (DvdPending*)ctx;
    const s32 got = pending_read_now(job->entry, job->addr, job->length, job->offset);
    DVDFileInfo* fileInfo = job->fileInfo;
    DVDCallback callback = job->callback;

    fileInfo->cb.transferredSize = got > 0 ? (u32)got : 0;
    // A poll that sees the new state also sees the size and the bytes.
    port_store_release_i32(&fileInfo->cb.state, DVD_STATE_END);
    if (callback)
        callback(got, fileInfo);
    // Released last: DVDClose may be waiting on it to free the file.
    port_store_release_i32(&job->inUse, 0);
}
#endif

s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block)
{
    if (!block)
        return DVD_STATE_END;
#if DVD_READER_THREAD
    if (take_busy_poll(block))
        return DVD_STATE_BUSY;
    return port_load_acquire_i32(&block->state);
#else
    if (block->state == DVD_STATE_BUSY)
    {
        // The one poll that says busy. The console's own status call read a block the drive was
        // still writing to through the same const pointer; the cast is the same lie it told.
        ((DVDCommandBlock*)block)->state = DVD_STATE_END;
        return DVD_STATE_BUSY;
    }
    return block->state;
#endif
}

s32 DVDGetDriveStatus(void)
{
    return 0;   // DVD_STATE_END: drive idle, no error, disc present
}

static int ieq(const char* a, const char* b) { return strcmpi(a, b) == 0; }

// `region` in strikers.ini: the disc the files came from, for an extraction with no sys/boot.bin.
static const char* region_id_from_setting(const char* v)
{
    static const char* const kIds[3] = { "G4QE01", "G4QP01", "G4QJ01" };
    static const char* const kNames[3][4] = {
        { "usa", "us", "ntsc-u", "0" },
        { "europe", "eur", "pal", "1" },
        { "japan", "jap", "ntsc-j", "2" },
    };
    int r, k;

    if (v == NULL || *v == '\0')
        return NULL;
    for (r = 0; r < 3; r++)
    {
        if (ieq(v, kIds[r]))
            return kIds[r];
        for (k = 0; k < 4; k++)
            if (ieq(v, kNames[r][k]))
                return kIds[r];
    }
    return NULL;
}

// PORT: split out of DVDGetCurrentDiskID so the id is read at DVDInit, before anything asks.
// port_region() answers every region conditional from these six bytes, and DVDInit runs before the
// first static initialiser.
static void load_disk_id(void)
{
    static int s_done;
    char p[1200];
    FILE* f;

    if (s_done)
        return;
    s_done = 1;

    // An image has already filled these in from its own first 32 bytes, which is exactly what
    // sys/boot.bin is a copy of.
    if (!s_disk_id_read)
    {
        snprintf(p, sizeof p, "%s/../sys/boot.bin", s_root);
        f = fopen(p, "rb");
        if (f)
        {
            s_disk_id_read = fread(s_disk_id, 1, sizeof s_disk_id, f)
                             == sizeof s_disk_id;
            fclose(f);
        }
    }
    // No sys/ beside files/: the player's `region` names the disc, and failing that USA, said out
    // loud because a European or Japanese extraction would otherwise run as the wrong game.
    if (!s_disk_id_read)
    {
        const char* setting = getenv("STRIKERS_REGION");
        const char* id = region_id_from_setting(setting);

        s_disk_id_guessed = 1;
        if (id != NULL)
        {
            memcpy(s_disk_id, id, 6);
            fprintf(stderr, "[port] DVD: no %s; disc id %s from the region "
                            "setting\n", p, id);
        }
        else
        {
            if (setting != NULL && *setting != '\0')
                fprintf(stderr, "[port] DVD: region=%s is not a region "
                                "(usa, europe, japan, or a disc id G4QE01 / "
                                "G4QP01 / G4QJ01); ignored\n", setting);
            memcpy(s_disk_id, "G4QE01", 6);
            fprintf(stderr, "[port] DVD: WARNING: no %s, so the disc id is a "
                            "guess: USA (G4QE01). A European or Japanese "
                            "extraction needs the disc's sys/ folder beside "
                            "files/ (tools/extract-disc.py writes it) or "
                            "`region` in strikers.ini.\n", p);
        }
    }
    else if (getenv("STRIKERS_REGION") != NULL && *getenv("STRIKERS_REGION") != '\0')
    {
        fprintf(stderr, "[port] DVD: region setting ignored: the data carries "
                        "its own disc id\n");
    }

    // Aurora derives the memory card's region directory from this; see src/platform/region.cpp.
    PortSetDiscGameName((const char*)s_disk_id);
}

void* DVDGetCurrentDiskID(void)
{
    load_disk_id();
    return s_disk_id;
}

static const u8* disk_id_bytes(void)
{
    load_disk_id();
    return s_disk_id;
}

// The whole of the runtime region decision, and the reason this port needs no per-region build.
int port_region(void)
{
    static int s_region = -1;
    if (s_region < 0)
    {
        const u8* id = disk_id_bytes();
        if (id[0] == 'G' && id[1] == '4' && id[2] == 'Q' && id[3] == 'P')
            s_region = PORT_REGION_EUROPE;
        else if (id[0] == 'G' && id[1] == '4' && id[2] == 'Q' && id[3] == 'J')
            s_region = PORT_REGION_JAPAN;
        else
            s_region = PORT_REGION_USA;
    }
    return s_region;
}

const char* port_disc_game_code(void)
{
    static char code[5];
    if (code[0] == 0)
    {
        memcpy(code, disk_id_bytes(), 4);
        code[4] = '\0';
    }
    return code;
}

const char* port_disc_maker_code(void)
{
    static char code[3];
    if (code[0] == 0)
    {
        memcpy(code, disk_id_bytes() + 4, 2);
        code[2] = '\0';
    }
    return code;
}
