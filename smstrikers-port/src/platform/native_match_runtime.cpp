#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"
#include "port/aspect.h"
#include "port/framerate.h"
#include "port/config.h"
#include "port/host.h"
#include <cstdlib>
#include <cstring>

namespace port::directmatch::detail
{
bool MatchRuntime::active = false, MatchRuntime::started = false;
Renderer MatchRuntime::gpu;
ModelSourceRegistry MatchRuntime::sources;
bool MatchRuntime::frameDiscarded = false;
uint64_t MatchRuntime::frameDrawStart = 0, MatchRuntime::frameStartNs = 0;
uint32_t MatchRuntime::shadowAtlas = 0;
std::unordered_map<uint64_t, MatchRuntime::Geometry> MatchRuntime::geometry;
std::unordered_map<const PlatTexture*, MatchRuntime::Texture> MatchRuntime::textures;
std::mutex MatchRuntime::writeMutex;
std::array<MatchRuntime::Range, 4096> MatchRuntime::writes{};
unsigned MatchRuntime::writeCount = 0;
bool MatchRuntime::invalidateAll = false;
std::array<float, 4> MatchRuntime::viewFog{}, MatchRuntime::viewFogColor{};
Renderer::FrameTiming MatchRuntime::timings{};
uint64_t MatchRuntime::timingFrames = 0;
uint64_t MatchRuntime::draws = 0, MatchRuntime::textured = 0, MatchRuntime::hud = 0,
         MatchRuntime::characters = 0, MatchRuntime::invalid = 0, MatchRuntime::decoded = 0,
         MatchRuntime::records = 0, MatchRuntime::resident = 0, MatchRuntime::dynamic = 0;
const char* MatchRuntime::failure = "none";
std::array<const glModelPacket*, 64> MatchRuntime::ballPackets{};
unsigned MatchRuntime::ballPacketCount = 0, MatchRuntime::ballPacketFrame = UINT32_MAX;

void MatchRuntime::enable(bool x) noexcept { active = x; }

bool MatchRuntime::enabled() noexcept { return active; }

bool MatchRuntime::uses_own_renderer() noexcept { return active; }

bool MatchRuntime::preload() noexcept
{
    if (started)
        return true;
    const auto* profile = std::getenv("STRIKERS_VITAGL_TIMING");
    started = gpu.initialize(profile && *profile == '1') && gpu.initialize_scene_program();
    if (!started)
    {
        failure = "initialize";
        report();
    }
    return started;
}

bool MatchRuntime::begin_frame() noexcept
{
    frameDiscarded = false;
    frameDrawStart = draws;
    frameStartNs = PortDiagnosticsEnabled() ? port_monotonic_ns() : 0;
    consume_writes();
    return preload() && gpu.begin_frame();
}

bool MatchRuntime::end_frame() noexcept
{
    bool result = gpu.end_frame(!frameDiscarded);
    if (result)
        PortNativeRetrace();
    trace_present(result && !frameDiscarded);
    const auto t = gpu.completed_timing();
    if (t.drawUs || t.fenceUs)
    {
        ++timingFrames;
        timings.fenceUs += t.fenceUs;
        timings.uploadUs += t.uploadUs;
        timings.drawUs += t.drawUs;
        timings.queueUs += t.queueUs;
        timings.draws += t.draws;
        timings.dynamicVertexBytes += t.dynamicVertexBytes;
        timings.frontendWaitUs += t.frontendWaitUs;
        timings.commandBytes += t.commandBytes;
        timings.slotWaits += t.slotWaits;
        timings.affineUvDraws += t.affineUvDraws;
        timings.projectiveUvDraws += t.projectiveUvDraws;
        timings.vertexFogDraws += t.vertexFogDraws;
    }
    if (!result || ((PortDiagnosticsEnabled() || timingFrames) && glGetCurrentFrame() % 60 == 0))
        report();
    return result;
}

void MatchRuntime::discard_present() noexcept { frameDiscarded = true; }

void MatchRuntime::clear_depth() noexcept { gpu.clear_depth(); }

void MatchRuntime::set_fog(float start, float end, bool enabled, unsigned type,
                           std::array<float, 4> color) noexcept
{
    viewFog = { start, end, float(enabled), float(type) };
    viewFogColor = color;
}

void MatchRuntime::shutdown() noexcept
{
    gpu.shutdown();
    sources.clear();
    geometry.clear();
    textures.clear();
    shadowAtlas = 0;
    started = false;
}

bool MatchRuntime::readback_presented_rgba8(std::vector<uint8_t>& p) noexcept
{
    return gpu.readback_presented_rgba8(p);
}

void MatchRuntime::begin_view(eGLView view) noexcept
{
    static std::array<uint32_t, 4> targets{};
    const auto* state = glViewGetState(view);
    uint32_t target = 0;
    if (state->target >= GLTG_Staging)
    {
        auto& t = targets[state->target];
        if (!t)
            t = gpu.create_target(960, 544);
        target = t;
    }
    if (!gpu.select_target(target, state->bClearColour, state->bClearDepth))
    {
        failure = "view_target";
        ++invalid;
    }
}

void MatchRuntime::clear_scene(std::array<float, 4> color) noexcept
{
    if (!gpu.clear_color(color))
    {
        failure = "view_clear";
        ++invalid;
    }
}

void MatchRuntime::draw_message_bitmap(unsigned width, unsigned height, const uint8_t* bits,
                                       uint32_t color, bool centered) noexcept
{
    try
    {
        if (!bits || !width || !height || width > 960 || height > 448 || !preload())
            return;
        bool own = !gpu.frame_active();
        if (own && !gpu.begin_frame())
            return;
        TextureUpload image;
        image.width = width;
        image.height = height;
        image.stride = (width + 7u) & ~7u;
        image.mipCount = 1;
        image.storage = TextureStorage::Rgba8;
        image.pixels.resize(size_t(image.stride) * height * 4);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
            {
                auto* p = image.pixels.data() + (y * image.stride + x) * 4;
                if (bits[y * (width / 8) + x / 8] & (1u << (x % 8)))
                {
                    p[0] = (color >> 16) & 255;
                    p[1] = (color >> 8) & 255;
                    p[2] = color & 255;
                    p[3] = color >> 24;
                }
            }
        unsigned texture = gpu.create_texture(image);
        if (!texture)
            return;
        SceneMaterial m;
        m.textures[0] = texture;
        m.wrapU[0] = m.wrapV[0] = 1;
        m.controls[0] = 1;
        const float logical = PortLogicalFrameWidth();
        float x = (logical - width) * .5f, y = centered ? (448 - height) * .5f : 416.f - height;
        ModelVertex v[4];
        float l = x / logical * 2 - 1, r = (x + width) / logical * 2 - 1, t = 1 - y / 448 * 2,
              b = 1 - (y + height) / 448 * 2;
        v[0].position = { l, t, 0, 1 };
        v[1].position = { r, t, 0, 1 };
        v[2].position = { r, b, 0, 1 };
        v[3].position = { l, b, 0, 1 };
        v[0].uv = { 0, 0, 1 };
        v[1].uv = { 1, 0, 1 };
        v[2].uv = { 1, 1, 1 };
        v[3].uv = { 0, 1, 1 };
        uint16_t indices[]{ 0, 1, 2, 0, 2, 3 };
        Raster raster;
        raster.depthTest = raster.depthWrite = false;
        raster.blend = 1;
        std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        gpu.draw_scene(0, v, indices, identity, raster, m);
        gpu.release_texture(texture);
        if (own)
            gpu.end_frame();
    }
    catch (...)
    {
        failure = "message_bitmap";
    }
}
}   // namespace port::directmatch::detail

namespace port::directmatch
{
void note_ball_packets(const glModel* model) noexcept
{
    detail::MatchRuntime::note_ball_packets(model);
}

void enable(bool yes) noexcept { detail::MatchRuntime::enable(yes); }

bool enabled() noexcept { return detail::MatchRuntime::enabled(); }

bool uses_own_renderer() noexcept { return detail::MatchRuntime::uses_own_renderer(); }

bool preload() noexcept { return detail::MatchRuntime::preload(); }

bool begin_frame() noexcept { return detail::MatchRuntime::begin_frame(); }

bool end_frame() noexcept { return detail::MatchRuntime::end_frame(); }

void discard_present() noexcept { detail::MatchRuntime::discard_present(); }

bool readback_presented_rgba8(std::vector<uint8_t>& pixels) noexcept
{
    return detail::MatchRuntime::readback_presented_rgba8(pixels);
}

void on_packet(eGLView view, unsigned long flags, const glModelPacket* packet) noexcept
{
    detail::MatchRuntime::on_packet(view, flags, packet);
}

void on_display_list_created(const glModelPacket* packet, uintptr_t displayList,
                             uint32_t markerLevel) noexcept
{
    detail::MatchRuntime::on_display_list_created(packet, displayList, markerLevel);
}

void on_stitch_selectors(const glModelPacket* packet, const uint8_t* selectors,
                         uint32_t count) noexcept
{
    detail::MatchRuntime::on_stitch_selectors(packet, selectors, count);
}

void on_resource_release(uint32_t markerLevel) noexcept
{
    detail::MatchRuntime::on_resource_release(markerLevel);
}

void shutdown() noexcept { detail::MatchRuntime::shutdown(); }

void begin_view(eGLView view) noexcept { detail::MatchRuntime::begin_view(view); }

void capture_texture(uintptr_t handle, unsigned x, unsigned y, unsigned width, unsigned height,
                     bool intensity) noexcept
{
    detail::MatchRuntime::capture_texture(handle, x, y, width, height, intensity);
}

void capture_shadow_atlas() noexcept { detail::MatchRuntime::capture_shadow_atlas(); }

void clear_scene(std::array<float, 4> color) noexcept { detail::MatchRuntime::clear_scene(color); }

void draw_message_bitmap(unsigned width, unsigned height, const uint8_t* bits, uint32_t color,
                         bool centered) noexcept
{
    detail::MatchRuntime::draw_message_bitmap(width, height, bits, color, centered);
}

void clear_depth() noexcept { detail::MatchRuntime::clear_depth(); }

void set_fog(float start, float end, bool enabled, unsigned type,
             std::array<float, 4> color) noexcept
{
    detail::MatchRuntime::set_fog(start, end, enabled, type, color);
}

void on_texture_release(uint32_t level) noexcept
{
    detail::MatchRuntime::on_texture_release(level);
}

bool admit_texture(const PlatTexture* texture, const nativegxm::TextureUpload& image) noexcept
{
    return detail::MatchRuntime::admit_texture(texture, image);
}

void notify_write(const void* ptr, size_t bytes) noexcept
{
    detail::MatchRuntime::notify_write(ptr, bytes);
}
}   // namespace port::directmatch
#endif
