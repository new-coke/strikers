#pragma once

#include "vita_vertex_pipeline.hpp"
#include "../../../lib/gx/native_model_options.hpp"
#include "../../../lib/gx/state_revisions.hpp"
#include <algorithm>
#include <cstdint>

namespace aurora::vita::gfx {

inline uint64_t fragment_uniform_revision(const PipelineDesc& pipeline,
                                         const aurora::gx::StateRevisions& revisions) noexcept {
  uint64_t revision = revisions.fragment;
  // Fog reads viewport coordinates; indirect TEV reads texture size/LOD.
  // Domain revisions share the same monotonic serial, so max preserves changes.
  if (pipeline.fogMode != FogMode::None) revision = std::max(revision, revisions.vertex);
  if (pipeline.tev.indirectStageCount) revision = std::max(revision, revisions.textures);
  return revision;
}

struct NativeModelUniformStamp {
  uint64_t recipe = 0, vertex = 0, fragment = 0;
  uint32_t stateIdentity = 0, vertexProgram = 0;
};

// Exact set of inputs copied by translate_fixed_vertex_state, compiled when a
// resident recipe is captured. Mesh identity and shader identity are deliberately
// absent: different programs can consume the same CPU snapshot. This is not a
// shader compatibility certificate; the native guard still checks that program.
struct NativeModelUniformContract {
  uint16_t textureMatrices = 0;
  uint32_t postMatrices = 0;
  uint8_t vertexFlags = 0; // normal, complete PN palette, complete texture palette
  uint8_t colors = 0, lights = 0;
  uint8_t fragmentFlags = 0; // fog, fog range, indirect

  bool same_vertex(const NativeModelUniformContract& other) const noexcept {
    return textureMatrices == other.textureMatrices && postMatrices == other.postMatrices &&
        vertexFlags == other.vertexFlags && colors == other.colors && lights == other.lights;
  }
};

inline NativeModelUniformContract native_model_uniform_contract(const PipelineDesc& pipeline) noexcept {
  NativeModelUniformContract result{};
  const auto requirements = vertex_pipeline_requirements(pipeline);
  result.vertexFlags = uint8_t(requirements.needNormal) |
      (uint8_t(pipeline.fixedVertexIndexedPn || pipeline.fixedVertexTexMtxMask) << 1) |
      (uint8_t(pipeline.fixedVertexTexMtxMask != 0) << 2);
  result.colors = requirements.colorMask;
  for (unsigned ch = 0; ch < pipeline.colorChannels.size(); ++ch)
    if ((result.colors & (1u << (ch & 1u))) && pipeline.colorChannels[ch].lightingEnabled)
      result.lights |= pipeline.colorChannels[ch].lightMask;
  const auto texgenMask = pipeline_texgen_compute_mask(pipeline);
  for (unsigned i = 0; i < pipeline.texgenCount && i < MaxTextures; ++i) if (texgenMask & (1u << i)) {
    const auto& t = pipeline.texgens[i];
    if (t.matrix >= 0 && t.matrix < 10) result.textureMatrices |= uint16_t(1u << t.matrix);
    if (t.postMatrix >= 0 && t.postMatrix < 20) result.postMatrices |= 1u << t.postMatrix;
    if (texgen_type_is_bump(t.type)) result.lights |= uint8_t(1u << std::min<unsigned>(
        static_cast<unsigned>(t.type) - static_cast<unsigned>(TexGenType::Bump0), MaxLights - 1));
  }
  if (pipeline.fogMode != FogMode::None)
    result.fragmentFlags = 1u | (uint8_t(pipeline.fogRangeEnabled) << 1);
  if (pipeline.tev.indirectStageCount) result.fragmentFlags |= 4u;
  return result;
}

// One owner-thread CPU snapshot, independent of the bounded resident meshes.
// Queued draws receive an immutable pool publication; no transient GPU uniform
// address is cached here, and no reservation is shared across GXM scenes.
class NativeModelUniformCache {
  NativeModelUniformStamp stamp_{};
  NativeModelUniformContract contract_{};
  bool compiled_ = false;
  bool valid_ = false;
  VertexTransformState vertex_{};
  DrawUniforms uniforms_{};

 public:
  struct Update { bool vertex = false, fragment = false; };

  template<class Build>
  Update prepare(const NativeModelUniformStamp& stamp, bool force, Build&& build) noexcept {
    const bool changedMaterial = !valid_ || stamp.recipe != stamp_.recipe ||
        stamp.stateIdentity != stamp_.stateIdentity || stamp.vertexProgram != stamp_.vertexProgram;
    Update update{force || changedMaterial || stamp.vertex != stamp_.vertex,
                  force || changedMaterial || stamp.fragment != stamp_.fragment};
    if (force || changedMaterial) {
      vertex_ = {};
      uniforms_ = {};
    }
    if (update.vertex || update.fragment)
      build(vertex_, uniforms_, update.vertex, update.fragment);
    stamp_ = stamp;
    valid_ = true;
    compiled_ = false;
    return update;
  }

  template<class Build>
  Update prepare(const NativeModelUniformStamp& stamp, const NativeModelUniformContract& contract,
                 bool force, Build&& build) noexcept {
    const bool reset = force || !valid_ || !compiled_ || stamp.stateIdentity != stamp_.stateIdentity;
    Update update{reset || !contract.same_vertex(contract_) || stamp.vertex != stamp_.vertex,
                  reset || contract.fragmentFlags != contract_.fragmentFlags || stamp.fragment != stamp_.fragment};
    // Clear only the changed domain. A matrix selector change can leave an old
    // palette slot otherwise, even if the new program no longer reads it.
    if (update.vertex) {
      vertex_ = {};
      uniforms_.mvp = vertex_.projection;
      uniforms_.channelAmbient = {};
      uniforms_.channelMaterial = {};
      uniforms_.lightEnabled = {};
      uniforms_.lights = {};
    }
    if (update.fragment) {
      const DrawUniforms defaults{};
      uniforms_.kcolor = defaults.kcolor; uniforms_.tevreg = defaults.tevreg;
      uniforms_.fogColor = defaults.fogColor; uniforms_.fogParams = defaults.fogParams;
      uniforms_.fogRangeK = {}; uniforms_.renderViewportWidth = defaults.renderViewportWidth;
      uniforms_.indirectMatrices = {}; uniforms_.texcoordScale = {}; uniforms_.textureSizeBias = {};
    }
    if (update.vertex || update.fragment)
      build(vertex_, uniforms_, update.vertex, update.fragment);
    stamp_ = stamp;
    contract_ = contract;
    valid_ = compiled_ = true;
    return update;
  }

  void invalidate() noexcept { valid_ = false; }
  const VertexTransformState& vertex() const noexcept { return vertex_; }
  const DrawUniforms& uniforms() const noexcept { return uniforms_; }
};

} // namespace aurora::vita::gfx
