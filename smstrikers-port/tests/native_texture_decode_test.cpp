#include "port/native_texture_record.hpp"
#include "port/asset_archive.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>
using namespace port::nativegxm;

// Standalone host gates can also link the hash-pinned pre-change decoder with
// renamed exports, comparing the two actual implementations in one process.
#if defined(STRIKERS_I8_COMPARE_BASELINE)
namespace port::nativegxm
{
bool decode_texture_before(const TextureSource&, TextureUpload&) noexcept;
}
#endif

namespace
{
bool failNextAllocation = false;
uint64_t i8Cases = 0, i8Bytes = 0;

bool same_image(const TextureUpload& a, const TextureUpload& b)
{
    return a.width == b.width && a.height == b.height && a.stride == b.stride &&
           a.mipCount == b.mipCount && a.storage == b.storage && a.swizzled == b.swizzled &&
           a.pixels == b.pixels;
}

// Independent per-coordinate oracle: preserve the old division/modulo address
// equation, not the new tile/pointer traversal. All padding starts at zero.
TextureUpload reference_i8(const TextureSource& s)
{
    TextureUpload result;
    result.width = s.width;
    result.height = s.height;
    result.mipCount = s.mipCount;
    result.storage = TextureStorage::Rgba8;
    result.swizzled = !(s.width & (s.width - 1)) && !(s.height & (s.height - 1));
    result.stride = result.swizzled ? s.width : ((s.width + 7) / 8) * 8;
    size_t input = 0;
    for (unsigned level = 0; level < s.mipCount; ++level)
    {
        const unsigned w = std::max(1u, s.width >> level), h = std::max(1u, s.height >> level);
        const size_t stride = result.swizzled ? w : ((w + 7) / 8) * 8;
        const size_t output = result.pixels.size();
        result.pixels.resize(output + stride * h * 4, 0);
        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x)
            {
                const size_t source = input + ((y / 4) * ((w + 7) / 8) + x / 8) * 32 +
                                      (y % 4) * 8 + x % 8;
                size_t index = size_t(y) * stride + x;
                if (result.swizzled)
                {
                    index = 0;
                    unsigned outBit = 0;
                    for (unsigned axisBit = 0; (1u << axisBit) < w || (1u << axisBit) < h; ++axisBit)
                    {
                        if ((1u << axisBit) < h) index |= size_t((y >> axisBit) & 1) << outBit++;
                        if ((1u << axisBit) < w) index |= size_t((x >> axisBit) & 1) << outBit++;
                    }
                }
                for (unsigned channel = 0; channel < 4; ++channel)
                    result.pixels[output + index * 4 + channel] = s.pixels[source];
            }
        input += size_t((w + 7) / 8) * ((h + 3) / 4) * 32;
    }
    assert(input == s.pixels.size());
    return result;
}

void check_i8_case(unsigned w, unsigned h, unsigned levels, uint32_t seed, bool constant = false)
{
    TextureSource source;
    source.width = w;
    source.height = h;
    source.mipCount = levels;
    source.encoding = TextureEncoding::I8;
    std::vector<uint8_t> input(encoded_texture_bytes(w, h, source.encoding, levels));
    assert(!input.empty());
    uint32_t random = seed;
    for (auto& byte : input)
    {
        random = random * 1664525u + 1013904223u;
        byte = constant ? uint8_t(seed) : uint8_t(random >> 24);
    }
    source.pixels = input;
    const auto expected = reference_i8(source);
    TextureUpload actual;
    assert(decode_texture(source, actual));
    assert(same_image(actual, expected));
#if defined(STRIKERS_I8_COMPARE_BASELINE)
    TextureUpload before;
    assert(decode_texture_before(source, before));
    assert(same_image(actual, before));
#endif
    ++i8Cases;
    i8Bytes += actual.pixels.size();
}

void check_i8_failures()
{
    TextureSource source;
    source.width = 9;
    source.height = 5;
    source.encoding = TextureEncoding::I8;
    std::vector<uint8_t> input(encoded_texture_bytes(9, 5, TextureEncoding::I8, 1), 0x87);
    source.pixels = input;
    const TextureUpload empty;
    const auto rejected = [&](const TextureSource& bad, bool failAllocation = false) {
        TextureUpload actual;
        actual.width = actual.height = actual.stride = actual.mipCount = 7;
        actual.storage = TextureStorage::Rgb565;
        actual.swizzled = true;
        actual.pixels.assign(16, 0xff);
        failNextAllocation = failAllocation;
        const bool accepted = decode_texture(bad, actual);
        assert(!failNextAllocation);
        assert(!accepted && same_image(actual, empty));
#if defined(STRIKERS_I8_COMPARE_BASELINE)
        actual.width = 9;
        actual.pixels.assign(16, 0xee);
        failNextAllocation = failAllocation;
        assert(!decode_texture_before(bad, actual));
        assert(!failNextAllocation && same_image(actual, empty));
#endif
    };
    rejected(source, true);
    for (unsigned dimension : {0u, 4097u})
    {
        auto bad = source;
        bad.width = dimension;
        rejected(bad);
        bad = source;
        bad.height = dimension;
        rejected(bad);
    }
    for (unsigned levels : {0u, 14u})
    {
        auto bad = source;
        bad.mipCount = levels;
        rejected(bad);
    }
    auto bad = source;
    bad.pixels = bad.pixels.first(bad.pixels.size() - 1);
    rejected(bad);
    input.push_back(0x91);
    bad = source;
    bad.pixels = input;
    rejected(bad);
    // Valid encoded extent, but expanded RGBA exceeds the unchanged 16 MiB cap.
    bad.width = 4096;
    bad.height = 1025;
    input.resize(encoded_texture_bytes(bad.width, bad.height, bad.encoding, 1));
    bad.pixels = input;
    rejected(bad);
}

void check_i8()
{
    for (unsigned value = 0; value < 256; ++value)
        check_i8_case(9, 5, 5, value, true);
    for (unsigned w = 1; w <= 19; ++w)
        for (unsigned h = 1; h <= 11; ++h)
            for (unsigned levels : {1u, 2u, 5u, 13u})
                check_i8_case(w, h, levels, (w * 65537u + h * 257u) ^ levels);
    constexpr std::array<std::array<unsigned, 2>, 11> shapes{{
        {512, 416}, {256, 208}, {513, 417}, {255, 207}, {1, 4095}, {4095, 1},
        {4096, 3}, {3, 4096}, {127, 257}, {256, 512}, {512, 256}}};
    for (const auto& shape : shapes)
        for (unsigned levels : {1u, 2u, 10u, 13u})
            check_i8_case(shape[0], shape[1], levels, shape[0] ^ (shape[1] << 16) ^ levels);
    // Full movie Y/U/V extents with changing data across repeated decodes.
    for (unsigned frame = 0; frame < 32; ++frame)
    {
        check_i8_case(512, 416, 1, 0x12345678u + frame);
        check_i8_case(256, 208, 1, 0x87654321u + frame);
        check_i8_case(256, 208, 1, 0xabcdef01u + frame);
    }
    check_i8_failures();
    std::printf("I8 oracle: %llu cases, %llu RGBA bytes identical, mip/edge/padding/failure checks passed\n",
                static_cast<unsigned long long>(i8Cases), static_cast<unsigned long long>(i8Bytes));
}
} // namespace

void* operator new(std::size_t bytes)
{
    if (failNextAllocation)
    {
        failNextAllocation = false;
        throw std::bad_alloc();
    }
    if (void* p = std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
uint32_t word(std::span<const uint8_t> b, unsigned offset)
{
    return uint32_t(b[offset]) | (uint32_t(b[offset + 1]) << 8) | (uint32_t(b[offset + 2]) << 16) |
           (uint32_t(b[offset + 3]) << 24);
}
int main(int argc, char** argv)
{
    check_i8();
    TextureSource s;
    s.width = s.height = 4;
    s.encoding = TextureEncoding::Rgba8;
    std::vector<uint8_t> encoded(64);
    for (unsigned i = 0; i < 16; ++i)
    {
        encoded[2 * i] = uint8_t(255 - i);
        encoded[2 * i + 1] = uint8_t(i);
        encoded[32 + 2 * i] = uint8_t(i + 16);
        encoded[33 + 2 * i] = uint8_t(i + 32);
    }
    s.pixels = encoded;
    TextureUpload image;
    assert(decode_texture(s, image));
    // Independently ordered Morton coordinates for a 4x4 GXM image.
    constexpr unsigned order[]{ 0, 4, 1, 5, 8, 12, 9, 13, 2, 6, 3, 7, 10, 14, 11, 15 };
    for (unsigned i = 0; i < 16; ++i)
    {
        unsigned original = order[i];
        assert(image.pixels[i * 4] == original && image.pixels[i * 4 + 1] == original + 16 &&
               image.pixels[i * 4 + 2] == original + 32 &&
               image.pixels[i * 4 + 3] == 255 - original);
    }
    s.encoding = TextureEncoding::Rgb5a3;
    encoded.resize(32);
    std::fill(encoded.begin(), encoded.end(), 0);
    encoded[0] = 0xFC;
    encoded[1] = 0;
    s.pixels = encoded;
    assert(decode_texture(s, image));
    assert(image.pixels[0] == 255 && image.pixels[1] == 0 && image.pixels[2] == 0 &&
           image.pixels[3] == 255);
    s.encoding = TextureEncoding::C8;
    encoded.resize(32);
    encoded.assign(32, 0);
    s.pixels = encoded;
    std::vector<uint8_t> palette{ 0xFC, 0 };
    s.palette = palette;
    s.paletteEncoding = PaletteEncoding::Rgb5a3;
    assert(decode_texture(s, image) && image.pixels[0] == 255);
    encoded[0] = 1;
    s.pixels = encoded;
    assert(!decode_texture(s, image) && image.pixels.empty());
    s.palette = {};
    s.paletteEncoding = PaletteEncoding::None;
    s.encoding = TextureEncoding::Cmpr;
    s.width = s.height = 8;
    encoded.assign(32, 0);
    for (unsigned i = 0; i < 4; ++i)
    {
        encoded[i * 8] = 0xF8;
        encoded[i * 8 + 2] = 0x07;
        encoded[i * 8 + 3] = 0xE0;
    }
    s.pixels = encoded;
    assert(decode_texture(s, image));
    for (unsigned i = 0; i < 64; ++i)
        assert(image.pixels[i * 4] == 255 && image.pixels[i * 4 + 3] == 255);
    if (argc < 2)
        return 0;
    char error[256];
    auto* archive = port_asset_archive_open(argv[1], error, sizeof error);
    if (!archive)
    {
        std::fprintf(stderr, "%s\n", error);
        return 1;
    }
    unsigned records = 0, matched = 0, bc1 = 0;
    for (unsigned i = 0; i < port_asset_archive_count(archive); ++i)
    {
        const char* path = port_asset_archive_path(archive, i);
        if (!std::string_view(path).starts_with("native/v1/textures/"))
            continue;
        std::vector<uint8_t> record(port_asset_archive_size(archive, i));
        assert(port_asset_archive_read(archive, i, record.data(), record.size(), 0) ==
               long(record.size()));
        const auto request = std::span(record).subspan(28, word(record, 12));
        TextureUpload expected;
        assert(parse_texture_record(record, request, expected));
        TextureSource source;
        source.width = word(request, 4);
        source.height = word(request, 8);
        source.encoding = TextureEncoding(word(request, 12));
        source.paletteEncoding = PaletteEncoding(word(request, 16));
        source.mipCount = word(request, 20);
        source.pixels = request.subspan(36, word(request, 28));
        source.palette = request.subspan(36 + word(request, 28), word(request, 32));
        TextureUpload decoded;
        assert(decode_texture(source, decoded));
        ++records;
        if (expected.storage == TextureStorage::Rgba8)
        {
            assert(decoded.pixels.size() == expected.pixels.size());
            if (decoded.pixels != expected.pixels)
            {
                std::fprintf(stderr, "Mismatch %s format=%u %ux%u swizzled=%u\n", path,
                             unsigned(source.encoding), source.width, source.height,
                             expected.swizzled);
                for (size_t k = 0, n = 0; k < decoded.pixels.size() && n < 12; ++k)
                    if (decoded.pixels[k] != expected.pixels[k])
                    {
                        std::fprintf(stderr, "byte %zu decoded=%u expected=%u\n", k,
                                     decoded.pixels[k], expected.pixels[k]);
                        ++n;
                    }
                return 2;
            }
            ++matched;
        }
        else
        {
            assert(expected.storage == TextureStorage::Bc1);
            ++bc1;
        }
    }
    port_asset_archive_close(archive);
    std::printf("native decoder: %u real records decoded; %u byte-identical RGBA; %u BC1 validated "
                "and decoded\n",
                records, matched, bc1);
    assert(records && matched);
}
