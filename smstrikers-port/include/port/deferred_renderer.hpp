#pragma once
#include "port/renderer.hpp"
namespace port::render
{
class DeferredRenderer
{
  public:
    using FrameTiming = Renderer::FrameTiming;
    DeferredRenderer() noexcept;
    ~DeferredRenderer();
    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;
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
    bool set_black(bool) noexcept;
    // Borrow the upload only until the owner finishes the display transaction.
    bool present_bitmap(const TextureUpload&, std::array<float, 4> destination) noexcept;
    // Upload and capture complete on the owner before the caller may block.
    bool set_loading(bool begin, const LoadingIndicator&) noexcept;
    bool frame_active() const noexcept;
    // Capture storage is borrowed only through the matching end_frame drain.
    // A null pointer leaves all new timeline clocks and writes inactive.
    // Quiet mode may enqueue complete frames without waiting on the GPU owner.
    // Frame commands are deep-copied into immutable bounded command slots.
    // Diagnostic captures keep the original synchronous completion barrier.
    bool begin_frame(FrameTimeline* capture = nullptr, bool asynchronous = false) noexcept;
    bool end_frame(bool present = true, bool asynchronous = false) noexcept;
    bool readback_presented_rgba8(std::vector<uint8_t>&) noexcept;
    void shutdown() noexcept;
    const char* last_error() const noexcept;
    int error_code() const noexcept;
    FrameTiming completed_timing() const noexcept;
    // Producer-owned counter: diagnostic subset of packet wall time.
    uint64_t pending_frontend_wait_us() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}   // namespace port::render
