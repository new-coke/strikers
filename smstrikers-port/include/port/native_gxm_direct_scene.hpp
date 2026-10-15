#pragma once

#if defined(PORT_VITA) && defined(STRIKERS_VITA_GXM_DIRECT_PROBE)
#include "gxm/gxm_renderer.hpp"
#include "port/native_gxm_scene_pack.hpp"
#include <cstdint>
#include <vector>

namespace port::directgxm {

// Explicitly bounded offline static-geometry workload. Does not initialize,
// observe or fall back to GX, and does not claim to run Strikers' match logic.
class NativeStaticScene {
public:
    bool initialize(aurora::vita::gxm::Renderer& renderer, unsigned draws) noexcept;
    bool submit(aurora::vita::gxm::Renderer& renderer) noexcept;
    void shutdown(aurora::vita::gxm::Renderer& renderer) noexcept;
    const char* error() const noexcept { return failure_; }
    unsigned draw_count() const noexcept { return static_cast<unsigned>(packets_.size()); }
    unsigned mesh_count() const noexcept { return static_cast<unsigned>(resident_.size()); }
private:
    struct ResidentMesh {
        aurora::vita::gfx::Handle vertices = 0;
        aurora::vita::gfx::Handle indices = 0;
        uint32_t vertexCount = 0, indexCount = 0;
        uint32_t vertexBytes = 0, indexBytes = 0;
        std::array<float,3> boundsMin{}, boundsMax{};
    };
    uint64_t pipeline_ = 0;
    std::vector<ResidentMesh> resident_;
    std::vector<aurora::vita::gfx::DrawPacket> packets_;
    const char* failure_ = nullptr;
};

} // namespace port::directgxm
#endif
