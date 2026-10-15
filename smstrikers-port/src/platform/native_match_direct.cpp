#include "port/native_match_direct.hpp"

#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
#include "port/native_match_gpu_assets.hpp"
#include "port/native_match_uv.hpp"
#include "port/native_match_source_bindings.hpp"
#include "port/native_model_sources.hpp"
#include "port/native_match_transform.hpp"
#include "port/native_gxm_renderer.hpp"
#include "port/native_gxm_scene_pack.hpp"
#include "Game/GL/ShaderSkinMesh.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glState.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/gl/glView.h"
#include "NL/nlMath.h"
#include "port/config.h"
#include "aurora_vita_backend.hpp"
#include "gfx/vita_renderer.hpp"
#include "gfx/vita_memory_revision.hpp"
#include "NL/glx/glxMemory.h"
#include "../../extern/aurora-vita/lib/gx/fifo.hpp"
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
#include "gx/aurora_vita_draw_sink.hpp"
#endif

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include <vector>
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
#include <psp2/kernel/threadmgr.h>
#endif

extern "C" int port_region_owns(const void*);

namespace port::directmatch {
namespace {
using aurora::vita::gfx::Handle;
namespace gfx = aurora::vita::gfx;

constexpr const char* kManifestPath = "ux0:data/strikersVita/gxm_native_match_meshes.bin";
constexpr const char* kUvPath = "ux0:data/strikersVita/gxm_native_match_uvs.bin";
constexpr const char* kReportPath = "ux0:data/strikersVita/gxm_native_match_status.csv";
struct DrawWork {
    uint32_t index=0;
    uint32_t frame=0;
    uint32_t packedRaster=0;
    uint64_t revision=0;
    std::array<float,16> mvp{};
};
static_assert(sizeof(DrawWork)<=128);
struct DynamicDrawWork {
    nativegxm::ModelVertices model{};
    uint32_t packedRaster=0;
    std::array<float,16> mvp{};
};
struct Counters {
    uint64_t callbacks=0, nativeDraws=0;
    uint64_t unsupportedView=0, unsupportedMaterial=0,unsupportedGeometry=0;
    uint64_t missingNativeAsset=0, uploadErrors=0, queueErrors=0;
    uint64_t unsupportedRaster=0;
    uint64_t skinnedRejected=0, userEffectRejected=0;
    uint64_t colorPreviewCandidates=0, colorPreviewDraws=0;
    uint64_t previewTextured=0, previewLit=0, plainUnlitDraws=0;
    uint64_t invalidMatrixIdentity=0;
    uint64_t sourceRegistrations=0, sourceReleases=0, sourceHits=0;
    uint64_t sourceUnbound=0, sourceDirtyRehash=0, sourceInvalid=0;
    uint64_t sourceMisses=0, sourceUnsafe=0;
    uint64_t invalidMvp=0;
    uint64_t dynamicQueued=0,dynamicSourceMissing=0,dynamicInvalid=0;
    std::atomic<uint64_t> presentErrors{0};
};
bool sEnabled=false,sLoaded=false,sGpuReady=false,sGpuFailed=false;
bool sFrameStarted=false;
uint32_t sLastReportFrame=~0u;
uint64_t sNextDrawRevision=1;
uint64_t sPipeline=0;
Handle sVertexBuffer=0,sIndexBuffer=0;
struct CachedRasterPipeline {uint32_t raster=0; uint64_t pipeline=0;};
std::vector<CachedRasterPipeline> sRasterPipelines{};
constexpr unsigned kMaxRasterPipelines=24;
std::atomic<uint64_t> sNativeDrawsAccepted{0},sNativeDrawsRejected{0};
std::atomic<const char*> sFailure{"none"};
PreparedGpuAssets sAssets{};
uint64_t sMeshPayloadHash=0;
const char* sUvStatus="not_loaded";
NativeSourceBindings sBindings{};
ModelSourceRegistry sModelSources{};
Counters sCounters{};
uint32_t sInvalidMvpFrame=~0u, sInvalidMvpView=~0u;
std::array<uint32_t,16> sInvalidMvpBits{};
enum CoverageReason { Candidates, Queued, ViewRejected, ProgramRejected, SkinRejected,
    UserDataRejected, RasterRejected, GeometryRejected, Unbound, SourceMissing,
    InvalidSource, InvalidTransform, CullAll, NoRgb, CoverageReasons };
struct ViewCoverage {
    std::array<uint64_t,CoverageReasons> count{};
    uint32_t rejectedRaster=0, rejectedProgram=0, rejectedUserMask=0;
    SourceIdentity firstMissingSource{};
};
bool sVisualDebug=false;
bool sCompactVertices=false;
std::array<ViewCoverage,GLV_Num> sViewCoverage{};
void coverage(eGLView view,CoverageReason reason,const glModelPacket* packet=nullptr) noexcept {
    if (!sVisualDebug || static_cast<unsigned>(view)>=GLV_Num) return;
    auto& c=sViewCoverage[view];
    if (++c.count[reason]!=1 || !packet) return;
    if (reason==RasterRejected) c.rejectedRaster=packet->state.raster;
    if (reason==ProgramRejected) c.rejectedProgram=packet->state.program;
    if (reason==UserDataRejected) {
        for (unsigned k=0;k<GLUD_Num;++k)
            if (glUserHasType(static_cast<eGLUserData>(k),packet)) c.rejectedUserMask|=1u<<k;
    }
}
void save_visual_coverage(uint32_t frame) noexcept {
    if (!sVisualDebug) return;
    auto* f=std::fopen("ux0:data/strikersVita/gxm_native_visual_coverage.csv","wb");
    if (!f) return;
    std::fprintf(f,"# native_visual_coverage frame=%u projection=glx_six_coeff fps_measurement=0\n",frame);
    std::fprintf(f,"view,candidates,queued,view_rejected,program_rejected,skin_rejected,userdata_rejected,raster_rejected,geometry_rejected,unbound,source_missing,invalid_source,invalid_transform,cull_all,no_rgb,first_rejected_raster,first_rejected_program,first_rejected_userdata_mask,missing_draw_vertices,missing_primitive,missing_raw_bytes,missing_position_bytes,missing_color_bytes,missing_raw_hash,missing_position_hash,missing_color_hash\n");
    for (unsigned view=0;view<GLV_Num;++view) {
        const auto& c=sViewCoverage[view];
        if (!c.count[Candidates]) continue;
        std::fprintf(f,"%u",view);
        for (const auto count : c.count) std::fprintf(f,",%llu",static_cast<unsigned long long>(count));
        const auto& s=c.firstMissingSource;
        std::fprintf(f,",%08x,%08x,%08x,%u,%u,%u,%u,%u,%016llx,%016llx,%016llx\n",
            c.rejectedRaster,c.rejectedProgram,c.rejectedUserMask,
            s.drawVertices,s.primitive,s.rawBytes,s.positionBytes,s.colorBytes,
            static_cast<unsigned long long>(s.rawHash),static_cast<unsigned long long>(s.positionHash),
            static_cast<unsigned long long>(s.colorHash));
    }
    std::fclose(f);
}
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
nativegxm::Renderer sNativeRenderer;
bool sGraphicsDetached=false, sBeginOk=false, sEndOk=false;
bool sSlotFence=false, sTiming=false;
uint64_t sDynamicInputVertices=0,sDynamicOutputVertices=0;
uint64_t sTimingFrames=0,sPrepareUs=0,sPresentWaitUs=0;
nativegxm::Renderer::FrameTiming sTimingTotal{};
#endif

bool load_pack() noexcept {
    if(sLoaded)return sAssets.mesh_count()!=0;
    sLoaded=true;
    std::FILE* f=std::fopen(kManifestPath,"rb");
    if(!f){sFailure.store("native_manifest_missing",std::memory_order_relaxed);return false;}
    if(std::fseek(f,0,SEEK_END)){std::fclose(f);sFailure.store("native_manifest_seek",std::memory_order_relaxed);return false;}
    const long length=std::ftell(f);
    if(length<static_cast<long>(kHeaderSize)||
       length>static_cast<long>(kMaxManifestBytes)||std::fseek(f,0,SEEK_SET)){
        std::fclose(f);sFailure.store("native_manifest_size",std::memory_order_relaxed);return false;
    }
    std::vector<uint8_t> data(static_cast<size_t>(length));
    const bool read=std::fread(data.data(),1,data.size(),f)==data.size()&&!std::ferror(f);
    const bool closed=std::fclose(f)==0;
    Manifest source{};
    if(!read||!closed||!parse(data,source)){
        sFailure.store("native_manifest_corrupt",std::memory_order_relaxed);return false;
    }
    sMeshPayloadHash=fnv64(data.data()+kHeaderSize,data.size()-kHeaderSize);
    // The parsed meshes now own their vertices and indices. Drop the original
    // file bytes before creating the two packed GPU upload buffers: with the
    // 1800-mesh pack this avoids a redundant ~6 MiB peak CPU allocation.
    std::vector<uint8_t>().swap(data);
    if(!sAssets.prepare(std::move(source))){
        sFailure.store("native_manifest_prepare_failed",std::memory_order_relaxed);
        return false;
    }
    return true;
}
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
bool load_uv_sidecar(UvAssets& result) {
    result={};
    std::FILE* f=std::fopen(kUvPath,"rb");
    if(!f){sUvStatus="missing_optional";return true;}
    if(std::fseek(f,0,SEEK_END)) {std::fclose(f);sUvStatus="seek_failed";return false;}
    const long length=std::ftell(f);
    if(length<32 || length>8*1024*1024 || std::fseek(f,0,SEEK_SET)){
        std::fclose(f);sUvStatus="invalid_size";return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    const bool read=std::fread(bytes.data(),1,bytes.size(),f)==bytes.size()&&!std::ferror(f);
    const bool closed=std::fclose(f)==0;
    if(!read||!closed||!parse_uv_assets(bytes,sMeshPayloadHash,sAssets.meshes(),result)){
        sUvStatus="identity_mismatch";return false;
    }
    // Release the serialized input before the GPU owner allocates UV storage.
    std::vector<uint8_t>().swap(bytes);
    sUvStatus="verified";
    return true;
}
#endif
void upload_on_owner(void*) {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    // GXInit/bootstrap is complete and the FIFO has drained. Release the GX
    // sink BEFORE the old renderer's pools, then terminate its GXM instance.
    // There is exactly one GXM owner; failure never reactivates the GX path.
    aurora::vita::draw_sink().shutdown();
    aurora::vita::renderer().shutdown();
    sGraphicsDetached=true;
    if (!sNativeRenderer.initialize(sSlotFence,sTiming)) {
        sGpuFailed=true;
        sFailure.store(sNativeRenderer.last_error(),std::memory_order_relaxed);
        return;
    }
    std::vector<nativegxm::Mesh> meshes;
    meshes.reserve(sAssets.mesh_count());
    for (const auto& mesh : sAssets.meshes())
        meshes.push_back({mesh.vertexOffset,mesh.vertexBytes,mesh.indexOffset,
                          mesh.indexCount,mesh.vertexCount});
    const auto& vertices=sAssets.vertices();
    const auto& indices=sAssets.indices();
    if (!sNativeRenderer.upload(vertices.data(),vertices.size(),indices.data(),
                                indices.size()*sizeof(uint16_t),meshes)) {
        sGpuFailed=true;
        sFailure.store(sNativeRenderer.last_error(),std::memory_order_relaxed);
        return;
    }
    sGpuReady=true;
#else
    auto& renderer=aurora::vita::renderer();
    sPipeline=renderer.create_pipeline(directgxm::scene_pipeline());
    if(!sPipeline){sGpuFailed=true;sFailure.store("native_pipeline_compilation",std::memory_order_relaxed);return;}

    // The backend-independent asset store already holds exactly two packed
    // CPU upload arrays and a compact identity table; GPU allocations happen
    // once, after the renderer owner thread has been initialized.
    const auto& vertices=sAssets.vertices();
    const auto& indices=sAssets.indices();
    sVertexBuffer=renderer.create_vertex_buffer(vertices.data(),vertices.size());
    if(!sVertexBuffer){sGpuFailed=true;sFailure.store("native_vertex_upload",std::memory_order_relaxed);return;}
    sIndexBuffer=renderer.create_index_buffer(indices.data(),indices.size()*2);
    if(!sIndexBuffer){sGpuFailed=true;sFailure.store("native_index_upload",std::memory_order_relaxed);return;}
    sGpuReady=true;
#endif
}
bool supported_raster(uint32_t packed,bool indexedPn=false) noexcept {
    // Every GX blend mode 0..7 has an equivalent native fixed-function mode.
    // Alpha test still needs the original texture alpha, which is unavailable.
    // Strikers' indexed-PN packets deliberately set SolidOffset=1 to mark the
    // bone-palette path; no GX polygon offset is emitted for that marker.
    const uint32_t solid=glGetRasterState(packed,GLS_SolidOffset);
    return glGetRasterState(packed,GLS_AlphaTest)==0 &&
           glGetRasterState(packed,GLS_AlphaBlend)<=7 &&
           (solid==0||(indexedPn&&solid==1)) &&
           glGetRasterState(packed,GLS_FillMode)==0;
}
#if !defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
uint64_t raster_pipeline(uint32_t packed) {
    for(const auto& entry:sRasterPipelines)
        if(entry.raster==packed)return entry.pipeline;
    if(sRasterPipelines.size()>=kMaxRasterPipelines) {
        sFailure.store("native_raster_pipeline_limit",std::memory_order_relaxed);
        return 0;
    }
    gfx::PipelineDesc d=directgxm::scene_pipeline();
    d.depthTest=glGetRasterState(packed,GLS_DepthTest)!=0;
    d.depthWrite=glGetRasterState(packed,GLS_DepthWrite)!=0;
    constexpr gfx::Compare depth[4]={gfx::Compare::Always,gfx::Compare::LessEqual,
                                      gfx::Compare::Equal,gfx::Compare::Less};
    d.depthFunc=depth[glGetRasterState(packed,GLS_DepthFunc)&3u];
    d.cull=static_cast<gfx::CullMode>(glGetRasterState(packed,GLS_Culling)&3u);
    const unsigned write=glGetRasterState(packed,GLS_ColourWrite);
    d.colorWrite=(write&1u)!=0;
    // ShadowTexture's case 3 writes alpha, but that view is rejected before
    // admission; all supported views leave alpha masked on case 3.
    d.alphaWrite=write==2;
    switch(glGetRasterState(packed,GLS_AlphaBlend)) {
    case 0: break;
    case 1:
        d.blendMode=gfx::BlendMode::Blend;
        d.srcFactor=gfx::BlendFactor::SrcAlpha;
        d.dstFactor=gfx::BlendFactor::OneMinusSrcAlpha;
        break;
    case 2:
        d.blendMode=gfx::BlendMode::Blend;
        d.srcFactor=gfx::BlendFactor::One;
        d.dstFactor=gfx::BlendFactor::One;
        break;
    case 3:
        d.blendMode=gfx::BlendMode::Blend;
        d.srcFactor=gfx::BlendFactor::SrcAlpha;
        d.dstFactor=gfx::BlendFactor::One;
        break;
    case 4:
        d.blendMode=gfx::BlendMode::Blend;
        d.srcFactor=gfx::BlendFactor::SrcColor;
        d.dstFactor=gfx::BlendFactor::Zero;
        break;
    case 5:
        d.blendMode=gfx::BlendMode::Blend;
        d.srcFactor=gfx::BlendFactor::OneMinusSrcColor;
        d.dstFactor=gfx::BlendFactor::One;
        break;
    case 6:
        d.blendMode=gfx::BlendMode::Blend;
        d.srcFactor=gfx::BlendFactor::One;
        d.dstFactor=gfx::BlendFactor::Zero;
        break;
    case 7:
        d.blendMode=gfx::BlendMode::Subtract;
        d.srcFactor=gfx::BlendFactor::SrcColor;
        d.dstFactor=gfx::BlendFactor::Zero;
        break;
    }
    const uint64_t key=aurora::vita::renderer().create_pipeline(d);
    if(!key) {sFailure.store("native_raster_pipeline_compile",std::memory_order_relaxed);return 0;}
    sRasterPipelines.push_back({packed,key});
    return key;
}
#endif
void draw_on_owner(void* context) {
    const auto& work=*static_cast<const DrawWork*>(context);
    if(!sGpuReady||work.index>=sAssets.mesh_count())return;
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    nativegxm::Raster raster{};
    raster.depthTest=glGetRasterState(work.packedRaster,GLS_DepthTest)!=0;
    raster.depthWrite=glGetRasterState(work.packedRaster,GLS_DepthWrite)!=0;
    raster.depthFunction=static_cast<uint8_t>(glGetRasterState(work.packedRaster,GLS_DepthFunc));
    raster.cull=static_cast<uint8_t>(glGetRasterState(work.packedRaster,GLS_Culling));
    raster.blend=static_cast<uint8_t>(glGetRasterState(work.packedRaster,GLS_AlphaBlend));
    const unsigned write=glGetRasterState(work.packedRaster,GLS_ColourWrite);
    raster.writeRgb=(write&1u)!=0;
    raster.writeAlpha=write==2;
    if (sNativeRenderer.draw(work.index,work.mvp,raster))
        sNativeDrawsAccepted.fetch_add(1,std::memory_order_relaxed);
    else {
        sNativeDrawsRejected.fetch_add(1,std::memory_order_relaxed);
        sFailure.store(sNativeRenderer.last_error(),std::memory_order_relaxed);
    }
#else
    const uint64_t activePipeline=raster_pipeline(work.packedRaster);
    if(!activePipeline){sNativeDrawsRejected.fetch_add(1,std::memory_order_relaxed);return;}
    const auto& mesh=sAssets.meshes()[work.index];
    gfx::DrawPacket p{};
    p.pipelineKey=activePipeline;
    p.vertices={sVertexBuffer,mesh.vertexOffset,mesh.vertexBytes};
    p.indices={sIndexBuffer,mesh.indexOffset,mesh.indexBytes};
    p.vertexCount=mesh.vertexCount;
    p.indexCount=mesh.indexCount;
    p.instanceCount=1;
    p.uniforms.mvp=work.mvp;
    p.viewport={0.f,0.f,960.f,544.f,0.f,1.f};
    p.scissor={0,0,960,544};
    // Frame+instance identity prevents the shader uniform cache from using
    // a matrix belonging to another live game instance.
    p.uniformRevision=work.revision;
    p.vertexUniformRevision=p.uniformRevision;
    p.fragmentUniformRevision=1;
    p.textureBindingRevision=1;
    auto& renderer=aurora::vita::renderer();
    renderer.draw(p);
    if(renderer.failed()) {
        sNativeDrawsRejected.fetch_add(1,std::memory_order_relaxed);
        sFailure.store("native_gxm_draw_failed",std::memory_order_relaxed);
    } else {
        sNativeDrawsAccepted.fetch_add(1,std::memory_order_relaxed);
    }
#endif
}

#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
void draw_dynamic_on_owner(void* context) {
    // FIFO copied only the owning pointer; the callback is responsible for
    // deletion on every path. GPU memory is independent of these CPU arrays.
    auto** slot=static_cast<DynamicDrawWork**>(context);
    std::unique_ptr<DynamicDrawWork> work(*slot);
    // run_async_native can execute inline and return serial=0. Clearing its
    // original pointer makes that case distinguishable from enqueue failure;
    // on a queued job this only modifies the FIFO's private inline copy.
    *slot=nullptr;
    if(!work || !sGpuReady) return;
    nativegxm::Raster raster{};
    raster.depthTest=glGetRasterState(work->packedRaster,GLS_DepthTest)!=0;
    raster.depthWrite=glGetRasterState(work->packedRaster,GLS_DepthWrite)!=0;
    raster.depthFunction=static_cast<uint8_t>(glGetRasterState(work->packedRaster,GLS_DepthFunc));
    raster.cull=static_cast<uint8_t>(glGetRasterState(work->packedRaster,GLS_Culling));
    raster.blend=static_cast<uint8_t>(glGetRasterState(work->packedRaster,GLS_AlphaBlend));
    const unsigned write=glGetRasterState(work->packedRaster,GLS_ColourWrite);
    raster.writeRgb=(write&1u)!=0;
    raster.writeAlpha=write==2;
    if(sNativeRenderer.draw_dynamic(work->model.vertices,work->model.indices,work->mvp,raster))
        sNativeDrawsAccepted.fetch_add(1,std::memory_order_relaxed);
    else
        sNativeDrawsRejected.fetch_add(1,std::memory_order_relaxed);
}
#endif

void present_on_owner(void*) {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    // This ordered callback drains prior draws for Hitz/VI. The backbuffer
    // already IS the native target; one display-queue present happens at the
    // game-frame boundary, without an EFB copy or second renderer present.
#else
    auto& renderer=aurora::vita::renderer();
    const gfx::Scissor image{0,0,static_cast<int32_t>(renderer.target_width()),
                                static_cast<int32_t>(renderer.target_height())};
    if(!renderer.display_copy(image)) {
        sCounters.presentErrors.fetch_add(1,std::memory_order_relaxed);
        sFailure.store("native_display_copy_failed",std::memory_order_relaxed);
    }
    aurora::vita::schedule_display_clear(0.f,0.f,0.f,1.f,1.f,true,true,true);
#endif
}
void release_on_owner(void*) {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    sNativeRenderer.shutdown();
#endif
    // The facade owns these GPU handles until its imminent shutdown. It
    // does not publish per-resource destruction outside its internal pools.
    // Worker completion is the resource lifetime barrier for all packets.
    sAssets.clear();
    sBindings.clear();
    sRasterPipelines.clear();
    sVertexBuffer=0;
    sIndexBuffer=0;
    sPipeline=0;
    sGpuReady=false;
}
void save_report(uint32_t frame) noexcept {
    std::FILE* f=std::fopen(kReportPath,"wb");
    if(!f)return;
    const auto& c=sCounters;
    std::fprintf(f,"# native_match_v5 gx_fallback=0 live_game=1 gpu_material=vertex_color_preview texture_samples=0 skinning=cpu_pn_preview projection=glx_six_coeff visual_debug=%u\n",unsigned(sVisualDebug));
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    std::fprintf(f,"# renderer=strikers_native_gxm gxm_owner=aurora_fifo native_error=0x%08x\n",
                 static_cast<unsigned>(sNativeRenderer.error_code()));
#else
    std::fprintf(f,"# renderer=aurora_gxm gxm_owner=aurora_fifo\n");
#endif
    std::fprintf(f,"# error=%s mesh_manifest=%u native_pipeline=%u\n",
                 sFailure.load(std::memory_order_relaxed),
                 unsigned(sAssets.mesh_count()),unsigned(sGpuReady));
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    std::fprintf(f,"# uv_sidecar=%s (texture_binding=not_connected)\n",sUvStatus);
    std::fprintf(f,"# dynamic_slot_fence=%u timing=%u\n",unsigned(sSlotFence),unsigned(sTiming));
    std::fprintf(f,"# compact_vertices=%u input_vertices=%llu output_vertices=%llu\n",unsigned(sCompactVertices),
        (unsigned long long)sDynamicInputVertices,(unsigned long long)sDynamicOutputVertices);
    if(sTiming)std::fprintf(f,"# native_timing attribution_only=1 frames=%llu prepare_us=%llu present_wait_us=%llu fence_us=%llu upload_us=%llu draw_us=%llu display_queue_us=%llu dynamic_bytes=%llu draws=%u global_finishes=%u slot_waits=%u\n",
        (unsigned long long)sTimingFrames,(unsigned long long)sPrepareUs,(unsigned long long)sPresentWaitUs,
        (unsigned long long)sTimingTotal.fenceUs,(unsigned long long)sTimingTotal.uploadUs,
        (unsigned long long)sTimingTotal.drawUs,(unsigned long long)sTimingTotal.queueUs,
        (unsigned long long)sTimingTotal.dynamicVertexBytes,sTimingTotal.draws,
        sTimingTotal.globalFinishes,sTimingTotal.slotWaits);
#endif
    if (sCounters.invalidMvp) {
        std::fprintf(f,"# first_invalid_mvp frame=%u view=%u bits=",sInvalidMvpFrame,sInvalidMvpView);
        for (const auto bits : sInvalidMvpBits) std::fprintf(f,"%08x ",bits);
        std::fputc('\n',f);
    }
    std::fprintf(f,"frame,callbacks,native_draws_queued,native_draws_accepted,native_draws_rejected,unsupported_view,unsupported_material,unsupported_raster,unsupported_geometry,missing_asset,upload_errors,queue_errors,present_errors,gx_suppressed_bytes,gx_suppressed_callbacks,skinned_rejected,user_effect_rejected,color_preview_candidates,color_preview_draws,preview_textured,preview_lit,plain_unlit_draws,invalid_matrix_identity,source_registrations,source_releases,source_hits,source_unbound,source_dirty_rehash,source_invalid,source_misses,source_unsafe,invalid_mvp,dynamic_queued,dynamic_source_missing,dynamic_invalid\n");
    // GX FIFO still receives API calls from the unported GLX/target/swap code,
    // but must discard their encoded commands. Never call these "GX-free sources".
    const uint64_t fields[] = {
        frame, c.callbacks, c.nativeDraws,
        sNativeDrawsAccepted.load(std::memory_order_relaxed),
        sNativeDrawsRejected.load(std::memory_order_relaxed),
        c.unsupportedView, c.unsupportedMaterial, c.unsupportedRaster,
        c.unsupportedGeometry, c.missingNativeAsset, c.uploadErrors,
        c.queueErrors, c.presentErrors.load(std::memory_order_relaxed),
        aurora::gx::fifo::native_match_suppressed_gx_bytes(),
        aurora::gx::fifo::native_match_suppressed_gx_callbacks(),
        c.skinnedRejected, c.userEffectRejected,
        c.colorPreviewCandidates, c.colorPreviewDraws,
        c.previewTextured, c.previewLit, c.plainUnlitDraws,
        c.invalidMatrixIdentity,
        c.sourceRegistrations, c.sourceReleases, c.sourceHits,
        c.sourceUnbound, c.sourceDirtyRehash, c.sourceInvalid,
        c.sourceMisses, c.sourceUnsafe, c.invalidMvp,
        c.dynamicQueued,c.dynamicSourceMissing,c.dynamicInvalid,
    };
    for (size_t i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i)
        std::fprintf(f, "%s%llu", i ? "," : "",
                     static_cast<unsigned long long>(fields[i]));
    std::fputc('\n', f);
    std::fclose(f);
    save_visual_coverage(frame);
}

// The GPU-ready AVNR records contain static position+RGBA8 geometry, not the
// TEV/texture/lights or indexed-PN palette of a live material. We can still
// submit those ORIGINAL game meshes directly through GXM with a vertex-color
// preview shader. Never route unsupported programs to GX as a substitute.
bool native_preview_program(uint32_t program) noexcept {
    static const uint32_t supported[] = {
        glGetProgram("3d unlit"), glGetProgram("3d unlit 2x"),
        glGetProgram("3d pointlit"), glGetProgram("3d pointlit dirt"),
        glGetProgram("3d crowd"), glGetProgram("3d crowd lit"),
        glGetProgram("2d unlit"),
    };
    for (const uint32_t value : supported)
        if (program == value) return true;
    return false;
}

bool visible_main_view(eGLView view) noexcept {
    switch (view) {
    case GLV_Skybox:
    case GLV_Shadowed:
    case GLV_WorldShadowed:
    case GLV_Unshadowed:
    case GLV_Characters:
    case GLV_UnsortedPerspective:
    case GLV_LingeringParticles:
    case GLV_Particles:
    case GLV_ElectricFence:
    case GLV_CameraSpace:
    case GLV_FrontEnd:
    case GLV_UnsortedOrtho:
    case GLV_Transitions3D:
    case GLV_Anark3D_BG:
    case GLV_Anark3D_FG:
        return true;
    default:
        // EFB/shadow maps, fullscreen filters and auxiliary render targets
        // need their own native passes before they can be displayed.
        return false;
    }
}

bool static_userdata_supported(const glModelPacket& packet,
                               bool& hasSkin,bool allowSkin=false) noexcept {
    hasSkin = false;
    if (!packet.userData) return true;
    // Live GLUserData tables come from the engine's frame or resource heap.
    // Lighting callbacks are safe to approximate by vertex-color preview;
    // transforms, stencil/shadows and viewport overrides are not.
    for (int i = 0; i < static_cast<int>(GLUD_Num); ++i) {
        const auto kind = static_cast<eGLUserData>(i);
        if (!glUserHasType(kind, &packet)) continue;
        if (kind == GLUD_Skin) {
            hasSkin = true;
            if (!allowSkin) return false;
            continue;
        }
        switch (kind) {
        case GLUD_EnvDiffuse:
            // DrawableModel::Fresnelify mutates vertex alpha without flushing
            // the color stream: static copies become stale, while dynamic
            // snapshots own the current color bytes and may preview them.
            if (!allowSkin) return false;
            break;
        case GLUD_ShadowVolume:
        case GLUD_ShadowBuffer:
        case GLUD_Viewport:
        case GLUD_Scissor:
        case GLUD_CoPlanar:
            // These change clip/scissor, stencil, or another render target.
            return false;
        default:
            // Lighting, constant tint, fog, diffuse and translucency are
            // intentionally *approximated* by the native color-preview pass.
            break;
        }
    }
    return true;
}

constexpr uint32_t source_primitive(uint8_t glg) noexcept {
    constexpr uint8_t kMap[]={1,2,3,0,4,5};
    return glg<6 ? kMap[glg] : 0xffffffffu;
}
bool source_location(const glModelPacket& p,SourceLocation& out) noexcept {
    if(!p.indexBuffer||!p.streams||p.numStreams==0||p.numStreams>16||p.primType>5)
        return false;
    const DisplayList* dl=dlGetStruct(p.indexBuffer);
    if(!dl||!dl->list||dl->indices[0]!=p.numStreams)return false;
    // dlMakeDisplayList inserts one direct PN matrix-index byte for stream
    // id 12. Color0 is stream id 2 and stays indexed, not a direct color.
    const unsigned directPnIndex=dl->indices[1]?1u:0u;
    if(directPnIndex>p.numStreams)return false;
    const size_t stride=size_t(p.numStreams-directPnIndex)*2+directPnIndex;
    const size_t actual=size_t(p.numVertices)*stride;
    if(!stride||actual>512*1024||dl->size<3+actual)return false;
    const glModelStream* pos=nullptr,*color=nullptr;
    for(unsigned i=0;i<p.numStreams;++i) {
        if(p.streams[i].id==0)pos=&p.streams[i];
        if(p.streams[i].id==2)color=&p.streams[i];
    }
    if(!pos||!pos->address||!pos->dataSize||pos->dataSize>1024*1024)
        return false;
    if(color&&(color->dataSize>512*1024||
              (color->dataSize!=0&&!color->address)))return false;
    const uint32_t colorBytes=color?color->dataSize:0;
    out={p.indexBuffer,reinterpret_cast<uintptr_t>(dl->list),pos->address,
         colorBytes?color->address:0u,dl->size,pos->dataSize,colorBytes,
         p.numVertices,source_primitive(p.primType),p.numStreams,
         dl->indices[1]};
    return true;
}
bool source_key(const glModelPacket& p,SourceIdentity& out,
                SourceLocation* location=nullptr) noexcept {
    SourceLocation current{};
    if(!source_location(p,current))return false;
    // Hash only at static resource registration, or if a tracked CPU range
    // is dirtied; the hot draw path uses the bound GPU mesh ID directly.
    const uint32_t indexed=current.streamCount-current.pnIndex;
    const uint32_t rawBytes=current.drawVertices*(2u*indexed+current.pnIndex);
    out={current.drawVertices,current.primitive,rawBytes,
         current.positionBytes,current.colorBytes,
         fnv64(reinterpret_cast<const uint8_t*>(current.listData)+3,rawBytes),
         fnv64(reinterpret_cast<const uint8_t*>(current.position),current.positionBytes),
         current.colorBytes?fnv64(reinterpret_cast<const uint8_t*>(current.color),
                                  current.colorBytes):0};
    if(location)*location=current;
    return true;
}

bool tracked_ranges_valid(NativeSourceBinding& entry,
                          const glModelPacket& packet) noexcept {
    namespace rev=aurora::vita::gfx;
    return validate_source_revisions(entry,
        []() noexcept {return rev::memory_write_epoch();},
        [](const NativeSourceBinding& entry) noexcept {
            const auto& w=entry.location;
            const rev::MemoryRevisionCheck checks[]={
                {reinterpret_cast<const void*>(w.listData),w.listBytes,entry.listRevision},
                {reinterpret_cast<const void*>(w.position),w.positionBytes,entry.positionRevision},
                {reinterpret_cast<const void*>(w.color),w.colorBytes,entry.colorRevision},
            };
            return rev::memory_ranges_match(checks,3);
        },
        [&packet](NativeSourceBinding& binding) noexcept {
            ++sCounters.sourceDirtyRehash;
            SourceIdentity current{};
            if(!source_key(packet,current)||current!=binding.identity) {
                ++sCounters.sourceInvalid;
                return false;
            }
            // 64KiB page tracking can report neighboring allocations as
            // dirty. Rehash once and refresh the revisions only if equal.
            const auto& w=binding.location;
            binding.listRevision=rev::memory_range_revision(
                reinterpret_cast<const void*>(w.listData),w.listBytes);
            binding.positionRevision=rev::memory_range_revision(
                reinterpret_cast<const void*>(w.position),w.positionBytes);
            binding.colorRevision=rev::memory_range_revision(
                reinterpret_cast<const void*>(w.color),w.colorBytes);
            return true;
        });
}

bool capture_dynamic(const glModelPacket& packet,bool hasPn,
                     nativegxm::ModelVertices& output) {
    if (!packet.streams || !packet.numStreams || packet.numStreams>16 ||
        packet.numVertices<3 || packet.numVertices>8192 || packet.primType>3) return false;
    const ModelSource* source=nullptr;
    if(packet.indexBuffer) {
        const auto* dl=dlGetStruct(packet.indexBuffer);
        if(!dl || !dl->list) return false;
        source=sModelSources.find(packet.indexBuffer,
            reinterpret_cast<uintptr_t>(dl->list),packet.numVertices,
            packet.primType,packet.numStreams,hasPn);
        if(!source)return false;
    } else if(hasPn) return false;

    nativegxm::ModelStreams streams{};
    for(unsigned i=0;i<packet.numStreams;++i) {
        const auto& s=packet.streams[i];
        nativegxm::ModelArray* target=nullptr;
        switch(s.id) {
        case 0:target=&streams.position;break;
        case 1:target=&streams.normal;break;
        case 2:target=&streams.color;break;
        case 3:target=&streams.uv;break;
        default:continue;
        }
        if(target->bytes.data() || !s.address || !s.stride || !s.dataSize ||
           s.dataSize>2u*1024u*1024u || s.beData>1)return false;
        *target={{reinterpret_cast<const uint8_t*>(s.address),s.dataSize},
                 s.stride,s.beData==0};
    }
    if(streams.position.bytes.empty())return false;
    std::vector<nativegxm::BoneTransform> bones;
    if(hasPn) {
        if(!source || !packet.userData || !glUserHasType(GLUD_Skin,&packet))return false;
        const uintptr_t skinHandle=reinterpret_cast<const uintptr_t*>(packet.userData)[GLUD_Skin];
        const auto* data=static_cast<const uint8_t*>(
            glUserGetData(reinterpret_cast<const void*>(skinHandle)));
        uint32_t count=0;std::memcpy(&count,data,sizeof count);
        if(!count || count>32)return false;
        const auto* skin=reinterpret_cast<const GLSkinUserData*>(data+4);
        bones.reserve(count);
        for(uint32_t i=0;i<count;++i) {
            const int selector=skin[i].reg+99;
            if(selector<3 || selector>27 || selector%3)return false;
            nativegxm::BoneTransform bone{};
            bone.selector=static_cast<uint32_t>(selector);
            std::memcpy(bone.matrix.data(),skin[i].mat,sizeof(skin[i].mat));
            bones.push_back(bone);
        }
    }
    const auto indices=source?std::span<const uint16_t>(source->indices):std::span<const uint16_t>{};
    const auto selectors=hasPn?std::span<const uint8_t>(source->selectors):std::span<const uint8_t>{};
    return nativegxm::prepare_model_vertices(streams,indices,packet.numVertices,
        packet.primType,selectors,bones,output,sCompactVertices);
}
} // namespace

void enable(bool yes) noexcept {
    sEnabled=yes;
}
bool enabled() noexcept {return sEnabled;}
bool uses_own_renderer() noexcept {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    return sEnabled;
#else
    return false;
#endif
}

bool readback_presented_rgba8(std::vector<uint8_t>& pixels) noexcept {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    if (sEnabled && sGpuReady && aurora::gx::fifo::worker_running()) {
        struct Work { std::vector<uint8_t>* pixels; bool ok; } work{&pixels,false};
        aurora::gx::fifo::run_sync_native([](void* p) {
            auto& w=*static_cast<Work*>(p);
            w.ok=sNativeRenderer.readback_presented_rgba8(*w.pixels);
        },&work);
        return work.ok;
    }
#endif
    pixels.clear();
    return false;
}

void on_display_list_created(const glModelPacket* packet,uintptr_t displayList,
                             uint32_t resourceLevel) noexcept {
    if(!sEnabled||!packet||!displayList)return;
    const auto* dl = dlGetStruct(displayList);
    bool hasPn = false;
    for (unsigned i=0;packet->streams && i<packet->numStreams;++i)
        hasPn |= packet->streams[i].id == 12;
    if(dl && packet->indexBuffer && packet->numVertices>=3 && packet->numVertices<=64000 &&
       packet->numStreams>0 && packet->numStreams<=16 && packet->primType<=3) {
        try {
            sModelSources.capture(displayList,reinterpret_cast<uintptr_t>(dl->list),
                resourceLevel,packet->primType,packet->numStreams,
                {reinterpret_cast<const uint16_t*>(packet->indexBuffer),packet->numVertices},hasPn);
        } catch(const std::bad_alloc&) {
            ++sCounters.sourceUnsafe;
        }
    }
    // Static GLX resource allocations survive frames. Frame pools and
    // external/morph buffers have mutable lifetimes, so never prebind them.
    glModelPacket compiled=*packet;
    compiled.indexBuffer=displayList;
    SourceLocation where{};
    SourceIdentity source{};
    namespace rev=aurora::vita::gfx;
    const uint64_t before=rev::memory_write_epoch();
    if(!source_location(compiled,where)||where.pnIndex) {
        ++sCounters.sourceUnsafe;
        return;
    }
    auto owned=[](uintptr_t base,uint32_t bytes) noexcept {
        return base&&bytes&&base<=UINTPTR_MAX-(bytes-1u)&&
               glxVitaResourceArenaOwns(reinterpret_cast<const void*>(base))&&
               glxVitaResourceArenaOwns(reinterpret_cast<const void*>(base+bytes-1u));
    };
    if(!owned(displayList,static_cast<uint32_t>(sizeof(DisplayList)))||
       !owned(where.listData,where.listBytes)||
       !owned(where.position,where.positionBytes)||
       (where.colorBytes&&!owned(where.color,where.colorBytes))) {
        ++sCounters.sourceUnsafe;
        return;
    }
    SourceLocation verified{};
    if(!source_key(compiled,source,&verified)||verified!=where) {
        ++sCounters.sourceUnsafe;
        return;
    }
    const auto listRev=rev::memory_range_revision(
        reinterpret_cast<const void*>(where.listData),where.listBytes);
    const auto posRev=rev::memory_range_revision(
        reinterpret_cast<const void*>(where.position),where.positionBytes);
    const auto colRev=rev::memory_range_revision(
        reinterpret_cast<const void*>(where.color),where.colorBytes);
    if(rev::memory_write_epoch()!=before) {
        ++sCounters.sourceUnsafe;
        return;
    }
    if(sBindings.register_source(where,source,resourceLevel,listRev,posRev,
                                 colRev,before,sAssets.mesh_count()?&sAssets:nullptr))
        ++sCounters.sourceRegistrations;
    else
        ++sCounters.sourceUnsafe;
}

void on_stitch_selectors(const glModelPacket* packet,const uint8_t* selectors,
                         uint32_t count) noexcept {
    if (!sEnabled || !packet || !selectors || count!=packet->numVertices) return;
    const auto* dl=dlGetStruct(packet->indexBuffer);
    if (dl) {
        try {
            sModelSources.attach_stitch(packet->indexBuffer,
                reinterpret_cast<uintptr_t>(dl->list),{selectors,count});
        } catch(const std::bad_alloc&) {
            ++sCounters.sourceUnsafe;
        }
    }
}

void on_resource_release(uint32_t markerLevel) noexcept {
    if(!sEnabled)return;
    sModelSources.release_after(markerLevel);
    sBindings.release_after(markerLevel);
    ++sCounters.sourceReleases;
}

bool preload() noexcept {
    if(!sEnabled)return true;
    const char* visualDebug=std::getenv("STRIKERS_GXM_NATIVE_VISUAL_DEBUG");
    sVisualDebug=visualDebug && std::strcmp(visualDebug,"1")==0;
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    const char* slotFence=std::getenv("STRIKERS_GXM_NATIVE_SLOT_FENCE");
    sSlotFence=slotFence && std::strcmp(slotFence,"1")==0;
    const char* timing=std::getenv("STRIKERS_GXM_NATIVE_TIMING");
    sTiming=timing && std::strcmp(timing,"1")==0;
    const char* compact=std::getenv("STRIKERS_GXM_NATIVE_COMPACT_VERTICES");
    sCompactVertices=compact && std::strcmp(compact,"1")==0;
#endif
    // GXInit has to finish configuring Aurora first. Native strict mode starts
    // here, before the first game frame, not while the backend is booting.
    aurora::gx::fifo::wait_idle();
    aurora::gx::fifo::set_native_match_strict_gx(true);
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    if (!aurora::gx::fifo::worker_running()) {
        sFailure.store("native_gxm_owner_inactive",std::memory_order_relaxed);
        save_report(0);
        return false;
    }
#endif
    if(!load_pack()){save_report(0);return false;}
    sBindings.resolve(sAssets);
    if(!sGpuReady&&!sGpuFailed) {
        aurora::gx::fifo::run_sync_native(upload_on_owner,nullptr);
        // run_sync_native is a real completion barrier for buffer uploads.
        // Keeping only IDs and offsets avoids retaining the entire 6 MiB
        // catalog twice in Vita main memory for the rest of the match.
        sAssets.release_upload_bytes();
    }
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    if(sGpuReady) {
        UvAssets uv{};
        bool loaded=false;
        try {loaded=load_uv_sidecar(uv);}
        catch(const std::bad_alloc&) {sUvStatus="out_of_memory";}
        if(!loaded) {
            sGpuReady=false;
            sFailure.store("native_uv_preload_failed",std::memory_order_relaxed);
        } else if(!uv.vertices.empty()) {
            aurora::gx::fifo::run_sync_native([](void* context) {
                const auto& data=*static_cast<const UvAssets*>(context);
                if(!sNativeRenderer.upload_uvs(data.vertices,data.hasUv)){
                    sGpuReady=false;
                    sUvStatus="gpu_upload_failed";
                    sFailure.store(sNativeRenderer.last_error(),std::memory_order_relaxed);
                } else sUvStatus="uploaded";
            },&uv);
        }
    }
#endif
    save_report(0);
    return sGpuReady;
}
bool begin_frame() noexcept {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    if (sEnabled) {
        sBeginOk=false;
        aurora::gx::fifo::run_sync_native([](void*) {
            sBeginOk=sNativeRenderer.begin_frame();
            if (!sBeginOk) sFailure.store(sNativeRenderer.last_error(),std::memory_order_relaxed);
        },nullptr);
        sFrameStarted=sBeginOk;
        if (!sBeginOk) save_report(static_cast<uint32_t>(glGetCurrentFrame()));
        return sBeginOk;
    }
#endif
    sFrameStarted=sEnabled;
    return true;
}
bool end_frame() noexcept {
    sFrameStarted=false;
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    if (sEnabled) {
        sEndOk=false;
        aurora::gx::fifo::run_sync_native([](void*) {
            sEndOk=sNativeRenderer.end_frame();
            if (!sEndOk) {
                sCounters.presentErrors.fetch_add(1,std::memory_order_relaxed);
                sFailure.store(sNativeRenderer.last_error(),std::memory_order_relaxed);
            }
        },nullptr);
        if(sEndOk && sTiming) {
            const auto t=sNativeRenderer.completed_timing();
            ++sTimingFrames;sTimingTotal.fenceUs+=t.fenceUs;
            sTimingTotal.uploadUs+=t.uploadUs;sTimingTotal.drawUs+=t.drawUs;
            sTimingTotal.queueUs+=t.queueUs;sTimingTotal.dynamicVertexBytes+=t.dynamicVertexBytes;
            sTimingTotal.draws+=t.draws;sTimingTotal.globalFinishes+=t.globalFinishes;
            sTimingTotal.slotWaits+=t.slotWaits;
        }
        if (!sEndOk) save_report(static_cast<uint32_t>(glGetCurrentFrame()));
        return sEndOk;
    }
#endif
    return true;
}
void present_native() noexcept {
    if(sEnabled&&sFrameStarted) {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
        const uint64_t start=sTiming?sceKernelGetSystemTimeWide():0;
#endif
        aurora::gx::fifo::run_sync_native(present_on_owner,nullptr);
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
        if(sTiming)sPresentWaitUs+=sceKernelGetSystemTimeWide()-start;
#endif
    }
}

void on_packet(eGLView view,unsigned long flags,const glModelPacket* packet) noexcept {
    if(!sEnabled)return;
    if(!sFrameStarted)return;
    ++sCounters.callbacks;
    const uint32_t frame=static_cast<uint32_t>(glGetCurrentFrame());
    if(frame!=sLastReportFrame&&(frame%120)==0){
        sLastReportFrame=frame;
        save_report(frame);
    }
    if(!(flags&0x800))return;
    coverage(view,Candidates);
    if(!packet||!visible_main_view(view)) {++sCounters.unsupportedView;coverage(view,ViewRejected);return;}
    if(!native_preview_program(packet->state.program)) {
        ++sCounters.unsupportedMaterial;coverage(view,ProgramRejected,packet);return;
    }
    bool hasPn=false,framePositions=false;
    for(unsigned i=0;packet->streams && i<packet->numStreams;++i) {
        const auto& s=packet->streams[i];
        if(s.id==12)hasPn=true;
        if(s.id==0 && s.address && s.stride==12 && s.beData==0 &&
           !glxVitaResourceArenaOwns(reinterpret_cast<const void*>(s.address)))
            framePositions=true;
    }
    const bool dynamic=hasPn || packet->indexBuffer==0 || framePositions;
    bool skinned = false;
    if (!static_userdata_supported(*packet, skinned,dynamic)) {
        if (skinned) {++sCounters.skinnedRejected;coverage(view,SkinRejected);}
        else {++sCounters.userEffectRejected;coverage(view,UserDataRejected,packet);}
        return;
    }
    if(hasPn&&!skinned){++sCounters.skinnedRejected;coverage(view,SkinRejected);return;}
    if(!supported_raster(packet->state.raster,hasPn)) {
        ++sCounters.unsupportedRaster;coverage(view,RasterRejected,packet);return;
    }
    if(dynamic) {
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
        if(!sGpuReady||!aurora::gx::fifo::worker_running()) {
            ++sCounters.uploadErrors;return;
        }
        if(packet->indexBuffer) {
            const auto* dl=dlGetStruct(packet->indexBuffer);
            if(!dl || !dl->list || !sModelSources.find(packet->indexBuffer,
                reinterpret_cast<uintptr_t>(dl->list),packet->numVertices,
                packet->primType,packet->numStreams,hasPn)) {
                ++sCounters.dynamicSourceMissing;
                coverage(view,SourceMissing);
                return;
            }
        }
        const uint64_t prepareStart=sTiming?sceKernelGetSystemTimeWide():0;
        auto work=std::unique_ptr<DynamicDrawWork>(new(std::nothrow) DynamicDrawWork);
        bool prepared=false;
        try {
            if(work) prepared=capture_dynamic(*packet,hasPn,work->model);
        } catch(const std::bad_alloc&) {
            prepared=false;
        }
        if(sTiming)sPrepareUs+=sceKernelGetSystemTimeWide()-prepareStart;
        if(!prepared) {
            ++sCounters.dynamicInvalid;coverage(view,GeometryRejected);return;
        }
        sDynamicInputVertices+=packet->numVertices;
        sDynamicOutputVertices+=work->model.vertices.size();
        nlMatrix4 viewMatrix,projection,modelView;
        glViewGetViewMatrix(view,viewMatrix);
        glViewGetProjectionMatrix(view,projection);
        if(packet->state.matrix!=glGetIdentityMatrix() && packet->state.matrix &&
           port_region_owns(reinterpret_cast<const void*>(packet->state.matrix)))
            nlMultMatrices(modelView,*reinterpret_cast<const nlMatrix4*>(packet->state.matrix),viewMatrix);
        else modelView=viewMatrix;
        work->packedRaster=packet->state.raster;
        work->mvp=compose_glx_mvp(modelView.e,projection.e);
        if(!nativegxm::finite_mvp(work->mvp)) {
            ++sCounters.invalidMvp;coverage(view,InvalidTransform);return;
        }
        DynamicDrawWork* owned=work.release();
        const uint64_t serial=aurora::gx::fifo::run_async_native(
            draw_dynamic_on_owner,&owned,sizeof owned);
        if(!serial && owned) {
            // A failed enqueue retains ownership; an inline fallback already
            // ran the callback and cleared the producer's pointer.
            delete owned;
            ++sCounters.queueErrors;
            sFailure.store("native_dynamic_queue_failed",std::memory_order_relaxed);
            return;
        }
        ++sCounters.dynamicQueued;
        ++sCounters.nativeDraws;
        coverage(view,Queued);
#else
        ++sCounters.dynamicInvalid;
#endif
        return;
    }
    const bool plainUnlit = packet->state.program == glGetProgram("3d unlit") &&
                            packet->state.texconfig == 0 &&
                            packet->state.materialstate == 1 &&
                            packet->userData == 0;
    if (!plainUnlit) ++sCounters.colorPreviewCandidates;
    if(!sLoaded||sAssets.mesh_count()==0) {++sCounters.missingNativeAsset;return;}
    if(!sGpuReady){++sCounters.uploadErrors;return;}
    SourceLocation where{};
    if(!source_location(*packet,where)){++sCounters.unsupportedGeometry;coverage(view,GeometryRejected);return;}
    NativeSourceBinding* binding=sBindings.lookup(packet->indexBuffer,where);
    if(!binding){++sCounters.sourceUnbound;coverage(view,Unbound);return;}
    if(binding->meshIndex==kNoNativeMesh){
        ++sCounters.sourceMisses;coverage(view,SourceMissing);
        if (sVisualDebug && static_cast<unsigned>(view)<GLV_Num && sViewCoverage[view].count[SourceMissing]==1)
            sViewCoverage[view].firstMissingSource=binding->identity;
        return;
    }
    if(!tracked_ranges_valid(*binding,*packet)){coverage(view,InvalidSource);return;}
    ++sCounters.sourceHits;
    const unsigned found=binding->meshIndex;
    if(!aurora::gx::fifo::worker_running()) {
        ++sCounters.queueErrors;
        sFailure.store("native_gxm_owner_inactive",std::memory_order_relaxed);
        return;
    }

    nlMatrix4 viewMatrix,projection,modelView;
    glViewGetViewMatrix(view,viewMatrix);
    glViewGetProjectionMatrix(view,projection);
    if(packet->state.matrix != glGetIdentityMatrix() &&
       packet->state.matrix && port_region_owns(
                reinterpret_cast<const void*>(packet->state.matrix)))
        nlMultMatrices(modelView,*reinterpret_cast<const nlMatrix4*>(packet->state.matrix),viewMatrix);
    else {
        // Same identity behavior as glx_SwitchMatrix, without executing GX.
        modelView=viewMatrix;
        if(packet->state.matrix!=glGetIdentityMatrix())
            ++sCounters.invalidMatrixIdentity;
    }
    DrawWork work{};
    work.index=found;
    work.frame=frame;
    work.packedRaster=packet->state.raster;
    work.revision=sNextDrawRevision++;
    work.mvp=compose_glx_mvp(modelView.e,projection.e);
    // Reject invalid producer data before queuing it to the GPU owner. A
    // NaN/Inf transform must not poison the native renderer's sticky driver
    // error state and stop otherwise valid draws for the entire frame.
    if (!nativegxm::finite_mvp(work.mvp)) {
        ++sCounters.invalidMvp;
        coverage(view,InvalidTransform);
        if (sInvalidMvpFrame==~0u) {
            sInvalidMvpFrame=frame;
            sInvalidMvpView=static_cast<uint32_t>(view);
            std::memcpy(sInvalidMvpBits.data(),work.mvp.data(),sizeof(work.mvp));
        }
        return;
    }
    if(!aurora::gx::fifo::run_async_native(draw_on_owner,&work,sizeof work)){
        ++sCounters.queueErrors;
        sFailure.store("direct_game_draw_queue_failed",std::memory_order_relaxed);
        return;
    }
    ++sCounters.nativeDraws;
    coverage(view,Queued);
    if (sVisualDebug) {
        if (glGetRasterState(packet->state.raster,GLS_Culling)==3) coverage(view,CullAll);
        if (!(glGetRasterState(packet->state.raster,GLS_ColourWrite)&1u)) coverage(view,NoRgb);
    }
    if (plainUnlit) ++sCounters.plainUnlitDraws;
    else {
        ++sCounters.colorPreviewDraws;
        if (packet->state.texconfig) ++sCounters.previewTextured;
        if (packet->state.program!=glGetProgram("3d unlit") &&
            packet->state.program!=glGetProgram("3d unlit 2x"))
            ++sCounters.previewLit;
    }
}

void shutdown() noexcept {
    if(!sEnabled)return;
    aurora::gx::fifo::wait_idle();
    save_report(static_cast<uint32_t>(glGetCurrentFrame()));
    if(sPipeline||sVertexBuffer||sIndexBuffer
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
       ||sGraphicsDetached
#endif
    )
        aurora::gx::fifo::run_sync_native(release_on_owner,nullptr);
    else
        sAssets.clear();
    sBindings.clear();
    sModelSources.clear();
    sEnabled=false;
#if defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
    // The Aurora graphics objects have been retired. Keep legacy jobs blocked
    // through backend shutdown; never decode queued GX into a dead renderer.
    if (!sGraphicsDetached)
#endif
    aurora::gx::fifo::set_native_match_strict_gx(false);
}
} // namespace port::directmatch
#endif
