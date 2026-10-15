#include "port/native_gxm_direct_scene.hpp"

#if defined(PORT_VITA) && defined(STRIKERS_VITA_GXM_DIRECT_PROBE)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace port::directgxm {
namespace {

constexpr const char* kPackPath = "ux0:data/strikersVita/gxm_direct_static_scene.bin";

// Storage input is bounded and read just once at boot, never in the draw loop.
bool read_scene_pack(std::vector<uint8_t>& bytes) noexcept {
    std::FILE* f = std::fopen(kPackPath, "rb");
    if (!f) return false;
    const bool sized = std::fseek(f, 0, SEEK_END) == 0;
    const long end = sized ? std::ftell(f) : -1;
    if (end < static_cast<long>(kSceneHeaderBytes) ||
        end > static_cast<long>(kSceneMaxBytes) ||
        std::fseek(f, 0, SEEK_SET)) {
        std::fclose(f);
        return false;
    }
    bytes.resize(static_cast<size_t>(end));
    const bool loaded = std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size() &&
                        !std::ferror(f);
    const bool closed = std::fclose(f) == 0;
    if (!loaded || !closed) {
        bytes.clear();
        return false;
    }
    return true;
}

aurora::vita::gfx::DrawPacket make_instance(
        uint64_t pipeline,
        const aurora::vita::gfx::BufferSlice& vertices,
        const aurora::vita::gfx::BufferSlice& indices,
        uint32_t vertexCount, uint32_t indexCount,
        const std::array<float,3>& min,
        const std::array<float,3>& max,
        unsigned index, unsigned draws) noexcept {
    using namespace aurora::vita::gfx;
    DrawPacket packet{};
    packet.pipelineKey = pipeline;
    packet.vertices = vertices;
    packet.indices = indices;
    packet.vertexCount = vertexCount;
    packet.indexCount = indexCount;
    packet.instanceCount = 1;
    packet.viewport = {0.f, 0.f, 960.f, 544.f, 0.f, 1.f};
    packet.scissor = {0, 0, 960, 544};

    // Each mesh is displayed in a small viewport tile. Source object-space
    // positions never change; the native vertex shader applies one GPU MVP.
    constexpr unsigned cols = 8;
    const unsigned rows = (draws + cols - 1u) / cols;
    const float cellX = 2.f / cols, cellY = 2.f / rows;
    const float cx = (min[0] + max[0]) * .5f;
    const float cy = (min[1] + max[1]) * .5f;
    const float cz = (min[2] + max[2]) * .5f;
    const float ex = std::max(max[0] - min[0], .0001f);
    const float ey = std::max(max[1] - min[1], .0001f);
    const float ez = std::max(max[2] - min[2], .0001f);
    const float scale = std::min({cellX * .78f / ex, cellY * .78f / ey, .34f / ez});
    auto& mvp = packet.uniforms.mvp;
    mvp = {scale,0,0,0, 0,scale,0,0, 0,0,scale,0,
           -1.f + (float(index % cols) + .5f) * cellX - cx * scale,
            1.f - (float(index / cols) + .5f) * cellY - cy * scale,
            -.25f - cz * scale,1.f};

    // Exact per-instance identity; no memcmp or generation of GX XF writes.
    const uint64_t revision = uint64_t(index) + 1u;
    packet.uniformRevision = revision;
    packet.vertexUniformRevision = revision;
    packet.fragmentUniformRevision = 1u;
    packet.textureBindingRevision = 1u;
    return packet;
}

} // namespace

bool NativeStaticScene::initialize(aurora::vita::gxm::Renderer& renderer,
                                   unsigned draws) noexcept {
    if (!resident_.empty() || pipeline_ || !packets_.empty()) return false;
    failure_ = nullptr;
    if (draws == 0 || draws > 256u) {
        failure_ = "invalid_direct_scene_draw_count";
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!read_scene_pack(bytes)) {
        failure_ = "scene_pack_missing_or_io_error";
        return false;
    }
    PackedScene scene;
    if (!parse_scene(bytes, scene)) {
        failure_ = "scene_pack_invalid_or_checksum_failed";
        return false;
    }
    pipeline_ = renderer.create_pipeline(scene_pipeline());
    if (!pipeline_) {
        failure_ = "scene_pipeline_gxm_compilation_failed";
        return false;
    }

    resident_.reserve(scene.meshes.size());
    for (const auto& mesh : scene.meshes) {
        ResidentMesh gpu{};
        gpu.vertexCount = static_cast<uint32_t>(mesh.vertices.size() / kSceneVertexStride);
        gpu.indexCount = static_cast<uint32_t>(mesh.indices.size());
        gpu.vertexBytes = static_cast<uint32_t>(mesh.vertices.size());
        gpu.indexBytes = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint16_t));
        gpu.boundsMin = mesh.boundsMin; gpu.boundsMax = mesh.boundsMax;
        gpu.vertices = renderer.create_buffer(mesh.vertices.data(), mesh.vertices.size());
        if (!gpu.vertices) {
            failure_ = "scene_vertex_buffer_upload_failed";
            return false;
        }
        // Track the first allocation so a later index upload failure releases it.
        resident_.push_back(gpu);
        gpu.indices = renderer.create_buffer(mesh.indices.data(), gpu.indexBytes);
        if (!gpu.indices) {
            failure_ = "scene_index_buffer_upload_failed";
            return false;
        }
        resident_.back().indices = gpu.indices;
    }
    if (resident_.empty()) {
        failure_ = "scene_pack_no_geometry";
        return false;
    }

    packets_.reserve(draws);
    for (unsigned i = 0; i < draws; ++i) {
        const auto& mesh = resident_[i % resident_.size()];
        packets_.push_back(make_instance(pipeline_,
            {mesh.vertices, 0, mesh.vertexBytes},
            {mesh.indices, 0, mesh.indexBytes},
            mesh.vertexCount, mesh.indexCount,
            mesh.boundsMin, mesh.boundsMax, i, draws));
    }
    return true;
}

bool NativeStaticScene::submit(aurora::vita::gxm::Renderer& renderer) noexcept {
    if (!pipeline_ || packets_.empty()) return false;
    for (const auto& packet : packets_)
        if (!renderer.draw(packet, true)) return false;
    return true;
}

void NativeStaticScene::shutdown(aurora::vita::gxm::Renderer& renderer) noexcept {
    // One end-of-test GPU fence, not a fence per draw or per resource.
    if (pipeline_ || !resident_.empty())
        (void)renderer.finish();
    for (const auto& mesh : resident_) {
        if (mesh.indices) renderer.destroy_buffer(mesh.indices);
        if (mesh.vertices) renderer.destroy_buffer(mesh.vertices);
    }
    if (pipeline_) renderer.destroy_pipeline(pipeline_);
    packets_.clear();
    resident_.clear();
    pipeline_ = 0;
}

} // namespace port::directgxm
#endif
