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
#include "port/fatal.h"
#include "port/host.h"
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
    u32 offset;   // byte offset into the image; unused for a host file
    u32 length;
} DvdEntry;

static DvdEntry* s_entries;
static int s_count;
static int s_cap;
static char s_root[1024];
// Non-NULL when the data is a disc image rather than a directory.
static PortDisc* s_disc;

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

static u32 be32(const u8* p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void image_fatal(const char* path, const char* text)
{
    char msg[4096];
    snprintf(msg, sizeof msg, "%s\n\n  Image: %s\n", text, path);
    port_fatal("Super Mario Strikers: unusable disc image", msg);
}

// Depth of the disc's directory tree. This game's is three deep; sixteen is there so a malformed
// FST cannot walk off the stack rather than because any disc needs it.
#define DVD_FST_DEPTH 16

static void index_image_fst(const char* path)
{
    u8 hdr[0x440];
    u8* fst;
    u32 fstOff, fstSize, nent, strOff;
    char dir[1024];
    char full[1024];
    u32 endStack[DVD_FST_DEPTH];
    int lenStack[DVD_FST_DEPTH];
    int sp = 0, dirLen = 0;
    u32 i;

    if (port_disc_read(s_disc, hdr, sizeof hdr, 0) != (long)sizeof hdr)
        image_fatal(path, "That image is too short to hold a disc header.");

    // The disc id, from the image itself. An extracted folder reads these same 32 bytes out of
    // sys/boot.bin, which is a copy of them, so region handling is identical either way
    // (include/port/region.h).
    memcpy(s_disk_id, hdr, sizeof s_disk_id);
    s_disk_id_read = 1;

    fstOff = be32(hdr + 0x424);
    fstSize = be32(hdr + 0x428);
    if (fstSize < 12 || fstSize > (16u << 20))
        image_fatal(path,
                    "That image's file table is not a sane size, so it is "
                    "damaged or is not a\nGameCube disc.");

    fst = (u8*)malloc(fstSize);
    if (fst == NULL)
        image_fatal(path, "Out of memory reading that image's file table.");
    if (port_disc_read(s_disc, fst, fstSize, fstOff) != (long)fstSize)
        image_fatal(path,
                    "That image ends inside its file table: it is truncated. "
                    "A part-downloaded\nimage looks exactly like this.");

    // The entry count is read off the disc, so it is checked before it is multiplied: nent * 12 on
    // a damaged image can wrap a 32-bit product back to a small number and walk straight past the
    // bounds test below it.
    nent = be32(fst + 8);
    if (nent == 0 || nent > fstSize / 12)
        image_fatal(path,
                    "That image's file table claims more entries than it "
                    "contains. It is damaged.");
    strOff = nent * 12;
    if (strOff >= fstSize)
        image_fatal(path,
                    "That image's file table claims more entries than it "
                    "contains. It is damaged.");

    endStack[0] = nent;
    lenStack[0] = 0;
    dir[0] = '\0';

    for (i = 1; i < nent; i++)
    {
        const u8* e = fst + (size_t)i * 12;
        u32 nameOff = be32(e) & 0xFFFFFF;
        const char* name;
        u32 limit;

        while (sp > 0 && i >= endStack[sp])
        {
            dirLen = lenStack[sp];
            dir[dirLen] = '\0';
            sp--;
        }

        if (strOff + nameOff >= fstSize)
            continue;   // a name outside the table; skip it rather than read it
        name = (const char*)fst + strOff + nameOff;
        for (limit = strOff + nameOff; limit < fstSize && fst[limit]; limit++)
            ;
        if (limit >= fstSize)
            continue;   // unterminated name at the end of the table

        if (e[0] & 1)
        {
            if (sp + 1 < DVD_FST_DEPTH)
            {
                sp++;
                endStack[sp] = be32(e + 8);
                lenStack[sp] = dirLen;
                dirLen += snprintf(dir + dirLen, sizeof dir - (size_t)dirLen,
                                   "%s%s", dirLen ? "/" : "", name);
                if (dirLen >= (int)sizeof dir)
                    image_fatal(path, "That image has a path too long to be this game's disc.");
            }
            continue;
        }

        snprintf(full, sizeof full, "%s%s%s", dir, dirLen ? "/" : "", name);
        add_entry(full, NULL, be32(e + 4), be32(e + 8));
    }

    free(fst);
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
#define DVD_MAX_TRIED 4

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

    const char* env = getenv("STRIKERS_DATA");
    const char* why = "STRIKERS_DATA / the 'data' key in strikers.ini";
    if (env != NULL && *env != '\0')
    {
        snprintf(s_root, sizeof s_root, "%s", env);
        snprintf(tried[nTried], sizeof tried[0], "%s", s_root);
        triedWhy[nTried++] = why;
        if (is_regular_file(s_root))
            open_image(s_root);   // never returns if it is not a usable image
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

    if (s_disc != NULL)
        fprintf(stderr, "[port] DVD: %d files in %s (%s image)\n", s_count,
                s_root, port_disc_format(s_disc));
    else
        fprintf(stderr, "[port] DVD: %d files under %s\n", s_count, s_root);

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

BOOL DVDClose(DVDFileInfo* fileInfo)
{
    if (fileInfo)
        fileInfo->cb.state = DVD_STATE_END;
    return TRUE;
}

s32 DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                     DVDCallback callback, s32 prio)
{
    (void)prio;
    if (!fileInfo || fileInfo->startAddr >= (u32)s_count)
        return -1;
    const DvdEntry* e = &s_entries[fileInfo->startAddr];

    // A null destination or nonsensical length is a caller bug, named here because the UCRT's fread
    // rejects them through _invalid_parameter, aborting from inside the CRT with the caller's
    // numbers nowhere.
    if (addr == NULL || length <= 0)
    {
        OSReport("[port] DVDReadAsyncPrio: refusing %s read of %s: "
                 "addr=%p length=%d offset=%d\n",
                 addr == NULL ? "null-destination" : "non-positive-length",
                 e->host != NULL ? e->host : e->path, addr, (int)length,
                 (int)offset);
        fileInfo->cb.transferredSize = 0;
        fileInfo->cb.state = DVD_STATE_END;
        if (callback)
            callback(-1, fileInfo);
        return -1;
    }

    s32 got = -1;
    if (offset < 0)
    {
        got = -1;
    }
    else if (s_disc != NULL)
    {
        // Clamp to the file. A host file gets this free from fread, but on an image the next file
        // sits right there, so an over-long read would return the neighbour's bytes.
        const u32 at = (u32)offset;
        if (at >= e->length)
        {
            got = 0;
        }
        else
        {
            u32 avail = e->length - at;
            size_t want = (size_t)length < (size_t)avail ? (size_t)length
                                                         : (size_t)avail;
            got = (s32)port_disc_read(s_disc, addr, want,
                                      (unsigned long long)e->offset + at);
        }
    }
    else
    {
        FILE* f = fopen(e->host, "rb");
        if (f)
        {
            if (fseek(f, offset, SEEK_SET) == 0)
                got = (s32)fread(addr, 1, (size_t)length, f);
            fclose(f);
        }
    }

    if (probe_dvd())
        OSReport("[port] dvd: read %s off=%d len=%d -> %d cb=%d\n", e->path,
                 (int)offset, (int)length, (int)got, callback != NULL);

    fileInfo->cb.transferredSize = got > 0 ? (u32)got : 0;
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
    return got < 0 ? -1 : 0;
}

s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block)
{
    if (!block)
        return DVD_STATE_END;
    if (block->state == DVD_STATE_BUSY)
    {
        // The one poll that says busy. The console's own status call read a block the drive was
        // still writing to through the same const pointer; the cast is the same lie it told.
        ((DVDCommandBlock*)block)->state = DVD_STATE_END;
        return DVD_STATE_BUSY;
    }
    return block->state;
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
