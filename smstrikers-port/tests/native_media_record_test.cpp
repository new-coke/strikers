#include "port/native_audio_record.hpp"
#include "port/native_video_record.hpp"
#include "port/native_video_sps.hpp"
#include <cassert>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
static void le32(std::vector<uint8_t>& b, unsigned v)
{
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(v >> (8 * i));
}
static void le16(std::vector<uint8_t>& b, int v)
{
    b.push_back(v & 255);
    b.push_back((unsigned(v) >> 8) & 255);
}
static void be32(std::vector<uint8_t>& b, unsigned v)
{
    for (int i = 3; i >= 0; --i)
        b.push_back(v >> (8 * i));
}
namespace
{
// Fixture construction is independent of the production reader/bit copier.
struct SpsFixture
{
    std::vector<unsigned> bits;
    void value(uint64_t v, unsigned n)
    {
        for (unsigned i = n; i; --i) bits.push_back(unsigned(v >> (i - 1)) & 1);
    }
    void ue(uint32_t v)
    {
        const uint64_t code = uint64_t(v) + 1;
        unsigned n = 0;
        for (auto copy = code; copy; copy >>= 1) ++n;
        value(0, n - 1);
        value(code, n);
    }
    void hrd(unsigned cpb)
    {
        ue(cpb);
        value(0xa3, 8);
        for (unsigned i = 0; i <= cpb; ++i) { ue(i + 3); ue(i + 7); value(i & 1, 1); }
        value(0x83de1, 20);
    }
    std::vector<uint8_t> finish()
    {
        value(1, 1);
        while (bits.size() % 8) value(0, 1);
        std::vector<uint8_t> result{0x67};
        unsigned zeros = 0;
        for (size_t at = 0; at < bits.size(); at += 8)
        {
            unsigned byte = 0;
            for (unsigned j = 0; j < 8; ++j) byte = byte * 2 + bits[at + j];
            if (zeros == 2 && byte <= 3) { result.push_back(3); zeros = 0; }
            result.push_back(uint8_t(byte));
            zeros = byte ? 0 : zeros + 1;
        }
        return result;
    }
};

std::vector<uint8_t> sps_fixture(unsigned refs, unsigned buffering, unsigned poc = 2,
                                 bool vui = true, bool restriction = true, bool optional = false,
                                 unsigned cycle = 2, unsigned cpb = 1)
{
    SpsFixture s;
    s.value(66, 8); s.value(0xc0, 8); s.value(32, 8);
    s.ue(0); s.ue(3); s.ue(poc);
    if (!poc) s.ue(2);
    else if (poc == 1)
    {
        s.value(0, 1); s.ue(1); s.ue(2); s.ue(cycle);
        for (unsigned i = 0; i < cycle; ++i) s.ue(i & 3);
    }
    s.ue(refs); s.value(0, 1);
    s.ue(0); s.ue(0); // 16x16 uncropped frames.
    s.value(1, 1); s.value(1, 1); s.value(0, 1);
    s.value(vui, 1);
    if (vui)
    {
        s.value(optional, 1);
        if (optional) { s.value(255, 8); s.value(321, 16); s.value(123, 16); }
        s.value(optional, 1);
        if (optional) s.value(1, 1);
        s.value(1, 1); s.value(5, 3); s.value(1, 1); s.value(optional, 1);
        if (optional) { s.value(2, 8); s.value(2, 8); s.value(5, 8); }
        s.value(optional, 1);
        if (optional) { s.ue(5); s.ue(3); }
        s.value(1, 1); s.value(1, 32); s.value(120, 32); s.value(1, 1);
        s.value(optional, 1);
        if (optional) s.hrd(cpb);
        s.value(optional, 1);
        if (optional) s.hrd(cpb);
        if (optional) s.value(1, 1);
        s.value(optional, 1); s.value(restriction, 1);
        if (restriction)
        {
            s.value(1, 1); s.ue(2); s.ue(1); s.ue(16); s.ue(16); s.ue(0); s.ue(buffering);
        }
    }
    return s.finish();
}

std::vector<uint8_t> access_unit(const std::vector<uint8_t>& sps, bool shortPrefix = false, unsigned trailing = 0)
{
    std::vector<uint8_t> result{0, 0, 0, 1, 9, 0xf0};
    if (!sps.empty())
    {
        if (!shortPrefix) result.push_back(0);
        result.insert(result.end(), {0, 0, 1});
        result.insert(result.end(), sps.begin(), sps.end());
        result.insert(result.end(), trailing, 0);
    }
    // Non-SPS NAL payloads and both forms of delimiter must remain byte-exact.
    result.insert(result.end(), {0, 0, 0, 1, 0x68, 0xce, 0x02, 0x32, 0xc8,
                                0, 0, 1, 0x06, 0x05, 0x02, 0x7e, 0x33, 0x80,
                                0, 0, 1, 0x65, 0x88, 0x84, 0x21});
    return result;
}

bool prepare(const std::vector<uint8_t>& bytes, std::vector<uint8_t>& out, port::NativeVideoSpsStatus& info,
             unsigned width = 16, unsigned height = 16)
{
    return port::prepare_native_video_au({width, height, bytes.data(), bytes.size()}, out, info);
}

void check_sps()
{
    const std::vector<uint8_t> realSps{0x67, 0x42, 0xc0, 0x20, 0xdc, 0x08, 0x03, 0x5a, 0x6e, 0x04, 0x04, 0x0a,
                                     0x80, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0b, 0xb5, 0x47, 0x8c, 0x19, 0xc0};
    // Independent trace_headers locations: max_num_ref_frames at bit37,
    // max_dec_frame_buffering at bit184. Only their ue(0) becomes ue(1).
    const std::vector<uint8_t> fixedSps{0x67, 0x42, 0xc0, 0x20, 0xda, 0x02, 0x00, 0xd6, 0x9b, 0x81, 0x01, 0x02,
                                      0xa0, 0x00, 0x00, 0x0c, 0x80, 0x00, 0x02, 0xed, 0x51, 0xe3, 0x06, 0x54};
    std::vector<uint8_t> output, repeated;
    port::NativeVideoSpsStatus info;
    const auto real = access_unit(realSps, false, 2);
    assert(prepare(real, output, info, 512, 416));
    assert(info.sps == 1 && info.patched == 1 && !info.error);
    assert(output == access_unit(fixedSps, false, 2));
    assert(prepare(output, repeated, info, 512, 416));
    assert(output == repeated && info.sps == 1 && !info.patched);
    bool sawGrowth = false, sawEmulation = false;
    for (unsigned poc = 0; poc < 3; ++poc)
        for (bool vui : {false, true})
            for (bool restricted : {false, true})
                for (bool optional : {false, true})
                    for (unsigned refs = 0; refs <= 1; ++refs)
                        for (unsigned buffering = 0; buffering <= 1; ++buffering)
                        {
                            const auto sourceSps = sps_fixture(refs, buffering, poc, vui, restricted, optional);
                            const auto source = access_unit(sourceSps, true);
                            const auto expected = access_unit(sps_fixture(1, 1, poc, vui, restricted, optional), true);
                            assert(prepare(source, output, info) && output == expected);
                            assert(info.patched == unsigned(!refs || (vui && restricted && !buffering)));
                            assert(prepare(output, repeated, info) && output == repeated && !info.patched);
                            sawGrowth |= output.size() > source.size();
                            for (size_t i = 2; i < sourceSps.size(); ++i)
                                sawEmulation |= !sourceSps[i - 2] && !sourceSps[i - 1] && sourceSps[i] == 3;
                        }
    assert(sawGrowth && sawEmulation);
    const auto maximumSyntax = sps_fixture(0, 0, 1, true, true, true, 255, 31);
    assert(prepare(access_unit(maximumSyntax), output, info));
    assert(output == access_unit(sps_fixture(1, 1, 1, true, true, true, 255, 31)));
    for (size_t bytes = 0; bytes < realSps.size(); ++bytes)
    {
        const std::vector<uint8_t> truncated(realSps.begin(), realSps.begin() + bytes);
        output = {0xde, 0xad};
        assert(!prepare(access_unit(truncated), output, info, 512, 416));
        assert(output.empty() && info.error);
    }
    auto malformed = sps_fixture(0, 0, 2, true, true, true);
    size_t prevention = 0;
    for (size_t i = 2; i + 1 < malformed.size(); ++i)
        if (!malformed[i - 2] && !malformed[i - 1] && malformed[i] == 3) { prevention = i; break; }
    assert(prevention);
    auto damaged = malformed;
    damaged[prevention + 1] = 4;
    assert(!prepare(access_unit(damaged), output, info) && output.empty());
    damaged = malformed;
    damaged.erase(damaged.begin() + prevention);
    assert(!prepare(access_unit(damaged), output, info) && output.empty());
    damaged.assign(malformed.begin(), malformed.begin() + prevention + 1);
    assert(!prepare(access_unit(damaged), output, info) && output.empty());
    damaged = {0x67, 0x42, 0xc0, 0x20, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0x80};
    assert(!prepare(access_unit(damaged), output, info)); // Exp-Golomb prefix overflow.
    damaged.assign(4097, 0xff);
    damaged[0] = 0x67;
    assert(!prepare(access_unit(damaged), output, info));
    damaged = malformed;
    damaged[0] |= 0x80;
    assert(!prepare(access_unit(damaged), output, info));
    damaged = malformed;
    damaged[1] = 100;
    assert(!prepare(access_unit(damaged), output, info));
    assert(!prepare(access_unit(sps_fixture(2, 2)), output, info));
    assert(std::strstr(info.error, "references"));
    assert(!prepare(access_unit(sps_fixture(1, 2)), output, info));
    assert(std::strstr(info.error, "buffering"));
    assert(!prepare(access_unit(sps_fixture(0, 0, 1, true, true, true, 256)), output, info));
    assert(!prepare(access_unit(sps_fixture(0, 0, 2, true, true, true, 2, 32)), output, info));
    assert(!prepare(real, output, info, 16, 16));
    assert(!port::prepare_native_video_au({16, 16, nullptr, 1}, output, info));
    assert(!port::prepare_native_video_au({16, 16, real.data(), 1024 * 1024 + 1}, output, info));
}

int patch_file(const char* input, const char* output, const char* width, const char* height)
{
    FILE* source = std::fopen(input, "rb");
    if (!source) return 1;
    assert(!std::fseek(source, 0, SEEK_END));
    const auto size = std::ftell(source);
    assert(size > 0 && size <= 1024 * 1024 && !std::fseek(source, 0, SEEK_SET));
    std::vector<uint8_t> bytes(size), patched;
    assert(std::fread(bytes.data(), 1, bytes.size(), source) == bytes.size());
    assert(!std::fclose(source));
    port::NativeVideoSpsStatus info;
    if (!prepare(bytes, patched, info, unsigned(std::strtoul(width, nullptr, 10)),
                 unsigned(std::strtoul(height, nullptr, 10))))
    {
        std::fprintf(stderr, "SPS: %s\n", info.error);
        return 1;
    }
    FILE* target = std::fopen(output, "wb");
    assert(target && std::fwrite(patched.data(), 1, patched.size(), target) == patched.size());
    assert(!std::fclose(target));
    std::printf("SPS: count=%u patched=%u input=%u output=%u\n", info.sps, info.patched,
                unsigned(bytes.size()), unsigned(patched.size()));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 6 && !std::strcmp(argv[1], "--patch-avc")) return patch_file(argv[2], argv[3], argv[4], argv[5]);
    assert(argc == 1);
    check_sps();
    std::array<int16_t, 16> coefficients{};
    coefficients[0] = 2048;
    const std::array<uint8_t, 8> raw{ { 0, 1, 2, 3, 4, 5, 6, 7 } };
    std::vector<uint8_t> record{ 'S', 'T', 'P', 'C', 'M', '0', '0', '1' };
    le32(record, 8);
    le32(record, 14);
    for (auto value : coefficients)
        le16(record, value);
    le16(record, -32768);
    le16(record, 32767);
    record.insert(record.end(), raw.begin(), raw.end());
    le16(record, -32768);
    le16(record, 32767);
    for (int i = 0; i < 14; ++i)
        le16(record, i == 13 ? -32768 : i);
    const auto hash = port::audio_hash(record.data(), record.size());
    for (unsigned i = 0; i < 8; ++i)
        record.push_back(hash >> (8 * i));
    port::NativeAudioRecord audio;
    assert(audio.parse(record));
    std::array<int16_t, 14> pcm;
    pcm.fill(1234);
    assert(audio.frame(0, raw.data(), coefficients.data(), -32768, 32767, pcm.data()));
    assert(pcm[13] == -32768);
    const auto expected = pcm;
    auto changed = raw;
    changed[7] ^= 1;
    assert(!audio.frame(0, changed.data(), coefficients.data(), -32768, 32767, pcm.data()));
    assert(!audio.frame(0, raw.data(), coefficients.data(), -32767, 32767, pcm.data()));
    auto badCoefficients = coefficients;
    badCoefficients[15] = 1;
    assert(!audio.frame(0, raw.data(), badCoefficients.data(), -32768, 32767, pcm.data()));
    assert(!audio.frame(1, raw.data(), coefficients.data(), -32768, 32767, pcm.data()));
    assert(pcm == expected);
    record[60] ^= 1;
    assert(!audio.parse(record));
    assert(!audio.frame(0, raw.data(), coefficients.data(), -32768, 32767, pcm.data()));
    // A minimal independent AU fixture validates the envelope, not H.264 decoding.
    const uint8_t es[] = { 0, 0, 0, 1, 9,    0xf0, 0, 0, 0, 1, 0x67, 66,
                           0, 0, 0, 1, 0x68, 1,    0, 0, 0, 1, 0x65, 1 };
    std::vector<uint8_t> video{ 'W', 'A', 'V', 'C' };
    be32(video, 1);
    be32(video, 16);
    be32(video, 16);
    be32(video, sizeof es);
    be32(video, 1);
    video.insert(video.end(), es, es + sizeof es);
    port::NativeVideoComponent component;
    assert(port::parse_native_video(video.data(), video.size(), component));
    assert(component.width == 16);
    const auto saved = component;
    video[video.size() - 2] = 0x61;
    assert(!port::parse_native_video(video.data(), video.size(), component));
    assert(component.elementary == saved.elementary);
    video[video.size() - 2] = 0x65;
    video[11] = 17;
    assert(!port::parse_native_video(video.data(), video.size(), component));
    const unsigned pitch = 32, w = 16, h = 16;
    std::vector<uint8_t> y(pitch * h), uv(pitch * h / 2);
    for (unsigned row = 0; row < h; ++row)
        for (unsigned col = 0; col < w; ++col)
            y[row * pitch + col] = row * w + col;
    for (unsigned row = 0; row < h / 2; ++row)
        for (unsigned col = 0; col < w / 2; ++col)
        {
            uv[row * pitch + 2 * col] = row * w / 2 + col;
            uv[row * pitch + 2 * col + 1] = 128 + row * w / 2 + col;
        }
    std::vector<uint8_t> ty(w * h + 2, 0xab), tu(w * h / 4 + 2, 0xab), tv = tu;
    assert(!port::tile_native_nv12(y.data(), y.size() - 1, uv.data(), uv.size(), pitch, w, h,
                                   ty.data() + 1, tu.data() + 1, tv.data() + 1));
    assert(ty[1] == 0xab);
    assert(port::tile_native_nv12(y.data(), y.size(), uv.data(), uv.size(), pitch, w, h,
                                  ty.data() + 1, tu.data() + 1, tv.data() + 1));
    assert(ty.front() == 0xab && ty.back() == 0xab && tu.front() == 0xab && tu.back() == 0xab &&
           tv.front() == 0xab && tv.back() == 0xab);
    unsigned n = 1;
    for (unsigned row = 0; row < h; row += 4)
        for (unsigned col = 0; col < w; col += 8)
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 8; ++c)
                    assert(ty[n++] == y[(row + r) * pitch + col + c]);
    n = 1;
    for (unsigned row = 0; row < h / 2; row += 4)
        for (unsigned col = 0; col < w / 2; col += 8)
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 8; ++c)
                {
                    assert(tu[n] == uv[(row + r) * pitch + 2 * (col + c)]);
                    assert(tv[n++] == uv[(row + r) * pitch + 2 * (col + c) + 1]);
                }
}
