#pragma once
#include "port/asset_archive.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace port {
// Cumulative counters include startup. Adjacent row differences describe live
// gameplay. Simultaneous reader timings overlap and cannot be a frame budget.
inline void capture_asset_io(bool livePlay) {
    static const char* path = std::getenv("STRIKERS_ASSET_IO_CAPTURE");
    static const char* enabled = std::getenv("STRIKERS_ASSET_IO_PROFILE");
    if (!path || !*path || !enabled || std::strcmp(enabled, "1") || !livePlay) return;
    const auto number = [](const char* name, unsigned fallback, unsigned maximum) {
        const char* value = std::getenv(name);
        if (!value || !*value) return fallback;
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(value, &end, 10);
        return end != value && !*end && parsed <= maximum ? unsigned(parsed) : fallback;
    };
    static const unsigned skip = number("STRIKERS_ASSET_IO_CAPTURE_SKIP", 600, 36000);
    static const unsigned limit = number("STRIKERS_ASSET_IO_CAPTURE_FRAMES", 120, 3600);
    static unsigned seen = 0;
    static bool written = false;
    static std::vector<PortAssetIoStats> rows;
    if (written || !limit) return;
    if (rows.empty()) rows.reserve(limit);
    if (seen++ < skip) return;
    PortAssetIoStats row = {};
    port_asset_io_snapshot(&row);
    rows.push_back(row);
    if (rows.size() < limit) return;
    written = true;
    FILE* file = std::fopen(path, "wb");
    if (file) {
        std::fprintf(file, "# strikers-asset-io-v1 cumulative=1 live_play=1 warmup=%u samples=%u; read_us measures API elapsed; concurrent read/inflate timings overlap\n", skip, limit);
        std::fprintf(file, "live_frame,requests,requested_bytes,physical_reads,physical_bytes,read_us,read_errors,inflates,inflated_bytes,inflate_us,inflate_errors\n");
        for (unsigned i = 0; i < limit; ++i) {
            const auto& s = rows[i];
            std::fprintf(file, "%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", skip + i,
                (unsigned long long)s.requests, (unsigned long long)s.requested_bytes,
                (unsigned long long)s.physical_reads, (unsigned long long)s.physical_bytes,
                (unsigned long long)s.read_us, (unsigned long long)s.read_errors,
                (unsigned long long)s.inflates, (unsigned long long)s.inflated_bytes,
                (unsigned long long)s.inflate_us, (unsigned long long)s.inflate_errors);
        }
        std::fclose(file);
    }
    rows.clear(); rows.shrink_to_fit();
}
}
