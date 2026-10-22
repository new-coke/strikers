#pragma once
#include "port/native_video_record.hpp"
#include <array>
#include <vector>

namespace port
{
struct NativeVideoSpsStatus
{
    unsigned sps = 0, patched = 0;
    const char* error = nullptr;
};

namespace native_video_sps
{
// Bounded SPS syntax, written from ITU-T H.264 sections 7.3.2.1.1,
// 7.4.2.1.1 and E.1.1/E.1.2. No slice data is parsed or re-encoded.
constexpr size_t MaxSpsBytes = 4096, MaxAuBytes = 1024 * 1024, MaxOutputBytes = 2 * MaxAuBytes;
struct Bits
{
    const uint8_t* bytes;
    size_t count, at = 0;
    bool valid = true;

    uint32_t get(unsigned n)
    {
        if (!valid || n > 32 || n > count - at) { valid = false; return 0; }
        uint32_t value = 0;
        for (unsigned i = 0; i < n; ++i, ++at)
            value = (value << 1) | ((bytes[at / 8] >> (7 - at % 8)) & 1);
        return value;
    }
    void skip(size_t n)
    {
        if (!valid || n > count - at) valid = false;
        else at += n;
    }
    uint32_t ue(uint32_t maximum = UINT32_MAX - 1)
    {
        unsigned zeros = 0;
        while (valid && !get(1))
            if (++zeros > 31) { valid = false; return 0; }
        const uint32_t suffix = get(zeros);
        if (!valid) return 0;
        const uint32_t value = ((uint32_t(1) << zeros) - 1) + suffix;
        if (value > maximum) valid = false;
        return value;
    }
};

struct Parsed
{
    std::array<uint8_t, MaxSpsBytes> rbsp{};
    size_t bytes = 0, refAt = 0, refEnd = 0, bufferingAt = 0, bufferingEnd = 0, dataEnd = 0;
    uint32_t references = 0, buffering = 0, reorder = 0;
    bool restricted = false;
};

inline void hrd(Bits& bits)
{
    const auto count = bits.ue(31);
    bits.skip(8); // bit_rate_scale, cpb_size_scale
    for (unsigned i = 0; i <= count && bits.valid; ++i)
    {
        bits.ue();
        bits.ue();
        bits.skip(1);
    }
    bits.skip(20); // Four 5-bit delay/time-offset lengths.
}

inline void vui(Bits& bits, Parsed& parsed)
{
    if (bits.get(1) && bits.get(8) == 255) bits.skip(32); // Extended sample aspect ratio.
    if (bits.get(1)) bits.skip(1);
    if (bits.get(1))
    {
        bits.skip(4); // video_format and video_full_range_flag are preserved.
        if (bits.get(1)) bits.skip(24); // Colour description.
    }
    if (bits.get(1)) { bits.ue(5); bits.ue(5); }
    if (bits.get(1)) bits.skip(65); // num_units_in_tick, time_scale, fixed_frame_rate_flag.
    const bool nalHrd = bits.get(1) != 0;
    if (nalHrd) hrd(bits);
    const bool vclHrd = bits.get(1) != 0;
    if (vclHrd) hrd(bits);
    if (nalHrd || vclHrd) bits.skip(1);
    bits.skip(1); // pic_struct_present_flag
    parsed.restricted = bits.get(1) != 0;
    if (parsed.restricted)
    {
        bits.skip(1);
        bits.ue(16); // max_bytes_per_pic_denom
        bits.ue(16); // max_bits_per_mb_denom
        bits.ue(16); // log2_max_mv_length_horizontal
        bits.ue(16); // log2_max_mv_length_vertical
        parsed.reorder = bits.ue(16);
        parsed.bufferingAt = bits.at;
        parsed.buffering = bits.ue(16);
        parsed.bufferingEnd = bits.at;
    }
}

inline const char* parse(const uint8_t* nal, size_t size, unsigned width, unsigned height, Parsed& parsed)
{
    if (size < 2 || size > MaxSpsBytes || (nal[0] & 0x80) || (nal[0] & 31) != 7 || !(nal[0] & 0x60))
        return "invalid SPS NAL";
    parsed = {};
    unsigned zeros = 0;
    for (size_t i = 1; i < size; ++i)
    {
        const auto value = nal[i];
        if (zeros == 2)
        {
            if (value == 3)
            {
                if (i + 1 == size || nal[i + 1] > 3) return "invalid SPS emulation prevention";
                zeros = 0;
                continue;
            }
            if (value < 3) return "missing SPS emulation prevention";
        }
        parsed.rbsp[parsed.bytes++] = value;
        zeros = value ? 0 : zeros + 1;
    }
    Bits bits{parsed.rbsp.data(), parsed.bytes * 8};
    if (bits.get(8) != 66) return "unsupported SPS profile (WAVC requires Baseline)";
    if (bits.get(8) & 3) return "invalid SPS reserved bits";
    bits.skip(8); // level_idc
    bits.ue(31); // seq_parameter_set_id
    bits.ue(12); // log2_max_frame_num_minus4
    const auto poc = bits.ue(2);
    if (!poc) bits.ue(12);
    else if (poc == 1)
    {
        bits.skip(1);
        bits.ue(); // Signed offsets only need their codeword lengths here.
        bits.ue();
        const auto cycle = bits.ue(255);
        for (unsigned i = 0; i < cycle && bits.valid; ++i) bits.ue();
    }
    parsed.refAt = bits.at;
    parsed.references = bits.ue(16);
    parsed.refEnd = bits.at;
    bits.skip(1);
    const auto widthMbs = bits.ue(43) + 1;
    const auto heightMaps = bits.ue(47) + 1;
    const bool frameOnly = bits.get(1) != 0;
    if (!frameOnly) bits.skip(1);
    bits.skip(1);
    bool cropped = false;
    if (bits.get(1))
        for (unsigned i = 0; i < 4; ++i) cropped |= bits.ue() != 0;
    if (bits.get(1)) vui(bits, parsed);
    parsed.dataEnd = bits.at;
    if (bits.get(1) != 1) bits.valid = false;
    while (bits.valid && bits.at % 8)
        if (bits.get(1)) bits.valid = false;
    if (!bits.valid || bits.at != bits.count) return "invalid or truncated SPS syntax";
    if (cropped || widthMbs * 16 != width || heightMaps * 16 * (frameOnly ? 1 : 2) != height)
        return "unsupported SPS geometry (WAVC dimensions must match uncropped frames)";
    if (parsed.references > 1) return "unsupported SPS references (WAVC driver supports one)";
    if (parsed.restricted && parsed.buffering > 1)
        return "unsupported SPS buffering (WAVC driver supports one)";
    if (parsed.restricted && parsed.reorder > (parsed.buffering ? parsed.buffering : 1))
        return "invalid SPS reorder/buffering relationship";
    return nullptr;
}

struct Writer
{
    std::array<uint8_t, MaxSpsBytes + 1> bytes{};
    size_t at = 0;
    void bit(unsigned value)
    {
        bytes[at / 8] |= uint8_t((value & 1) << (7 - at % 8));
        ++at;
    }
    void copy(const Parsed& parsed, size_t begin, size_t end)
    {
        for (size_t i = begin; i < end; ++i) bit(parsed.rbsp[i / 8] >> (7 - i % 8));
    }
};

inline size_t prefix(const uint8_t* source, size_t size, size_t at)
{
    if (size - at >= 3 && !source[at] && !source[at + 1])
    {
        if (source[at + 2] == 1) return 3;
        if (size - at >= 4 && !source[at + 2] && source[at + 3] == 1) return 4;
    }
    return 0;
}

inline void append_sps(const uint8_t* nal, const Parsed& parsed, std::vector<uint8_t>& output)
{
    Writer writer;
    writer.copy(parsed, 0, parsed.refAt);
    if (!parsed.references) { writer.bit(0); writer.bit(1); writer.bit(0); }
    else writer.copy(parsed, parsed.refAt, parsed.refEnd);
    if (parsed.restricted)
    {
        writer.copy(parsed, parsed.refEnd, parsed.bufferingAt);
        if (!parsed.buffering) { writer.bit(0); writer.bit(1); writer.bit(0); }
        else writer.copy(parsed, parsed.bufferingAt, parsed.bufferingEnd);
        writer.copy(parsed, parsed.bufferingEnd, parsed.dataEnd);
    }
    else writer.copy(parsed, parsed.refEnd, parsed.dataEnd);
    writer.bit(1);
    while (writer.at % 8) writer.bit(0);
    output.push_back(nal[0]);
    unsigned zeros = 0;
    for (size_t i = 0; i < writer.at / 8; ++i)
    {
        const auto value = writer.bytes[i];
        if (zeros == 2 && value <= 3) { output.push_back(3); zeros = 0; }
        output.push_back(value);
        zeros = value ? 0 : zeros + 1;
    }
}
} // namespace native_video_sps

// Prepare an independent WAVC access unit for the one-reference Vita decoder.
// Only zero reference/buffering codewords change. Output is empty on failure;
// its retained capacity is reused on the next frame. No decoder resources grow.
inline bool prepare_native_video_au(const NativeVideoComponent& component,
                                    std::vector<uint8_t>& output, NativeVideoSpsStatus& status) noexcept
{
    using namespace native_video_sps;
    status = {};
    output.clear();
    const auto reject = [&](const char* error) {
        status.error = error;
        output.clear();
        return false;
    };
    const auto* source = component.elementary;
    const auto size = component.bytes;
    if (!source || !size || size > MaxAuBytes || component.width < 16 || component.width > 704 ||
        component.height < 16 || component.height > 768 || component.width % 16 || component.height % 16)
        return reject("invalid WAVC access unit extent");
    try
    {
        output.reserve(size + 64);
        for (size_t at = 0; at < size;)
        {
            const auto start = at, code = prefix(source, size, at);
            if (!code || code == size - at) return reject("invalid Annex-B access unit");
            at += code;
            const auto nal = at;
            if (source[nal] & 0x80) return reject("invalid NAL forbidden bit");
            ++at;
            while (at < size && !prefix(source, size, at)) ++at;
            if ((source[nal] & 31) == 7)
            {
                // Annex-B trailing_zero_8bits belong to the byte stream, not SPS RBSP.
                auto end = at;
                while (end > nal + 1 && !source[end - 1]) --end;
                Parsed parsed;
                if (const char* error = parse(source + nal, end - nal, component.width, component.height, parsed))
                    return reject(error);
                ++status.sps;
                const bool patch = !parsed.references || (parsed.restricted && !parsed.buffering);
                if (patch)
                {
                    ++status.patched;
                    output.insert(output.end(), source + start, source + nal);
                    append_sps(source + nal, parsed, output);
                    output.insert(output.end(), source + end, source + at);
                }
                else output.insert(output.end(), source + start, source + at);
            }
            else output.insert(output.end(), source + start, source + at);
            if (output.size() > MaxOutputBytes) return reject("expanded access unit exceeds limit");
        }
        if (!status.sps) return reject("access unit has no SPS");
        return true;
    }
    catch (...) { return reject("access unit allocation failed"); }
}
} // namespace port
