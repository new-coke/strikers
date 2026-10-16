#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"

namespace port::directmatch::detail
{
void MatchRuntime::on_packet(eGLView view, unsigned long, const glModelPacket* ptr) noexcept
{
    if (!active || !ptr)
        return;
    try
    {
        consume_writes();
        const auto& p = *ptr;
        PacketTrace trace{ view, p, invalid };
        PacketGeometry data;
        if (!prepare_packet_geometry(p, data))
            return;

        PreparedMaterial prepared;
        std::span<const GLSkinUserData> skin;
        if (!prepare_skin(p, data.hasPn, prepared.scene, skin))
            return;
        if (!prepare_material(view, p, prepared))
            return;

        uint32_t modelId = 0;
        ModelVertices model;
        if (!resolve_model(p, data, skin, model, modelId))
            return;
        trace_ball_packet(view, p, ptr, modelId, data, prepared);
        const auto& matrix = prepared.matrix;
        const auto& raster = prepared.raster;
        const auto& m = prepared.scene;
        if (!gpu.draw_scene(modelId, model.vertices, model.indices, matrix, raster, m))
        {
            failure = "scene_draw";
            ++invalid;
            return;
        }
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
    }
}
}   // namespace port::directmatch::detail
#endif
