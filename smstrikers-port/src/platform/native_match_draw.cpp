#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"
#include "Game/GL/ShaderSkinMesh.h"

namespace port::directmatch::detail
{
void MatchRuntime::on_packet(eGLView view, unsigned long flags, const glModelPacket* ptr) noexcept
{
    if (!active || !ptr)
        return;
    try
    {
        const auto& p = *ptr;
        using Profile = render::PacketProfile;
        Profile::Scope profile{packetProfile, unsigned(view), p.numVertices,
                               [] { return gpu.pending_frontend_wait_us(); }};
        const bool profileNets = packetProfile.recording();
        NetPacketRegistry::Match net;
        // The constructor attaches these packets only to this unsorted view.
        // Other views keep the generic path without probing the tag registry.
        if ((netConversion || profileNets) && view == GLV_UnsortedPerspective)
        {
            net = netPackets.find(glGetCurrentFrame(), ptr);
            if (profileNets)
            {
                profile.net_source(net.source);
                if (net.descriptorMismatch) packetProfile.net_tag_failure(false);
            }
        }
        consume_writes();
        PacketTrace trace{ view, p, invalid };
        // GLX updates material state before sending geometry, so even a packet
        // rejected below must retain its partial TEV updates for later packets.
        observe_material_state(flags, p);
        profile.next(Profile::Arrays);
        PacketGeometry data;
        if (!prepare_packet_geometry(p, data))
            return;

        prepared.reset(preparedPalette);
        preparedPalette = data.hasPn;
        std::span<const GLSkinUserData> skin;
        profile.next(Profile::Skin);
        if (!prepare_skin(p, data.hasPn, prepared.scene, skin))
            return;
        profile.next(Profile::Material);
        if (!prepare_material(view, p, prepared, profileNets ? &profile : nullptr))
            return;
        prepared.raster.primitive = p.primType >= 4 ? 1 : 0;

        uint32_t modelId = 0;
        auto& model = modelScratch;
        model.vertices.clear();
        model.indices.clear();
        struct RecycleGeometry
        {
            ModelVertices& model;
            ModelVertexScratch& scratch;
            ~RecycleGeometry() { recycle_model_vertex_scratch(model, scratch); }
        } recycle{ model, vertexScratch };
        profile.next(Profile::Geometry);
        bool eligibleNet = false, fastNet = false;
        if (net.source < 2 && !data.source && !data.immutable && !data.nativeDisplayList &&
            !data.hasPn && skin.empty() &&
            p.numStreams == 3 && p.streams[0].id == 0 && p.streams[1].id == 2 && p.streams[2].id == 3)
            eligibleNet = bounded_net_record_count(data.streams, data.indices, p.numVertices, p.primType) != 0;
        if (profileNets && net.source < 2) profile.net_route(eligibleNet, false, false);
        if (netConversion && eligibleNet)
            fastNet = prepare_net_model_vertices(data.streams, data.indices, p.numVertices,
                                                 p.primType, model, vertexScratch);
        if (profileNets && net.source < 2) profile.net_route(eligibleNet, fastNet, !fastNet);
        if (!fastNet && !resolve_model(p, data, skin, model, modelId))
            return;
        // Skip resident packets whose box lies wholly outside one clip plane.
        if (modelId && !fastNet && skin.empty() && resolvedGeometry && resolvedGeometry->boxed &&
            view != GLV_ShadowTexture)
        {
            const auto& b = resolvedGeometry->box;
            const auto& m = prepared.matrix;
            unsigned outside = 0x3F;
            for (unsigned c = 0; c < 8 && outside; ++c)
            {
                const float x = b[c & 1 ? 3 : 0], y = b[c & 2 ? 4 : 1], z = b[c & 4 ? 5 : 2];
                const float cx = m[0] * x + m[4] * y + m[8] * z + m[12];
                const float cy = m[1] * x + m[5] * y + m[9] * z + m[13];
                const float cz = m[2] * x + m[6] * y + m[10] * z + m[14];
                const float cw = m[3] * x + m[7] * y + m[11] * z + m[15];
                unsigned corner = 0;
                if (cx > cw) corner |= 1;
                if (cx < -cw) corner |= 2;
                if (cy > cw) corner |= 4;
                if (cy < -cw) corner |= 8;
                if (cz < -cw) corner |= 16;   // GX depth runs -w..0
                if (cz > 0.f) corner |= 32;
                outside &= corner;
            }
            if (outside)
                return;
        }
        trace_ball_packet(view, p, ptr, modelId, data, prepared);
        const auto& matrix = prepared.matrix;
        const auto& raster = prepared.raster;
        const auto& m = prepared.scene;
        profile.next(Profile::Submit);
        // Selectors are reg + 99, three rows each: copy rows up to the last one used.
        unsigned paletteRows = 0;
        if (data.hasPn)
        {
            paletteRows = 3;
            for (const auto& bone : skin)
                paletteRows = std::max(paletteRows, unsigned(bone.reg + 99 + 3));
        }
        if (!gpu.draw_scene(modelId, model.vertices, model.indices, matrix, raster, m, paletteRows))
        {
            failure = "scene_draw";
            ++invalid;
            frameDiscarded = true;
            return;
        }
        profile.submitted(modelId, model.vertices.size(), model.indices.size());
        profile.next(Profile::Cleanup);
        ++draws;
        if (p.state.texconfig)
            ++textured;
        if (view == GLV_Anark)
            ++hud;
        if (view == GLV_Characters)
            ++characters;
        if (!modelId)
            ++dynamic;
    }
    catch (...)
    {
        failure = "draw_allocation";
        ++invalid;
        frameDiscarded = true;
    }
}
}   // namespace port::directmatch::detail
#endif
