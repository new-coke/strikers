#if (defined(PORT_VITA) && defined(STRIKERS_VITA_VITAGL)) || defined(STRIKERS_NATIVE_TEXTURE_TEST)
#include "port/native_texture_record.hpp"
#define XXH_INLINE_ALL
#include <xxhash.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace port::nativegxm
{
namespace
{
constexpr size_t kLimit = 16u * 1024u * 1024u;
uint32_t word(std::span<const uint8_t> b, size_t offset) noexcept
{
    return uint32_t(b[offset]) | (uint32_t(b[offset + 1]) << 8) | (uint32_t(b[offset + 2]) << 16) |
           (uint32_t(b[offset + 3]) << 24);
}
void append(std::vector<uint8_t>& out, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i)
        out.push_back(uint8_t(n >> (8 * i)));
}
bool dimensions(uint32_t w, uint32_t h, uint32_t levels) noexcept
{
    return w && h && w <= 4096 && h <= 4096 && levels && levels <= 13;
}
}   // namespace
uint64_t native_asset_hash(std::span<const uint8_t> bytes) noexcept
{
    return XXH64(bytes.data(), bytes.size(), 0);
}
size_t encoded_texture_bytes(uint32_t width, uint32_t height, TextureEncoding encoding,
                             uint32_t mipCount) noexcept
{
    if (!dimensions(width, height, mipCount) || uint32_t(encoding) > 11)
        return 0;
    constexpr std::array<std::array<unsigned, 3>, 12> tiles{ { { 8, 8, 32 },
                                                               { 8, 4, 32 },
                                                               { 8, 4, 32 },
                                                               { 4, 4, 32 },
                                                               { 4, 4, 32 },
                                                               { 4, 4, 32 },
                                                               { 4, 4, 64 },
                                                               { 8, 8, 32 },
                                                               { 8, 4, 32 },
                                                               { 4, 4, 32 },
                                                               { 8, 8, 32 },
                                                               { 1, 1, 4 } } };
    const auto t = tiles[uint32_t(encoding)];
    size_t total = 0;
    for (unsigned i = 0; i < mipCount; ++i)
    {
        const size_t w = std::max(1u, width >> i), h = std::max(1u, height >> i);
        const size_t bytes = ((w + t[0] - 1) / t[0]) * ((h + t[1] - 1) / t[1]) * t[2];
        if (bytes > kLimit - total)
            return 0;
        total += bytes;
    }
    return total;
}
bool make_texture_request(const TextureSource& s, std::vector<uint8_t>& request) noexcept
{
    request.clear();
    const size_t expected = encoded_texture_bytes(s.width, s.height, s.encoding, s.mipCount);
    const bool indexed = uint32_t(s.encoding) >= 7 && uint32_t(s.encoding) <= 9;
    if (!expected || s.pixels.size() != expected || s.palette.size() > 65536 ||
        s.palette.size() % 2 || uint32_t(s.paletteEncoding) > 3 ||
        (indexed && (s.palette.empty() || s.paletteEncoding == PaletteEncoding::None)) ||
        (!indexed && (!s.palette.empty() || s.paletteEncoding != PaletteEncoding::None)))
        return false;
    request.reserve(36 + expected + s.palette.size());
    for (uint32_t n : { 1u, s.width, s.height, uint32_t(s.encoding), uint32_t(s.paletteEncoding),
                        s.mipCount, 0u, uint32_t(expected), uint32_t(s.palette.size()) })
        append(request, n);
    request.insert(request.end(), s.pixels.begin(), s.pixels.end());
    request.insert(request.end(), s.palette.begin(), s.palette.end());
    return true;
}
std::string texture_record_path(std::span<const uint8_t> request)
{
    if (request.size() < 36)
        return {};
    char path[80];
    std::snprintf(path, sizeof(path), "native/v1/textures/%016llx.avnr",
                  static_cast<unsigned long long>(native_asset_hash(request)));
    return path;
}
bool parse_texture_record(std::span<const uint8_t> record, std::span<const uint8_t> request,
                          TextureUpload& out) noexcept
{
    out = {};
    if (record.size() < 28 || request.size() < 36 || request.size() > kLimit + 65536 + 36 ||
        word(record, 0) != 0x524e5641 || word(record, 4) != 1 || word(record, 8) != 1 ||
        word(request, 0) != 1)
        return false;
    const size_t srcBytes = word(record, 12), payloadBytes = word(record, 16);
    if (srcBytes != request.size() || payloadBytes < 16 || payloadBytes > kLimit ||
        srcBytes > record.size() - 28 || payloadBytes != record.size() - 28 - srcBytes)
        return false;
    const auto source = record.subspan(28, srcBytes),
               payload = record.subspan(28 + srcBytes, payloadBytes);
    if (!std::equal(source.begin(), source.end(), request.begin()) ||
        native_asset_hash(payload) !=
            (uint64_t(word(record, 20)) | (uint64_t(word(record, 24)) << 32)))
        return false;
    const uint32_t width = word(source, 4), height = word(source, 8),
                   sourceLevels = word(source, 20);
    const uint32_t storage = word(payload, 0), stride = word(payload, 4), levels = word(payload, 8),
                   swizzled = word(payload, 12);
    if (!dimensions(width, height, levels) || levels != sourceLevels || storage < 1 ||
        storage > 5 || swizzled > 1 || word(source, 12) > 11 || word(source, 16) > 3 ||
        word(source, 24) != 0)
        return false;
    const size_t sourcePixels = word(source, 28), paletteBytes = word(source, 32);
    if (sourcePixels !=
            encoded_texture_bytes(width, height, TextureEncoding(word(source, 12)), sourceLevels) ||
        paletteBytes > 65536 || paletteBytes % 2 || sourcePixels > source.size() - 36 ||
        paletteBytes != source.size() - 36 - sourcePixels)
        return false;
    const bool indexed = word(source, 12) >= 7 && word(source, 12) <= 9;
    if ((indexed && (!paletteBytes || word(source, 16) == 0)) ||
        (!indexed && (paletteBytes || word(source, 16) != 0)))
        return false;
    if ((storage == 1 && word(source, 12) > 1) ||
        (storage == 2 && (word(source, 12) < 2 || word(source, 12) > 3)) ||
        (storage == 3 && word(source, 12) != 4) || (storage == 4 && word(source, 12) != 10))
        return false;
    if (stride != (swizzled || storage == 4 ? width : (width + 7u) & ~7u))
        return false;
    if (swizzled && ((width & (width - 1)) || (height & (height - 1))))
        return false;
    size_t expected = 0;
    for (unsigned i = 0; i < levels; ++i)
    {
        const size_t w = std::max(1u, width >> i), h = std::max(1u, height >> i);
        const unsigned bpp = storage == 1 ? 1 : storage == 5 ? 4 : 2;
        const size_t bytes = storage == 4 ? ((w + 3) / 4) * ((h + 3) / 4) * 8
                                          : (swizzled ? w : (w + 7u) & ~size_t(7)) * h * bpp;
        if (bytes > kLimit - expected)
            return false;
        expected += bytes;
    }
    if (expected != payload.size() - 16)
        return false;
    out.width = width;
    out.height = height;
    out.stride = stride;
    out.mipCount = levels;
    out.storage = TextureStorage(storage);
    out.swizzled = swizzled != 0;
    out.pixels.assign(payload.begin() + 16, payload.end());
    return true;
}

namespace
{
using Pixel = std::array<uint8_t, 4>;
uint16_t be16(const uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
Pixel rgb565(uint16_t c)
{
    unsigned r = c >> 11, g = (c >> 5) & 63, b = c & 31;
    return { uint8_t((r << 3) | (r >> 2)), uint8_t((g << 2) | (g >> 4)),
             uint8_t((b << 3) | (b >> 2)), 255 };
}
Pixel rgb5a3(uint16_t c)
{
    if (c & 32768)
    {
        auto p = rgb565(uint16_t(((c >> 10) & 31) << 11 | ((c >> 5) & 31) << 6 | (c & 31)));
        unsigned g = (c >> 5) & 31;
        p[1] = uint8_t((g << 3) | (g >> 2));
        return p;
    }
    return { uint8_t(((c >> 8) & 15) * 17), uint8_t(((c >> 4) & 15) * 17), uint8_t((c & 15) * 17),
             uint8_t((((c >> 12) & 7) << 5) | (((c >> 12) & 7) << 2) | (((c >> 12) & 7) >> 1)) };
}
// GXM interleaves y then x, continuing with the longer dimension.
size_t morton(unsigned x, unsigned y, unsigned w, unsigned h)
{
    size_t n = 0;
    unsigned bit = 0;
    for (unsigned mask = 1; mask < w || mask < h; mask <<= 1)
    {
        if (mask < h)
            n |= size_t((y & mask) != 0) << bit++;
        if (mask < w)
            n |= size_t((x & mask) != 0) << bit++;
    }
    return n;
}
}   // namespace
bool decode_texture(const TextureSource& s, TextureUpload& out) noexcept
{
    out = {};
    const size_t bytes = encoded_texture_bytes(s.width, s.height, s.encoding, s.mipCount);
    if (!bytes || bytes != s.pixels.size())
        return false;
    try
    {
        TextureUpload image;
        image.width = s.width;
        image.height = s.height;
        image.mipCount = s.mipCount;
        image.storage = TextureStorage::Rgba8;
        image.swizzled = !(s.width & (s.width - 1)) && !(s.height & (s.height - 1));
        image.stride = image.swizzled ? s.width : (s.width + 7u) & ~7u;
        const size_t target = texture_upload_bytes(image);
        if (!target)
            return false;
        image.pixels.resize(target);
        constexpr unsigned tw[]{ 8, 8, 8, 4, 4, 4, 4, 8, 8, 4, 8, 1 },
            th[]{ 8, 4, 4, 4, 4, 4, 4, 8, 4, 4, 8, 1 },
            tb[]{ 32, 32, 32, 32, 32, 32, 64, 32, 32, 32, 32, 4 };
        const unsigned fmt = unsigned(s.encoding);
        size_t input = 0, output = 0;
        auto palette = [&](unsigned index, Pixel& p) {
            if (s.paletteEncoding == PaletteEncoding::None || index >= s.palette.size() / 2)
                return false;
            uint16_t c = be16(s.palette.data() + index * 2);
            if (s.paletteEncoding == PaletteEncoding::Rgb565)
                p = rgb565(c);
            else if (s.paletteEncoding == PaletteEncoding::Rgb5a3)
                p = rgb5a3(c);
            else
                p = { uint8_t(c), uint8_t(c), uint8_t(c), uint8_t(c >> 8) };
            return true;
        };
        for (unsigned level = 0; level < s.mipCount; ++level)
        {
            const unsigned w = std::max(1u, s.width >> level), h = std::max(1u, s.height >> level),
                           stride = image.swizzled ? w : (w + 7u) & ~7u;
            for (unsigned y = 0; y < h; ++y)
                for (unsigned x = 0; x < w; ++x)
                {
                    const size_t tile =
                        ((y / th[fmt]) * ((w + tw[fmt] - 1) / tw[fmt]) + x / tw[fmt]) * tb[fmt];
                    const unsigned local = (y % th[fmt]) * tw[fmt] + x % tw[fmt];
                    const auto* p = s.pixels.data() + input + tile;
                    Pixel c{};
                    switch (s.encoding)
                    {
                    case TextureEncoding::I4:
                    {
                        unsigned i = (p[local / 2] >> ((local & 1) ? 0 : 4) & 15) * 17;
                        c = { uint8_t(i), uint8_t(i), uint8_t(i), uint8_t(i) };
                        break;
                    }
                    case TextureEncoding::I8: c = { p[local], p[local], p[local], p[local] }; break;
                    case TextureEncoding::IA4:
                    {
                        unsigned i = (p[local] & 15) * 17;
                        c = { uint8_t(i), uint8_t(i), uint8_t(i), uint8_t((p[local] >> 4) * 17) };
                        break;
                    }
                    case TextureEncoding::IA8:
                        c = { p[local * 2 + 1], p[local * 2 + 1], p[local * 2 + 1], p[local * 2] };
                        break;
                    case TextureEncoding::Rgb565: c = rgb565(be16(p + local * 2)); break;
                    case TextureEncoding::Rgb5a3: c = rgb5a3(be16(p + local * 2)); break;
                    case TextureEncoding::Rgba8:
                        c = { p[local * 2 + 1], p[32 + local * 2], p[33 + local * 2],
                              p[local * 2] };
                        break;
                    case TextureEncoding::C4:
                        if (!palette(p[local / 2] >> ((local & 1) ? 0 : 4) & 15, c))
                            return false;
                        break;
                    case TextureEncoding::C8:
                        if (!palette(p[local], c))
                            return false;
                        break;
                    case TextureEncoding::C14x2:
                        if (!palette(be16(p + local * 2) & 16383, c))
                            return false;
                        break;
                    case TextureEncoding::LinearRgba8: std::memcpy(c.data(), p, 4); break;
                    case TextureEncoding::Cmpr:
                    {
                        const unsigned sx = x & 7, sy = y & 7;
                        const auto* b = p + ((sy / 4) * 2 + sx / 4) * 8;
                        uint16_t a = be16(b), d = be16(b + 2);
                        Pixel ca = rgb565(a), cb = rgb565(d);
                        unsigned code = (b[4 + (sy & 3)] >> (6 - (sx & 3) * 2)) & 3;
                        if (code == 0)
                            c = ca;
                        else if (code == 1)
                            c = cb;
                        else if (a > d)
                        {
                            for (unsigned k = 0; k < 3; ++k)
                                c[k] = uint8_t(
                                    (ca[k] * (code == 2 ? 5 : 3) + cb[k] * (code == 2 ? 3 : 5)) >>
                                    3);
                            c[3] = 255;
                        }
                        else
                        {
                            for (unsigned k = 0; k < 3; ++k)
                                c[k] = uint8_t((ca[k] + cb[k]) / 2);
                            c[3] = code == 2 ? 255 : 0;
                        }
                        break;
                    }
                    }
                    const size_t index =
                        image.swizzled ? morton(x, y, w, h) : size_t(y) * stride + x;
                    std::memcpy(image.pixels.data() + output + index * 4, c.data(), 4);
                }
            input += size_t((w + tw[fmt] - 1) / tw[fmt]) * ((h + th[fmt] - 1) / th[fmt]) * tb[fmt];
            output += size_t(stride) * h * 4;
        }
        out = std::move(image);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
}   // namespace port::nativegxm
#endif
