#pragma once

// Backend-neutral source identities and GPU buffer slices for Strikers' native
// meshes. No GX command state, Aurora handle or sceGxm allocation is stored.
#include "port/native_match_manifest.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace port::directmatch {

struct GpuMeshSlice {
    SourceIdentity source{};
    uint32_t vertexOffset = 0;
    uint32_t vertexBytes = 0;
    uint32_t indexOffset = 0;
    uint32_t indexBytes = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

class PreparedGpuAssets {
public:
    // Consumes the decoded AVNR records: their separate CPU allocations are
    // released while contiguous GPU upload buffers are being assembled.
    bool prepare(Manifest&& manifest) {
        clear();
        if (manifest.meshes.empty() || manifest.meshes.size() > kMaxMeshes)
            return false;

        size_t vertexBytes = 0, indexCount = 0;
        for (const auto& mesh : manifest.meshes) {
            if (mesh.vertices.empty() || mesh.vertices.size() % kVertexStride ||
                mesh.indices.empty() || mesh.indices.size() % 3)
                return false;
            const size_t vb = mesh.vertices.size();
            const size_t ib = mesh.indices.size() * sizeof(uint16_t);
            if (vb > kMaxManifestBytes || ib > kMaxManifestBytes ||
                vertexBytes > kMaxManifestBytes - vb ||
                indexCount * sizeof(uint16_t) > kMaxManifestBytes - vertexBytes - vb ||
                ib > kMaxManifestBytes - vertexBytes - vb -
                     indexCount * sizeof(uint16_t))
                return false;
            vertexBytes += vb;
            indexCount += mesh.indices.size();
        }

        vertices_.reserve(vertexBytes);
        indices_.reserve(indexCount);
        meshes_.reserve(manifest.meshes.size());
        rawIndex_.reserve(manifest.meshes.size());
        for (auto& mesh : manifest.meshes) {
            const auto index = static_cast<uint32_t>(meshes_.size());
            const auto vb = static_cast<uint32_t>(mesh.vertices.size());
            const auto ic = static_cast<uint32_t>(mesh.indices.size());
            meshes_.push_back({
                mesh.source, static_cast<uint32_t>(vertices_.size()), vb,
                static_cast<uint32_t>(indices_.size() * sizeof(uint16_t)),
                static_cast<uint32_t>(ic * sizeof(uint16_t)),
                static_cast<uint32_t>(vb / kVertexStride), ic
            });
            vertices_.insert(vertices_.end(), mesh.vertices.begin(), mesh.vertices.end());
            indices_.insert(indices_.end(), mesh.indices.begin(), mesh.indices.end());
            rawIndex_.emplace_back(mesh.source.rawHash, index);

            // clear() leaves every decoded mesh allocation retained. Release
            // its capacity now instead of keeping 1,800 redundant CPU copies.
            std::vector<uint8_t>().swap(mesh.vertices);
            std::vector<uint16_t>().swap(mesh.indices);
        }
        std::sort(rawIndex_.begin(), rawIndex_.end());
        return true;
    }

    uint32_t find(const SourceIdentity& identity) const noexcept {
        const auto first = std::lower_bound(
            rawIndex_.begin(), rawIndex_.end(),
            std::pair<uint64_t, uint32_t>{identity.rawHash, 0u});
        for (auto it = first; it != rawIndex_.end() &&
             it->first == identity.rawHash; ++it) {
            if (meshes_[it->second].source == identity)
                return it->second;
        }
        return ~0u;
    }

    // Call only after a *synchronous* upload has completed. GPU buffer handles
    // are owned by the calling renderer; identifiers and offsets stay resident.
    void release_upload_bytes() noexcept {
        std::vector<uint8_t>().swap(vertices_);
        std::vector<uint16_t>().swap(indices_);
    }

    void clear() noexcept {
        release_upload_bytes();
        std::vector<GpuMeshSlice>().swap(meshes_);
        std::vector<std::pair<uint64_t, uint32_t>>().swap(rawIndex_);
    }

    const std::vector<GpuMeshSlice>& meshes() const noexcept { return meshes_; }
    const std::vector<uint8_t>& vertices() const noexcept { return vertices_; }
    const std::vector<uint16_t>& indices() const noexcept { return indices_; }
    size_t mesh_count() const noexcept { return meshes_.size(); }
    size_t upload_bytes() const noexcept {
        return vertices_.size() + indices_.size() * sizeof(uint16_t);
    }

private:
    std::vector<GpuMeshSlice> meshes_;
    std::vector<uint8_t> vertices_;
    std::vector<uint16_t> indices_;
    std::vector<std::pair<uint64_t, uint32_t>> rawIndex_;
};

} // namespace port::directmatch
