#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_GXM_RENDERER)
#include "port/native_gxm_renderer.hpp"
#include "port/native_scene_shaders.hpp"
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <vitashark.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <vector>
#include <string>

namespace port::nativegxm {
namespace {
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
std::FILE* shaderLog=nullptr;
void shader_log(const char* message,shark_log_level level,int line){if(!shaderLog)shaderLog=std::fopen("ux0:data/strikersVita/native_shader_compile.log","w");if(shaderLog){std::fprintf(shaderLog,"%d:%d %s\n",int(level),line,message);std::fflush(shaderLog);}}
#endif
constexpr uint32_t kWidth = 960, kHeight = 544, kStride = 960;
enum class Mapping { Data, VertexUsse, FragmentUsse };

// Every memblock is owned here, including rollback of partially mapped
// allocations. No shared Aurora pool or renderer handle is used.
struct Block {
    SceUID uid = -1;
    void* data = nullptr;
    uint32_t bytes = 0, usseOffset = 0;
    bool mapped = false;
    Mapping mapping = Mapping::Data;
    ~Block() { reset(); }
    int allocate(size_t requested, bool cdram = false, Mapping kind = Mapping::Data) noexcept {
        if (uid >= 0 || !requested) return SCE_GXM_ERROR_INVALID_VALUE;
        const size_t alignment = cdram ? 256u * 1024u : 4096u;
        if (requested > UINT32_MAX - (alignment - 1u)) return SCE_GXM_ERROR_OUT_OF_MEMORY;
        bytes = static_cast<uint32_t>((requested + alignment - 1u) & ~(alignment - 1u));
        mapping = kind;
        uid = sceKernelAllocMemBlock("strikers-native-gxm",
            cdram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
            bytes, nullptr);
        if (uid < 0) return uid;
        int result = sceKernelGetMemBlockBase(uid, &data);
        if (result >= 0) {
            if (kind == Mapping::VertexUsse) result = sceGxmMapVertexUsseMemory(data, bytes, &usseOffset);
            else if (kind == Mapping::FragmentUsse) result = sceGxmMapFragmentUsseMemory(data, bytes, &usseOffset);
            else result = sceGxmMapMemory(data, bytes, SCE_GXM_MEMORY_ATTRIB_RW);
        }
        mapped = result >= 0;
        if (result < 0) reset();
        return result;
    }
    void reset() noexcept {
        if (mapped) {
            if (mapping == Mapping::VertexUsse) sceGxmUnmapVertexUsseMemory(data);
            else if (mapping == Mapping::FragmentUsse) sceGxmUnmapFragmentUsseMemory(data);
            else sceGxmUnmapMemory(data);
        }
        if (uid >= 0) sceKernelFreeMemBlock(uid);
        uid = -1; data = nullptr; bytes = usseOffset = 0; mapped = false;
    }
};

struct DisplayRequest { void* pixels; std::atomic<int>* result; };
void display_callback(const void* opaque) {
    const auto& request = *static_cast<const DisplayRequest*>(opaque);
    SceDisplayFrameBuf fb{};
    fb.size = sizeof(fb); fb.base = request.pixels;
    fb.width = kWidth; fb.height = kHeight; fb.pitch = kStride;
    fb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    int result = sceDisplaySetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME);
    if (result >= 0) result = sceDisplayWaitVblankStart();
    if (result < 0) request.result->store(result, std::memory_order_relaxed);
}
void* patch_alloc(void*, SceSize n) { return std::malloc(n); }
void patch_free(void*, void* p) { std::free(p); }

constexpr const char* kVertexShader = R"(
void main(float4 a_position : POSITION, float4 a_color : COLOR0,
          uniform float4 u_mvp[4], out float4 v_position : POSITION,
          out float4 v_color : COLOR0) {
    float4 p=u_mvp[0]*a_position.x+u_mvp[1]*a_position.y+
             u_mvp[2]*a_position.z+u_mvp[3]*a_position.w;
    p.z=2.0*p.z+p.w;
    v_position=p;
    v_color=a_color;
})";
constexpr const char* kFragmentShader = R"(
float4 main(float4 v_color : COLOR0) : COLOR { return v_color; }
)";
constexpr const char* kTexturedVertexShader = R"(
void main(float4 a_position : POSITION, float4 a_color : COLOR0, float3 a_uv : TEXCOORD0,
          uniform float4 u_mvp[4], out float4 v_position : POSITION,
          out float4 v_color : COLOR0, out float3 v_uv : TEXCOORD0) {
    float4 p=u_mvp[0]*a_position.x+u_mvp[1]*a_position.y+
             u_mvp[2]*a_position.z+u_mvp[3]*a_position.w;
    p.z=2.0*p.z+p.w;
    v_position=p;v_color=a_color;v_uv=a_uv;
})";
constexpr const char* kTexturedFragmentShader = R"(
float4 main(float4 v_color : COLOR0, float3 v_uv : TEXCOORD0,
            uniform sampler2D u_texture : TEXUNIT0,
            uniform float4 u_tint, uniform float4 u_controls) : COLOR {
    float4 base=v_color*u_tint;
    if(fmod(u_controls.w,2.0)>0.5) base.rgb=u_tint.rgb;
    if(u_controls.w>=2.0) base.a=u_tint.a;
    float4 c=tex2D(u_texture,v_uv.xy/v_uv.z)*base;
    if(u_controls.x>0.5 && c.a<=u_controls.y) discard;
    c.rgb=saturate(c.rgb*u_controls.z);
    return c;
})";
constexpr std::array<float, 16> kIdentity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
struct Vertex { float position[4]; uint8_t color[4]; };
static_assert(sizeof(Vertex) == kVertexStride);
constexpr Vertex kClearVertices[] = {
    {{-1,-1,0,1}, {0,0,0,255}}, {{3,-1,0,1}, {0,0,0,255}}, {{-1,3,0,1}, {0,0,0,255}}
};
constexpr uint16_t kClearIndices[] = {0,1,2};

SceGxmBlendInfo blend_info(unsigned blend, unsigned mask) noexcept {
    SceGxmBlendInfo b{};
    b.colorMask = ((mask & 1u) ? SCE_GXM_COLOR_MASK_R|SCE_GXM_COLOR_MASK_G|SCE_GXM_COLOR_MASK_B : 0) |
                  ((mask & 2u) ? SCE_GXM_COLOR_MASK_A : 0);
    b.colorFunc = b.alphaFunc = SCE_GXM_BLEND_FUNC_ADD;
    b.colorSrc = b.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
    b.colorDst = b.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;
    switch (blend) {
    case 1: b.colorSrc=b.alphaSrc=SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
            b.colorDst=b.alphaDst=SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
    case 2: b.colorDst=b.alphaDst=SCE_GXM_BLEND_FACTOR_ONE; break;
    case 3: b.colorSrc=b.alphaSrc=SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
            b.colorDst=b.alphaDst=SCE_GXM_BLEND_FACTOR_ONE; break;
    case 4: b.colorSrc=b.alphaSrc=SCE_GXM_BLEND_FACTOR_SRC_COLOR; break;
    case 5: b.colorSrc=b.alphaSrc=SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            b.colorDst=b.alphaDst=SCE_GXM_BLEND_FACTOR_ONE; break;
    case 6: break;
    case 7: b.colorFunc=b.alphaFunc=SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT;
            b.colorDst=b.alphaDst=SCE_GXM_BLEND_FACTOR_ONE; break;
    }
    return b;
}
} // namespace

struct Renderer::Impl {
    SceUID owner = -1;
    bool ownsGxm = false, ownsCompiler = false, initialized = false;
    bool inScene = false, displayed = false;
    bool slotFence=false, timings=false;
    FrameTiming currentTiming{}, completedTiming{};
    volatile unsigned int* notifications=nullptr;
    SceGxmContext* context = nullptr;
    SceGxmRenderTarget* target = nullptr;
    SceGxmShaderPatcher* patcher = nullptr;
    void* host = nullptr;
    Block vdm, vertexRing, fragmentRing, fragmentUsseRing;
    Block patchBuffer, vertexUsse, fragmentUsse, depth;
    Block vertices, indices, clearVertices, clearIndices;
    Block uvVertices;
    struct DynamicFrame {
        Block vertex, uv, index;
        size_t vertexUsed=0, indexUsed=0;
        bool gpuUsed=false;
        uint32_t retireValue=0;
    };
    std::array<DynamicFrame,3> dynamic{};
    std::vector<uint8_t> hasUv;
    struct Surface { Block memory; SceGxmColorSurface color{}; SceGxmSyncObject* sync = nullptr; };
    std::array<Surface, 3> surfaces;
    SceGxmDepthStencilSurface depthSurface{};
    SceGxmShaderPatcherId vertexId = nullptr, fragmentId = nullptr;
    SceGxmVertexProgram* vertexProgram = nullptr;
    std::array<SceGxmFragmentProgram*, 32> fragments{};
    const SceGxmProgramParameter* mvp = nullptr;
    std::vector<uint32_t> vertexCode, fragmentCode;
    SceGxmShaderPatcherId texturedVertexId=nullptr,texturedFragmentId=nullptr;
    SceGxmVertexProgram* texturedVertexProgram=nullptr;
    std::array<SceGxmFragmentProgram*,32> texturedFragments{};
    const SceGxmProgramParameter *texturedMvp=nullptr,*tint=nullptr,*controls=nullptr;
    std::vector<uint32_t> texturedVertexCode,texturedFragmentCode;
    struct Texture { Block memory,targetDepth; SceGxmTexture descriptor{}; uint32_t mipCount=0,width=0,height=0,stride=0;uint8_t minAlpha=255;bool swizzled=false;
        SceGxmRenderTarget* target=nullptr;SceGxmColorSurface color{};SceGxmDepthStencilSurface depth{};
        ~Texture(){if(target)sceGxmDestroyRenderTarget(target);}
    };
    std::vector<std::unique_ptr<Texture>> textures;
    size_t textureBytes=0;
    struct ResidentModel {Block vertex,index;uint32_t count=0;uint8_t minAlpha=255;};
    std::vector<std::unique_ptr<ResidentModel>> models,retiredModels;
    std::vector<std::unique_ptr<Texture>> retiredTextures;
    size_t modelBytes=0;
    struct SceneProgram {
        SceGxmShaderPatcherId vertexId=nullptr,fragmentId=nullptr;
        SceGxmVertexProgram* vertex=nullptr;
        std::array<SceGxmFragmentProgram*,32> fragments{};
        std::vector<uint32_t> vertexCode,fragmentCode;
        const SceGxmProgramParameter *mvp=nullptr,*mv=nullptr,*bones=nullptr,*ambient=nullptr,*diffuse=nullptr,
          *lighting=nullptr,*lightpos=nullptr,*lightcolor=nullptr,*specular=nullptr,*specularcolor=nullptr,
          *environment=nullptr,*controls=nullptr,*effects=nullptr,*tint=nullptr,*shadow=nullptr,*shadowadd=nullptr,*fog=nullptr,*fogcolor=nullptr,*scissor=nullptr;
    } scene;
    struct SceneFragment {
        SceGxmShaderPatcherId id=nullptr;
        std::vector<uint32_t> code;
        std::array<SceGxmFragmentProgram*,32> fragments{};
        const SceGxmProgramParameter *controls=nullptr,*effects=nullptr,*tint=nullptr,*shadow=nullptr,*shadowadd=nullptr,*fog=nullptr,*fogcolor=nullptr,*scissor=nullptr;
    };
    std::array<std::unique_ptr<SceneFragment>,256> sceneFragments;
    std::array<uint64_t,6> boundTextures{};
    uint32_t whiteTexture=0,currentTarget=0;
    Block sceneFrameVertex[3],sceneFrameIndex[3];
    size_t sceneVertexUsed=0,sceneIndexUsed=0;

    std::vector<Mesh> meshes;
    FrameSlots frames;
    std::atomic<int> displayError{0}, error{0};
    std::atomic<const char*> operation{"none"};

    bool fail(const char* name, int code = SCE_GXM_ERROR_INVALID_VALUE) noexcept {
        if (error.load(std::memory_order_relaxed) == 0) {
            operation.store(name, std::memory_order_relaxed);
            error.store(code, std::memory_order_relaxed);
        }
        return false;
    }
    bool check(int result, const char* name) noexcept { return result >= 0 || fail(name, result); }
    bool on_owner() noexcept {
        return owner == sceKernelGetThreadId() || fail("native_wrong_owner_thread");
    }
    bool compile(const char* source, shark_type type, std::vector<uint32_t>& code) noexcept {
        uint32_t bytes = static_cast<uint32_t>(std::strlen(source));
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
        shark_install_log_cb(shader_log);
#endif
        const auto* program = shark_compile_shader(source, &bytes, type);
        if (!program || !bytes) { shark_clear_output(); return fail("native_shader_compile"); }
        code.resize((bytes + 3u) / 4u);
        std::memcpy(code.data(), program, bytes);
        shark_clear_output();
        return check(sceGxmProgramCheck(reinterpret_cast<const SceGxmProgram*>(code.data())), "native_gxp_check");
    }
    bool bind_and_draw(const void* vertexData, const void* indexData, unsigned count,
                       const std::array<float,16>& matrix, const Raster& r,
                       const void* uvData=nullptr,const Material* material=nullptr) noexcept {
        if (r.cull == 3) return true;
        struct DrawTimer {
            Impl& d; uint64_t start;
            ~DrawTimer() { if(d.timings) {d.currentTiming.drawUs+=sceKernelGetSystemTimeWide()-start; ++d.currentTiming.draws;} }
        } timer{*this,timings?uint64_t(sceKernelGetSystemTimeWide()):0};
        sceGxmSetVertexProgram(context,material?texturedVertexProgram:vertexProgram);
        sceGxmSetFragmentProgram(context,material?texturedFragments[r.fragment_slot()]:fragments[r.fragment_slot()]);
        constexpr SceGxmDepthFunc depthFunctions[] = {SCE_GXM_DEPTH_FUNC_ALWAYS,
            SCE_GXM_DEPTH_FUNC_LESS_EQUAL, SCE_GXM_DEPTH_FUNC_EQUAL, SCE_GXM_DEPTH_FUNC_LESS};
        const auto function = r.depthTest ? depthFunctions[r.depthFunction] : SCE_GXM_DEPTH_FUNC_ALWAYS;
        const auto write = r.depthTest && r.depthWrite ? SCE_GXM_DEPTH_WRITE_ENABLED : SCE_GXM_DEPTH_WRITE_DISABLED;
        sceGxmSetFrontDepthFunc(context, function); sceGxmSetBackDepthFunc(context, function);
        sceGxmSetFrontDepthWriteEnable(context, write); sceGxmSetBackDepthWriteEnable(context, write);
        sceGxmSetCullMode(context, r.cull == 0 ? SCE_GXM_CULL_NONE : r.cull == 2 ? SCE_GXM_CULL_CCW : SCE_GXM_CULL_CW);
        if(material) {
            auto& source=*textures[material->texture-1u];
            SceGxmTexture descriptor=source.descriptor;
            const auto filter=material->filter?SCE_GXM_TEXTURE_FILTER_LINEAR:SCE_GXM_TEXTURE_FILTER_POINT;
            const auto mips=material->mipFilter?source.mipCount:1u;
            const auto mipFilter=mips>1&&material->mipFilter==2?
                SCE_GXM_TEXTURE_MIP_FILTER_ENABLED:SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
            if(!check(sceGxmTextureSetUAddrMode(&descriptor,material->wrapU?SCE_GXM_TEXTURE_ADDR_CLAMP:SCE_GXM_TEXTURE_ADDR_REPEAT),"native_texture_wrap_u")||
               !check(sceGxmTextureSetVAddrMode(&descriptor,material->wrapV?SCE_GXM_TEXTURE_ADDR_CLAMP:SCE_GXM_TEXTURE_ADDR_REPEAT),"native_texture_wrap_v")||
               !check(sceGxmTextureSetMinFilter(&descriptor,filter),"native_texture_min_filter")||
               !check(sceGxmTextureSetMagFilter(&descriptor,filter),"native_texture_mag_filter")||
               !check(sceGxmTextureSetMipmapCount(&descriptor,mips),"native_texture_mip_count")||
               !check(sceGxmTextureSetMipFilter(&descriptor,mipFilter),"native_texture_mip_filter")||
               !check(sceGxmSetFragmentTexture(context,0,&descriptor),"native_bind_texture"))return false;
            const float values[4]{float((material->flags&1)!=0),float(material->alphaRef)/255.f,
                                  material->flags&4?2.f:1.f,float((material->flags&2)!=0)+2.f*float((material->flags&8)!=0)};
            void* fragmentUniforms=nullptr;
            if(!check(sceGxmReserveFragmentDefaultUniformBuffer(context,&fragmentUniforms),"native_reserve_material")||
               !check(sceGxmSetUniformDataF(fragmentUniforms,tint,0,4,material->tint.data()),"native_material_tint")||
               !check(sceGxmSetUniformDataF(fragmentUniforms,controls,0,4,values),"native_material_controls")||
               !check(sceGxmSetVertexStream(context,1,uvData),"native_bind_uv"))return false;
        }
        void* uniforms = nullptr;
        return check(sceGxmReserveVertexDefaultUniformBuffer(context, &uniforms), "native_reserve_mvp") &&
               check(sceGxmSetUniformDataF(uniforms, material?texturedMvp:mvp, 0, 16, matrix.data()), "native_upload_mvp") &&
               check(sceGxmSetVertexStream(context, 0, vertexData), "native_bind_vertices") &&
               check(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indexData, count), "native_draw");
    }
};

Renderer::Renderer() noexcept = default;
Renderer::~Renderer() {
    shutdown();
    // A foreign thread must never implicitly unmap live GPU resources via
    // unique_ptr/Block destructors. The adapter explicitly retires on-owner.
    if (impl_ && impl_->ownsGxm) (void)impl_.release();
}

bool Renderer::initialize(bool slotFence,bool timings) noexcept {
    if (impl_) return impl_->initialized && impl_->on_owner();
    impl_.reset(new (std::nothrow) Impl);
    if (!impl_) return false;
    auto& d = *impl_;
    d.owner = sceKernelGetThreadId();
    d.slotFence=slotFence;d.timings=timings;
    const auto abort = [&]() { shutdown(); return false; };
    SceGxmInitializeParams init{};
    init.displayQueueMaxPendingCount = 2;
    init.displayQueueCallback = display_callback;
    init.displayQueueCallbackDataSize = sizeof(DisplayRequest);
    init.parameterBufferSize = 4u * 1024u * 1024u;
    if (!d.check(sceGxmInitialize(&init), "native_gxm_initialize")) return false;
    d.ownsGxm = true;
    if(d.slotFence) {
        d.notifications=sceGxmGetNotificationRegion();
        if(!d.notifications) {d.fail("native_notification_region");return abort();}
        // This renderer owns the only initialized GXM instance. Three words
        // belong exclusively to its frame slots until fenced shutdown.
        for(unsigned i=0;i<d.dynamic.size();++i)d.notifications[i]=0;
    }
    if (!d.check(d.vdm.allocate(SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE), "native_vdm") ||
        !d.check(d.vertexRing.allocate(SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE), "native_vertex_ring") ||
        !d.check(d.fragmentRing.allocate(SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE), "native_fragment_ring") ||
        !d.check(d.fragmentUsseRing.allocate(SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, false, Mapping::FragmentUsse), "native_fragment_usse_ring")) return abort();
    d.host = std::calloc(1, SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
    if (!d.host) { d.fail("native_context_host_memory"); return abort(); }
    SceGxmContextParams ctx{};
    ctx.hostMem=d.host; ctx.hostMemSize=SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
    ctx.vdmRingBufferMem=d.vdm.data; ctx.vdmRingBufferMemSize=d.vdm.bytes;
    ctx.vertexRingBufferMem=d.vertexRing.data; ctx.vertexRingBufferMemSize=d.vertexRing.bytes;
    ctx.fragmentRingBufferMem=d.fragmentRing.data; ctx.fragmentRingBufferMemSize=d.fragmentRing.bytes;
    ctx.fragmentUsseRingBufferMem=d.fragmentUsseRing.data; ctx.fragmentUsseRingBufferMemSize=d.fragmentUsseRing.bytes;
    ctx.fragmentUsseRingBufferOffset=d.fragmentUsseRing.usseOffset;
    if (!d.check(sceGxmCreateContext(&ctx, &d.context), "native_context")) return abort();
    SceGxmRenderTargetParams rt{};
    rt.width=kWidth; rt.height=kHeight; rt.scenesPerFrame=1;
    rt.multisampleMode=SCE_GXM_MULTISAMPLE_NONE; rt.driverMemBlock=-1;
    if (!d.check(sceGxmCreateRenderTarget(&rt, &d.target), "native_render_target")) return abort();
    for (auto& surface : d.surfaces) {
        if (!d.check(surface.memory.allocate(kStride*kHeight*4u, true), "native_display_memory")) return abort();
        std::memset(surface.memory.data, 0, surface.memory.bytes);
        if (!d.check(sceGxmColorSurfaceInit(&surface.color, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE, SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
            kWidth, kHeight, kStride, surface.memory.data), "native_display_surface") ||
            !d.check(sceGxmSyncObjectCreate(&surface.sync), "native_display_sync")) return abort();
    }
    if (!d.check(d.depth.allocate(kStride*kHeight*4u, true), "native_depth_memory") ||
        !d.check(sceGxmDepthStencilSurfaceInit(&d.depthSurface, SCE_GXM_DEPTH_STENCIL_FORMAT_DF32,
            SCE_GXM_DEPTH_STENCIL_SURFACE_TILED, kStride, d.depth.data, nullptr), "native_depth_surface")) return abort();
    sceGxmDepthStencilSurfaceSetBackgroundDepth(&d.depthSurface, 1.f);
    if (!d.check(d.patchBuffer.allocate(2u*1024u*1024u), "native_patch_buffer") ||
        !d.check(d.vertexUsse.allocate(256u*1024u, false, Mapping::VertexUsse), "native_vertex_usse") ||
        !d.check(d.fragmentUsse.allocate(4u*1024u*1024u, false, Mapping::FragmentUsse), "native_fragment_usse")) return abort();
    SceGxmShaderPatcherParams patch{};
    patch.hostAllocCallback=patch_alloc; patch.hostFreeCallback=patch_free;
    patch.bufferMem=d.patchBuffer.data; patch.bufferMemSize=d.patchBuffer.bytes;
    patch.vertexUsseMem=d.vertexUsse.data; patch.vertexUsseMemSize=d.vertexUsse.bytes;
    patch.vertexUsseOffset=d.vertexUsse.usseOffset;
    patch.fragmentUsseMem=d.fragmentUsse.data; patch.fragmentUsseMemSize=d.fragmentUsse.bytes;
    patch.fragmentUsseOffset=d.fragmentUsse.usseOffset;
    if (!d.check(sceGxmShaderPatcherCreate(&patch, &d.patcher), "native_shader_patcher") ||
        !d.check(shark_init(nullptr), "native_shader_compiler")) return abort();
    d.ownsCompiler=true;
    if (!d.compile(kVertexShader, SHARK_VERTEX_SHADER, d.vertexCode) ||
        !d.compile(kFragmentShader, SHARK_FRAGMENT_SHADER, d.fragmentCode)) return abort();
    // Only two fixed shaders compile at initialization. No shader compilation
    // or pipeline creation occurs in the match draw loop.
    shark_end(); d.ownsCompiler=false;
    const auto* vp=reinterpret_cast<const SceGxmProgram*>(d.vertexCode.data());
    const auto* fp=reinterpret_cast<const SceGxmProgram*>(d.fragmentCode.data());
    if (!d.check(sceGxmShaderPatcherRegisterProgram(d.patcher,vp,&d.vertexId), "native_register_vertex") ||
        !d.check(sceGxmShaderPatcherRegisterProgram(d.patcher,fp,&d.fragmentId), "native_register_fragment")) return abort();
    const auto* position=sceGxmProgramFindParameterByName(vp,"a_position");
    const auto* color=sceGxmProgramFindParameterByName(vp,"a_color");
    d.mvp=sceGxmProgramFindParameterByName(vp,"u_mvp");
    if (!position || !color || !d.mvp ||
        sceGxmProgramParameterGetCategory(position)!=SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ||
        sceGxmProgramParameterGetCategory(color)!=SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ||
        sceGxmProgramParameterGetCategory(d.mvp)!=SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
        d.fail("native_shader_reflection"); return abort();
    }
    SceGxmVertexAttribute attributes[2]{};
    attributes[0].componentCount=4; attributes[0].format=SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[0].regIndex=sceGxmProgramParameterGetResourceIndex(position);
    attributes[1].componentCount=4; attributes[1].format=SCE_GXM_ATTRIBUTE_FORMAT_U8N;
    attributes[1].offset=16; attributes[1].regIndex=sceGxmProgramParameterGetResourceIndex(color);
    SceGxmVertexStream stream{};
    stream.stride=kVertexStride; stream.indexSource=SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
    if (!d.check(sceGxmShaderPatcherCreateVertexProgram(d.patcher,d.vertexId,attributes,2,&stream,1,&d.vertexProgram), "native_patch_vertex")) return abort();
    for (unsigned blend=0;blend<8;++blend) for (unsigned mask=0;mask<4;++mask) {
        const auto info=blend_info(blend,mask);
        if (!d.check(sceGxmShaderPatcherCreateFragmentProgram(d.patcher,d.fragmentId,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,SCE_GXM_MULTISAMPLE_NONE,&info,vp,
            &d.fragments[blend*4u+mask]), "native_patch_fragment")) return abort();
    }
    if (!d.check(d.clearVertices.allocate(sizeof(kClearVertices)), "native_clear_vertices") ||
        !d.check(d.clearIndices.allocate(sizeof(kClearIndices)), "native_clear_indices")) return abort();
    std::memcpy(d.clearVertices.data,kClearVertices,sizeof(kClearVertices));
    std::memcpy(d.clearIndices.data,kClearIndices,sizeof(kClearIndices));
    d.initialized=true;
    return true;
}

bool Renderer::upload(const void* vertices, size_t vertexBytes, const uint16_t* indices,
                      size_t indexBytes, std::span<const Mesh> meshes) noexcept {
    if (!impl_) return false;
    auto& d=*impl_;
    if (!d.on_owner() || !d.initialized || d.frames.active() || !d.meshes.empty()) return d.fail("native_upload_lifecycle");
    if (!vertices || !indices || !vertexBytes || !indexBytes || meshes.empty()) return d.fail("native_upload_empty");
    for (const auto& mesh : meshes) {
        if (!valid_mesh(mesh,vertexBytes,indexBytes)) return d.fail("native_mesh_bounds");
        const auto start=mesh.indexOffset/2u;
        for (uint32_t n=0;n<mesh.indexCount;++n)
            if (indices[start+n]>=mesh.vertexCount) return d.fail("native_local_index");
    }
    if (!d.check(d.vertices.allocate(vertexBytes), "native_vertices") ||
        !d.check(d.indices.allocate(indexBytes), "native_indices")) return false;
    std::memcpy(d.vertices.data,vertices,vertexBytes);
    std::memcpy(d.indices.data,indices,indexBytes);
    d.meshes.assign(meshes.begin(),meshes.end());
    return true;
}

bool Renderer::upload_uvs(std::span<const uint8_t> vertices,std::span<const uint8_t> hasUv) noexcept {
    if(!impl_)return false;
    auto& d=*impl_;
    if(!d.on_owner()||!d.initialized||d.inScene||d.meshes.empty()||d.uvVertices.data||hasUv.size()!=d.meshes.size())return d.fail("native_uv_lifecycle");
    uint64_t endVertex=0;
    for(unsigned i=0;i<d.meshes.size();++i) {
        const auto& mesh=d.meshes[i];
        if(mesh.vertexOffset%kVertexStride||hasUv[i]>1)return d.fail("native_uv_mesh_layout");
        endVertex=std::max(endVertex,(uint64_t(mesh.vertexOffset)+mesh.vertexBytes)/kVertexStride);
    }
    if(endVertex*12u!=vertices.size()||vertices.size()>8u*1024u*1024u)return d.fail("native_uv_bounds");
    for(size_t i=0;i<vertices.size();i+=4) {
        float value=0;std::memcpy(&value,vertices.data()+i,4);
        if(!std::isfinite(value))return d.fail("native_uv_nonfinite");
    }
    if(!d.check(d.uvVertices.allocate(vertices.size()),"native_uv_memory"))return false;
    std::memcpy(d.uvVertices.data,vertices.data(),vertices.size());
    d.hasUv.assign(hasUv.begin(),hasUv.end());
    if(!d.check(shark_init(nullptr),"native_material_compiler"))return false;
    d.ownsCompiler=true;
    const bool compiled=d.compile(kTexturedVertexShader,SHARK_VERTEX_SHADER,d.texturedVertexCode)&&
                        d.compile(kTexturedFragmentShader,SHARK_FRAGMENT_SHADER,d.texturedFragmentCode);
    shark_end();d.ownsCompiler=false;
    if(!compiled)return false;
    const auto* vp=reinterpret_cast<const SceGxmProgram*>(d.texturedVertexCode.data());
    const auto* fp=reinterpret_cast<const SceGxmProgram*>(d.texturedFragmentCode.data());
    if(!d.check(sceGxmShaderPatcherRegisterProgram(d.patcher,vp,&d.texturedVertexId),"native_material_vertex_register")||
       !d.check(sceGxmShaderPatcherRegisterProgram(d.patcher,fp,&d.texturedFragmentId),"native_material_fragment_register"))return false;
    const auto *position=sceGxmProgramFindParameterByName(vp,"a_position"),
               *color=sceGxmProgramFindParameterByName(vp,"a_color"),
               *uv=sceGxmProgramFindParameterByName(vp,"a_uv");
    d.texturedMvp=sceGxmProgramFindParameterByName(vp,"u_mvp");
    d.tint=sceGxmProgramFindParameterByName(fp,"u_tint");
    d.controls=sceGxmProgramFindParameterByName(fp,"u_controls");
    const auto* sampler=sceGxmProgramFindParameterByName(fp,"u_texture");
    if(!position||!color||!uv||!d.texturedMvp||!d.tint||!d.controls||!sampler||
       sceGxmProgramParameterGetResourceIndex(sampler)!=0||
       sceGxmProgramParameterGetCategory(d.texturedMvp)!=SCE_GXM_PARAMETER_CATEGORY_UNIFORM||
       sceGxmProgramParameterGetCategory(d.tint)!=SCE_GXM_PARAMETER_CATEGORY_UNIFORM||
       sceGxmProgramParameterGetCategory(d.controls)!=SCE_GXM_PARAMETER_CATEGORY_UNIFORM)return d.fail("native_material_reflection");
    SceGxmVertexAttribute attributes[3]{};
    attributes[0].componentCount=4;attributes[0].format=SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[0].regIndex=sceGxmProgramParameterGetResourceIndex(position);
    attributes[1].componentCount=4;attributes[1].format=SCE_GXM_ATTRIBUTE_FORMAT_U8N;
    attributes[1].offset=16;attributes[1].regIndex=sceGxmProgramParameterGetResourceIndex(color);
    attributes[2].componentCount=3;attributes[2].format=SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[2].streamIndex=1;attributes[2].regIndex=sceGxmProgramParameterGetResourceIndex(uv);
    SceGxmVertexStream streams[2]{};
    streams[0].stride=kVertexStride;streams[1].stride=12;
    streams[0].indexSource=streams[1].indexSource=SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
    if(!d.check(sceGxmShaderPatcherCreateVertexProgram(d.patcher,d.texturedVertexId,attributes,3,streams,2,&d.texturedVertexProgram),"native_material_vertex_patch"))return false;
    for(unsigned blend=0;blend<8;++blend)for(unsigned mask=0;mask<4;++mask) {
        const auto info=blend_info(blend,mask);
        if(!d.check(sceGxmShaderPatcherCreateFragmentProgram(d.patcher,d.texturedFragmentId,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,SCE_GXM_MULTISAMPLE_NONE,&info,vp,
            &d.texturedFragments[blend*4+mask]),"native_material_fragment_patch"))return false;
    }
    return true;
}

uint32_t Renderer::create_texture(const TextureUpload& image) noexcept {
    if(!impl_)return 0;
    auto& d=*impl_;
    const auto expected=texture_upload_bytes(image);
    if(!d.on_owner()||!d.initialized||!expected||expected!=image.pixels.size())return 0;
    const auto allocated=(expected+4095u)&~size_t(4095u);
    if(allocated>64u*1024u*1024u-d.textureBytes||d.textures.size()>=4096)return 0;
    auto texture=std::unique_ptr<Impl::Texture>(new(std::nothrow)Impl::Texture);
    if(!texture||!d.check(texture->memory.allocate(expected),"native_texture_memory"))return 0;
    std::memcpy(texture->memory.data,image.pixels.data(),expected);
    constexpr SceGxmTextureFormat formats[]{SCE_GXM_TEXTURE_FORMAT_U8_RRRR,
        SCE_GXM_TEXTURE_FORMAT_A8L8,SCE_GXM_TEXTURE_FORMAT_R5G6B5,
        SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR};
    const auto format=formats[uint32_t(image.storage)-1u];
    const int rc=image.swizzled?
        sceGxmTextureInitSwizzled(&texture->descriptor,texture->memory.data,format,image.width,image.height,image.mipCount):
        sceGxmTextureInitLinear(&texture->descriptor,texture->memory.data,format,image.width,image.height,image.mipCount);
    if(!d.check(rc,"native_texture_descriptor"))return 0;
    if(image.storage==TextureStorage::Rgba8){for(size_t i=3;i<image.pixels.size();i+=4)texture->minAlpha=std::min(texture->minAlpha,image.pixels[i]);}
    else if(image.storage==TextureStorage::Intensity8){for(uint8_t a:image.pixels)texture->minAlpha=std::min(texture->minAlpha,a);}
    else if(image.storage==TextureStorage::LuminanceAlpha8){for(size_t i=1;i<image.pixels.size();i+=2)texture->minAlpha=std::min(texture->minAlpha,image.pixels[i]);}
    else if(image.storage==TextureStorage::Bc1){for(size_t i=0;i<image.pixels.size();i+=8){const auto* p=image.pixels.data()+i;const unsigned c0=p[0]|unsigned(p[1])<<8,c1=p[2]|unsigned(p[3])<<8;if(c0<=c1){const uint32_t selectors=p[4]|uint32_t(p[5])<<8|uint32_t(p[6])<<16|uint32_t(p[7])<<24;for(unsigned n=0;n<16;++n)if(((selectors>>(n*2))&3)==3)texture->minAlpha=0;}}}
    texture->mipCount=image.mipCount;texture->swizzled=image.swizzled;texture->width=image.width;texture->height=image.height;texture->stride=image.stride;d.textureBytes+=texture->memory.bytes;
    for(unsigned id=0;id<d.textures.size();++id)if(!d.textures[id]){d.textures[id]=std::move(texture);return id+1;}
    d.textures.push_back(std::move(texture));
    return static_cast<uint32_t>(d.textures.size());
}

uint32_t Renderer::create_target(uint32_t width,uint32_t height) noexcept {
 if(!impl_||!width||!height||width>960||height>544)return 0;auto& d=*impl_;if(!d.on_owner())return 0;
 TextureUpload image;image.width=width;image.height=height;image.stride=(width+7u)&~7u;image.mipCount=1;image.storage=TextureStorage::Rgba8;image.pixels.resize(size_t(image.stride)*height*4);
 uint32_t id=create_texture(image);if(!id)return 0;auto& t=*d.textures[id-1];
 SceGxmRenderTargetParams rt{};rt.width=width;rt.height=height;rt.scenesPerFrame=1;rt.multisampleMode=SCE_GXM_MULTISAMPLE_NONE;rt.driverMemBlock=-1;
 const unsigned stride=(width+31u)&~31u,heightAligned=(height+31u)&~31u;
 if(!d.check(sceGxmCreateRenderTarget(&rt,&t.target),"native_offscreen_target")||!d.check(t.targetDepth.allocate(size_t(stride)*heightAligned*4),"native_offscreen_depth")||!d.check(sceGxmColorSurfaceInit(&t.color,SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,SCE_GXM_COLOR_SURFACE_LINEAR,SCE_GXM_COLOR_SURFACE_SCALE_NONE,SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,width,height,t.stride,t.memory.data),"native_offscreen_color")||!d.check(sceGxmDepthStencilSurfaceInit(&t.depth,SCE_GXM_DEPTH_STENCIL_FORMAT_DF32,SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,stride,t.targetDepth.data,nullptr),"native_offscreen_depth_surface"))return 0;
 sceGxmDepthStencilSurfaceSetBackgroundDepth(&t.depth,1);sceGxmDepthStencilSurfaceSetForceStoreMode(&t.depth,SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);return id;
}
bool Renderer::select_target(uint32_t id,bool clearColor,bool clearDepth) noexcept {
 if(!impl_)return false;auto& d=*impl_;if(!d.on_owner()||!d.frames.active())return false;
 if(id&&(id>d.textures.size()||!d.textures[id-1]||!d.textures[id-1]->target))return d.fail("native_target_identity");
 if(d.inScene&&id==d.currentTarget){if(clearColor)return clear_color({0,0,0,0},clearDepth);if(clearDepth)return clear_depth();return true;}
 if(d.inScene){if(!d.check(sceGxmEndScene(d.context,nullptr,nullptr),"native_target_end"))return false;d.inScene=false;const uint64_t wait=d.timings?sceKernelGetSystemTimeWide():0;sceGxmFinish(d.context);if(d.timings)d.currentTiming.fenceUs+=sceKernelGetSystemTimeWide()-wait;}
 SceGxmRenderTarget* rt=d.target;SceGxmColorSurface* color=&d.surfaces[d.frames.back()].color;SceGxmDepthStencilSurface* depth=&d.depthSurface;SceGxmSyncObject* sync=d.surfaces[d.frames.back()].sync;
 if(id){auto& t=*d.textures[id-1];rt=t.target;color=&t.color;depth=&t.depth;sync=nullptr;}
 sceGxmDepthStencilSurfaceSetForceLoadMode(depth,clearDepth?SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED:SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED);
 sceGxmDepthStencilSurfaceSetForceStoreMode(depth,SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
 if(!d.check(sceGxmBeginScene(d.context,0,rt,nullptr,nullptr,sync,color,depth),"native_target_begin"))return false;
 d.inScene=true;d.currentTarget=id;d.boundTextures={};
 sceGxmSetViewportEnable(d.context,SCE_GXM_VIEWPORT_ENABLED);sceGxmSetRegionClip(d.context,SCE_GXM_REGION_CLIP_NONE,0,0,kWidth-1,kHeight-1);
 sceGxmSetTwoSidedEnable(d.context,SCE_GXM_TWO_SIDED_ENABLED);sceGxmSetFrontPolygonMode(d.context,SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);sceGxmSetBackPolygonMode(d.context,SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
 sceGxmSetFrontFragmentProgramEnable(d.context,SCE_GXM_FRAGMENT_PROGRAM_ENABLED);sceGxmSetBackFragmentProgramEnable(d.context,SCE_GXM_FRAGMENT_PROGRAM_ENABLED);
 sceGxmSetFrontDepthBias(d.context,0,0);sceGxmSetBackDepthBias(d.context,0,0);
 sceGxmSetFrontStencilFunc(d.context,SCE_GXM_STENCIL_FUNC_ALWAYS,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,0xff,0);sceGxmSetBackStencilFunc(d.context,SCE_GXM_STENCIL_FUNC_ALWAYS,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,0xff,0);sceGxmSetFrontStencilRef(d.context,0);sceGxmSetBackStencilRef(d.context,0);
 return !clearColor||clear_color({0,0,0,0},clearDepth);
}
bool Renderer::clear_color(std::array<float,4> color,bool depth) noexcept {
 if(!impl_||!impl_->inScene)return false;auto& d=*impl_;ModelVertex vertices[3];vertices[0].position={-1,-1,0,1};vertices[1].position={3,-1,0,1};vertices[2].position={-1,3,0,1};for(auto& v:vertices)for(unsigned c=0;c<4;++c)v.color[c]=uint8_t(std::clamp(color[c],0.f,1.f)*255);
 const uint16_t indices[]{0,1,2};Raster r;r.depthFunction=0;r.depthWrite=depth;r.writeAlpha=true;SceneMaterial m;
 if(d.currentTarget){const auto& t=*d.textures[d.currentTarget-1];m.viewport={0,0,float(t.width),float(t.height)};m.scissor={0,0,float(t.width),float(t.height)};}
 return draw_scene(0,vertices,indices,kIdentity,r,m);
}
bool Renderer::copy_texture(uint32_t destination,std::array<float,4> rect,bool intensity) noexcept {
 const TextureCopy request{destination,rect,0,intensity};return copy_textures({&request,1});
}
bool Renderer::copy_textures(std::span<const TextureCopy> copies) noexcept {
 if(!impl_)return false;auto& d=*impl_;if(copies.empty())return true;if(!d.inScene||!d.on_owner())return false;
 const uint32_t source=d.currentTarget;
 for(const auto& request:copies)if(!request.destination||request.destination==source||request.destination>d.textures.size()||!d.textures[request.destination-1]||!d.textures[request.destination-1]->target)return false;
 const unsigned width=source?d.textures[source-1]->width:kWidth,height=source?d.textures[source-1]->height:kHeight;
 SceGxmTexture input{};if(source)input=d.textures[source-1]->descriptor;else if(!d.check(sceGxmTextureInitLinear(&input,d.surfaces[d.frames.back()].memory.data,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,kWidth,kHeight,1),"native_copy_source"))return false;
 // Finish the atlas once. Each destination retires before switching; the main
 // depth/color surfaces resume once after all copies, preserving draw order.
 if(!d.check(sceGxmEndScene(d.context,nullptr,nullptr),"native_copy_source_end"))return false;d.inScene=false;
 const uint64_t wait=d.timings?sceKernelGetSystemTimeWide():0;sceGxmFinish(d.context);if(d.timings)d.currentTiming.fenceUs+=sceKernelGetSystemTimeWide()-wait;
 auto holder=std::unique_ptr<Impl::Texture>(new(std::nothrow)Impl::Texture);if(!holder)return false;holder->descriptor=input;holder->mipCount=1;holder->width=width;holder->height=height;
 const unsigned inputId=d.textures.size()+1;d.textures.push_back(std::move(holder));bool ok=true;
 for(const auto& request:copies){
  if(!select_target(request.destination,true,true)){ok=false;break;}
  ModelVertex v[4];v[0].position={-1,1,0,1};v[1].position={1,1,0,1};v[2].position={1,-1,0,1};v[3].position={-1,-1,0,1};
  const auto& rect=request.sourceRect;float x=rect[0]/width,y=rect[1]/height,x2=(rect[0]+rect[2])/width,y2=(rect[1]+rect[3])/height;
  v[0].uv={x,y,1};v[1].uv={x2,y,1};v[2].uv={x2,y2,1};v[3].uv={x,y2,1};const uint16_t indices[]{0,1,2,0,2,3};
  SceneMaterial material;const auto& t=*d.textures[request.destination-1];const float h=request.destinationHeight?std::min(request.destinationHeight,t.height):t.height;
  material.viewport={0,0,float(t.width),h};material.scissor={0,0,float(t.width),h};material.controls={1,-1,1,request.intensity?3.f:0.f};material.textures[0]=inputId;material.wrapU[0]=material.wrapV[0]=1;
  Raster raster;raster.depthTest=raster.depthWrite=false;raster.writeAlpha=true;
  if(!draw_scene(0,v,indices,kIdentity,raster,material)){ok=false;break;}
 }
 if(!select_target(source))ok=false;d.textures.pop_back();return ok;
}
bool Renderer::initialize_scene_program() noexcept {
 if(!impl_)return false;auto& d=*impl_;auto& p=d.scene;
 if(p.vertex)return true;
 if(!d.on_owner()||!d.initialized||d.inScene)return false;
 if(!d.check(shark_init(nullptr),"native_scene_compiler"))return false;d.ownsCompiler=true;
 const bool ok=d.compile(kSceneVertexShader,SHARK_VERTEX_SHADER,p.vertexCode)&&d.compile(kSceneFragmentShader,SHARK_FRAGMENT_SHADER,p.fragmentCode);
 shark_end();d.ownsCompiler=false;if(!ok)return false;
 const auto* vp=reinterpret_cast<const SceGxmProgram*>(p.vertexCode.data());const auto* fp=reinterpret_cast<const SceGxmProgram*>(p.fragmentCode.data());
 if(!d.check(sceGxmShaderPatcherRegisterProgram(d.patcher,vp,&p.vertexId),"native_scene_vertex_register")||!d.check(sceGxmShaderPatcherRegisterProgram(d.patcher,fp,&p.fragmentId),"native_scene_fragment_register"))return false;
 const char* names[]{"a_position","a_color","a_normal","a_uv0","a_uv1","a_uv2","a_uv3","a_uv4","a_uv5","a_bone"};
 const unsigned offsets[]{0,16,32,20,44,56,68,80,92,104},counts[]{4,4,3,3,3,3,3,3,3,1};
 SceGxmVertexAttribute attrs[10]{};
 for(unsigned i=0;i<10;++i){const auto* a=sceGxmProgramFindParameterByName(vp,names[i]);if(!a)return d.fail(names[i]);attrs[i].offset=offsets[i];attrs[i].componentCount=counts[i];attrs[i].format=i==1?SCE_GXM_ATTRIBUTE_FORMAT_U8N:SCE_GXM_ATTRIBUTE_FORMAT_F32;attrs[i].regIndex=sceGxmProgramParameterGetResourceIndex(a);}
 SceGxmVertexStream stream{};stream.stride=sizeof(ModelVertex);stream.indexSource=SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
 if(!d.check(sceGxmShaderPatcherCreateVertexProgram(d.patcher,p.vertexId,attrs,10,&stream,1,&p.vertex),"native_scene_vertex_patch"))return false;
 for(unsigned blend=0;blend<8;++blend)for(unsigned mask=0;mask<4;++mask){const auto b=blend_info(blend,mask);if(!d.check(sceGxmShaderPatcherCreateFragmentProgram(d.patcher,p.fragmentId,SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,SCE_GXM_MULTISAMPLE_NONE,&b,vp,&p.fragments[blend*4+mask]),"native_scene_fragment_patch"))return false;}
 #define PARAM(member,program,name) p.member=sceGxmProgramFindParameterByName(program,name);if(!p.member)return d.fail(name)
 PARAM(mvp,vp,"u_mvp");PARAM(mv,vp,"u_mv");PARAM(bones,vp,"u_bones");PARAM(ambient,vp,"u_ambient");PARAM(diffuse,vp,"u_diffuse");PARAM(lighting,vp,"u_lighting");PARAM(lightpos,vp,"u_lightpos");PARAM(lightcolor,vp,"u_lightcolor");PARAM(specular,vp,"u_specular");PARAM(specularcolor,vp,"u_specularcolor");
 PARAM(environment,vp,"u_environment");
 PARAM(controls,fp,"u_controls");PARAM(effects,fp,"u_effects");PARAM(tint,fp,"u_tint");PARAM(shadow,fp,"u_shadow");PARAM(shadowadd,fp,"u_shadowadd");PARAM(fog,fp,"u_fog");PARAM(fogcolor,fp,"u_fogcolor");PARAM(scissor,fp,"u_scissor");
 #undef PARAM
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
 if(!d.check(shark_init(nullptr),"native_material_warm_compiler"))return false;d.ownsCompiler=true;
 constexpr unsigned configs[]{0,1,3,5,7,9,13,17,19,21,25,33,35,37,39,41,43,49,53,57};
 for(unsigned mask:configs)for(unsigned alpha=0;alpha<2;++alpha)for(unsigned clip=0;clip<2;++clip){
   // Exact scissors are principally UI/portrait views. The generic native
   // shader continues to cover other combinations without dropping draws.
   if(clip&&mask!=0&&mask!=1&&mask!=33)continue;
   auto variant=std::unique_ptr<Impl::SceneFragment>(new(std::nothrow)Impl::SceneFragment);if(!variant)return false;
   std::string shader=kSceneFragmentShader;
   const auto replace=[&](const std::string& before,const std::string& after){const auto position=shader.find(before);if(position!=std::string::npos)shader.replace(position,before.size(),after);};
   replace("float mask=u_controls.x;","float mask="+std::to_string(mask)+".0;");
   if(!alpha)replace(" if(u_controls.y>=0.0&&c.a<=u_controls.y)discard;","");
   if(!clip)replace(" if(pixel.x<u_scissor.x||pixel.y<u_scissor.y||pixel.x>=u_scissor.z||pixel.y>=u_scissor.w)discard;","");
   replace(" if(u_controls.w==2.0){float y=d.r,u=detail.r-0.5,v=sh.r-0.5;c=float4(y+1.402*v,y-0.344136*u-0.714136*v,y+1.772*u,1.0);}","");
   replace(" if(u_controls.w==3.0)c=float4(c.r,c.r,c.r,c.r);","");
   if(!d.compile(shader.c_str(),SHARK_FRAGMENT_SHADER,variant->code))return false;
   const auto* fp=reinterpret_cast<const SceGxmProgram*>(variant->code.data());
   if(!d.check(sceGxmShaderPatcherRegisterProgram(d.patcher,fp,&variant->id),"native_material_warm_register"))return false;
   for(unsigned blend=0;blend<8;++blend)for(unsigned write=0;write<4;++write){const auto info=blend_info(blend,write);if(!d.check(sceGxmShaderPatcherCreateFragmentProgram(d.patcher,variant->id,SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,SCE_GXM_MULTISAMPLE_NONE,&info,vp,&variant->fragments[blend*4+write]),"native_material_warm_patch"))return false;}
   #define OPTIONAL(member) variant->member=sceGxmProgramFindParameterByName(fp,"u_" #member)
   OPTIONAL(controls);OPTIONAL(effects);OPTIONAL(tint);OPTIONAL(shadow);OPTIONAL(shadowadd);OPTIONAL(fog);OPTIONAL(fogcolor);OPTIONAL(scissor);
   #undef OPTIONAL
   d.sceneFragments[mask|(alpha<<6)|(clip<<7)]=std::move(variant);
 }
 shark_end();d.ownsCompiler=false;
#endif
 TextureUpload white;white.width=white.height=white.stride=white.mipCount=1;white.storage=TextureStorage::Rgba8;white.swizzled=true;white.pixels={255,255,255,255};
 d.whiteTexture=create_texture(white);return d.whiteTexture!=0;
}
uint32_t Renderer::create_model(std::span<const ModelVertex> v,std::span<const uint16_t> i) noexcept {
 if(!impl_||v.empty()||v.size()>64000||i.empty()||i.size()%3||i.size()>192000)return 0;auto& d=*impl_;
 if(!d.on_owner()||!d.initialized)return 0;for(auto index:i)if(index>=v.size())return 0;
 const size_t bytes=((v.size_bytes()+4095)&~size_t(4095))+((i.size_bytes()+4095)&~size_t(4095));
 if(bytes>48u*1024u*1024u-d.modelBytes)return 0;
 auto m=std::unique_ptr<Impl::ResidentModel>(new(std::nothrow)Impl::ResidentModel);
 if(!m||!d.check(m->vertex.allocate(v.size_bytes()),"native_model_vertex_memory")||!d.check(m->index.allocate(i.size_bytes()),"native_model_index_memory"))return 0;
 std::memcpy(m->vertex.data,v.data(),v.size_bytes());std::memcpy(m->index.data,i.data(),i.size_bytes());for(const auto& vertex:v)m->minAlpha=std::min(m->minAlpha,vertex.color[3]);m->count=i.size();d.modelBytes+=bytes;
 for(unsigned id=0;id<d.models.size();++id)if(!d.models[id]){d.models[id]=std::move(m);return id+1;}
 d.models.push_back(std::move(m));return d.models.size();
}
void Renderer::release_model(uint32_t id) noexcept {
 if(!impl_||!id)return;auto& d=*impl_;if(!d.on_owner()||id>d.models.size()||!d.models[id-1])return;
 auto& m=d.models[id-1];d.modelBytes-=m->vertex.bytes+m->index.bytes;d.retiredModels.push_back(std::move(m));
}
void Renderer::release_texture(uint32_t id) noexcept {
 if(!impl_||!id)return;auto& d=*impl_;if(!d.on_owner()||id>d.textures.size()||!d.textures[id-1]||id==d.whiteTexture)return;
 auto& t=d.textures[id-1];d.boundTextures={};d.textureBytes-=t->memory.bytes;d.retiredTextures.push_back(std::move(t));
}
bool Renderer::draw_scene(uint32_t id,std::span<const ModelVertex> v,std::span<const uint16_t> indices,
 const std::array<float,16>& matrix,const Raster& r,const SceneMaterial& m) noexcept {
 if(!impl_)return false;auto& d=*impl_;auto& p=d.scene;
 if(!d.on_owner()||!d.inScene||!p.vertex||!r.valid()||!finite_mvp(matrix)||d.error.load()<0)return false;
 if(r.cull==3||m.scissor[2]<=m.scissor[0]||m.scissor[3]<=m.scissor[1])return true;
 struct DrawTiming {Impl& d;uint64_t begin;~DrawTiming(){if(d.timings){d.currentTiming.drawUs+=sceKernelGetSystemTimeWide()-begin;++d.currentTiming.draws;}}} timing{d,d.timings?uint64_t(sceKernelGetSystemTimeWide()):0};
 const void* vertex=nullptr;const void* index=nullptr;unsigned count=0;
 if(id){if(id>d.models.size()||!d.models[id-1])return false;const auto& resident=*d.models[id-1];vertex=resident.vertex.data;index=resident.index.data;count=resident.count;}
 else {
  if(v.empty()||v.size()>64000||indices.empty()||indices.size()%3||indices.size()>192000)return false;for(auto i:indices)if(i>=v.size())return false;
  constexpr size_t vertexLimit=8u*1024u*1024u,indexLimit=2u*1024u*1024u;const unsigned slot=d.frames.back();
  if(v.size_bytes()>vertexLimit-d.sceneVertexUsed||indices.size_bytes()>indexLimit-d.sceneIndexUsed)return d.fail("native_scene_frame_capacity");
  auto& vb=d.sceneFrameVertex[slot];auto& ib=d.sceneFrameIndex[slot];
  if(!vb.data&&!d.check(vb.allocate(vertexLimit),"native_scene_frame_vertices"))return false;
  if(!ib.data&&!d.check(ib.allocate(indexLimit),"native_scene_frame_indices"))return false;
  auto* target=static_cast<uint8_t*>(vb.data)+d.sceneVertexUsed;auto* elements=static_cast<uint8_t*>(ib.data)+d.sceneIndexUsed;
  std::memcpy(target,v.data(),v.size_bytes());std::memcpy(elements,indices.data(),indices.size_bytes());vertex=target;index=elements;count=indices.size();
  d.sceneVertexUsed+=v.size_bytes();d.sceneIndexUsed+=indices.size_bytes();if(d.timings)d.currentTiming.dynamicVertexBytes+=v.size_bytes();d.dynamic[slot].gpuUsed=true;
 }
 const unsigned mask=unsigned(m.controls[0]);
 const float width=d.currentTarget?float(d.textures[d.currentTarget-1]->width):float(kWidth);
 const float height=d.currentTarget?float(d.textures[d.currentTarget-1]->height):float(kHeight);
 const bool partial=m.scissor[0]>0||m.scissor[1]>0||m.scissor[2]<width||m.scissor[3]<height;
 uint8_t vertexAlpha=255;if(id)vertexAlpha=d.models[id-1]->minAlpha;else for(const auto& vertex:v)vertexAlpha=std::min(vertexAlpha,vertex.color[3]);
 float minimumAlpha=m.effects[1]>=0?m.effects[1]:vertexAlpha/255.f;
 const auto textureAlpha=[&](unsigned unit){const auto id=m.textures[unit];return id&&id<=d.textures.size()&&d.textures[id-1]?d.textures[id-1]->minAlpha/255.f:1.f;};
 if(mask&2)minimumAlpha*=std::min(textureAlpha(0),textureAlpha(1));else if(mask&1)minimumAlpha*=textureAlpha(0);
 const bool alphaTest=m.controls[1]>=0&&(m.environment[2]>0||minimumAlpha<=m.controls[1]);
 const unsigned shaderKey=mask|(unsigned(alphaTest)<<6)|(unsigned(partial)<<7);
 const auto* variant=m.controls[3]<2&&shaderKey<256?d.sceneFragments[shaderKey].get():nullptr;
 sceGxmSetVertexProgram(d.context,p.vertex);sceGxmSetFragmentProgram(d.context,variant?variant->fragments[r.fragment_slot()]:p.fragments[r.fragment_slot()]);
 constexpr SceGxmDepthFunc funcs[]{SCE_GXM_DEPTH_FUNC_ALWAYS,SCE_GXM_DEPTH_FUNC_LESS_EQUAL,SCE_GXM_DEPTH_FUNC_EQUAL,SCE_GXM_DEPTH_FUNC_LESS};
 const auto f=r.depthTest?funcs[r.depthFunction]:SCE_GXM_DEPTH_FUNC_ALWAYS;const auto w=r.depthWrite?SCE_GXM_DEPTH_WRITE_ENABLED:SCE_GXM_DEPTH_WRITE_DISABLED;
 sceGxmSetFrontDepthFunc(d.context,f);sceGxmSetBackDepthFunc(d.context,f);sceGxmSetFrontDepthWriteEnable(d.context,w);sceGxmSetBackDepthWriteEnable(d.context,w);
 sceGxmSetCullMode(d.context,r.cull==0?SCE_GXM_CULL_NONE:r.cull==2?SCE_GXM_CULL_CCW:SCE_GXM_CULL_CW);
 sceGxmSetViewport(d.context,m.viewport[0]+m.viewport[2]*.5f,m.viewport[2]*.5f,m.viewport[1]+m.viewport[3]*.5f,-m.viewport[3]*.5f,.5f,.5f);
 sceGxmSetRegionClip(d.context,SCE_GXM_REGION_CLIP_NONE,0,0,kWidth-1,kHeight-1);
 for(unsigned unit=0;unit<6;++unit){if(variant&&!(mask&(1u<<unit)))continue;unsigned texture=m.textures[unit]?m.textures[unit]:d.whiteTexture;if(texture>d.textures.size()||!d.textures[texture-1])return d.fail("native_scene_texture_identity");
  const auto& t=*d.textures[texture-1];
  const uint64_t samplerKey=uint64_t(texture)|(uint64_t(m.wrapU[unit])<<32)|(uint64_t(m.wrapV[unit])<<33)|(uint64_t(m.filter[unit])<<34);
  if(d.boundTextures[unit]==samplerKey)continue;
  SceGxmTexture desc=t.descriptor;const auto filter=m.filter[unit]?SCE_GXM_TEXTURE_FILTER_LINEAR:SCE_GXM_TEXTURE_FILTER_POINT;
  if(!d.check(sceGxmTextureSetUAddrMode(&desc,m.wrapU[unit]||!t.swizzled?SCE_GXM_TEXTURE_ADDR_CLAMP:SCE_GXM_TEXTURE_ADDR_REPEAT),"native_scene_wrap_u")||!d.check(sceGxmTextureSetVAddrMode(&desc,m.wrapV[unit]||!t.swizzled?SCE_GXM_TEXTURE_ADDR_CLAMP:SCE_GXM_TEXTURE_ADDR_REPEAT),"native_scene_wrap_v")||!d.check(sceGxmTextureSetMinFilter(&desc,filter),"native_scene_min_filter")||!d.check(sceGxmTextureSetMagFilter(&desc,filter),"native_scene_mag_filter")||!d.check(sceGxmTextureSetMipFilter(&desc,t.mipCount>1?SCE_GXM_TEXTURE_MIP_FILTER_ENABLED:SCE_GXM_TEXTURE_MIP_FILTER_DISABLED),"native_scene_mips")||!d.check(sceGxmSetFragmentTexture(d.context,unit,&desc),"native_scene_bind_texture"))return false;
  d.boundTextures[unit]=samplerKey;
 }
 void* vu=nullptr;void* fu=nullptr;if(!d.check(sceGxmReserveVertexDefaultUniformBuffer(d.context,&vu),"native_scene_vertex_uniforms")||!d.check(sceGxmReserveFragmentDefaultUniformBuffer(d.context,&fu),"native_scene_fragment_uniforms"))return false;
 const float lighting[]{float(m.lit),m.effects[1],float(m.controls[3]==1),m.effects[3]};
 #define V(member,n,data) if(!d.check(sceGxmSetUniformDataF(vu,p.member,0,n,data),"native_scene_" #member))return false
 V(environment,4,m.environment.data());V(mvp,16,matrix.data());V(mv,16,m.modelView.data());V(bones,120,m.bones[0].data());V(ambient,4,m.ambient.data());V(diffuse,4,m.diffuse.data());V(lighting,4,lighting);V(lightpos,32,m.lightPosition[0].data());V(lightcolor,32,m.lightColor[0].data());V(specular,4,m.specular.data());V(specularcolor,4,m.specularColor.data());
 #undef V
 #define F(member,data) {const auto* parameter=variant?variant->member:p.member;if(parameter&&!d.check(sceGxmSetUniformDataF(fu,parameter,0,4,data),"native_scene_" #member))return false;}
 F(controls,m.controls.data());F(effects,m.effects.data());F(tint,m.tint.data());F(shadow,m.shadow.data());F(shadowadd,m.shadowAdd.data());F(fog,m.fog.data());F(fogcolor,m.fogColor.data());F(scissor,m.scissor.data());
 #undef F
 return d.check(sceGxmSetVertexStream(d.context,0,vertex),"native_scene_vertices")&&d.check(sceGxmDraw(d.context,SCE_GXM_PRIMITIVE_TRIANGLES,SCE_GXM_INDEX_FORMAT_U16,index,count),"native_scene_draw");
}
bool Renderer::clear_depth() noexcept {
 if(!impl_||!impl_->inScene)return false;auto& d=*impl_;
 sceGxmSetViewport(d.context,kWidth*.5f,kWidth*.5f,float(kHeight)*.5f,-float(kHeight)*.5f,.5f,.5f);Raster r;r.depthFunction=0;r.writeRgb=r.writeAlpha=false;
 return d.bind_and_draw(d.clearVertices.data,d.clearIndices.data,3,kIdentity,r);
}
bool Renderer::finish() noexcept {
 if(!impl_)return false;auto& d=*impl_;if(!d.on_owner())return false;const bool resume=d.inScene;
 if(resume){if(!d.check(sceGxmEndScene(d.context,nullptr,nullptr),"native_fence_end"))return false;d.inScene=false;}
 sceGxmFinish(d.context);
 if(resume)return select_target(d.currentTarget);

 return true;
}

bool Renderer::frame_active() const noexcept{return impl_&&impl_->frames.active();}
bool Renderer::begin_frame() noexcept {
    if (!impl_) return false;
    auto& d=*impl_;
    if (!d.on_owner() || !d.initialized || d.error.load()<0 || !d.frames.begin()) return d.fail("native_begin_frame_lifecycle");
    auto& dynamic=d.dynamic[d.frames.back()];
    d.currentTiming={};
    // Retire the actual slot before any CPU write to its mapped arrays.
    // The fragment notification proves completion of that scene, while
    // allowing newer scenes to remain in flight. The original global fence
    // remains available as the control and as the default.
    if(dynamic.gpuUsed) {
        const uint64_t start=d.timings?sceKernelGetSystemTimeWide():0;
        if(d.slotFence) {
            const SceGxmNotification retired{d.notifications+d.frames.back(),dynamic.retireValue};
            if(*retired.address!=retired.value) {
                if(!d.check(sceGxmNotificationWait(&retired),"native_dynamic_slot_wait"))return false;
                if(d.timings)++d.currentTiming.slotWaits;
            }
            std::atomic_thread_fence(std::memory_order_acquire);
        } else {
            sceGxmFinish(d.context);
            if(d.timings)++d.currentTiming.globalFinishes;
        }
        if(d.timings)d.currentTiming.fenceUs=sceKernelGetSystemTimeWide()-start;
    }
    if(!d.retiredModels.empty()||!d.retiredTextures.empty()) {
        sceGxmFinish(d.context);d.retiredModels.clear();d.retiredTextures.clear();
    }
    d.sceneVertexUsed=d.sceneIndexUsed=0;
    dynamic.vertexUsed=dynamic.indexUsed=0;
    dynamic.gpuUsed=false;
    if (!d.check(d.displayError.load(std::memory_order_relaxed), "native_display_callback")) return false;
    auto& back=d.surfaces[d.frames.back()];
    sceGxmDepthStencilSurfaceSetForceLoadMode(&d.depthSurface,SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED);
    sceGxmDepthStencilSurfaceSetForceStoreMode(&d.depthSurface,SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
    if (!d.check(sceGxmBeginScene(d.context,0,d.target,nullptr,nullptr,back.sync,&back.color,&d.depthSurface), "native_begin_scene")) return false;
    d.inScene=true;d.currentTarget=0;d.boundTextures={};
    // Re-establish all used context state after EVERY BeginScene, including
    // after suspend/resume. Partial scissors/auxiliary views are not admitted.
    sceGxmSetViewportEnable(d.context,SCE_GXM_VIEWPORT_ENABLED);
    sceGxmSetViewport(d.context,float(kWidth)*.5f,float(kWidth)*.5f,
                      float(kHeight)*.5f,-float(kHeight)*.5f,.5f,.5f);
    sceGxmSetRegionClip(d.context,SCE_GXM_REGION_CLIP_NONE,0,0,kWidth-1,kHeight-1);
    sceGxmSetTwoSidedEnable(d.context,SCE_GXM_TWO_SIDED_ENABLED);
    sceGxmSetFrontPolygonMode(d.context,SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
    sceGxmSetBackPolygonMode(d.context,SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
    sceGxmSetFrontFragmentProgramEnable(d.context,SCE_GXM_FRAGMENT_PROGRAM_ENABLED);
    sceGxmSetBackFragmentProgramEnable(d.context,SCE_GXM_FRAGMENT_PROGRAM_ENABLED);
    sceGxmSetFrontDepthBias(d.context,0,0); sceGxmSetBackDepthBias(d.context,0,0);
    sceGxmSetFrontStencilFunc(d.context,SCE_GXM_STENCIL_FUNC_ALWAYS,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,0xff,0);
    sceGxmSetBackStencilFunc(d.context,SCE_GXM_STENCIL_FUNC_ALWAYS,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,0xff,0);
    sceGxmSetFrontStencilRef(d.context,0); sceGxmSetBackStencilRef(d.context,0);
    Raster clear{};
    clear.depthFunction=0; clear.writeAlpha=true;
    return d.bind_and_draw(d.clearVertices.data,d.clearIndices.data,3,kIdentity,clear);
}

bool Renderer::draw(uint32_t mesh, const std::array<float,16>& mvp, const Raster& raster) noexcept {
    if (!impl_) return false;
    auto& d=*impl_;
    if (!d.on_owner() || !d.inScene || d.error.load()<0 || !raster.valid() || mesh>=d.meshes.size()) return d.fail("native_draw_lifecycle");
    if (!finite_mvp(mvp)) return d.fail("native_nonfinite_mvp");
    const auto& resident=d.meshes[mesh];
    return d.bind_and_draw(static_cast<const uint8_t*>(d.vertices.data)+resident.vertexOffset,
                          static_cast<const uint8_t*>(d.indices.data)+resident.indexOffset,
                          resident.indexCount,mvp,raster);
}

bool Renderer::draw_textured(uint32_t mesh,const std::array<float,16>& mvp,
                             const Raster& raster,const Material& material) noexcept {
    if(!impl_)return false;
    auto& d=*impl_;
    if(!d.on_owner()||!d.inScene||d.error.load()<0||!raster.valid()||!material.valid()||
       mesh>=d.meshes.size()||!d.texturedVertexProgram||mesh>=d.hasUv.size()||!d.hasUv[mesh]||
       material.texture>d.textures.size())return d.fail("native_textured_draw_lifecycle");
    if(!d.textures[material.texture-1u]->swizzled && (!material.wrapU||!material.wrapV))return false;
    if(!finite_mvp(mvp))return d.fail("native_nonfinite_mvp");
    const auto& resident=d.meshes[mesh];
    return d.bind_and_draw(static_cast<const uint8_t*>(d.vertices.data)+resident.vertexOffset,
        static_cast<const uint8_t*>(d.indices.data)+resident.indexOffset,resident.indexCount,mvp,raster,
        static_cast<const uint8_t*>(d.uvVertices.data)+(resident.vertexOffset/kVertexStride)*12u,&material);
}

bool Renderer::draw_dynamic(std::span<const ModelVertex> source,
                            std::span<const uint16_t> sourceIndices,
                            const std::array<float,16>& mvp,const Raster& raster,
                            const Material* material) noexcept {
    if(!impl_)return false;
    auto& d=*impl_;
    if(!d.on_owner()||!d.inScene||d.error.load()<0||!raster.valid())return false;
    const uint64_t uploadStart=d.timings?sceKernelGetSystemTimeWide():0;
    if(source.empty()||source.size()>64000||sourceIndices.empty()||
       sourceIndices.size()%3||sourceIndices.size()>192000||!finite_mvp(mvp))return false;
    if(material && (!material->valid()||!d.texturedVertexProgram||
                    material->texture>d.textures.size()))return false;
    if(material && !d.textures[material->texture-1u]->swizzled &&
       (!material->wrapU || !material->wrapV))return false;
    for(uint16_t index : sourceIndices)if(index>=source.size())return false;

    constexpr size_t kVertices=1024u*1024u, kUv=512u*1024u, kIndices=512u*1024u;
    const size_t vb=source.size()*kVertexStride, ub=source.size()*12u;
    const size_t ib=sourceIndices.size()*sizeof(uint16_t);
    auto& frame=d.dynamic[d.frames.back()];
    const size_t uvUsed=(frame.vertexUsed/kVertexStride)*12u;
    if(vb>kVertices-frame.vertexUsed||ib>kIndices-frame.indexUsed||
       (material && (uvUsed>kUv||ub>kUv-uvUsed)))return false;
    if(!frame.vertex.data && !d.check(frame.vertex.allocate(kVertices),"native_dynamic_vertex_memory"))return false;
    if(!frame.index.data && !d.check(frame.index.allocate(kIndices),"native_dynamic_index_memory"))return false;
    if(material && !frame.uv.data && !d.check(frame.uv.allocate(kUv),"native_dynamic_uv_memory"))return false;

    auto* vertex=static_cast<uint8_t*>(frame.vertex.data)+frame.vertexUsed;
    auto* index=static_cast<uint8_t*>(frame.index.data)+frame.indexUsed;
    auto* uv=material?static_cast<uint8_t*>(frame.uv.data)+uvUsed:nullptr;
    for(size_t i=0;i<source.size();++i) {
        static_assert(offsetof(ModelVertex,color)==16 && offsetof(ModelVertex,uv)==20);
        std::memcpy(vertex+i*kVertexStride,&source[i],kVertexStride);
        if(uv)std::memcpy(uv+i*12u,source[i].uv.data(),12u);
    }
    std::memcpy(index,sourceIndices.data(),ib);
    frame.vertexUsed+=vb;frame.indexUsed+=ib;
    if(d.timings)d.currentTiming.dynamicVertexBytes+=vb;
    frame.gpuUsed=true;
    if(d.timings)d.currentTiming.uploadUs+=sceKernelGetSystemTimeWide()-uploadStart;
    return d.bind_and_draw(vertex,index,static_cast<unsigned>(sourceIndices.size()),
                           mvp,raster,uv,material);
}

bool Renderer::end_frame() noexcept {
    if (!impl_) return false;
    auto& d=*impl_;
    if (!d.on_owner() || !d.inScene || !d.frames.active() || d.error.load()<0) return d.fail("native_end_frame_lifecycle");
    if(d.currentTarget&&!select_target())return false;
    auto& dynamic=d.dynamic[d.frames.back()];
    SceGxmNotification retired{};
    if(d.slotFence && dynamic.gpuUsed) {
        if(++dynamic.retireValue==0)++dynamic.retireValue;
        retired={d.notifications+d.frames.back(),dynamic.retireValue};
    }
    if (!d.check(sceGxmEndScene(d.context,nullptr,retired.address?&retired:nullptr), "native_end_scene")) return false;
    d.inScene=false;
    auto& back=d.surfaces[d.frames.back()];
    const DisplayRequest request{back.memory.data,&d.displayError};
    const uint64_t start=d.timings?sceKernelGetSystemTimeWide():0;
    if (!d.check(sceGxmPadHeartbeat(&back.color,back.sync), "native_display_heartbeat") ||
        !d.check(sceGxmDisplayQueueAddEntry(d.surfaces[d.frames.front()].sync,back.sync,&request), "native_queue_present")) return false;
    d.displayed=true;
    if(d.timings)d.currentTiming.queueUs=sceKernelGetSystemTimeWide()-start;
    d.completedTiming=d.currentTiming;
    return d.frames.present();
}

bool Renderer::readback_presented_rgba8(std::vector<uint8_t>& pixels) noexcept {
    pixels.clear();
    if (!impl_) return false;
    auto& d=*impl_;
    if (!d.on_owner() || !d.initialized || !d.displayed || d.inScene || d.frames.active()) return false;
    sceGxmFinish(d.context);
    if (sceGxmDisplayQueueFinish()<0 || d.displayError.load(std::memory_order_relaxed)<0) return false;
    SceDisplayFrameBuf fb{};
    fb.size=sizeof(fb);
    if (sceDisplayGetFrameBuf(&fb,SCE_DISPLAY_SETBUF_IMMEDIATE)<0 ||
        fb.base!=d.surfaces[d.frames.front()].memory.data ||
        fb.width!=kWidth || fb.height!=kHeight || fb.pitch!=kStride ||
        fb.pixelformat!=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8) return false;
    pixels.resize(kWidth*kHeight*4u);
    std::memcpy(pixels.data(),fb.base,pixels.size());
    return true;
}

void Renderer::shutdown() noexcept {
    if (!impl_) return;
    auto& d=*impl_;
    if (!d.ownsGxm && !d.context) return;
    if (!d.on_owner()) return;
    if (d.inScene && d.context) { sceGxmEndScene(d.context,nullptr,nullptr); d.inScene=false; }
    if (d.context) sceGxmFinish(d.context);
    if (d.ownsGxm) sceGxmDisplayQueueFinish();
    if (d.displayed) { sceDisplaySetFrameBuf(nullptr,SCE_DISPLAY_SETBUF_NEXTFRAME); sceDisplayWaitVblankStart(); }
    // GPU and display are retired before releasing patched programs, buffers
    // or callback storage. Retain compiler output until programs unregister.
    if (d.patcher) {
        for(auto*& fp : d.texturedFragments) {if(fp)sceGxmShaderPatcherReleaseFragmentProgram(d.patcher,fp);fp=nullptr;}
        if(d.texturedVertexProgram)sceGxmShaderPatcherReleaseVertexProgram(d.patcher,d.texturedVertexProgram);
        d.texturedVertexProgram=nullptr;
        if(d.texturedFragmentId)sceGxmShaderPatcherUnregisterProgram(d.patcher,d.texturedFragmentId);
        if(d.texturedVertexId)sceGxmShaderPatcherUnregisterProgram(d.patcher,d.texturedVertexId);
        d.texturedFragmentId=d.texturedVertexId=nullptr;
        for (auto*& fp : d.fragments) { if (fp) sceGxmShaderPatcherReleaseFragmentProgram(d.patcher,fp); fp=nullptr; }
        if (d.vertexProgram) sceGxmShaderPatcherReleaseVertexProgram(d.patcher,d.vertexProgram);
        d.vertexProgram=nullptr;
        if (d.fragmentId) sceGxmShaderPatcherUnregisterProgram(d.patcher,d.fragmentId);
        if (d.vertexId) sceGxmShaderPatcherUnregisterProgram(d.patcher,d.vertexId);
        d.fragmentId=d.vertexId=nullptr;
    for(auto& p:d.scene.fragments)if(p){sceGxmShaderPatcherReleaseFragmentProgram(d.patcher,p);p=nullptr;}
    if(d.scene.vertex){sceGxmShaderPatcherReleaseVertexProgram(d.patcher,d.scene.vertex);d.scene.vertex=nullptr;}
    if(d.scene.vertexId){sceGxmShaderPatcherUnregisterProgram(d.patcher,d.scene.vertexId);d.scene.vertexId=nullptr;}
    if(d.scene.fragmentId){sceGxmShaderPatcherUnregisterProgram(d.patcher,d.scene.fragmentId);d.scene.fragmentId=nullptr;}
        for(auto& variant:d.sceneFragments)if(variant){for(auto* fragment:variant->fragments)if(fragment)sceGxmShaderPatcherReleaseFragmentProgram(d.patcher,fragment);if(variant->id)sceGxmShaderPatcherUnregisterProgram(d.patcher,variant->id);variant.reset();}
        sceGxmShaderPatcherDestroy(d.patcher); d.patcher=nullptr;
    }
    if (d.ownsCompiler) { shark_clear_output(); shark_end(); d.ownsCompiler=false; }
    if (d.target) { sceGxmDestroyRenderTarget(d.target); d.target=nullptr; }
    if (d.context) { sceGxmDestroyContext(d.context); d.context=nullptr; }
    for (auto& surface : d.surfaces) {
        if (surface.sync) sceGxmSyncObjectDestroy(surface.sync);
        surface.sync=nullptr; surface.memory.reset();
    }
    d.vertices.reset(); d.indices.reset(); d.clearVertices.reset(); d.clearIndices.reset();
    for(auto& frame:d.dynamic) {
        frame.vertex.reset();frame.uv.reset();frame.index.reset();
        frame.vertexUsed=frame.indexUsed=0;frame.gpuUsed=false;
    }
    d.models.clear();d.retiredModels.clear();d.retiredTextures.clear();
    for(auto& x:d.sceneFrameVertex)x.reset();for(auto& x:d.sceneFrameIndex)x.reset();
    d.uvVertices.reset();d.hasUv.clear();d.textures.clear();d.textureBytes=0;
    d.depth.reset(); d.patchBuffer.reset(); d.vertexUsse.reset(); d.fragmentUsse.reset();
    d.vdm.reset(); d.vertexRing.reset(); d.fragmentRing.reset(); d.fragmentUsseRing.reset();
    std::free(d.host); d.host=nullptr;
    if (d.ownsGxm) { sceGxmTerminate(); d.ownsGxm=false; }
    d.frames.reset(); d.initialized=false; d.displayed=false; d.mvp=nullptr;
    std::vector<Mesh>().swap(d.meshes);
    std::vector<uint32_t>().swap(d.vertexCode); std::vector<uint32_t>().swap(d.fragmentCode);
    std::vector<uint32_t>().swap(d.texturedVertexCode);std::vector<uint32_t>().swap(d.texturedFragmentCode);
    d.texturedMvp=d.tint=d.controls=nullptr;
}
const char* Renderer::last_error() const noexcept { return impl_ ? impl_->operation.load(std::memory_order_relaxed) : "native_not_initialized"; }
int Renderer::error_code() const noexcept { return impl_ ? impl_->error.load(std::memory_order_relaxed) : static_cast<int>(SCE_GXM_ERROR_INVALID_VALUE); }
Renderer::FrameTiming Renderer::completed_timing() const noexcept {return impl_?impl_->completedTiming:FrameTiming{};}
} // namespace port::nativegxm
#endif
