#pragma once

// Source-matched, offline-prepared native GameCube GLG mesh records.
// Shader/material/animation coverage is intentionally NOT inferred here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace port::directmatch {
constexpr uint8_t kMagic[8] = {'S','M','N','V','M','0','0','1'};
constexpr size_t kHeaderSize = 24;
constexpr unsigned kMaxMeshes = 5000;
constexpr size_t kMaxManifestBytes = 16 * 1024 * 1024;
constexpr size_t kVertexStride = 20;

inline uint64_t fnv64(const uint8_t* data, size_t length) noexcept {
    uint64_t state = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < length; ++i) {
        state ^= data[i];
        state *= 0x100000001b3ull;
    }
    return state;
}

struct SourceIdentity {
    uint32_t drawVertices = 0;
    uint32_t primitive = 0;
    uint32_t rawBytes = 0;
    uint32_t positionBytes = 0;
    uint32_t colorBytes = 0;
    uint64_t rawHash = 0;
    uint64_t positionHash = 0;
    uint64_t colorHash = 0;
    bool operator==(const SourceIdentity&) const = default;
};

struct Mesh {
    SourceIdentity source{};
    std::vector<uint8_t> vertices;
    std::vector<uint16_t> indices;
};

struct Manifest {
    std::vector<Mesh> meshes;
};

struct Reader {
    const uint8_t* data = nullptr;
    size_t length = 0, offset = 0;
    bool read(void* target, size_t bytes) noexcept {
        if (!data || offset > length || bytes > length - offset) return false;
        if (bytes) std::memcpy(target, data + offset, bytes);
        offset += bytes;
        return true;
    }
    bool u32(uint32_t& v) noexcept {
        uint8_t b[4];
        if (!read(b, sizeof b)) return false;
        v = uint32_t(b[0]) | (uint32_t(b[1]) << 8) |
            (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
        return true;
    }
    bool u64(uint64_t& v) noexcept {
        uint32_t lo = 0, hi = 0;
        if (!u32(lo) || !u32(hi)) return false;
        v = uint64_t(lo) | uint64_t(hi) << 32;
        return true;
    }
};

inline bool parse(const std::vector<uint8_t>& bytes, Manifest& out) {
    out.meshes.clear();
    if (bytes.size() < kHeaderSize || bytes.size() > kMaxManifestBytes ||
        std::memcmp(bytes.data(), kMagic, 8)) return false;
    Reader r{bytes.data(), bytes.size(), 8};
    uint32_t version = 0, meshCount = 0;
    uint64_t payloadHash = 0;
    if (!r.u32(version) || !r.u32(meshCount) || !r.u64(payloadHash) ||
        version != 1 || meshCount == 0 || meshCount > kMaxMeshes ||
        fnv64(bytes.data() + kHeaderSize, bytes.size() - kHeaderSize) != payloadHash)
        return false;
    Manifest candidate;
    candidate.meshes.reserve(meshCount);
    size_t gpuBytes = 0;
    for (uint32_t i = 0; i < meshCount; ++i) {
        Mesh mesh;
        uint32_t nv = 0, ni = 0;
        auto& s = mesh.source;
        if (!r.u32(s.drawVertices) || !r.u32(s.primitive) ||
            !r.u32(s.rawBytes) || !r.u32(s.positionBytes) ||
            !r.u32(s.colorBytes) || !r.u32(nv) || !r.u32(ni) ||
            !r.u64(s.rawHash) || !r.u64(s.positionHash) || !r.u64(s.colorHash) ||
            s.drawVertices < 3 || s.drawVertices > 64000 || s.primitive > 5 ||
            s.rawBytes == 0 || s.rawBytes > 512 * 1024 ||
            s.positionBytes == 0 || s.positionBytes > 1024 * 1024 ||
            s.colorBytes > 512 * 1024 ||
            nv < 3 || nv > 64000 || ni < 3 || ni > 192000 || ni % 3)
            return false;
        const size_t vertexBytes = size_t(nv) * kVertexStride;
        const size_t indexBytes = size_t(ni) * sizeof(uint16_t);
        if (gpuBytes + vertexBytes + indexBytes > kMaxManifestBytes)
            return false;
        gpuBytes += vertexBytes + indexBytes;
        mesh.vertices.resize(vertexBytes);
        if (!r.read(mesh.vertices.data(), vertexBytes)) return false;
        mesh.indices.resize(ni);
        for (uint16_t& index : mesh.indices) {
            uint8_t b[2]{};
            if (!r.read(b, sizeof b)) return false;
            index = uint16_t(uint16_t(b[0]) | uint16_t(b[1]) << 8);
            if (index >= nv) return false;
        }
        candidate.meshes.push_back(std::move(mesh));
    }
    if (r.offset != bytes.size()) return false;
    out = std::move(candidate);
    return true;
}

inline const Mesh* find(const Manifest& pack, const SourceIdentity& key) noexcept {
    for (const auto& model : pack.meshes)
        if (model.source == key) return &model;
    return nullptr;
}
}
