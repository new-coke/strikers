// Portable PSARC 1.4 reader. No game, GX, FIOS or proprietary SDK dependency.
#ifndef PORT_ASSET_ARCHIVE_H
#define PORT_ASSET_ARCHIVE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PortAssetArchive PortAssetArchive;
// Supports zlib-tagged 64 KiB archives, stored blocks and optional zlib blocks.
// Compressed blocks require STRIKERS_ZLIB; stored archives need no dependency.
PortAssetArchive* port_asset_archive_open(const char* path, char* error, size_t error_size);
void port_asset_archive_close(PortAssetArchive* archive);
unsigned port_asset_archive_count(const PortAssetArchive* archive);
const char* port_asset_archive_path(const PortAssetArchive* archive, unsigned index);
uint64_t port_asset_archive_size(const PortAssetArchive* archive, unsigned index);
int port_asset_archive_find(const PortAssetArchive* archive, const char* path);
// Positional, bounded to the logical file. Returns bytes read, 0 at EOF, -1 on error.
long port_asset_archive_read(PortAssetArchive* archive, unsigned index, void* dst,
                            size_t bytes, uint64_t offset);
// Optional process-wide observations for archives opened with
// STRIKERS_ASSET_IO_PROFILE=1. Snapshot counters only; never writes a log.
// I/O and inflate times are CPU-observed elapsed time, not device busy time.
typedef struct PortAssetIoStats {
    uint64_t requests, requested_bytes;
    uint64_t physical_reads, physical_bytes, read_us, read_errors;
    uint64_t inflates, inflated_bytes, inflate_us, inflate_errors;
} PortAssetIoStats;
void port_asset_io_snapshot(PortAssetIoStats* out);
#ifdef __cplusplus
}
#endif
#endif
