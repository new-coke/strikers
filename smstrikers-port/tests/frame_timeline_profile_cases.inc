static void profile_timeline_case(port::render::FrameTimeline& t, uint64_t offset)
{
    using T = port::render::FrameTimeline;
    t = {};
    auto& p = t.producer;
    p.observed = p.beginSucceeded = p.endSucceeded = true;
    p.beginRequestNs = offset + 100000; p.beginReturnNs = offset + 102000;
    p.firstDrawReadyNs = offset + 103000; p.firstDrawPublishNs = offset + 104000;
    p.endRequestNs = offset + 110000; p.endReturnNs = offset + 118000;
    p.batches = 3; p.drawBatches = p.drawCommands = 1;
    auto& o = t.owner;
    o.beginStartNs = offset + 100500; o.beginEndNs = offset + 101500;
    o.firstDrawCommandNs = offset + 105000; o.lastDrawCommandEndNs = offset + 110000;
    o.endStartNs = offset + 111000; o.endEndNs = offset + 117000;
    o.batches = 3; o.drawCommands = 1;
    o.drawTimingEnabled = o.presentRequested = true;
    o.drawPrepareNs = o.drawApiNs = o.drawSuffixNs = 1000;
    o.timedDrawScopes = 1;
    o.presentPrefixNs = o.presentSuffixNs = 1000; o.presentSwapNs = 3000;
    o.timedPresentScopes = 1;
    o.provider.addEntryNs = o.provider.gcNs = 500;
    o.provider.addEntryCalls = o.provider.gcCalls = 1;
    for (unsigned i = 0; i < T::WaitCapacity + 4; ++i)
        o.add_wait(offset + 110000 + i * 10, offset + 110005 + i * 10);
}

static void check_frame_timeline_profile(const std::filesystem::path& dir)
{
    using T = port::render::FrameTimeline;
    P p;
    const auto beforeClocks = calls;
    assert(!p.capture_timeline());
    {
        P::Scope quiet(p, 0, 3, wait_us);
        quiet.next(P::Material);
        quiet.material_next(P::StateLighting);
        quiet.material_count(P::PointLights, 4);
    }
    assert(calls == beforeClocks);
    const auto base = (dir / "timeline.csv").string();
    assert(p.configure(base.c_str(), 2));
    p.begin_frame(capture(0, 1));
    assert(!p.capture_timeline());
    p.begin_frame(capture(0));
    auto* timeline = p.capture_timeline();
    assert(timeline && !timeline->producer.observed);
    profile_timeline_case(*timeline, 0);
    timeline->producer.drawCommands = 99;
    {
        P::Scope excluded(p, 0, 3, wait_us);
        excluded.next(P::Material); ns += 100;
    }
    p.end_frame(capture(0), 99, false, {});
    assert(!p.capture_timeline() && p.samples().empty());
    p.begin_frame(capture(0));
    assert(p.capture_timeline() == timeline && !timeline->producer.drawCommands);
    profile_timeline_case(*timeline, 0);
    const auto first = calls;
    {
        P::Scope accepted(p, 11, 3, wait_us);
        accepted.next(P::Material);
        accepted.material_count(P::ModelConcats);
        ns += 100; accepted.material_next(P::StateLighting);
        accepted.material_count(P::PointLights, 4);
        ns += 200; accepted.material_next(P::TextureResolution);
        accepted.material_count(P::TextureRequests, 2);
        accepted.material_count(P::TextureResolutions, 2);
        ns += 300; accepted.next(P::Geometry);
        accepted.submitted(1, 0, 0);
    }
    assert(calls - first == 6); // Constructor, parent boundaries, two child boundaries, destructor.
    {
        P::Scope invalidMatrix(p, 11, 3, wait_us);
        invalidMatrix.next(P::Material);
        invalidMatrix.material_count(P::ViewOnlyMatrices);
        invalidMatrix.material_count(P::InvalidMatrices);
        ns += 10; // Return before the first child boundary.
    }
    {
        P::Scope invalidTexture(p, 11, 3, wait_us);
        invalidTexture.next(P::Material);
        invalidTexture.material_count(P::ViewOnlyMatrices);
        ns += 20; invalidTexture.material_next(P::StateLighting);
        ns += 30; invalidTexture.material_next(P::TextureResolution);
        invalidTexture.material_count(P::TextureRequests);
        invalidTexture.material_count(P::TextureFailures);
        ns += 40;
    }
    try
    {
        P::Scope throwing(p, 11, 3, wait_us);
        throwing.next(P::Material);
        throwing.material_count(P::ViewOnlyMatrices);
        ns += 50; throwing.material_next(P::StateLighting);
        ns += 60;
        throw 1;
    }
    catch (int) {}
    port::render::Renderer::FrameTiming timing{};
    timing.drawUs = 3; timing.queueUs = 5;
    p.end_frame(capture(1), 100, false, timing);
    assert(p.samples().size() == 1 && !p.capture_timeline());
    const auto& row = p.samples().front();
    const auto& m = row.materials;
    assert((m.ns == std::array<uint64_t, 3>{180, 290, 340}));
    assert(std::accumulate(m.ns.begin(), m.ns.end(), uint64_t{}) == row.producer.ns[P::Material]);
    assert(m.counts[P::MaterialAttempts] == 4 && m.counts[P::ModelConcats] == 1 && m.counts[P::ViewOnlyMatrices] == 3);
    assert(m.counts[P::TextureRequests] == 3 && m.counts[P::TextureResolutions] == 2 && m.counts[P::TextureFailures] == 1);
    assert(row.timeline.owner.waitCalls == 36 && row.timeline.owner.storedWaits == 32 && row.timeline.owner.waitOverflow == 4);
    assert(row.timeline.owner.readyWaitNs == 180 && row.timeline.owner.maxReadyWaitNs == 5);
    assert(!std::filesystem::exists(base + ".timeline.csv"));
    p.begin_frame(capture(1));
    assert(!p.capture_timeline()->producer.observed && !p.capture_timeline()->owner.waitCalls);
    profile_timeline_case(*p.capture_timeline(), 1000000);
    p.capture_timeline()->owner.provider.lastAddEntryResult = -5;
    p.capture_timeline()->owner.provider.addEntryErrors = 1;
    // Fail one sidecar atomically; all selected records remain available for retry.
    std::filesystem::create_directory(base + ".timeline_waits.csv");
    auto done = capture(2); done.state = PORT_CAPTURE_COMPLETE;
    p.end_frame(done, 101, false, timing);
    assert(p.output_error() && p.samples().size() == 2);
    assert(p.samples()[0].timeline.producer.drawCommands == 1); // No alias with the next scratch record.
    assert(!std::filesystem::exists(base + ".timeline_waits.csv.tmp"));
    std::filesystem::remove(base + ".timeline_waits.csv");
    assert(p.write() && !p.output_error());
    std::ifstream output(base + ".timeline.csv");
    std::string line;
    std::getline(output, line); assert(line.starts_with("# strikers-frame-timeline-v1 "));
    std::getline(output, line); assert(line.find("display_add_entry_last_result") != std::string::npos);
    std::getline(output, line); assert(line.starts_with("0,600,100,0,"));
    std::getline(output, line); assert(line.starts_with("1,601,101,0,"));
    assert(line.ends_with(",500,1,1,-5,500,1,0,0") && !std::getline(output, line));
    const auto clocks = calls;
    p.begin_frame(done);
    { P::Scope after(p, 0, 3, wait_us); after.next(P::Material); after.material_next(P::TextureResolution); }
    assert(!p.capture_timeline() && calls == clocks);
    P mixed;
    assert(mixed.configure((dir / "mixed-timeline.csv").c_str(), 2));
    mixed.begin_frame(capture(0)); profile_timeline_case(*mixed.capture_timeline(), 0);
    mixed.end_frame(capture(1), 100, false, timing);
    mixed.begin_frame(capture(1)); mixed.end_frame(done, 101, false, timing);
    assert(mixed.output_error() && !mixed.write());
    std::puts("Timeline profile: quiet window, exact Material children/errors, immutable selection, overflow, identity and writer recovery passed");
}
