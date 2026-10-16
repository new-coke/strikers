#include "port/native_graphics.hpp"
#include "port/asset_archive.h"
#include <cassert>
#include <cstdio>
#include <string_view>

using Bytes = std::vector<uint8_t>;
using namespace port;
static void set(Bytes& b, size_t at, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i)
        b.at(at + i) = uint8_t(n >> (8 * i));
}
static Bytes section(uint32_t kind, const Bytes& data)
{
    Bytes out(16 + ((data.size() + 15) & ~size_t(15)));
    set(out, 0, kind);
    set(out, 4, data.size());
    std::copy(data.begin(), data.end(), out.begin() + 16);
    return out;
}
static void append(Bytes& a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); }
static uint32_t be(const uint8_t* p)
{
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
static bool original_texture(std::span<const uint8_t> b)
{
    if (b.size() < 32)
        return false;
    const unsigned levels = be(b.data()), format = be(b.data() + 4), palette = be(b.data() + 20);
    const unsigned w = unsigned(b[14]) << 8 | b[15], h = unsigned(b[16]) << 8 | b[17];
    if (!levels || levels > 13 || format > 8 || !w || !h || w > 4096 || h > 4096 ||
        palette > 32767 || b[12] > 1)
        return false;
    for (unsigned i = 8; i < 12; ++i)
        if (b[i] > 8 && b[i] != 255)
            return false;
    if ((format == 8) != (palette > 0))
        return false;
    constexpr nativegxm::TextureEncoding formats[]{
        nativegxm::TextureEncoding::Rgb565, nativegxm::TextureEncoding::Rgb5a3,
        nativegxm::TextureEncoding::Cmpr,   nativegxm::TextureEncoding::Rgba8,
        nativegxm::TextureEncoding::I8,     nativegxm::TextureEncoding::I4,
        nativegxm::TextureEncoding::I8,     nativegxm::TextureEncoding::IA8,
        nativegxm::TextureEncoding::C8
    };
    const size_t n =
        nativegxm::encoded_texture_bytes(w, h, formats[format], levels) + 32 + palette * 2;
    return n <= b.size() && b.size() - n < 32;
}
static Bytes file(const char* magic, const Bytes& data, unsigned count)
{
    Bytes out(16);
    std::memcpy(out.data(), magic, 8);
    append(out, data);
    set(out, 8, out.size());
    set(out, 12, count);
    return out;
}
static Bytes model()
{
    Bytes models(16), packet(80), streams(12), vertices(36), indices{ 0, 0, 1, 0, 2, 0 };
    set(models, 0, 1);
    set(models, 4, 0x12345678);
    packet[8] = 3;
    packet[11] = 1;
    streams[5] = 12;
    set(streams, 8, 36);
    Bytes payload;
    for (const auto& [kind, bytes] :
         std::initializer_list<std::pair<uint32_t, Bytes>>{ { assets::References, Bytes(64) },
                                                            { assets::Models, models },
                                                            { assets::Packets, packet },
                                                            { assets::Streams, streams },
                                                            { assets::Vertices, vertices },
                                                            { assets::Indices, indices } })
        append(payload, section(kind, bytes));
    return file("NSMD0001", section(assets::Group, payload), 1);
}
static void fixtures()
{
    auto b = model();
    assert(assets::validate_model(b));
    auto bad = b;
    bad.pop_back();
    assert(!assets::validate_model(bad));
    bad = b;
    set(bad, 12, 2);
    assert(!assets::validate_model(bad));
    bad = b;
    bad[16] = 255;
    assert(!assets::validate_model(bad));
    // The final native index must be in bounds for every referenced stream.
    bad = b;
    const size_t indexAt = bad.size() - 16;
    bad[indexAt + 4] = 3;
    assert(!assets::validate_model(bad));
    Bytes pairs{ 0, 0, 255, 255 };
    auto skin = file("NSSK0001", section(assets::Pairs, pairs), 1);
    assert(assets::validate_skin(skin));
    skin[16] = 99;
    assert(!assets::validate_skin(skin));
    Bytes texture(64 + 8 * 4 * 4);
    std::memcpy(texture.data(), "NSTX0001", 8);
    set(texture, 8, texture.size());
    set(texture, 12, 8);
    set(texture, 16, 4);
    set(texture, 20, 1);
    set(texture, 24, 8);
    set(texture, 28, 3);
    set(texture, 40, 128);
    std::fill(texture.begin() + 64, texture.end(), 177);
    const uint64_t hash = nativegxm::native_asset_hash(std::span(texture).subspan(64));
    set(texture, 48, uint32_t(hash));
    set(texture, 52, uint32_t(hash >> 32));
    nativegxm::TextureUpload image;
    assert(assets::texture(texture, image));
    assert(image.width == 8 && image.height == 4 && !image.swizzled && image.pixels.size() == 128 &&
           image.pixels[0] == 177);
    bad = texture;
    bad.back() ^= 1;
    assert(!assets::texture(bad, image) && image.pixels.empty());
    bad = texture;
    set(bad, 24, 16);
    assert(!assets::texture(bad, image));
    Bytes bundle(32);
    std::memcpy(bundle.data(), "NSTB0001", 8);
    set(bundle, 12, 1);
    set(bundle, 16, 99);
    set(bundle, 20, 32);
    set(bundle, 24, texture.size());
    append(bundle, texture);
    set(bundle, 8, bundle.size());
    assert(assets::texture_bundle(bundle));
    // Stadium crowd bundles have distinct entry IDs but replace the same
    // atlas used by already-submitted crowd materials. Apply the captured
    // request remapper before admission, preserving the image and its alpha.
    unsigned admissions = 0;
    const auto target = [](uint32_t) { return 0x756f644au; };
    assert(assets::admit_texture_bundle(bundle, target,
                                        [&](uint32_t handle, std::span<const uint8_t> pixels) {
                                            ++admissions;
                                            assert(handle == 0x756f644a);
                                            nativegxm::TextureUpload upload;
                                            assert(assets::texture(pixels, upload));
                                            assert(upload.pixels[3] == 177);
                                            return true;
                                        }) &&
           admissions == 1);
    assert(assets::admit_texture_bundle(
        bundle, [](uint32_t) { return UINT32_MAX; },
        [](uint32_t, std::span<const uint8_t>) {
            assert(false);
            return true;
        }));
    assert(!assets::admit_texture_bundle(bundle, target,
                                         [](uint32_t, std::span<const uint8_t>) { return false; }));
    set(bundle, 20, 16);
    assert(!assets::texture_bundle(bundle));
    assert(!assets::admit_texture_bundle(bundle, target, [](uint32_t, std::span<const uint8_t>) {
        assert(false);
        return true;
    }));
}
int main(int argc, char** argv)
{
    fixtures();
    if (argc == 1)
    {
        std::puts("Native graphics: model/skin bounds, native texture checksum, extent and bundle "
                  "validation passed");
        return 0;
    }
    char error[512];
    auto* archive = port_asset_archive_open(argv[1], error, sizeof error);
    assert(archive);
    unsigned models = 0, bundles = 0, textures = 0, resources = 0, embedded = 0;
    for (unsigned i = 0; i < port_asset_archive_count(archive); ++i)
    {
        const std::string_view path = port_asset_archive_path(archive, i);
        const bool modelFile = path.ends_with(".glg"), textureFile = path.ends_with(".glt");
        uint8_t header[16];
        if (!modelFile && !textureFile)
        {
            if (port_asset_archive_size(archive, i) < 16)
                continue;
            assert(port_asset_archive_read(archive, i, header, 16, 0) == 16);
            const unsigned sector = be(header);
            if (sector < 16 || sector > 4096 || (sector & (sector - 1)) || !be(header + 4) ||
                be(header + 4) > 65536)
                continue;
        }
        Bytes b(port_asset_archive_size(archive, i));
        assert(port_asset_archive_read(archive, i, b.data(), b.size(), 0) == long(b.size()));
        if (modelFile)
        {
            assert(assets::validate_model(b));
            ++models;
            continue;
        }
        if (!textureFile)
        {
            const uint64_t sector = be(header), count = be(header + 4),
                           directory = uint64_t(be(header + 8)) * sector;
            if (directory < 16 || directory > b.size() || count * 12 > b.size() - directory ||
                uint64_t(be(header + 12)) * sector > b.size())
                continue;
            unsigned nativeImages = 0;
            for (unsigned entry = 0; entry < count; ++entry)
            {
                const auto* e = b.data() + directory + entry * 12;
                const uint64_t at = uint64_t(be(e + 4)) * sector, n = be(e + 8);
                assert(at <= b.size() && n <= b.size() - at);
                auto payload = std::span(b).subspan(at, n);
                if (payload.size() >= 8 && !std::memcmp(payload.data(), "NSTX0001", 8))
                {
                    nativegxm::TextureUpload image;
                    assert(assets::texture(payload, image));
                    ++nativeImages;
                }
                else if (original_texture(payload))
                {
                    std::fprintf(stderr, "Original embedded texture remains: %.*s entry=%u\n",
                                 int(path.size()), path.data(), entry);
                    assert(false);
                }
            }
            if (nativeImages)
            {
                ++resources;
                embedded += nativeImages;
            }
            continue;
        }
        assert(assets::texture_bundle(b));
        ++bundles;
        for (unsigned n = 0; n < assets::word(b.data() + 12); ++n)
        {
            const auto* e = b.data() + 16 + n * 16;
            nativegxm::TextureUpload image;
            assert(assets::texture(std::span(b).subspan(assets::word(e + 4), assets::word(e + 8)),
                                   image));
            ++textures;
        }
    }
    port_asset_archive_close(archive);
    std::printf("Native PSARC consumer: models=%u bundles=%u textures=%u resources=%u embedded=%u; "
                "no retail model/texture payloads\n",
                models, bundles, textures, resources, embedded);
    assert(models && bundles && textures);
}
