#pragma once
// Strikers graphics contracts. All device rendering is implemented with VitaGL.
#include <array>
#include <memory>
#include <span>
#include <vector>
#include "port/native_texture_record.hpp"
#include "port/native_model_vertices.hpp"
#include "port/scene_skinning.hpp"
#include "port/frame_timeline.hpp"
#include "port/native_fog_range.hpp"
namespace port::render
{
using nativegxm::ModelVertex;
using nativegxm::TextureStorage;
using nativegxm::TextureUpload;
enum class TargetFormat : uint8_t { Rgba8, Rgb565 };
struct Raster
{
    bool depthTest = true, depthWrite = true;
    uint8_t depthFunction = 1;   // always, less/equal, equal, less
    uint8_t cull = 0;            // none, front, back, all
    uint8_t blend = 0;           // Strikers blend modes 0..7
    bool writeRgb = true, writeAlpha = false;
    uint8_t primitive = 0;       // triangle list, line list
    constexpr bool valid() const noexcept
    {
        return depthFunction < 4 && cull < 4 && blend < 8 && primitive < 2;
    }
};

struct SceneMaterial
{
    std::array<uint32_t, 6> textures{};
    std::array<uint8_t, 6> wrapU{}, wrapV{}, filter{ 1, 1, 1, 1, 1, 1 };
    std::array<float, 4> tint{ 1, 1, 1, 1 }, ambient{ 1, 1, 1, 1 }, diffuse{ 1, 1, 1, 1 };
    std::array<float, 4> controls{
        0, -1, 1, 0
    };   // texconfig, alpha-test reference (-1 disables), RGB scale, program family
    std::array<float, 4> effects{
        0, -1, .5f, 0
    };   // constant RGB, alpha override, detail blend, crowd V offset
    std::array<float, 4> shadow{ 1, 1, 1, 1 }, shadowAdd{};
    std::array<float, 4> fog{};   // start, end, enabled, type
    std::array<float, 4> fogColor{};
    // Three vec4 table rows and one control, owned by each deferred material.
    // Defaults must remain finite and constexpr for PreparedMaterial::initialScene.
    std::array<float, 12> fogRangeTable = kFogRangeDefaultTable;
    std::array<float, 4> fogRangeControl{}; // effective enable, 2/rounded width, global center, start/range
    std::array<std::array<float, 4>, 4> specular{}, specularColor{};
    std::array<float, 4> material{ 1, 0, 0, 0 }; // gloss weight, dirt program, shadow partition+1, stage-1 RGB cleanup
    std::array<float, 4> alpha{ 1, 1, 1, 0 }; // resolved K-alpha stages 0..2, inherited stage-2 pattern
    std::array<std::array<float, 4>, 8> lightPosition{};
    std::array<std::array<float, 4>, 8> lightColor{};
    SceneBonePalette bones{};
    std::array<float, 16> modelView{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    std::array<float, 4> viewport{ 0, 0, 960, 544 }, scissor{ 0, 0, 960, 544 };
    std::array<float, 4>
        environment{};   // environment UV, mobile UV, Fresnel exponent, animation phase
    bool lit = false;
};
struct TextureCopy
{
    uint32_t destination = 0;
    std::array<float, 4> sourceRect{};
    uint32_t destinationHeight = 0;
    bool intensity = false;
    bool alphaOnly = false; // GX_CTF_A8 copies coverage, not framebuffer luminance.
    // Optional GL texture coordinates in pixels: x/y, width/height. Regional
    // copies overwrite coverage and retain the other cached atlas partitions.
    std::array<uint32_t,4> destinationRect{};
    bool preserveDestination = false;
    bool clearSource = false;
    std::array<float, 4> clearColor{};
};

struct LoadingIndicator
{
    // Original 32x32 YUV422 operations, normal then selected. R is Y, G/B
    // shared U/V, and A=85*(writeY+2*writeUV); alpha is an operation mask.
    std::array<TextureUpload, 2> images;
    std::array<std::array<float, 4>, 3> rectangles{};
    uint32_t waitRetraces = 0, phaseRetraces = 19;
    bool blackBackground = false;
};

class Renderer
{
  public:
    struct UniformTiming
    {
        // Per-frame owner deltas: pack calls/input bytes, reservations/bytes,
        // cache copies/bytes, and unchanged values/input bytes, for VS then FS.
        uint64_t stage[2][8]{};
        uint64_t fallbackUploads = 0, errors = 0, cacheBytes = 0, cachePeakBytes = 0;
        bool valid = false, enabled = false;
    };
    struct VertexProgramTiming
    {
        // Per-frame deltas of the provider's eleven quantity counters. These
        // are requests/SDK calls, not GPU draw counts or elapsed time.
        uint64_t counters[11]{};
        uint64_t cacheBytes = 0, cachePeakBytes = 0;
        bool valid = false, enabled = false;
    };
    struct VertexStreamTiming
    {
        uint64_t counters[6]{};
        bool valid = false, enabled = false;
    };
    struct FrameTiming
    {
        uint64_t fenceUs = 0, uploadUs = 0, drawUs = 0, queueUs = 0, dynamicVertexBytes = 0;
        uint64_t frontendWaitUs = 0, commandBytes = 0;
        uint64_t commandCopyNs = 0, commandVertexBytes = 0, commandIndexBytes = 0;
        uint32_t draws = 0, globalFinishes = 0, slotWaits = 0;
        uint32_t affineUvDraws = 0, projectiveUvDraws = 0, vertexFogDraws = 0;
        UniformTiming uniforms{};
        VertexProgramTiming vertexPrograms{};
        VertexStreamTiming vertexStreams{};
    };
    Renderer() noexcept;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool initialize(bool timings = false) noexcept;
    bool initialize_scene_program() noexcept;
    uint32_t create_texture(const TextureUpload&) noexcept;
    uint32_t create_target(uint32_t width, uint32_t height,
                           TargetFormat format = TargetFormat::Rgba8) noexcept;
    void release_texture(uint32_t texture) noexcept;
    uint32_t create_model(std::span<const ModelVertex>, std::span<const uint16_t>) noexcept;
    void release_model(uint32_t model) noexcept;
    bool draw_scene(uint32_t model, std::span<const ModelVertex>, std::span<const uint16_t>,
                    const std::array<float, 16>&, const Raster&, const SceneMaterial&) noexcept;
    bool select_target(uint32_t texture = 0, bool color = false, bool depth = false) noexcept;
    bool copy_texture(uint32_t destination, std::array<float, 4> source,
                      bool intensity = false) noexcept;
    bool copy_textures(std::span<const TextureCopy>) noexcept;
    bool clear_color(std::array<float, 4>, bool depth = true) noexcept;
    bool clear_depth() noexcept;
    // Stage VI-style blanking; the third subsequent successful frame is visible.
    // Setting the flag alone never swaps or changes any render-target pixels.
    bool set_black(bool) noexcept;
    // Publish a complete loading bitmap independently of an open engine frame.
    // The destination is top-left x/y/width/height in physical display pixels.
    bool present_bitmap(const TextureUpload&, std::array<float, 4> destination) noexcept;
    bool set_loading(bool begin, const LoadingIndicator&) noexcept;
    // Owner-only timer entry: advances by the actual display retrace count.
    bool loading_tick() noexcept;
    bool loading_active() const noexcept;
    bool frame_active() const noexcept;
    bool begin_frame(FrameTimeline::Owner* capture = nullptr) noexcept;
    // Complete an engine pass without swapping an image it invalidated while
    // resources were loading. The display retains the last valid image.
    bool end_frame(bool present = true) noexcept;
    bool readback_presented_rgba8(std::vector<uint8_t>&) noexcept;
    void shutdown() noexcept;
    const char* last_error() const noexcept;
    int error_code() const noexcept;
    FrameTiming completed_timing() const noexcept;

  private:
    uint32_t create_target_internal(uint32_t, uint32_t, TargetFormat, bool depth) noexcept;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}   // namespace port::render
