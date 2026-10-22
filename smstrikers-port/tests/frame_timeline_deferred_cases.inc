static void check_frame_timeline_deferred()
{
 DeferredRenderer r;
 auto restart=[&] {
  r.shutdown();gpuFailure=false;failedCall=FailedCall::None;
  assert(r.initialize(true)&&r.initialize_scene_program());
 };
 TextureUpload image;
 std::array<ModelVertex,3> vertices{};
 std::array<uint16_t,3> indices{0,1,2};
 std::array<float,16> matrix{};
 Raster raster;SceneMaterial material;
 FrameTimeline t;
 restart();
 assert(r.begin_frame(&t));
 const auto afterBegin=ownerWaitEntries.load(std::memory_order_acquire);
 assert(r.create_texture(image)==17);
 eventually([&]{return ownerWaitEntries.load(std::memory_order_acquire)>afterBegin;});
 vertices[0].position[0]=413;matrix[0]=521;material.tint[0]=647;
 assert(r.draw_scene(0,vertices,indices,matrix,raster,material));
 vertices[0].position[0]=-1;matrix[0]=-1;material.tint[0]=-1;
 for(unsigned i=0;i<40;++i) {
  const auto entries=ownerWaitEntries.load(std::memory_order_acquire);
  if(i==5)spuriousNextWait=true;
  assert(r.create_texture(image)==17); // Publish the first Draw, then wake the idle owner.
  eventually([&]{return ownerWaitEntries.load(std::memory_order_acquire)>entries;});
 }
 assert(r.end_frame());
 assert(geometry.back()==413&&uniforms.back()==521&&materials.back()==647);
 const auto& p=t.producer;const auto& o=t.owner;
 assert(p.observed&&p.beginSucceeded&&p.endSucceeded&&!p.cancelled&&!o.cancelled);
 assert(p.beginRequestNs<=o.beginStartNs&&o.beginEndNs<=p.beginReturnNs);
 assert(p.beginReturnNs<=p.firstDrawReadyNs&&p.firstDrawReadyNs<=p.firstDrawPublishNs);
 assert(p.firstDrawPublishNs<=o.firstDrawCommandNs&&o.firstDrawCommandNs<=o.lastDrawCommandEndNs);
 assert(o.lastDrawCommandEndNs<=o.endStartNs&&p.endRequestNs<=o.endStartNs&&o.endEndNs<=p.endReturnNs);
 assert(p.drawCommands==1&&p.drawBatches==1&&!p.borrowedDrawCommands&&o.drawCommands==1&&!o.failedDrawCommands);
 assert(p.batches==o.batches&&o.waitCalls>=40&&o.storedWaits==32&&o.waitOverflow==o.waitCalls-32);
 uint64_t stored=0,maxStored=0;
 for(unsigned i=0;i<o.storedWaits;++i) {
  const auto& w=o.waits[i];assert(w.beginNs>=o.lastDrawCommandEndNs&&w.endNs>=w.beginNs&&w.endNs<=o.endStartNs);
  if(i)assert(o.waits[i-1].endNs<=w.beginNs);
  stored+=w.endNs-w.beginNs;maxStored=std::max(maxStored,w.endNs-w.beginNs);
 }
 assert(stored<=o.readyWaitNs&&maxStored<=o.maxReadyWaitNs);
 std::array<unsigned char,sizeof(t)> completed{};
 std::memcpy(completed.data(),&t,sizeof(t));
 auto clocks=systemClockQueries.load();
 assert(r.begin_frame()&&r.draw_scene(19,{},{},matrix,raster,material)&&r.end_frame(false));
 assert(systemClockQueries.load()==clocks&&std::memcmp(completed.data(),&t,sizeof(t))==0);
 // Reuse resets the capture; an oversized Draw borrows only through its drain.
 assert(r.begin_frame(&t));
 std::vector<ModelVertex> large(33000);large[0].position[0]=733;
 assert(r.draw_scene(0,large,indices,matrix,raster,material));large[0].position[0]=-1;
 assert(r.end_frame(false));
 assert(t.producer.drawCommands==1&&t.producer.borrowedDrawCommands==1&&t.owner.drawCommands==1);
 assert(t.owner.waitCalls<32&&geometry.back()==733&&!t.owner.failedDrawCommands);
 // Async GPU failure is observed at End, retaining the failed command timeline.
 assert(r.begin_frame(&t));gpuFailure=true;
 assert(r.draw_scene(19,{},{},matrix,raster,material)&&!r.end_frame());
 assert(t.producer.beginSucceeded&&!t.producer.endSucceeded&&t.owner.failedDrawCommands==1);
 assert(t.owner.endEndNs&&t.producer.endReturnNs>=t.owner.endEndNs);
 // The mode remains known when a prior async error skips core Begin entirely.
 assert(!r.begin_frame(&t)&&t.owner.drawTimingEnabled&&!t.producer.beginSucceeded);
 assert(t.owner.beginStartNs&&t.owner.beginEndNs&&!t.owner.endStartNs);
 // Begin failures leave no owner borrow; subsequent queue work cannot alter it.
 restart();failedCall=FailedCall::BeginFrame;
 assert(!r.begin_frame(&t)&&t.producer.observed&&!t.producer.beginSucceeded&&!t.owner.endStartNs&&t.owner.drawTimingEnabled);
 std::memcpy(completed.data(),&t,sizeof(t));
 assert(r.create_texture(image)==17);
 r.shutdown();assert(std::memcmp(completed.data(),&t,sizeof(t))==0);
 // Shutdown drains a pending Draw and ends both borrows before returning.
 restart();assert(r.begin_frame(&t));
 assert(r.draw_scene(19,{},{},matrix,raster,material));
 r.shutdown();assert(t.producer.cancelled&&t.owner.cancelled&&t.owner.drawCommands==1);
 assert(!t.producer.endReturnNs&&!t.owner.endEndNs);
 std::memcpy(completed.data(),&t,sizeof(t));
 restart();assert(r.begin_frame()&&r.end_frame());r.shutdown();
 assert(std::memcmp(completed.data(),&t,sizeof(t))==0);
 // Completed Begin with no Draw has no fabricated first Draw or owner wait.
 restart();assert(r.begin_frame(&t)&&r.end_frame(false));
 assert(!t.producer.drawCommands&&!t.producer.firstDrawReadyNs&&!t.owner.firstDrawCommandNs&&!t.owner.waitCalls);
 r.shutdown();
 std::puts("Frame timeline deferred: owned lifetime, publication/borrowed Draw, bounded waits/spurious wake, failures, shutdown and null OFF passed");
}
