#pragma once

// Strikers-owned GPU interface. No Aurora/GX types or runtime state enter it.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <memory>
#include <span>
#include <vector>
#include "port/native_texture_record.hpp"
#include "port/native_model_vertices.hpp"

namespace port::nativegxm {

inline constexpr uint32_t kVertexStride = 20; // float4 position + RGBA8

inline bool finite_mvp(const std::array<float,16>& matrix) noexcept {
    for (float x : matrix) if (!std::isfinite(x)) return false;
    return true;
}

struct Mesh {
    uint32_t vertexOffset = 0, vertexBytes = 0;
    uint32_t indexOffset = 0, indexCount = 0, vertexCount = 0;
};

struct Raster {
    bool depthTest = true, depthWrite = true;
    uint8_t depthFunction = 1; // always, less/equal, equal, less
    uint8_t cull = 0;          // none, front, back, all
    uint8_t blend = 0;         // Strikers blend modes 0..7
    bool writeRgb = true, writeAlpha = false;
    constexpr bool valid() const noexcept {
        return depthFunction < 4 && cull < 4 && blend < 8;
    }
    constexpr unsigned fragment_slot() const noexcept {
        return blend * 4u + unsigned(writeRgb) + 2u * unsigned(writeAlpha);
    }
};

struct Material {
    uint32_t texture=0;
    std::array<float,4> tint{1,1,1,1};
    uint8_t wrapU=0,wrapV=0,filter=1,mipFilter=2; // repeat/clamp, point/linear
    uint8_t alphaRef=0,flags=0; // alpha test, replace RGB, RGB x2, replace alpha
    bool valid() const noexcept {
        return texture && wrapU<2 && wrapV<2 && filter<2 && mipFilter<3 && flags<16 &&
            std::isfinite(tint[0]) && std::isfinite(tint[1]) &&
            std::isfinite(tint[2]) && std::isfinite(tint[3]);
    }
};

struct SceneMaterial {
    std::array<uint32_t,6> textures{};
    std::array<uint8_t,6> wrapU{},wrapV{},filter{1,1,1,1,1,1};
    std::array<float,4> tint{1,1,1,1},ambient{1,1,1,1},diffuse{1,1,1,1};
    std::array<float,4> controls{0,-1,1,0}; // texconfig, alpha-test reference (-1 disables), RGB scale, program family
    std::array<float,4> effects{0,-1,.5f,0}; // constant RGB, alpha override, detail blend, crowd V offset
    std::array<float,4> shadow{1,1,1,1},shadowAdd{};
    std::array<float,4> fog{}; // start, end, enabled, type
    std::array<float,4> fogColor{};
    std::array<float,4> specular{0,0,1,16},specularColor{};
    std::array<std::array<float,4>,8> lightPosition{};
    std::array<std::array<float,4>,8> lightColor{};
    std::array<std::array<float,4>,10*3> bones{};
    std::array<float,16> modelView{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,4> viewport{0,0,960,544},scissor{0,0,960,544};
    std::array<float,4> environment{}; // environment UV, mobile UV, Fresnel exponent, animation phase
    bool lit=false;
};
struct TextureCopy {
    uint32_t destination=0;
    std::array<float,4> sourceRect{};
    uint32_t destinationHeight=0;
    bool intensity=false;
};

// Bounds are checked at upload; draw takes only a resident mesh ID. The
// caller must also validate all local U16 indices before publishing a catalog.
inline bool valid_mesh(const Mesh& m, size_t vertexBytes, size_t indexBytes) noexcept {
    return m.vertexCount && m.vertexCount <= 65536 && m.indexCount &&
           m.indexCount % 3u == 0 && m.vertexOffset % 4u == 0 &&
           m.indexOffset % 2u == 0 &&
           uint64_t(m.vertexCount) * kVertexStride == m.vertexBytes &&
           m.vertexOffset <= vertexBytes && m.vertexBytes <= vertexBytes - m.vertexOffset &&
           m.indexOffset <= indexBytes &&
           uint64_t(m.indexCount) * 2u <= indexBytes - m.indexOffset;
}

// Commit the buffer rotation only after DisplayQueueAddEntry succeeds. This
// prevents a failed present from recycling a surface still owned by the GPU.
class FrameSlots {
public:
    bool begin() noexcept { if (active_) return false; active_ = true; return true; }
    bool present() noexcept {
        if (!active_) return false;
        active_ = false;
        front_ = back_;
        back_ = (back_ + 1u) % 3u;
        return true;
    }
    void reset() noexcept { active_ = false; front_ = 0; back_ = 1; }
    bool active() const noexcept { return active_; }
    unsigned front() const noexcept { return front_; }
    unsigned back() const noexcept { return back_; }
private:
    bool active_ = false;
    unsigned front_ = 0, back_ = 1;
};

// All methods run on one GPU owner thread. Uploads are immutable until
// shutdown, which fences GPU/display users before unmapping their storage.
class Renderer {
public:
    struct FrameTiming {
        uint64_t fenceUs=0, uploadUs=0, drawUs=0, queueUs=0;
        uint64_t dynamicVertexBytes=0;
        uint32_t draws=0, globalFinishes=0, slotWaits=0;
    };
    Renderer() noexcept;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    // Experimental slot retirement is opt-in. Timings are diagnostic-only;
    // both choices are fixed before the renderer starts submitting scenes.
    bool initialize(bool slotFence=false, bool timings=false) noexcept;
    bool upload(const void* vertices, size_t vertexBytes,
                const uint16_t* indices, size_t indexBytes,
                std::span<const Mesh> meshes) noexcept;
    bool upload_uvs(std::span<const uint8_t> vertices,std::span<const uint8_t> hasUv) noexcept;
    uint32_t create_texture(const TextureUpload& upload) noexcept;
    uint32_t create_target(uint32_t width,uint32_t height) noexcept;
    bool select_target(uint32_t texture=0,bool clearColor=false,bool clearDepth=false) noexcept;
    bool copy_texture(uint32_t destination,std::array<float,4> sourceRect,bool intensity=false) noexcept;
    bool copy_textures(std::span<const TextureCopy> copies) noexcept;
    bool clear_color(std::array<float,4> color,bool depth=true) noexcept;
    void release_texture(uint32_t texture) noexcept;
    bool initialize_scene_program() noexcept;
    uint32_t create_model(std::span<const ModelVertex> vertices,std::span<const uint16_t> indices) noexcept;
    void release_model(uint32_t model) noexcept;
    bool draw_scene(uint32_t model,std::span<const ModelVertex> vertices,std::span<const uint16_t> indices,
                    const std::array<float,16>& mvp,const Raster& raster,const SceneMaterial& material) noexcept;
    bool clear_depth() noexcept;
    bool finish() noexcept;
    bool frame_active() const noexcept;
    bool begin_frame() noexcept;
    bool draw(uint32_t mesh, const std::array<float, 16>& mvp, const Raster& raster) noexcept;
    bool draw_textured(uint32_t mesh,const std::array<float,16>& mvp,
                       const Raster& raster,const Material& material) noexcept;
    // Copy an owned CPU snapshot into frame-local mapped GPU storage. No
    // producer/frame pointer is retained when this function returns.
    bool draw_dynamic(std::span<const ModelVertex> vertices,
                      std::span<const uint16_t> indices,
                      const std::array<float,16>& mvp,const Raster& raster,
                      const Material* material=nullptr) noexcept;
    bool end_frame() noexcept;
    // Diagnostic only, between frames. Fence GPU/display and verify that the
    // displayed surface is ours before copying its RGBA8 pixels.
    bool readback_presented_rgba8(std::vector<uint8_t>& pixels) noexcept;
    void shutdown() noexcept;
    const char* last_error() const noexcept;
    int error_code() const noexcept;
    FrameTiming completed_timing() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace port::nativegxm
