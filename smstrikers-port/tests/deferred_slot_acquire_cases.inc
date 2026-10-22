// Deterministic host-only ownership checks. No production hook is added.
#include <condition_variable>
#include <mutex>

namespace {
enum class SlotAcquireCase { NextDraw, NextAppend, Shutdown, EmplaceCapacity, AppendCapacity };
struct SlotAcquireEvent {
 char kind;
 float identity;
 size_t count;
 bool operator==(const SlotAcquireEvent&) const = default;
};
struct SlotAcquireGate {
 std::mutex mutex;
 std::condition_variable changed;
 unsigned producerStage=0, firstWaitStage=0;
 bool ownerHeld=false, releaseOwner=false, ownerReleased=false, firstWait=false;
 bool publishedFullRing=false, cpuPrepared=false, tailReturned=false;
 bool premature=false, timedOut=false, controllerValid=false;
 std::vector<SlotAcquireEvent> events;
};
std::atomic<SlotAcquireGate*> slotAcquireGate{nullptr};

void slot_acquire_wait_observed() {
 auto* gate=slotAcquireGate.load(std::memory_order_acquire);
 if(!gate||!pthread_equal(pthread_self(),caller))return;
 std::lock_guard<std::mutex> lock(gate->mutex);
 if(!gate->firstWait) {
  gate->firstWait=true;
  gate->firstWaitStage=gate->producerStage;
  gate->premature=gate->producerStage==128;
  gate->changed.notify_all();
 }
}
void slot_acquire_owner_draw(float identity) {
 auto* gate=slotAcquireGate.load(std::memory_order_acquire);
 if(!gate)return;
 std::unique_lock<std::mutex> lock(gate->mutex);
 gate->events.push_back({'d',identity,1});
 if(!gate->ownerHeld) {
  gate->ownerHeld=true;
  gate->changed.notify_all();
  gate->changed.wait(lock,[&]{return gate->releaseOwner;});
  gate->ownerReleased=true;
 }
}
void slot_acquire_owner_copy(std::span<const TextureCopy> list) {
 auto* gate=slotAcquireGate.load(std::memory_order_acquire);
 if(!gate)return;
 std::lock_guard<std::mutex> lock(gate->mutex);
 gate->events.push_back({'c',list.empty()?0:list.front().sourceRect[0],list.size()});
}

bool check_deferred_slot_case(SlotAcquireCase which) {
 constexpr size_t SlotBytes=2*1024*1024;
 constexpr size_t CopyCount=(3*1024*1024/2)/sizeof(TextureCopy);
 constexpr size_t CapacityVertices=12000;
 static_assert(CopyCount*sizeof(TextureCopy)>SlotBytes/2);
 static_assert(CopyCount*sizeof(TextureCopy)+CapacityVertices*sizeof(ModelVertex)>SlotBytes);
 static_assert(CapacityVertices*sizeof(ModelVertex)+sizeof(DrawLayout)+32<SlotBytes);
 const bool capacity=which==SlotAcquireCase::EmplaceCapacity||which==SlotAcquireCase::AppendCapacity;
 const bool appendTail=which==SlotAcquireCase::NextAppend||which==SlotAcquireCase::AppendCapacity;
 const size_t tailCopyCount=which==SlotAcquireCase::AppendCapacity?CopyCount:3;
 const unsigned prefix=capacity?112:128;
 const unsigned expectedWait=capacity?300:129;
 const char* name=which==SlotAcquireCase::NextDraw?"next_draw":
                  which==SlotAcquireCase::NextAppend?"append_initial":
                  which==SlotAcquireCase::Shutdown?"shutdown_without_append":
                  which==SlotAcquireCase::EmplaceCapacity?"emplace_capacity_retry":"append_capacity_retry";
 const size_t geometryStart=geometry.size(), uniformStart=uniforms.size();
 const size_t materialStart=materials.size(), paletteStart=palettes.size();
 const size_t copyStart=copies.size(), regionStart=copiedRegions.size();
 const unsigned frameStart=frames, endStart=endCalls;
 gpuFailure=false;failedCall=FailedCall::None;verifyDrawPayloads=false;
 DeferredRenderer r;
 if(!r.initialize(true)||!r.begin_frame())return false;
 SlotAcquireGate gate;
 slotAcquireGate.store(&gate,std::memory_order_release);
 // This independent controller always releases the owner. The deadline bounds
 // a broken handshake; it is never the PASS or the baseline-failure oracle.
 std::thread controller([&] {
  std::unique_lock<std::mutex> lock(gate.mutex);
  const bool observed=gate.changed.wait_for(lock,std::chrono::seconds(3),
     [&]{return gate.ownerHeld&&gate.firstWait;});
  gate.timedOut=gate.timedOut||!observed;
  gate.controllerValid=observed&&gate.firstWaitStage==expectedWait&&
      gate.cpuPrepared&&!gate.tailReturned&&!gate.ownerReleased&&
      (capacity||gate.publishedFullRing);
  gate.releaseOwner=true;
  gate.changed.notify_all();
 });
 const auto stage=[&](unsigned value) {
  std::lock_guard<std::mutex> lock(gate.mutex);gate.producerStage=value;
 };
 std::array<ModelVertex,3> vertices{};
 std::array<uint16_t,3> indices{0,1,2};
 std::array<float,16> matrix{};
 Raster raster;SceneMaterial material;
 std::vector<SlotAcquireEvent> expected;
 expected.reserve(prefix+3);
 bool ok=true;
 for(unsigned draw=0;draw<prefix;++draw) {
  stage(draw+1);
  vertices[0].position[0]=float(draw+100);
  matrix[0]=float(draw+1000);
  material.tint[0]=float(draw+2000);
  material.bones.back()[3]=float(draw+4000);
  ok=r.draw_scene(0,vertices,indices,matrix,raster,material)&&ok;
  expected.push_back({'d',float(draw+1000),1});
  vertices[0].position[0]=matrix[0]=material.tint[0]=material.bones.back()[3]=-1;
  if(draw==15) {
   // Establish that the owner is already holding the first published batch
   // before filling the other slots or demonstrating later CPU preparation.
   std::unique_lock<std::mutex> lock(gate.mutex);
   if(!gate.changed.wait_for(lock,std::chrono::seconds(3),[&]{return gate.ownerHeld;})) {
    gate.timedOut=true;
    gate.releaseOwner=true;
    gate.changed.notify_all();
   }
  }
 }
 bool premature;
 {
  std::lock_guard<std::mutex> lock(gate.mutex);
  premature=gate.premature;
  if(!capacity)gate.publishedFullRing=true;
 }
 size_t expectedCopies=0;
 if(!premature) {
  std::vector<TextureCopy> copyA,copyB;
  if(capacity) {
   copyA.resize(CopyCount);
   for(auto& copy:copyA){copy.sourceRect[0]=7001;copy.destination=73;}
   ok=r.copy_textures(copyA)&&ok;
   expected.push_back({'c',7001,CopyCount});
   expectedCopies+=CopyCount;
   for(auto& copy:copyA)copy={};
  }
  // Real CPU preparation after the last publication, while the first owner
  // draw remains blocked. Its result becomes the next immutable payload.
  for(unsigned i=0;i<matrix.size();++i)matrix[i]=float(prefix+1000+i);
  vertices[0].position[0]=float(prefix+100);
  material.tint[0]=float(prefix+2000);material.bones.back()[3]=float(prefix+4000);
  std::vector<ModelVertex> large;
  if(which==SlotAcquireCase::EmplaceCapacity) {
   large.resize(CapacityVertices);large.front()=vertices.front();
  }
  if(appendTail) {
   copyB.resize(tailCopyCount);
   for(auto& copy:copyB){copy.sourceRect[0]=8002;copy.destination=74;}
  }
  {
   std::lock_guard<std::mutex> lock(gate.mutex);
   gate.cpuPrepared=matrix[0]==float(prefix+1000);
   gate.producerStage=expectedWait;
  }
  if(which==SlotAcquireCase::Shutdown) {
   r.shutdown(); // No new append: drain must not inspect or republish the busy slot.
  } else {
   if(appendTail) {
    ok=r.copy_textures(copyB)&&ok;
    expected.push_back({'c',8002,tailCopyCount});
    expectedCopies+=tailCopyCount;
    for(auto& copy:copyB)copy={};
   } else {
    const auto payload=which==SlotAcquireCase::EmplaceCapacity
        ?std::span<const ModelVertex>(large):std::span<const ModelVertex>(vertices);
    ok=r.draw_scene(0,payload,indices,matrix,raster,material)&&ok;
    expected.push_back({'d',float(prefix+1000),1});
    if(!large.empty())large.front().position[0]=-1;
   }
   vertices[0].position[0]=matrix[0]=material.tint[0]=material.bones.back()[3]=-1;
   {
    std::lock_guard<std::mutex> lock(gate.mutex);
    ok=gate.ownerReleased&&gate.events.size()>=16&&ok;
    gate.tailReturned=true;
   }
   ok=r.end_frame()&&ok;
   const auto timing=r.completed_timing();
   ok=timing.frontendWaitUs>0&&timing.slotWaits>0&&ok;
   std::vector<uint8_t> pixels;
   ok=r.readback_presented_rgba8(pixels)&&pixels==std::vector<uint8_t>(4,42)&&ok;
   r.shutdown();
  }
 } else {
  // The original source waits inside draw 128's publication. The controller
  // has identified that exact wait and released the owner; close and join
  // before reporting the expected witness failure.
  ok=r.end_frame(false)&&ok;
  r.shutdown();
 }
 controller.join();
 slotAcquireGate.store(nullptr,std::memory_order_release);
 const size_t expectedDraws=prefix+(!premature&&
      (which==SlotAcquireCase::NextDraw||which==SlotAcquireCase::EmplaceCapacity)?1:0);
 bool fifo=gate.events==expected&&geometry.size()==geometryStart+expectedDraws&&
     uniforms.size()==uniformStart+expectedDraws&&materials.size()==materialStart+expectedDraws&&
     palettes.size()==paletteStart+expectedDraws;
 for(unsigned draw=0;draw<expectedDraws;++draw) {
  fifo=fifo&&geometry[geometryStart+draw]==float(draw+100)&&
       uniforms[uniformStart+draw]==float(draw+1000)&&
       materials[materialStart+draw]==float(draw+2000)&&
       palettes[paletteStart+draw]==float(draw+4000);
 }
 fifo=fifo&&copies.size()==copyStart+expectedCopies&&copiedRegions.size()==regionStart+expectedCopies;
 for(size_t i=0;i<expectedCopies;++i) {
  const bool second=!capacity||i>=CopyCount;
  fifo=fifo&&copies[copyStart+i]==(second?8002.f:7001.f)&&
       copiedRegions[regionStart+i].destination==(second?74u:73u);
 }
 const bool cleanup=!initialized&&!controller.joinable()&&gate.ownerReleased&&!gate.timedOut;
 if(premature) {
  std::printf("[deferred-slot-gate] FAIL premature_acquire_after_publish draw=128 "
              "first_wait=%u fifo=%u cleanup=%u timeout=%u\n",
              gate.firstWaitStage,unsigned(fifo),unsigned(cleanup&&ok),unsigned(gate.timedOut));
  return false;
 }
 const bool closed=which==SlotAcquireCase::Shutdown
      ?frames==frameStart&&endCalls==endStart:frames==frameStart+1&&endCalls==endStart+1;
 const bool passed=ok&&fifo&&cleanup&&closed&&gate.controllerValid;
 std::printf("[deferred-slot-gate] %s case=%s first_wait=%u draws=%zu copy_records=%zu "
             "fifo=%u cleanup=%u timeout=%u\n",passed?"PASS":"FAIL",name,gate.firstWaitStage,
             expectedDraws,expectedCopies,unsigned(fifo),unsigned(cleanup),unsigned(gate.timedOut));
 return passed;
}
bool check_deferred_slot_acquire(bool witnessOnly) {
 if(!check_deferred_slot_case(SlotAcquireCase::NextDraw))return false;
 if(witnessOnly)return true;
 for(auto which:{SlotAcquireCase::NextAppend,SlotAcquireCase::Shutdown,SlotAcquireCase::EmplaceCapacity,SlotAcquireCase::AppendCapacity})
  if(!check_deferred_slot_case(which))return false;
 std::puts("Deferred slot acquisition: saturated publication, bounded acquire, FIFO, immutable payloads, "
           "initial append, both capacity retries and shutdown without append passed");
 return true;
}
} // namespace
