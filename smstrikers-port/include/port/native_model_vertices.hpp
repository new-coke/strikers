#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace port::nativegxm
{
struct ModelVertex
{
    std::array<float, 4> position{ 0, 0, 0, 1 };
    std::array<uint8_t, 4> color{ 255, 255, 255, 255 };
    std::array<float, 3> uv{ 0, 0, 1 };
    std::array<float, 3> normal{ 0, 0, 1 };
    std::array<std::array<float, 3>, 5> extraUv{
        { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }
    };
    float boneSlot = 0;
};
static_assert(sizeof(ModelVertex) == 108);
struct ModelArray
{
    std::span<const uint8_t> bytes{};
    uint32_t stride = 0;
    bool littleEndian = false;
};
struct ModelStreams
{
    ModelArray position{}, color{}, uv{}, normal{};
    std::array<ModelArray, 5> extraUv{};
};
struct BoneTransform
{
    uint32_t selector = 0;
    std::array<float, 12> matrix{};
};
struct ModelVertices
{
    std::vector<ModelVertex> vertices{};
    std::vector<uint16_t> indices{};
};
struct ModelVertexScratch
{
    struct Entry
    {
        uint32_t key = UINT32_MAX;
        uint16_t vertex = 0;
    };
    std::vector<Entry> lookup;
    std::vector<uint16_t> remap;
};

// Retain at most 528 KiB of producer scratch. Oversized packets still decode
// normally; release their capacity after the renderer owns its snapshot.
inline void recycle_model_vertex_scratch(ModelVertices& model, ModelVertexScratch& scratch) noexcept
{
    constexpr size_t retainedVertices = 4096;
    model.vertices.clear();
    model.indices.clear();
    scratch.lookup.clear();
    scratch.remap.clear();
    if (model.vertices.capacity() > retainedVertices)
        std::vector<ModelVertex>().swap(model.vertices);
    if (model.indices.capacity() > retainedVertices * 3)
        std::vector<uint16_t>().swap(model.indices);
    if (scratch.lookup.capacity() > retainedVertices * 2)
        std::vector<ModelVertexScratch::Entry>().swap(scratch.lookup);
    if (scratch.remap.capacity() > retainedVertices)
        std::vector<uint16_t>().swap(scratch.remap);
}

inline bool model_array_at(const ModelArray& a, uint32_t index, uint32_t need,
                           const uint8_t*& p) noexcept
{
    const uint64_t offset = uint64_t(index) * a.stride;
    if (a.stride < need || offset > a.bytes.size() || need > a.bytes.size() - offset)
        return false;
    p = a.bytes.data() + offset;
    return true;
}
inline uint32_t model_word(const uint8_t* p, bool little) noexcept
{
    if (little)
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
               (uint32_t(p[3]) << 24);
    return uint32_t(p[3]) | (uint32_t(p[2]) << 8) | (uint32_t(p[1]) << 16) | (uint32_t(p[0]) << 24);
}
inline float model_float(const uint8_t* p, bool little) noexcept
{
    const uint32_t bits = model_word(p, little);
    float value = 0;
    std::memcpy(&value, &bits, 4);
    return value;
}
inline float model_short(const uint8_t* p, bool little, float scale) noexcept
{
    const uint16_t bits =
        little ? uint16_t(p[0]) | (uint16_t(p[1]) << 8) : uint16_t(p[1]) | (uint16_t(p[0]) << 8);
    return float(int16_t(bits)) * scale;
}
// Bounds consume the same host-order indices as rendering. In particular a
// native DisplayList contains u16 indices, with no GX opcode/attribute prefix.
inline bool model_position_bounds(const ModelArray& position, std::span<const uint16_t> indices,
                                  uint32_t count, std::array<float, 3>& minimum,
                                  std::array<float, 3>& maximum) noexcept
{
    if (!count || (!indices.empty() && indices.size() != count) ||
        (position.stride != 6 && position.stride != 12 && position.stride != 16))
        return false;
    std::array<float, 3> low{}, high{};
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t* p = nullptr;
        if (!model_array_at(position, indices.empty() ? i : indices[i], position.stride, p))
            return false;
        for (unsigned k = 0; k < 3; ++k)
        {
            const float f = position.stride == 6
                                ? model_short(p + 2 * k, position.littleEndian, 1.f / 256.f)
                                : model_float(p + 4 * k, position.littleEndian);
            if (!std::isfinite(f))
                return false;
            if (!i || f < low[k])
                low[k] = f;
            if (!i || f > high[k])
                high[k] = f;
        }
    }
    minimum = low;
    maximum = high;
    return true;
}
// Typed engine arrays and original host-order GLG indices are the producer
// inputs. No GX opcode/display-list decoder or Aurora canonical vertex enters
// this path. Rigid bones are applied to owned vertices before queueing; CPU
// skinned arrays use exactly the same bounded snapshot path.
inline bool prepare_model_vertices(const ModelStreams& streams,
                                   std::span<const uint16_t> originalIndices, uint32_t count,
                                   uint8_t primitive, std::span<const uint8_t> selectors,
                                   std::span<const BoneTransform> bones, ModelVertices& out,
                                   bool compact = false, bool applyBoneMatrix = true,
                                   ModelVertexScratch* reusableScratch = nullptr)
{
    out.vertices.clear();
    out.indices.clear();
    struct ClearOnFailure
    {
        ModelVertices& model;
        bool complete = false;
        ~ClearOnFailure()
        {
            if (!complete)
            {
                model.vertices.clear();
                model.indices.clear();
            }
        }
    } result{ out };
    if (primitive > 5 || count < (primitive >= 4 ? 2u : 3u) || count > 64000 ||
        (!originalIndices.empty() && originalIndices.size() != count) ||
        (!selectors.empty() && selectors.size() != count))
        return false;
    if ((primitive == 0 && count % 3) || (primitive == 3 && count % 4) ||
        (primitive == 4 && count % 2))
        return false;
    if (streams.position.stride != 6 && streams.position.stride != 12 &&
        streams.position.stride != 16)
        return false;
    std::array<const BoneTransform*, 256> palette{};
    for (const auto& bone : bones)
    {
        if (bone.selector > 255 || bone.selector % 3 || palette[bone.selector])
            return false;
        for (float f : bone.matrix)
            if (!std::isfinite(f))
                return false;
        palette[bone.selector] = &bone;
    }
    auto& candidate = out;
    candidate.vertices.reserve(count);
    const size_t outputIndices = primitive == 0 || primitive == 4 ? count
                                 : primitive == 3 ? size_t(count) / 2u * 3u
                                 : primitive == 5 ? size_t(count - 1u) * 2u
                                                  : size_t(count - 2u) * 3u;
    candidate.indices.reserve(outputIndices);
    // Deduplicate only inside this immutable draw snapshot. Two references
    // share a vertex exactly when BOTH the original stream index and PN slot
    // agree. Pose/color/UV changes in another draw or frame never reuse data.
    ModelVertexScratch localScratch;
    auto& scratch = reusableScratch ? *reusableScratch : localScratch;
    auto& lookup = scratch.lookup;
    auto& remap = scratch.remap;
    lookup.clear();
    remap.clear();
    if (compact && !originalIndices.empty())
    {
        size_t size = 8;
        while (size < size_t(count) * 2u)
            size *= 2u;
        lookup.resize(size);
        remap.resize(count);
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t index = originalIndices.empty() ? i : originalIndices[i];
        ModelVertexScratch::Entry* entry = nullptr;
        if (!lookup.empty())
        {
            const uint32_t key = index | (uint32_t(selectors.empty() ? 0 : selectors[i]) << 16);
            uint32_t hash = key ^ (key >> 16);
            hash *= 0x7feb352du;
            hash ^= hash >> 15;
            size_t slot = hash & (lookup.size() - 1u);
            while (lookup[slot].key != UINT32_MAX && lookup[slot].key != key)
                slot = (slot + 1u) & (lookup.size() - 1u);
            entry = &lookup[slot];
            if (entry->key == key)
            {
                remap[i] = entry->vertex;
                continue;
            }
        }
        ModelVertex v;
        const uint8_t* p = nullptr;
        if (!model_array_at(streams.position, index, streams.position.stride, p))
            return false;
        for (unsigned n = 0; n < 3; ++n)
            v.position[n] = streams.position.stride == 6
                                ? model_short(p + n * 2, streams.position.littleEndian, 1.f / 256.f)
                                : model_float(p + n * 4, streams.position.littleEndian);
        if (streams.position.stride == 16)
            v.position[3] = model_float(p + 12, streams.position.littleEndian);
        if (!streams.color.bytes.empty())
        {
            if (!model_array_at(streams.color, index, 4, p))
                return false;
            std::memcpy(v.color.data(), p, 4);
        }
        if (!streams.uv.bytes.empty())
        {
            if (streams.uv.stride != 4 && streams.uv.stride != 8 && streams.uv.stride != 12)
                return false;
            if (!model_array_at(streams.uv, index, streams.uv.stride, p))
                return false;
            for (unsigned n = 0; n < 2; ++n)
                v.uv[n] = streams.uv.stride == 4
                              ? model_short(p + n * 2, streams.uv.littleEndian, 1.f / 1024.f)
                              : model_float(p + n * 4, streams.uv.littleEndian);
            if (streams.uv.stride == 12)
                v.uv[2] = model_float(p + 8, streams.uv.littleEndian);
        }
        if (!streams.normal.bytes.empty())
        {
            if (streams.normal.stride != 3 && streams.normal.stride != 12)
                return false;
            if (!model_array_at(streams.normal, index, streams.normal.stride, p))
                return false;
            for (unsigned n = 0; n < 3; ++n)
                v.normal[n] = streams.normal.stride == 3
                                  ? float(int8_t(p[n])) / 64.f
                                  : model_float(p + n * 4, streams.normal.littleEndian);
        }
        for (unsigned u = 0; u < 5; ++u)
        {
            const auto& a = streams.extraUv[u];
            if (a.bytes.empty())
                continue;
            if ((a.stride != 4 && a.stride != 8 && a.stride != 12) ||
                !model_array_at(a, index, a.stride, p))
                return false;
            for (unsigned n = 0; n < 2; ++n)
                v.extraUv[u][n] = a.stride == 4
                                      ? model_short(p + n * 2, a.littleEndian, 1.f / 1024.f)
                                      : model_float(p + n * 4, a.littleEndian);
            if (a.stride == 12)
                v.extraUv[u][2] = model_float(p + 8, a.littleEndian);
            for (float f : v.extraUv[u])
                if (!std::isfinite(f))
                    return false;
        }
        if (!selectors.empty())
        {
            const auto* bone = palette[selectors[i]];
            if (!bone)
                return false;
            v.boneSlot = applyBoneMatrix ? 0.f : float(selectors[i] / 3);
            if (applyBoneMatrix)
            {
                const auto position = v.position;
                const auto normal = v.normal;
                for (unsigned row = 0; row < 3; ++row)
                {
                    const auto* m = bone->matrix.data() + row * 4;
                    v.position[row] = m[0] * position[0] + m[1] * position[1] + m[2] * position[2] +
                                      m[3] * position[3];
                    v.normal[row] = m[0] * normal[0] + m[1] * normal[1] + m[2] * normal[2];
                }
            }
        }
        for (float f : v.position)
            if (!std::isfinite(f))
                return false;
        for (float f : v.uv)
            if (!std::isfinite(f))
                return false;
        for (float f : v.normal)
            if (!std::isfinite(f))
                return false;
        if (entry)
        {
            entry->key = index | (uint32_t(selectors.empty() ? 0 : selectors[i]) << 16);
            entry->vertex = uint16_t(candidate.vertices.size());
            remap[i] = entry->vertex;
        }
        candidate.vertices.push_back(v);
    }
    const auto triangle = [&](uint32_t a, uint32_t b, uint32_t c) {
        candidate.indices.push_back(remap.empty() ? uint16_t(a) : remap[a]);
        candidate.indices.push_back(remap.empty() ? uint16_t(b) : remap[b]);
        candidate.indices.push_back(remap.empty() ? uint16_t(c) : remap[c]);
    };
    if (primitive == 0)
        for (uint32_t i = 0; i < count; i += 3)
            triangle(i, i + 1, i + 2);
    if (primitive == 1)
        for (uint32_t i = 2; i < count; ++i)
        {
            if (i & 1)
                triangle(i - 1, i - 2, i);
            else
                triangle(i - 2, i - 1, i);
        }
    if (primitive == 2)
        for (uint32_t i = 2; i < count; ++i)
            triangle(0, i - 1, i);
    if (primitive == 3)
        for (uint32_t i = 0; i < count; i += 4)
        {
            triangle(i, i + 1, i + 2);
            triangle(i, i + 2, i + 3);
        }
    if (primitive == 4)
        for (uint32_t i = 0; i < count; ++i)
            candidate.indices.push_back(remap.empty() ? uint16_t(i) : remap[i]);
    if (primitive == 5)
        for (uint32_t i = 1; i < count; ++i)
        {
            candidate.indices.push_back(remap.empty() ? uint16_t(i - 1) : remap[i - 1]);
            candidate.indices.push_back(remap.empty() ? uint16_t(i) : remap[i]);
        }
    result.complete = true;
    return true;
}
}   // namespace port::nativegxm
