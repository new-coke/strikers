#include "port/deferred_renderer.hpp"
#include "port/host.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <pthread.h>
#include <cassert>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
using namespace port::render;
namespace {
pthread_t caller,owner;
bool initialized=false,active=false;
std::atomic<bool> gpuFailure{false};
int driverError=0;
std::vector<float> geometry,uniforms,materials,copies;
std::vector<TextureCopy> copiedRegions;
unsigned released=0,frames=0,discardedFrames=0;
void owned(){assert(initialized&&pthread_equal(pthread_self(),owner)&&!pthread_equal(owner,caller));}
}
extern "C" {
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
bool Renderer::initialize(bool) noexcept {owner=pthread_self();initialized=true;driverError=0;owned();return true;}
bool Renderer::initialize_scene_program() noexcept {owned();return true;}
uint32_t Renderer::create_texture(const TextureUpload&) noexcept {owned();return 17;}
uint32_t Renderer::create_target(uint32_t,uint32_t) noexcept {owned();return 18;}
uint32_t Renderer::create_model(std::span<const ModelVertex> v,std::span<const uint16_t> i) noexcept {owned();assert(v.size()==3&&i.size()==3);return 19;}
void Renderer::release_texture(uint32_t) noexcept {owned();++released;}
void Renderer::release_model(uint32_t) noexcept {owned();++released;}
bool Renderer::draw_scene(uint32_t model,std::span<const ModelVertex> v,std::span<const uint16_t> i,const std::array<float,16>& m,const Raster&,const SceneMaterial& material) noexcept {
 owned();assert(active);
 if(!model){assert(!v.empty()&&i.size()==3&&i[0]==0&&i[1]==1&&i[2]==2);geometry.push_back(v[0].position[0]);}
 uniforms.push_back(m[0]);materials.push_back(material.tint[0]);
 std::this_thread::sleep_for(std::chrono::microseconds(150));
 if(gpuFailure){driverError=1282;return false;}return true;
}
bool Renderer::select_target(uint32_t,bool,bool) noexcept {owned();return true;}
bool Renderer::copy_texture(uint32_t,std::array<float,4>,bool) noexcept {owned();return true;}
bool Renderer::copy_textures(std::span<const TextureCopy> list) noexcept {owned();for(auto x:list){copies.push_back(x.sourceRect[0]);copiedRegions.push_back(x);}return true;}
bool Renderer::clear_color(std::array<float,4>,bool) noexcept {owned();return true;}
bool Renderer::clear_depth() noexcept {owned();return true;}
bool Renderer::frame_active() const noexcept {owned();return active;}
bool Renderer::begin_frame() noexcept {owned();assert(!active);active=true;return true;}
bool Renderer::end_frame(bool present) noexcept {owned();assert(active);active=false;if(present)++frames;else ++discardedFrames;return driverError==0;}
bool Renderer::readback_presented_rgba8(std::vector<uint8_t>& out) noexcept {owned();assert(!active);out.assign(4,42);return true;}
void Renderer::shutdown() noexcept {if(initialized){owned();initialized=false;}}
const char* Renderer::last_error() const noexcept {owned();return driverError?"driver_failure":"none";}
int Renderer::error_code() const noexcept {owned();return driverError;}
Renderer::FrameTiming Renderer::completed_timing() const noexcept {owned();FrameTiming t;t.draws=uniforms.size();return t;}
}
int main(){
 caller=pthread_self();DeferredRenderer r;
 assert(r.initialize(true)&&r.initialize_scene_program());assert(r.initialize());
 TextureUpload upload;assert(r.create_texture(upload)==17&&r.create_target(960,544)==18);
 std::array<ModelVertex,3> vertices{};std::array<uint16_t,3> indices{0,1,2};
 assert(r.create_model(vertices,indices)==19&&r.begin_frame()&&r.frame_active());
 std::array<float,16> matrix{};Raster raster;SceneMaterial material;
 // The producer keeps changing/reusing these buffers before the consumer runs.
 // More than eight batches exercises bounded backpressure and slot reuse.
 for(unsigned i=0;i<500;++i){
  vertices[0].position[0]=float(i);matrix[0]=float(i+1000);material.tint[0]=float(i+2000);
  assert(r.draw_scene(0,vertices,indices,matrix,raster,material));
  vertices[0].position[0]=-1;matrix[0]=-1;material.tint[0]=-1;
 }
 std::vector<TextureCopy> list(3);for(unsigned i=0;i<3;++i){list[i].sourceRect[0]=float(i+3000);list[i].alphaOnly=true;list[i].preserveDestination=true;list[i].destinationRect={80*i,160,80,74};}
 assert(r.copy_textures(list));for(auto& x:list)x={};list.clear();
 assert(r.select_target(0)&&r.clear_depth()&&r.clear_color({0,0,0,1}));
 // A command larger than a slot borrows only until its synchronous completion.
 std::vector<ModelVertex> large(33000);large[0].position[0]=987;matrix[0]=654;material.tint[0]=321;
 assert(r.draw_scene(0,large,indices,matrix,raster,material));large[0].position[0]=-1;
 assert(r.end_frame()&&!r.frame_active()&&r.error_code()==0);
 assert(geometry.size()==501&&uniforms.size()==501);
 for(unsigned i=0;i<500;++i){assert(geometry[i]==float(i));assert(uniforms[i]==float(i+1000));assert(materials[i]==float(i+2000));}
 assert(geometry.back()==987&&uniforms.back()==654&&materials.back()==321);
 assert((copies==std::vector<float>{3000,3001,3002}));
 for(unsigned i=0;i<3;++i){assert(copiedRegions[i].alphaOnly&&copiedRegions[i].preserveDestination);assert((copiedRegions[i].destinationRect==std::array<uint32_t,4>{80*i,160,80,74}));}
 std::vector<uint8_t> pixels;assert(r.readback_presented_rgba8(pixels)&&pixels[0]==42);
 assert(r.completed_timing().draws==501&&r.completed_timing().globalFinishes==0);
 assert(r.completed_timing().frontendWaitUs>0&&r.completed_timing().commandBytes>500*sizeof(SceneMaterial)&&r.completed_timing().slotWaits>0);
 // A loading abort drains the deferred commands and closes the frame on the
 // owner, while preserving the previously displayed image. The next pass
 // resumes normal presentation.
 assert(frames==1&&r.begin_frame());
 assert(r.draw_scene(19,{},{},matrix,raster,material));
 assert(r.end_frame(false)&&!r.frame_active()&&frames==1&&discardedFrames==1);
 assert(r.begin_frame()&&r.end_frame()&&frames==2);
 // Deferred driver errors become visible at the frame completion barrier.
 gpuFailure=true;assert(r.begin_frame());assert(r.draw_scene(19,{},{},matrix,raster,material));assert(!r.end_frame());assert(r.error_code()==1282);
 gpuFailure=false;r.release_model(19);r.release_texture(17);r.shutdown();assert(released==2&&!initialized);
 assert(r.initialize()&&r.initialize_scene_program());r.shutdown();
 std::puts("Deferred frontend: GPU owner, immutable geometry/uniform/copy payloads, bounded slot reuse, oversized borrowed lifetime, frame barriers, errors and lifecycle passed");
}
