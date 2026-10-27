#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"
#include "NL/gl/glState.h"
#include "NL/gl/glConstant.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/glx/glxMemory.h"
#include "port/aspect.h"
#include <algorithm>
#include <cstdio>

namespace port::directmatch::detail
{
bool MatchRuntime::overlaps(Range a, Range b)
{
    return a.size && b.size &&
           (a.address <= b.address ? b.address - a.address < a.size : a.address - b.address < b.size);
}

void MatchRuntime::consume_writes()
{
    std::array<Range, 4096> changed;
    unsigned n;
    bool all;
    {
        std::lock_guard<std::mutex> guard(writeMutex);
        n = writeCount;
        std::copy_n(writes.begin(), n, changed.begin());
        all = invalidateAll;
        writeCount = 0;
        invalidateAll = false;
    }
    if (!n && !all)
        return;
    bool scanGeometry = all, scanTextures = all;
    for (unsigned i = 0; i < n && (!scanGeometry || !scanTextures); ++i)
    {
        if (!scanGeometry)
            scanGeometry = geometryBounds.intersects(changed[i]);
        if (!scanTextures)
            scanTextures = mutableTextureBounds.intersects(changed[i]);
    }
    // Writes outside both supersets cannot touch cached CPU data. Possible
    // overlaps still take the original exact checks, including gaps and erases.
    if (!scanGeometry && !scanTextures)
        return;
    for (auto it = geometry.begin(); scanGeometry && it != geometry.end();)
    {
        bool dirty = all;
        for (unsigned i = 0; i < n && !dirty; ++i)
            for (auto range : it->second.ranges)
                if (overlaps(range, changed[i]))
                {
                    dirty = true;
                    break;
                }
        if (dirty)
        {
            gpu.release_model(it->second.id);
            it = geometry.erase(it);
            forget_geometry();
        }
        else
            ++it;
    }
    if (geometry.empty())
        geometryBounds = {};
    for (auto it = textures.begin(); scanTextures && it != textures.end();)
    {
        if (it->second.target || it->second.fileNative)
        {
            ++it;
            continue;
        }
        bool dirty = all;
        for (unsigned i = 0; i < n && !dirty; ++i)
            dirty =
                overlaps(it->second.pixels, changed[i]) || overlaps(it->second.palette, changed[i]);
        if (dirty)
        {
            gpu.release_texture(it->second.id);
            it = textures.erase(it);
            forget_textures();
        }
        else
            ++it;
    }
    if (textures.empty())
        mutableTextureBounds = {};
}

uint32_t MatchRuntime::texture_for(const PlatTexture* p, int* shadowPartition)
{
    if (shadowPartition)
        *shadowPartition = -1;
    if (!p || p->m_Magic != 0x50544558 || !p->m_Width || !p->m_Height || unsigned(p->m_Format) > 8)
        return 0;
    auto& front = textureFront[(reinterpret_cast<uintptr_t>(p) >> 4 ^ reinterpret_cast<uintptr_t>(p) >> 12) & 255u];
    if (front.key == p && front.generation == textureGeneration)
    {
        if (shadowPartition)
            *shadowPartition = front.entry->shadowPartition;
        return front.entry->id;
    }
    auto existing = textures.find(p);
    if (existing != textures.end())
    {
        front = { p, &existing->second, textureGeneration };
        if (shadowPartition)
            *shadowPartition = existing->second.shadowPartition;
        return existing->second.id;
    }
    if (p->m_NativeDataSize || !p->m_SwizzledData)
    {
        failure = "native_texture_not_resident";
        ++invalid;
        return 0;
    }
    TextureUpload upload;
    size_t bytes = 0, paletteBytes = 0;
    // Only runtime-generated surfaces reach the decoder. File textures must be NSTX.
    constexpr TextureEncoding formats[]{ TextureEncoding::Rgb565, TextureEncoding::Rgb5a3,
                                         TextureEncoding::Cmpr,   TextureEncoding::Rgba8,
                                         TextureEncoding::I8,     TextureEncoding::I4,
                                         TextureEncoding::I8,     TextureEncoding::IA8,
                                         TextureEncoding::C8 };
    TextureSource source;
    source.width = p->m_Width;
    source.height = p->m_Height;
    source.mipCount = std::max(1u, unsigned(p->m_Levels));
    source.encoding = formats[p->m_Format];
    bytes = encoded_texture_bytes(source.width, source.height, source.encoding, source.mipCount);
    if (!bytes)
        return 0;
    source.pixels = { static_cast<const uint8_t*>(p->m_SwizzledData), bytes };
    if (p->m_Format == GXTex_CI8)
    {
        if (!p->m_PaletteData || p->m_nPaletteEntries <= 0)
            return 0;
        paletteBytes = size_t(p->m_nPaletteEntries) * 2;
        source.palette = { reinterpret_cast<const uint8_t*>(p->m_PaletteData), paletteBytes };
        source.paletteEncoding = PaletteEncoding::Rgb5a3;
    }
    if (!decode_texture(source, upload))
    {
        failure = "generated_texture_decode";
        return 0;
    }
    ++decoded;
    const uint32_t id = gpu.create_texture(upload);
    if (!id)
    {
        failure = "texture_upload";
        frameDiscarded = true;
        return 0;
    }
    Texture entry;
    entry.id = id;
    entry.level = p->m_OwnerTextureLevel;
    entry.pixels = { reinterpret_cast<uintptr_t>(p->m_SwizzledData), bytes };
    entry.palette = { reinterpret_cast<uintptr_t>(p->m_PaletteData), paletteBytes };
    try
    {
        const auto [it, inserted] = textures.emplace(p, entry);
        if (inserted)
        {
            mutableTextureBounds.include(entry.pixels);
            mutableTextureBounds.include(entry.palette);
        }
        if (shadowPartition)
            *shadowPartition = it->second.shadowPartition;
    }
    catch (...)
    {
        gpu.release_texture(id);
        throw;
    }
    return id;
}

void MatchRuntime::on_display_list_created(const glModelPacket* p, uintptr_t list,
                                           uint32_t level) noexcept
{
    try
    {
        const auto* dl = dlGetStruct(list);
        bool pn = false;
        for (unsigned i = 0; i < p->numStreams; ++i)
            pn |= p->streams[i].id == 12;
        if (!sources.capture(
                list, reinterpret_cast<uintptr_t>(dl->list), level, p->primType, p->numStreams,
                { reinterpret_cast<const uint16_t*>(p->indexBuffer), p->numVertices }, pn))
        {
            failure = "source_registration";
            ++invalid;
        }
    }
    catch (...)
    {
        failure = "source_allocation";
        ++invalid;
    }
}

void MatchRuntime::on_stitch_selectors(const glModelPacket* p, const uint8_t* s,
                                       uint32_t n) noexcept
{
    bool pn = false;
    for (unsigned i = 0; i < p->numStreams; ++i)
        pn |= p->streams[i].id == 12;
    if (!pn)
        return;
    try
    {
        const auto* dl = dlGetStruct(p->indexBuffer);
        if (!dl)
        {
            failure = "skin_selectors";
            ++invalid;
            return;
        }
        const auto address = reinterpret_cast<uintptr_t>(dl->list);
        const auto* previous =
            sources.find(p->indexBuffer, address, p->numVertices, p->primType, p->numStreams, true);
        const auto priorGeneration = previous ? previous->generation : 0;
        if (!sources.attach_stitch(p->indexBuffer, address, { s, n }))
        {
            failure = "skin_selectors";
            ++invalid;
            return;
        }
        const auto* updated =
            sources.find(p->indexBuffer, address, p->numVertices, p->primType, p->numStreams, true);
        if (priorGeneration && updated && updated->generation != priorGeneration)
        {
            for (auto it = geometry.begin(); it != geometry.end();)
            {
                if (it->second.generation == priorGeneration)
                {
                    gpu.release_model(it->second.id);
                    it = geometry.erase(it);
                    forget_geometry();
                }
                else
                    ++it;
            }
        }
    }
    catch (...)
    {
        ++invalid;
        failure = "skin_allocation";
    }
}

void MatchRuntime::on_resource_release(uint32_t level) noexcept
{
    glx_InvalidateTextureHandles();
    sources.release_after(level);
    for (auto it = geometry.begin(); it != geometry.end();)
        if (it->second.level > level)
        {
            gpu.release_model(it->second.id);
            it = geometry.erase(it);
            forget_geometry();
        }
        else
            ++it;
    if (geometry.empty())
        geometryBounds = {};
    for (auto it = textures.begin(); it != textures.end();)
        if (it->second.fileNative && it->second.resourceLevel > level)
        {
            gpu.release_texture(it->second.id);
            it = textures.erase(it);
            forget_textures();
        }
        else
            ++it;
    if (textures.empty())
        mutableTextureBounds = {};
}

void MatchRuntime::on_texture_release(uint32_t level) noexcept
{
    glx_InvalidateTextureHandles();
    for (auto it = textures.begin(); it != textures.end();)
        if (it->second.level > level)
        {
            if (it->second.id != shadowAtlas)
                gpu.release_texture(it->second.id);
            it = textures.erase(it);
            forget_textures();
        }
        else
            ++it;
    if (textures.empty())
        mutableTextureBounds = {};
}

bool MatchRuntime::admit_texture(const PlatTexture* p,
                                 const nativegxm::TextureUpload& image) noexcept
{
    glx_InvalidateTextureHandles();
    if (!p || !preload())
        return false;
    const uint32_t id = gpu.create_texture(image);
    if (!id)
        return false;
    try
    {
        auto [it, inserted] = textures.try_emplace(p);
        const uint32_t old = inserted ? 0 : it->second.id;
        Texture entry;
        entry.id = id;
        entry.level = p->m_OwnerTextureLevel;
        entry.fileNative = true;
        entry.resourceLevel = p->m_OwnerResourceLevel;
        it->second = entry;
        ++records;
        if (old && old != shadowAtlas)
            gpu.release_texture(old);
        return true;
    }
    catch (...)
    {
        gpu.release_texture(id);
        failure = "native_texture_admission";
        ++invalid;
        return false;
    }
}

void MatchRuntime::notify_write(const void* ptr, size_t bytes) noexcept
{
    if (!ptr || !bytes)
        return;
    std::lock_guard<std::mutex> guard(writeMutex);
    if (writeCount < writes.size())
        writes[writeCount++] = { reinterpret_cast<uintptr_t>(ptr), bytes };
    else
        invalidateAll = true;
}

void MatchRuntime::capture_shadow_atlas() noexcept
{
    try
    {
        std::vector<TextureCopy> copies;
        copies.reserve(10);
        const float sx = 960.f / PortLogicalFrameWidth(), sy = 544.f / 448.f;
        for (unsigned i = 0; i < 10; ++i)
        {
            char name[40];
            std::snprintf(name, sizeof name, "target/pshadow_updated%02u", i);
            if (glConstantGet(name).x != 1)
                continue;
            std::snprintf(name, sizeof name, "target/pshadow%02u", i);
            auto* p = glx_GetTex(glGetTexture(name), false, false);
            if (!p)
                continue;
            if (!shadowAtlas)
                shadowAtlas = gpu.create_target(320, 240);
            if (!shadowAtlas)
            {
                failure = "shadow_target";
                ++invalid;
                frameDiscarded = true;
                return;
            }
            auto it = textures.find(p);
            if (it == textures.end() || it->second.id != shadowAtlas)
            {
                auto [entryIt, inserted] = textures.try_emplace(p);
                const auto previous = entryIt->second.id;
                Texture entry;
                entry.id = shadowAtlas;
                entry.level = p->m_OwnerTextureLevel;
                entry.target = true;
                entry.shadowPartition = i;
                entryIt->second = entry;
                if (previous && previous != shadowAtlas)
                    gpu.release_texture(previous);
            }
            TextureCopy copy{ shadowAtlas,
                              { (i % 4) * 160 * sx, (i / 4) * 148 * sy, 160 * sx, 148 * sy },
                              74,
                              false,
                              true };
            copy.destinationRect = { (i % 4) * 80, (i / 4) * 80, 80, 74 };
            copy.preserveDestination = true;
            copies.push_back(copy);
        }
        if (!gpu.copy_textures(copies))
        {
            failure = "shadow_copy";
            ++invalid;
            frameDiscarded = true;
        }
    }
    catch (...)
    {
        failure = "shadow_allocation";
        ++invalid;
        frameDiscarded = true;
    }
}

void MatchRuntime::capture_texture(uintptr_t handle, unsigned x, unsigned y, unsigned width,
                                   unsigned height, bool intensity, unsigned destinationWidth,
                                   unsigned destinationHeight, bool clearSource) noexcept
{
    try
    {
        auto* p = glx_GetTex(handle, false, false);
        const auto sourceWidth = PortLogicalFrameWidth();
        if (!p || !width || !height || x > sourceWidth || width > sourceWidth - x ||
            y > 448 || height > 448 - y ||
            ((destinationWidth == 0) != (destinationHeight == 0)))
        {
            failure = "capture_source";
            ++invalid;
            frameDiscarded = true;
            return;
        }
        const auto targetWidth = destinationWidth ? destinationWidth : p->m_Width;
        const auto targetHeight = destinationHeight ? destinationHeight : p->m_Height;
        if (!targetWidth || !targetHeight)
        {
            failure = "capture_extent";
            ++invalid;
            frameDiscarded = true;
            return;
        }
        // Active screen grabs are RGB565; the dedicated shadow-atlas path
        // handles A8 separately. Never reinterpret another copy format as RGBA.
        if (p->m_Format != GXTex_RGB565 && p->m_Format != GXTex_RGBA8)
        {
            failure = "capture_format";
            ++invalid;
            frameDiscarded = true;
            return;
        }
        const auto targetFormat = p->m_Format == GXTex_RGB565 ? render::TargetFormat::Rgb565
                                                             : render::TargetFormat::Rgba8;
        auto it = textures.find(p);
        uint32_t id = 0;
        if (it != textures.end() && it->second.target && it->second.id == shadowAtlas)
        {
            failure = "capture_shared_target";
            ++invalid;
            frameDiscarded = true;
            return;
        }
        if (it != textures.end() && it->second.target &&
            it->second.targetWidth == targetWidth && it->second.targetHeight == targetHeight &&
            it->second.targetFormat == targetFormat)
            id = it->second.id;
        else
        {
            // Allocate map storage before the GPU resource. A failed resize
            // leaves the prior texture valid until a later capture can retry.
            auto [entryIt, inserted] = textures.try_emplace(p);
            const auto previous = entryIt->second.id;
            id = gpu.create_target(targetWidth, targetHeight, targetFormat);
            if (!id)
            {
                if (inserted)
                {
                    textures.erase(entryIt);
                    forget_textures();
                }
                failure = "capture_target";
                ++invalid;
                frameDiscarded = true;
                return;
            }
            Texture entry;
            entry.id = id;
            entry.level = p->m_OwnerTextureLevel;
            entry.target = true;
            entry.targetWidth = targetWidth;
            entry.targetHeight = targetHeight;
            entry.targetFormat = targetFormat;
            entryIt->second = entry;
            if (previous)
                gpu.release_texture(previous);
        }
        const float sx = 960.f / PortLogicalFrameWidth(), sy = 544.f / 448.f;
        TextureCopy copy{ id, { x * sx, y * sy, width * sx, height * sy }, 0, intensity };
        copy.clearSource = clearSource;
        copy.clearColor = copyClearColor;
        if (!gpu.copy_textures({ &copy, 1 }))
        {
            failure = "capture_copy";
            ++invalid;
            frameDiscarded = true;
        }
    }
    catch (...)
    {
        failure = "capture_allocation";
        ++invalid;
        frameDiscarded = true;
    }
}
}   // namespace port::directmatch::detail
#endif
