#pragma once
#include "port/native_texture_record.hpp"
#include <array>
#include <algorithm>
#include <cstring>
namespace port::render
{
struct TextureLevel
{
    unsigned width = 0, height = 0;
    std::vector<uint8_t> rgba;
};
// AVNR is an on-disk format; its old GPU swizzle is removed before GL upload.
inline size_t texture_morton(unsigned x, unsigned y, unsigned w, unsigned h) noexcept
{
    size_t n = 0;
    unsigned bit = 0;
    for (unsigned m = 1; m < w || m < h; m <<= 1)
    {
        if (m < h)
            n |= size_t(bool(y & m)) << bit++;
        if (m < w)
            n |= size_t(bool(x & m)) << bit++;
    }
    return n;
}
inline bool texture_level_rgba(const nativegxm::TextureUpload& image, unsigned level,
                               TextureLevel& out)
{
    using namespace nativegxm;
    out = {};
    const auto bytes = texture_upload_bytes(image);
    if (!bytes || image.pixels.size() != bytes || level >= image.mipCount)
        return false;
    unsigned w = image.width, h = image.height;
    size_t offset = 0;
    const unsigned bpp = image.storage == TextureStorage::Rgba8        ? 4
                         : image.storage == TextureStorage::Intensity8 ? 1
                                                                       : 2;
    for (unsigned i = 0; i < level; ++i)
    {
        offset += image.storage == TextureStorage::Bc1
                      ? size_t((w + 3) / 4) * ((h + 3) / 4) * 8
                      : size_t(image.swizzled ? w : (w + 7) & ~7u) * h * bpp;
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }
    out.width = w;
    out.height = h;
    out.rgba.resize(size_t(w) * h * 4);
    const auto* data = image.pixels.data() + offset;
    const auto rgb565 = [](unsigned c) {
        unsigned r = c >> 11, g = c >> 5 & 63, b = c & 31;
        return std::array<uint8_t, 4>{ uint8_t(r << 3 | r >> 2), uint8_t(g << 2 | g >> 4),
                                       uint8_t(b << 3 | b >> 2), 255 };
    };
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x)
        {
            std::array<uint8_t, 4> c{};
            if (image.storage == TextureStorage::Bc1)
            {
                const unsigned bw = (w + 3) / 4, bh = (h + 3) / 4;
                const auto block = image.swizzled ? texture_morton(x / 4, y / 4, bw, bh)
                                                  : size_t(y / 4) * bw + x / 4;
                const auto* p = data + block * 8;
                unsigned a = p[0] | unsigned(p[1]) << 8, b = p[2] | unsigned(p[3]) << 8;
                auto ca = rgb565(a), cb = rgb565(b);
                uint32_t bits =
                    p[4] | uint32_t(p[5]) << 8 | uint32_t(p[6]) << 16 | uint32_t(p[7]) << 24;
                unsigned code = bits >> (((y % 4) * 4 + x % 4) * 2) & 3;
                if (code == 0)
                    c = ca;
                else if (code == 1)
                    c = cb;
                else if (a > b)
                {
                    for (unsigned k = 0; k < 3; ++k)
                        c[k] = uint8_t((ca[k] * (code == 2 ? 2 : 1) + cb[k] * (code == 2 ? 1 : 2)) /
                                       3);
                    c[3] = 255;
                }
                else
                {
                    for (unsigned k = 0; k < 3; ++k)
                        c[k] = uint8_t((ca[k] + cb[k]) / 2);
                    c[3] = code == 3 ? 0 : 255;
                }
            }
            else
            {
                const size_t index =
                    image.swizzled ? texture_morton(x, y, w, h) : size_t(y) * ((w + 7) & ~7u) + x;
                const auto* p = data + index * bpp;
                switch (image.storage)
                {
                case TextureStorage::Rgba8:           std::memcpy(c.data(), p, 4); break;
                case TextureStorage::Intensity8:      c = { p[0], p[0], p[0], p[0] }; break;
                case TextureStorage::LuminanceAlpha8: c = { p[0], p[0], p[0], p[1] }; break;
                case TextureStorage::Rgb565:          c = rgb565(p[0] | unsigned(p[1]) << 8); break;
                default:                              return false;
                }
            }
            std::memcpy(out.rgba.data() + (size_t(y) * w + x) * 4, c.data(), 4);
        }
    return true;
}
}   // namespace port::render
