#pragma once
#include "port/native_model_vertices.hpp"
#include <algorithm>
#include <bit>

namespace port::nativegxm
{
// Admission only: no vertex or index contents are retained or read here.
// The caller additionally requires an explicit current-frame net provenance
// tag and excludes native display-list/resident/PN paths.
inline size_t bounded_net_record_count(const ModelStreams& streams,
                                      std::span<const uint16_t> indices,
                                      uint32_t count, uint8_t primitive) noexcept
{
    if constexpr (std::endian::native != std::endian::little)
        return 0;
    if (primitive != 1 || count < 3 || count > 64000 || indices.size() != count ||
        !indices.data() || streams.position.stride != 12 || !streams.position.littleEndian ||
        streams.color.stride != 4 || streams.uv.stride != 4 || !streams.uv.littleEndian ||
        streams.position.bytes.empty() || streams.color.bytes.empty() || streams.uv.bytes.empty() ||
        !streams.position.bytes.data() || !streams.color.bytes.data() || !streams.uv.bytes.data() ||
        !streams.normal.bytes.empty())
        return 0;
    for (const auto& uv : streams.extraUv)
        if (!uv.bytes.empty())
            return 0;
    const auto records = std::min({ streams.position.bytes.size() / 12,
                                    streams.color.bytes.size() / 4, streams.uv.bytes.size() / 4 });
    // Reuse the existing scratch allocation within its retention bound. The
    // direct table never requires more entries than the generic hash table.
    return records && records <= 4096 && records <= size_t(count) * 2 ? records : 0;
}

inline bool prepare_net_model_vertices(const ModelStreams& streams,
                                       std::span<const uint16_t> indices,
                                       uint32_t count, uint8_t primitive,
                                       ModelVertices& out, ModelVertexScratch& scratch)
{
    out.vertices.clear();
    out.indices.clear();
    struct ClearOnFailure
    {
        ModelVertices& out;
        bool complete = false;
        ~ClearOnFailure()
        {
            if (!complete)
            {
                out.vertices.clear();
                out.indices.clear();
            }
        }
    } result{ out };
    const auto records = bounded_net_record_count(streams, indices, count, primitive);
    if (!records)
        return false;
    out.vertices.reserve(std::min(size_t(count), records));
    out.indices.reserve(size_t(count - 2) * 3);
    scratch.lookup.clear();
    scratch.remap.clear();
    scratch.lookup.resize(records);
    scratch.remap.resize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto index = indices[i];
        if (index >= records)
            return false;
        auto& entry = scratch.lookup[index];
        if (entry.key != UINT32_MAX)
        {
            scratch.remap[i] = entry.vertex;
            continue;
        }
        // Write the owned current draw snapshot directly. Defaults are the
        // exact ModelVertex defaults; no prior frame's data is reused.
        auto& v = out.vertices.emplace_back();
        std::memcpy(v.position.data(), streams.position.bytes.data() + size_t(index) * 12, 12);
        std::memcpy(v.color.data(), streams.color.bytes.data() + size_t(index) * 4, 4);
        const auto* uv = streams.uv.bytes.data() + size_t(index) * 4;
        v.uv[0] = model_short(uv, true, 1.f / 1024.f);
        v.uv[1] = model_short(uv + 2, true, 1.f / 1024.f);
        if (!std::isfinite(v.position[0]) || !std::isfinite(v.position[1]) ||
            !std::isfinite(v.position[2]))
            return false;
        entry.key = index;
        entry.vertex = uint16_t(out.vertices.size() - 1);
        scratch.remap[i] = entry.vertex;
    }
    // Preserve all strip triangles, winding and degenerate references exactly.
    for (uint32_t i = 2; i < count; ++i)
    {
        const auto a = (i & 1) ? i - 1 : i - 2;
        const auto b = (i & 1) ? i - 2 : i - 1;
        out.indices.push_back(scratch.remap[a]);
        out.indices.push_back(scratch.remap[b]);
        out.indices.push_back(scratch.remap[i]);
    }
    result.complete = true;
    return true;
}
} // namespace port::nativegxm
