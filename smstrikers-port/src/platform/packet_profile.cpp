#if defined(STRIKERS_VITA_NATIVE_RUNTIME) || defined(STRIKERS_PACKET_PROFILE_TEST)
#include "port/packet_profile.hpp"
#include "port/host.h"
#include <cerrno>
#include <cstdio>
#include <initializer_list>
#include <numeric>

namespace port::render
{
void PacketProfile::Stats::add(const Stats& other) noexcept
{
    for (size_t i = 0; i < ns.size(); ++i) ns[i] += other.ns[i];
    for (size_t i = 0; i < counts.size(); ++i) counts[i] += other.counts[i];
}
PacketProfile::Scope::Scope(PacketProfile& profile, unsigned v, uint32_t references, Wait w) noexcept
{
    if (!profile.recording()) return;
    owner = &profile;
    netSource = 2;
    netEligible = netFast = netFallback = false;
    view = v < Views - 1 ? v : Views - 1;
    wait = w;
    initialWait = wait ? wait() : 0;
    stats.emplace();
    stats->counts[Attempts] = 1;
    stats->counts[InputReferences] = references;
    previous = port_monotonic_ns();
}
void PacketProfile::Scope::next(Phase nextPhase) noexcept
{
    if (!owner) return;
    const auto now = port_monotonic_ns();
    if (phase == Material)
        owner->currentMaterials.ns[materialPart] += now - materialPrevious;
    stats->ns[phase] += now - previous;
    previous = now;
    phase = nextPhase;
    if (phase == Material)
    {
        materialPrevious = now;
        materialPart = ViewportsMatrices;
        ++owner->currentMaterials.counts[MaterialAttempts];
    }
}
void PacketProfile::Scope::material_next(MaterialPart nextPart) noexcept
{
    if (!owner || phase != Material) return;
    const auto now = port_monotonic_ns();
    owner->currentMaterials.ns[materialPart] += now - materialPrevious;
    materialPrevious = now;
    materialPart = nextPart;
}
void PacketProfile::Scope::material_count(MaterialCounter counter, uint64_t count) noexcept
{
    if (owner && phase == Material) owner->currentMaterials.counts[counter] += count;
}
void PacketProfile::Scope::submitted(uint32_t model, size_t vertices, size_t indices) noexcept
{
    if (!owner) return;
    stats->counts[Accepted] = 1;
    ++stats->counts[model ? (vertices ? ResidentBuilds : ResidentHits) : DynamicDraws];
    stats->counts[DecodedVertices] = vertices;
    stats->counts[DecodedIndices] = indices;
    if (!model)
    {
        stats->counts[VertexBytes] = vertices * sizeof(ModelVertex);
        stats->counts[IndexBytes] = indices * sizeof(uint16_t);
    }
}
void PacketProfile::Scope::net_source(unsigned source) noexcept
{
    if (owner && owner->netCensus && source < 2) netSource = source;
}
void PacketProfile::Scope::net_route(bool eligible, bool fast, bool fallback) noexcept
{
    if (!owner || netSource >= 2) return;
    netEligible = eligible;
    netFast = fast;
    netFallback = fallback;
}
PacketProfile::Scope::~Scope()
{
    if (!owner) return;
    next(Cleanup);
    stats->counts[Rejected] = !stats->counts[Accepted];
    const auto finalWait = wait ? wait() : initialWait;
    stats->counts[PacketWaitNs] = (finalWait - initialWait) * 1000;
    owner->current.add(*stats);
    owner->currentViews[view].add(*stats);
    if (netSource < 2)
    {
        auto& n = owner->currentNets.sources[netSource];
        ++n[NetCensus];
        n[NetEligible] += netEligible;
        n[NetFast] += netFast;
        n[NetFallback] += netFallback;
        n[NetAccepted] += stats->counts[Accepted];
        n[NetRejected] += stats->counts[Rejected];
        n[NetReferences] += stats->counts[InputReferences];
        n[NetVertices] += stats->counts[DecodedVertices];
        n[NetIndices] += stats->counts[DecodedIndices];
        n[NetGeometryNs] += stats->ns[Geometry];
    }
}
bool PacketProfile::configure(const char* capturePath, size_t requested) noexcept
{
    if (enabled()) return false;
    if (!capturePath || !*capturePath || !requested || requested > 8192) return false;
    try
    {
        path = capturePath;
        frames.reserve(requested);
        goal = requested;
        return true;
    }
    catch (...) { error = ENOMEM; return false; }
}
bool PacketProfile::configure_nets(bool conversionEnabled) noexcept
{
    if (!enabled() || armed || finished || netCensus || !frames.empty()) return false;
    netCensus = true;
    netConversion = conversionEnabled;
    return true;
}
void PacketProfile::net_registered(unsigned source) noexcept
{
    if (armed && netCensus && source < 2) ++currentNets.registered[source];
}
void PacketProfile::net_tag_failure(bool overflow) noexcept
{
    if (!armed || !netCensus) return;
    if (overflow) ++currentNets.tagOverflow;
    else ++currentNets.tagMismatch;
}
void PacketProfile::begin_frame(const PortBenchCaptureStatus& status) noexcept
{
    armed = enabled() && !finished && status.state == PORT_CAPTURE_COLLECTING && !status.warmupRemaining;
    if (!armed) return;
    before = status.samples;
    current = {};
    currentViews = {};
    currentNets = {};
    currentTimeline = {};
    currentMaterials = {};
    currentNets.valid = netCensus;
    currentNets.enabled = netConversion;
}
void PacketProfile::end_frame(const PortBenchCaptureStatus& status, uint32_t engineFrame,
                              bool discarded, const Renderer::FrameTiming& timing) noexcept
{
    const bool selected = armed && status.samples == before + 1;
    armed = false;
    if (!selected || finished) return;
    // The benchmark owns eligibility and logical-frame identity. A missed
    // sample is an error, never silently replaced by an intro/replay frame.
    if (before != frames.size() || frames.size() >= goal)
    {
        error = EINVAL;
        finished = true;
        return;
    }
    frames.push_back(Frame{before, status.lastFrame, engineFrame, discarded, current, timing, currentNets,
                           currentTimeline, currentMaterials});
    for (size_t i = 0; i < Views; ++i) totals[i].add(currentViews[i]);
    if (frames.size() == goal)
    {
        finished = true;
        write();
    }
}
namespace
{
constexpr const char* columns =
    "packet_ns,invalidation_ns,arrays_ns,skin_ns,material_ns,geometry_ns,submit_ns,cleanup_ns,"
    "attempts,accepted,rejected,resident_hits,resident_builds,dynamic_draws,input_references,"
    "decoded_vertices,decoded_indices,vertex_bytes,index_bytes,packet_wait_ns";
bool stats(FILE* f, const PacketProfile::Stats& s)
{
    if (std::fprintf(f, "%llu", (unsigned long long)std::accumulate(s.ns.begin(), s.ns.end(), uint64_t{})) < 0) return false;
    for (auto n : s.ns) if (std::fprintf(f, ",%llu", (unsigned long long)n) < 0) return false;
    for (auto n : s.counts) if (std::fprintf(f, ",%llu", (unsigned long long)n) < 0) return false;
    return true;
}
bool identity(FILE* f, const PacketProfile::Frame& row)
{
    return std::fprintf(f, "%u,%lu,%u,%u", unsigned(row.sample), row.liveFrame,
                        row.engineFrame, unsigned(row.discarded)) >= 0;
}
bool values(FILE* f, std::initializer_list<uint64_t> list)
{
    for (auto value : list)
        if (std::fprintf(f, ",%llu", (unsigned long long)value) < 0) return false;
    return true;
}
template <class Writer> bool output(const std::string& path, Writer writer)
{
    const auto temp = path + ".tmp";
    FILE* f = std::fopen(temp.c_str(), "wb");
    if (!f) return false;
    bool ok = writer(f);
    if (std::fclose(f)) ok = false;
    if (!ok) { std::remove(temp.c_str()); return false; }
    if (std::rename(temp.c_str(), path.c_str())) { std::remove(temp.c_str()); return false; }
    return true;
}
}
bool PacketProfile::write() noexcept
{
    if (!enabled() || frames.size() != goal) return false;
    try
    {
        errno = 0;
        const bool uniforms = frames.front().renderer.uniforms.valid;
        const bool vertexPrograms = frames.front().renderer.vertexPrograms.valid;
        const bool vertexStreams = frames.front().renderer.vertexStreams.valid;
        const bool nets = frames.front().nets.valid;
        const bool timeline = frames.front().timeline.producer.observed;
        for (const auto& row : frames)
            if (row.renderer.uniforms.valid != uniforms || row.renderer.vertexPrograms.valid != vertexPrograms ||
                row.renderer.vertexStreams.valid != vertexStreams ||
                row.nets.valid != nets ||
                row.timeline.producer.observed != timeline ||
                (nets && row.nets.enabled != frames.front().nets.enabled) ||
                (vertexPrograms && row.renderer.vertexPrograms.enabled != frames.front().renderer.vertexPrograms.enabled) ||
                (vertexStreams && row.renderer.vertexStreams.enabled != frames.front().renderer.vertexStreams.enabled))
            {
                error = EINVAL;
                return false;
            }
        bool ok = output(path + ".packets.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-packet-profile-v1 attribution_only=1 waits_are_subsets=1\n"
                "sample,live_frame,engine_frame,discarded,%s,owner_draw_us,owner_present_us,frontend_wait_us,"
                "command_bytes,command_copy_ns,command_vertex_bytes,command_index_bytes,slot_waits,renderer_draws\n", columns) < 0) return false;
            for (const auto& row : frames)
            {
                // Vita's formatter lacks %z. Capture and view indices are
                // bounded by configure's 8192 samples and the 35 view slots.
                if (std::fprintf(f, "%u,%lu,%u,%u,", unsigned(row.sample), row.liveFrame, row.engineFrame, unsigned(row.discarded)) < 0 || !stats(f, row.producer)) return false;
                const auto& r = row.renderer;
                if (std::fprintf(f, ",%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u\n",
                    (unsigned long long)r.drawUs, (unsigned long long)r.queueUs,
                    (unsigned long long)r.frontendWaitUs, (unsigned long long)r.commandBytes,
                    (unsigned long long)r.commandCopyNs, (unsigned long long)r.commandVertexBytes,
                    (unsigned long long)r.commandIndexBytes, r.slotWaits, r.draws) < 0) return false;
            }
            return true;
        });
        if (ok) ok = output(path + ".views.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-packet-profile-views-v1 samples=%u\nview,%s\n", unsigned(frames.size()), columns) < 0) return false;
            for (size_t i = 0; i < Views; ++i)
                if (totals[i].counts[Attempts] && (std::fprintf(f, "%u,", unsigned(i)) < 0 || !stats(f, totals[i]) || std::fputc('\n', f) == EOF)) return false;
            return true;
        });
        if (ok && uniforms) ok = output(path + ".uniforms.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-uniform-profile-v1 quantities_only=1\n"
                "sample,live_frame,engine_frame,discarded,cache_enabled,"
                "vs_pack_calls,vs_pack_bytes,vs_reservations,vs_reserved_bytes,vs_cache_copies,vs_cache_copy_bytes,vs_reused_values,vs_reused_bytes,"
                "fs_pack_calls,fs_pack_bytes,fs_reservations,fs_reserved_bytes,fs_cache_copies,fs_cache_copy_bytes,fs_reused_values,fs_reused_bytes,"
                "fallback_uploads,errors,cache_bytes,cache_peak_bytes\n") < 0) return false;
            for (const auto& row : frames)
            {
                const auto& u = row.renderer.uniforms;
                if (std::fprintf(f, "%u,%lu,%u,%u,%u", unsigned(row.sample), row.liveFrame,
                    row.engineFrame, unsigned(row.discarded), unsigned(u.enabled)) < 0) return false;
                for (const auto& stage : u.stage)
                    for (auto n : stage)
                        if (std::fprintf(f, ",%llu", (unsigned long long)n) < 0) return false;
                if (std::fprintf(f, ",%llu,%llu,%llu,%llu\n", (unsigned long long)u.fallbackUploads,
                    (unsigned long long)u.errors, (unsigned long long)u.cacheBytes,
                    (unsigned long long)u.cachePeakBytes) < 0) return false;
            }
            return true;
        });
        if (ok && vertexPrograms) ok = output(path + ".vertex_programs.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-vertex-program-profile-v1 quantities_only=1\n"
                "sample,live_frame,engine_frame,discarded,cache_enabled,"
                "arrays_requests,elements_requests,multi_requests,create_calls,create_successes,cache_hits,"
                "cache_misses,uncached_requests,errors,metadata_alloc_failures,metadata_fallbacks,"
                "cache_bytes,cache_peak_bytes\n") < 0) return false;
            for (const auto& row : frames)
            {
                const auto& v = row.renderer.vertexPrograms;
                if (std::fprintf(f, "%u,%lu,%u,%u,%u", unsigned(row.sample), row.liveFrame,
                    row.engineFrame, unsigned(row.discarded), unsigned(v.enabled)) < 0) return false;
                for (auto n : v.counters)
                    if (std::fprintf(f, ",%llu", (unsigned long long)n) < 0) return false;
                if (std::fprintf(f, ",%llu,%llu\n", (unsigned long long)v.cacheBytes,
                    (unsigned long long)v.cachePeakBytes) < 0) return false;
            }
            return true;
        });
        if (ok && vertexStreams) ok = output(path + ".vertex_streams.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-vertex-stream-profile-v1 quantities_only=1\n"
                "sample,live_frame,engine_frame,discarded,interleaved_enabled,"
                "elements_requests,eligible_requests,collapsed_requests,fallback_requests,source_streams,binding_calls\n") < 0) return false;
            for (const auto& row : frames)
            {
                const auto& v = row.renderer.vertexStreams;
                if (std::fprintf(f, "%u,%lu,%u,%u,%u", unsigned(row.sample), row.liveFrame,
                    row.engineFrame, unsigned(row.discarded), unsigned(v.enabled)) < 0) return false;
                for (auto n : v.counters)
                    if (std::fprintf(f, ",%llu", (unsigned long long)n) < 0) return false;
                if (std::fputc('\n', f) == EOF) return false;
            }
            return true;
        });
        if (ok && nets) ok = output(path + ".nets.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-net-profile-v1 geometry_is_subset=1 source_tags=constructor\n"
                "sample,live_frame,engine_frame,discarded,conversion_enabled,tag_overflow,tag_mismatch,"
                "net0_registered,net0_census,net0_eligible,net0_fast,net0_fallback,net0_accepted,net0_rejected,"
                "net0_input_references,net0_decoded_vertices,net0_decoded_indices,net0_geometry_ns,"
                "net1_registered,net1_census,net1_eligible,net1_fast,net1_fallback,net1_accepted,net1_rejected,"
                "net1_input_references,net1_decoded_vertices,net1_decoded_indices,net1_geometry_ns\n") < 0) return false;
            for (const auto& row : frames)
            {
                const auto& n = row.nets;
                if (std::fprintf(f, "%u,%lu,%u,%u,%u,%llu,%llu", unsigned(row.sample), row.liveFrame,
                    row.engineFrame, unsigned(row.discarded), unsigned(n.enabled),
                    (unsigned long long)n.tagOverflow, (unsigned long long)n.tagMismatch) < 0) return false;
                for (unsigned source = 0; source < n.sources.size(); ++source)
                {
                    if (std::fprintf(f, ",%llu", (unsigned long long)n.registered[source]) < 0) return false;
                    for (auto value : n.sources[source])
                        if (std::fprintf(f, ",%llu", (unsigned long long)value) < 0) return false;
                }
                if (std::fputc('\n', f) == EOF) return false;
            }
            return true;
        });
        if (ok && timeline) ok = output(path + ".timeline.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-frame-timeline-v1 clock=system_time_ns owner_waits_are_subsets=1 max_intervals=32\n"
                "sample,live_frame,engine_frame,discarded,"
                "producer_begin_request_ns,producer_begin_return_ns,first_draw_ready_ns,first_draw_publish_ns,producer_end_request_ns,producer_end_return_ns,"
                "producer_batches,draw_batches,producer_draw_commands,borrowed_draw_commands,begin_succeeded,end_succeeded,producer_cancelled,"
                "owner_begin_start_ns,owner_begin_end_ns,first_draw_command_ns,last_draw_command_end_ns,owner_end_start_ns,owner_end_end_ns,"
                "owner_batches,owner_draw_commands,failed_draw_commands,ready_wait_calls,stored_waits,wait_overflow,ready_wait_ns,max_ready_wait_ns,"
                "draw_timing_enabled,draw_prepare_ns,draw_api_ns,draw_suffix_ns,timed_draw_scopes,present_requested,present_prefix_ns,present_swap_ns,present_suffix_ns,timed_present_scopes,owner_cancelled,"
                "display_add_entry_ns,display_add_entry_calls,display_add_entry_errors,display_add_entry_last_result,gc_ns,gc_calls,gc_errors,gc_last_result\n") < 0) return false;
            for (const auto& row : frames)
            {
                const auto& p = row.timeline.producer;
                const auto& o = row.timeline.owner;
                const auto& v = o.provider;
                if (!identity(f, row) || !values(f, {
                    p.beginRequestNs, p.beginReturnNs, p.firstDrawReadyNs, p.firstDrawPublishNs, p.endRequestNs, p.endReturnNs,
                    p.batches, p.drawBatches, p.drawCommands, p.borrowedDrawCommands, p.beginSucceeded, p.endSucceeded, p.cancelled,
                    o.beginStartNs, o.beginEndNs, o.firstDrawCommandNs, o.lastDrawCommandEndNs, o.endStartNs, o.endEndNs,
                    o.batches, o.drawCommands, o.failedDrawCommands, o.waitCalls, o.storedWaits, o.waitOverflow, o.readyWaitNs, o.maxReadyWaitNs,
                    o.drawTimingEnabled, o.drawPrepareNs, o.drawApiNs, o.drawSuffixNs, o.timedDrawScopes,
                    o.presentRequested, o.presentPrefixNs, o.presentSwapNs, o.presentSuffixNs, o.timedPresentScopes, o.cancelled,
                    v.addEntryNs, v.addEntryCalls, v.addEntryErrors})) return false;
                if (std::fprintf(f, ",%d", int(v.lastAddEntryResult)) < 0 ||
                    !values(f, {v.gcNs, v.gcCalls, v.gcErrors}) ||
                    std::fprintf(f, ",%d\n", int(v.lastGcResult)) < 0) return false;
            }
            return true;
        });
        if (ok && timeline) ok = output(path + ".timeline_waits.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-frame-timeline-waits-v1 clock=system_time_ns max_intervals=32\n"
                "sample,live_frame,engine_frame,discarded,interval,begin_ns,end_ns\n") < 0) return false;
            for (const auto& row : frames)
                for (unsigned i = 0; i < row.timeline.owner.storedWaits; ++i)
                {
                    const auto& wait = row.timeline.owner.waits[i];
                    if (!identity(f, row) || !values(f, {i, wait.beginNs, wait.endNs}) ||
                        std::fputc('\n', f) == EOF) return false;
                }
            return true;
        });
        if (ok) ok = output(path + ".materials.csv", [&](FILE* f) {
            if (std::fprintf(f, "# strikers-material-profile-v1 children_partition_material=1 counts_from_existing_operations=1\n"
                "sample,live_frame,engine_frame,discarded,viewports_matrices_ns,state_lighting_ns,texture_resolution_ns,"
                "material_attempts,viewport_overrides,viewport_matrix_overrides,model_concats,view_only_matrices,invalid_matrices,"
                "point_lights,directional_lights,specular_lights,texture_requests,texture_resolutions,texture_failures\n") < 0) return false;
            for (const auto& row : frames)
            {
                if (!identity(f, row)) return false;
                for (auto value : row.materials.ns)
                    if (!values(f, {value})) return false;
                for (auto value : row.materials.counts)
                    if (!values(f, {value})) return false;
                if (std::fputc('\n', f) == EOF) return false;
            }
            return true;
        });
        error = ok ? 0 : (errno ? errno : EIO);
    }
    catch (...) { error = ENOMEM; }
    std::fprintf(stderr, "[packet-profile] samples=%u output_error=%d path=%s\n", unsigned(frames.size()), error, path.c_str());
    return !error;
}
} // namespace port::render
#endif
