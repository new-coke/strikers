#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"
#include "port/aspect.h"
#include "port/framerate.h"
#include "port/config.h"
#include "port/host.h"
#include "port/loading_indicator.hpp"
#include <cstdlib>
#include <cstring>

namespace port::directmatch::detail
{
bool MatchRuntime::active = false, MatchRuntime::started = false;
bool MatchRuntime::frameAsync = false;
Renderer MatchRuntime::gpu;
ModelSourceRegistry MatchRuntime::sources;
ModelVertices MatchRuntime::modelScratch;
ModelVertexScratch MatchRuntime::vertexScratch;
bool MatchRuntime::netConversion = false;
NetPacketRegistry MatchRuntime::netPackets;
bool MatchRuntime::frameDiscarded = false;
uint64_t MatchRuntime::frameDrawStart = 0, MatchRuntime::frameStartNs = 0;
uint32_t MatchRuntime::shadowAtlas = 0;
std::array<uint32_t, 4> MatchRuntime::viewTargets{};
std::unordered_map<uint64_t, MatchRuntime::Geometry> MatchRuntime::geometry;
std::unordered_map<const PlatTexture*, MatchRuntime::Texture> MatchRuntime::textures;
MatchRuntime::RangeBounds MatchRuntime::geometryBounds, MatchRuntime::mutableTextureBounds;
std::mutex MatchRuntime::writeMutex;
std::array<MatchRuntime::Range, 4096> MatchRuntime::writes{};
unsigned MatchRuntime::writeCount = 0;
bool MatchRuntime::invalidateAll = false;
std::array<float, 4> MatchRuntime::viewFog{}, MatchRuntime::viewFogColor{};
render::FogRangeState MatchRuntime::viewFogRange{};
bool MatchRuntime::fogRangeFrameFailed = false;
std::array<float, 4> MatchRuntime::copyClearColor{ 0, 0, 0, 64.f / 255.f };
render::SceneAlphaState MatchRuntime::alphaState;
Renderer::FrameTiming MatchRuntime::timings{};
render::PacketProfile MatchRuntime::packetProfile;
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

void MatchRuntime::note_net_packet(const glModelPacket* packet, unsigned source) noexcept
{
    if (!active || !(netConversion || packetProfile.recording())) return;
    const auto added = netPackets.add(glGetCurrentFrame(), packet, source);
    if (!packetProfile.recording()) return;
    if (added == NetPacketRegistry::Add::Added) packetProfile.net_registered(source);
    else if (added == NetPacketRegistry::Add::Overflow) packetProfile.net_tag_failure(true);
    else if (added == NetPacketRegistry::Add::Invalid) packetProfile.net_tag_failure(false);
}

bool MatchRuntime::preload() noexcept
{
    if (started)
        return true;
    viewFogRange = {};
    const auto* textureHandles = std::getenv("STRIKERS_VITAGL_TEXTURE_HANDLE_CACHE");
    glx_EnableMaterialTextureCache(textureHandles && std::strcmp(textureHandles, "1") == 0);
    const auto* net = std::getenv("STRIKERS_VITAGL_NET_CONVERSION");
    netConversion = net && std::strcmp(net, "1") == 0;
    const auto* profile = std::getenv("STRIKERS_VITAGL_TIMING");
    const auto* packets = std::getenv("STRIKERS_VITAGL_PACKET_PROFILE");
    if (packets && std::strcmp(packets, "1") == 0 && !packetProfile.enabled())
    {
        PortBenchCaptureStatus status{};
        PortBenchGetCaptureStatus(&status);
        if (!packetProfile.configure(std::getenv("STRIKERS_FRAME_CAPTURE"), status.requested))
            std::fprintf(stderr, "[packet-profile] requires a finite frame_capture (1..8192 frames)\n");
        else
            packetProfile.configure_nets(netConversion);
    }
    started = gpu.initialize((profile && *profile == '1') || packetProfile.enabled()) && gpu.initialize_scene_program();
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
    fogRangeFrameFailed = false;
    frameDrawStart = draws;
    frameStartNs = PortDiagnosticsEnabled() ? port_monotonic_ns() : 0;
    consume_writes();
    if (!preload()) return false;
    if (packetProfile.enabled())
    {
        PortBenchCaptureStatus status{};
        PortBenchGetCaptureStatus(&status);
        packetProfile.begin_frame(status);
    }
    if (netConversion || packetProfile.recording()) netPackets.begin(glGetCurrentFrame());
    PortBenchCaptureStatus status{};
    PortBenchGetCaptureStatus(&status);
    const char* asynchronous = std::getenv("STRIKERS_VITA_ASYNC_RENDERER");
    frameAsync = (!asynchronous || std::strcmp(asynchronous, "1") == 0) &&
                 !PortDiagnosticsEnabled() && !packetProfile.enabled() &&
                 status.state != PORT_CAPTURE_COLLECTING;
    return gpu.begin_frame(frameAsync ? nullptr : packetProfile.capture_timeline(), frameAsync);
}

void MatchRuntime::finish_packet_profile_frame() noexcept
{
    if (!packetProfile.enabled()) return;
    PortBenchCaptureStatus status{};
    PortBenchGetCaptureStatus(&status);
    const auto before = packetProfile.samples().size();
    packetProfile.end_frame(status, glGetCurrentFrame(), frameDiscarded, gpu.completed_timing());
    if (before < status.requested && packetProfile.samples().size() == status.requested)
        report();
}

bool MatchRuntime::end_frame() noexcept
{
    const bool async = frameAsync;
    const bool result = gpu.end_frame(!frameDiscarded, async);
    frameAsync = false;
    // Deferred command failures already suppress the swap. Reflect that generic
    // failure in the frontend profile, without counting it again as a fog error.
    if (!result)
    {
        failure = "frame_end";
        frameDiscarded = true;
    }
    if (result)
        PortNativeRetrace();
    trace_present(result && !frameDiscarded);
    // The quiet asynchronous path has only queued a frame, not retired it.
    // Read owner-owned timing exclusively after a synchronous completion.
    const auto t = async ? Renderer::FrameTiming{} : gpu.completed_timing();
    if (!async && (t.drawUs || t.fenceUs))
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
    if (!result || (!packetProfile.enabled() && (PortDiagnosticsEnabled() || timingFrames) && glGetCurrentFrame() % 60 == 0))
        report();
    return result;
}

void MatchRuntime::discard_present() noexcept { frameDiscarded = true; }

void MatchRuntime::set_black(bool black) noexcept
{
    if (!preload()) return;
    if (!gpu.set_black(black))
    {
        failure = "present_black";
        ++invalid;
        frameDiscarded = true;
    }
}

bool MatchRuntime::set_loading(const loading::Request& request) noexcept
{
    if (!request.submit || (!request.begin && !started))
        return true;
    const auto reject = [&](const char* reason) {
        // A failed re-arm must not leave a previous indicator service alive.
        if (started)
            gpu.set_loading(false, {});
        failure = reason;
        ++invalid;
        frameDiscarded = true;
        return false;
    };
    if (request.begin && request.targetFps != 50 && request.targetFps != 60)
        return reject("loading_dimensions");
    if (request.begin && !preload())
        return reject("loading_initialize");
    try
    {
        render::LoadingIndicator indicator;
        if (request.begin)
        {
            for (unsigned i = 0; i < indicator.images.size(); ++i)
            {
                auto& image = indicator.images[i];
                image.width = image.height = 32;
                image.stride = 32;
                image.mipCount = 1;
                image.storage = TextureStorage::Rgba8;
                const auto& operations = i ? loading::selectedOperations : loading::normalOperations;
                image.pixels.assign(operations.begin(), operations.end());
            }
            indicator.rectangles = loading::rectangles(request);
            indicator.waitRetraces = request.waitRetraces;
            indicator.phaseRetraces = loading::phase_retraces(request.targetFps);
            indicator.blackBackground = request.blackBackground;
        }
        if (!gpu.set_loading(request.begin, indicator))
            return reject("loading_indicator");
    }
    catch (...)
    {
        return reject("loading_allocation");
    }
    return true;
}

void MatchRuntime::clear_depth() noexcept
{
    if (!gpu.clear_depth())
    {
        failure = "depth_clear";
        ++invalid;
        frameDiscarded = true;
    }
}

void MatchRuntime::set_fog(float start, float end, bool enabled, unsigned type,
                           std::array<float, 4> color) noexcept
{
    viewFog = { start, end, float(enabled), float(type) };
    viewFogColor = color;
}

bool MatchRuntime::reject_fog_range(const char* reason) noexcept
{
    // The same invalid view can reach several packets. Count its fog failure
    // only once per frame, while keeping every such frame unpresentable.
    if (!fogRangeFrameFailed)
    {
        failure = reason;
        ++invalid;
        fogRangeFrameFailed = true;
    }
    frameDiscarded = true;
    return false;
}

bool MatchRuntime::set_fog_range(bool enabled,
                                const std::array<float, 16>& gxViewProjection) noexcept
{
    if (!enabled)
    {
        viewFogRange.enabled = false;
        return true;
    }
    render::FogRangeState next;
    if (!render::make_fog_range_state(gxViewProjection, next))
    {
        // Retain finite owned bytes, but never use a stale table for a new view.
        viewFogRange.enabled = true;
        viewFogRange.valid = false;
        return reject_fog_range("fog_range_projection");
    }
    viewFogRange = next;
    return true;
}

void MatchRuntime::shutdown() noexcept
{
    glx_EnableMaterialTextureCache(false);
    glx_InvalidateTextureHandles();
    gpu.shutdown();
    sources.clear();
    geometry.clear();
    textures.clear();
    geometryBounds = {};
    mutableTextureBounds = {};
    modelScratch = {};
    vertexScratch = {};
    netPackets.clear();
    netConversion = false;
    shadowAtlas = 0;
    viewTargets.fill(0);
    copyClearColor = { 0, 0, 0, 64.f / 255.f };
    alphaState = {};
    viewFogRange = {};
    fogRangeFrameFailed = false;
    frameAsync = false;
    started = false;
}

bool MatchRuntime::readback_presented_rgba8(std::vector<uint8_t>& p) noexcept
{
    return gpu.readback_presented_rgba8(p);
}

void MatchRuntime::begin_view(eGLView view) noexcept
{
    const auto* state = glViewGetState(view);
    if (!state || unsigned(state->target) >= viewTargets.size())
    {
        failure = "view_target_index";
        ++invalid;
        frameDiscarded = true;
        return;
    }
    uint32_t target = 0;
    if (state->target >= GLTG_Staging)
    {
        auto& t = viewTargets[state->target];
        if (!t)
            t = gpu.create_target(960, 544);
        if (!t)
        {
            failure = "view_target_allocation";
            ++invalid;
            frameDiscarded = true;
            return;
        }
        target = t;
    }
    if (!gpu.select_target(target, state->bClearColour, state->bClearDepth))
    {
        failure = "view_target";
        ++invalid;
        frameDiscarded = true;
    }
}

void MatchRuntime::clear_scene(std::array<float, 4> color) noexcept
{
    copyClearColor = color;
    if (!gpu.clear_color(color))
    {
        failure = "view_clear";
        ++invalid;
        frameDiscarded = true;
    }
}

void MatchRuntime::draw_message_bitmap(unsigned width, unsigned height, const uint8_t* bits,
                                       uint32_t color, bool centered) noexcept
{
    const auto reject = [](const char* reason) {
        failure = reason;
        ++invalid;
        frameDiscarded = true;
    };
    try
    {
        // Retail bitmap rows are byte aligned; rejecting a partial byte avoids
        // indexing a differently packed source as width/8 bytes per row.
        if (!bits || !width || !height || width > 960 || height > 448 || (width & 7u))
        {
            reject("message_bitmap_dimensions");
            return;
        }
        const float logical = PortLogicalFrameWidth();
        if (!(logical > 0))
        {
            reject("message_bitmap_dimensions");
            return;
        }
        if (!preload()) return;
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
        const float x = (logical - width) * .5f;
        const float y = centered ? (448 - height) * .5f : 416.f - height;
        // This operation targets only the display surface and waits for the
        // swap, preserving any in-progress scene/target. In particular, the
        // loading message is visible before a synchronous FE load can block.
        if (!gpu.present_bitmap(image, { x / logical * 960.f, y / 448.f * 544.f,
                                         width / logical * 960.f, height / 448.f * 544.f }))
            reject("message_bitmap_present");
    }
    catch (...)
    {
        reject("message_bitmap_allocation");
    }
}
}   // namespace port::directmatch::detail

namespace port::directmatch
{
void note_ball_packets(const glModel* model) noexcept
{
    detail::MatchRuntime::note_ball_packets(model);
}
void note_net_packet(const glModelPacket* packet, unsigned source) noexcept
{
    detail::MatchRuntime::note_net_packet(packet, source);
}

void enable(bool yes) noexcept { detail::MatchRuntime::enable(yes); }

bool enabled() noexcept { return detail::MatchRuntime::enabled(); }

bool uses_own_renderer() noexcept { return detail::MatchRuntime::uses_own_renderer(); }

bool preload() noexcept { return detail::MatchRuntime::preload(); }

bool begin_frame() noexcept { return detail::MatchRuntime::begin_frame(); }

bool end_frame() noexcept { return detail::MatchRuntime::end_frame(); }

void finish_packet_profile_frame() noexcept { detail::MatchRuntime::finish_packet_profile_frame(); }

void discard_present() noexcept { detail::MatchRuntime::discard_present(); }

void set_black(bool black) noexcept { detail::MatchRuntime::set_black(black); }
bool set_loading(const loading::Request& request) noexcept { return detail::MatchRuntime::set_loading(request); }

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
                     bool intensity, unsigned destinationWidth, unsigned destinationHeight,
                     bool clearSource) noexcept
{
    detail::MatchRuntime::capture_texture(handle, x, y, width, height, intensity,
                                         destinationWidth, destinationHeight, clearSource);
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

bool set_fog_range(bool enabled, const std::array<float, 16>& gxViewProjection) noexcept
{
    return detail::MatchRuntime::set_fog_range(enabled, gxViewProjection);
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
