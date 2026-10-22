#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_avc_continuity_probe.hpp"
#if defined(STRIKERS_VITA_AVC_CONTINUITY_PROBE)
#include "port/avc_continuity_probe_core.hpp"
#include "port/native_boot_screen.hpp"
#include "native_avc_probe_owner_io.hpp"
#include "native_avc_continuity_inputs.inc"
#include <psp2/sysmodule.h>
#include <psp2/power.h>
#include <cstdlib>
#include <new>

namespace
{
namespace core = avc_continuity_probe;
using namespace avc_probe_owner;
using namespace avc_probe_inputs;
static_assert(sizeof(SceAvcdecCtrl) == 12 && sizeof(SceAvcdecAu) == 24 &&
              sizeof(SceAvcdecPicture) == 100 && sizeof(SceAvcdecArrayPicture) == 12,
              "The probe requires the bound Vita 32-bit ABI");
static_assert(core::Inputs == 3 && core::Width == 512 && core::Height == 416 &&
              core::Pitch == 512 && core::MaxPictureCalls == 7, "Frozen kernel contract");

struct Step
{
    const char* name = nullptr;
    bool called = false, returned = false;
    int rc = 0;
    uint64_t begin_us = 0, end_us = 0;
};
struct Owner
{
    SceAvcdecCtrl control{};
    SceVideodecQueryInitInfoHwAvcdec init{sizeof(init), 512, 416, 1, 1};
    SceAvcdecQueryDecoderInfo query{512, 416, 1};
    SceAvcdecDecoderInfo info{};
    SceUID reference = -1, raster = -1;
    uint8_t* pixels = nullptr;
    unsigned reference_bytes = 0, steps_used = 0;
    std::array<Step, 24> steps{};
    bool started = false, module_preexisting = false, module_owned = false, library = false;
    bool create_attempted = false, created = false, delete_confirmed = false;
    bool ownership_uncertain = false, cleanup_attempted = false, cleanup_complete = false;
    const char* error = nullptr;

    template<class Call> int call(IO& io, const char* name, Call function, bool cleanup = false) noexcept
    {
        if (steps_used >= steps.size()) { error = "owner step bound"; return -7101; }
        auto& step = steps[steps_used++];
        step.name = name;
        const bool logged = io.note(name, "intent", 0);
        if ((!logged || !io.live()) && !cleanup)
        { error = "owner journal/deadline"; return -7102; }
        step.called = true; step.begin_us = Guardian::now();
        step.rc = function();
        step.end_us = Guardian::now(); step.returned = true;
        io.note(name, "returned", step.rc);
        return step.rc;
    }
    bool fail(const char* why) noexcept { if (!error) error = why; return false; }
    bool open(IO& io) noexcept
    {
        if (started) return fail("owner reused");
        started = true;
        const int loaded = call(io, "module_is_loaded", [] { return sceSysmoduleIsLoaded(SCE_SYSMODULE_AVCDEC); });
        if (loaded == 0) module_preexisting = true;
        else if (uint32_t(loaded) == uint32_t(SCE_SYSMODULE_ERROR_UNLOADED))
        {
            const int rc = call(io, "module_load", [] { return sceSysmoduleLoadModule(SCE_SYSMODULE_AVCDEC); });
            if (rc < 0) return fail("module load");
            module_owned = true;
        }
        else return fail("module status error");
        int rc = call(io, "library_init", [&] { return sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC, &init); });
        if (rc < 0) return fail("library init");
        library = true;
        rc = call(io, "memory_query", [&] {
            return sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, &query, &info);
        });
        if (rc < 0 || !info.frameMemSize || info.frameMemSize > 32u * MiB) return fail("reference query/bound");
        reference_bytes = (info.frameMemSize + MiB - 1u) & ~(MiB - 1u);
        rc = call(io, "reference_alloc", [&] {
            return sceKernelAllocMemBlock("strikers_avc_probe_ref",
                SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, reference_bytes, nullptr);
        });
        if (rc < 0) return fail("reference allocation");
        reference = rc; control.frameBuf.size = reference_bytes;
        rc = call(io, "reference_base", [&] { return sceKernelGetMemBlockBase(reference, &control.frameBuf.pBuf); });
        if (rc < 0 || !control.frameBuf.pBuf) return fail("reference base");
        rc = call(io, "raster_alloc", [] {
            return sceKernelAllocMemBlock("strikers_avc_probe_nv12",
                SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, MiB, nullptr);
        });
        if (rc < 0) return fail("raster allocation");
        raster = rc;
        void* base = nullptr;
        rc = call(io, "raster_base", [&] { return sceKernelGetMemBlockBase(raster, &base); });
        if (rc < 0 || !base) return fail("raster base");
        pixels = static_cast<uint8_t*>(base);
        rc = call(io, "decoder_create", [&] {
            create_attempted = true;
            return sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC, &control, &query);
        });
        if (rc < 0)
        {
            // No assumption that a failed Create partially published no state.
            ownership_uncertain = create_attempted;
            return fail("create failed; ownership not assumed clear");
        }
        created = true;
        return !io.error;
    }
    bool close(IO& io) noexcept
    {
        if (cleanup_attempted) return cleanup_complete; // No repeated Delete/free/Term.
        cleanup_attempted = true;
        if (ownership_uncertain) return fail("retain storage for uncertain creation");
        if (created)
        {
            const int rc = call(io, "decoder_delete", [&] { return sceAvcdecDeleteDecoder(&control); }, true);
            if (rc < 0)
            {
                ownership_uncertain = true;
                return fail("delete failed; retain all referenced storage");
            }
            delete_confirmed = true; created = false;
        }
        bool ok = true;
        if (reference >= 0)
        {
            const int rc = call(io, "reference_free", [&] { return sceKernelFreeMemBlock(reference); }, true);
            if (rc < 0) { ok = false; fail("reference free"); } else reference = -1;
        }
        if (raster >= 0)
        {
            const int rc = call(io, "raster_free", [&] { return sceKernelFreeMemBlock(raster); }, true);
            if (rc < 0) { ok = false; fail("raster free"); } else raster = -1;
        }
        if (library)
        {
            const int rc = call(io, "library_term", [] { return sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC); }, true);
            if (rc < 0) return fail("library term; keep owned module loaded");
            library = false;
        }
        if (module_owned)
        {
            const int rc = call(io, "module_unload", [] { return sceSysmoduleUnloadModule(SCE_SYSMODULE_AVCDEC); }, true);
            if (rc < 0) { ok = false; fail("module unload"); } else module_owned = false;
        }
        cleanup_complete = ok && !created && !library && !module_owned && reference < 0 && raster < 0;
        return cleanup_complete;
    }
};

struct Clocks
{
    int arm = 0, bus = 0, gpu = 0, xbar = 0;
    static Clocks observe() noexcept
    {
        return {scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(),
                scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency()};
    }
    bool equal_valid(const Clocks& other) const noexcept
    {
        return arm > 0 && bus > 0 && gpu > 0 && xbar > 0 &&
               arm == other.arm && bus == other.bus && gpu == other.gpu && xbar == other.xbar;
    }
};
// Deliberately allocated for the entire process. No automatic destructor can
// release an AU, descriptor, result vector or guardian state after a failed join/Delete.
struct Session
{
    Guardian guardian;
    IO io;
    core::Batch batch;
    std::array<core::InputRecord, 3> records{};
    std::array<Owner, 2> owners{};
    std::array<core::Workspace, 2> work{};
    std::array<core::Result, 2> result{};
    std::array<Digest, 3> prepared{}, padded{};
    std::array<bool, 2> mode_written{{false, false}};
    std::array<Clocks, 2> clocks_before{}, clocks_after{};
    core::Comparison comparison{};
    Digest journal_hash;
    bool inputs_verified = false, input_postcheck = false;
    bool comparison_observed = false, data_match = false;
    const char* error = nullptr;
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};

const char* truth(bool value) noexcept { return value ? "true" : "false"; }
const char* mode_name(unsigned mode) noexcept { return mode == 0 ? "per-au" : "continuous"; }
const char* phase_name(core::Phase phase) noexcept
{ return phase == core::Phase::Decode ? "decode" : phase == core::Phase::Stop ? "stop" : "flush"; }

bool hash_equal(IO& io, const void* data, size_t bytes, const char* expected) noexcept
{
    Digest observed;
    return io.hash(data, bytes, observed) && !std::strcmp(observed.hex, expected);
}
bool prepare(Session& s)
{
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto& input = Inputs[i];
        if (!hash_equal(s.io, input.wavc, input.wavc_bytes, input.wavc_sha) ||
            !hash_equal(s.io, input.audio, input.audio_bytes, input.audio_sha))
            return false;
        port::NativeVideoComponent component;
        if (!port::parse_native_video(input.wavc, input.wavc_bytes, component) ||
            component.bytes != input.elementary_bytes ||
            !hash_equal(s.io, component.elementary, component.bytes, input.elementary_sha) ||
            port::video_word(input.audio + 4) != input.audio_samples)
            return false;
        auto& record = s.records[i];
        record.source_index = input.index; record.audio_sample_start = input.audio_start;
        record.wavc.assign(input.wavc, input.wavc + input.wavc_bytes);
        record.audio.assign(input.audio, input.audio + input.audio_bytes);
    }
    if (!s.batch.prepare(s.records)) return false;
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto& input = s.batch.input[i];
        if (!s.io.hash(input.elementary.data(), input.elementary_bytes, s.prepared[i]) ||
            !s.io.hash(input.elementary.data(), input.elementary.size(), s.padded[i]))
            return false;
    }
    // The product's throwing global new aborts on OOM. Allocate both modes'
    // image vectors before any CreateDecoder so the frozen kernel's resize to
    // these identical sizes cannot allocate while a decoder is live.
    if (!s.io.note("output_preallocation", "intent", 0, 1916928)) return false;
    for (auto& result : s.result)
        for (auto& image : result.images)
        { image.y.resize(core::YBytes); image.u.resize(core::CBytes); image.v.resize(core::CBytes); }
    if (!s.io.note("output_preallocation", "returned", 0, 1916928)) return false;
    s.inputs_verified = s.batch.intact();
    auto& j = s.io.json; j.reset();
    j.add("{\"schema\":\"avc-probe-inputs-v1\",\"selection_sha256\":\"%s\",\"kernel_sha256\":\"%s\","
          "\"device_asset_identity_observed\":false,\"audio_playback_exercised\":false,"
          "\"audio_channels_from_frozen_header\":2,\"audio_frequency_from_frozen_header\":32000,"
          "\"abi\":{\"ctrl\":%u,\"au\":%u,\"picture\":%u,\"array_picture\":%u,"
          "\"pointer\":%u,\"init_info\":%u,\"query_info\":%u},"
          "\"decoder_config\":{\"width\":512,\"height\":416,\"pitch\":512,\"references\":1,\"streams\":1},\"inputs\":[",
          SelectionSha, KernelSha, unsigned(sizeof(SceAvcdecCtrl)), unsigned(sizeof(SceAvcdecAu)),
          unsigned(sizeof(SceAvcdecPicture)), unsigned(sizeof(SceAvcdecArrayPicture)), unsigned(sizeof(void*)),
          unsigned(sizeof(SceVideodecQueryInitInfoHwAvcdec)), unsigned(sizeof(SceAvcdecQueryDecoderInfo)));
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto& input = Inputs[i]; const auto& prepared = s.batch.input[i];
        j.add("%s{\"source_index\":%u,\"audio_start\":%llu,\"audio_samples_per_channel\":%u,"
              "\"wavc_bytes\":%u,\"wavc_sha256\":\"%s\",\"audio_bytes\":%u,\"audio_sha256\":\"%s\","
              "\"raw_es_bytes\":%u,\"raw_es_sha256\":\"%s\",\"prepared_es_bytes\":%u,"
              "\"prepared_es_sha256\":\"%s\",\"padding_bytes\":64,\"padded_es_sha256\":\"%s\","
              "\"sps_count\":%u,\"sps_patched\":%u,\"pts\":[4294967295,4294967295],"
              "\"dts\":[4294967295,4294967295]}",
              i ? "," : "", input.index, static_cast<unsigned long long>(input.audio_start),
              input.audio_samples, unsigned(input.wavc_bytes), input.wavc_sha,
              unsigned(input.audio_bytes), input.audio_sha, unsigned(input.elementary_bytes),
              input.elementary_sha, unsigned(prepared.elementary_bytes), s.prepared[i].hex,
              s.padded[i].hex, prepared.sps.sps, prepared.sps.patched);
    }
    j.add("]}\n");
    return s.inputs_verified && s.io.json_artifact("inputs.json");
}
bool postcheck(Session& s) noexcept
{
    if (!s.batch.intact()) return false;
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto& expected = Inputs[i]; const auto& actual = s.batch.input[i];
        if (actual.original.source_index != expected.index ||
            actual.original.audio_sample_start != expected.audio_start ||
            actual.original.wavc.size() != expected.wavc_bytes ||
            actual.original.audio.size() != expected.audio_bytes ||
            !hash_equal(s.io, actual.original.wavc.data(), actual.original.wavc.size(), expected.wavc_sha) ||
            !hash_equal(s.io, actual.original.audio.data(), actual.original.audio.size(), expected.audio_sha) ||
            !hash_equal(s.io, actual.elementary.data(), actual.elementary_bytes, s.prepared[i].hex) ||
            !hash_equal(s.io, actual.elementary.data(), actual.elementary.size(), s.padded[i].hex))
            return false;
    }
    return true;
}

void owner_json(Json& j, const Owner& owner) noexcept
{
    j.add("{\"started\":%s,\"module_preexisting\":%s,\"module_owned_remaining\":%s,"
          "\"library_remaining\":%s,\"create_attempted\":%s,\"created_remaining\":%s,"
          "\"delete_confirmed\":%s,\"ownership_uncertain\":%s,\"cleanup_complete\":%s,"
          "\"reference_query_bytes\":%u,\"reference_allocation_bytes\":%u,"
          "\"raster_allocation_bytes\":1048576,\"reference_uid_remaining\":%d,\"raster_uid_remaining\":%d,\"error\":",
          truth(owner.started), truth(owner.module_preexisting), truth(owner.module_owned),
          truth(owner.library), truth(owner.create_attempted), truth(owner.created),
          truth(owner.delete_confirmed), truth(owner.ownership_uncertain), truth(owner.cleanup_complete),
          owner.info.frameMemSize, owner.reference_bytes, owner.reference, owner.raster);
    j.string(owner.error); j.add(",\"steps\":[");
    for (unsigned i = 0; i < owner.steps_used; ++i)
    {
        const auto& e = owner.steps[i];
        j.add("%s{\"op\":", i ? "," : ""); j.string(e.name);
        j.add(",\"called\":%s,\"returned\":%s,\"rc\":%d,\"begin_us\":%llu,\"end_us\":%llu}",
              truth(e.called), truth(e.returned), e.rc,
              static_cast<unsigned long long>(e.begin_us), static_cast<unsigned long long>(e.end_us));
    }
    j.add("]}");
}
bool persist_mode(Session& s, unsigned mode) noexcept
{
    if (s.mode_written[mode]) return true;
    s.mode_written[mode] = true; // A partially written mode is never retried/overwritten.
    const auto& result = s.result[mode];
    if (result.outputs > 3 || result.event_count > core::MaxEvents) return false;
    for (unsigned output = 0; output < result.outputs; ++output)
    {
        const auto& image = result.images[output];
        if (image.y.size() != core::YBytes || image.u.size() != core::CBytes || image.v.size() != core::CBytes)
            return false;
        for (unsigned plane = 0; plane < 3; ++plane)
        {
            const auto& data = plane == 0 ? image.y : plane == 1 ? image.u : image.v;
            char name[48];
            std::snprintf(name, sizeof name, "%s-%u.%c.bin", mode_name(mode), output, "yuv"[plane]);
            if (!s.io.artifact(name, data.data(), data.size())) return false;
        }
    }
    auto& j = s.io.json; j.reset();
    j.add("{\"schema\":\"avc-probe-mode-v1\",\"mode\":\"%s\",\"data_complete\":%s,"
          "\"decoder_teardown_pending\":%s,\"submitted\":%u,\"outputs\":%u,\"outputs_before_tail\":%u,"
          "\"empty_tail_observed\":%s,\"tail_reset_after_per_au_stop\":%s,\"error\":",
          mode_name(mode), truth(result.data_complete), truth(result.decoder_teardown_pending),
          result.submitted, result.outputs, result.outputs_before_tail, truth(result.empty_tail_observed),
          truth(result.tail_reset_after_per_au_stop));
    j.string(result.error); j.add(",\"owner\":"); owner_json(j, s.owners[mode]);
    const auto& before = s.clocks_before[mode]; const auto& after = s.clocks_after[mode];
    j.add(",\"clocks_mhz_order\":[\"arm\",\"bus\",\"gpu\",\"xbar\"],"
          "\"clocks_before\":[%d,%d,%d,%d],\"clocks_after\":[%d,%d,%d,%d],"
          "\"normal_runtime_clock_and_affinity_setup_reached\":false,\"calls\":[",
          before.arm, before.bus, before.gpu, before.xbar, after.arm, after.bus, after.gpu, after.xbar);
    for (unsigned i = 0; i < result.event_count; ++i)
    {
        const auto& e = result.events[i]; const auto& f = e.picture.frame;
        j.add("%s{\"phase\":\"%s\",\"submitted_input_ordinal_not_output_identity\":%d,\"rc\":%d,"
              "\"count\":%u,\"capacity\":%u,\"slot\":%u,\"begin_ns\":%llu,\"end_ns\":%llu,"
              "\"picture_size\":%u,\"pixel_type\":%u,\"pitch\":%u,\"frame_width\":%u,\"frame_height\":%u,"
              "\"width\":%u,\"height\":%u,\"crop\":[%u,%u,%u,%u],"
              "\"picture_ptr\":[%llu,%llu],\"picture_pts\":[%u,%u]",
              i ? "," : "", phase_name(e.phase), e.submitted_input, e.returncode, e.output_count,
              e.capacity, e.picture_slot, static_cast<unsigned long long>(e.begin_ns),
              static_cast<unsigned long long>(e.end_ns), e.picture.size, f.pixelType, f.framePitch,
              f.frameWidth, f.frameHeight, f.horizontalSize, f.verticalSize, f.frameCropLeftOffset,
              f.frameCropRightOffset, f.frameCropTopOffset, f.frameCropBottomOffset,
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(f.pPicture[0])),
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(f.pPicture[1])),
              e.picture.info.pts.upper, e.picture.info.pts.lower);
        const auto& info = e.picture.info;
        j.add(",\"picture_present\":%s,\"info\":{\"num_units_in_tick\":%u,\"time_scale\":%u,"
              "\"fixed_frame_rate_flag\":%u,\"aspect_ratio_idc\":%u,\"sar\":[%u,%u],"
              "\"colour_primaries\":%u,\"transfer\":%u,\"matrix\":%u,\"full_range\":%u,"
              "\"pic_struct\":[%u,%u],\"ct_type\":%u}}",
              truth(e.output_count == 1), info.numUnitsInTick, info.timeScale,
              unsigned(info.fixedFrameRateFlag), unsigned(info.aspectRatioIdc),
              unsigned(info.sarWidth), unsigned(info.sarHeight), unsigned(info.colourPrimaries),
              unsigned(info.transferCharacteristics), unsigned(info.matrixCoefficients),
              unsigned(info.videoFullRangeFlag), unsigned(info.picStruct[0]),
              unsigned(info.picStruct[1]), unsigned(info.ctType));
    }
    j.add("],\"images\":[");
    for (unsigned i = 0; i < result.outputs; ++i)
        j.add("%s{\"ordinal\":%u,\"picture_slot\":%u,\"expected_source_index\":%d}",
              i ? "," : "", i, result.images[i].picture_slot, result.images[i].expected_source_index);
    j.add("],\"output_source_association\":\"per-au source index is the synchronous baseline expectation; continuous -1 remains unassigned until ordered content comparison\"}\n");
    char name[48]; std::snprintf(name, sizeof name, "mode-%s.json", mode_name(mode));
    return s.io.json_artifact(name);
}
bool run_mode(Session& s, unsigned mode) noexcept
{
    auto& owner = s.owners[mode]; auto& work = s.work[mode]; auto& result = s.result[mode];
    for (const auto& image : result.images)
        if (image.y.size() != core::YBytes || image.u.size() != core::CBytes || image.v.size() != core::CBytes)
            return false;
    if (!s.io.note(mode_name(mode), "owner_begin", 0)) return false;
    const bool opened = owner.open(s.io);
    bool ran = false;
    if (opened)
    {
        work.owner.control = &owner.control; work.owner.raster = owner.pixels;
        work.owner.raster_bytes = MiB; work.owner.owner_attests_fresh_created = true;
        if (s.io.note(mode_name(mode), "clock_snapshot_before", 0))
            s.clocks_before[mode] = Clocks::observe();
        if (s.io.note(mode_name(mode), "kernel_begin", 0))
            ran = core::run_mode(mode == 0 ? core::Mode::PerAu : core::Mode::Continuous,
                                 s.batch, work, result);
        s.io.note(mode_name(mode), "kernel_returned", ran ? 0 : -1, result.event_count);
        if (s.io.note(mode_name(mode), "clock_snapshot_after", 0))
            s.clocks_after[mode] = Clocks::observe();
    }
    const bool closed = owner.close(s.io);
    result.decoder_teardown_pending = !owner.delete_confirmed;
    // Mode/planes are retained in Session even if serialization or deletion fails.
    const bool persisted = persist_mode(s, mode);
    return opened && ran && closed && persisted && !s.io.error;
}

void final_data(Session& s) noexcept
{
    auto& j = s.io.json; j.reset();
    j.add("{\"schema\":\"avc-probe-comparison-v1\",\"acceptance\":false,\"external_binding_pending\":true,"
          "\"self_sha256\":null,\"elf_sha256\":null,\"device_identity\":null,\"kernel_sha256\":\"%s\","
          "\"inputs_verified\":%s,\"input_postcheck\":%s,\"comparison_observed\":%s,"
          "\"byte_exact_ordered_match\":%s,\"order_distinguishable\":%s,"
          "\"streaming_output_before_tail\":%s,\"data_match\":%s,\"error\":",
          KernelSha, truth(s.inputs_verified), truth(s.input_postcheck), truth(s.comparison_observed),
          truth(s.comparison.byte_exact_ordered_match), truth(s.comparison.order_distinguishable),
          truth(s.comparison.streaming_output_before_tail), truth(s.data_match));
    j.string(s.error); j.add(",\"io_error\":"); j.string(s.io.error);
    const bool clocks_equal = s.clocks_before[0].equal_valid(s.clocks_after[0]) &&
                              s.clocks_before[0].equal_valid(s.clocks_before[1]) &&
                              s.clocks_before[0].equal_valid(s.clocks_after[1]);
    j.add(",\"four_clock_snapshots_equal_valid\":%s,\"api_wall_is_gpu_time\":false", truth(clocks_equal));
    j.add(",\"association\":[");
    for (unsigned i = 0; i < 3; ++i)
    {
        j.add("%s{\"output_ordinal\":%u,\"matched_source_index\":", i ? "," : "", i);
        if (s.data_match) j.add("%u", Inputs[i].index); else j.add("null");
        j.add(",\"reference_expected_source_index\":%u,\"audio_sha256\":\"%s\","
              "\"audio_start\":%llu,\"audio_samples_per_channel\":%u,\"audio_played\":false}",
              Inputs[i].index, Inputs[i].audio_sha,
              static_cast<unsigned long long>(Inputs[i].audio_start), Inputs[i].audio_samples);
    }
    j.add("],\"artifacts\":[");
    for (unsigned i = 0; i < s.io.count; ++i)
    {
        const auto& file = s.io.files[i];
        j.add("%s{\"name\":", i ? "," : ""); j.string(file.name);
        j.add(",\"bytes\":%u,\"sha256\":\"%s\",\"written\":%s,\"readback_equal\":%s,"
              "\"open_rc\":%d,\"sync_rc\":%d,\"close_rc\":%d,\"read_close_rc\":%d}",
              file.bytes, file.sha256, truth(file.written), truth(file.readback_equal),
              file.open_rc, file.sync_rc, file.close_rc, file.read_close_rc);
    }
    j.add("],\"limits\":{\"pictures_per_mode\":3,\"capacity_per_call\":1,\"picture_calls_per_mode\":7,"
          "\"guardian_us\":30000000,\"root_launch_deadline_required_seconds\":60,"
          "\"live_contexts\":1,\"plane_bytes_cap\":1916928,\"audio_sync_or_fps_proven\":false}}\n");
    if (!s.io.json_artifact("comparison.json") && !s.error) s.error = "comparison artifact";
}

[[noreturn]] void preserve_and_exit(Session& s, int status) noexcept
{
    // This path never destructs Session, even if the exit request fails. It does
    // not free live decoder memory or hand control back to normal game startup.
    const int rc = sceKernelExitProcess(status);
    s.guardian.exit_return.store(rc, std::memory_order_release);
    (void)sceKernelExitThread(status);
    // Both exit APIs returning is an unconfirmed process failure, not cleanup.
    // Root's mandatory independent deadline ends this parked process.
    for (;;) (void)sceKernelDelayThread(1000000);
}
} // namespace

bool PortAvcContinuityProbeRequested() noexcept
{
    const char* value = std::getenv("STRIKERS_VITA_AVC_CONTINUITY_PROBE");
    return value && *value && std::strcmp(value, "0") != 0;
}
int PortRunAvcContinuityProbe() noexcept
{
    const char* label = std::getenv("STRIKERS_VITA_AVC_CONTINUITY_PROBE");
    if (!label || !*label || !std::strcmp(label, "0") || std::strlen(label) > 40) return 90;
    for (const char* p = label; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return 90;
    Session* const session = new (std::nothrow) Session;
    if (!session) return 90;
    auto& s = *session;
    if (!s.guardian.start())
    {
        if (!s.guardian.finish()) preserve_and_exit(s, 91);
        delete session;
        return 91;
    }
    try
    {
        if (!s.io.open(label, s.guardian)) throw "output directory/journal";
        s.io.note("startup_shutdown", "intent", 0);
        // startup::begin uses display storage but spawns no workers. The guardian
        // is already live before shutdown's framebuffer query/vblank wait.
        port::startup::shutdown();
        if (!s.io.note("startup_shutdown", "returned_void", 0)) throw "startup journal";
        if (!s.io.allocate_hash()) throw "SHA setup";
        if (!prepare(s)) throw "input verification/preparation";
        if (!run_mode(s, 0)) throw "per-AU mode";
        if (!run_mode(s, 1)) throw "continuous mode";
        s.input_postcheck = postcheck(s);
        if (!s.input_postcheck) throw "input postcheck";
        s.comparison = core::compare(s.result[0], s.result[1]);
        s.comparison_observed = true;
        s.data_match = s.comparison.byte_exact_ordered_match && s.comparison.order_distinguishable &&
                       s.owners[0].cleanup_complete && s.owners[1].cleanup_complete;
        if (!s.data_match) s.error = "mismatch or indistinguishable baseline; association inconclusive";
    }
    catch (const char* error) { s.error = error; }
    catch (...) { s.error = "unhandled preparation/serialization exception"; }

    // Finally: close every actually entered owner once; never retry failed cleanup.
    bool owners_closed = true;
    for (unsigned mode = 0; mode < 2; ++mode)
    {
        auto& owner = s.owners[mode];
        if (owner.started)
        {
            if (!owner.cleanup_attempted) owner.close(s.io);
            s.result[mode].decoder_teardown_pending = !owner.delete_confirmed;
            owners_closed &= owner.cleanup_complete;
            if (!s.mode_written[mode] && s.io.scratch) persist_mode(s, mode);
        }
    }
    if (s.io.scratch && s.io.journal >= 0) final_data(s);
    if (!owners_closed)
    {
        // Error records have been attempted; all referenced buffers stay alive.
        if (s.io.journal >= 0) (void)sceIoSyncByFd(s.io.journal, 0);
        preserve_and_exit(s, 95);
    }
    const bool journal_ok = s.io.scratch && s.io.journal >= 0 && s.io.close_journal(s.journal_hash);
    // If SHA allocation/base failed before it could serve readback, still close
    // an existing journal once. Its hash is then deliberately unavailable.
    if (s.io.journal >= 0)
    {
        s.io.journal_sync_rc = sceIoSyncByFd(s.io.journal, 0);
        s.io.journal_close_rc = sceIoClose(s.io.journal);
        s.io.journal = -1;
    }
    const bool hash_freed = s.io.free_hash();
    const bool guardian_stopped = s.guardian.finish();
    if (!hash_freed || !guardian_stopped) preserve_and_exit(s, 96);
    if (!s.io.directory_owned)
    {
        // A rejected existing label never authorizes writing into the old run.
        delete session;
        return 1;
    }
    const bool observed_match = s.data_match && journal_ok && !s.io.error && !s.error;
    auto& j = s.io.json; j.reset();
    j.add("{\"schema\":\"avc-probe-terminal-v1\",\"status\":\"%s\",\"acceptance\":false,"
          "\"external_binding_pending\":true,\"guard_ready\":%s,\"guard_reaped\":%s,\"guard_deleted\":%s,"
          "\"guard_priority\":%d,\"guard_create_rc\":%d,\"guard_start_rc\":%d,\"guard_wait_rc\":%d,"
          "\"guard_thread_status\":%d,\"guard_delete_rc\":%d,\"guard_fired\":%u,"
          "\"guard_begin_us\":%llu,\"guard_deadline_us\":%llu,"
          "\"owners_closed\":%s,\"sha_free_rc\":%d,\"journal_bytes\":%u,\"journal_sha256\":",
          observed_match ? "observed_match_unbound" : "observed_failure_or_inconclusive",
          truth(s.guardian.ready.load(std::memory_order_acquire)), truth(s.guardian.reaped),
          truth(s.guardian.deleted), s.guardian.priority, s.guardian.create_rc, s.guardian.start_rc,
          s.guardian.wait_rc, s.guardian.thread_status, s.guardian.delete_rc,
          s.guardian.fired.load(std::memory_order_acquire),
          static_cast<unsigned long long>(s.guardian.begin_us),
          static_cast<unsigned long long>(s.guardian.deadline_us), truth(owners_closed),
          s.io.scratch_free_rc, s.io.journal_bytes);
    j.string(s.journal_hash.valid ? s.journal_hash.hex : nullptr);
    const Artifact* comparison_file = nullptr;
    for (unsigned i = 0; i < s.io.count; ++i)
        if (!std::strcmp(s.io.files[i].name, "comparison.json") && s.io.files[i].readback_equal)
            comparison_file = &s.io.files[i];
    j.add(",\"comparison_bytes\":%u,\"comparison_sha256\":", comparison_file ? comparison_file->bytes : 0);
    j.string(comparison_file ? comparison_file->sha256 : nullptr);
    j.add(",\"error\":"); j.string(s.error); j.add(",\"io_error\":"); j.string(s.io.error);
    j.add(",\"io_rc\":%d,\"total_written\":%u,\"total_read\":%u,"
          "\"terminal_sha256\":null,\"terminal_hash_requires_external_collector\":true}\n",
          s.io.error_rc, s.io.total_written, s.io.total_read);
    const bool terminal_ok = j.good && s.io.terminal(j.bytes.data(), unsigned(j.size));
    if (!terminal_ok) preserve_and_exit(s, 97);
    // Only confirmed safe cleanup reaches destructors. No game initialization follows.
    delete session;
    return observed_match ? 0 : 1;
}
#else
bool PortAvcContinuityProbeRequested() noexcept { return false; }
int PortRunAvcContinuityProbe() noexcept { return 1; }
#endif
#endif
