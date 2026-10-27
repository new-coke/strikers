// BC1 uploads: which images stay compressed, their block order and
// truncated mip chains. GXM's own BC1 decode needs a Vita.
#include "port/vitagl_texture.hpp"
namespace
{
// One 8x8 swizzled BC1 image with 8x8, 4x4, 2x2 and 1x1 levels. Every block
// is a distinct four-colour block, so block order is observable.
port::nativegxm::TextureUpload bc1_image(bool transparentTexel)
{
    port::nativegxm::TextureUpload image;
    image.width = image.height = image.stride = 8;
    image.mipCount = 4;
    image.storage = port::nativegxm::TextureStorage::Bc1;
    image.swizzled = true;
    image.pixels.resize(port::nativegxm::texture_upload_bytes(image));
    for (size_t block = 0; block < image.pixels.size() / 8; ++block)
    {
        auto* p = image.pixels.data() + block * 8;
        const unsigned c0 = 0xf000u + unsigned(block), c1 = 0x0100u;
        p[0] = uint8_t(c0); p[1] = uint8_t(c0 >> 8); p[2] = uint8_t(c1); p[3] = uint8_t(c1 >> 8);
        p[4] = uint8_t(block); p[5] = 0x1b; p[6] = 0xe4; p[7] = 0x27;
    }
    if (transparentTexel)
    {
        // Last level: endpoints swapped to three-colour mode, texel 0 index 3.
        auto* p = image.pixels.data() + image.pixels.size() - 8;
        std::swap(p[0], p[2]);
        std::swap(p[1], p[3]);
        p[4] = 3;
    }
    return image;
}
void check_bc1_direct_renderer()
{
    using namespace port::render;
    for (const char* flag : {static_cast<const char*>(nullptr), "0", "1"})
    {
        if (flag) setenv("STRIKERS_VITAGL_BC1_DIRECT", flag, 1);
        else unsetenv("STRIKERS_VITAGL_BC1_DIRECT");
        const bool on = flag && std::strcmp(flag, "1") == 0;
        for (bool transparent : {false, true})
        {
            Renderer r;
            assert(r.initialize());
            compressedUploads.clear();
            const auto image = bc1_image(transparent);
            const unsigned before = textureUploads;
            const auto id = r.create_texture(image);
            assert(id);
            if (on && !transparent)
            {
                // 8x8 and 4x4 upload; levels below one block end the chain.
                assert(compressedUploads.size() == 2 && textureUploads - before == 2);
                for (unsigned level = 0; level < 2; ++level)
                {
                    const auto& u = compressedUploads[level];
                    assert(u.texture == id && u.level == GLint(level) &&
                           u.format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
                    assert(u.width == GLsizei(8 >> level) && u.height == GLsizei(8 >> level));
                }
                // Each block's byte 4 is its index in the pack, so linear block
                // (bx, by) must hold the pack's Morton-ordered block.
                const auto& top = compressedUploads[0].blocks;
                for (unsigned by = 0; by < 2; ++by)
                    for (unsigned bx = 0; bx < 2; ++bx)
                        assert(top[(by * 2 + bx) * 8 + 4] == texture_morton(bx, by, 2, 2));
                assert(compressedUploads[1].blocks[4] == 4);
                assert(textureParameters.at(id).at(GL_TEXTURE_MIN_FILTER) == GL_LINEAR_MIPMAP_LINEAR);
            }
            else
            {
                assert(compressedUploads.empty() && textureUploads - before == 4);
                assert(textureAllocations.at(id).format == GL_RGBA);
            }
            r.release_texture(id);
            r.shutdown();
            empty();
        }
    }
    unsetenv("STRIKERS_VITAGL_BC1_DIRECT");
    std::puts("BC1 direct upload: flag, opaque-only, deswizzled blocks and whole-block mip chain passed");
}
}
