#pragma once

// Offline-prepared static-gameplay-geometry scene for the GX-free GXM test.
// This is NOT a Strikers match replay: AVNR v2 carries static geometry layouts
// but not the original live material program, view order or skinning palette.
#include "gfx/vita_gfx_types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <utility>
#include <vector>

namespace port::directgxm {

struct PackedSceneMesh {
    std::vector<uint8_t> vertices;
    std::vector<uint16_t> indices;
    std::array<float, 3> boundsMin{};
    std::array<float, 3> boundsMax{};
};

struct PackedScene {
    std::vector<PackedSceneMesh> meshes;
};

inline constexpr char kSceneMagic[8] = {'S','M','G','X','M','0','0','1'};
inline constexpr size_t kSceneHeaderBytes = 24;
inline constexpr unsigned kSceneMaxMeshes = 16;
inline constexpr size_t kSceneMaxBytes = 4 * 1024 * 1024;
inline constexpr size_t kSceneVertexStride = 20;

inline uint64_t scene_hash(const uint8_t* p, size_t bytes) noexcept {
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < bytes; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

class SceneReader {
public:
    SceneReader(const uint8_t* p, size_t size) noexcept: data_(p), size_(size) {}
    bool bytes(void* dst, size_t len) noexcept {
        if (!data_ || pos_ > size_ || len > size_ - pos_) return false;
        if (len) std::memcpy(dst, data_ + pos_, len);
        pos_ += len; return true;
    }
    bool word(uint32_t& value) noexcept {
        uint8_t b[4]{};
        if (!bytes(b, sizeof b)) return false;
        value = uint32_t(b[0]) | (uint32_t(b[1]) << 8) |
                (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
        return true;
    }
    bool quad(uint64_t& value) noexcept {
        uint32_t lo = 0, hi = 0;
        if (!word(lo) || !word(hi)) return false;
        value = uint64_t(lo) | (uint64_t(hi) << 32);
        return true;
    }
    bool floats(std::array<float,3>& value) noexcept {
        for (auto& x : value) {
            uint32_t bits = 0;
            if (!word(bits)) return false;
            std::memcpy(&x, &bits, sizeof x);
            if (!std::isfinite(x)) return false;
        }
        return true;
    }
    bool done() const noexcept { return pos_ == size_; }
private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
};

inline aurora::vita::gfx::PipelineDesc scene_pipeline() noexcept {
    using namespace aurora::vita::gfx;
    PipelineDesc p{};
    p.primitive = Primitive::Triangles;
    p.depthTest = true;
    p.depthWrite = true;
    p.depthFunc = Compare::LessEqual;
    p.reversedZ = false;
    p.cull = CullMode::None;
    p.blendMode = BlendMode::None;
    p.fogMode = FogMode::None;
    p.layout.count = 2;
    p.layout.attributes[0] = {0, 4, VertexScalar::F32, false, kSceneVertexStride, 0};
    p.layout.attributes[1] = {1, 4, VertexScalar::U8, true, kSceneVertexStride, 16};
    p.texgenCount = 0;
    p.tev.texCoordCount = 0;
    p.tev.rasterColorCount = 1;
    p.tev.stageCount = 1;
    p.tev.stages[0].color.d = TevColorArg::RasColor;
    p.tev.stages[0].alpha.d = TevAlphaArg::RasAlpha;
    return p;
}

// Every byte is validated before upload. The file is never interpreted as a
// GX display list, and parse failure can never select the legacy renderer.
inline bool parse_scene(const std::vector<uint8_t>& input, PackedScene& output) {
    output = {};
    if (input.size() < kSceneHeaderBytes || input.size() > kSceneMaxBytes ||
        std::memcmp(input.data(), kSceneMagic, sizeof kSceneMagic)) return false;
    SceneReader r(input.data() + 8, input.size() - 8);
    uint32_t version = 0, meshCount = 0;
    uint64_t hash = 0;
    if (!r.word(version) || !r.word(meshCount) || !r.quad(hash) ||
        version != 1 || meshCount == 0 || meshCount > kSceneMaxMeshes ||
        hash != scene_hash(input.data() + kSceneHeaderBytes,
                           input.size() - kSceneHeaderBytes))
        return false;

    PackedScene candidate;
    candidate.meshes.reserve(meshCount);
    for (uint32_t meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
        uint32_t nv = 0, ni = 0;
        if (!r.word(nv) || !r.word(ni) ||
            nv < 3 || nv > 64000 || ni < 3 || ni > 192000 || ni % 3)
            return false;

        PackedSceneMesh mesh;
        if (!r.floats(mesh.boundsMin) || !r.floats(mesh.boundsMax))
            return false;
        for (size_t axis = 0; axis < 3; ++axis)
            if (mesh.boundsMin[axis] > mesh.boundsMax[axis])
                return false;

        mesh.vertices.resize(size_t(nv) * kSceneVertexStride);
        if (!r.bytes(mesh.vertices.data(), mesh.vertices.size()))
            return false;
        // Assert real GPU vertex byte layout and finite positions before GPU DMA.
        for (uint32_t vi = 0; vi < nv; ++vi) {
            const uint8_t* vertex = mesh.vertices.data() + size_t(vi) * kSceneVertexStride;
            for (int axis = 0; axis < 4; ++axis) {
                float f = 0.f;
                std::memcpy(&f, vertex + axis * sizeof(float), sizeof(float));
                if (!std::isfinite(f)) return false;
            }
        }
        mesh.indices.resize(ni);
        for (auto& idx : mesh.indices) {
            uint8_t bytes[2]{};
            if (!r.bytes(bytes, 2)) return false;
            idx = uint16_t(bytes[0]) | uint16_t(bytes[1] << 8);
            if (idx >= nv) return false;
        }
        candidate.meshes.push_back(std::move(mesh));
    }
    if (!r.done()) return false;
    output = std::move(candidate);
    return true;
}

} // namespace port::directgxm
