#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_match_direct.hpp"
#include "port/deferred_renderer.hpp"
#include "port/native_model_sources.hpp"
#include "port/native_match_transform.hpp"
#include "port/scene_texture_sampling.hpp"
#include "port/scene_specular.hpp"
#include "port/native_assets.hpp"
#include "Game/GL/ShaderSkinMesh.h"
#include "NL/gl/glState.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glLightUserData.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/glx/glxMemory.h"
#include "port/aspect.h"
#include "port/framerate.h"
#include "port/config.h"
#include "port/host.h"
#include "NL/nlTask.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <cstdlib>
#include <unordered_map>
extern "C" int port_region_owns(const void*);
extern u8 glx_bLoadingIndicator;
namespace port::directmatch {
namespace {
using namespace nativegxm;
using Renderer=render::DeferredRenderer;using render::Raster;using render::SceneMaterial;using render::TextureCopy;
bool active=false,started=false;Renderer gpu;ModelSourceRegistry sources;
bool frameDiscarded=false;
uint64_t frameDrawStart=0,frameStartNs=0;
// Scratch invalidation snapshots copy only their live prefix. Keep this POD
// free of default member initializers: otherwise every draw zeros 32 KiB
// before discovering that there are no pending writes.
struct Range {uintptr_t address;size_t size;};
bool overlaps(Range a,Range b){return a.address<=b.address?b.address-a.address<a.size:a.address-b.address<b.size;}
struct Geometry {uint32_t id=0,level=0;uint64_t generation=0;std::vector<Range> ranges;};
struct Texture {uint32_t id=0,level=0;const PlatTexture* source=nullptr;Range pixels{},palette{};unsigned width=0,height=0,levels=0,format=0;bool target=false;int shadowPartition=-1;};
uint32_t shadowAtlas=0;
std::unordered_map<uint64_t,Geometry> geometry;
std::unordered_map<const PlatTexture*,Texture> textures;
std::mutex writeMutex;std::array<Range,4096> writes{};unsigned writeCount=0;bool invalidateAll=false;
std::array<float,4> viewFog{},viewFogColor{};
Renderer::FrameTiming timings{};uint64_t timingFrames=0;
uint64_t draws=0,textured=0,hud=0,characters=0,invalid=0,decoded=0,records=0,resident=0,dynamic=0;
const char* failure="none";
void trace_present(bool presented) {
 if(!PortDiagnosticsEnabled())return;
 static const bool enabled=[] {const auto* value=std::getenv("STRIKERS_VITAGL_TRANSITION_TRACE");return value&&value[0]=='1';}();
 if(!enabled)return;
 static FILE* file=std::fopen("ux0:data/strikersVita/native_transition_trace.csv","w");
 static bool header=false;static unsigned frames=0;
 if(!file)return;
 if(++frames>2400){std::fclose(file);file=nullptr;return;}
 if(!header){std::fprintf(file,"frame,start_ns,end_ns,state,loading,discarded,presented,draws\n");header=true;}
 std::fprintf(file,"%u,%llu,%llu,%u,%u,%u,%u,%llu\n",unsigned(glGetCurrentFrame()),frameStartNs,port_monotonic_ns(),unsigned(nlTaskManager::m_pInstance->m_CurrState),unsigned(glx_bLoadingIndicator),unsigned(frameDiscarded),unsigned(presented),draws-frameDrawStart);
 std::fflush(file);
}
std::array<const glModelPacket*,64> ballPackets{};unsigned ballPacketCount=0,ballPacketFrame=UINT32_MAX;
void consume_writes(){
 std::array<Range,4096> changed;unsigned n;bool all;{std::lock_guard<std::mutex> guard(writeMutex);n=writeCount;std::copy_n(writes.begin(),n,changed.begin());all=invalidateAll;writeCount=0;invalidateAll=false;}
 if(!n&&!all)return;
 for(auto it=geometry.begin();it!=geometry.end();){bool dirty=all;for(unsigned i=0;i<n&&!dirty;++i)for(auto range:it->second.ranges)if(overlaps(range,changed[i])){dirty=true;break;}if(dirty){gpu.release_model(it->second.id);it=geometry.erase(it);}else ++it;}
 for(auto it=textures.begin();it!=textures.end();){if(it->second.target){++it;continue;}bool dirty=all;for(unsigned i=0;i<n&&!dirty;++i)dirty=overlaps(it->second.pixels,changed[i])||overlaps(it->second.palette,changed[i]);if(dirty){gpu.release_texture(it->second.id);it=textures.erase(it);}else ++it;}
}
const void* userdata(const glModelPacket& p,eGLUserData type){if(!p.userData)return nullptr;auto handle=reinterpret_cast<const uintptr_t*>(p.userData)[type];return handle?glUserGetData(reinterpret_cast<const void*>(handle)):nullptr;}
void report(){auto* f=std::fopen("ux0:data/strikersVita/vitagl_match_status.csv","wb");if(!f)return;
 std::fprintf(f,"# vitagl_match_v8 backend=VitaGL deferred=RE4 aurora=0 gx_fifo=0 texture_decoder=owned skinning=gpu views=34\nframe,draws,textured,hud,characters,invalid,native_records,native_decoded,resident,dynamic,gpu_error,operation,failure\n%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%d,%s,%s\n",unsigned(glGetCurrentFrame()),draws,textured,hud,characters,invalid,records,decoded,resident,dynamic,gpu.error_code(),gpu.last_error(),failure);std::fclose(f);
 if(timingFrames){f=std::fopen("ux0:data/strikersVita/native_renderer_timing.csv","w");if(f){std::fprintf(f,"frames,fence_us,upload_us,draw_calls_us,queue_us,draws,dynamic_bytes,frontend_wait_us,command_bytes,slot_waits\n%llu,%llu,%llu,%llu,%llu,%u,%llu,%llu,%llu,%u\n",timingFrames,timings.fenceUs,timings.uploadUs,timings.drawUs,timings.queueUs,timings.draws,timings.dynamicVertexBytes,timings.frontendWaitUs,timings.commandBytes,timings.slotWaits);std::fclose(f);}}
}
uint32_t texture_for(const PlatTexture* p){
 if(!p||p->m_Magic!=0x50544558||!p->m_SwizzledData||!p->m_Width||!p->m_Height||unsigned(p->m_Format)>8)return 0;
 auto existing=textures.find(p);if(existing!=textures.end())return existing->second.id;
 constexpr TextureEncoding formats[]{TextureEncoding::Rgb565,TextureEncoding::Rgb5a3,TextureEncoding::Cmpr,TextureEncoding::Rgba8,TextureEncoding::I8,TextureEncoding::I4,TextureEncoding::I8,TextureEncoding::IA8,TextureEncoding::C8};
 TextureSource source;source.width=p->m_Width;source.height=p->m_Height;source.mipCount=std::max(1u,unsigned(p->m_Levels));source.encoding=formats[p->m_Format];
 const size_t bytes=encoded_texture_bytes(source.width,source.height,source.encoding,source.mipCount);if(!bytes)return 0;
 source.pixels={static_cast<const uint8_t*>(p->m_SwizzledData),bytes};
 if(p->m_Format==GXTex_CI8){if(!p->m_PaletteData||p->m_nPaletteEntries<=0)return 0;source.palette={reinterpret_cast<const uint8_t*>(p->m_PaletteData),size_t(p->m_nPaletteEntries)*2};source.paletteEncoding=PaletteEncoding::Rgb5a3;}
 TextureUpload upload;std::vector<uint8_t> request,record;bool loaded=false;
 if(make_texture_request(source,request)&&read_native_asset(texture_record_path(request).c_str(),record,nullptr)&&parse_texture_record(record,request,upload)){loaded=true;++records;}
 if(!loaded){if(!decode_texture(source,upload)){failure="texture_decode";return 0;}++decoded;}
 const uint32_t id=gpu.create_texture(upload);if(!id){failure="texture_upload";return 0;}
 textures.emplace(p,Texture{id,uint32_t(glx_GetTexMarkerLevel()),p,{reinterpret_cast<uintptr_t>(source.pixels.data()),source.pixels.size()},{reinterpret_cast<uintptr_t>(source.palette.data()),source.palette.size()},source.width,source.height,source.mipCount,unsigned(source.encoding)});return id;
}
std::array<float,4> vector4(const nlVector4& v){return {v.x,v.y,v.z,v.w};}
void transform_point(std::array<float,4>& out,const float* xyz,const nlMatrix4& m,bool direction){for(unsigned k=0;k<3;++k)out[k]=xyz[0]*m.e[k]+xyz[1]*m.e[4+k]+xyz[2]*m.e[8+k]+(direction?0.f:m.e[12+k]);}
}
void enable(bool x) noexcept{active=x;}
void note_ball_packets(const glModel* model) noexcept{
 if(!model||!PortDiagnosticsEnabled())return;const unsigned frame=glGetCurrentFrame();if(frame!=ballPacketFrame){ballPacketCount=0;ballPacketFrame=frame;}
 for(unsigned i=0;i<model->numPackets&&ballPacketCount<ballPackets.size();++i)ballPackets[ballPacketCount++]=model->packets+i;
}
bool enabled() noexcept{return active;}
bool uses_own_renderer() noexcept{return active;}
bool preload() noexcept{if(started)return true;const auto* profile=std::getenv("STRIKERS_VITAGL_TIMING");started=gpu.initialize(profile&&*profile=='1')&&gpu.initialize_scene_program();if(!started){failure="initialize";report();}return started;}
bool begin_frame() noexcept{frameDiscarded=false;frameDrawStart=draws;frameStartNs=PortDiagnosticsEnabled()?port_monotonic_ns():0;consume_writes();return preload()&&gpu.begin_frame();}
bool end_frame() noexcept{bool result=gpu.end_frame(!frameDiscarded);if(result)PortNativeRetrace();trace_present(result&&!frameDiscarded);const auto t=gpu.completed_timing();if(t.drawUs||t.fenceUs){++timingFrames;timings.fenceUs+=t.fenceUs;timings.uploadUs+=t.uploadUs;timings.drawUs+=t.drawUs;timings.queueUs+=t.queueUs;timings.draws+=t.draws;timings.dynamicVertexBytes+=t.dynamicVertexBytes;timings.frontendWaitUs+=t.frontendWaitUs;timings.commandBytes+=t.commandBytes;timings.slotWaits+=t.slotWaits;}if(glGetCurrentFrame()%60==0||!result)report();return result;}
void discard_present() noexcept{frameDiscarded=true;}
void present_native() noexcept{}
void finish() noexcept{}
void clear_depth() noexcept{gpu.clear_depth();}
void set_fog(float start,float end,bool enabled,unsigned type,std::array<float,4> color) noexcept{viewFog={start,end,float(enabled),float(type)};viewFogColor=color;}
void shutdown() noexcept{gpu.shutdown();sources.clear();geometry.clear();textures.clear();shadowAtlas=0;started=false;}
bool readback_presented_rgba8(std::vector<uint8_t>& p) noexcept{return gpu.readback_presented_rgba8(p);}
void on_display_list_created(const glModelPacket* p,uintptr_t list,uint32_t level) noexcept{
 try{const auto* dl=dlGetStruct(list);bool pn=false;for(unsigned i=0;i<p->numStreams;++i)pn|=p->streams[i].id==12;
 if(!sources.capture(list,reinterpret_cast<uintptr_t>(dl->list),level,p->primType,p->numStreams,{reinterpret_cast<const uint16_t*>(p->indexBuffer),p->numVertices},pn)){failure="source_registration";++invalid;}}catch(...){failure="source_allocation";++invalid;}
}
void on_stitch_selectors(const glModelPacket* p,const uint8_t* s,uint32_t n) noexcept {
 bool pn=false;for(unsigned i=0;i<p->numStreams;++i)pn|=p->streams[i].id==12;if(!pn)return;
 try {
  const auto* dl=dlGetStruct(p->indexBuffer);
  if(!dl){failure="skin_selectors";++invalid;return;}
  const auto address=reinterpret_cast<uintptr_t>(dl->list);
  const auto* previous=sources.find(p->indexBuffer,address,p->numVertices,p->primType,p->numStreams,true);
  const auto priorGeneration=previous?previous->generation:0;
  if(!sources.attach_stitch(p->indexBuffer,address,{s,n})){failure="skin_selectors";++invalid;return;}
  const auto* updated=sources.find(p->indexBuffer,address,p->numVertices,p->primType,p->numStreams,true);
  if(priorGeneration&&updated&&updated->generation!=priorGeneration)
   for(auto it=geometry.begin();it!=geometry.end();)
    if(it->second.generation==priorGeneration){gpu.release_model(it->second.id);it=geometry.erase(it);}else ++it;
 }catch(...){++invalid;failure="skin_allocation";}
}
void on_resource_release(uint32_t level) noexcept{sources.release_after(level);for(auto it=geometry.begin();it!=geometry.end();)if(it->second.level>level){gpu.release_model(it->second.id);it=geometry.erase(it);}else ++it;}
void on_texture_release(uint32_t level) noexcept{for(auto it=textures.begin();it!=textures.end();)if(it->second.level>level){if(it->second.id!=shadowAtlas)gpu.release_texture(it->second.id);it=textures.erase(it);}else ++it;}
void notify_write(const void* ptr,size_t bytes) noexcept{if(!ptr||!bytes)return;std::lock_guard<std::mutex> guard(writeMutex);if(writeCount<writes.size())writes[writeCount++]={reinterpret_cast<uintptr_t>(ptr),bytes};else invalidateAll=true;}
void draw_message_bitmap(unsigned width,unsigned height,const uint8_t* bits,uint32_t color,bool centered) noexcept {
 try {
 if(!bits||!width||!height||width>960||height>448||!preload())return;bool own=!gpu.frame_active();if(own&&!gpu.begin_frame())return;
 TextureUpload image;image.width=width;image.height=height;image.stride=(width+7u)&~7u;image.mipCount=1;image.storage=TextureStorage::Rgba8;image.pixels.resize(size_t(image.stride)*height*4);
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto* p=image.pixels.data()+(y*image.stride+x)*4;if(bits[y*(width/8)+x/8]&(1u<<(x%8))){p[0]=(color>>16)&255;p[1]=(color>>8)&255;p[2]=color&255;p[3]=color>>24;}}
 unsigned texture=gpu.create_texture(image);if(!texture)return;SceneMaterial m;m.textures[0]=texture;m.wrapU[0]=m.wrapV[0]=1;m.controls[0]=1;
 const float logical=PortLogicalFrameWidth();float x=(logical-width)*.5f,y=centered?(448-height)*.5f:416.f-height;
 ModelVertex v[4];float l=x/logical*2-1,r=(x+width)/logical*2-1,t=1-y/448*2,b=1-(y+height)/448*2;
 v[0].position={l,t,0,1};v[1].position={r,t,0,1};v[2].position={r,b,0,1};v[3].position={l,b,0,1};v[0].uv={0,0,1};v[1].uv={1,0,1};v[2].uv={1,1,1};v[3].uv={0,1,1};uint16_t indices[]{0,1,2,0,2,3};Raster raster;raster.depthTest=raster.depthWrite=false;raster.blend=1;
 std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};gpu.draw_scene(0,v,indices,identity,raster,m);gpu.release_texture(texture);if(own)gpu.end_frame();
 }catch(...){failure="message_bitmap";}
}
void begin_view(eGLView view) noexcept {
 static std::array<uint32_t,4> targets{};
 const auto* state=glViewGetState(view);uint32_t target=0;
 if(state->target>=GLTG_Staging){auto& t=targets[state->target];if(!t)t=gpu.create_target(960,544);target=t;}
 if(!gpu.select_target(target,state->bClearColour,state->bClearDepth)){failure="view_target";++invalid;}
}
void clear_scene(std::array<float,4> color) noexcept{if(!gpu.clear_color(color)){failure="view_clear";++invalid;}}
void capture_shadow_atlas() noexcept {
 try {std::vector<TextureCopy> copies;copies.reserve(10);const float sx=960.f/PortLogicalFrameWidth(),sy=544.f/448.f;
 for(unsigned i=0;i<10;++i){char name[40];std::snprintf(name,sizeof name,"target/pshadow_updated%02u",i);if(glConstantGet(name).x!=1)continue;
  std::snprintf(name,sizeof name,"target/pshadow%02u",i);auto* p=glx_GetTex(glGetTexture(name),false,false);if(!p)continue;
  if(!shadowAtlas)shadowAtlas=gpu.create_target(320,240);
  if(!shadowAtlas){failure="shadow_target";++invalid;return;}
  auto it=textures.find(p);if(it==textures.end()||it->second.id!=shadowAtlas){
   if(it!=textures.end()){gpu.release_texture(it->second.id);textures.erase(it);}
   Texture entry;entry.id=shadowAtlas;entry.level=glx_GetTexMarkerLevel();entry.source=p;entry.target=true;entry.shadowPartition=i;textures.emplace(p,entry);
  }
  TextureCopy copy{shadowAtlas,{(i%4)*160*sx,(i/4)*148*sy,160*sx,148*sy},74,false,true};
  copy.destinationRect={(i%4)*80,(i/4)*80,80,74};copy.preserveDestination=true;copies.push_back(copy);
 }
 if(!gpu.copy_textures(copies)){failure="shadow_copy";++invalid;}
 }catch(...){failure="shadow_allocation";++invalid;}
}
void capture_texture(uintptr_t handle,unsigned x,unsigned y,unsigned width,unsigned height,bool intensity) noexcept {
 try {
 auto* p=glx_GetTex(handle,false,false);if(!p)return;
 auto it=textures.find(p);uint32_t id=0;
 if(it!=textures.end()&&it->second.target)id=it->second.id;
 else {if(it!=textures.end()){gpu.release_texture(it->second.id);textures.erase(it);}
  id=gpu.create_target(p->m_Width,p->m_Height);if(!id){failure="capture_target";++invalid;return;}
  Texture entry;entry.id=id;entry.level=glx_GetTexMarkerLevel();entry.source=p;entry.target=true;textures.emplace(p,entry);}
 const float sx=960.f/PortLogicalFrameWidth(),sy=544.f/448.f;
 if(!gpu.copy_texture(id,{x*sx,y*sy,width*sx,height*sy},intensity)){failure="capture_copy";++invalid;}
 }catch(...){failure="capture_allocation";++invalid;}
}
struct PacketTrace {
 eGLView view;const glModelPacket& packet;uint64_t initial;
 ~PacketTrace(){if(invalid==initial)return;static unsigned recorded=0;if(recorded++>=32)return;
 auto* f=std::fopen("ux0:data/strikersVita/native_rejected_packets.log",recorded==1?"w":"a");if(!f)return;
 std::fprintf(f,"frame=%u view=%u reason=%s count=%u primitive=%u program=%08x texconfig=%02x raster=%08x gpu=%d\n",unsigned(glGetCurrentFrame()),unsigned(view),failure,packet.numVertices,packet.primType,packet.state.program,packet.state.texconfig,packet.state.raster,gpu.error_code());
 if(packet.streams&&packet.numStreams<=16)for(unsigned i=0;i<packet.numStreams;++i){const auto& a=packet.streams[i];std::fprintf(f,"  stream id=%u address=%08lx stride=%u bytes=%u endian=%u\n",a.id,(unsigned long)a.address,a.stride,a.dataSize,a.beData);}std::fclose(f);
 }
};
void on_packet(eGLView view,unsigned long,const glModelPacket* ptr) noexcept {
 if(!active||!ptr)return;try {
 consume_writes();const auto& p=*ptr;PacketTrace trace{view,p,invalid};if(!p.streams||!p.numStreams||p.numStreams>16||p.numVertices<3||p.primType>3){failure="packet_shape";++invalid;return;}
 bool hasPn=false;for(unsigned i=0;i<p.numStreams;++i)hasPn|=p.streams[i].id==12;
 const ModelSource* source=nullptr;std::span<const uint16_t> original;
 if(p.indexBuffer){const auto* dl=dlGetStruct(p.indexBuffer);if(!dl){if(hasPn){failure="native_indexed_skin";++invalid;return;}original={reinterpret_cast<const uint16_t*>(p.indexBuffer),p.numVertices};}
  else {if(!dl->list){failure="native_list";++invalid;return;}source=sources.find(p.indexBuffer,reinterpret_cast<uintptr_t>(dl->list),p.numVertices,p.primType,p.numStreams,hasPn);
  if(source)original=source->indices;else if(!hasPn&&dl->size>=p.numVertices*2u)original={static_cast<const uint16_t*>(dl->list),p.numVertices};else {failure="native_source";++invalid;return;}}}
 SceneMaterial m;ModelStreams streams;std::array<Range,16> ranges{};unsigned rangeCount=0;bool immutable=source!=nullptr;
 uint64_t key=source?source->generation*0x9e3779b97f4a7c15ull:0;key^=p.state.texconfig;
 for(unsigned i=0;i<p.numStreams;++i){const auto& s=p.streams[i];ModelArray* a=nullptr;
  if(s.id==0||s.id==13)a=&streams.position;else if(s.id==1)a=&streams.normal;else if(s.id==2)a=&streams.color;else if(s.id==3)a=&streams.uv;else if(s.id>=4&&s.id<=8)a=&streams.extraUv[s.id-4];else continue;
  if(!s.address||!s.stride||!s.dataSize||s.dataSize>8u*1024u*1024u){failure="array_bounds";++invalid;return;}
  *a={{reinterpret_cast<const uint8_t*>(s.address),s.dataSize},s.stride,s.beData==0};ranges[rangeCount++]={s.address,s.dataSize};immutable&=glxVitaResourceArenaOwns(reinterpret_cast<const void*>(s.address))!=0;
  key^=uint64_t(s.address)+(key<<6)+(key>>2);key^=uint64_t(s.id)|(uint64_t(s.stride)<<8)|(uint64_t(s.dataSize)<<16);
 }
 if(streams.position.bytes.empty()){failure="position_missing";++invalid;return;}
 std::array<BoneTransform,32> bones{};unsigned boneCount=0;
 for(unsigned slot=0;slot<10;++slot)for(unsigned r=0;r<3;++r)m.bones[slot*3+r][r]=1;
 if(hasPn){const auto* skin=static_cast<const uint8_t*>(userdata(p,GLUD_Skin));if(!skin){failure="skin_userdata";++invalid;return;}uint32_t count;std::memcpy(&count,skin,4);if(!count||count>32){failure="skin_count";++invalid;return;}
  const auto* data=reinterpret_cast<const GLSkinUserData*>(skin+4);for(unsigned i=0;i<count;++i){int selector=data[i].reg+99;if(selector<3||selector>27||selector%3){failure="skin_register";++invalid;return;}auto& b=bones[boneCount++];b.selector=selector;std::memcpy(b.matrix.data(),data[i].mat,48);std::memcpy(m.bones[selector].data(),data[i].mat,48);}}
 nlMatrix4 v,projection,mv;glViewGetViewMatrix(view,v);glViewGetProjectionMatrix(view,projection);
 const auto* state=glViewGetState(view);const float sx=960.f/PortLogicalFrameWidth(),sy=544.f/448.f;
 m.viewport={state->vpX*sx,state->vpY*sy,state->vpWidth*sx,state->vpHeight*sy};m.scissor={m.viewport[0],m.viewport[1],m.viewport[0]+m.viewport[2],m.viewport[1]+m.viewport[3]};
 if(const auto* vp=static_cast<const GLViewportUserData*>(userdata(p,GLUD_Viewport))){if(vp->view&&vp->projection){v=*reinterpret_cast<const nlMatrix4*>(vp->view);projection=*reinterpret_cast<const nlMatrix4*>(vp->projection);}m.viewport={vp->x*sx,vp->y*sy,vp->w*sx,vp->h*sy};m.scissor={m.viewport[0],m.viewport[1],m.viewport[0]+m.viewport[2],m.viewport[1]+m.viewport[3]};}
 if(const auto* sc=static_cast<const uint16_t*>(userdata(p,GLUD_Scissor))){m.scissor={sc[0]*sx,sc[1]*sy,(sc[0]+sc[2])*sx,(sc[1]+sc[3])*sy};}
 for(unsigned k=0;k<4;++k){if(std::fabs(m.scissor[k])<.001f)m.scissor[k]=0;const float full=(k&1)?544.f:960.f;if(std::fabs(m.scissor[k]-full)<.001f)m.scissor[k]=full;}
 // Match GLX's matrix-handle contract: identity and unresolved file offsets
 // are not matrices to dereference. Resource/frame allocations are registered.
 const bool matrixOwned=p.state.matrix&&port_region_owns(reinterpret_cast<const void*>(p.state.matrix));
 if(p.state.matrix!=glGetIdentityMatrix()&&matrixOwned)nlMultMatrices(mv,*reinterpret_cast<const nlMatrix4*>(p.state.matrix),v);else mv=v;
 std::memcpy(m.modelView.data(),mv.e,64);auto matrix=compose_glx_mvp(mv.e,projection.e);
 if(!std::all_of(matrix.begin(),matrix.end(),[](float f){return std::isfinite(f);})){
  failure="invalid_matrix";++invalid;
  static unsigned logged=0;
  if(logged++<4){auto* f=std::fopen("ux0:data/strikersVita/native_invalid_matrices.log",logged==1?"w":"a");if(f){
   std::fprintf(f,"frame=%u view=%u handle=%08lx owned=%u identity=%08lx\n",unsigned(glGetCurrentFrame()),unsigned(view),(unsigned long)p.state.matrix,unsigned(matrixOwned),(unsigned long)glGetIdentityMatrix());
   const float* matrices[]{v.e,projection.e,mv.e,matrix.data(),matrixOwned?reinterpret_cast<const float*>(p.state.matrix):nullptr};
   const char* names[]{"view","projection","model_view","mvp","packet_model"};
   for(unsigned i=0;i<5;++i)if(matrices[i]){std::fprintf(f,"%s",names[i]);for(unsigned k=0;k<16;++k){uint32_t bits;std::memcpy(&bits,&matrices[i][k],4);std::fprintf(f," %08x",bits);}std::fputc('\n',f);}std::fclose(f);
  }}
  return;
 }
 m.environment[0]=userdata(p,GLUD_EnvDiffuse)?1:0;m.environment[1]=userdata(p,GLUD_MobileDiffuse)?1:0;
 const float gloss=glGetTextureState(p.state.texturestate,GLTS_GlossLevel)/63.f;m.material[0]=gloss;
 if((m.environment[0]||userdata(p,GLUD_OnePassFresnel))&&gloss<=.85f)m.environment[2]=gloss>.5f?2:gloss>.25f?3:4;
 m.environment[3]=2.f*float(glGetCurrentFrame()%5)/5.f-1.f;
 Raster raster;raster.depthTest=glGetRasterState(p.state.raster,GLS_DepthTest);raster.depthWrite=glGetRasterState(p.state.raster,GLS_DepthWrite);raster.depthFunction=glGetRasterState(p.state.raster,GLS_DepthFunc);raster.cull=glGetRasterState(p.state.raster,GLS_Culling);raster.blend=glGetRasterState(p.state.raster,GLS_AlphaBlend);const unsigned write=glGetRasterState(p.state.raster,GLS_ColourWrite);raster.writeRgb=write&1;raster.writeAlpha=(write==2)||(write==3&&view==GLV_ShadowTexture);
 m.controls[0]=p.state.texconfig;m.controls[1]=glGetRasterState(p.state.raster,GLS_AlphaTest)?float(glGetRasterState(p.state.raster,GLS_AlphaTestRef))/255.f:-1;
 const auto program=p.state.program;m.lit=program==glGetProgram("3d pointlit")||program==glGetProgram("3d pointlit dirt")||program==glGetProgram("3d crowd lit");
 m.material[1]=program==glGetProgram("3d pointlit dirt")?1.f:0.f;
 // The light-ramp materials scale their diffuse stage by lighting/range.
 // GameRenderTask defaults that range to 2; GLX selected scale 2 unless it was 1.
 m.controls[2]=(p.state.texconfig&32)?(glConstantGet("lighting/range").x==1.f?1.f:2.f):(program==glGetProgram("3d unlit 2x")?2.f:1.f);m.controls[3]=program==glGetProgram("2d movie")?2.f:(program==glGetProgram("3d crowd")||program==glGetProgram("3d crowd lit"))?1.f:0.f;
 m.effects[3]=glConstantGet("crowd/frame").x;m.effects[2]=glGetTextureState(p.state.texturestate,GLTS_DiffuseLevel)/63.f;
 // Standard shadow-map combiners use K1=ShadowLevel and K2=1-ShadowLevel.
 // shadow/pass*_colour belongs to the separate shadow-volume passes.
 const float shadowLevel=glGetTextureState(p.state.texturestate,GLTS_ShadowLevel)/63.f;
 m.shadow.fill(shadowLevel);m.shadowAdd.fill(1.f-shadowLevel);
 if(const auto* color=static_cast<const uint8_t*>(userdata(p,GLUD_ConstantColour))){m.effects[0]=1;for(unsigned i=0;i<4;++i)m.tint[i]=float(color[i])/255;}
 if(const auto* alpha=static_cast<const float*>(userdata(p,GLUD_Translucent)))m.effects[1]=std::clamp(float(int(*alpha*255.5f)),0.f,255.f)/255.f;
 if(userdata(p,GLUD_NoRasterizedAlpha))m.effects[1]=1.f;
 if(m.lit){m.ambient=vector4(glConstantGet("lighting/ambient_colour"));if(const auto* a=static_cast<const float*>(userdata(p,GLUD_Ambient)))std::copy_n(a,4,m.ambient.begin());if(const auto* a=static_cast<const float*>(userdata(p,GLUD_Diffuse)))std::copy_n(a,4,m.diffuse.begin());}
 if(const auto* data=static_cast<const uint32_t*>(userdata(p,GLUD_Light))){const auto* ls=reinterpret_cast<const GLLightUserData*>(data+1);for(unsigned i=0;i<std::min(*data,4u);++i){transform_point(m.lightPosition[i],&ls[i].worldPosition.x,v,ls[i].outerRadius==0);m.lightPosition[i][3]=ls[i].outerRadius?1.f/(.666f*.666f*ls[i].outerRadius*ls[i].outerRadius):-1.f;for(unsigned k=0;k<4;++k)m.lightColor[i][k]=std::clamp(ls[i].colour.c[k]*ls[i].intensity,0.f,1.f);}}
 if(const auto* data=static_cast<const uint32_t*>(userdata(p,GLUD_DirectionalLight))){const auto* ls=reinterpret_cast<const GLDirectionalLightUserData*>(data+1);for(unsigned i=0;i<std::min(*data,4u);++i){transform_point(m.lightPosition[i],&ls[i].direction.x,v,true);m.lightPosition[i][3]=-1;std::copy_n(ls[i].colour.c,4,m.lightColor[i].begin());}}
 if(const auto* data=static_cast<const uint32_t*>(userdata(p,GLUD_Specular))){const auto* lights=reinterpret_cast<const GLSpecularUserData*>(data+1);for(unsigned i=0;i<std::min(*data,4u);++i){std::array<float,4> direction{};transform_point(direction,&lights[i].worldDirection.x,v,true);const auto light=port::render::scene_specular_light(direction,lights[i].colour.c,lights[i].intensity,lights[i].exponent);m.specular[i]=light.direction;m.specularColor[i]=light.color;}}
 m.fog=viewFog;m.fogColor=viewFogColor;if(userdata(p,GLUD_NoFog))m.fog[2]=0;
 for(unsigned semantic=0;semantic<6;++semantic)if(p.state.texconfig&(1u<<semantic)){
  m.textures[semantic]=texture_for(glx_GetTex(p.state.texture[semantic],false,false));if(!m.textures[semantic]){failure="material_texture";++invalid;return;}
  if(semantic==0){const auto* pTex=glx_GetTex(p.state.texture[semantic],false,false);auto found=textures.find(pTex);if(found!=textures.end()&&found->second.shadowPartition>=0)m.material[2]=float(found->second.shadowPartition+1);}
  const unsigned wrap=glGetTextureState(p.state.texturestate,eGLTextureState(semantic));
  const auto sampling=port::render::scene_texture_sampling(semantic,wrap,glGetTextureState(p.state.texturestate,eGLTextureState(semantic+6))==GLTF_Linear);
  m.wrapU[semantic]=sampling.wrapU;m.wrapV[semantic]=sampling.wrapV;m.filter[semantic]=sampling.linear;}
 // Environment materials alter streamed colour/UV arrays; take a fresh snapshot.
 // Fresnel and environment UVs are evaluated on the GPU; immutable source
 // positions/colours remain resident while only uniforms change.
 uint32_t modelId=0;auto it=geometry.find(key);ModelVertices model;
 if(immutable&&it!=geometry.end())modelId=it->second.id;
 else {
  if(!prepare_model_vertices(streams,original,p.numVertices,p.primType,hasPn?std::span<const uint8_t>(source->selectors):std::span<const uint8_t>{},{bones.data(),boneCount},model,true,false)){failure="geometry_decode";++invalid;return;}
  // ModelStreams and the shader both use semantic slots (Diffuse=0,
  // Gloss=4), while GX packed only its own texture units. Repacking these
  // vertices by the active texture count replaced sparse gloss/shadow UVs
  // with absent intermediate arrays, sampling a single texture coordinate.
  if(immutable){modelId=gpu.create_model(model.vertices,model.indices);if(modelId){geometry.emplace(key,Geometry{modelId,source->marker,source->generation,{ranges.begin(),ranges.begin()+rangeCount}});++resident;}}
 }
 if(PortDiagnosticsEnabled()&&ballPacketFrame==glGetCurrentFrame()&&glGetCurrentFrame()%60==0&&std::find(ballPackets.begin(),ballPackets.begin()+ballPacketCount,ptr)!=ballPackets.begin()+ballPacketCount){
  static bool first=true;auto* f=std::fopen("ux0:data/strikersVita/native_ball_packets.log",first?"w":"a");first=false;if(f){
   std::array<float,4> low{1e30f,1e30f,1e30f,1e30f},high{-1e30f,-1e30f,-1e30f,-1e30f};
   for(unsigned i=0;i<p.numVertices;++i){const uint32_t index=original.empty()?i:original[i];const uint8_t* a=nullptr;if(!model_array_at(streams.position,index,streams.position.stride,a))continue;float pos[4]{0,0,0,1};for(unsigned k=0;k<3;++k)pos[k]=streams.position.stride==6?model_short(a+2*k,streams.position.littleEndian,1.f/256.f):model_float(a+4*k,streams.position.littleEndian);float c[4]{};for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)c[r]+=matrix[k*4+r]*pos[k];for(unsigned k=0;k<3;++k)c[k]/=c[3];for(unsigned k=0;k<4;++k){low[k]=std::min(low[k],c[k]);high[k]=std::max(high[k],c[k]);}}
   std::fprintf(f,"frame=%u view=%u count=%u resident=%u owned=%u matrix=%08lx modelpos=%.3f,%.3f,%.3f cull=%u depth=%u,%u,%u blend=%u texmask=%u test=%.3f alpha=%.3f env=%.3f,%.3f,%.3f clip=%.3f,%.3f,%.3f..%.3f,%.3f,%.3f w=%.3f..%.3f\n",unsigned(glGetCurrentFrame()),unsigned(view),p.numVertices,modelId,unsigned(matrixOwned),(unsigned long)p.state.matrix,matrixOwned?reinterpret_cast<const float*>(p.state.matrix)[12]:0,matrixOwned?reinterpret_cast<const float*>(p.state.matrix)[13]:0,matrixOwned?reinterpret_cast<const float*>(p.state.matrix)[14]:0,raster.cull,raster.depthTest,raster.depthWrite,raster.depthFunction,raster.blend,p.state.texconfig,m.controls[1],m.effects[1],m.environment[0],m.environment[1],m.environment[2],low[0],low[1],low[2],high[0],high[1],high[2],low[3],high[3]);std::fclose(f);
  }
 }
 if(!gpu.draw_scene(modelId,model.vertices,model.indices,matrix,raster,m)){failure="scene_draw";++invalid;return;}
 ++draws;if(p.state.texconfig)++textured;if(view==GLV_Anark)++hud;if(view==GLV_Characters)++characters;if(!modelId)++dynamic;
 }catch(...){failure="draw_allocation";++invalid;}
}
}
#endif
