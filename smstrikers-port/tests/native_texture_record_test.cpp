#include "port/native_texture_record.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

using namespace port::nativegxm;
static void append(std::vector<uint8_t>& b, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(uint8_t(n >> (i * 8)));
}
static uint32_t word(const std::vector<uint8_t>& b, size_t n)
{
    return uint32_t(b[n]) | (uint32_t(b[n + 1]) << 8) | (uint32_t(b[n + 2]) << 16) |
           (uint32_t(b[n + 3]) << 24);
}
static std::vector<uint8_t> record(const std::vector<uint8_t>& req, unsigned storage,
                                   unsigned stride, unsigned mips, bool swizzled, size_t bytes)
{
    std::vector<uint8_t> payload;
    for (unsigned n : { storage, stride, mips, unsigned(swizzled) })
        append(payload, n);
    for (size_t i = 0; i < bytes; ++i)
        payload.push_back(uint8_t(i));
    const auto hash = native_asset_hash(payload);
    std::vector<uint8_t> result;
    for (uint32_t n : { 0x524e5641u, 1u, 1u, uint32_t(req.size()), uint32_t(payload.size()),
                        uint32_t(hash), uint32_t(hash >> 32) })
        append(result, n);
    result.insert(result.end(), req.begin(), req.end());
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
int main(int argc, char** argv)
{
    TextureSource src{};
    src.width = 2;
    src.height = 2;
    src.encoding = TextureEncoding::I8;
    std::vector<uint8_t> encoded(32, 77), request;
    src.pixels = encoded;
    assert(make_texture_request(src, request));
    assert(request.size() == 68 && word(request, 12) == 1 && word(request, 28) == 32);
    assert(texture_record_path(request).starts_with("native/v1/textures/"));
    auto bytes = record(request, 5, 2, 1, true, 16);
    TextureUpload upload;
    assert(parse_texture_record(bytes, request, upload));
    assert(upload.width == 2 && upload.height == 2 && upload.swizzled &&
           upload.storage == TextureStorage::Rgba8);
    assert(upload.pixels.size() == 16 && upload.pixels[3] == 3);
    // Malformed records never retain a previous successful image.
    auto corrupt = bytes;
    corrupt.back() ^= 1;
    assert(!parse_texture_record(corrupt, request, upload) && upload.pixels.empty());
    auto other = request;
    other.back() ^= 1;
    assert(!parse_texture_record(bytes, other, upload));
    corrupt = bytes;
    corrupt.push_back(0);
    assert(!parse_texture_record(corrupt, request, upload));
    assert(!parse_texture_record(std::span(bytes).first(27), request, upload));
    for (unsigned offset : { 0u, 4u, 8u, 12u, 16u, 20u, 24u })
    {
        corrupt = bytes;
        corrupt[offset] ^= 1;
        assert(!parse_texture_record(corrupt, request, upload));
    }
    assert(!parse_texture_record(record(request, 5, 2, 1, true, 15), request, upload));
    assert(!parse_texture_record(record(request, 4, 2, 1, true, 8), request,
                                 upload));   // BC1 requires CMPR source
    src.width = 3;
    src.height = 4;
    assert(make_texture_request(src, request));
    assert(!parse_texture_record(record(request, 5, 3, 1, true, 48), request,
                                 upload));   // NPOT swizzle
    assert(parse_texture_record(record(request, 5, 8, 1, false, 128), request, upload));
    assert(!parse_texture_record(record(request, 5, 3, 1, false, 128), request, upload));
    src.width = 4;
    src.height = 4;
    src.mipCount = 2;
    encoded.resize(64);
    src.pixels = encoded;
    assert(make_texture_request(src, request));
    assert(parse_texture_record(record(request, 5, 4, 2, true, 80), request, upload));
    assert(!parse_texture_record(record(request, 5, 4, 3, true, 84), request, upload));
    encoded.resize(63);
    src.pixels = encoded;
    assert(!make_texture_request(src, request) && request.empty());
    src = {};
    src.width = 0;
    src.height = 4;
    assert(!make_texture_request(src, request));
    src.width = 4097;
    assert(!make_texture_request(src, request));
    src.width = 8;
    src.height = 8;
    src.encoding = TextureEncoding::C4;
    encoded.resize(32);
    src.pixels = encoded;
    assert(!make_texture_request(src, request));
    std::vector<uint8_t> palette{ 0, 1, 2 };
    src.paletteEncoding = PaletteEncoding::Rgb5a3;
    src.palette = palette;
    assert(!make_texture_request(src, request));
    palette.push_back(3);
    src.palette = palette;
    assert(make_texture_request(src, request));
    assert(parse_texture_record(record(request, 5, 8, 1, true, 256), request, upload));
    if (argc > 1)
    {
        std::ifstream f(argv[1], std::ios::binary);
        std::vector<uint8_t> real((std::istreambuf_iterator<char>(f)), {});
        assert(f && real.size() >= 28);
        const uint32_t sourceBytes = word(real, 12);
        assert(sourceBytes <= real.size() - 28);
        assert(parse_texture_record(real, std::span(real).subspan(28, sourceBytes), upload));
        std::printf("native texture record: real %ux%u mips=%u storage=%u bytes=%zu validated\n",
                    upload.width, upload.height, upload.mipCount, unsigned(upload.storage),
                    upload.pixels.size());
    }
}
