// PSARC 1.4 subset emitted by tools/asset_pipeline/psarc.py.
// Metadata is immutable after open; reads use positional I/O or a locked stream.
#include "port/asset_archive.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#ifdef STRIKERS_VITA
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#elif defined(_WIN32)
#include <io.h>
#endif
#ifdef STRIKERS_ZLIB
#include <zlib.h>
#endif

#define BLOCK_SIZE 65536u
#define MAX_TOC (32u * 1024u * 1024u)
#define MAX_MANIFEST (8u * 1024u * 1024u)
typedef struct ArchiveEntry {
    uint64_t offset, size;
    unsigned block;
    int stored;
} ArchiveEntry;
typedef struct ArchiveName { const char* path; unsigned index; } ArchiveName;
struct PortAssetArchive {
    FILE* file;
    int profile;
#ifdef STRIKERS_VITA
    SceUID fd;
#endif
    unsigned count, blocks;
    ArchiveEntry* entries;  // Entry 0 is the PSARC path manifest.
    unsigned short* lengths;
    uint64_t* offsets;
    char* manifest;
    char** paths;
    ArchiveName* names;
    int ignore_case;
    unsigned char* toc;
};
#ifdef _WIN32
static SRWLOCK s_stats_lock = SRWLOCK_INIT;
static void stats_lock(void) { AcquireSRWLockExclusive(&s_stats_lock); }
static void stats_unlock(void) { ReleaseSRWLockExclusive(&s_stats_lock); }
#else
static pthread_mutex_t s_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static void stats_lock(void) { pthread_mutex_lock(&s_stats_lock); }
static void stats_unlock(void) { pthread_mutex_unlock(&s_stats_lock); }
#endif
static PortAssetIoStats s_stats;
void port_asset_io_snapshot(PortAssetIoStats* out)
{
    if (!out) return;
    stats_lock(); *out = s_stats; stats_unlock();
}
static uint64_t io_time_us(void)
{
#ifdef STRIKERS_VITA
    return sceKernelGetProcessTimeWide();
#elif defined(_WIN32)
    LARGE_INTEGER ticks, frequency;
    QueryPerformanceCounter(&ticks); QueryPerformanceFrequency(&frequency);
    return (uint64_t)(ticks.QuadPart / frequency.QuadPart) * 1000000u +
           (uint64_t)(ticks.QuadPart % frequency.QuadPart) * 1000000u / frequency.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u;
#endif
}
static uint64_t be(const unsigned char* p, unsigned n)
{
    uint64_t v = 0;
    while (n--) v = (v << 8) | *p++;
    return v;
}
static size_t read_at(PortAssetArchive* a, void* dst, size_t n, uint64_t pos)
{
    const uint64_t start = a->profile ? io_time_us() : 0;
    size_t got = 0;
#ifdef STRIKERS_VITA
    if (pos > INT64_MAX || n > INT_MAX) return 0;
    const int result = sceIoPread(a->fd, dst, (unsigned)n, (SceOff)pos);
    got = result > 0 ? (size_t)result : 0;
#else
#ifdef _WIN32
    _lock_file(a->file);
    clearerr(a->file);
    if (pos <= INT64_MAX && _fseeki64(a->file, (int64_t)pos, SEEK_SET) == 0)
        got = fread(dst, 1, n, a->file);
    _unlock_file(a->file);
#else
    flockfile(a->file);
    clearerr(a->file);
    if (pos <= INT64_MAX && fseeko(a->file, (off_t)pos, SEEK_SET) == 0)
        got = fread(dst, 1, n, a->file);
    funlockfile(a->file);
#endif
#endif
    if (a->profile) {
        const uint64_t elapsed = io_time_us() - start;
        stats_lock();
        ++s_stats.physical_reads; s_stats.physical_bytes += got;
        s_stats.read_us += elapsed; s_stats.read_errors += got != n;
        stats_unlock();
    }
    return got;
}
static unsigned stored_size(const PortAssetArchive* a, unsigned block)
{
    return a->lengths[block] ? a->lengths[block] : BLOCK_SIZE;
}
void port_asset_archive_close(PortAssetArchive* a)
{
    if (!a) return;
#ifdef STRIKERS_VITA
    if (a->fd >= 0) sceIoClose(a->fd);
#endif
    if (a->file) fclose(a->file);
    free(a->entries); free(a->lengths); free(a->offsets);
    free(a->paths); free(a->names); free(a->manifest); free(a->toc); free(a);
}
static PortAssetArchive* fail(PortAssetArchive* a, char* error, size_t n, const char* text)
{
    if (error && n) snprintf(error, n, "PSARC: %s", text);
    port_asset_archive_close(a);
    return NULL;
}
static int safe_path(const char* s)
{
    const char* part = s;
    if (!*s) return 0;
    for (;; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c && (c < 32 || c == 127 || c == '\\' || c == ':')) return 0;
        if (!c || c == '/') {
            size_t n = (size_t)(s - part);
            if (!n || (n == 1 && part[0] == '.') ||
                (n == 2 && part[0] == '.' && part[1] == '.')) return 0;
            if (!c) return 1;
            part = s + 1;
        }
    }
}
static int compare_path(const char* a, const char* b, int fold)
{
    for (;;) {
        unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (fold) {
            if (x >= 'a' && x <= 'z') x -= 'a' - 'A';
            if (y >= 'a' && y <= 'z') y -= 'a' - 'A';
        }
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}
static int compare_names_fold(const void* a, const void* b)
{ return compare_path(((const ArchiveName*)a)->path, ((const ArchiveName*)b)->path, 1); }
static int compare_names_exact(const void* a, const void* b)
{ return compare_path(((const ArchiveName*)a)->path, ((const ArchiveName*)b)->path, 0); }
unsigned port_asset_archive_count(const PortAssetArchive* a) { return a ? a->count - 1 : 0; }
const char* port_asset_archive_path(const PortAssetArchive* a, unsigned i)
{ return a && i < a->count - 1 ? a->paths[i] : NULL; }
uint64_t port_asset_archive_size(const PortAssetArchive* a, unsigned i)
{ return a && i < a->count - 1 ? a->entries[i + 1].size : 0; }
int port_asset_archive_find(const PortAssetArchive* a, const char* path)
{
    if (!a || !path) return -1;
    unsigned lo = 0, hi = a->count - 1;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        int cmp = compare_path(a->names[mid].path, path, a->ignore_case);
        if (!cmp) return (int)a->names[mid].index;
        if (cmp < 0) lo = mid + 1;
        else hi = mid;
    }
    return -1;
}
static long read_entry(PortAssetArchive* a, unsigned i, void* dst, size_t bytes, uint64_t at)
{
    if (!a || i >= a->count || (!dst && bytes) || bytes > LONG_MAX) return -1;
    const ArchiveEntry* e = &a->entries[i];
    if (at >= e->size || !bytes) return 0;
    if (bytes > e->size - at) bytes = (size_t)(e->size - at);
    if (e->stored)
        return read_at(a, dst, bytes, e->offset + at) == bytes ? (long)bytes : -1;
#ifdef STRIKERS_ZLIB
    unsigned char* scratch = (unsigned char*)malloc(BLOCK_SIZE * 2u);
    if (!scratch) return -1;
    size_t copied = 0;
    while (copied < bytes) {
        uint64_t pos = at + copied;
        unsigned k = e->block + (unsigned)(pos / BLOCK_SIZE);
        unsigned inside = (unsigned)(pos % BLOCK_SIZE);
        unsigned expected = (unsigned)((e->size - (pos / BLOCK_SIZE) * BLOCK_SIZE) < BLOCK_SIZE
            ? (e->size - (pos / BLOCK_SIZE) * BLOCK_SIZE) : BLOCK_SIZE);
        unsigned physical = stored_size(a, k);
        if (read_at(a, scratch, physical, a->offsets[k]) != physical) break;
        const unsigned char* data = scratch;
        if (physical < expected) {
            const uint64_t start = a->profile ? io_time_us() : 0;
            z_stream stream;
            memset(&stream, 0, sizeof stream);
            stream.next_in = scratch; stream.avail_in = physical;
            stream.next_out = scratch + BLOCK_SIZE; stream.avail_out = expected;
            int valid = 0;
            if (inflateInit(&stream) == Z_OK) {
                const int rc = inflate(&stream, Z_FINISH);
                valid = rc == Z_STREAM_END && stream.total_out == expected && stream.total_in == physical;
                inflateEnd(&stream);
            }
            if (a->profile) {
                const uint64_t elapsed = io_time_us() - start;
                stats_lock(); ++s_stats.inflates;
                s_stats.inflated_bytes += valid ? expected : 0;
                s_stats.inflate_us += elapsed; s_stats.inflate_errors += !valid;
                stats_unlock();
            }
            if (!valid) break;
            data = scratch + BLOCK_SIZE;
        }
        size_t n = expected - inside;
        if (n > bytes - copied) n = bytes - copied;
        memcpy((unsigned char*)dst + copied, data + inside, n);
        copied += n;
    }
    free(scratch);
    return copied == bytes ? (long)copied : -1;
#else
    return -1;
#endif
}
long port_asset_archive_read(PortAssetArchive* a, unsigned i, void* dst, size_t bytes, uint64_t offset)
{
    if (!a || i >= a->count - 1) return -1;
    if (a->profile) {
        stats_lock(); ++s_stats.requests; s_stats.requested_bytes += bytes; stats_unlock();
    }
    return read_entry(a, i + 1, dst, bytes, offset);
}
PortAssetArchive* port_asset_archive_open(const char* path, char* error, size_t n)
{
    unsigned char h[32];
    PortAssetArchive* a = (PortAssetArchive*)calloc(1, sizeof *a);
    if (!a) return fail(NULL, error, n, "out of memory");
    const char* profile = getenv("STRIKERS_ASSET_IO_PROFILE");
    a->profile = profile && strcmp(profile, "1") == 0;
#ifdef STRIKERS_VITA
    a->fd = -1;
#endif
    if (!path || !(a->file = fopen(path, "rb"))) return fail(a, error, n, "cannot open archive");
#ifdef _WIN32
    if (_fseeki64(a->file, 0, SEEK_END)) return fail(a, error, n, "cannot size archive");
    int64_t file_size = _ftelli64(a->file);
#else
    if (fseeko(a->file, 0, SEEK_END)) return fail(a, error, n, "cannot size archive");
    int64_t file_size = (int64_t)ftello(a->file);
#endif
#ifdef STRIKERS_VITA
    a->fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (a->fd < 0) return fail(a, error, n, "cannot open positional reader");
#endif
    if (file_size < 32 || read_at(a, h, 32, 0) != 32) return fail(a, error, n, "truncated header");
    if (memcmp(h, "PSAR", 4) || be(h + 4, 4) != 0x10004 || memcmp(h + 8, "zlib", 4) ||
        be(h + 16, 4) != 30 || be(h + 24, 4) != BLOCK_SIZE || be(h + 28, 4) > 1)
        return fail(a, error, n, "unsupported version, codec, flags or block size");
    uint64_t toc = be(h + 12, 4), count = be(h + 20, 4);
    uint64_t entries_end = 32 + count * 30;
    if (!count || count > 65536 || toc < entries_end || toc > MAX_TOC || toc > (uint64_t)file_size ||
        (toc - entries_end) % 2) return fail(a, error, n, "invalid table size");
    a->count = (unsigned)count; a->blocks = (unsigned)((toc - entries_end) / 2);
    a->ignore_case = (int)be(h + 28, 4);
    a->entries = (ArchiveEntry*)calloc(a->count, sizeof *a->entries);
    a->lengths = (unsigned short*)calloc(a->blocks ? a->blocks : 1, sizeof *a->lengths);
    a->offsets = (uint64_t*)malloc((a->blocks ? a->blocks : 1) * sizeof *a->offsets);
    a->paths = (char**)calloc(a->count, sizeof *a->paths);
    a->names = (ArchiveName*)calloc(a->count, sizeof *a->names);
    a->toc = (unsigned char*)malloc((size_t)toc - 32);
    if (!a->entries || !a->lengths || !a->offsets || !a->paths || !a->names || !a->toc)
        return fail(a, error, n, "out of memory");
    if (read_at(a, a->toc, (size_t)toc - 32, 32) != toc - 32)
        return fail(a, error, n, "truncated TOC");
    for (unsigned k = 0; k < a->blocks; ++k) {
        const unsigned char* t = a->toc + (size_t)entries_end - 32 + k * 2u;
        a->lengths[k] = (unsigned short)be(t, 2); a->offsets[k] = UINT64_MAX;
    }
    for (unsigned i = 0; i < a->count; ++i) {
        const unsigned char* t = a->toc + i * 30u;
        if (!i) for (unsigned j = 0; j < 16; ++j)
            if (t[j]) return fail(a, error, n, "invalid manifest entry hash");
        ArchiveEntry* e = &a->entries[i];
        e->block = (unsigned)be(t + 16, 4); e->size = be(t + 20, 5); e->offset = be(t + 25, 5);
        uint64_t blocks = (e->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        if (e->offset < toc || e->offset > (uint64_t)file_size || e->block > a->blocks ||
            blocks > a->blocks - e->block) return fail(a, error, n, "entry outside archive or block table");
        e->stored = 1;
        uint64_t pos = e->offset, left = e->size;
        for (unsigned k = 0; k < (unsigned)blocks; ++k) {
            unsigned b = e->block + k, physical = stored_size(a, b);
            unsigned logical = left < BLOCK_SIZE ? (unsigned)left : BLOCK_SIZE;
            if (physical > logical || physical > (uint64_t)file_size - pos ||
                (a->offsets[b] != UINT64_MAX && a->offsets[b] != pos))
                return fail(a, error, n, "invalid block extent or alias");
            if (physical < logical) {
#ifndef STRIKERS_ZLIB
                return fail(a, error, n, "compressed block requires a zlib build");
#endif
                e->stored = 0;
            }
            a->offsets[b] = pos; pos += physical; left -= logical;
        }
    }
    const uint64_t manifest_size = a->entries[0].size;
    if (manifest_size > MAX_MANIFEST) return fail(a, error, n, "manifest too large");
    a->manifest = (char*)malloc((size_t)manifest_size + 1);
    if (!a->manifest) return fail(a, error, n, "out of memory");
    if (read_entry(a, 0, a->manifest, (size_t)manifest_size, 0) != (long)manifest_size)
        return fail(a, error, n, "cannot read manifest");
    if (memchr(a->manifest, 0, (size_t)manifest_size)) return fail(a, error, n, "NUL in path manifest");
    a->manifest[manifest_size] = 0;
    unsigned names = 0;
    char* start = a->manifest;
    if (manifest_size) for (char* p = start;; ++p) {
        if (*p == '\n' || !*p) {
            int done = !*p; *p = 0;
            if (names >= a->count - 1 || !safe_path(start)) return fail(a, error, n, "invalid manifest path/count");
            a->paths[names++] = start;
            if (done) break;
            start = p + 1;
        }
    }
    if (names != a->count - 1) return fail(a, error, n, "manifest/table count mismatch");
    for (unsigned i = 0; i < names; ++i) { a->names[i].path = a->paths[i]; a->names[i].index = i; }
    qsort(a->names, names, sizeof *a->names, a->ignore_case ? compare_names_fold : compare_names_exact);
    for (unsigned i = 1; i < names; ++i)
        if (!compare_path(a->names[i - 1].path, a->names[i].path, a->ignore_case))
            return fail(a, error, n, "duplicate manifest path");
    free(a->toc); a->toc = NULL;
    if (error && n) error[0] = 0;
    return a;
}
