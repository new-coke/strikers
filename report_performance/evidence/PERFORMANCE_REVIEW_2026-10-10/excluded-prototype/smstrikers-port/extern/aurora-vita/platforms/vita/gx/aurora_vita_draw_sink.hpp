#pragma once

#include "aurora_gx_bridge.hpp"
#include "../gfx/vita_command_stream.hpp"
#include "../gfx/vita_streaming_arena.hpp"
#include "../gfx/vita_telemetry.hpp"
#include "../gfx/vita_memory_budget.hpp"
#include "../gfx/vita_static_geometry.hpp"
#include "../gfx/vita_fixed_uniform_pool.hpp"
#include "../gfx/vita_fixed_vertex.hpp"
#include "../gfx/vita_native_model_uniform_cache.hpp"
#include "../gfx/vita_vertex_reuse_probe.hpp"
#include "../integration/vita_feature_coverage.hpp"
#include "../integration/vita_frame_trace.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include "../../../lib/gx/native_model_cache.hpp"
#include "../../../lib/gx/native_model_sources.hpp"
#include "../gfx/vita_hash_map.hpp"
#if defined(AURORA_VITA_UPSTREAM) && !defined(AURORA_VITA_UPSTREAM_STUB)
#include "../../../lib/gx/pipeline.hpp"
#endif

namespace aurora::vita::gxbridge {

enum class SubmitWarning : uint8_t {
  None = 0,
  MissingTextureFallback = 1 << 0,
  DynamicCopyFallback = 1 << 1,
  LogicOpFallback = 1 << 2,
  OrigLodApproximation = 1 << 3,
  ZCompLocApproximation = 1 << 4,
};
inline SubmitWarning operator|(SubmitWarning a, SubmitWarning b) noexcept {
  return static_cast<SubmitWarning>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline SubmitWarning& operator|=(SubmitWarning& a, SubmitWarning b) noexcept { a = a | b; return a; }
inline bool has_warning(SubmitWarning set, SubmitWarning bit) noexcept {
  return (static_cast<uint8_t>(set) & static_cast<uint8_t>(bit)) != 0;
}

struct SubmitResult {
  bool ok = false;
  gfx::PrepareDrawError drawError = gfx::PrepareDrawError::None;
  SubmitWarning warnings = SubmitWarning::None;
  uint8_t fallbackTextureMask = 0;
};

struct DrawSinkConfig {
  gfx::StreamingArenaConfig streaming{};
  size_t commandReserve = 2048;
  gfx::Telemetry* telemetry = nullptr;
  integration::FeatureCoverage* coverage = nullptr;
  integration::FrameTrace* trace = nullptr;
  // Human-readable geometry dumps intentionally walk prepared vertices and
  // write several stderr lines per sampled draw. Keep that independent from
  // structured telemetry/coverage so profiling does not perturb the hot path.
  bool verboseGeometryDiagnostics = false;
  bool strictUnsupported = false;
  size_t staticGeometryBudget = 0; // Zero keeps the established CPU vertex path.
  bool staticGeometryBudgetPreflight = false;
  bool nativeGpuStatic = false;
  bool vertexReuseProbe = false;
  // Zero uses the original deque; retained CPU snapshots are capped at 1 MiB.
  size_t fixedUniformPoolBytes = 0;
  uint32_t staticGeometryMinVertices = 48;
  // Restrict the persistent fixed-vertex cache to sources whose lifetime is
  // explicitly tracked by the GX display-list path. Dynamic GXBegin/GXEnd
  // streams otherwise pay a full content hash before falling back to CPU work.
  bool staticGeometryStableOnly = false;
  bool allowLitFixedVertexGpu = false;
  bool localDrawBatching = false;
  bool immediateDrawView = false;
  bool allowStreamedFixedVertexGpu = false;
  bool allowDynamicTexMatrixGpu = false;
  bool allowBumpFixedVertexGpu = false;
  bool allowPrimitiveExpansionGpu = false;
  uint32_t diagnosticDrawLimit = 0;
};

enum DrawSinkRuntimeFeature : uint32_t {
  RuntimeStaticGeometry      = 1u << 0,
  RuntimeStreamedFixedVertex = 1u << 1,
  RuntimeLitFixedVertex      = 1u << 2,
  RuntimeDynamicTexMatrix    = 1u << 3,
  RuntimeBumpFixedVertex     = 1u << 4,
  RuntimePrimitiveExpansion  = 1u << 5,
  RuntimeStaticStableOnly    = 1u << 6,
  RuntimeLocalDrawBatching   = 1u << 7,
};

class DrawSink {
public:
  DrawSink() = default;
  ~DrawSink();
  DrawSink(const DrawSink&) = delete;
  DrawSink& operator=(const DrawSink&) = delete;

  bool initialize(gfx::Renderer& renderer, const DrawSinkConfig& config = {}) noexcept;
  void shutdown() noexcept;
  void begin_frame(uint64_t frame) noexcept;
  void flush() noexcept;
  void reset_commands() noexcept { clear_native_replay();stream_.reset(); fixedVertexUniforms_.reset(); lastFixedUniforms_ = nullptr; reset_pipeline_run_cache(); }
  void invalidate_texture_resolve_cache() noexcept {
#if defined(AURORA_VITA_UPSTREAM)
    resolvedTextureBindingsValid_ = false;
#endif
  }

#if defined(AURORA_VITA_UPSTREAM)
  SubmitResult submit(uint8_t primitive, uint8_t fmt, const uint8_t* rawVertices,
                      size_t rawBytes, uint32_t vertexCount,
                      const uint16_t* rawIndices = nullptr, uint32_t indexCount = 0,
                      const uint8_t* stableSource = nullptr) noexcept;
  // GXCopyTex integration. Call after the source EFB has been rendered and before
  // a texture object backed by dest is sampled. The backend owns the copied image;
  // native GXM currently uses a synchronized CPU conversion for color copies.
  bool copy_tex(const void* dest, bool clear) noexcept;
  void evict_copy_tex(const void* dest) noexcept;
  void clear_copy_textures() noexcept;
#endif

  gfx::CommandStream& stream() noexcept { return stream_; }
  const gfx::CommandStream& stream() const noexcept { return stream_; }
  uint64_t submitted_draws() const noexcept { return submittedDraws_; }
#if defined(AURORA_VITA_UPSTREAM)
  // A6: execute a proven immutable AVNR v2 draw again without GX BP/CP/XF
  // translation. Owner-thread only; returns false for exact-slot GX fallback.
  bool replay_last_native_draw(const uint8_t* pinnedDisplayList,uint64_t immutableIdentity) noexcept;
  bool submit_native_model_recipe(uint64_t identity,uint64_t pinnedIdentity) noexcept;
  void invalidate_native_model_sources(const aurora::gx::fifo::NativeModelSourceSpan* sources,
                                      size_t count) noexcept;
#endif
  gfx::FixedUniformPool::Stats fixed_uniform_pool_stats() const noexcept { return fixedVertexUniforms_.stats(); }
  gfx::VertexReuseProbe::Stats vertex_reuse_stats() const noexcept {
    return vertexReuseProbe_?vertexReuseProbe_->stats():gfx::VertexReuseProbe::Stats{};
  }
  uint64_t geometry_preflight_rejects() const noexcept {
    return staticGeometry_?staticGeometry_->budget_preflight_rejects():0;
  }
  bool strict_failed() const noexcept { return strictFailed_; }
  gfx::MemoryBudgetSnapshot memory_budget() const noexcept;
  uint32_t runtime_feature_flags() const noexcept;
  uint32_t runtime_feature_capabilities() const noexcept;
  void set_runtime_feature_flags(uint32_t flags) noexcept;

private:
#if defined(AURORA_VITA_UPSTREAM) && !defined(AURORA_VITA_UPSTREAM_STUB)
  struct NativeModelPrepared {
    aurora::gx::PipelineConfig guard{};
    gfx::PipelineDesc pipeline{};
    gfx::NativeModelUniformContract uniformContract{};
    gfx::DrawPacket packet{};
    const gfx::StaticGeometryCache::Entry* geometry=nullptr;
    uint64_t geometryKey=0,pinnedIdentity=0;
    uint32_t features=0,disableMask=0;
    uint8_t primitive=0,fmt=0,textureMask=0;
  };
  gfx::FlatHashMap<uint64_t,NativeModelPrepared> nativeModels_{};
  aurora::gx::fifo::NativeModelCache<NativeModelPrepared> nativeModelCache_{};
  gfx::NativeModelUniformCache nativeModelUniforms_{};
#endif
  std::array<gfx::TextureBinding,gfx::MaxTextures> resolve_textures(
      uint8_t textureMask,bool broadDirty,uint8_t& nativeWrapMask,SubmitResult& result) noexcept;
  gfx::Handle white_texture() noexcept;
  void reset_pipeline_run_cache() noexcept {
#if defined(AURORA_VITA_UPSTREAM)
    queuedPipelineValid_ = false;
#endif
  }
  gfx::Renderer* renderer_ = nullptr;
  std::unique_ptr<gfx::StreamingArena> arena_{};
  gfx::CommandStream stream_{};
  gfx::PreparedDraw preparedScratch_{};
  std::unique_ptr<gfx::VertexReuseProbe> vertexReuseProbe_{};
  std::unique_ptr<gfx::StaticGeometryCache> staticGeometry_{};
  gfx::FixedUniformPool fixedVertexUniforms_{};
  gfx::FixedVertexUniformBuilder fixedUniformBuilder_{};
  gfx::FixedVertexUniforms& build_fixed_uniforms(const gfx::PipelineDesc& pipeline,
      const gfx::VertexTransformState& state,bool distinct=false) noexcept;
  // Snapshot reuse across consecutive GPU-geometry draws (see submit()).
  gfx::FixedVertexUniforms* lastFixedUniforms_ = nullptr;
  uint64_t vertexStateVersion_ = 0;
  uint64_t fragmentUniformVersion_ = 0;

  gfx::PipelineDesc translatedGpuPipeline_{};
  gfx::FlatHashMap<uint64_t,uint64_t> fixedPipelineKeys_{};
  gfx::VertexTransformState translatedVertexState_{};
  gfx::DrawUniforms translatedUniforms_{};
  bool translatedVertexStateValid_ = false;
  bool translatedVertexStateLightweight_ = false;
  gfx::Handle whiteTexture_ = gfx::InvalidHandle;
#if defined(AURORA_VITA_UPSTREAM)
  uint64_t translatedVertexRevision_ = 0, translatedFragmentRevision_ = 0;
  uint64_t resolvedTextureRevision_ = 0, consumedStateSerial_ = 0;
  uint32_t resolvedTextureStateIdentity_ = 0;
  uint32_t translatedStateGeneration_ = 0;
  uint32_t translatedLayoutGeneration_ = 0;
  uint32_t translatedVertexProgramStateGeneration_ = 0;
  uint8_t translatedPrimitive_ = 0;
  uint8_t translatedFmt_ = 0;
  gfx::PipelineDesc translatedPipeline_{};
  gfx::DrawRecipe cpuRecipe_{},gpuRecipe_{};
  uint32_t gpuRecipeFlags_=0;
  bool gpuRecipeValid_=false;
  gfx::VertexDecodeLayout translatedLayout_{};
  uint64_t translatedPipelineKey_ = 0;
  uint64_t translatedBaseKey_ = 0;
  uint64_t translatedFixedVertexProgramKey_ = 0;
  uint8_t translatedTextureMask_ = 0;
  bool translatedUsesOrigLod_ = false;
  bool translatedHasIndirect_ = false;
  bool translatedLit_ = false;
  bool translatedStateValid_ = false;
  std::array<gfx::TextureBinding,gfx::MaxTextures> resolvedTextureBindings_{};
  uint64_t resolvedTextureBindingVersion_ = 1;
  uint8_t resolvedTextureMask_ = 0;
  uint8_t resolvedVolatileTextureMask_ = 0;
  uint8_t resolvedFallbackTextureMask_ = 0;
  uint8_t resolvedNativeWrapMask_ = 0;
  SubmitWarning resolvedTextureWarnings_ = SubmitWarning::None;
  bool resolvedTextureBindingsValid_ = false;
  uint64_t queuedTranslatedPipelineKey_ = 0;
  uint64_t queuedResolvedPipelineKey_ = 0;
  gfx::Primitive queuedPrimitive_ = gfx::Primitive::Triangles;
  bool queuedPositionIsClipSpace_ = false;
  bool queuedPipelineValid_ = false;
#endif
  uint64_t submittedDraws_ = 0;
  // Invalidated on every new draw/frame/flush. The identity and cache key are
  // checked before dereferencing the entry; no GPU handle is retained across
  // frames, buffer retirement, views or renderer resets.
  uint64_t lastNativeReplayKey_=0,lastNativeReplayIdentity_=0;
  const uint8_t* lastNativeReplaySource_=nullptr;
  uint32_t lastNativeReplayBytes_=0;
  const gfx::StaticGeometryCache::Entry* lastNativeReplayEntry_=nullptr;
  integration::DrawTraceRecord lastNativeReplayTrace_{};
  void clear_native_replay() noexcept {
    if(!lastNativeReplayEntry_)return;
    lastNativeReplayKey_=lastNativeReplayIdentity_=0;
    lastNativeReplaySource_=nullptr;lastNativeReplayBytes_=0;lastNativeReplayEntry_=nullptr;
    lastNativeReplayTrace_={};
  }
  gfx::Telemetry* telemetry_ = nullptr;
  integration::FeatureCoverage* coverage_ = nullptr;
  integration::FrameTrace* trace_ = nullptr;
  bool verboseGeometryDiagnostics_ = false;
  bool strictUnsupported_ = false;
  bool strictFailed_ = false;
  bool allowLitFixedVertexGpu_ = false;
  bool localDrawBatching_ = false;
  bool immediateDrawView_ = false;
  gfx::GpuUniformSnapshot immediateGpuUniforms_{};
  bool allowStreamedFixedVertexGpu_ = false;
  bool allowDynamicTexMatrixGpu_ = false;
  bool allowBumpFixedVertexGpu_ = false;
  bool allowPrimitiveExpansionGpu_ = false;
  bool staticGeometryRuntimeEnabled_ = false;
  bool staticGeometryStableOnly_ = false;
  uint32_t staticGeometryMinVertices_ = 48;
  uint32_t diagnosticDrawLimit_ = 0;
  uint32_t frameDrawIndex_ = 0;
  struct CopyTextureEntry {
    gfx::Handle handle=gfx::InvalidHandle;
    uint32_t width=0,height=0;
    uint32_t revision=0;
    bool logicalFlipX=false,logicalFlipY=false,forceOpaque=false;
    gfx::EfbCopyFormat sampleFormat=gfx::EfbCopyFormat::Passthrough;
  };
  gfx::FlatHashMap<uintptr_t,CopyTextureEntry> copyTextures_{};
  bool initialized_ = false;
};

} // namespace aurora::vita::gxbridge
