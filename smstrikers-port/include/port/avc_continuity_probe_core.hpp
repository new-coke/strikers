#pragma once
// Prepared Vita probe kernel. No main, allocator, launcher or execution binding.
// The future harness must satisfy OWNER_CONTRACT.md and retain every object
// through successful DeleteDecoder. This header has not been compiled or run.
#if !defined(PORT_VITA)
#error This kernel is for a reviewed Vita harness, not a CPU decoder mock
#endif
#include <psp2/videodec.h>
#include "port/native_video_record.hpp"
#include "port/native_video_sps.hpp"
#include "port/host.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Same missing public declarations and local consumer provenance as production.
extern "C" int sceAvcdecDecodeStop(SceAvcdecCtrl*, SceAvcdecArrayPicture*);
extern "C" int sceAvcdecDecodeFlush(SceAvcdecCtrl*);

namespace avc_continuity_probe
{
constexpr unsigned Inputs = 3, FirstSourceIndex = 299;
constexpr unsigned Width = 512, Height = 416, Pitch = 512;
constexpr unsigned MaxPictureCalls = 7, MaxEvents = 12;
constexpr size_t YBytes = size_t(Width) * Height, CBytes = YBytes / 4;

enum class Mode { PerAu, Continuous };
enum class Phase { Decode, Stop, Flush };

struct InputRecord
{
    unsigned source_index = 0;
    uint64_t audio_sample_start = 0;
    std::vector<uint8_t> wavc, audio;
};

struct PreparedInput
{
    InputRecord original;
    port::NativeVideoSpsStatus sps{};
    std::vector<uint8_t> elementary, pristine_elementary;
    size_t elementary_bytes = 0;
    SceAvcdecAu au{};
};

// Addresses inside the batch become API inputs. Never move or copy it.
struct Batch
{
    std::array<PreparedInput, Inputs> input{};
    bool prepared = false;
    const char* error = nullptr;
    Batch() = default;
    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;
    Batch(Batch&&) = delete;
    Batch& operator=(Batch&&) = delete;

    bool prepare(const std::array<InputRecord, Inputs>& records)
    {
        if (prepared) { error = "batch already prepared"; return false; }
        try
        {
            for (unsigned i = 0; i < Inputs; ++i)
            {
                auto& target = input[i];
                target.original = records[i];
                const auto& record = target.original;
                port::NativeVideoComponent component;
                if (record.source_index != FirstSourceIndex + i ||
                    record.audio.size() < 80 || record.audio.size() > 1024 * 1024 ||
                    !port::parse_native_video(record.wavc.data(), record.wavc.size(), component) ||
                    component.width != Width || component.height != Height)
                { error = "input record contract"; return false; }
                if (i && record.audio_sample_start != input[i - 1].original.audio_sample_start +
                         port::video_word(input[i - 1].original.audio.data() + 4))
                { error = "audio sample association"; return false; }
                if (!port::prepare_native_video_au(component, target.elementary, target.sps))
                { error = target.sps.error ? target.sps.error : "SPS preparation"; return false; }
                target.elementary_bytes = target.elementary.size();
                if (!target.elementary_bytes || target.elementary_bytes > 2 * 1024 * 1024)
                { error = "prepared elementary extent"; return false; }
                target.elementary.resize(target.elementary_bytes + 64, 0);
                target.pristine_elementary = target.elementary;
                target.au.pts = { UINT32_MAX, UINT32_MAX };
                target.au.dts = target.au.pts;
                target.au.es = { target.elementary.data(), uint32_t(target.elementary_bytes) };
            }
        }
        catch (...) { error = "batch allocation"; return false; }
        prepared = true;
        return true;
    }

    bool intact() const
    {
        if (!prepared) return false;
        for (const auto& item : input)
            if (item.elementary != item.pristine_elementary ||
                item.au.es.pBuf != item.elementary.data() ||
                item.au.es.size != item.elementary_bytes ||
                item.au.pts.upper != UINT32_MAX || item.au.pts.lower != UINT32_MAX ||
                item.au.dts.upper != UINT32_MAX || item.au.dts.lower != UINT32_MAX)
                return false;
        return true;
    }
};

// Supplied only after the future harness has created a fresh, independent
// decoder using the exact production init/query/allocation policy.
// No member asserts that a real SDK call succeeded without the owner's receipt.
struct OwnerView
{
    SceAvcdecCtrl* control = nullptr;
    uint8_t* raster = nullptr;
    size_t raster_bytes = 0;
    bool owner_attests_fresh_created = false;
};

struct CallSlot
{
    unsigned index = MaxPictureCalls;
    SceAvcdecPicture picture{};
    SceAvcdecPicture* pointer = nullptr;
    SceAvcdecArrayPicture array{};
};

struct Workspace
{
    OwnerView owner;
    std::array<CallSlot, MaxPictureCalls> calls{};
    unsigned used_calls = 0;
    bool used = false;
    Workspace() = default;
    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;
    Workspace(Workspace&&) = delete;
    Workspace& operator=(Workspace&&) = delete;
};

struct ApiEvent
{
    Phase phase = Phase::Decode;
    int submitted_input = -1; // -1 means tail; NOT an attribution of its output.
    int returncode = 0;
    unsigned output_count = 0, capacity = 1, picture_slot = 0;
    uint64_t begin_ns = 0, end_ns = 0;
    SceAvcdecPicture picture{}; // Meaningful only when output_count == 1.
};

struct Image
{
    // Exact production GX I8 tile bytes, without pitch padding.
    std::vector<uint8_t> y, u, v;
    unsigned picture_slot = 0;
    int expected_source_index = -1; // B remains unassigned until content comparison.
};

struct Result
{
    Mode mode = Mode::PerAu;
    std::array<ApiEvent, MaxEvents> events{};
    std::array<Image, Inputs> images{};
    unsigned event_count = 0, submitted = 0, outputs = 0, outputs_before_tail = 0;
    bool used = false, empty_tail_observed = false, tail_reset_after_per_au_stop = false;
    bool data_complete = false, decoder_teardown_pending = true;
    const char* error = nullptr;
};

inline bool fail(Result& result, const char* error)
{
    if (!result.error) result.error = error;
    return false;
}

inline bool geometry(const Workspace& work, const CallSlot& slot)
{
    const auto& frame = slot.picture.frame;
    return slot.index < MaxPictureCalls && &slot == &work.calls[slot.index] &&
           slot.array.numOfElm == 1 && slot.array.pPicture == &slot.pointer &&
           slot.pointer == &slot.picture && slot.picture.size == sizeof slot.picture &&
           frame.pixelType == SCE_AVCDEC_PIXELFORMAT_YUV420_PACKED_RASTER &&
           frame.framePitch == Pitch && frame.pPicture[0] == work.owner.raster &&
           !frame.pPicture[1] && frame.frameWidth == Width && frame.frameHeight == Height &&
           frame.horizontalSize == Width && frame.verticalSize == Height &&
           !frame.frameCropLeftOffset && !frame.frameCropRightOffset &&
           !frame.frameCropTopOffset && !frame.frameCropBottomOffset;
}

inline CallSlot* picture_call(Workspace& work, Result& result, Phase phase,
                              int submitted, const SceAvcdecAu* au)
{
    if (work.used_calls >= MaxPictureCalls || result.event_count >= MaxEvents ||
        (phase != Phase::Decode && phase != Phase::Stop) ||
        (phase == Phase::Decode && !au))
    { fail(result, "picture call bound/shape"); return nullptr; }
    const unsigned index = work.used_calls++;
    auto& slot = work.calls[index];
    slot.index = index;
    slot.picture.size = sizeof slot.picture;
    slot.picture.frame.pixelType = SCE_AVCDEC_PIXELFORMAT_YUV420_PACKED_RASTER;
    slot.picture.frame.framePitch = Pitch;
    slot.picture.frame.frameWidth = Width;
    slot.picture.frame.frameHeight = Height;
    slot.picture.frame.pPicture[0] = work.owner.raster;
    slot.pointer = &slot.picture;
    slot.array = { 0, 1, &slot.pointer };
    auto& event = result.events[result.event_count++];
    event.phase = phase;
    event.submitted_input = submitted;
    event.picture_slot = index;
    event.begin_ns = port_monotonic_ns();
    event.returncode = phase == Phase::Decode
        ? sceAvcdecDecode(work.owner.control, au, &slot.array)
        : sceAvcdecDecodeStop(work.owner.control, &slot.array);
    event.end_ns = port_monotonic_ns();
    event.output_count = slot.array.numOfOutput;
    event.picture = slot.picture;
    if (event.returncode < 0) { fail(result, "decoder API error"); return nullptr; }
    if (event.end_ns < event.begin_ns) { fail(result, "wall clock regressed"); return nullptr; }
    if (slot.array.numOfOutput > 1)
    { fail(result, "output count exceeds one-picture capacity"); return nullptr; }
    if (slot.array.numOfOutput == 1 && !geometry(work, slot))
    { fail(result, "output geometry/ownership"); return nullptr; }
    return &slot;
}

inline bool flush(Workspace& work, Result& result, int submitted)
{
    if (result.event_count >= MaxEvents) return fail(result, "event bound");
    auto& event = result.events[result.event_count++];
    event.phase = Phase::Flush;
    event.submitted_input = submitted;
    event.capacity = 0;
    event.begin_ns = port_monotonic_ns();
    event.returncode = sceAvcdecDecodeFlush(work.owner.control);
    event.end_ns = port_monotonic_ns();
    if (event.returncode < 0) return fail(result, "flush API error");
    return event.end_ns >= event.begin_ns || fail(result, "wall clock regressed");
}

inline bool retain_image(Workspace& work, Result& result, const CallSlot& slot,
                         int expected_source_index, bool before_tail)
{
    if (result.outputs >= Inputs || slot.array.numOfOutput != 1 || !geometry(work, slot))
        return fail(result, "image count/geometry");
    auto& image = result.images[result.outputs];
    const size_t yExtent = size_t(Pitch) * Height;
    if (!port::tile_native_nv12(work.owner.raster, yExtent, work.owner.raster + yExtent,
                               yExtent / 2, Pitch, Width, Height,
                               image.y.data(), image.u.data(), image.v.data()))
        return fail(result, "production tiler rejected output");
    image.picture_slot = slot.index;
    image.expected_source_index = expected_source_index;
    ++result.outputs;
    result.outputs_before_tail += before_tail;
    return true;
}

inline bool run_mode(Mode mode, const Batch& batch, Workspace& work, Result& result)
{
    if (work.used || result.used || !batch.intact() ||
        (mode != Mode::PerAu && mode != Mode::Continuous) ||
        !work.owner.owner_attests_fresh_created || !work.owner.control ||
        !work.owner.control->frameBuf.pBuf || !work.owner.control->frameBuf.size ||
        !work.owner.raster || work.owner.raster_bytes != 1024 * 1024)
        return fail(result, "fresh owner/input contract");
    work.used = result.used = true;
    result.mode = mode;
    // All allocation occurs before the first codec call. No I/O is done here.
    try
    {
        for (auto& image : result.images)
        { image.y.resize(YBytes); image.u.resize(CBytes); image.v.resize(CBytes); }
    }
    catch (...) { return fail(result, "output allocation"); }
    bool last_per_au_flush = false;
    for (unsigned i = 0; i < Inputs; ++i)
    {
        ++result.submitted;
        auto* slot = picture_call(work, result, Phase::Decode, int(i), &batch.input[i].au);
        if (!slot) return false;
        if (mode == Mode::PerAu)
        {
            // Policy derived from frozen native_video.cpp:355-409:
            // Decode; Stop only on zero; require one valid picture; Flush only
            // after Stop and before copying the output. No future AU is fed.
            const bool stopped = slot->array.numOfOutput == 0;
            if (stopped) slot = picture_call(work, result, Phase::Stop, int(i), nullptr);
            if (!slot) return false;
            if (slot->array.numOfOutput != 1) return fail(result, "per-AU picture missing");
            if (stopped && !flush(work, result, int(i))) return false;
            if (!retain_image(work, result, *slot, int(FirstSourceIndex + i), true)) return false;
            last_per_au_flush = stopped;
        }
        else if (slot->array.numOfOutput == 1)
        {
            if (!retain_image(work, result, *slot, -1, true)) return false;
        }
        // In continuous mode zero output is pending; it is never published.
    }
    if (mode == Mode::PerAu && last_per_au_flush)
    {
        // The final AU was already drained and reset as in the product.
        result.tail_reset_after_per_au_stop = true;
    }
    else
    {
        // At most three pending pictures plus one empty witness. The local
        // public consumer motivates repeated tail Stop, but does not establish
        // an SDK guarantee; any error here remains a failed/inconclusive tail.
        for (unsigned attempt = 0; attempt <= Inputs; ++attempt)
        {
            auto* slot = picture_call(work, result, Phase::Stop, -1, nullptr);
            if (!slot) return false;
            if (!slot->array.numOfOutput)
            { result.empty_tail_observed = true; break; }
            if (mode == Mode::PerAu) return fail(result, "extra per-AU tail picture");
            if (!retain_image(work, result, *slot, -1, false)) return false;
        }
        if (!result.empty_tail_observed) return fail(result, "tail did not become empty");
        if (!flush(work, result, -1)) return false;
    }
    if (result.outputs != Inputs) return fail(result, "incomplete picture count");
    if (!batch.intact()) return fail(result, "submitted elementary storage changed");
    result.data_complete = true;
    // Owner still must DeleteDecoder successfully before releasing any input,
    // picture descriptor, reference block or raster. No final PASS is emitted.
    return true;
}

inline bool same_image(const Image& left, const Image& right)
{
    return left.y == right.y && left.u == right.u && left.v == right.v;
}

struct Comparison
{
    bool byte_exact_ordered_match = false;
    bool order_distinguishable = false;
    bool streaming_output_before_tail = false;
    bool owner_teardown_and_artifact_binding_pending = true;
};

inline Comparison compare(const Result& baseline, const Result& continuous)
{
    Comparison out;
    if (!baseline.data_complete || !continuous.data_complete ||
        baseline.mode != Mode::PerAu || continuous.mode != Mode::Continuous ||
        baseline.outputs != Inputs || continuous.outputs != Inputs ||
        !continuous.empty_tail_observed)
        return out;
    out.byte_exact_ordered_match = true;
    out.order_distinguishable = true;
    for (unsigned i = 0; i < Inputs; ++i)
    {
        out.byte_exact_ordered_match &= same_image(baseline.images[i], continuous.images[i]);
        for (unsigned j = i + 1; j < Inputs; ++j)
            out.order_distinguishable &= !same_image(baseline.images[i], baseline.images[j]);
    }
    out.streaming_output_before_tail = continuous.outputs_before_tail != 0;
    return out;
}
} // namespace avc_continuity_probe
