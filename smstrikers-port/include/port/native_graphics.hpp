#pragma once

#include "port/native_texture_record.hpp"
#include <cstring>

namespace port::assets
{
inline uint16_t half(const void* p) noexcept
{
    const auto* b = static_cast<const uint8_t*>(p);
    return uint16_t(b[0]) | uint16_t(b[1]) << 8;
}
inline uint32_t word(const void* p) noexcept
{
    const auto* b = static_cast<const uint8_t*>(p);
    return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
}
inline float scalar(const void* p) noexcept
{
    const uint32_t bits = word(p);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
inline bool magic(std::span<const uint8_t> b, const char* expected) noexcept
{
    return b.size() >= 16 && !std::memcmp(b.data(), expected, 8) && word(b.data() + 8) == b.size();
}
enum ModelSection : uint32_t
{
    Group = 1,
    References = 2,
    Models = 3,
    Packets = 4,
    Streams = 5,
    Vertices = 6,
    Indices = 7,
    Skin = 8,
    TextureAnimation = 9,
    VertexAnimation = 10,
    Materials = 11
};
enum SkinSection : uint32_t
{
    BoneMatrices = 1,
    BoneMap = 2,
    Morphs = 3,
    SoftwareVertices = 4,
    Pairs = 5,
    Stitching = 6
};
struct Section
{
    uint32_t type = 0;
    std::span<const uint8_t> payload{};
    const uint8_t* next = nullptr;
};
inline bool section(const uint8_t* p, const uint8_t* end, Section& out) noexcept
{
    out = {};
    if (p > end || size_t(end - p) < 16 || word(p + 8) || word(p + 12))
        return false;
    const size_t n = word(p + 4), available = size_t(end - p) - 16;
    if (n > available || ((n + 15u) & ~size_t(15)) > available)
        return false;
    out = { word(p), { p + 16, n }, p + 16 + ((n + 15u) & ~size_t(15)) };
    return true;
}
inline bool texture(std::span<const uint8_t> b, nativegxm::TextureUpload& out,
                    uint32_t* semantic = nullptr, uint8_t* bits = nullptr, bool* missing = nullptr)
{
    out = {};
    if (b.size() < 64 || !magic(b, "NSTX0001") || word(b.data() + 28) > 8 ||
        word(b.data() + 36) > 1 || word(b.data() + 44) || word(b.data() + 56) ||
        word(b.data() + 60))
        return false;
    nativegxm::TextureUpload image;
    image.width = word(b.data() + 12);
    image.height = word(b.data() + 16);
    image.mipCount = word(b.data() + 20);
    image.stride = word(b.data() + 24);
    image.storage = nativegxm::TextureStorage::Rgba8;
    image.swizzled = false;
    const size_t n = nativegxm::texture_upload_bytes(image);
    if (!n || n != word(b.data() + 40) || n != b.size() - 64 ||
        nativegxm::native_asset_hash(b.subspan(64)) !=
            (uint64_t(word(b.data() + 48)) | uint64_t(word(b.data() + 52)) << 32))
        return false;
    if (semantic)
        *semantic = word(b.data() + 28);
    if (bits)
        std::memcpy(bits, b.data() + 32, 4);
    if (missing)
        *missing = word(b.data() + 36) != 0;
    image.pixels.assign(b.begin() + 64, b.end());
    out = std::move(image);
    return true;
}
inline bool texture_bundle(std::span<const uint8_t> b) noexcept
{
    if (!magic(b, "NSTB0001"))
        return false;
    const size_t count = word(b.data() + 12);
    if (count > (b.size() - 16) / 16)
        return false;
    size_t previous = 16 + count * 16;
    for (size_t i = 0; i < count; ++i)
    {
        const auto* entry = b.data() + 16 + i * 16;
        const size_t at = word(entry + 4), n = word(entry + 8);
        if (word(entry + 12) || at < previous || at % 16 || at > b.size() || n > b.size() - at ||
            n < 64 || !magic(b.subspan(at, n), "NSTX0001"))
            return false;
        previous = at + n;
    }
    return true;
}
template <class Map, class Admit>
bool admit_texture_bundle(std::span<const uint8_t> bytes, Map map, Admit admit)
{
    if (!texture_bundle(bytes))
        return false;
    for (size_t i = 0; i < word(bytes.data() + 12); ++i)
    {
        const auto* entry = bytes.data() + 16 + i * 16;
        const uint32_t handle = map(word(entry));
        if (handle == UINT32_MAX)
            continue;
        if (!admit(handle, bytes.subspan(word(entry + 4), word(entry + 8))))
            return false;
    }
    return true;
}
bool validate_model(std::span<const uint8_t> b) noexcept;
bool validate_skin(std::span<const uint8_t> b) noexcept;
using VerificationProgress = void (*)(unsigned current, unsigned total) noexcept;
int verify_archive_if_requested(VerificationProgress progress = nullptr) noexcept;
}   // namespace port::assets
