static void check_frame_timeline_renderer()
{
    using namespace port::render;
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<ModelVertex,3> vertices{};
    std::array<uint16_t,3> indices{0,1,2};
    SceneMaterial material; Raster raster;
    for (bool timed : {false,true})
    {
        Renderer r; assert(r.initialize(timed) && r.initialize_scene_program());
        const auto model = r.create_model(vertices,indices); assert(model);
        FrameTimeline::Owner o{};
        assert(r.begin_frame(&o) && o.drawTimingEnabled == timed);
        auto clocks = systemClockQueries;
        auto setters = presentProfileSets;
        assert(r.draw_scene(model,{},{},identity,raster,material));
        assert(r.end_frame());
        const auto parent = r.completed_timing();
        assert(o.drawPrepareNs+o.drawApiNs+o.drawSuffixNs == parent.drawUs*1000);
        assert(o.presentPrefixNs+o.presentSwapNs+o.presentSuffixNs == parent.queueUs*1000);
        assert(o.presentRequested && o.timedDrawScopes == unsigned(timed) && o.timedPresentScopes == unsigned(timed));
        assert(!activePresentProfile && presentProfileSets-setters == 2*unsigned(timed));
        assert(o.provider.addEntryCalls == unsigned(timed) && o.provider.gcCalls == unsigned(timed));
        assert(o.provider.addEntryNs+o.provider.gcNs<=o.presentSwapNs);
        if (!timed) assert(systemClockQueries == clocks);
        // A normal following frame has no hook or writes to the old record.
        std::array<unsigned char,sizeof(o)> frozen{}; std::memcpy(frozen.data(),&o,sizeof(o));
        setters=presentProfileSets;
        assert(r.begin_frame() && r.draw_scene(model,{},{},identity,raster,material) && r.end_frame());
        assert(presentProfileSets == setters && std::memcmp(frozen.data(),&o,sizeof(o)) == 0);
        o={}; assert(r.begin_frame(&o));
        Raster culled=raster;culled.cull=3;
        assert(r.draw_scene(model,{},{},identity,culled,material));
        assert(r.end_frame(false));
        assert(!o.timedDrawScopes && !o.timedPresentScopes && !o.presentRequested);
        r.shutdown();empty();
    }
    // A copy conversion uses the same draw timer, while a display-only bitmap
    // restores that timer and must leave all scene timing children unchanged.
    {
        Renderer r; assert(r.initialize(true) && r.initialize_scene_program());
        const auto target=r.create_target(32,32);assert(target);
        FrameTimeline::Owner o{};assert(r.begin_frame(&o));
        assert(r.copy_texture(target,{0,0,16,16},true));
        assert(o.timedDrawScopes==1);
        const auto before=std::array<uint64_t,4>{o.drawPrepareNs,o.drawApiNs,o.drawSuffixNs,o.timedDrawScopes};
        TextureUpload bitmap;bitmap.width=bitmap.height=bitmap.stride=8;
        bitmap.mipCount=1;bitmap.storage=TextureStorage::Rgba8;bitmap.pixels.assign(8*8*4,255);
        auto setters=presentProfileSets;
        assert(r.present_bitmap(bitmap,{0,0,960,544}));
        assert((before==std::array<uint64_t,4>{o.drawPrepareNs,o.drawApiNs,o.drawSuffixNs,o.timedDrawScopes}));
        assert(presentProfileSets==setters && !activePresentProfile);
        assert(r.end_frame(false));
        assert(o.drawPrepareNs+o.drawApiNs+o.drawSuffixNs==r.completed_timing().drawUs*1000);
        r.release_texture(target);r.shutdown();empty();
    }
    // Early present failure, swap failure, draw failure and shutdown all end
    // the borrow without leaving the provider's raw pointer installed.
    for(unsigned failure=0;failure<4;++failure)
    {
        Renderer r;assert(r.initialize(true)&&r.initialize_scene_program());
        FrameTimeline::Owner o{};assert(r.begin_frame(&o));
        if(failure==0) {
            failBlit=true;assert(!r.end_frame());failBlit=false;
            assert(o.presentRequested&&!o.timedPresentScopes&&!o.provider.addEntryCalls);
        } else if(failure==1) {
            failSwap=true;assert(!r.end_frame());failSwap=false;
            assert(o.timedPresentScopes==1&&o.provider.addEntryCalls==1);
        } else if(failure==2) {
            failDraw=true;assert(!r.draw_scene(0,vertices,indices,identity,raster,material));failDraw=false;
            assert(o.timedDrawScopes==1);assert(r.end_frame(false));
        } else {
            assert(r.draw_scene(0,vertices,indices,identity,raster,material));r.shutdown();
            assert(o.cancelled);
        }
        assert(!activePresentProfile);r.shutdown();empty();
    }
    std::puts("Frame timeline renderer: exact parent partitions, quiet OFF, internal copies, isolated bitmap, errors and borrow teardown passed");
}
