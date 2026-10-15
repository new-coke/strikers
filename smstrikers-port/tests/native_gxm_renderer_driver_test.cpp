// Run the actual renderer against VitaSDK declarations and a delayed mock
// driver. These tests establish CPU ownership/lifetime/error contracts only.
#include "port/native_gxm_renderer.hpp"
#include "port/native_match_gpu_assets.hpp"
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <vitashark.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <iterator>
#include <limits>

namespace {
constexpr int kFailure=-77;
unsigned calls=0,failAt=0,draws=0,compiles=0,patches=0,stateMask=0,finishes=0,slotWaits=0;
unsigned notificationWords[3]{};
std::vector<SceGxmNotification> gpuNotifications;
bool noNotificationRegion=false;
bool initialized=false,inScene=false,inFlight=false,compiler=false;
SceUID currentThread=1,nextUid=1;
struct Memory {void* p; bool mapped=false;};
std::map<SceUID,Memory> memory;
std::set<void*> objects;
std::vector<std::vector<unsigned char>> pending;
SceGxmDisplayQueueCallback* callback=nullptr;
unsigned callbackBytes=0;
void* scanout=nullptr;
SceDisplayFrameBuf displayed{};
bool foreignScanout=false;
float uniform[256]{},lastMatrix[16]{};
const void* streamZero=nullptr;
int result() {return ++calls==failAt?kFailure:0;}
template<class T> int create(T** p) {
    const int r=result(); if(r<0)return r;
    *p=reinterpret_cast<T*>(std::malloc(16)); assert(*p);
    objects.insert(*p); return 0;
}
template<class T> int destroy(T* p) {
    assert(!inFlight && objects.erase(p)==1); std::free(p);return 0;
}
void empty() {assert(memory.empty() && objects.empty() && pending.empty() && !initialized && !compiler && !scanout);}
void reset(unsigned failure=0) {empty();calls=draws=compiles=patches=finishes=slotWaits=0;failAt=failure;currentThread=1;streamZero=nullptr;gpuNotifications.clear();noNotificationRegion=false;std::memset(notificationWords,0,sizeof notificationWords);}
void drain_one() {
    auto bytes=std::move(pending.front());pending.erase(pending.begin());
    callback(bytes.data());
}
int map_memory(void* p) {
    int r=result();if(r<0)return r;
    for(auto& [uid,m] : memory)if(m.p==p){assert(!m.mapped);m.mapped=true;return 0;}
    assert(false);return kFailure;
}
int unmap(void* p) {
    assert(!inFlight && pending.empty());
    for(auto& [uid,m] : memory)if(m.p==p){assert(m.mapped);m.mapped=false;return 0;}
    assert(false);return kFailure;
}
}
extern "C" {
SceUID sceKernelGetThreadId() {return currentThread;}
SceInt64 sceKernelGetSystemTimeWide() {static SceInt64 clock=0;return ++clock;}
volatile unsigned int* sceGxmGetNotificationRegion() {return noNotificationRegion?nullptr:notificationWords;}
int sceGxmNotificationWait(const SceGxmNotification* n) {
    ++slotWaits;assert(!inScene && n->value);
    const int r=result();if(r<0)return r;
    assert(n->address>=notificationWords && n->address<notificationWords+3);
    while(*n->address!=n->value) {
        assert(!gpuNotifications.empty());
        const auto done=gpuNotifications.front();gpuNotifications.erase(gpuNotifications.begin());
        *done.address=done.value;
    }
    inFlight=!gpuNotifications.empty();return 0;
}
SceUID sceKernelAllocMemBlock(const char*,SceKernelMemBlockType type,SceSize size,SceKernelAllocMemBlockOpt*) {
    const int r=result();if(r<0)return r;
    assert(size%(type==SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW?262144:4096)==0);
    void* p=nullptr;assert(!posix_memalign(&p,4096,size));
    const auto uid=nextUid++;memory[uid]={p};return uid;
}
int sceKernelGetMemBlockBase(SceUID uid,void** p) {int r=result();if(r<0)return r;*p=memory.at(uid).p;return 0;}
int sceKernelFreeMemBlock(SceUID uid) {const auto m=memory.at(uid);assert(!m.mapped && scanout!=m.p);std::free(m.p);memory.erase(uid);return 0;}
int sceGxmInitialize(const SceGxmInitializeParams* p) {
    int r=result();if(r<0)return r;assert(!initialized);initialized=true;
    assert(p->displayQueueMaxPendingCount==2);callback=p->displayQueueCallback;
    callbackBytes=p->displayQueueCallbackDataSize;return 0;
}
int sceGxmTerminate() {assert(initialized && objects.empty() && memory.empty());initialized=false;return 0;}
int sceGxmMapMemory(void* p,SceSize,SceGxmMemoryAttribFlags) {return map_memory(p);}
int sceGxmMapVertexUsseMemory(void* p,SceSize,unsigned* offset) {*offset=0;return map_memory(p);}
int sceGxmMapFragmentUsseMemory(void* p,SceSize,unsigned* offset) {*offset=0;return map_memory(p);}
int sceGxmUnmapMemory(void* p) {return unmap(p);}
int sceGxmUnmapVertexUsseMemory(void* p) {return unmap(p);}
int sceGxmUnmapFragmentUsseMemory(void* p) {return unmap(p);}
int sceGxmCreateContext(const SceGxmContextParams*,SceGxmContext** p) {return create(p);}
int sceGxmDestroyContext(SceGxmContext* p) {return destroy(p);}
int sceGxmCreateRenderTarget(const SceGxmRenderTargetParams*,SceGxmRenderTarget** p) {return create(p);}
int sceGxmDestroyRenderTarget(SceGxmRenderTarget* p) {return destroy(p);}
int sceGxmSyncObjectCreate(SceGxmSyncObject** p) {return create(p);}
int sceGxmSyncObjectDestroy(SceGxmSyncObject* p) {return destroy(p);}
int sceGxmColorSurfaceInit(SceGxmColorSurface*,SceGxmColorFormat,SceGxmColorSurfaceType,SceGxmColorSurfaceScaleMode,SceGxmOutputRegisterSize,unsigned,unsigned,unsigned,void*) {return result();}
int sceGxmDepthStencilSurfaceInit(SceGxmDepthStencilSurface*,SceGxmDepthStencilFormat,SceGxmDepthStencilSurfaceType,unsigned,void*,void*) {return result();}
void sceGxmDepthStencilSurfaceSetBackgroundDepth(SceGxmDepthStencilSurface*,float) {}
void sceGxmDepthStencilSurfaceSetForceLoadMode(SceGxmDepthStencilSurface*,SceGxmDepthStencilForceLoadMode) {}
void sceGxmDepthStencilSurfaceSetForceStoreMode(SceGxmDepthStencilSurface*,SceGxmDepthStencilForceStoreMode) {}
int sceGxmShaderPatcherCreate(const SceGxmShaderPatcherParams*,SceGxmShaderPatcher** p) {return create(p);}
int sceGxmShaderPatcherDestroy(SceGxmShaderPatcher* p) {return destroy(p);}
int sceGxmShaderPatcherRegisterProgram(SceGxmShaderPatcher*,const SceGxmProgram*,SceGxmShaderPatcherId* p) {return create(p);}
int sceGxmShaderPatcherUnregisterProgram(SceGxmShaderPatcher*,SceGxmShaderPatcherId p) {return destroy(p);}
int sceGxmShaderPatcherCreateVertexProgram(SceGxmShaderPatcher*,SceGxmShaderPatcherId,const SceGxmVertexAttribute* a,unsigned n,const SceGxmVertexStream* s,unsigned ns,SceGxmVertexProgram** p) {
    assert((n==2&&ns==1)||(n==3&&ns==2)||(n==10&&ns==1));
    assert(s->stride==(n==10?108u:20u) && a[0].format==SCE_GXM_ATTRIBUTE_FORMAT_F32 && a[1].format==SCE_GXM_ATTRIBUTE_FORMAT_U8N && a[1].offset==16);
    if(n==3)assert(a[2].streamIndex==1&&a[2].componentCount==3&&a[2].format==SCE_GXM_ATTRIBUTE_FORMAT_F32&&s[1].stride==12);
    ++patches;return create(p);
}
int sceGxmShaderPatcherCreateFragmentProgram(SceGxmShaderPatcher*,SceGxmShaderPatcherId,SceGxmOutputRegisterFormat,SceGxmMultisampleMode,const SceGxmBlendInfo*,const SceGxmProgram*,SceGxmFragmentProgram** p) {++patches;return create(p);}
int sceGxmShaderPatcherReleaseVertexProgram(SceGxmShaderPatcher*,SceGxmVertexProgram* p) {return destroy(p);}
int sceGxmShaderPatcherReleaseFragmentProgram(SceGxmShaderPatcher*,SceGxmFragmentProgram* p) {return destroy(p);}
int shark_init(const char*) {int r=result();if(r<0)return r;assert(!compiler);compiler=true;return 0;}
void shark_end() {assert(compiler);compiler=false;}
SceGxmProgram* shark_compile_shader(const char*,uint32_t* size,shark_type) {
    ++compiles;if(result()<0)return nullptr;static uint32_t bytes[16]{};*size=sizeof(bytes);return reinterpret_cast<SceGxmProgram*>(bytes);
}
void shark_clear_output() {}
int sceGxmProgramCheck(const SceGxmProgram*) {return result();}
const SceGxmProgramParameter* sceGxmProgramFindParameterByName(const SceGxmProgram*,const char* name) {
    const uintptr_t id=std::strcmp(name,"u_mvp")==0?3:std::strcmp(name,"a_color")==0?2:
        std::strcmp(name,"u_tint")==0?4:std::strcmp(name,"u_controls")==0?5:
        std::strcmp(name,"u_texture")==0?6:std::strcmp(name,"a_uv")==0?7:1;
    return reinterpret_cast<const SceGxmProgramParameter*>(id);
}
SceGxmParameterCategory sceGxmProgramParameterGetCategory(const SceGxmProgramParameter* p) {
    const auto id=reinterpret_cast<uintptr_t>(p);
    return id>=3&&id<=5?SCE_GXM_PARAMETER_CATEGORY_UNIFORM:id==6?SCE_GXM_PARAMETER_CATEGORY_SAMPLER:SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE;
}
unsigned sceGxmProgramParameterGetResourceIndex(const SceGxmProgramParameter* p) {const auto id=reinterpret_cast<uintptr_t>(p);return id==6?0:id==7?2:unsigned(id-1);}
int sceGxmBeginScene(SceGxmContext*,unsigned,const SceGxmRenderTarget*,const SceGxmValidRegion*,SceGxmSyncObject*,SceGxmSyncObject*,const SceGxmColorSurface*,const SceGxmDepthStencilSurface*) {
    int r=result();if(r<0)return r;assert(!inScene);inScene=true;stateMask=0;return 0;
}
int sceGxmEndScene(SceGxmContext*,const SceGxmNotification* vertex,const SceGxmNotification* fragment) {int r=result();if(r<0)return r;assert(inScene && !vertex);if(fragment) {assert(fragment->address && fragment->value);gpuNotifications.push_back(*fragment);}inScene=false;return 0;}
void sceGxmFinish(SceGxmContext*) {++finishes;for(const auto n:gpuNotifications)*n.address=n.value;gpuNotifications.clear();inFlight=false;}
int sceGxmDisplayQueueFinish() {while(!pending.empty())drain_one();return 0;}
int sceGxmPadHeartbeat(const SceGxmColorSurface*,SceGxmSyncObject*) {return result();}
int sceGxmDisplayQueueAddEntry(SceGxmSyncObject*,SceGxmSyncObject*,const void* data) {
    int r=result();if(r<0)return r;
    if(pending.size()==2)drain_one();
    const auto* bytes=static_cast<const unsigned char*>(data);
    pending.emplace_back(bytes,bytes+callbackBytes);return 0;
}
int sceDisplaySetFrameBuf(const SceDisplayFrameBuf* p,SceDisplaySetBufSync) {
    scanout=p?p->base:nullptr;displayed=p?*p:SceDisplayFrameBuf{};
    if(p) {
        const uint8_t pixel[]{15,35,77,255};
        std::memcpy(p->base,pixel,sizeof(pixel));
    }
    return 0;
}
int sceDisplayGetFrameBuf(SceDisplayFrameBuf* p,SceDisplaySetBufSync sync) {
    assert(!inFlight && pending.empty() && sync==SCE_DISPLAY_SETBUF_IMMEDIATE);
    const int r=result();if(r<0)return r;
    *p=displayed;
    if(foreignScanout)p->base=reinterpret_cast<void*>(1);
    return 0;
}
int sceDisplayWaitVblankStart() {return 0;}
void sceGxmSetViewport(SceGxmContext*,float x,float sx,float y,float sy,float z,float sz) {assert(sx>0 && sy<0 && x>=0 && y>=0 && z==.5f && sz==.5f);stateMask|=1;}
void sceGxmSetRegionClip(SceGxmContext*,SceGxmRegionClipMode mode,unsigned,unsigned,unsigned,unsigned) {assert(mode==SCE_GXM_REGION_CLIP_NONE);stateMask|=2;}
void sceGxmSetViewportEnable(SceGxmContext*,SceGxmViewportMode) {stateMask|=4;}
void sceGxmSetTwoSidedEnable(SceGxmContext*,SceGxmTwoSidedMode) {stateMask|=8;}
void sceGxmSetFrontPolygonMode(SceGxmContext*,SceGxmPolygonMode) {stateMask|=16;}
void sceGxmSetBackPolygonMode(SceGxmContext*,SceGxmPolygonMode) {stateMask|=32;}
void sceGxmSetFrontFragmentProgramEnable(SceGxmContext*,SceGxmFragmentProgramMode) {stateMask|=64;}
void sceGxmSetBackFragmentProgramEnable(SceGxmContext*,SceGxmFragmentProgramMode) {stateMask|=128;}
void sceGxmSetFrontDepthBias(SceGxmContext*,int,int) {stateMask|=256;}
void sceGxmSetBackDepthBias(SceGxmContext*,int,int) {stateMask|=512;}
void sceGxmSetFrontStencilFunc(SceGxmContext*,SceGxmStencilFunc,SceGxmStencilOp,SceGxmStencilOp,SceGxmStencilOp,unsigned char,unsigned char) {stateMask|=1024;}
void sceGxmSetBackStencilFunc(SceGxmContext*,SceGxmStencilFunc,SceGxmStencilOp,SceGxmStencilOp,SceGxmStencilOp,unsigned char,unsigned char) {stateMask|=2048;}
void sceGxmSetFrontStencilRef(SceGxmContext*,unsigned) {stateMask|=4096;}
void sceGxmSetBackStencilRef(SceGxmContext*,unsigned) {stateMask|=8192;}
void sceGxmSetVertexProgram(SceGxmContext*,const SceGxmVertexProgram*) {stateMask|=16384;}
void sceGxmSetFragmentProgram(SceGxmContext*,const SceGxmFragmentProgram*) {stateMask|=32768;}
void sceGxmSetFrontDepthFunc(SceGxmContext*,SceGxmDepthFunc) {stateMask|=65536;}
void sceGxmSetBackDepthFunc(SceGxmContext*,SceGxmDepthFunc) {stateMask|=131072;}
void sceGxmSetFrontDepthWriteEnable(SceGxmContext*,SceGxmDepthWriteMode) {stateMask|=262144;}
void sceGxmSetBackDepthWriteEnable(SceGxmContext*,SceGxmDepthWriteMode) {stateMask|=524288;}
void sceGxmSetCullMode(SceGxmContext*,SceGxmCullMode) {stateMask|=1048576;}
int sceGxmReserveVertexDefaultUniformBuffer(SceGxmContext*,void** p) {int r=result();if(r<0)return r;*p=uniform;return 0;}
int sceGxmReserveFragmentDefaultUniformBuffer(SceGxmContext*,void** p) {int r=result();if(r<0)return r;*p=uniform;return 0;}
int sceGxmSetUniformDataF(void* p,const SceGxmProgramParameter*,unsigned,unsigned n,const float* data) {int r=result();if(r<0)return r;assert(n==16||n==4||n==32||n==120);std::memcpy(p,data,n*4);if(n==16)std::memcpy(lastMatrix,data,64);return 0;}
int sceGxmTextureInitSwizzled(SceGxmTexture*,const void*,SceGxmTextureFormat,unsigned,unsigned,unsigned) {return result();}
int sceGxmTextureInitLinear(SceGxmTexture*,const void*,SceGxmTextureFormat,unsigned,unsigned,unsigned) {return result();}
int sceGxmTextureSetUAddrMode(SceGxmTexture*,SceGxmTextureAddrMode) {return result();}
int sceGxmTextureSetVAddrMode(SceGxmTexture*,SceGxmTextureAddrMode) {return result();}
int sceGxmTextureSetMinFilter(SceGxmTexture*,SceGxmTextureFilter) {return result();}
int sceGxmTextureSetMagFilter(SceGxmTexture*,SceGxmTextureFilter) {return result();}
int sceGxmTextureSetMipmapCount(SceGxmTexture*,unsigned) {return result();}
int sceGxmTextureSetMipFilter(SceGxmTexture*,SceGxmTextureMipFilter) {return result();}
int sceGxmSetFragmentTexture(SceGxmContext*,unsigned,const SceGxmTexture*) {return result();}
int sceGxmSetVertexStream(SceGxmContext*,unsigned index,const void* address) {
    if(index==0)streamZero=address;
    stateMask|=2097152;return result();
}
int sceGxmDraw(SceGxmContext*,SceGxmPrimitiveType,SceGxmIndexFormat,const void*,unsigned count) {
    int r=result();if(r<0)return r;assert(inScene && count%3==0 && stateMask==4194303);++draws;inFlight=true;return 0;
}
}

int main(int argc, char** argv) {
    using port::nativegxm::Renderer;
    using port::nativegxm::Mesh;
    const std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,4,5,6,1};
    const std::array<unsigned char,60> vertices{};
    const uint16_t indices[]{0,1,2};
    const std::array<Mesh,1> meshes{{{0,60,0,3,3}}};
    reset();
    unsigned initCalls=0;
    {
        Renderer r;assert(r.initialize());initCalls=calls;
        assert(compiles==2 && patches==33 && !compiler);
        assert(r.upload(vertices.data(),vertices.size(),indices,sizeof(indices),meshes));
        std::vector<uint8_t> pixels{1,2,3};
        assert(!r.readback_presented_rgba8(pixels) && pixels.empty());
        for(unsigned i=0;i<5;++i) {
            // Producer admission skips a bad transform while the GPU frame
            // remains usable. In particular, no reserve/draw reaches the
            // driver and its sticky error state remains clear.
            auto invalid=matrix;invalid[5]=std::numeric_limits<float>::quiet_NaN();
            assert(!port::nativegxm::finite_mvp(invalid));
            assert(r.begin_frame());assert(r.draw(0,matrix,{}));
            assert(!r.readback_presented_rgba8(pixels) && pixels.empty());
            assert(r.end_frame());
            assert(r.readback_presented_rgba8(pixels));
            assert(pixels.size()==960*544*4 && pixels[0]==15 && pixels[1]==35 && pixels[2]==77 && pixels[3]==255);
            assert(!inFlight && pending.empty());
            foreignScanout=true;
            assert(!r.readback_presented_rgba8(pixels) && pixels.empty());
            foreignScanout=false;
            failAt=calls+1;
            assert(!r.readback_presented_rgba8(pixels) && pixels.empty());
            failAt=0;
            assert(!std::memcmp(lastMatrix,matrix.data(),64));
            assert(r.error_code()==0);
            assert(compiles==2 && patches==33);
        }
        assert(draws==10); // one clear + one mesh each frame
        r.shutdown();r.shutdown();empty();
    }
    reset();
    {
        Renderer r;assert(r.initialize());assert(r.initialize_scene_program());
        using namespace port::nativegxm;
        ModelVertex source[3];source[0].position={1,2,3,1};source[1].position={2,2,3,1};source[2].position={1,3,3,1};
        auto model=r.create_model(source,indices);assert(model);
        const auto target=r.create_target(80,80);assert(target);
        assert(r.begin_frame());SceneMaterial material;material.controls={1,-1,1,0};
        assert(r.draw_scene(model,{}, {},matrix,{},material));
        source[0].position[0]=9;assert(r.draw_scene(0,source,indices,matrix,{},material));
        const auto* snapshot=static_cast<const ModelVertex*>(streamZero);assert(snapshot[0].position[0]==9);
        source[0].position[0]=77;assert(snapshot[0].position[0]==9);
        assert(r.copy_texture(target,{0,0,160,148},true));assert(r.clear_depth());
        r.release_model(model);assert(r.end_frame());
        std::vector<uint8_t> captured;assert(r.readback_presented_rgba8(captured));
        assert(r.begin_frame());assert(r.end_frame());r.shutdown();empty();
    }
    for(unsigned failure=1;failure<=initCalls;++failure) {
        reset(failure);
        { Renderer r;assert(!r.initialize());r.shutdown(); }
        empty();
    }
    for(unsigned failure=1;failure<=6;++failure) {
        reset();
        Renderer r;assert(r.initialize());
        // Allocation, base lookup and mapping for each immutable GPU buffer.
        failAt=calls+failure;
        assert(!r.upload(vertices.data(),vertices.size(),indices,sizeof(indices),meshes));
        r.shutdown();empty();
    }
    reset();
    {
        Renderer r;assert(r.initialize());
        const uint16_t invalid[]{0,1,3};
        assert(!r.upload(vertices.data(),vertices.size(),invalid,sizeof(invalid),meshes));
        assert(draws==0);r.shutdown();empty();
    }
    reset();
    {
        Renderer r;assert(r.initialize());
        assert(r.upload(vertices.data(),vertices.size(),indices,sizeof(indices),meshes));
        assert(r.begin_frame());assert(r.draw(0,matrix,{}));
        failAt=calls+3; // EndScene, heartbeat, queue insertion
        assert(!r.end_frame());assert(!r.begin_frame());
        r.shutdown();empty();
    }
    reset();
    {
        Renderer r;assert(r.initialize());
        currentThread=2;assert(!r.begin_frame());
        currentThread=1;r.shutdown();empty();
    }
    const std::array<float,9> uv{0,0,1,1,0,1,0,1,1};
    const std::span<const uint8_t> uvBytes(reinterpret_cast<const uint8_t*>(uv.data()),sizeof(uv));
    const std::array<uint8_t,1> hasUv{1};
    unsigned uvCalls=0;
    reset();
    {
        Renderer r;assert(r.initialize());
        assert(r.upload(vertices.data(),vertices.size(),indices,sizeof(indices),meshes));
        const auto before=calls;
        assert(r.upload_uvs(uvBytes,hasUv));uvCalls=calls-before;
        assert(compiles==4 && patches==66);
        port::nativegxm::TextureUpload image;
        image.width=image.height=image.stride=2;image.mipCount=1;
        image.storage=port::nativegxm::TextureStorage::Rgba8;image.swizzled=true;
        image.pixels.resize(16,255);
        const auto texture=r.create_texture(image);assert(texture==1);
        port::nativegxm::Material material;material.texture=texture;
        material.flags=1;material.alphaRef=64;
        assert(r.begin_frame());assert(r.draw_textured(0,matrix,{},material));assert(r.end_frame());
        assert(draws==2 && r.error_code()==0 && compiles==4 && patches==66);
        r.shutdown();empty();
    }
    // Every UV warm-up/registration/patch failure must retire partial GXM
    // programs and both immutable streams without leaking compiler ownership.
    for(unsigned failure=1;failure<=uvCalls;++failure) {
        reset();Renderer r;assert(r.initialize());
        assert(r.upload(vertices.data(),vertices.size(),indices,sizeof(indices),meshes));
        failAt=calls+failure;assert(!r.upload_uvs(uvBytes,hasUv));r.shutdown();empty();
    }
    for(unsigned failure=1;failure<=4;++failure) {
        reset();Renderer r;assert(r.initialize());
        assert(r.upload(vertices.data(),vertices.size(),indices,sizeof(indices),meshes));
        assert(r.upload_uvs(uvBytes,hasUv));
        port::nativegxm::TextureUpload image;
        image.width=image.height=image.stride=2;image.mipCount=1;
        image.storage=port::nativegxm::TextureStorage::Rgba8;image.swizzled=true;image.pixels.resize(16,255);
        failAt=calls+failure;assert(r.create_texture(image)==0);r.shutdown();empty();
    }
    // A dynamic draw owns its upload. Mutating the temporary producer buffer
    // after draw submission must not change the GPU-mapped source. Frame slot
    // rollover forces a fence before reusing that storage.
    reset();
    {
        Renderer r;assert(r.initialize());
        std::array<port::nativegxm::ModelVertex,3> dynamic{};
        dynamic[0].position={1,2,3,1};
        dynamic[1].position={4,5,6,1};
        dynamic[2].position={7,8,9,1};
        for(unsigned frame=0;frame<4;++frame) {
            assert(r.begin_frame());
            assert(r.draw_dynamic(dynamic,indices,matrix,{}));
            const auto* resident=static_cast<const uint8_t*>(streamZero);
            assert(resident && resident!=reinterpret_cast<const uint8_t*>(dynamic.data()));
            const float expected=dynamic[0].position[0];
            dynamic[0].position[0]=1000.f;
            assert(*reinterpret_cast<const float*>(resident)==expected);
            dynamic[0].position[0]=expected;
            assert(r.end_frame());
        }
        assert(finishes>=1); // slot 1 was reused on the fourth frame
        r.shutdown();empty();
    }
    // A slot wait retires only the old scene using that slot, not newer
    // frames. A completed notification skips the SDK wait entirely. It must
    // be observed before writing to the previous frame's GPU arrays.
    reset();
    {
        Renderer r;assert(r.initialize(true,true));
        std::array<port::nativegxm::ModelVertex,3> dynamic{};
        for(unsigned frame=0;frame<7;++frame) {
            assert(r.begin_frame());
            if(frame>=3)assert(finishes==0 && gpuNotifications.size()==2 && inFlight);
            assert(r.draw_dynamic(dynamic,indices,matrix,{}));
            assert(r.end_frame());
            const auto timing=r.completed_timing();
            assert(timing.globalFinishes==0 && timing.dynamicVertexBytes==60);
            assert(timing.slotWaits==unsigned(frame>=3));
            assert(timing.uploadUs && timing.drawUs && timing.draws==2);
        }
        assert(slotWaits==4 && finishes==0 && gpuNotifications.size()==3);
        // Pretend the GPU completed slot 2 independently. The next begin
        // must accept that fence without forcing either Wait or Finish.
        const auto done=gpuNotifications.front();gpuNotifications.erase(gpuNotifications.begin());
        *done.address=done.value;
        assert(r.begin_frame() && slotWaits==4 && finishes==0);
        assert(r.draw_dynamic(dynamic,indices,matrix,{}));assert(r.end_frame());
        r.shutdown();empty();
    }
    reset();
    {
        noNotificationRegion=true;Renderer r;
        assert(!r.initialize(true));assert(!std::strcmp(r.last_error(),"native_notification_region"));
        r.shutdown();empty();
    }
    reset();
    {
        Renderer r;assert(r.initialize(true));
        std::array<port::nativegxm::ModelVertex,3> dynamic{};
        for(unsigned frame=0;frame<3;++frame) {assert(r.begin_frame());assert(r.draw_dynamic(dynamic,indices,matrix,{}));assert(r.end_frame());}
        failAt=calls+1;assert(!r.begin_frame());
        assert(!std::strcmp(r.last_error(),"native_dynamic_slot_wait"));
        r.shutdown();empty();
    }
    reset();
    {
        Renderer r;assert(r.initialize());assert(r.begin_frame());
        std::array<port::nativegxm::ModelVertex,3> dynamic{};
        const uint16_t badIndices[]{0,1,3};
        assert(!r.draw_dynamic(dynamic,badIndices,matrix,{}));
        assert(r.error_code()==0);
        // Allocation failure cannot leave mapped blocks alive at shutdown.
        failAt=calls+1;
        assert(!r.draw_dynamic(dynamic,indices,matrix,{}));
        r.shutdown();empty();
    }
    reset();
    {
        Renderer r;assert(r.initialize());assert(r.begin_frame());
        std::array<port::nativegxm::ModelVertex,1> vertex{};
        const uint16_t degenerate[]{0,0,0};
        assert(r.draw_dynamic(vertex,degenerate,matrix,{}));
        assert(r.end_frame());r.shutdown();empty();
    }
    // Pass the full real catalog through the production renderer's CPU-side
    // upload and submission. This driver simulation establishes no pixels/FPS.
    if (argc>1) {
        reset();
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        port::directmatch::Manifest manifest;
        assert(file && port::directmatch::parse(bytes,manifest));
        port::directmatch::PreparedGpuAssets assets;
        const size_t expectedMeshes=manifest.meshes.size();
        assert(assets.prepare(std::move(manifest)) && assets.mesh_count()==expectedMeshes);
        std::vector<Mesh> native;
        for (const auto& mesh : assets.meshes())
            native.push_back({mesh.vertexOffset,mesh.vertexBytes,mesh.indexOffset,
                              mesh.indexCount,mesh.vertexCount});
        Renderer r;assert(r.initialize());
        assert(r.upload(assets.vertices().data(),assets.vertices().size(),
                        assets.indices().data(),assets.indices().size()*2,native));
        assets.release_upload_bytes();
        assert(r.begin_frame());
        for (uint32_t n=0;n<native.size();++n) assert(r.draw(n,matrix,{}));
        assert(draws==expectedMeshes+1 && compiles==2 && patches==33);
        assert(r.end_frame());r.shutdown();empty();
    }
}
