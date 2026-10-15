#include "port/native_gxm_direct_data.hpp"
#include "port/native_gxm_scene_pack.hpp"
#include "gxm/gxm_shader_gen.hpp"
#include "gfx/vita_cpu_workers.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Shader-generation validation does not run vertex jobs. Satisfy the single
// unused worker reference without starting platform worker threads on the host.
namespace aurora::vita::gfx {
bool cpu_parallel_for_vertex(size_t count, CpuRangeTask fn, void* ctx) noexcept {
    return !count || fn(ctx, 0, count, 0);
}
}

namespace {
using namespace port::directgxm;
using namespace aurora::vita::gfx;

void check(bool ok, int line) {
    if (!ok) {
        std::fprintf(stderr, "gxm_direct_probe_contract: failure at %d\n", line);
        std::exit(1);
    }
}
#define CHECK(test) check(bool(test), __LINE__)

void direct_contract() {
    auto pipeline = color_pipeline();
    const auto mesh = probe_mesh(pipeline);
    CHECK(sizeof(ColorVertex) == 32);
    CHECK(admit(mesh) == Admission::Accepted);
    CHECK(pipeline_sampled_texture_mask(pipeline) == 0);
    CHECK(pipeline.layout.attributes[0].stride == sizeof(ColorVertex));
    CHECK(pipeline.tev.stages[0].color.d == TevColorArg::RasColor);
    CHECK(pipeline.tev.stages[0].alpha.d == TevAlphaArg::RasAlpha);
    const auto shader = aurora::vita::gxm::build_tev_cg(pipeline);
    CHECK(shader.ok());
    CHECK(shader.textureMask == 0);
    CHECK(shader.colorMask == 1);
    CHECK(shader.vertex.find("a_color0") != std::string::npos);

    auto broken = mesh;
    broken.vertexBytes -= 1;
    CHECK(admit(broken) == Admission::MissingGeometry);
    broken = mesh;
    broken.indices = nullptr;
    CHECK(admit(broken) == Admission::MissingGeometry);
    broken = mesh;
    broken.indexCount = 4;
    CHECK(admit(broken) == Admission::MissingGeometry);
    broken = mesh;
    broken.indexBytes -= 2;
    CHECK(admit(broken) == Admission::MissingGeometry);

    std::array<uint16_t, 36> corrupt = kProbeIndices;
    corrupt[2] = 65535;
    broken = mesh;
    broken.indices = corrupt.data();
    CHECK(admit(broken) == Admission::InvalidIndex);

    pipeline.layout.attributes[1].offset = 12;
    CHECK(admit(mesh) == Admission::UnsupportedLayout);
    pipeline = color_pipeline();
    pipeline.fixedVertexOnGpu = true;
    CHECK(admit(mesh) == Admission::UnsupportedMaterial);
    pipeline = color_pipeline();
    pipeline.tev.stages[0].color.d = TevColorArg::TexColor;
    pipeline.tev.stages[0].texture = 0;
    CHECK(admit(mesh) == Admission::UnsupportedMaterial);
    pipeline = color_pipeline();
    pipeline.tev.indirectStageCount = 1;
    CHECK(admit(mesh) == Admission::UnsupportedMaterial);
    pipeline = color_pipeline();
    pipeline.primitive = Primitive::Lines;
    CHECK(admit(mesh) == Admission::UnsupportedMaterial);

    pipeline = color_pipeline();
    pipeline.tev.stages[0].color.a = TevColorArg::Reg1;
    CHECK(admit(mesh) == Admission::UnsupportedMaterial);

    pipeline = color_pipeline();
    CHECK(admit(mesh) == Admission::Accepted);
}

void scene_contract() {
    using namespace port::directgxm;
    auto pipeline = scene_pipeline();
    CHECK(pipeline.layout.attributes[0].stride == kSceneVertexStride);
    CHECK(pipeline.layout.attributes[1].scalar == VertexScalar::U8);
    CHECK(pipeline.layout.attributes[1].normalized);
    const auto shader = aurora::vita::gxm::build_tev_cg(pipeline);
    CHECK(shader.ok() && shader.textureMask == 0 && shader.colorMask == 1);

    std::vector<uint8_t> wire(kSceneHeaderBytes);
    std::memcpy(wire.data(), kSceneMagic, sizeof kSceneMagic);
    auto push32 = [&](uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) wire.push_back(uint8_t(value >> (8 * i)));
    };
    auto pushFloat = [&](float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof bits);
        push32(bits);
    };
    push32(3);push32(3); // One triangle with 3 vertices.
    for (float value : {-1.f,-1.f,-1.f,1.f,1.f,1.f}) pushFloat(value);
    for (float vertex : {-1.f,-1.f,0.f,1.f,
                          1.f,-1.f,0.f,1.f,
                          0.f,1.f,0.f,1.f}) {
        pushFloat(vertex);
        if (((wire.size() - (kSceneHeaderBytes + 32)) % kSceneVertexStride) == 16) {
            for (uint8_t color : {uint8_t(255), uint8_t(120), uint8_t(30), uint8_t(255)})
                wire.push_back(color);
        }
    }
    for (uint8_t value : {uint8_t(0),uint8_t(0),uint8_t(1),uint8_t(0),uint8_t(2),uint8_t(0)})
        wire.push_back(value);
    // Header: magic(8), version(4), mesh count(4), FNV1A payload hash(8).
    wire[8] = 1; wire[12] = 1;
    const uint64_t hash = scene_hash(wire.data() + kSceneHeaderBytes,
                                      wire.size() - kSceneHeaderBytes);
    for (unsigned i = 0; i < 8; ++i)
        wire[16 + i] = uint8_t(hash >> (8 * i));

    PackedScene result;
    CHECK(parse_scene(wire, result));
    CHECK(result.meshes.size() == 1);
    CHECK(result.meshes[0].vertices.size() == 3 * kSceneVertexStride);
    CHECK(result.meshes[0].indices == std::vector<uint16_t>({0,1,2}));

    auto invalid = wire;
    invalid.back() ^= 1u;
    CHECK(!parse_scene(invalid, result));
    invalid = wire;
    invalid.pop_back();
    CHECK(!parse_scene(invalid, result));
    invalid = wire;
    invalid[0] ^= 1u;
    CHECK(!parse_scene(invalid, result));
    invalid = wire;
    invalid[12] = uint8_t(kSceneMaxMeshes + 1u);
    CHECK(!parse_scene(invalid, result));
}
}

int main() {
    direct_contract();
    scene_contract();
    std::puts("gxm direct probe contract: native packed mesh admitted; invalid/material fallback rejected");
}
