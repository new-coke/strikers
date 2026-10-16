#pragma once
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
namespace port
{
inline uint32_t audio_word(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline int16_t audio_short(const uint8_t* p)
{
    return int16_t(uint16_t(p[0]) | uint16_t(p[1]) << 8);
}
inline uint64_t audio_hash(const void* data, size_t size, uint64_t h = 14695981039346656037ull)
{
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i)
        h = (h ^ p[i]) * 1099511628211ull;
    return h;
}
inline uint64_t audio_frame_key(const uint8_t* source, const int16_t* coef, int16_t y1, int16_t y2)
{
    uint8_t key[44];
    std::memcpy(key, source, 8);
    for (unsigned i = 0; i < 16; ++i)
    {
        key[8 + i * 2] = uint16_t(coef[i]) & 255;
        key[9 + i * 2] = uint16_t(coef[i]) >> 8;
    }
    key[40] = uint16_t(y1) & 255;
    key[41] = uint16_t(y1) >> 8;
    key[42] = uint16_t(y2) & 255;
    key[43] = uint16_t(y2) >> 8;
    return audio_hash(key, sizeof key);
}
struct NativeAudioRecord
{
    const uint8_t* bytes = nullptr;
    size_t sourceBytes = 0, frames = 0;
    bool parse(const std::vector<uint8_t>& b)
    {
        *this = {};
        if (b.size() < 60 || b.size() > 64 * 1024 || std::memcmp(b.data(), "STPCM001", 8))
            return false;
        const size_t raw = audio_word(b.data() + 8), samples = audio_word(b.data() + 12),
                     count = (samples + 13) / 14;
        if (!samples || !count || count > 1024 || raw != count * 8 ||
            b.size() != 52 + raw + count * 32 + 8)
            return false;
        const auto* tail = b.data() + b.size() - 8;
        if (audio_hash(b.data(), b.size() - 8) !=
            (uint64_t(audio_word(tail)) | uint64_t(audio_word(tail + 4)) << 32))
            return false;
        bytes = b.data();
        sourceBytes = raw;
        frames = count;
        return true;
    }
    bool frame(size_t index, const uint8_t* source, const int16_t* coefficients, int16_t y1,
               int16_t y2, int16_t* pcm) const
    {
        if (!bytes || index >= frames || !source || !coefficients || !pcm)
            return false;
        const auto* raw = bytes + 52 + index * 8;
        const auto* decoded = bytes + 52 + sourceBytes + index * 32;
        if (std::memcmp(source, raw, 8) || y1 != audio_short(decoded) ||
            y2 != audio_short(decoded + 2))
            return false;
        for (unsigned i = 0; i < 16; ++i)
            if (coefficients[i] != audio_short(bytes + 16 + i * 2))
                return false;
        for (unsigned i = 0; i < 14; ++i)
            pcm[i] = audio_short(decoded + 4 + i * 2);
        return true;
    }
};
}   // namespace port
