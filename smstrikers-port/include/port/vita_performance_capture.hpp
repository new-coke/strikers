#pragma once
#include "aurora_vita_backend.hpp"
#include "port/config.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace port {
inline void capture_completed_performance(bool livePlay) {
  static const char* path=std::getenv("STRIKERS_PERFORMANCE_CAPTURE");
  if(!path||!*path||!PortDiagnosticsEnabled()||!livePlay)return;
  static std::vector<aurora::vita::PerformanceSnapshot> rows;
  static bool written=false;
  static size_t seen=0;
  if(written)return;
  const auto number=[](const char* name,unsigned fallback,unsigned maximum) {
    const char* text=std::getenv(name);if(!text||!*text)return fallback;
    char* end=nullptr;unsigned long value=std::strtoul(text,&end,10);
    return end!=text&&!*end&&value<=maximum?unsigned(value):fallback;
  };
  static const size_t limit=number("STRIKERS_PERFORMANCE_CAPTURE_FRAMES",600,3600);
  static const size_t warmup=number("STRIKERS_PERFORMANCE_CAPTURE_SKIP",600,36000);
  if(!limit){written=true;return;}
  if(rows.empty())rows.reserve(limit);
  const auto sample=aurora::vita::completed_performance_snapshot();
  if(!sample.completedFrame)return;
  if(seen++<warmup)return;
  rows.push_back(sample);
  if(rows.size()<limit)return;
  written=true;
  FILE* file=std::fopen(path,"w");if(!file)return;
  // Vita's minimal printf does not support the size_t length modifier.
  std::fprintf(file,"# strikers-consumer-capture-v1 diagnostics=1 warmup=%u samples=%u; nested phases and overlapping threads must not be summed\n",unsigned(warmup),unsigned(rows.size()));
  std::fprintf(file,"frame,frame_us,gx_total_us,xf_pos_inspected,xf_pos_unchanged,xf_pos_changed,producer_wait_us,consumer_wait_us,draws,vertices,gpu_vertices,texture_uploads,texture_upload_bytes,geometry_hits,geometry_misses,geometry_bytes,prepared_hits,prepared_misses,prepared_rejected,pool_busy_fallbacks,core3_total_pct_x100,finish_calls,finish_wait_us,display_queue_us,native_texture_hits,native_geometry_hits,native_rejected,scene0_wait_us,scene1_wait_us,scene2_wait_us,scene3_wait_us,core3_telemetry_valid,core3_chunks,core3_denied,core3_telemetry_failures,core3_overruns,native_misses,vertex_delta_copies,vertex_delta_saved_calls,vertex_delta_fallbacks,vertex_delta_copied_bytes,vertex_delta_saved_bytes,native_gpu_geometry_hits,native_gpu_geometry_attempts,xf_tex_inspected,xf_tex_unchanged,xf_tex_changed,xf_normal_inspected,xf_normal_unchanged,xf_normal_changed,xf_post_inspected,xf_post_unchanged,xf_post_changed,tev_decoded_inspected,tev_decoded_skipped,tev_decoded_changed,native_replay_attempts,native_replay_hits,native_replay_fallbacks");
  using namespace aurora::vita::gfx;
  std::fprintf(file,",native_model_attempts,native_model_draws,native_model_fallbacks,native_model_compiled");
  std::fprintf(file,",native_pn_palettes_queued,native_pn_palettes_applied,native_pn_matrices_applied");
  using namespace aurora::gx::fifo;
  std::fprintf(file,",native_census_callbacks,native_census_draw_callbacks,native_census_filter_passed,native_census_transport_ready,native_census_eligible,native_census_classifier_mismatches");
  for(const char* name:ModelRejectNames)std::fprintf(file,",native_census_reject_%s",name);
  for(size_t i=0;i<ModelViewCount;++i)std::fprintf(file,",native_census_view_%u",unsigned(i));
  for(size_t i=0;i<ModelProgramCount;++i)std::fprintf(file,",native_census_program_%u",unsigned(i));
  for(const char* name:ModelStageNames)std::fprintf(file,",native_census_stage_%s",name);
  for(const char* name:ModelCacheEventNames)std::fprintf(file,",native_cache_%s",name);
  for(const char* name:ModelCachePeakNames)std::fprintf(file,",native_cache_peak_%s",name);
  std::fprintf(file,",reuse_sampled_frames,reuse_draws,reuse_vertices,reuse_full_hits,reuse_full_hit_vertices,reuse_pn_draws,reuse_pn_vertices,reuse_pn_hits,reuse_pn_hit_vertices,reuse_table_overflows,reuse_skipped_draws,reuse_pn_cross_draw_vertices,reuse_pn_vertex_table_overflows");
  for(size_t i=0;i<size_t(TelemetryPhase::Count);++i)std::fprintf(file,",%s_us",telemetry_phase_name(TelemetryPhase(i)));
  std::fprintf(file,"\n");
  for(const auto& s:rows) {
    const auto& c=s.frontend.counters;uint64_t calls=0,wait=0;
    for(size_t i=0;i<FinishReasonCount;++i){calls+=s.nativeFinishReasonCalls[i];wait+=s.nativeFinishReasonWaitUs[i];}
    const uint64_t values[]={s.frameIndex,s.frameUs,s.gxProcessTotalUs,s.xfPositionWritesInspected,s.xfPositionWritesUnchanged,s.xfPositionWritesChanged,s.producerWaitUs,s.consumerWaitUs,c.draws,c.vertices,c.gpuVertices,c.textureUploads,c.textureUploadBytes,s.staticGeometryHits,s.staticGeometryMisses,s.staticGeometryBytes,s.preparedListHits,s.preparedListMisses,s.preparedListRejected,s.poolBusyFallbacks,s.core3LastTotalPercentX100,calls,wait,s.displayQueueLastUs,s.nativeAssets.textureHits,s.nativeAssets.geometryHits,s.nativeAssets.rejected,s.diagSceneGpuUs[0],s.diagSceneGpuUs[1],s.diagSceneGpuUs[2],s.diagSceneGpuUs[3],s.core3TelemetryValid,s.core3Chunks,s.core3Denied,s.core3TelemetryFailures,s.core3Overruns,s.nativeAssets.misses,s.nativeVertexDeltaCopies,s.nativeVertexDeltaSavedCalls,s.nativeVertexDeltaFallbacks,s.nativeVertexDeltaCopiedBytes,s.nativeVertexDeltaSavedBytes,s.nativeAssets.gpuGeometryHits,s.nativeAssets.gpuGeometryAttempts,s.xfTexWritesInspected,s.xfTexWritesUnchanged,s.xfTexWritesChanged,s.xfNormalWritesInspected,s.xfNormalWritesUnchanged,s.xfNormalWritesChanged,s.xfPostWritesInspected,s.xfPostWritesUnchanged,s.xfPostWritesChanged,s.tevDecodedWritesInspected,s.tevDecodedWritesSkipped,s.tevDecodedWritesChanged,s.nativeReplayAttempts,s.nativeReplayHits,s.nativeReplayFallbacks};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);++i)std::fprintf(file,"%s%llu",i?",":"",(unsigned long long)values[i]);
    const uint64_t models[]={s.nativeModelAttempts,s.nativeModelDraws,s.nativeModelFallbacks,s.nativeModelCompiled};
    for(uint64_t value:models)std::fprintf(file,",%llu",(unsigned long long)value);
    const uint64_t pn[]={s.nativePnPalettesQueued,s.nativePnPalettesApplied,s.nativePnMatricesApplied};
    for(uint64_t value:pn)std::fprintf(file,",%llu",(unsigned long long)value);
    const auto& census=s.nativeModelCensus;
    const uint64_t censusTotals[]={census.callbacks,census.drawCallbacks,census.filterPassed,
        census.transportReady,census.eligible,census.mismatches};
    for(uint64_t value:censusTotals)std::fprintf(file,",%llu",(unsigned long long)value);
    for(uint64_t value:census.rejects)std::fprintf(file,",%llu",(unsigned long long)value);
    for(uint64_t value:census.views)std::fprintf(file,",%llu",(unsigned long long)value);
    for(uint64_t value:census.programs)std::fprintf(file,",%llu",(unsigned long long)value);
    for(uint64_t value:census.stages)std::fprintf(file,",%llu",(unsigned long long)value);
    for(uint64_t value:s.nativeModelCache.events)std::fprintf(file,",%llu",(unsigned long long)value);
    for(uint64_t value:s.nativeModelCache.peaks)std::fprintf(file,",%llu",(unsigned long long)value);
    const auto& r=s.vertexReuse;
    const uint64_t reuseValues[]={r.sampledFrames,r.draws,r.vertices,r.fullHits,r.fullHitVertices,
        r.positionNormalDraws,r.positionNormalVertices,r.positionNormalHits,r.positionNormalHitVertices,
        r.tableOverflows,r.skippedDraws,r.positionNormalCrossDrawVertices,r.positionNormalVertexTableOverflows};
    for(uint64_t value:reuseValues)std::fprintf(file,",%llu",(unsigned long long)value);
    for(uint64_t us:s.frontend.phaseUs)std::fprintf(file,",%llu",(unsigned long long)us);
    std::fprintf(file,"\n");
  }
  const auto evidence=native_model_census_evidence();
  if(evidence.counts.callbacks){
    std::fprintf(file,"# native_model_census_v1 examples_scope=process_start; rejection reasons overlap; per-row counters are coherent cumulative producer observations\n");
    const auto example=[&](const char* kind,unsigned index,const ModelCensusExample& e){
      if(!e.valid)return;
      const auto& f=e.facts;
      std::fprintf(file,"# native_model_example kind=%s index=%u frame=%llu view=%u previous_view=%u flags=%u rejects=%u transport_rejects=%u program=%u program_kind=%u texconfig=%u texture_mask=%u stream_mask=%u modifier_mask=%u vertices=%u streams=%u primitive=%u list_bytes=%u\n",
          kind,index,(unsigned long long)f.frame,f.view,f.previousView,f.flags,e.rejects,f.transportRejects,
          f.program,f.programKind,f.texconfig,f.textureMask,f.streamMask,f.modifierMask,
          f.vertices,f.streams,f.primitive,f.listBytes);
    };
    for(size_t i=0;i<evidence.reasons.size();++i)example("reason",unsigned(i),evidence.reasons[i]);
    for(size_t i=0;i<evidence.views.size();++i)example("view",unsigned(i),evidence.views[i]);
  }
  std::fclose(file);rows.clear();rows.shrink_to_fit();
}
} // namespace port
