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
    uint32_t create_target(uint32_t width, uint32_t height) noexcept;
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
    bool frame_active() const noexcept;
    bool begin_frame() noexcept;
    bool end_frame(bool present = true) noexcept;
    bool readback_presented_rgba8(std::vector<uint8_t>&) noexcept;
    void shutdown() noexcept;
    const char* last_error() const noexcept;
    int error_code() const noexcept;
    FrameTiming completed_timing() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}   // namespace port::render
