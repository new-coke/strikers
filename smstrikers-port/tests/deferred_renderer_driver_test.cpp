// The forced host header redirects only the production translation unit.
#undef clock_gettime
#undef pthread_cond_timedwait
#undef pthread_cond_timedwait_relative_np
#undef pthread_cond_wait
#include "port/deferred_renderer.hpp"
#include "port/host.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <pthread.h>
#include <cassert>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <cerrno>
#include <tuple>
#include <type_traits>
#include <cstddef>
using namespace port::render;
namespace {
pthread_t caller,owner;
bool initialized=false,active=false,loadingArmed=false;
std::atomic<bool> gpuFailure{false};
std::atomic<bool> holdFrameEnd{false}, frameEndEntered{false};
enum class FailedCall { None, SelectTarget, CopyTexture, CopyTextures, ClearColor, ClearDepth, SetBlack, PresentBitmap, SetLoading, LoadingTick, BeginFrame, EndFrame };
std::atomic<FailedCall> failedCall{FailedCall::None};
std::atomic<bool> modelAdmissionFailure{false};
int driverError=0;
std::vector<float> geometry,uniforms,materials,palettes,copies;
std::vector<TextureCopy> copiedRegions;
struct TargetRequest {
 uint32_t width,height;
 TargetFormat format;
 bool operator==(const TargetRequest&) const=default;
};
std::vector<TargetRequest> targetRequests;
unsigned released=0,frames=0,discardedFrames=0;
uint32_t selectedTarget=0;
struct BitmapRequest {
 std::vector<uint8_t> pixels;
 std::array<float,4> destination;
 bool active;
 uint32_t target;
 size_t precedingDraws;
};
std::vector<BitmapRequest> bitmapRequests;
std::vector<std::pair<char,bool>> displayEvents;
std::atomic<unsigned> completedBitmaps{0};
unsigned beginCalls=0,endCalls=0;
std::atomic<unsigned> loadingTicks{0},loadingQueries{0},loadingStops{0};
std::atomic<unsigned> clockQueries{0},timedWaitCalls{0};
std::atomic<bool> failClock{false},failTimedWait{false};
std::atomic<unsigned> systemClockQueries{0},ownerWaitEntries{0};
std::atomic<bool> spuriousNextWait{false};
struct LoadingRequest {
 LoadingIndicator request;
 bool active;
 uint32_t target;
 size_t precedingDraws;
};
std::vector<LoadingRequest> loadingRequests;
bool verifyDrawPayloads=false;
unsigned verifiedDrawPayloads=0;
constexpr unsigned PayloadDraws=20,PayloadVertices=12000;
// Retain the existing command layout independently of its new constructor.
struct DrawLayout {
 void* owner;
 uint32_t model,vertexCount,indexCount;
 std::array<float,16> matrix;
 Raster raster;
 SceneMaterial material;
};
template<class T> auto material_fields(T& m) {
 return std::tie(m.textures,m.wrapU,m.wrapV,m.filter,m.tint,m.ambient,m.diffuse,
  m.controls,m.effects,m.shadow,m.shadowAdd,m.fog,m.fogColor,m.specular,m.specularColor,
  m.material,m.alpha,m.lightPosition,m.lightColor,m.bones,m.modelView,m.viewport,m.scissor,
  m.environment,m.lit);
}
auto raster_fields(const Raster& r) {
 return std::tie(r.depthTest,r.depthWrite,r.depthFunction,r.cull,r.blend,r.writeRgb,
  r.writeAlpha,r.primitive);
}
auto vertex_fields(const ModelVertex& v) {
 return std::tie(v.position,v.color,v.uv,v.normal,v.extraUv,v.boneSlot);
}
template<class T> void fill_pattern(T& value,unsigned& next) {
 if constexpr(std::is_same_v<T,bool>)value=(next++&1)!=0;
 else if constexpr(std::is_floating_point_v<T>)value=float(next++%1000)*.001f;
 else if constexpr(std::is_integral_v<T>)value=T(next++%3);
 else for(auto& element:value)fill_pattern(element,next);
}
SceneMaterial payload_material(unsigned draw) {
 SceneMaterial m;
 unsigned next=draw*41;
 std::apply([&](auto&... fields){(fill_pattern(fields,next),...);},material_fields(m));
 return m;
}
Raster payload_raster(unsigned draw) {
 return {bool(draw&1),bool(draw&2),uint8_t(draw%4),uint8_t((draw+1)%4),
  uint8_t(draw%8),bool(draw&4),bool(draw&8),uint8_t(draw%2)};
}
std::array<float,16> payload_matrix(unsigned draw) {
 std::array<float,16> m;
 for(unsigned i=0;i<m.size();++i)m[i]=float(draw*16+i);
 return m;
}
ModelVertex payload_vertex(unsigned draw,bool last) {
 ModelVertex v;
 unsigned next=draw*37+unsigned(last)*17;
 fill_pattern(v.position,next);fill_pattern(v.color,next);fill_pattern(v.uv,next);
 fill_pattern(v.normal,next);fill_pattern(v.extraUv,next);fill_pattern(v.boneSlot,next);
 return v;
}
void owned(){assert(initialized&&pthread_equal(pthread_self(),owner)&&!pthread_equal(owner,caller));}
template<class Predicate> void eventually(Predicate predicate) {
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(!predicate()) {
  assert(std::chrono::steady_clock::now()<deadline);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
}
}
// Host-only handshakes exercise the real producer/owner queue under saturation.
#include "deferred_slot_acquire_cases.inc"
extern "C" {
int strikers_test_cond_wait(pthread_cond_t* condition,pthread_mutex_t* mutex) {
 slot_acquire_wait_observed();
 if(!pthread_equal(pthread_self(),caller)) {
  ownerWaitEntries.fetch_add(1,std::memory_order_release);
  if(spuriousNextWait.exchange(false))return 0;
 }
 return pthread_cond_wait(condition,mutex);
}
SceInt64 sceKernelGetSystemTimeWide() {
 systemClockQueries.fetch_add(1,std::memory_order_relaxed);
 return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
int strikers_test_clock_gettime(clockid_t clock,timespec* value) {
 assert(clock==CLOCK_MONOTONIC);clockQueries.fetch_add(1,std::memory_order_release);
 if(failClock.exchange(false)){errno=EIO;return -1;}
 return clock_gettime(clock,value);
}
int strikers_test_cond_timedwait(pthread_cond_t* condition,pthread_mutex_t* mutex,const timespec* until) {
 timedWaitCalls.fetch_add(1,std::memory_order_release);
 if(failTimedWait.exchange(false))return EINVAL;
 return pthread_cond_timedwait(condition,mutex,until);
}
#if defined(__APPLE__)
int strikers_test_cond_timedwait_relative(pthread_cond_t* condition,pthread_mutex_t* mutex,const timespec* duration) {
 timedWaitCalls.fetch_add(1,std::memory_order_release);
 if(failTimedWait.exchange(false))return EINVAL;
 return pthread_cond_timedwait_relative_np(condition,mutex,duration);
}
#endif
unsigned long long port_monotonic_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void* port_aligned_alloc(size_t alignment,size_t bytes){void* p=nullptr;return posix_memalign(&p,alignment,bytes)?nullptr:p;}
void port_aligned_free(void* p){std::free(p);}
SceUID sceKernelGetThreadId(){return pthread_equal(pthread_self(),caller)?1:2;}
int sceKernelChangeThreadCpuAffinityMask(SceUID id,int mask){assert(id==2&&mask==SCE_KERNEL_CPU_MASK_USER_1);return 0;}
int sceKernelChangeThreadPriority(SceUID id,int priority){assert(id==2&&priority==0x10000100);return 0;}
}
namespace port::render {
struct Renderer::Impl{};
Renderer::Renderer() noexcept:impl_(new Impl){}
Renderer::~Renderer(){shutdown();}
bool Renderer::initialize(bool) noexcept {owner=pthread_self();initialized=true;active=loadingArmed=false;selectedTarget=0;driverError=0;owned();return true;}
bool Renderer::initialize_scene_program() noexcept {owned();return true;}
uint32_t Renderer::create_texture(const TextureUpload&) noexcept {owned();return 17;}
uint32_t Renderer::create_target(uint32_t width,uint32_t height,TargetFormat format) noexcept {
 owned();targetRequests.push_back({width,height,format});return 18;
}
uint32_t Renderer::create_model(std::span<const ModelVertex> v,std::span<const uint16_t> i) noexcept {owned();assert(v.size()==3&&i.size()==3);if(modelAdmissionFailure){driverError=1285;return 0;}return 19;}
void Renderer::release_texture(uint32_t) noexcept {owned();++released;}
void Renderer::release_model(uint32_t) noexcept {owned();++released;}
bool Renderer::draw_scene(uint32_t model,std::span<const ModelVertex> v,std::span<const uint16_t> i,const std::array<float,16>& m,const Raster& raster,const SceneMaterial& material) noexcept {
 owned();assert(active);
 slot_acquire_owner_draw(m[0]);
 if(verifyDrawPayloads) {
  const unsigned draw=verifiedDrawPayloads++;
  assert(draw<PayloadDraws&&model==(draw%2?19u:0u)&&selectedTarget==77u+draw%2);
  const auto expected=payload_material(draw);
  assert(material_fields(material)==material_fields(expected));
  assert(m==payload_matrix(draw)&&raster_fields(raster)==raster_fields(payload_raster(draw)));
  const uintptr_t command=reinterpret_cast<uintptr_t>(&material)-offsetof(DrawLayout,material);
  assert(command%16==0&&reinterpret_cast<uintptr_t>(&m)==command+offsetof(DrawLayout,matrix));
  assert(reinterpret_cast<uintptr_t>(&raster)==command+offsetof(DrawLayout,raster));
  if(model)assert(v.empty()&&i.empty());
  else {
   assert(v.size()==PayloadVertices&&reinterpret_cast<uintptr_t>(v.data())==command+sizeof(DrawLayout));
   assert(reinterpret_cast<uintptr_t>(i.data())==reinterpret_cast<uintptr_t>(v.data())+v.size_bytes());
   assert(vertex_fields(v.front())==vertex_fields(payload_vertex(draw,false)));
   assert(vertex_fields(v.back())==vertex_fields(payload_vertex(draw,true)));
  }
 }
 if(!model){assert(!v.empty()&&i.size()==3&&i[0]==0&&i[1]==1&&i[2]==2);geometry.push_back(v[0].position[0]);}
 uniforms.push_back(m[0]);materials.push_back(material.tint[0]);palettes.push_back(material.bones.back()[3]);
 std::this_thread::sleep_for(std::chrono::microseconds(150));
 if(gpuFailure){driverError=1282;return false;}return true;
}
bool Renderer::select_target(uint32_t target,bool,bool) noexcept {owned();if(failedCall==FailedCall::SelectTarget)return false;selectedTarget=target;return true;}
bool Renderer::copy_texture(uint32_t,std::array<float,4>,bool) noexcept {owned();return failedCall!=FailedCall::CopyTexture;}
bool Renderer::copy_textures(std::span<const TextureCopy> list) noexcept {owned();if(failedCall==FailedCall::CopyTextures)return false;slot_acquire_owner_copy(list);for(auto x:list){copies.push_back(x.sourceRect[0]);copiedRegions.push_back(x);}return true;}
bool Renderer::clear_color(std::array<float,4>,bool) noexcept {owned();return failedCall!=FailedCall::ClearColor;}
bool Renderer::clear_depth() noexcept {owned();return failedCall!=FailedCall::ClearDepth;}
bool Renderer::set_black(bool black) noexcept {owned();displayEvents.emplace_back('b',black);return failedCall!=FailedCall::SetBlack;}
bool Renderer::present_bitmap(const TextureUpload& upload,std::array<float,4> destination) noexcept {
 owned();bitmapRequests.push_back({upload.pixels,destination,active,selectedTarget,uniforms.size()});
 displayEvents.emplace_back('m',active);
 // The caller must stay blocked until the owner's complete transaction ends.
 std::this_thread::sleep_for(std::chrono::microseconds(150));
 if(failedCall==FailedCall::PresentBitmap)return false;
 completedBitmaps.fetch_add(1,std::memory_order_release);return true;
}
bool Renderer::set_loading(bool begin,const LoadingIndicator& request) noexcept {
 owned();
 if(!begin){loadingArmed=false;loadingStops.fetch_add(1,std::memory_order_release);return true;}
 loadingRequests.push_back({request,active,selectedTarget,uniforms.size()});
 if(failedCall==FailedCall::SetLoading){loadingArmed=false;return false;}
 loadingArmed=true;return true;
}
bool Renderer::loading_active() const noexcept {
 owned();loadingQueries.fetch_add(1,std::memory_order_release);return loadingArmed;
}
bool Renderer::loading_tick() noexcept {
 owned();assert(loadingArmed);loadingTicks.fetch_add(1,std::memory_order_release);
 if(failedCall==FailedCall::LoadingTick){loadingArmed=false;driverError=1282;return false;}
 return true;
}
bool Renderer::frame_active() const noexcept {owned();return active;}
bool Renderer::begin_frame(FrameTimeline::Owner*) noexcept {owned();assert(!active);++beginCalls;if(failedCall==FailedCall::BeginFrame)return false;active=true;selectedTarget=0;return true;}
// A consumed GL error does not make a later successful end_frame check fail.
bool Renderer::end_frame(bool present) noexcept {
 owned();assert(active);
 if(holdFrameEnd.load(std::memory_order_acquire)){
  frameEndEntered.store(true,std::memory_order_release);
  while(holdFrameEnd.load(std::memory_order_acquire))
   std::this_thread::sleep_for(std::chrono::microseconds(100));
 }
 active=false;++endCalls;if(present)++frames;else ++discardedFrames;
 return failedCall!=FailedCall::EndFrame;
}
bool Renderer::readback_presented_rgba8(std::vector<uint8_t>& out) noexcept {owned();assert(!active);out.assign(4,42);return true;}
void Renderer::shutdown() noexcept {if(initialized){owned();loadingArmed=false;initialized=false;}}
const char* Renderer::last_error() const noexcept {owned();return driverError?"driver_failure":"none";}
int Renderer::error_code() const noexcept {owned();return driverError;}
Renderer::FrameTiming Renderer::completed_timing() const noexcept {owned();FrameTiming t;t.draws=uniforms.size();return t;}
}
#include "frame_timeline_deferred_cases.inc"
int main(int argc,char** argv){
 caller=pthread_self();
 if(argc==2&&std::strcmp(argv[1],"--slot-witness")==0)
  return check_deferred_slot_acquire(true)?0:64;
 DeferredRenderer r;
 assert(r.initialize(true)&&r.initialize_scene_program());assert(r.initialize());
 TextureUpload upload;assert(r.create_texture(upload)==17&&r.create_target(960,544)==18);
 assert(r.create_target(320,224,TargetFormat::Rgb565)==18);
 assert((targetRequests==std::vector<TargetRequest>{{960,544,TargetFormat::Rgba8},{320,224,TargetFormat::Rgb565}}));
 std::array<ModelVertex,3> vertices{};std::array<uint16_t,3> indices{0,1,2};
 assert(r.create_model(vertices,indices)==19&&r.begin_frame()&&r.frame_active());
 std::array<float,16> matrix{};Raster raster;SceneMaterial material;
 // The producer keeps changing/reusing these buffers before the consumer runs.
 // More than eight batches exercises bounded backpressure and slot reuse.
 for(unsigned i=0;i<500;++i){
  vertices[0].position[0]=float(i);matrix[0]=float(i+1000);material.tint[0]=float(i+2000);material.bones.back()[3]=float(i+4000);
  assert(r.draw_scene(0,vertices,indices,matrix,raster,material));
  vertices[0].position[0]=-1;matrix[0]=-1;material.tint[0]=-1;material.bones.back()[3]=-1;
 }
 std::vector<TextureCopy> list(3);for(unsigned i=0;i<3;++i){list[i].sourceRect[0]=float(i+3000);list[i].alphaOnly=true;list[i].preserveDestination=true;list[i].destinationRect={80*i,160,80,74};list[i].clearSource=true;list[i].clearColor={float(i),.5f,0,1};}
 assert(r.copy_textures(list));for(auto& x:list)x={};list.clear();
 assert(r.select_target(0)&&r.clear_depth()&&r.clear_color({0,0,0,1}));
 // A command larger than a slot borrows only until its synchronous completion.
 std::vector<ModelVertex> large(33000);large[0].position[0]=987;matrix[0]=654;material.tint[0]=321;material.bones.back()[3]=7654;
 assert(r.draw_scene(0,large,indices,matrix,raster,material));large[0].position[0]=-1;
 assert(r.end_frame()&&!r.frame_active()&&r.error_code()==0);
 assert(geometry.size()==501&&uniforms.size()==501);
 for(unsigned i=0;i<500;++i){assert(geometry[i]==float(i));assert(uniforms[i]==float(i+1000));assert(materials[i]==float(i+2000));assert(palettes[i]==float(i+4000));}
 assert(geometry.back()==987&&uniforms.back()==654&&materials.back()==321&&palettes.back()==7654);
 assert((copies==std::vector<float>{3000,3001,3002}));
 for(unsigned i=0;i<3;++i){assert(copiedRegions[i].alphaOnly&&copiedRegions[i].preserveDestination);assert((copiedRegions[i].destinationRect==std::array<uint32_t,4>{80*i,160,80,74}));assert(copiedRegions[i].clearSource&&(copiedRegions[i].clearColor==std::array<float,4>{float(i),.5f,0,1}));}
 std::vector<uint8_t> pixels;assert(r.readback_presented_rgba8(pixels)&&pixels[0]==42);
 assert(r.completed_timing().draws==501&&r.completed_timing().globalFinishes==0);
 assert(r.completed_timing().frontendWaitUs>0&&r.completed_timing().commandBytes>500*sizeof(SceneMaterial)&&r.completed_timing().slotWaits>0);
 assert(r.completed_timing().commandVertexBytes==500*vertices.size()*sizeof(ModelVertex));
 assert(r.completed_timing().commandIndexBytes==500*indices.size()*sizeof(uint16_t));
 assert(r.completed_timing().commandCopyNs>0);
 assert(r.pending_frontend_wait_us()==r.completed_timing().frontendWaitUs);
 // A loading abort drains the deferred commands and closes the frame on the
 // owner, while preserving the previously displayed image. The next pass
 // resumes normal presentation.
 assert(frames==1&&r.begin_frame());
 assert(r.draw_scene(19,{},{},matrix,raster,material));
 assert(r.end_frame(false)&&!r.frame_active()&&frames==1&&discardedFrames==1);
 assert(r.begin_frame()&&r.end_frame()&&frames==2);
 // A failed queued draw must prevent presentation even when the core's later
 // end_frame succeeds. The first failure remains latched until shutdown.
 auto expectFailedFrame=[&](const char* operation,int code,unsigned priorFrames,unsigned priorDiscards){
  assert(!r.end_frame()&&!r.frame_active());
  assert(frames==priorFrames&&discardedFrames==priorDiscards+1);
  assert(r.error_code()==code&&std::strcmp(r.last_error(),operation)==0);
  assert(!r.begin_frame()&&!r.frame_active());
 };
 auto restart=[&]{
  r.shutdown();gpuFailure=false;failedCall=FailedCall::None;modelAdmissionFailure=false;
  assert(!initialized&&r.initialize(true)&&r.initialize_scene_program());
  assert(r.error_code()==0&&std::strcmp(r.last_error(),"none")==0);
 };
 // A borrowed bitmap is a synchronous owner transaction, independent of the
 // scene frame. It drains earlier commands and leaves later ring slots usable.
 TextureUpload bitmap;bitmap.width=bitmap.stride=8;bitmap.height=2;bitmap.mipCount=1;
 bitmap.storage=TextureStorage::Rgba8;bitmap.pixels.resize(64);
 for(size_t i=0;i<bitmap.pixels.size();++i)bitmap.pixels[i]=uint8_t(i*3);
 const auto bitmapPixels=bitmap.pixels;
 const std::array<float,4> bitmapRect{90,68,480,136};
 const auto beforeIdleBegin=beginCalls,beforeIdleEnd=endCalls,beforeIdleFrames=frames;
 const auto beforeIdleBitmaps=completedBitmaps.load(std::memory_order_acquire);
 assert(r.present_bitmap(bitmap,bitmapRect));
 assert(completedBitmaps.load(std::memory_order_acquire)==beforeIdleBitmaps+1);
 assert(!r.frame_active()&&!bitmapRequests.back().active&&bitmapRequests.back().pixels==bitmapPixels);
 assert(beginCalls==beforeIdleBegin&&endCalls==beforeIdleEnd&&frames==beforeIdleFrames);
 assert(r.begin_frame());
 const auto activeBegin=beginCalls,activeEnd=endCalls,activeFrames=frames;
 const auto eventStart=displayEvents.size(),drawStart=uniforms.size();
 assert(r.set_black(true)&&r.select_target(77));
 for(unsigned i=0;i<37;++i){
  vertices[0].position[0]=float(i+8000);matrix[0]=float(i+9000);
  assert(r.draw_scene(0,vertices,indices,matrix,raster,material));
  vertices[0].position[0]=-1;matrix[0]=-1;
 }
 const auto beforeActiveBitmaps=completedBitmaps.load(std::memory_order_acquire);
 assert(r.present_bitmap(bitmap,bitmapRect));
 assert(completedBitmaps.load(std::memory_order_acquire)==beforeActiveBitmaps+1);
 const auto& firstActiveBitmap=bitmapRequests.back();
 assert(firstActiveBitmap.active&&firstActiveBitmap.target==77&&firstActiveBitmap.precedingDraws==drawStart+37);
 assert(firstActiveBitmap.pixels==bitmapPixels&&firstActiveBitmap.destination==bitmapRect);
 assert(r.frame_active()&&active&&selectedTarget==77&&beginCalls==activeBegin&&endCalls==activeEnd&&frames==activeFrames);
 bitmap.pixels.assign(64,249);
 assert(firstActiveBitmap.pixels==bitmapPixels);
 assert(r.set_black(false));
 // More than eight batches after the mid-frame presentation exercises the
 // producer's immutable ring independently of VitaGL's swapped scratch pools.
 for(unsigned i=0;i<257;++i){
  vertices[0].position[0]=float(i+10000);matrix[0]=float(i+11000);
  assert(r.draw_scene(0,vertices,indices,matrix,raster,material));
  vertices[0].position[0]=-1;matrix[0]=-1;
 }
 assert(r.present_bitmap(bitmap,bitmapRect));
 assert(bitmapRequests.back().precedingDraws==drawStart+294&&bitmapRequests.back().pixels==bitmap.pixels);
 assert((std::vector<std::pair<char,bool>>(displayEvents.begin()+eventStart,displayEvents.end())==
         std::vector<std::pair<char,bool>>{{'b',true},{'m',true},{'b',false},{'m',true}}));
 assert(beginCalls==activeBegin&&endCalls==activeEnd&&frames==activeFrames&&selectedTarget==77);
 for(unsigned i=0;i<37;++i)assert(uniforms[drawStart+i]==float(i+9000));
 for(unsigned i=0;i<257;++i)assert(uniforms[drawStart+37+i]==float(i+11000));
 assert(r.end_frame(false)&&!r.frame_active()&&frames==activeFrames);
 assert(r.begin_frame()&&r.end_frame()&&frames==activeFrames+1);
 gpuFailure=true;assert(r.begin_frame());assert(r.draw_scene(19,{},{},matrix,raster,material));
 expectFailedFrame("draw_scene",1282,frames,discardedFrames);
 gpuFailure=false;r.release_model(19);r.release_texture(17);r.shutdown();assert(released==2&&!initialized);
 // Target/copy/clear commands can fail validation without setting a GL error.
 // A following successful draw must not erase either the failure or its name.
 const std::array<std::pair<FailedCall,const char*>,6> failures{{
  {FailedCall::SelectTarget,"select_target"},{FailedCall::CopyTexture,"copy_texture"},
  {FailedCall::CopyTextures,"copy_textures"},{FailedCall::ClearColor,"clear_color"},
  {FailedCall::ClearDepth,"clear_depth"},{FailedCall::SetBlack,"set_black"}
 }};
 TextureCopy region;region.destination=18;region.sourceRect={0,0,16,16};
 for(const auto& failure:failures){
  restart();assert(r.begin_frame());failedCall=failure.first;
  switch(failure.first){
   case FailedCall::SelectTarget:assert(r.select_target(18));break;
   case FailedCall::CopyTexture:assert(r.copy_texture(18,region.sourceRect));break;
   case FailedCall::CopyTextures:assert(r.copy_textures({&region,1}));break;
   case FailedCall::ClearColor:assert(r.clear_color({0,0,0,1}));break;
   case FailedCall::ClearDepth:assert(r.clear_depth());break;
   case FailedCall::SetBlack:assert(r.set_black(true));break;
   case FailedCall::PresentBitmap:case FailedCall::SetLoading:case FailedCall::LoadingTick:case FailedCall::BeginFrame:case FailedCall::EndFrame:case FailedCall::None:assert(false);break;
  }
  assert(r.draw_scene(19,{},{},matrix,raster,material));
  expectFailedFrame(failure.second,-1,frames,discardedFrames);
 }
 // A failed queued command prevents bitmap publication and preserves the
 // original diagnostic. A bitmap's own failure uses that same failure latch.
 restart();assert(r.begin_frame());failedCall=FailedCall::SelectTarget;
 const auto beforeRejectedBitmap=bitmapRequests.size();
 assert(r.select_target(18)&&!r.present_bitmap(bitmap,bitmapRect));
 assert(bitmapRequests.size()==beforeRejectedBitmap&&r.frame_active()&&active);
 expectFailedFrame("select_target",-1,frames,discardedFrames);
 restart();assert(r.begin_frame());failedCall=FailedCall::PresentBitmap;
 const auto beforeFailedBitmap=completedBitmaps.load(std::memory_order_acquire);
 assert(!r.present_bitmap(bitmap,bitmapRect)&&r.frame_active()&&active);
 assert(completedBitmaps.load(std::memory_order_acquire)==beforeFailedBitmap);
 expectFailedFrame("present_bitmap",-1,frames,discardedFrames);
 restart();failedCall=FailedCall::PresentBitmap;
 const auto beforeIdleFailureFrames=frames,beforeIdleFailureEnd=endCalls;
 assert(!r.present_bitmap(bitmap,bitmapRect)&&!r.frame_active()&&!active);
 assert(!r.begin_frame()&&frames==beforeIdleFailureFrames&&endCalls==beforeIdleFailureEnd);
 assert(r.error_code()==-1&&std::strcmp(r.last_error(),"present_bitmap")==0);
 // Borrowed oversized commands return their result synchronously and still
 // participate in the same frame-failure latch and presentation barrier.
 restart();assert(r.begin_frame());gpuFailure=true;
 assert(!r.draw_scene(0,large,indices,matrix,raster,material)&&r.error_code()==1282);
 expectFailedFrame("draw_scene",1282,frames,discardedFrames);
 restart();assert(r.begin_frame());failedCall=FailedCall::CopyTextures;
 std::vector<TextureCopy> largeCopies(2*1024*1024/sizeof(TextureCopy)+1,region);
 assert(!r.copy_textures(largeCopies)&&r.error_code()==-1);
 expectFailedFrame("copy_textures",-1,frames,discardedFrames);
 // Resource admission can fail and leave a diagnostic while the caller uses
 // dynamic geometry successfully. That handled fallback must still present.
 restart();assert(r.begin_frame());modelAdmissionFailure=true;
 assert(r.create_model(vertices,indices)==0&&r.error_code()==1285);modelAdmissionFailure=false;
 const auto beforeFallback=frames;
 assert(r.draw_scene(0,vertices,indices,matrix,raster,material)&&r.end_frame());
 assert(frames==beforeFallback+1&&r.error_code()==1285);
 assert(r.begin_frame()&&r.end_frame()&&frames==beforeFallback+2);
 restart();assert(r.begin_frame()&&r.end_frame());r.shutdown();
 // Quiet frame packs must not make the frontend wait for each GPU-owned
 // swap. The second complete pack may queue while the first end is held.
 restart();
 const auto asynchronousFrames=frames;
 frameEndEntered.store(false,std::memory_order_release);
 holdFrameEnd.store(true,std::memory_order_release);
 assert(r.begin_frame(nullptr,true));
 assert(r.draw_scene(19,{},{},matrix,raster,material));
 assert(r.end_frame(true,true));
 eventually([]{return frameEndEntered.load(std::memory_order_acquire);});
 assert(frames==asynchronousFrames);
 assert(r.begin_frame(nullptr,true));
 assert(r.draw_scene(19,{},{},matrix,raster,material));
 assert(r.end_frame(true,true));
 assert(frames==asynchronousFrames);
 holdFrameEnd.store(false,std::memory_order_release);
 std::vector<uint8_t> asynchronousReadback;
 assert(r.readback_presented_rgba8(asynchronousReadback));
 assert(frames==asynchronousFrames+2 && asynchronousReadback.size()==4);
 for(unsigned frame=0;frame<64;++frame){
  assert(r.begin_frame(nullptr,true));
  assert(r.draw_scene(19,{},{},matrix,raster,material));
  assert(r.end_frame(true,true));
 }
 assert(r.readback_presented_rgba8(asynchronousReadback));
 assert(frames==asynchronousFrames+66);
 r.shutdown();
 // Exercise the in-place constructor on both resident and streamed commands,
 // including a capacity-triggered publish well before the 16-draw threshold.
 restart();assert(r.begin_frame());
 for(unsigned draw=0;draw<PayloadDraws;++draw)assert(r.select_target(77u+draw%2));
 assert(r.end_frame(false));
 const auto controlBytes=r.completed_timing().commandBytes;
 std::vector<ModelVertex> payloadVertices(PayloadVertices);
 size_t expectedCommandBytes=controlBytes;
 verifyDrawPayloads=true;verifiedDrawPayloads=0;
 assert(r.begin_frame());
 for(unsigned draw=0;draw<PayloadDraws;++draw) {
  const uint32_t model=draw%2?19u:0u;
  auto drawMatrix=payload_matrix(draw);auto drawRaster=payload_raster(draw);
  auto drawMaterial=payload_material(draw);
  std::array<uint16_t,3> drawIndices{0,1,2};
  payloadVertices.front()=payload_vertex(draw,false);
  payloadVertices.back()=payload_vertex(draw,true);
  assert(r.select_target(77u+draw%2));
  assert(r.draw_scene(model,payloadVertices,drawIndices,drawMatrix,drawRaster,drawMaterial));
  const size_t extra=model?0:payloadVertices.size()*sizeof(ModelVertex)+sizeof(drawIndices);
  expectedCommandBytes+=(16+sizeof(DrawLayout)+extra+15)&~size_t(15);
  // These inputs die or change while queued commands still own their copies.
  drawMatrix.fill(-1);drawRaster={};drawMaterial={};drawIndices.fill(99);
  payloadVertices.front()={};payloadVertices.back()={};
 }
 assert(r.end_frame(false));
 assert(verifiedDrawPayloads==PayloadDraws&&r.completed_timing().commandBytes==expectedCommandBytes);
 assert(r.completed_timing().commandVertexBytes==PayloadDraws/2*PayloadVertices*sizeof(ModelVertex));
 assert(r.completed_timing().commandIndexBytes==PayloadDraws/2*3*sizeof(uint16_t));
 verifyDrawPayloads=false;r.shutdown();
 // Loading OFF leaves the owner's original indefinite wait untouched. In
 // particular ordinary rendering has never queried the loading core at all.
 assert(loadingTicks==0&&loadingQueries==0&&clockQueries==0&&timedWaitCalls==0);
 restart();
 LoadingIndicator loading;
 loading.images[0]=bitmap;loading.images[1]=bitmap;
 loading.rectangles={{{432,464,48,39},{468,464,48,39},{504,464,48,39}}};
 loading.waitRetraces=120;loading.phaseRetraces=19;
 const auto savedPixels=loading.images[0].pixels;
 const auto loadingBeginCalls=beginCalls,loadingEndCalls=endCalls;
 assert(r.set_loading(true,loading));
 assert(!r.frame_active()&&!loadingRequests.back().active);
 assert(loadingRequests.back().request.waitRetraces==120&&loadingRequests.back().request.phaseRetraces==19);
 assert(loadingRequests.back().request.rectangles==loading.rectangles);
 loading.images[0].pixels.assign(64,7);
 assert(loadingRequests.back().request.images[0].pixels==savedPixels);
 // The game thread may block in synchronous loading: the same GPU owner keeps
 // ticking without a single new command or game frame from the producer.
 eventually([&]{return loadingTicks.load(std::memory_order_acquire)>=3;});
 assert(beginCalls==loadingBeginCalls&&endCalls==loadingEndCalls);
 assert(r.set_loading(false,{}));
 const auto stoppedTicks=loadingTicks.load(std::memory_order_acquire);
 const auto stoppedQueries=loadingQueries.load(std::memory_order_acquire);
 const auto stoppedClock=clockQueries.load(std::memory_order_acquire);
 const auto stoppedWaits=timedWaitCalls.load(std::memory_order_acquire);
 std::this_thread::sleep_for(std::chrono::milliseconds(45));
 assert(loadingTicks==stoppedTicks&&loadingQueries==stoppedQueries&&clockQueries==stoppedClock&&timedWaitCalls==stoppedWaits);
 // A rearm drains preceding commands, preserves an open frame/target, and
 // leaves the ring available. A continuously busy queue cannot starve ticks.
 assert(r.begin_frame()&&r.select_target(77));
 const auto preceding=uniforms.size();
 for(unsigned i=0;i<37;++i)assert(r.draw_scene(19,{},{},matrix,raster,material));
 assert(r.set_loading(true,loading));
 assert(loadingRequests.back().active&&loadingRequests.back().target==77);
 assert(loadingRequests.back().precedingDraws==preceding+37&&r.frame_active());
 const auto queueTicks=loadingTicks.load(std::memory_order_acquire);
 for(unsigned i=0;i<500;++i)assert(r.draw_scene(19,{},{},matrix,raster,material));
 assert(r.end_frame(false));
 assert(loadingTicks.load(std::memory_order_acquire)>queueTicks);
 assert(r.set_loading(false,{}));
 // OFF is a cleanup transaction even when a previous command has latched a
 // failure. Neither a fresh arm nor a later tick may erase that diagnostic.
 assert(r.begin_frame()&&r.set_loading(true,loading));
 failedCall=FailedCall::SelectTarget;assert(r.select_target(18));
 const auto offBeforeFailure=loadingStops.load(std::memory_order_acquire);
 assert(r.set_loading(false,{}));
 assert(loadingStops>=offBeforeFailure+1&&!loadingArmed);
 assert(r.error_code()==-1&&std::strcmp(r.last_error(),"select_target")==0);
 const auto rejectedArms=loadingRequests.size();
 assert(!r.set_loading(true,loading)&&loadingRequests.size()==rejectedArms);
 expectFailedFrame("select_target",-1,frames,discardedFrames);
 restart();failedCall=FailedCall::SetLoading;
 assert(!r.set_loading(true,loading)&&!loadingArmed);
 assert(r.error_code()==-1&&std::strcmp(r.last_error(),"set_loading")==0);
 assert(r.set_loading(false,{})&&r.error_code()==-1);
 restart();failedCall=FailedCall::LoadingTick;
 assert(r.set_loading(true,loading));
 eventually([&]{return r.error_code()!=0;});
 assert(r.error_code()==1282&&std::strcmp(r.last_error(),"loading_tick")==0);
 const auto failedTicks=loadingTicks.load(std::memory_order_acquire);
 std::this_thread::sleep_for(std::chrono::milliseconds(45));
 assert(loadingTicks==failedTicks&&r.set_loading(false,{}));
 // Operating-system failures use the same first-error latch and clean up the
 // core immediately, even while the producer is idle in synchronous loading.
 for(unsigned failure=0;failure<3;++failure) {
  restart();
  const auto stops=loadingStops.load(std::memory_order_acquire);
  if(failure==0) {
   failClock=true;
   assert(!r.set_loading(true,loading));
  } else {
   assert(r.set_loading(true,loading));
   if(failure==1)failClock=true;else failTimedWait=true;
   eventually([&]{return r.error_code()!=0;});
  }
  const char* operation=failure==2?"loading_wait":"loading_clock";
  assert(r.error_code()==(failure==2?-EINVAL:-EIO)&&std::strcmp(r.last_error(),operation)==0);
  assert(loadingStops.load(std::memory_order_acquire)>stops);
  const auto ticks=loadingTicks.load(std::memory_order_acquire);
  const auto clocks=clockQueries.load(std::memory_order_acquire);
  std::this_thread::sleep_for(std::chrono::milliseconds(45));
  assert(loadingTicks==ticks&&clockQueries==clocks&&r.set_loading(false,{}));
 }
 restart();assert(r.set_loading(true,loading));
 r.shutdown();
 const auto shutdownTicks=loadingTicks.load(std::memory_order_acquire);
 std::this_thread::sleep_for(std::chrono::milliseconds(45));
 assert(loadingTicks==shutdownTicks&&!initialized);
 restart();assert(r.begin_frame()&&r.end_frame());r.shutdown();
 assert(!systemClockQueries.load()); // Every pre-existing case used a null timeline.
 check_frame_timeline_deferred();
 if(!check_deferred_slot_acquire(false))return 64;
 std::puts("Deferred frontend: GPU owner, immutable payloads, bounded slot reuse, isolated bitmap, monotonic loading timer/queue/disarm, ordered blanking, failed-frame discard and restart passed");
}
