#include "../lib/gx/gx.hpp"
#include "../lib/gx/native_model_sources.hpp"
#include "gx/aurora_gx_bridge.hpp"
#include "gfx/vita_native_model_uniform_cache.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace aurora::vita;
#undef CHECK
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (false)

int main() {
  aurora::gx::fifo::NativeModelSources<4> sources;
  std::array<uint8_t,16> data{};
  std::array<uintptr_t,4> handles{0x100,0x200,0x300,0x400};
  std::array<uint64_t,4> generations{};
  for (unsigned i=0;i<handles.size();++i) {
    generations[i]=sources.register_source(handles[i],data.data(),data.size(),i);
    CHECK(generations[i]);
  }
  CHECK(sources.size()==4);
  CHECK(!sources.register_source(0x500,data.data(),data.size(),4));
  for(unsigned i=0;i<handles.size();++i) CHECK(sources.find(handles[i])->generation==generations[i]);
  unsigned retired=0;
  CHECK(sources.release_after(1,[&](const auto& source){CHECK(source.ownerLevel>1);++retired;})==2);
  CHECK(retired==2 && sources.size()==2);
  CHECK(sources.find(handles[0]) && sources.find(handles[1]));
  CHECK(!sources.find(handles[2]) && !sources.find(handles[3]));
  const auto reloaded=sources.register_source(handles[2],data.data(),data.size(),2);
  CHECK(reloaded>generations.back() && sources.find(handles[2])->generation==reloaded);
  const auto replaced=sources.register_source(handles[2],data.data(),data.size(),2);
  CHECK(replaced>reloaded && sources.size()==3);
  CHECK(!sources.register_source(0,data.data(),data.size(),0));
  CHECK(!sources.register_source(0x500,nullptr,data.size(),0));
  sources.clear();
  CHECK(sources.size()==0 && !sources.find(handles[0]));
  CHECK(sources.register_source(handles[0],data.data(),data.size(),0)>replaced);
  // Removing a collision-chain head must leave older persistent assets findable.
  sources.clear();
  CHECK(sources.register_source(0x100,data.data(),data.size(),2));
  CHECK(sources.register_source(0x300,data.data(),data.size(),0));
  CHECK(sources.register_source(0x500,data.data(),data.size(),1));
  CHECK(sources.release_after(1,[](const auto&){})==1);
  CHECK(!sources.find(0x100)&&sources.find(0x300)&&sources.find(0x500));
  CHECK(sources.register_source(0x700,data.data(),data.size(),2));
  CHECK(sources.size()==3);

  auto& g=aurora::gx::g_gxState;
  g=aurora::gx::GXState{};
  g.proj.m0={1,0,0,0};g.proj.m1={0,1,0,0};g.proj.m2={0,0,1,0};g.proj.m3={0,0,0,1};
  g.logicalViewport=g.renderViewport={0,0,960,544,0,1};
  g.numChans=1;g.numTevStages=1;
  g.tevStages[0].channelId=GX_COLOR0A0;
  g.tevStages[0].colorPass.d=GX_CC_RASC;g.tevStages[0].alphaPass.d=GX_CA_RASA;
  g.colorChannelConfig[GX_COLOR0].lightingEnabled=true;
  g.colorChannelState[GX_COLOR0].lightMask=1;
  g.colorChannelState[GX_COLOR0].matColor={1,0.5f,0.25f,1};
  g.lights[0].color={0.5f,1,0.25f,1};g.lights[0].pos={1,2,3,1};
  auto pipeline=gxbridge::translate_current_pipeline(GX_TRIANGLES,0);
  pipeline.fixedVertexOnGpu=true;pipeline.fixedVertexIndexedPn=true;
  gxbridge::refresh_current_vertex_program_state(pipeline);
  gfx::NativeModelUniformCache cache;
  gfx::NativeModelUniformStamp stamp{1,g.stateRevisions.vertex,g.stateRevisions.fragment,
                                    g.stateIdentity,g.vertexProgramStateGeneration};
  unsigned vertexBuilds=0,fragmentBuilds=0;
  auto compare=[&](bool force=false) {
    const auto update=cache.prepare(stamp,force,[&](auto& vertex,auto& uniforms,bool v,bool f){
      vertexBuilds+=v;fragmentBuilds+=f;
      if(v)gxbridge::translate_fixed_vertex_state(vertex,uniforms,pipeline,f);
      else gxbridge::translate_fragment_uniforms(uniforms,pipeline);
    });
    // Full translation into fresh storage is the independent reference.
    gfx::VertexTransformState full{};gfx::DrawUniforms uniforms{};
    gxbridge::translate_fixed_vertex_state(full,uniforms,pipeline,true);
    CHECK(!std::memcmp(&cache.vertex(),&full,sizeof(full)));
    CHECK(!std::memcmp(&cache.uniforms(),&uniforms,sizeof(uniforms)));
    return update;
  };
  CHECK(compare().vertex);
  for(unsigned i=0;i<1000;++i){const auto update=compare();CHECK(!update.vertex&&!update.fragment);}
  CHECK(vertexBuilds==1 && fragmentBuilds==1);
  g.pnMtx[0].pos.m0[3]=19;g.pnMtx[0].nrm.m1[1]=0.5f;
  g.lights[0].pos={3,2,1,1};++stamp.vertex;
  auto update=compare();CHECK(update.vertex&&!update.fragment);
  g.colorRegs[0]={0.1f,0.2f,0.3f,1};++stamp.fragment;
  update=compare();CHECK(!update.vertex&&update.fragment);
  // Different materials reset fields that a smaller program no longer reads.
  pipeline.colorChannels[0].lightingEnabled=false;pipeline.fixedVertexIndexedPn=false;
  ++stamp.recipe;update=compare();CHECK(update.vertex&&update.fragment);
  // A replaced GXState may restart revisions at the same scalar values.
  g.pnMtx[0].pos.m0[3]=-7;++stamp.stateIdentity;
  CHECK(compare().vertex);
  // Untracked dirty writes and the reference-disable flag force both builders.
  g.colorRegs[0]={0.9f,0.2f,0.3f,1};update=compare(true);CHECK(update.vertex&&update.fragment);
  cache.invalidate();CHECK(compare().vertex);
  ++stamp.vertexProgram;CHECK(compare().vertex);
  // Cross-domain fragment dependencies retain their actual uniform values.
  pipeline.fogMode=gfx::FogMode::Linear;pipeline.fogRangeEnabled=true;
  pipeline.tev.indirectStageCount=1;++stamp.recipe;
  stamp.vertex=g.stateRevisions.vertex;
  stamp.fragment=gfx::fragment_uniform_revision(pipeline,g.stateRevisions);compare();
  g.logicalViewport.left=30;g.renderViewport.width=800;g.mark_dirty(aurora::gx::StateDomain::Vertex);
  stamp.vertex=g.stateRevisions.vertex;
  stamp.fragment=gfx::fragment_uniform_revision(pipeline,g.stateRevisions);
  update=compare();CHECK(update.vertex&&update.fragment);
  g.loadedTextures[0].image0=7u|(7u<<10);g.mark_dirty(aurora::gx::StateDomain::Textures);
  stamp.fragment=gfx::fragment_uniform_revision(pipeline,g.stateRevisions);
  update=compare();CHECK(!update.vertex&&update.fragment);
  // Real model sequences switch mesh identities while sharing uniform inputs.
  // Each pose is followed by twenty mesh draws, rather than twenty repeats of
  // one recipe. The independent oracle always translates into fresh storage.
  gfx::NativeModelUniformCache compiled;
  unsigned compiledVertex=0,compiledFragment=0;
  auto checkCompiled=[&](bool force=false) {
    const auto contract=gfx::native_model_uniform_contract(pipeline);
    stamp.vertex=g.stateRevisions.vertex;
    stamp.fragment=gfx::fragment_uniform_revision(pipeline,g.stateRevisions);
    const auto changed=compiled.prepare(stamp,contract,force,[&](auto& vertex,auto& uniforms,bool v,bool f){
      compiledVertex+=v;compiledFragment+=f;
      if(v)gxbridge::translate_fixed_vertex_state(vertex,uniforms,pipeline,f);
      else gxbridge::translate_fragment_uniforms(uniforms,pipeline);
    });
    gfx::VertexTransformState full{};gfx::DrawUniforms uniforms{};
    gxbridge::translate_fixed_vertex_state(full,uniforms,pipeline,true);
    CHECK(!std::memcmp(&compiled.vertex(),&full,sizeof(full)));
    CHECK(!std::memcmp(&compiled.uniforms(),&uniforms,sizeof(uniforms)));
    return changed;
  };
  pipeline.fogMode=gfx::FogMode::None;pipeline.tev.indirectStageCount=0;
  pipeline.fixedVertexIndexedPn=true;
  for(unsigned i=0;i<1000;++i){
    stamp.recipe=1+i%20;
    if(i%20==0){g.pnMtx[0].pos.m0[3]=float(i);g.mark_dirty(aurora::gx::StateDomain::Vertex);}
    checkCompiled();
  }
  CHECK(compiledVertex==50 && compiledFragment==1);
  // Shader identities alone do not change the copied uniform input contract.
  ++stamp.vertexProgram;CHECK(!checkCompiled().vertex);
  const auto base=pipeline;
  for(unsigned i=0;i<160;++i){
    pipeline=base;stamp.recipe=200+i;
    switch(i%8){
    case 0: pipeline.fixedVertexIndexedPn=false;g.currentPnMtx=i%20;break;
    case 1: pipeline.colorChannels[0].lightingEnabled=true;pipeline.colorChannels[0].lightMask=0x81;break;
    case 2: pipeline.fixedVertexTexMtxMask=1;break;
    case 3: pipeline.texgenCount=1;pipeline.texgens[0].matrix=8;pipeline.texgens[0].postMatrix=19;
      pipeline.tev.stages[0].texCoord=0;pipeline.tev.stages[0].texture=0;
      pipeline.tev.stages[0].color.d=gfx::TevColorArg::TexColor;break;
    case 4: pipeline.texgenCount=1;pipeline.texgens[0].type=gfx::TexGenType::Bump7;
      pipeline.tev.stages[0].texCoord=0;pipeline.tev.stages[0].texture=0;
      pipeline.tev.stages[0].color.d=gfx::TevColorArg::TexColor;break;
    case 5: pipeline.fogMode=gfx::FogMode::Linear;pipeline.fogRangeEnabled=true;break;
    case 6: pipeline.tev.indirectStageCount=1;break;
    default: pipeline.fogMode=gfx::FogMode::Exp;pipeline.fogRangeEnabled=false;break;
    }
    g.mark_dirty(aurora::gx::StateDomain::Vertex|aurora::gx::StateDomain::Fragment);
    checkCompiled();
  }
  ++stamp.stateIdentity;CHECK(checkCompiled().vertex);
  CHECK(checkCompiled(true).fragment);
  compiled.invalidate();CHECK(checkCompiled().vertex);
  std::puts("prepared native models: bounded loader generations and full-translation equivalence; 1000 repeated draws use one vertex/fragment preparation");
  std::puts("compiled uniform inputs: 1000 draws over 20 meshes and 50 poses use 50 vertex / 1 fragment preparations; 160 contract transitions match fresh translation");
}
