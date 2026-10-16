#pragma once

// Implementation boundary for the single native match renderer. The public
// game hooks remain in port/native_match_runtime.hpp; resource, material and
// diagnostic details are private to this process-wide runtime.
#include "port/native_match_runtime.hpp"
#include "port/deferred_renderer.hpp"
#include "port/native_model_sources.hpp"
#include "NL/glx/glxTexture.h"
#include "NL/nlMath.h"
#include <mutex>
#include <unordered_map>

struct GLSkinUserData;

namespace port::directmatch::detail
{
using namespace nativegxm;
using Renderer = render::DeferredRenderer;
using render::Raster;
using render::SceneMaterial;
using render::TextureCopy;

class MatchRuntime final
{
  public:
    MatchRuntime() = delete;
    static void note_ball_packets(const glModel* model) noexcept;
    static void enable(bool yes) noexcept;
    static bool enabled() noexcept;
    static bool uses_own_renderer() noexcept;
    static bool preload() noexcept;
    static bool begin_frame() noexcept;
    static bool end_frame() noexcept;
    static void discard_present() noexcept;
    static bool readback_presented_rgba8(std::vector<uint8_t>& pixels) noexcept;
    static void on_packet(eGLView view, unsigned long flags, const glModelPacket* packet) noexcept;
    static void on_display_list_created(const glModelPacket* packet, uintptr_t displayList,
                                        uint32_t markerLevel) noexcept;
    static void on_stitch_selectors(const glModelPacket* packet, const uint8_t* selectors,
                                    uint32_t count) noexcept;
    static void on_resource_release(uint32_t markerLevel) noexcept;
    static void shutdown() noexcept;
    static void begin_view(eGLView view) noexcept;
    static void capture_texture(uintptr_t handle, unsigned x, unsigned y, unsigned width,
                                unsigned height, bool intensity) noexcept;
    static void capture_shadow_atlas() noexcept;
    static void clear_scene(std::array<float, 4> color) noexcept;
    static void draw_message_bitmap(unsigned width, unsigned height, const uint8_t* bits,
                                    uint32_t color, bool centered) noexcept;
    static void clear_depth() noexcept;
    static void set_fog(float start, float end, bool enabled, unsigned type,
                        std::array<float, 4> color) noexcept;
    static void on_texture_release(uint32_t level) noexcept;
    static bool admit_texture(const PlatTexture* texture,
                              const nativegxm::TextureUpload& image) noexcept;
    static void notify_write(const void* ptr, size_t bytes) noexcept;

  private:
    // Static storage retains the established initialization and destruction
    // order; no lazy singleton or additional synchronization is introduced.
    // Invalidation copies only the live prefix; keep unused scratch uninitialized.
    struct Range
    {
        uintptr_t address;
        size_t size;
    };
    struct Geometry
    {
        uint32_t id = 0, level = 0;
        uint64_t generation = 0;
        std::vector<Range> ranges;
    };
    struct Texture
    {
        uint32_t id = 0, level = 0;
        Range pixels{}, palette{};
        bool target = false;
        int shadowPartition = -1;
        bool fileNative = false;
        uint32_t resourceLevel = 0;
    };
    struct PacketGeometry
    {
        const ModelSource* source = nullptr;
        std::span<const uint16_t> indices;
        ModelStreams streams;
        std::array<Range, 16> ranges{};
        unsigned rangeCount = 0;
        bool immutable = false, hasPn = false;
        uint64_t key = 0;
    };
    struct PreparedMaterial
    {
        SceneMaterial scene;
        Raster raster;
        std::array<float, 16> matrix;
        bool matrixOwned = false;
    };
    struct PacketTrace
    {
        eGLView view;
        const glModelPacket& packet;
        uint64_t initial;
        ~PacketTrace();
    };

    static bool active, started, frameDiscarded;
    static Renderer gpu;
    static ModelSourceRegistry sources;
    static uint64_t frameDrawStart, frameStartNs;
    static uint32_t shadowAtlas;
    static std::unordered_map<uint64_t, Geometry> geometry;
    static std::unordered_map<const PlatTexture*, Texture> textures;
    static std::mutex writeMutex;
    static std::array<Range, 4096> writes;
    static unsigned writeCount;
    static bool invalidateAll;
    static std::array<float, 4> viewFog, viewFogColor;
    static Renderer::FrameTiming timings;
    static uint64_t timingFrames;
    static uint64_t draws, textured, hud, characters, invalid, decoded, records, resident, dynamic;
    static const char* failure;
    static std::array<const glModelPacket*, 64> ballPackets;
    static unsigned ballPacketCount, ballPacketFrame;

    static bool overlaps(Range a, Range b);
    static const void* userdata(const glModelPacket& p, eGLUserData type);
    static std::array<float, 4> vector4(const nlVector4& v);
    static void transform_point(std::array<float, 4>& out, const float* xyz, const nlMatrix4& m,
                                bool direction);
    static void consume_writes();
    static uint32_t texture_for(const PlatTexture* texture);
    static bool prepare_packet_geometry(const glModelPacket& p, PacketGeometry& data);
    static bool prepare_skin(const glModelPacket& p, bool hasPn, SceneMaterial& m,
                             std::span<const GLSkinUserData>& skin);
    static bool prepare_material(eGLView view, const glModelPacket& p, PreparedMaterial& material);
    static bool resolve_model(const glModelPacket& p, const PacketGeometry& data,
                              std::span<const GLSkinUserData> skin, ModelVertices& model,
                              uint32_t& modelId);
    static void report();
    static void trace_present(bool presented);
    static void trace_invalid_matrix(eGLView view, const glModelPacket& p, const nlMatrix4& v,
                                     const nlMatrix4& projection, const nlMatrix4& mv,
                                     const std::array<float, 16>& matrix, bool matrixOwned);
    static void trace_ball_packet(eGLView view, const glModelPacket& p, const glModelPacket* ptr,
                                  uint32_t modelId, const PacketGeometry& data,
                                  const PreparedMaterial& material);
};
}   // namespace port::directmatch::detail
