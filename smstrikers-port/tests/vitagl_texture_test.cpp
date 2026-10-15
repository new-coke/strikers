#include "port/vitagl_texture.hpp"
#include "port/asset_archive.h"
#include <cassert>
#include <cstdio>
#include <string_view>
using namespace port::nativegxm;
using namespace port::render;
static unsigned word(std::span<const uint8_t> b, unsigned o)
{
    return b[o] | unsigned(b[o + 1]) << 8 | unsigned(b[o + 2]) << 16 | unsigned(b[o + 3]) << 24;
}
int main(int argc, char** argv)
{
    TextureSource s;
    s.width = s.height = 4;
    s.encoding = TextureEncoding::Rgba8;
    std::vector<uint8_t> bytes(64);
    for (unsigned i = 0; i < 16; ++i)
    {
        bytes[2 * i] = 255 - i;
        bytes[2 * i + 1] = i;
        bytes[32 + 2 * i] = i + 16;
        bytes[33 + 2 * i] = i + 32;
    }
    s.pixels = bytes;
    TextureUpload image;
    TextureLevel out;
    assert(decode_texture(s, image) && texture_level_rgba(image, 0, out));
    for (unsigned i = 0; i < 16; ++i)
    {
        assert(out.rgba[i * 4] == i && out.rgba[i * 4 + 1] == i + 16 &&
               out.rgba[i * 4 + 2] == i + 32 && out.rgba[i * 4 + 3] == 255 - i);
    }
    assert(!texture_level_rgba(image, 1, out));
    image.pixels.pop_back();
    assert(!texture_level_rgba(image, 0, out));
    // Padded rows must not become image pixels; small mip rows have their own padding.
    image = {};
    image.width = 3;
    image.height = 2;
    image.stride = 8;
    image.storage = TextureStorage::Rgba8;
    image.mipCount = 2;
    image.pixels.assign((8 * 2 + 8) * 4, 199);
    for (unsigned y = 0; y < 2; ++y)
        for (unsigned x = 0; x < 3; ++x)
            image.pixels[(y * 8 + x) * 4] = x + 3 * y;
    image.pixels[64] = 91;
    assert(texture_level_rgba(image, 0, out));
    for (unsigned i = 0; i < 6; ++i)
        assert(out.rgba[i * 4] == i);
    assert(texture_level_rgba(image, 1, out) && out.width == 1 && out.height == 1 &&
           out.rgba[0] == 91);
    if (argc == 1)
        return 0;
    char error[256];
    auto* archive = port_asset_archive_open(argv[1], error, sizeof error);
    assert(archive);
    unsigned records = 0, levels = 0, exact = 0;
    for (unsigned i = 0; i < port_asset_archive_count(archive); ++i)
    {
        const char* path = port_asset_archive_path(archive, i);
        if (!std::string_view(path).starts_with("native/v1/textures/"))
            continue;
        std::vector<uint8_t> record(port_asset_archive_size(archive, i));
        assert(port_asset_archive_read(archive, i, record.data(), record.size(), 0) ==
               long(record.size()));
        auto request = std::span(record).subspan(28, word(record, 12));
        TextureUpload precalculated;
        assert(parse_texture_record(record, request, precalculated));
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
        for (unsigned mip = 0; mip < source.mipCount; ++mip)
        {
            TextureLevel a, b;
            assert(texture_level_rgba(precalculated, mip, a));
            assert(texture_level_rgba(decoded, mip, b));
            assert(a.width == b.width && a.height == b.height);
            if (precalculated.storage != TextureStorage::Bc1)
                assert(a.rgba == b.rgba);
            else
            {
                for (size_t n = 0; n < a.rgba.size(); ++n)
                    if (a.rgba[n] != b.rgba[n])
                    {
                        std::fprintf(stderr, "BC1 difference %s mip=%u byte=%zu %u/%u\n", path, mip,
                                     n, a.rgba[n], b.rgba[n]);
                        return 2;
                    }
            }
            ++levels;
        }
        ++records;
        ++exact;
    }
    port_asset_archive_close(archive);
    std::printf("VitaGL conversion: %u textures, %u mip levels, %u records exactly equal to source "
                "decode\n",
                records, levels, exact);
    assert(records == 763);
}
