#pragma once

// Implementation boundary for the single native match renderer. The public
// game hooks remain in port/native_match_runtime.hpp; resource, material and
// diagnostic details are private to this process-wide runtime.
#include "port/native_match_runtime.hpp"
#include "port/deferred_renderer.hpp"
#include "port/native_model_sources.hpp"
#include "port/packet_profile.hpp"
#include "port/scene_alpha_state.hpp"
#include "port/native_net_packets.hpp"
#include "port/native_net_vertices.hpp"
#include "NL/glx/glxTexture.h"
#include "NL/nlMath.h"
#include <cstddef>
#include <cstring>
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
    static void note_net_packet(const glModelPacket* packet, unsigned source) noexcept;
    static void enable(bool yes) noexcept;
    static bool enabled() noexcept;
    static bool uses_own_renderer() noexcept;
    static bool preload() noexcept;
    static bool begin_frame() noexcept;
    static bool end_frame() noexcept;
    static void finish_packet_profile_frame() noexcept;
    static void discard_present() noexcept;
    static void set_black(bool black) noexcept;
    static bool set_loading(const loading::Request& request) noexcept;
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
                                unsigned height, bool intensity, unsigned destinationWidth,
                                unsigned destinationHeight, bool clearSource) noexcept;
    static void capture_shadow_atlas() noexcept;
    static void clear_scene(std::array<float, 4> color) noexcept;
    static void draw_message_bitmap(unsigned width, unsigned height, const uint8_t* bits,
                                    uint32_t color, bool centered) noexcept;
    static void clear_depth() noexcept;
    static void set_fog(float start, float end, bool enabled, unsigned type,
                        std::array<float, 4> color) noexcept;
    static bool set_fog_range(bool enabled, const std::array<float, 16>& gxViewProjection) noexcept;
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
    struct RangeBounds
    {
        uintptr_t first = UINTPTR_MAX, last = 0;

        void include(Range range) noexcept
        {
            if (!range.size)
                return;
            first = range.address < first ? range.address : first;
            const auto end = last_byte(range);
            last = end > last ? end : last;
        }
        bool intersects(Range range) const noexcept
        {
            return first <= last && range.size && range.address <= last && last_byte(range) >= first;
        }

      private:
        static uintptr_t last_byte(Range range) noexcept
        {
            // Inclusive endpoints retain the final address without wrapping.
            return range.size - 1 > UINTPTR_MAX - range.address
                       ? UINTPTR_MAX : range.address + (range.size - 1);
        }
    };
    friend struct MatchRuntimeTestAccess;
    struct Geometry
    {
        uint32_t id = 0, level = 0;
        uint64_t generation = 0;
        std::vector<Range> ranges;
        std::array<float, 6> box{};
        bool boxed = false;
    };
    static const Geometry* resolvedGeometry;
    struct Texture
    {
        uint32_t id = 0, level = 0;
        Range pixels{}, palette{};
        bool target = false;
        uint32_t targetWidth = 0, targetHeight = 0;
        render::TargetFormat targetFormat = render::TargetFormat::Rgba8;
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
        bool immutable = false, hasPn = false, nativeDisplayList = false;
        uint64_t key = 0;
    };
    struct PreparedMaterial
    {
        // Copy constant defaults into each command's own material. No mutable
        // render state or palette is retained by this shared initializer.
        inline static constexpr SceneMaterial initialScene{};
        SceneMaterial scene = initialScene;
        Raster raster;
        std::array<float, 16> matrix;
        bool matrixOwned = false;
        // Only PN packets write the palette, so it is reset only after one did.
        void reset(bool paletteWritten) noexcept
        {
            constexpr size_t begin = offsetof(SceneMaterial, bones),
                             end = begin + sizeof(render::SceneBonePalette);
            auto* to = reinterpret_cast<unsigned char*>(&scene);
            const auto* from = reinterpret_cast<const unsigned char*>(&initialScene);
            std::memcpy(to, from, begin);
            std::memcpy(to + end, from + end, sizeof(SceneMaterial) - end);
            if (paletteWritten)
                scene.bones = initialScene.bones;
            raster = {};
            matrix = {};
            matrixOwned = false;
        }
    };
    static PreparedMaterial prepared;
    static bool preparedPalette;
    struct PacketTrace
    {
        eGLView view;
        const glModelPacket& packet;
        uint64_t initial;
        ~PacketTrace();
    };

    static bool active, started, frameDiscarded, frameAsync, asyncModels;
    static Renderer gpu;
    static ModelSourceRegistry sources;
    static ModelVertices modelScratch;
    static ModelVertexScratch vertexScratch;
    static bool netConversion;
    static NetPacketRegistry netPackets;
    static uint64_t frameDrawStart, frameStartNs;
    static uint32_t shadowAtlas;
    static std::array<uint32_t, 4> viewTargets;
    static std::unordered_map<uint64_t, Geometry> geometry;
    struct GeometryFront
    {
        uint64_t key = 0;
        Geometry* entry = nullptr;
        uint32_t generation = 0;
    };
    static std::array<GeometryFront, 512> geometryFront;
    static uint32_t geometryGeneration;
    static void forget_geometry() noexcept { ++geometryGeneration; }
    static Geometry* find_geometry(uint64_t key) noexcept
    {
        auto& front = geometryFront[(key ^ key >> 17 ^ key >> 37) & 511u];
        if (front.entry && front.key == key && front.generation == geometryGeneration)
            return front.entry;
        const auto it = geometry.find(key);
        if (it == geometry.end())
            return nullptr;
        front = { key, &it->second, geometryGeneration };
        return &it->second;
    }
    static std::unordered_map<const PlatTexture*, Texture> textures;
    // Direct-mapped hints in front of the maps; every erase bumps the generation.
    struct TextureFront
    {
        const PlatTexture* key = nullptr;
        Texture* entry = nullptr;
        uint32_t generation = 0;
    };
    static std::array<TextureFront, 256> textureFront;
    static uint32_t textureGeneration;
    static void forget_textures() noexcept { ++textureGeneration; }
    // Frontend-only supersets: removals may leave gaps, checked by exact ranges.
    static RangeBounds geometryBounds, mutableTextureBounds;
    static std::mutex writeMutex;
    static std::array<Range, 4096> writes;
    static unsigned writeCount;
    static bool invalidateAll;
    static std::array<float, 4> viewFog, viewFogColor;
    static render::FogRangeState viewFogRange;
    static bool fogRangeFrameFailed;
    static std::array<float, 4> copyClearColor;
    static render::SceneAlphaState alphaState;
    static Renderer::FrameTiming timings;
    static render::PacketProfile packetProfile;
    static uint64_t timingFrames;
    static uint64_t draws, textured, hud, characters, invalid, decoded, records, resident, dynamic;
    static const char* failure;
    static std::array<const glModelPacket*, 64> ballPackets;
    static unsigned ballPacketCount, ballPacketFrame;

    static bool overlaps(Range a, Range b);
    static bool reject_fog_range(const char* reason) noexcept;
    static const void* userdata(const glModelPacket& p, eGLUserData type);
    static std::array<float, 4> vector4(const nlVector4& v);
    static void transform_point(std::array<float, 4>& out, const float* xyz, const nlMatrix4& m,
                                bool direction);
    static void consume_writes();
    static uint32_t texture_for(const PlatTexture* texture, int* shadowPartition = nullptr);
    static bool prepare_packet_geometry(const glModelPacket& p, PacketGeometry& data);
    static bool prepare_skin(const glModelPacket& p, bool hasPn, SceneMaterial& m,
                             std::span<const GLSkinUserData>& skin);
    static void observe_material_state(unsigned long flags, const glModelPacket& p);
    static bool prepare_material(eGLView view, const glModelPacket& p, PreparedMaterial& material,
                                 render::PacketProfile::Scope* profile = nullptr);
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
