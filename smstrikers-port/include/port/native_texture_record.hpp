#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace port::nativegxm
{

// The numeric values below are the public AVNR asset-file ABI, not GX or
// Aurora runtime descriptors. No GPU handle or source address is serialized.
enum class TextureEncoding : uint32_t
{
    I4,
    I8,
    IA4,
    IA8,
    Rgb565,
    Rgb5a3,
    Rgba8,
    C4,
    C8,
    C14x2,
    Cmpr,
    LinearRgba8
};
enum class PaletteEncoding : uint32_t
{
    None,
    IA8,
    Rgb565,
    Rgb5a3
};
enum class TextureStorage : uint32_t
{
    None,
    Intensity8,
    LuminanceAlpha8,
    Rgb565,
    Bc1,
    Rgba8
};

struct TextureSource
{
    uint32_t width = 0, height = 0, mipCount = 1;
    TextureEncoding encoding = TextureEncoding::Rgba8;
    PaletteEncoding paletteEncoding = PaletteEncoding::None;
    std::span<const uint8_t> pixels{}, palette{};
};
struct TextureUpload
{
    uint32_t width = 0, height = 0, stride = 0, mipCount = 0;
    TextureStorage storage = TextureStorage::None;
    bool swizzled = false;
    std::vector<uint8_t> pixels{};
};
inline size_t texture_upload_bytes(const TextureUpload& image) noexcept
{
    if (!image.width || !image.height || image.width > 4096 || image.height > 4096 ||
        !image.mipCount || image.mipCount > 13 || uint32_t(image.storage) < 1 ||
        uint32_t(image.storage) > 5)
        return 0;
    const bool bc1 = image.storage == TextureStorage::Bc1;
    if (image.stride != (image.swizzled || bc1 ? image.width : (image.width + 7u) & ~7u) ||
        (image.swizzled &&
         ((image.width & (image.width - 1)) || (image.height & (image.height - 1)))))
        return 0;
    size_t total = 0;
    for (unsigned i = 0; i < image.mipCount; ++i)
    {
        const size_t w = (image.width >> i) ? (image.width >> i) : 1u,
                     h = (image.height >> i) ? (image.height >> i) : 1u;
        const unsigned bpp = image.storage == TextureStorage::Intensity8 ? 1
                             : image.storage == TextureStorage::Rgba8    ? 4
                                                                         : 2;
        const size_t bytes = bc1 ? ((w + 3) / 4) * ((h + 3) / 4) * 8
                                 : (image.swizzled ? w : (w + 7u) & ~size_t(7)) * h * bpp;
        if (bytes > 16u * 1024u * 1024u - total)
            return 0;
        total += bytes;
    }
    return total;
}

uint64_t native_asset_hash(std::span<const uint8_t> bytes) noexcept;
size_t encoded_texture_bytes(uint32_t width, uint32_t height, TextureEncoding encoding,
                             uint32_t mipCount) noexcept;
bool make_texture_request(const TextureSource& source, std::vector<uint8_t>& request) noexcept;
std::string texture_record_path(std::span<const uint8_t> request);
// Verify complete source bytes, dimensions, mip extents, payload checksum and
// format before publishing an owned immutable GPU upload. Failure clears out.
bool parse_texture_record(std::span<const uint8_t> record, std::span<const uint8_t> expectedRequest,
                          TextureUpload& out) noexcept;
// Decode the game's tiled texture and palette bytes directly. Power-of-two
// images use native Morton storage, including mip levels, so repeat sampling
// never depends on the linear-texture clamp restriction.
bool decode_texture(const TextureSource& source, TextureUpload& out) noexcept;

}   // namespace port::nativegxm
