
static void check_net_conversion() {
    namespace match = port::directmatch;
    using Access = port::directmatch::detail::MatchRuntimeTestAccess;
    using P = port::render::PacketProfile;
    using namespace port::nativegxm;
    const auto output = std::filesystem::path(std::getenv("NET_GATE_OUTPUT"));
    std::filesystem::create_directories(output);
    match::shutdown();
    Access::reset_profile();
    match::enable(true);
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
    unsetenv("STRIKERS_VITAGL_NET_CONVERSION");
    netCapture = {};
    const auto quietClocks = netClockCalls;
    assert(match::begin_frame());
    assert(!Access::net_enabled());
    // A disabled provenance hook must return before dereferencing its argument.
    match::note_net_packet(reinterpret_cast<const glModelPacket*>(uintptr_t(1)), 0);
    assert(netClockCalls == quietClocks);
    assert(match::end_frame());
    match::shutdown();

    float positions[8][3]{};
    std::array<uint8_t, 32> colors{};
    int16_t uv[8][2]{};
    std::array<uint16_t, 16> indices{7,2,7,1,0,0,3,3,4,6,5,5,2,1,7,0};
    glModelStream streams[]{
        {reinterpret_cast<uintptr_t>(positions), 0, 12, 0, sizeof positions},
        {reinterpret_cast<uintptr_t>(colors.data()), 2, 4, 0, sizeof colors},
        {reinterpret_cast<uintptr_t>(uv), 3, 4, 0, sizeof uv}
    };
    glModelPacket p{};
    p.indexBuffer = reinterpret_cast<uintptr_t>(indices.data());
    p.numVertices = indices.size(); p.primType = 1; p.numStreams = 3; p.streams = streams;
    p.state.matrix = 1;
    p.state.program = fixture_program("3d unlit");
    auto second = p;
    const auto fresh = [&](unsigned frame, unsigned source) {
        for (unsigned i=0;i<8;++i) {
            positions[i][0] = float(int(frame) * 13 - int(i)) * .25f;
            positions[i][1] = float(source * 19 + i) * -3.f;
            positions[i][2] = frame + source * .5f + i * .125f;
            for (unsigned k=0;k<4;++k) colors[i*4+k] = uint8_t(frame*29+i*17+k+source);
            uv[i][0] = int16_t(frame*171+i*511);
            uv[i][1] = int16_t(-int(frame)*337+int(i)*117);
        }
    };
    const auto equivalent = [&](const glModelPacket& packet) {
        ModelStreams ms;
        ms.position = {{reinterpret_cast<const uint8_t*>(positions),sizeof positions},12,true};
        ms.color = {{colors.data(),colors.size()},4,true};
        ms.uv = {{reinterpret_cast<const uint8_t*>(uv),sizeof uv},4,true};
        ModelVertices reference;
        ModelVertexScratch scratch;
        assert(prepare_model_vertices(ms, indices, packet.numVertices, packet.primType, {}, {},
                                      reference, true, false, &scratch));
        assert(reference.indices == lastDrawIndices);
        assert(reference.vertices.size() == lastDrawVertices.size());
        assert(!std::memcmp(reference.vertices.data(), lastDrawVertices.data(),
                            reference.vertices.size()*sizeof(ModelVertex)));
    };
    identity(viewMatrix); identity(projectionMatrix);
    viewState = {};
    viewState.vpWidth=640; viewState.vpHeight=448;
    arenaOwns=false; diagnostics=false; failDraw=false;
    for (unsigned mode=0;mode<2;++mode) {
        match::shutdown();
        Access::reset_profile();
        currentFrame=90;
        const auto base=(output/(mode?"on.csv":"off.csv")).string();
        setenv("STRIKERS_VITAGL_NET_CONVERSION",mode?"1":"0",1);
        setenv("STRIKERS_VITAGL_PACKET_PROFILE","1",1);
        setenv("STRIKERS_FRAME_CAPTURE",base.c_str(),1);
        netCapture={PORT_CAPTURE_COLLECTING,0,10,1,0,599};
        auto clocks=netClockCalls;
        assert(match::begin_frame() && Access::net_enabled()==bool(mode));
        fresh(0,0);
        match::note_net_packet(&p,0);
        match::on_packet(GLV_UnsortedPerspective,0,&p);
        equivalent(p);
        assert(netClockCalls==clocks); // Profile has no warm-up clocks.
        assert(match::end_frame());
        ++currentFrame;
        match::finish_packet_profile_frame();
        assert(Access::profile().samples().empty());
        netCapture.warmupRemaining=0;
        assert(match::begin_frame());
        match::note_net_packet(&p,0);
        match::on_packet(GLV_UnsortedPerspective,0,&p);
        assert(match::end_frame());
        ++currentFrame;
        match::finish_packet_profile_frame(); // Real work, benchmark excluded it.
        assert(Access::profile().samples().empty());

        for (unsigned sample=0;sample<10;++sample) {
            p.indexBuffer=reinterpret_cast<uintptr_t>(indices.data());
            streams[1].id=2; streams[1].beData=0;
            fresh(sample+1,0);
            match::on_resource_release(0);
            assert(match::begin_frame());
            const auto previousInvalid=Access::invalid_count();
            auto previousSubmissions=submitted.size();
            if (sample==0) {
                for (unsigned source=0;source<2;++source) {
                    fresh(sample+1,source);
                    auto* packet=source?&second:&p;
                    match::note_net_packet(packet,source);
                    match::on_packet(GLV_UnsortedPerspective,0,packet);
                    equivalent(*packet);
                    if(!source) {
                        match::on_packet(GLV_Characters,0,packet);
                        equivalent(*packet); // A different view remains generic despite the tag.
                    }
                }
                auto untagged=p;
                match::on_packet(GLV_UnsortedPerspective,0,&untagged);
                equivalent(untagged); // Same shape does not imply net provenance.
            } else if (sample==1) {
                match::note_net_packet(&p,0);
                streams[1].beData=1; // Descriptor changes; RGBA byte semantics unchanged.
                match::on_packet(GLV_UnsortedPerspective,0,&p);
                equivalent(p);
                streams[1].beData=0;
            } else if (sample==2 || sample==3) {
                displayList.list=indices.data(); displayList.size=sizeof indices;
                if (sample==3) match::on_display_list_created(&p,ListHandle,3);
                p.indexBuffer=ListHandle;
                match::note_net_packet(&p,0);
                match::on_packet(GLV_UnsortedPerspective,0,&p);
                equivalent(p); // Both unregistered and registered native DL stay generic.
            } else if (sample==4) {
                streams[1].id=12;
                match::note_net_packet(&p,0);
                match::on_packet(GLV_UnsortedPerspective,0,&p);
                assert(submitted.size()==previousSubmissions);
                assert(!std::strcmp(Access::failure(),"native_indexed_skin"));
            } else if (sample==5) {
                positions[indices[0]][0]=std::numeric_limits<float>::quiet_NaN();
                match::note_net_packet(&p,0);
                match::on_packet(GLV_UnsortedPerspective,0,&p);
                assert(submitted.size()==previousSubmissions);
                assert(!std::strcmp(Access::failure(),"geometry_decode"));
            } else if (sample==6) {
                failDraw=true;
                match::note_net_packet(&p,0);
                match::on_packet(GLV_UnsortedPerspective,0,&p);
                failDraw=false;
                equivalent(p);
                assert(!std::strcmp(Access::failure(),"scene_draw"));
            } else if (sample==7) {
                Access::release_scratch();
                match::note_net_packet(&p,0);
                failNextHeapAllocation=true;
                match::on_packet(GLV_UnsortedPerspective,0,&p);
                assert(!failNextHeapAllocation);
                assert(submitted.size()==previousSubmissions);
                assert(!std::strcmp(Access::failure(),"draw_allocation"));
                assert(Access::scratch_empty());
            } else if (sample==8) {
                std::array<glModelPacket,9> packets;
                packets.fill(p);
                for(unsigned i=0;i<9;++i) match::note_net_packet(&packets[i],i%2);
                match::on_packet(GLV_UnsortedPerspective,0,&packets[0]);
                equivalent(p); // Overflow invalidates all provenance for this frame.
            } else {
                assert(Access::net_lookup(currentFrame,&p).source==2);
                match::on_packet(GLV_UnsortedPerspective,0,&p); // Previous frame's tag is gone.
                equivalent(p);
            }
            if(sample>=4 && sample<=7) assert(Access::invalid_count()==previousInvalid+1);
            else assert(Access::invalid_count()==previousInvalid);
            assert(match::end_frame());
            assert(lastPresent == (sample!=6 && sample!=7));
            ++currentFrame; // glSendFrame increments only after all packet callbacks.
            ++netCapture.samples;
            netCapture.lastFrame=600+sample;
            if(sample==9) netCapture.state=PORT_CAPTURE_COMPLETE;
            match::finish_packet_profile_frame();
            const auto& rows=Access::profile().samples();
            assert(rows.size()==sample+1);
            const auto& row=rows.back();
            const auto& n=row.nets;
            assert(row.sample==sample && row.liveFrame==600+sample && row.engineFrame==93+sample);
            assert(row.discarded==(sample==6||sample==7));
            assert(n.valid && n.enabled==bool(mode));
            if(sample==0) {
                for(unsigned source=0;source<2;++source) {
                    const auto& c=n.sources[source];
                    assert(n.registered[source]==1 && c[P::NetCensus]==1 && c[P::NetEligible]==1);
                    assert(c[P::NetFast]==mode && c[P::NetFallback]==!mode && c[P::NetAccepted]==1);
                    assert(c[P::NetReferences]==16 && c[P::NetVertices]==8 && c[P::NetIndices]==42);
                }
                assert(row.producer.counts[P::Attempts]==4);
            } else if(sample==1 || sample>=8) {
                assert(n.sources[0][P::NetCensus]==0 && n.sources[1][P::NetCensus]==0);
                assert(n.tagMismatch==(sample==1));
                assert(n.tagOverflow==(sample==8));
            } else {
                const auto& c=n.sources[0];
                assert(n.registered[0]==1 && c[P::NetCensus]==1);
                assert(c[P::NetEligible]==(sample>=5));
                assert(c[P::NetFast]==(mode && sample==6));
                assert(c[P::NetAccepted]==(sample<4));
                assert(c[P::NetRejected]==(sample>=4));
                const unsigned fallback = (sample==2||sample==3||sample==5||sample==6)?1u:
                    (sample==7&&!mode?1u:0u);
                assert(c[P::NetFallback]==(sample==6&&mode?0u:fallback));
            }
            const auto netGeometry=n.sources[0][P::NetGeometryNs]+n.sources[1][P::NetGeometryNs];
            assert(netGeometry<=row.producer.ns[P::Geometry]);
        }
        assert(!Access::profile().output_error());
        assert(std::filesystem::exists(base+".nets.csv"));
        const auto clocksAfter=netClockCalls;
        assert(match::begin_frame());
        match::on_packet(GLV_UnsortedPerspective,0,&second);
        assert(match::end_frame());
        match::finish_packet_profile_frame();
        assert(netClockCalls==clocksAfter); // Finite capture does not keep taking clocks.
        match::shutdown();
        assert(!Access::net_enabled() && Access::net_lookup(currentFrame,&p).source==2);
    }
    Access::reset_profile();
    netCapture={};
    unsetenv("STRIKERS_VITAGL_NET_CONVERSION");
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
    unsetenv("STRIKERS_FRAME_CAPTURE");
    std::puts("Native net runtime: OFF/ON provenance, fresh bytes, DL/PN rejection, capture identity, failures and retirement passed");
}
