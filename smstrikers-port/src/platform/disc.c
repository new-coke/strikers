// See include/port/disc.h for what this is and which formats it takes.

#include "port/disc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef STRIKERS_VITA
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#endif
#include "port/disc_reader.h"
#include "port/host.h"

#ifdef STRIKERS_ZLIB
#include <zlib.h>
#endif

#if defined(STRIKERS_ZLIB) || defined(PORT_DISC_INFLATE_HOOK)
#define DISC_CAN_INFLATE 1
#else
#define DISC_CAN_INFLATE 0
#endif

// 64-bit seek. A GameCube image is 1.36 GB so a 32-bit `long` would in fact reach every byte of
// one, but only just, and being one bad image away from a silent wrap is not worth the four lines
// saved.
#ifdef _WIN32
#define disc_seek_raw(f, off) _fseeki64((f), (__int64)(off), SEEK_SET)
#else
#define disc_seek_raw(f, off) fseeko((f), (off_t)(off), SEEK_SET)
#endif

// Every seek clears the stream's flags first, and that is not tidiness.
static int disc_seek(FILE* f, unsigned long long off)
{
    clearerr(f);
    return disc_seek_raw(f, off);
}

#define DISC_RAW 0
#define DISC_CISO 1
#define DISC_GCZ 2

// The disc header's own magic, big-endian at 0x1C, on every GameCube disc.
#define GC_MAGIC 0xC2339F3Du
#define WII_MAGIC 0x5D1C9EA3u
#define GCZ_MAGIC 0xB10BC001u

#define CISO_HEADER 0x8000
#define CISO_MAP (CISO_HEADER - 8)

struct PortDisc
{
    FILE* f;
#ifdef STRIKERS_VITA
    // Raw ISO reads on Vita use positional I/O.  stdio seek state is shared and
    // proved unreliable once the game starts issuing many random FST reads.
    SceUID rawFd;
    char rawPath[512];
#endif
    int kind;
    const char* name;

    unsigned blockSize;

    // CISO: logical block -> stored block, or -1 for a hole.
    int32_t* map;
    unsigned mapCount;

    // GCZ: one offset per block, top bit meaning "stored uncompressed".
    uint64_t* off;
    unsigned blockCount;
    uint64_t dataStart;
    uint64_t compressedSize;
    unsigned char* scratch;   // one compressed block, read before inflating

    // Decoded-block cache; see the note above cache_get.
    unsigned char** cache;
    unsigned* cacheBlock;     // which block each slot holds, ~0u for empty
    unsigned* cacheLen;       // and how many bytes of it are real
    unsigned* cacheAge;
    unsigned cacheSlots;

    // Background copy of a raw image; reads below ramLoaded use it and the rest use the file.
    unsigned char* ram;
    unsigned long long ramSize;
    unsigned long long ramLoaded;
    char* ramPath;
    void* ramThread;
    int32_t ramStop;
    unsigned cacheClock;
};

static uint32_t rd_be32(const unsigned char* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t rd_le32(const unsigned char* p)
{
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}

static uint64_t rd_le64(const unsigned char* p)
{
    return (uint64_t)rd_le32(p) | ((uint64_t)rd_le32(p + 4) << 32);
}

int port_disc_looks_like_image(const char* path)
{
    static const char* const kExt[] = { ".iso", ".gcm", ".ciso", ".gcz",
                                        ".rvz", ".wia", ".nkit" };
    size_t n = strlen(path);
    size_t i;
    for (i = 0; i < sizeof kExt / sizeof kExt[0]; i++)
    {
        size_t e = strlen(kExt[i]);
        if (n > e && strcmpi(path + n - e, kExt[i]) == 0)
            return 1;
    }
    return 0;
}

const char* port_disc_format(const PortDisc* disc)
{
    return disc != NULL && disc->name != NULL ? disc->name : "?";
}

void port_disc_close(PortDisc* disc)
{
    unsigned i;
    if (disc == NULL)
        return;
#ifdef STRIKERS_VITA
    if (disc->rawFd >= 0)
        sceIoClose(disc->rawFd);
#endif
    if (disc->ramThread != NULL)
    {
        port_store_release_i32(&disc->ramStop, 1);
        PortDiscJoin(disc->ramThread);
    }
    free(disc->ram);
    free(disc->ramPath);
    if (disc->f)
        fclose(disc->f);
    free(disc->map);
    free(disc->off);
    free(disc->scratch);
    for (i = 0; i < disc->cacheSlots; i++)
        free(disc->cache[i]);
    free(disc->cache);
    free(disc->cacheBlock);
    free(disc->cacheLen);
    free(disc->cacheAge);
    free(disc);
}

static PortDisc* fail(PortDisc* d, char* err, size_t errsize, const char* text)
{
    snprintf(err, errsize, "%s", text);
    port_disc_close(d);
    return NULL;
}

static int gcz_load(PortDisc* d, unsigned block, unsigned char* out, unsigned* outLen);

static const unsigned char* cache_get(PortDisc* d, unsigned block, unsigned* len)
{
    unsigned i, victim = 0;
    unsigned oldest = ~0u;

    for (i = 0; i < d->cacheSlots; i++)
        if (d->cacheBlock[i] == block)
        {
            d->cacheAge[i] = ++d->cacheClock;
            *len = d->cacheLen[i];
            return d->cache[i];
        }

    for (i = 0; i < d->cacheSlots; i++)
        if (d->cacheAge[i] < oldest)
        {
            oldest = d->cacheAge[i];
            victim = i;
        }

    if (d->cache[victim] == NULL)
    {
        d->cache[victim] = (unsigned char*)malloc(d->blockSize);
        if (d->cache[victim] == NULL)
            return NULL;
    }
    d->cacheBlock[victim] = ~0u;
    if (gcz_load(d, block, d->cache[victim], len) != 0)
        return NULL;
    d->cacheBlock[victim] = block;
    d->cacheLen[victim] = *len;
    d->cacheAge[victim] = ++d->cacheClock;
    return d->cache[victim];
}

static int cache_init(PortDisc* d)
{
    // Sixteen slots, not a setting: the 1011 reads between launch and kickoff cost 29.8 ms at one
    // slot and at sixty-four alike, because the reads run forward.
    d->cacheSlots = 16u;
    d->cache = (unsigned char**)calloc(d->cacheSlots, sizeof *d->cache);
    d->cacheBlock = (unsigned*)malloc(d->cacheSlots * sizeof *d->cacheBlock);
    d->cacheLen = (unsigned*)calloc(d->cacheSlots, sizeof *d->cacheLen);
    d->cacheAge = (unsigned*)calloc(d->cacheSlots, sizeof *d->cacheAge);
    if (d->cache == NULL || d->cacheBlock == NULL || d->cacheLen == NULL
        || d->cacheAge == NULL)
        return -1;
    memset(d->cacheBlock, 0xFF, d->cacheSlots * sizeof *d->cacheBlock);
    return 0;
}

static int gcz_load(PortDisc* d, unsigned block, unsigned char* out, unsigned* outLen)
{
    const uint64_t kStored = 1ull << 63;
    uint64_t raw, next, clen;
    int stored;

    *outLen = 0;
    if (block >= d->blockCount)
        return -1;

    stored = (d->off[block] & kStored) != 0;
    raw = d->off[block] & ~kStored;
    next = block + 1 < d->blockCount ? (d->off[block + 1] & ~kStored)
                                     : d->compressedSize;
    if (next < raw)
        return -1;
    clen = next - raw;

    if (disc_seek(d->f, d->dataStart + raw) != 0)
        return -1;

    if (stored)
    {
        // Bigger compressed than raw, so the writer kept it as it was.
        size_t want = clen < d->blockSize ? (size_t)clen : d->blockSize;
        size_t got = fread(out, 1, want, d->f);
        *outLen = (unsigned)got;
        return got == want ? 0 : -1;
    }

#if DISC_CAN_INFLATE
    if (clen > d->blockSize + (d->blockSize >> 3) + 256)
        return -1;   // a compressed block larger than zlib's worst case
    if (fread(d->scratch, 1, (size_t)clen, d->f) != (size_t)clen)
        return -1;
    {
#ifdef STRIKERS_ZLIB
        uLongf dst = d->blockSize;
        if (uncompress(out, &dst, d->scratch, (uLong)clen) != Z_OK)
            return -1;
#else
        unsigned long dst = d->blockSize;
        if (port_disc_inflate(out, &dst, d->scratch, (unsigned long)clen) != 0)
            return -1;
#endif
        *outLen = (unsigned)dst;
    }
    return 0;
#else
    (void)clen;
    return -1;
#endif
}

static PortDisc* gcz_open(PortDisc* d, const unsigned char* head, char* err,
                          size_t errsize)
{
    unsigned char* table;
    size_t tableBytes;

#if !DISC_CAN_INFLATE
    return fail(d, err, errsize,
                "That is a .gcz disc image, and this build of the game was "
                "compiled without zlib,\nso it cannot decompress one.\n\n"
                "Use a plain .iso, a .gcm or a .ciso instead.");
#else
    d->kind = DISC_GCZ;
    d->name = "GCZ";
    d->compressedSize = rd_le64(head + 8);
    d->blockSize = rd_le32(head + 24);
    d->blockCount = rd_le32(head + 28);

    if (d->blockSize < 0x400 || d->blockSize > (16u << 20) || d->blockCount == 0
        || d->blockCount > (64u << 20))
        return fail(d, err, errsize,
                    "That .gcz file's header does not make sense (block size or "
                    "block count out of\nrange). It is probably damaged; "
                    "re-create it, or use a plain .iso.");

    tableBytes = (size_t)d->blockCount * 8;
    table = (unsigned char*)malloc(tableBytes);
    d->off = (uint64_t*)malloc((size_t)d->blockCount * sizeof *d->off);
    d->scratch = (unsigned char*)malloc(d->blockSize + (d->blockSize >> 3) + 256);
    if (table == NULL || d->off == NULL || d->scratch == NULL)
    {
        free(table);
        return fail(d, err, errsize, "Out of memory reading the .gcz block table.");
    }
    if (disc_seek(d->f, 32) != 0 || fread(table, 1, tableBytes, d->f) != tableBytes)
    {
        free(table);
        return fail(d, err, errsize,
                    "That .gcz file ends inside its block table, it is "
                    "truncated.");
    }
    {
        unsigned i;
        for (i = 0; i < d->blockCount; i++)
            d->off[i] = rd_le64(table + (size_t)i * 8);
    }
    free(table);

    // Header, then the offsets, then one adler32 per block.
    d->dataStart = 32 + (uint64_t)d->blockCount * 12;

    if (cache_init(d) != 0)
        return fail(d, err, errsize, "Out of memory sizing the .gcz block cache.");
    return d;
#endif
}

static PortDisc* ciso_open(PortDisc* d, const unsigned char* head, char* err,
                           size_t errsize)
{
    unsigned char map[CISO_MAP];
    unsigned i;
    int32_t stored = 0;

    d->kind = DISC_CISO;
    d->name = "CISO";
    d->blockSize = rd_le32(head + 4);
    if (d->blockSize < 0x800 || d->blockSize > (64u << 20))
        return fail(d, err, errsize,
                    "That .ciso file's block size is not a sane one, so this is "
                    "not a GameCube CISO.\n\nThe PSP uses a different format "
                    "with the same four-letter magic. Convert the image\nto a "
                    "plain .iso.");

    if (disc_seek(d->f, 8) != 0 || fread(map, 1, sizeof map, d->f) != sizeof map)
        return fail(d, err, errsize, "That .ciso file ends inside its header.");

    d->map = (int32_t*)malloc(sizeof(int32_t) * CISO_MAP);
    if (d->map == NULL)
        return fail(d, err, errsize, "Out of memory reading the .ciso map.");
    d->mapCount = CISO_MAP;
    for (i = 0; i < CISO_MAP; i++)
    {
        // One byte per block, 1 for stored and 0 for a hole, and nothing else.
        if (map[i] > 1)
            return fail(d, err, errsize,
                        "That .ciso file's block map is not one byte per block, "
                        "so it is not a GameCube\nCISO, the PSP's format uses "
                        "the same magic. Convert the image to a plain .iso.");
        d->map[i] = map[i] ? stored++ : -1;
    }
    if (d->map[0] < 0)
        return fail(d, err, errsize,
                    "That .ciso file does not store its first block, which "
                    "holds the disc header.\nIt is not a disc image this can "
                    "read.");
    return d;
}


#if defined(__SWITCH__)
// Use a separate handle so background reads cannot change d->f's seek position.
static void ram_load(void* ctx)
{
    PortDisc* d = (PortDisc*)ctx;
    const unsigned long long start = port_monotonic_ns();
    const size_t chunk = 4u * 1024u * 1024u;
    unsigned long long at = 0;
    FILE* f = fopen(d->ramPath, "rb");
    if (f == NULL)
    {
        fprintf(stderr, "[disc] could not reopen %s for the memory copy; reading from the file\n",
                d->ramPath);
        return;
    }
    while (at < d->ramSize && !port_load_acquire_i32(&d->ramStop))
    {
        size_t want = d->ramSize - at < chunk ? (size_t)(d->ramSize - at) : chunk;
        size_t got = fread(d->ram + at, 1, want, f);
        if (got == 0)
            break;
        at += got;
        port_store_release_u64(&d->ramLoaded, at);
    }
    fclose(f);
    fprintf(stderr, "[disc] %llu of %llu MiB in memory after %.1f s\n", at >> 20, d->ramSize >> 20,
            (double)(port_monotonic_ns() - start) / 1e9);
}

// STRIKERS_DISC_IN_RAM=1 copies a raw image into memory in the background, if it fits.
static void ram_start(PortDisc* d, const char* path)
{
    const char* e = getenv("STRIKERS_DISC_IN_RAM");
    long long size;
    if (e == NULL || *e == '\0' || atoi(e) == 0)
        return;
    if (disc_seek(d->f, 0) != 0 || fseek(d->f, 0, SEEK_END) != 0)
        return;
    size = ftell(d->f);
    if (size <= 0)
        return;
    d->ram = (unsigned char*)malloc((size_t)size);
    if (d->ram == NULL)
    {
        fprintf(stderr, "[disc] %lld MiB would not fit in memory; reading from the file\n", size >> 20);
        return;
    }
    d->ramPath = (char*)malloc(strlen(path) + 1);
    if (d->ramPath == NULL)
    {
        free(d->ram);
        d->ram = NULL;
        return;
    }
    strcpy(d->ramPath, path);
    d->ramSize = (unsigned long long)size;
    d->ramLoaded = 0;
    fprintf(stderr, "[disc] copying %lld MiB into memory in the background\n", size >> 20);
    d->ramThread = PortDiscSpawn(ram_load, d);
}
#endif

int port_disc_in_memory(PortDisc* d)
{
    return d != NULL && d->ram != NULL && port_load_acquire_u64(&d->ramLoaded) == d->ramSize;
}

long port_disc_read(PortDisc* d, void* dst, size_t len, unsigned long long offset)
{
    unsigned char* out = (unsigned char*)dst;
    size_t done = 0;

    if (d == NULL || dst == NULL)
        return -1;

    if (d->kind == DISC_RAW)
    {
#ifdef STRIKERS_VITA
        if (d->rawFd < 0 || len > 0x7fffffffU)
            return -1;
        {
            int got = sceIoPread(d->rawFd, out, (SceSize)len, (SceOff)offset);
            // After standby the descriptor may be dead: reopen and retry for about a second.
            for (int attempt = 0; got < 0 && attempt < 20; ++attempt)
            {
                sceKernelDelayThread(50000);
                SceUID fd = sceIoOpen(d->rawPath, SCE_O_RDONLY, 0);
                if (fd < 0)
                    continue;
                sceIoClose(d->rawFd);
                d->rawFd = fd;
                got = sceIoPread(d->rawFd, out, (SceSize)len, (SceOff)offset);
            }
            return got < 0 ? -1 : (long)got;
        }
#else
        if (d->ram != NULL && offset + len <= port_load_acquire_u64(&d->ramLoaded))
        {
            memcpy(out, d->ram + offset, len);
            return (long)len;
        }
        if (disc_seek(d->f, offset) != 0)
            return -1;
        return (long)fread(out, 1, len, d->f);
#endif
    }

    while (done < len)
    {
        unsigned long long abs = offset + done;
        unsigned block = (unsigned)(abs / d->blockSize);
        unsigned within = (unsigned)(abs % d->blockSize);
        size_t want = d->blockSize - within;
        if (want > len - done)
            want = len - done;

        if (d->kind == DISC_CISO)
        {
            if (block >= d->mapCount)
                break;
            if (d->map[block] < 0)
            {
                // A hole: padding the writer dropped, which was zeroes on the disc.
                memset(out + done, 0, want);
            }
            else
            {
                unsigned long long at = (unsigned long long)CISO_HEADER
                                        + (unsigned long long)d->map[block] * d->blockSize
                                        + within;
                size_t got;
                if (disc_seek(d->f, at) != 0)
                    return done > 0 ? (long)done : -1;
                got = fread(out + done, 1, want, d->f);
                done += got;
                if (got != want)
                    break;
                continue;
            }
        }
        else
        {
            unsigned have = 0;
            const unsigned char* p = cache_get(d, block, &have);
            if (p == NULL)
                return done > 0 ? (long)done : -1;
            if (within >= have)
                break;
            if (want > have - within)
                want = have - within;
            memcpy(out + done, p + within, want);
        }
        done += want;
    }
    return (long)done;
}

// NKit is *not* a refusal, and working out why cost a wrong one first.
static int note_nkit(const unsigned char* head, size_t got)
{
    if (got >= 0x204 && memcmp(head + 0x200, "NKIT", 4) == 0)
    {
        fprintf(stderr,
                "[port] disc: NKit-processed image, the disc's padding was "
                "removed and rebuilt.\n"
                "[port] disc: WARNING: this lightweight reader does not rebuild "
                "legacy NKit logical offsets;\n"
                "[port] disc: if file fingerprints or FEN headers disagree with "
                "the FST, use a restored ISO\n"
                "[port] disc: or an extracted files/ directory.\n");
        return 1;
    }
    return 0;
}

PortDisc* port_disc_open(const char* path, char* err, size_t errsize)
{
    unsigned char head[0x220];
    size_t got;
    PortDisc* d;

    err[0] = '\0';
    d = (PortDisc*)calloc(1, sizeof *d);
    if (d == NULL)
    {
        snprintf(err, errsize, "Out of memory.");
        return NULL;
    }
#ifdef STRIKERS_VITA
    d->rawFd = -1;
#endif

    d->f = fopen(path, "rb");
    if (d->f == NULL)
    {
        snprintf(err, errsize,
                 "That file could not be opened:\n\n  %s\n\nCheck the path and "
                 "that the file is readable.", path);
        free(d);
        return NULL;
    }

    got = fread(head, 1, sizeof head, d->f);
    if (got < 0x20)
        return fail(d, err, errsize,
                    "That file is too small to be a disc image.");

    // The formats that are recognisable and cannot be read.
    if (memcmp(head, "RVZ\x01", 4) == 0 || memcmp(head, "WIA\x01", 4) == 0)
    {
        char msg[1024];
        snprintf(msg, sizeof msg,
                 "That is a .%s disc image, which this port cannot read.\n\n"
                 "RVZ and WIA store the disc re-encoded, zstd, LZMA and bzip2 "
                 "blocks, with the\nlayout rebuilt around the partitions; "
                 "which is a much larger reader than the\nthree formats here "
                 "put together.\n\nConvert it to a plain .iso first. In "
                 "Dolphin: right-click the game, Convert File\nFormat..., and "
                 "set Format to ISO.",
                 head[0] == 'R' ? "rvz" : "wia");
        return fail(d, err, errsize, msg);
    }

    if (rd_be32(head + 0x1C) == GC_MAGIC)
    {
        d->kind = DISC_RAW;
        d->name = note_nkit(head, got) ? "NKit/raw" : "raw";
#ifdef STRIKERS_VITA
        d->rawFd = sceIoOpen(path, SCE_O_RDONLY, 0);
        snprintf(d->rawPath, sizeof d->rawPath, "%s", path);
        if (d->rawFd < 0)
        {
            char msg[1024];
            snprintf(msg, sizeof msg,
                     "That disc image could not be opened for positional reads: %s", path);
            return fail(d, err, errsize, msg);
        }
        // Keep exactly one backend for raw images on Vita.
        fclose(d->f);
        d->f = NULL;
#elif defined(__SWITCH__)
        ram_start(d, path);
#endif
        return d;
    }

    if (rd_be32(head + 0x18) == WII_MAGIC)
        return fail(d, err, errsize,
                    "That is a Wii disc image. Super Mario Strikers is a "
                    "GameCube game, the disc\nthis needs has a code beginning "
                    "G4Q.");

    if (memcmp(head, "CISO", 4) == 0)
        d = ciso_open(d, head, err, errsize);
    else if (rd_le32(head) == GCZ_MAGIC)
        d = gcz_open(d, head, err, errsize);
    else
        return fail(d, err, errsize,
                    "That file is not a disc image this port recognises.\n\n"
                    "It reads .iso and .gcm (a plain disc image, NKit-processed "
                    "or not), .ciso and\n.gcz. It names .rvz and .wia and says "
                    "to convert them; anything else has no\nheader it knows.");

    if (d == NULL)
        return NULL;

    // A container this build can read, holding something that is not this console's disc.
    if (port_disc_read(d, head, sizeof head, 0) < 0x204)
        return fail(d, err, errsize,
                    "That image could not be read past its own header, it is "
                    "truncated or damaged.");
    if (rd_be32(head + 0x1C) != GC_MAGIC)
        return fail(d, err, errsize,
                    "That image decompresses to something that is not a "
                    "GameCube disc: the disc\nmagic is missing from its header. "
                    "It is either damaged or an image of\nsomething else.");
    if (note_nkit(head, sizeof head) && d->kind == DISC_GCZ)
        d->name = "NKit/GCZ";
    return d;
}

static unsigned be32u(const unsigned char* p)
{
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

// Depth of the disc's directory tree. This game's is three deep; sixteen is there so a malformed
// FST cannot walk off the stack rather than because any disc needs it.
#define DISC_FST_DEPTH 16

static int walk_fail(unsigned char* fst, char* err, size_t errsize, const char* text)
{
    free(fst);
    snprintf(err, errsize, "%s", text);
    return -1;
}

int port_disc_walk(PortDisc* d, PortDiscVisit visit, void* user, char* err, size_t errsize)
{
    unsigned char hdr[0x440];
    unsigned char* fst = NULL;
    unsigned fstOff, fstSize, nent, strOff, i;
    char dir[1024];
    char full[1024];
    unsigned endStack[DISC_FST_DEPTH];
    int lenStack[DISC_FST_DEPTH];
    int sp = 0, dirLen = 0;

    err[0] = '\0';
    if (port_disc_read(d, hdr, sizeof hdr, 0) != (long)sizeof hdr)
        return walk_fail(fst, err, errsize, "That image is too short to hold a disc header.");

    fstOff = be32u(hdr + 0x424);
    fstSize = be32u(hdr + 0x428);
    if (fstSize < 12 || fstSize > (16u << 20))
        return walk_fail(fst, err, errsize,
                         "That image's file table is not a sane size, so it is "
                         "damaged or is not a\nGameCube disc.");

    fst = (unsigned char*)malloc(fstSize);
    if (fst == NULL)
        return walk_fail(fst, err, errsize, "Out of memory reading that image's file table.");
    if (port_disc_read(d, fst, fstSize, fstOff) != (long)fstSize)
        return walk_fail(fst, err, errsize,
                         "That image ends inside its file table: it is truncated. "
                         "A part-downloaded\nimage looks exactly like this.");

    // The entry count is read off the disc, so it is checked before it is multiplied: nent * 12 on
    // a damaged image can wrap a 32-bit product back to a small number and walk straight past the
    // bounds test below it.
    nent = be32u(fst + 8);
    if (nent == 0 || nent > fstSize / 12)
        return walk_fail(fst, err, errsize,
                         "That image's file table claims more entries than it "
                         "contains. It is damaged.");
    strOff = nent * 12;
    if (strOff >= fstSize)
        return walk_fail(fst, err, errsize,
                         "That image's file table claims more entries than it "
                         "contains. It is damaged.");

    endStack[0] = nent;
    lenStack[0] = 0;
    dir[0] = '\0';

    for (i = 1; i < nent; i++)
    {
        const unsigned char* e = fst + (size_t)i * 12;
        unsigned nameOff = be32u(e) & 0xFFFFFF;
        const char* name;
        unsigned limit;

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
            if (sp + 1 < DISC_FST_DEPTH)
            {
                sp++;
                endStack[sp] = be32u(e + 8);
                lenStack[sp] = dirLen;
                dirLen += snprintf(dir + dirLen, sizeof dir - (size_t)dirLen,
                                   "%s%s", dirLen ? "/" : "", name);
                if (dirLen >= (int)sizeof dir)
                    return walk_fail(fst, err, errsize,
                                     "That image has a path too long to be this game's disc.");
                if (visit(user, dir, 0, 0, 1) != 0)
                    break;
            }
            continue;
        }

        snprintf(full, sizeof full, "%s%s%s", dir, dirLen ? "/" : "", name);
        if (visit(user, full, be32u(e + 4), be32u(e + 8), 0) != 0)
            break;
    }

    free(fst);
    return 0;
}
