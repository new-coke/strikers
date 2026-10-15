#pragma once

// Typed, already-packed GXM geometry. This contract never accepts GX display
// lists, indexed GX attribute streams, or runtime model-format conversion.
#include "gfx/vita_gfx_types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace port::directgxm {

struct ColorVertex {
    float position[4];
    float color[4];
};
static_assert(sizeof(ColorVertex) == 32);

struct MeshView {
    const void* packedVertices = nullptr;
    size_t vertexBytes = 0;
    const uint16_t* indices = nullptr;
    size_t indexBytes = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    const aurora::vita::gfx::PipelineDesc* pipeline = nullptr;
};

enum class Admission : uint8_t {
    Accepted, MissingGeometry, UnsupportedLayout, UnsupportedMaterial, InvalidIndex
};

// First direct-GXM contract: position+RGBA in the exact GPU layout, an unlit
// single-stage vertex-color material and immutable indexed triangles. Missing
// material features are rejected, never executed through GX as a fallback.
inline aurora::vita::gfx::PipelineDesc color_pipeline() noexcept {
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
    p.layout.attributes[0] = {0, 4, VertexScalar::F32, false, sizeof(ColorVertex), 0};
    p.layout.attributes[1] = {1, 4, VertexScalar::F32, false, sizeof(ColorVertex), 16};
    p.texgenCount = 0;
    p.tev.texCoordCount = 0;
    p.tev.rasterColorCount = 1;
    p.tev.stageCount = 1;
    p.tev.stages[0].color.d = TevColorArg::RasColor;
    p.tev.stages[0].alpha.d = TevAlphaArg::RasAlpha;
    return p;
}

inline Admission admit(const MeshView& mesh) noexcept {
    using namespace aurora::vita::gfx;
    if (!mesh.packedVertices || !mesh.indices || !mesh.pipeline ||
        mesh.vertexCount < 3 || mesh.vertexCount > 64000 ||
        mesh.indexCount < 3 || mesh.indexCount > 192000 || mesh.indexCount % 3 ||
        mesh.vertexBytes != size_t(mesh.vertexCount) * sizeof(ColorVertex) ||
        mesh.indexBytes != size_t(mesh.indexCount) * sizeof(uint16_t))
        return Admission::MissingGeometry;

    const auto& p = *mesh.pipeline;
    const auto& a = p.layout.attributes;
    if (p.layout.count != 2 || a[0].location != 0 || a[0].components != 4 ||
        a[0].scalar != VertexScalar::F32 || a[0].normalized ||
        a[0].stride != sizeof(ColorVertex) || a[0].offset != 0 ||
        a[1].location != 1 || a[1].components != 4 ||
        a[1].scalar != VertexScalar::F32 || a[1].normalized ||
        a[1].stride != sizeof(ColorVertex) || a[1].offset != 16)
        return Admission::UnsupportedLayout;

    if (p.primitive != Primitive::Triangles || p.fixedVertexOnGpu ||
        p.fixedVertexIndexedPn || p.positionIsClipSpace ||
        p.texgenCount != 0 || p.tev.stageCount != 1 ||
        p.tev.texCoordCount != 0 || p.tev.rasterColorCount != 1 ||
        p.tev.indirectStageCount != 0 || p.fogMode != FogMode::None ||
        p.tev.stages[0].indirectEnabled ||
        p.tev.stages[0].texture != 0xff ||
        p.tev.stages[0].color.a != TevColorArg::Zero ||
        p.tev.stages[0].color.b != TevColorArg::Zero ||
        p.tev.stages[0].color.c != TevColorArg::Zero ||
        p.tev.stages[0].alpha.a != TevAlphaArg::Zero ||
        p.tev.stages[0].alpha.b != TevAlphaArg::Zero ||
        p.tev.stages[0].alpha.c != TevAlphaArg::Zero ||
        p.tev.stages[0].color.d != TevColorArg::RasColor ||
        p.tev.stages[0].alpha.d != TevAlphaArg::RasAlpha ||
        pipeline_sampled_texture_mask(p) != 0)
        return Admission::UnsupportedMaterial;

    for (uint32_t i = 0; i < mesh.indexCount; ++i)
        if (mesh.indices[i] >= mesh.vertexCount)
            return Admission::InvalidIndex;
    return Admission::Accepted;
}

// A small already-packed native fixture. Replacing these arrays with a mesh
// sidecar is a pure resource-upload change: the submit path is identical.
inline constexpr std::array<ColorVertex, 8> kProbeVertices{{
    {{-.46f, -.64f, -.30f, 1.f}, {1.f, .20f, .16f, 1.f}},
    {{ .46f, -.64f, -.30f, 1.f}, {.18f, 1.f, .28f, 1.f}},
    {{ .46f,  .64f, -.30f, 1.f}, {.19f, .34f, 1.f, 1.f}},
    {{-.46f,  .64f, -.30f, 1.f}, {1.f, .91f, .26f, 1.f}},
    {{-.29f, -.43f,  .30f, 1.f}, {.60f, .10f, .08f, 1.f}},
    {{ .64f, -.43f,  .30f, 1.f}, {.09f, .65f, .12f, 1.f}},
    {{ .64f,  .83f,  .30f, 1.f}, {.10f, .17f, .63f, 1.f}},
    {{-.29f,  .83f,  .30f, 1.f}, {.68f, .55f, .14f, 1.f}}
}};
inline constexpr std::array<uint16_t, 36> kProbeIndices{{
    0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
    0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
    2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7
}};

inline MeshView probe_mesh(const aurora::vita::gfx::PipelineDesc& pipeline) noexcept {
    return {kProbeVertices.data(), sizeof(kProbeVertices), kProbeIndices.data(),
            sizeof(kProbeIndices), static_cast<uint32_t>(kProbeVertices.size()),
            static_cast<uint32_t>(kProbeIndices.size()), &pipeline};
}

} // namespace port::directgxm
