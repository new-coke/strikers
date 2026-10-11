// Exercise the real reader AND DVD adapter against host files supplied by Python.
#undef NDEBUG
#include <assert.h>
#include <stdarg.h>
#include <strings.h>
#define strcmpi strcasecmp
#define TARGET_PC 1
#include "../src/platform/dvd.c"

void OSReport(const char* fmt, ...) { (void)fmt; }
int PortConfigLoad(void) { return 0; }
void PortSetDiscGameName(const char* code) { (void)code; }
int port_executable_dir(char* b, size_t n) { (void)b; (void)n; return -1; }
int port_scan_dir(const char* p, void (*v)(void*, const char*), void* u)
{ (void)p; (void)v; (void)u; return -1; }
void port_fatal(const char* t, const char* m) { fprintf(stderr, "%s: %s\n", t, m); exit(86); }
PortDisc* port_disc_open(const char* p, char* e, size_t n) { (void)p; (void)e; (void)n; return NULL; }
long port_disc_read(PortDisc* d, void* b, size_t n, unsigned long long o)
{ (void)d; (void)b; (void)n; (void)o; return -1; }
const char* port_disc_format(const PortDisc* d) { (void)d; return "raw"; }
int port_disc_looks_like_image(const char* p) { (void)p; return 0; }
int port_disc_walk(PortDisc* d, PortDiscVisit v, void* u, char* e, size_t n)
{ (void)d; (void)v; (void)u; (void)e; (void)n; return -1; }
unsigned long long port_monotonic_ns(void) { return 0; }
static int calls, result;
static void completed(s32 r, DVDFileInfo* f) { (void)f; ++calls; result = r; }

int main(int argc, char** argv)
{
    assert(argc == 3 || (argc == 4 && strcmp(argv[3], "--reader-only") == 0));
    char err[2048];
    PortAssetArchive* a = port_asset_archive_open(argv[1], err, sizeof err);
    if (strcmp(argv[2], "--reject") == 0) {
        if (a) { port_asset_archive_close(a); return 1; }
        assert(err[0]); return 0;
    }
    if (!a) { fprintf(stderr, "%s\n", err); return 2; }
    if (strcmp(argv[2], "--read-reject") == 0) {
        unsigned char b[100];
        int i = port_asset_archive_find(a, "files/common.ini");
        assert(i >= 0);
        int rc = port_asset_archive_read(a, (unsigned)i, b, sizeof b, 0) < 0 ? 0 : 1;
        port_asset_archive_close(a); return rc;
    }
    unsigned char actual[65573], expected[65573];
    for (unsigned i = 0; i < port_asset_archive_count(a); ++i) {
        const char* name = port_asset_archive_path(a, i);
        assert(port_asset_archive_find(a, name) == (int)i);
        uint64_t size = port_asset_archive_size(a, i);
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", argv[2], name);
        FILE* source = fopen(path, "rb"); assert(source);
        for (uint64_t at = 0; at < size; at += sizeof actual) {
            size_t n = size - at < sizeof actual ? (size_t)(size - at) : sizeof actual;
            assert(fread(expected, 1, n, source) == n);
            assert(port_asset_archive_read(a, i, actual, n, at) == (long)n);
            assert(memcmp(actual, expected, n) == 0);
        }
        if (size) {
            uint64_t at = size > 17 ? size - 17 : 0;
            size_t n = (size_t)(size - at);
            memset(actual, 0x88, sizeof actual);
            assert(port_asset_archive_read(a, i, actual, 64, at) == (long)n);
            assert(fseeko(source, (off_t)at, SEEK_SET) == 0);
            assert(fread(expected, 1, n, source) == n);
            assert(memcmp(actual, expected, n) == 0 && actual[n] == 0x88);
        }
        assert(port_asset_archive_read(a, i, actual, 64, size) == 0);
        assert(port_asset_archive_read(a, i, NULL, 0, 0) == 0);
        assert(port_asset_archive_read(a, i, NULL, 1, 0) == -1);
        fclose(source);
    }
    {
        PortAssetIoStats stats;
        port_asset_io_snapshot(&stats);
        const char* profile = getenv("STRIKERS_ASSET_IO_PROFILE");
        if (profile && strcmp(profile, "1") == 0) {
            assert(stats.requests > 0 && stats.physical_reads >= 3);
            assert(stats.physical_bytes > 32 && stats.requested_bytes > 0);
            assert(stats.read_errors == 0 && stats.inflate_errors == 0);
        } else {
            const PortAssetIoStats zero = {0};
            assert(memcmp(&stats, &zero, sizeof stats) == 0);
        }
    }
    port_asset_archive_close(a);
    if (argc == 4) {
        puts("PSARC sidecar byte equivalence and random reads passed");
        return 0;
    }
    setenv("STRIKERS_ASSET_ARCHIVE", argv[1], 1);
    DVDInit();
    assert(memcmp(DVDGetCurrentDiskID(), "G4QP01", 6) == 0);
    assert(DVDConvertPathToEntrynum("/COMMON.INI") >= 0);
    for (int i = 0; i < s_count; ++i) {
        DVDFileInfo file;
        assert(DVDFastOpen(i, &file));
        if (!file.length) continue;
        int at = file.length > 17 ? (int)file.length - 17 : 0;
        unsigned n = file.length - (unsigned)at;
        calls = 0; memset(actual, 0x88, sizeof actual);
        assert(DVDReadAsyncPrio(&file, actual, 32, at, completed, 0));
        assert(calls == 1 && result == (int)n && file.cb.transferredSize == n);
        assert(DVDGetCommandBlockStatus(&file.cb) == DVD_STATE_END);
        snprintf(err, sizeof err, "%s/files/%s", argv[2], s_entries[i].path);
        // Host files can have mixed case. Compare against the archive's canonical name.
        const char* canonical = port_asset_archive_path(s_archive, s_entries[i].offset);
        char path[4096]; snprintf(path, sizeof path, "%s/%s", argv[2], canonical);
        FILE* source = fopen(path, "rb"); assert(source);
        assert(fseek(source, at, SEEK_SET) == 0 && fread(expected, 1, n, source) == n);
        assert(memcmp(actual, expected, n) == 0 && actual[n] == 0x88);
        fclose(source);
        calls = 0;
        assert(!DVDReadAsyncPrio(&file, actual, 32, (s32)file.length, completed, 0));
        assert(calls == 1 && result == -1 && file.cb.state == DVD_STATE_FATAL_ERROR);
        assert(DVDClose(&file));
    }
    port_asset_archive_close(s_archive);
    puts("PSARC byte equivalence, random reads and DVD callbacks passed");
    return 0;
}
