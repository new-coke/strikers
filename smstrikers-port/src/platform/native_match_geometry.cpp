#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"
#include "Game/GL/GLMeshWriter.h"
#include "Game/GL/ShaderSkinMesh.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/glx/glxMemory.h"
#include "port/scene_skinning.hpp"
#include <cmath>
#include <cstring>

namespace port::directmatch::detail
{
bool MatchRuntime::prepare_packet_geometry(const glModelPacket& p, PacketGeometry& data)
{
    if (!p.streams || !p.numStreams || p.numStreams > 16 || p.primType > 5 ||
        p.numVertices < (p.primType >= 4 ? 2u : 3u))
    {
        failure = "packet_shape";
        ++invalid;
        return false;
    }
    auto& hasPn = data.hasPn;
    for (unsigned i = 0; i < p.numStreams; ++i)
        hasPn |= p.streams[i].id == 12;
    auto& source = data.source;
    auto& original = data.indices;
    if (p.indexBuffer)
    {
        const auto* dl = dlGetStruct(p.indexBuffer);
        data.nativeDisplayList = dl != nullptr;
        if (!dl)
        {
            if (hasPn)
            {
                failure = "native_indexed_skin";
                ++invalid;
                return false;
            }
            original = { reinterpret_cast<const uint16_t*>(p.indexBuffer), p.numVertices };
        }
        else
        {
            if (!dl->list)
            {
                failure = "native_list";
                ++invalid;
                return false;
            }
            source = sources.find(p.indexBuffer, reinterpret_cast<uintptr_t>(dl->list),
                                  p.numVertices, p.primType, p.numStreams, hasPn);
            if (source)
                original = source->indices;
            else if (!hasPn && dl->size >= p.numVertices * 2u)
                original = { static_cast<const uint16_t*>(dl->list), p.numVertices };
            else
            {
                failure = "native_source";
                ++invalid;
                return false;
            }
        }
    }
    auto& streams = data.streams;
    auto& ranges = data.ranges;
    auto& rangeCount = data.rangeCount;
    auto& immutable = data.immutable;
    immutable = source != nullptr;
    auto& key = data.key;
    key = source ? source->generation * 0x9e3779b97f4a7c15ull : 0;
    key ^= p.state.texconfig;
    for (unsigned i = 0; i < p.numStreams; ++i)
    {
        const auto s = p.streams[i];
        ModelArray* a = nullptr;
        if (s.id == GLStream_Position || s.id == GLStream_Position4)
            a = &streams.position;
        else if (s.id == 1)
            a = &streams.normal;
        else if (s.id == 2)
            a = &streams.color;
        else if (s.id == 3)
            a = &streams.uv;
        else if (s.id >= 4 && s.id <= 8)
            a = &streams.extraUv[s.id - 4];
        else
            continue;
        if (!s.address || !s.stride || !s.dataSize || s.dataSize > 8u * 1024u * 1024u)
        {
            failure = "array_bounds";
            ++invalid;
            return false;
        }
        *a = { { reinterpret_cast<const uint8_t*>(s.address), s.dataSize },
               s.stride,
               s.beData == 0 };
        ranges[rangeCount++] = { s.address, s.dataSize };
        immutable = immutable && glxVitaResourceArenaOwns(reinterpret_cast<const void*>(s.address));
        key ^= uint64_t(s.address) + (key << 6) + (key >> 2);
        key ^= uint64_t(s.id) | (uint64_t(s.stride) << 8) | (uint64_t(s.dataSize) << 16);
    }
    if (streams.position.bytes.empty())
    {
        failure = "position_missing";
        ++invalid;
        return false;
    }

    return true;
}

bool MatchRuntime::prepare_skin(const glModelPacket& p, bool hasPn, SceneMaterial& m,
                                std::span<const GLSkinUserData>& palette)
{
    if (hasPn)
    {
        for (unsigned slot = 0; slot < render::kSceneBoneSlots; ++slot)
            for (unsigned r = 0; r < 3; ++r)
                m.bones[slot * 3 + r][r] = 1;
        const auto* skin = static_cast<const uint8_t*>(userdata(p, GLUD_Skin));
        if (!skin)
        {
            failure = "skin_userdata";
            ++invalid;
            return false;
        }
        uint32_t count;
        std::memcpy(&count, skin, 4);
        if (!count || count > render::kSceneSkinBoneCount)
        {
            failure = "skin_count";
            ++invalid;
            return false;
        }
        const auto* data = reinterpret_cast<const GLSkinUserData*>(skin + 4);
        uint32_t used = 0;
        for (unsigned i = 0; i < count; ++i)
        {
            const int reg = data[i].reg;
            if (reg < render::kSceneSkinMinRegister || reg > render::kSceneSkinMaxRegister ||
                reg % 3)
            {
                failure = "skin_register";
                ++invalid;
                return false;
            }
            const unsigned bone = unsigned(reg - render::kSceneSkinMinRegister) / 3u;
            const uint32_t bit = uint32_t{1} << bone;
            if (used & bit)
            {
                failure = "skin_duplicate";
                ++invalid;
                return false;
            }
            used |= bit;
            for (float element : data[i].mat)
                if (!std::isfinite(element))
                {
                    failure = "skin_matrix";
                    ++invalid;
                    return false;
                }
            const unsigned selector = unsigned(reg + 99);
            std::memcpy(m.bones[selector].data(), data[i].mat, 48);
        }
        // Borrowed only until resolve_model returns in this callback. The
        // deferred renderer receives its owned SceneMaterial, never this span.
        palette = { data, count };
    }

    return true;
}

bool MatchRuntime::resolve_model(const glModelPacket& p, const PacketGeometry& data,
                                 std::span<const GLSkinUserData> skin, ModelVertices& model,
                                 uint32_t& modelId)
{
    const auto* source = data.source;
    const auto original = data.indices;
    const auto& streams = data.streams;
    const auto& ranges = data.ranges;
    const auto rangeCount = data.rangeCount;
    const auto immutable = data.immutable;
    const auto hasPn = data.hasPn;
    const auto key = data.key;
    // Environment materials alter streamed colour/UV arrays; take a fresh snapshot.
    // Fresnel and environment UVs are evaluated on the GPU; immutable source
    // positions/colours remain resident while only uniforms change.
    auto it = geometry.find(key);
    if (immutable && it != geometry.end())
    {
        if (hasPn)
        {
            // A VBO retains its PN slots, but a later draw supplies a fresh
            // palette. Validate required slots without rescanning its vertices.
            uint32_t supplied = 0;
            for (const auto& bone : skin)
                supplied |= uint32_t{1} <<
                    (unsigned(bone.reg - render::kSceneSkinMinRegister) / 3u);
            if (!source || (supplied & source->skinBoneMask) != source->skinBoneMask)
            {
                failure = "skin_palette";
                ++invalid;
                return false;
            }
        }
        modelId = it->second.id;
    }
    else
    {
        const auto decode = [&](std::span<const BoneTransform> bones) {
            return prepare_model_vertices(streams, original, p.numVertices, p.primType,
                hasPn ? std::span<const uint8_t>(source->selectors)
                      : std::span<const uint8_t>{}, bones, model, true, false, &vertexScratch);
        };
        bool prepared;
        if (skin.empty())
            prepared = decode({});
        else
        {
            // The CPU decoder's representation is needed only on a miss.
            // Resident draws already have PN selectors in the GPU vertices;
            // prepare_skin still publishes their current pose on every draw.
            std::array<BoneTransform, render::kSceneSkinBoneCount> bones;
            for (size_t i = 0; i < skin.size(); ++i)
            {
                bones[i].selector = unsigned(skin[i].reg + 99);
                std::memcpy(bones[i].matrix.data(), skin[i].mat, 48);
            }
            prepared = decode({ bones.data(), skin.size() });
        }
        if (!prepared)
        {
            failure = "geometry_decode";
            ++invalid;
            return false;
        }
        // ModelStreams and the shader both use semantic slots (Diffuse=0,
        // Gloss=4), while GX packed only its own texture units. Repacking these
        // vertices by the active texture count replaced sparse gloss/shadow UVs
        // with absent intermediate arrays, sampling a single texture coordinate.
        if (immutable)
        {
            modelId = gpu.create_model(model.vertices, model.indices);
            if (modelId)
            {
                try
                {
                    const auto [entryIt, inserted] = geometry.emplace(
                        key, Geometry{ modelId, source->marker, source->generation,
                                       { ranges.begin(), ranges.begin() + rangeCount } });
                    if (inserted)
                        for (const auto range : entryIt->second.ranges)
                            geometryBounds.include(range);
                }
                catch (...)
                {
                    gpu.release_model(modelId);
                    modelId = 0;
                    throw;
                }
                ++resident;
            }
        }
    }

    return true;
}
}   // namespace port::directmatch::detail
#endif
