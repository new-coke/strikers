#include "port/native_audio_record.hpp"
#include "port/native_video_record.hpp"
#include <cassert>
#include <array>
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
int main()
{
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
