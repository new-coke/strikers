#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace port
{
inline uint32_t video_word(const uint8_t* p)
{
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
struct NativeVideoComponent
{
    uint32_t width = 0, height = 0;
    const uint8_t* elementary = nullptr;
    size_t bytes = 0;
};
inline bool parse_native_video(const void* source, size_t size, NativeVideoComponent& out)
{
    if (!source || size < 24 || size > 1024 * 1024)
        return false;
    const auto* p = static_cast<const uint8_t*>(source);
    const uint32_t w = video_word(p + 8), h = video_word(p + 12), n = video_word(p + 16);
    if (std::memcmp(p, "WAVC", 4) || video_word(p + 4) != 1 || video_word(p + 20) != 1 || w < 16 ||
        w > 704 || h < 16 || h > 768 || (w % 16) || (h % 16) || !n || n > size - 24 ||
        size - 24 - n > 3)
        return false;
    bool sps = false, pps = false, aud = false, idr = false;
    size_t at = 0;
    while (at < n)
    {
        size_t start = at;
        // An AU consists only of Annex B NALs. No hidden leading bytes.
        if (at + 4 <= n && !p[24 + at] && !p[25 + at] && !p[26 + at] && p[27 + at] == 1)
            at += 4;
        else if (at + 3 <= n && !p[24 + at] && !p[25 + at] && p[26 + at] == 1)
            at += 3;
        else
            return false;
        if (at >= n)
            return false;
        const auto type = p[24 + at] & 31;
        if (type == 1 || type == 2 || type == 3 || type == 4)
            return false;
        if (type == 7)
        {
            if (at + 1 >= n || p[25 + at] != 66)
                return false;
            sps = true;
        }
        pps |= type == 8;
        aud |= type == 9;
        idr |= type == 5;
        if (!start && type != 9)
            return false;
        ++at;
        while (at < n)
        {
            if (at + 3 <= n && !p[24 + at] && !p[25 + at] &&
                (p[26 + at] == 1 || (at + 4 <= n && !p[26 + at] && p[27 + at] == 1)))
                break;
            ++at;
        }
    }
    if (!sps || !pps || !aud || !idr)
        return false;
    NativeVideoComponent staged;
    staged.width = w;
    staged.height = h;
    staged.elementary = p + 24;
    staged.bytes = n;
    out = staged;
    return true;
}
// The existing movie renderer owns exact w*h and w*h/4 GX I8 destinations.
// Validate every extent before writing any of them.
inline bool tile_native_nv12(const uint8_t* y, size_t yBytes, const uint8_t* uv, size_t uvBytes,
                             unsigned pitch, unsigned w, unsigned h, void* tileY, void* tileU,
                             void* tileV)
{
    if (!y || !uv || !tileY || !tileU || !tileV || w < 16 || w > 704 || h < 16 || h > 768 ||
        w % 16 || h % 16 || pitch < w || pitch > 704 || yBytes < size_t(pitch) * h ||
        uvBytes < size_t(pitch) * h / 2)
        return false;
    auto* dy = static_cast<uint8_t*>(tileY);
    auto* du = static_cast<uint8_t*>(tileU);
    auto* dv = static_cast<uint8_t*>(tileV);
    for (unsigned row = 0; row < h; row += 4)
        for (unsigned col = 0; col < w; col += 8)
            for (unsigned r = 0; r < 4; ++r)
            {
                std::memcpy(dy, y + size_t(row + r) * pitch + col, 8);
                dy += 8;
            }
    for (unsigned row = 0; row < h / 2; row += 4)
        for (unsigned col = 0; col < w / 2; col += 8)
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 8; ++c)
                {
                    const auto at = size_t(row + r) * pitch + 2 * (col + c);
                    *du++ = uv[at];
                    *dv++ = uv[at + 1];
                }
    return true;
}
}   // namespace port
