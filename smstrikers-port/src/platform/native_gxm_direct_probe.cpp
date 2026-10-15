#include "port/native_gxm_direct_probe.hpp"

#if defined(PORT_VITA) && defined(STRIKERS_VITA_GXM_DIRECT_PROBE)
#include "port/native_gxm_direct_data.hpp"
#include "port/native_gxm_direct_scene.hpp"
#include "gxm/gxm_renderer.hpp"
#include "vita_log.hpp"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace port {
namespace {

namespace gfx = aurora::vita::gfx;
namespace gxm = aurora::vita::gxm;

constexpr uint32_t kWarmupFrames = 60;
constexpr uint32_t kMeasuredFrames = 300;
constexpr uint32_t kFrameCount = kWarmupFrames + kMeasuredFrames;
constexpr const char* kCapturePath = "ux0:data/strikersVita/gxm_direct_probe.csv";
constexpr const char* kSceneCapturePath = "ux0:data/strikersVita/gxm_direct_static_scene.csv";

// One typed native model, prepared once and then submitted without any GX
// entry point, display-list decoding, state translation or fallback path.
class NativeMesh {
public:
    bool upload(gxm::Renderer& renderer, const directgxm::MeshView& source) noexcept {
        if (directgxm::admit(source) != directgxm::Admission::Accepted)
            return false;
        renderer_ = &renderer;
        pipeline_ = renderer.create_pipeline(*source.pipeline);
        if (!pipeline_)
            return false;
        vertexBuffer_ = renderer.create_buffer(source.packedVertices, source.vertexBytes);
        if (!vertexBuffer_)
            return false;
        indexBuffer_ = renderer.create_buffer(source.indices, source.indexBytes);
        if (!indexBuffer_)
            return false;

        packet_ = {};
        packet_.pipelineKey = pipeline_;
        packet_.vertices = {vertexBuffer_, 0, static_cast<uint32_t>(source.vertexBytes)};
        packet_.indices = {indexBuffer_, 0, static_cast<uint32_t>(source.indexBytes)};
        packet_.vertexCount = source.vertexCount;
        packet_.indexCount = source.indexCount;
        packet_.instanceCount = 1;
        packet_.viewport = {0.f, 0.f, 960.f, 544.f, 0.f, 1.f};
        packet_.scissor = {0, 0, 960, 544};
        packet_.uniforms = gfx::GpuDrawUniforms{};
        packet_.uniformRevision = 1;
        packet_.vertexUniformRevision = 1;
        packet_.fragmentUniformRevision = 1;
        packet_.textureBindingRevision = 1;
        ready_ = true;
        return true;
    }

    bool draw() noexcept {
        return ready_ && renderer_->draw(packet_, true);
    }

    void release() noexcept {
        if (!renderer_)
            return;
        // Called after the final frame: keep GPU users retired before release.
        (void)renderer_->finish();
        if (indexBuffer_)
            renderer_->destroy_buffer(indexBuffer_);
        if (vertexBuffer_)
            renderer_->destroy_buffer(vertexBuffer_);
        if (pipeline_)
            renderer_->destroy_pipeline(pipeline_);
        ready_ = false;
        renderer_ = nullptr;
    }

private:
    gxm::Renderer* renderer_ = nullptr;
    gfx::DrawPacket packet_{};
    gfx::Handle vertexBuffer_ = 0;
    gfx::Handle indexBuffer_ = 0;
    uint64_t pipeline_ = 0;
    bool ready_ = false;
};

bool save_capture(const std::array<uint32_t, kMeasuredFrames>& timings,
                  uint32_t recorded, bool complete, const char* failure,
                  unsigned mode, unsigned nativeDraws, unsigned meshes) noexcept {
    FILE* f = std::fopen(mode == 2 ? kSceneCapturePath : kCapturePath, "wb");
    if (!f)
        return false;
    bool ok = std::fprintf(f, "# strikers-gxm-direct-probe-v2 gx_calls=0 fallback=0 status=%s test=%s\n",
                           complete ? "completed" : "incomplete",
                           mode == 2 ? "gpu_static_asset_scene_NOT_live_match" : "single_mesh_fixture") >= 0;
    ok = std::fprintf(f, "# failure=%s warmup=%u expected_frames=%u measured_frames=%u\n",
                      failure ? failure : "none", kWarmupFrames, kMeasuredFrames, recorded) >= 0 && ok;
    ok = std::fprintf(f, "# scene_meshes=%u draws_per_frame=%u no_gx_translation=1\n",
                      meshes, nativeDraws) >= 0 && ok;
    ok = std::fprintf(f, "# actual_cpu_mhz=%d actual_gpu_mhz=%d actual_bus_mhz=%d actual_xbar_mhz=%d\n",
                      scePowerGetArmClockFrequency(), scePowerGetGpuClockFrequency(),
                      scePowerGetBusClockFrequency(), scePowerGetGpuXbarClockFrequency()) >= 0 && ok;
    ok = std::fprintf(f, "# timer=sceKernelGetProcessTimeWide_delta_us gpu_execution_not_measured\n") >= 0 && ok;
    ok = std::fprintf(f, "sample,frame_us,native_draws\n") >= 0 && ok;
    for (uint32_t i = 0; i < recorded; ++i)
        ok = std::fprintf(f, "%u,%u,%u\n", i, timings[i], nativeDraws) >= 0 && ok;
    ok = std::ferror(f) == 0 && ok;
    return std::fclose(f) == 0 && ok;
}

} // namespace

int run_native_gxm_direct_probe(unsigned mode) noexcept {
    // Match the known-safe hardware clocks but do not start game/asset/audio/GX.
    if (mode != 1 && mode != 2)
        return 1;
    aurora::vita::set_runtime_diagnostics_enabled(false);
    (void)scePowerSetArmClockFrequency(444);
    (void)scePowerSetGpuClockFrequency(222);
    (void)scePowerSetBusClockFrequency(222);
    (void)scePowerSetGpuXbarClockFrequency(166);
    (void)sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    gxm::Renderer renderer;
    gxm::Config config{};
    config.width = 960;
    config.height = 544;
    config.scenesPerFrame = 5;
    config.waitVblank = true;

    std::array<uint32_t, kMeasuredFrames> timings{};
    uint32_t recorded = 0;
    bool complete = false;
    const char* failure = nullptr;
    unsigned requestedDraws = 1;
    if (mode == 2) {
        requestedDraws = 64;
        if (const char* value = std::getenv("STRIKERS_GXM_DIRECT_SCENE_DRAWS")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(value, &end, 10);
            if (value != end && end && *end == '\0' && parsed >= 1 && parsed <= 256)
                requestedDraws = static_cast<unsigned>(parsed);
        }
    }
    if (!renderer.initialize(config)) {
        failure = renderer.last_error();
        (void)save_capture(timings, recorded, complete, failure, mode, requestedDraws, 0);
        return 1;
    }

    NativeMesh mesh;
    directgxm::NativeStaticScene nativeScene;
    const gfx::PipelineDesc pipeline = directgxm::color_pipeline();
    const bool uploaded = mode == 1
        ? mesh.upload(renderer, directgxm::probe_mesh(pipeline))
        : nativeScene.initialize(renderer, requestedDraws);
    if (!uploaded) {
        const char* driverError = renderer.last_error();
        failure = mode == 2 ? nativeScene.error() :
            driverError && *driverError ? driverError : "native_mesh_admission_failed";
    } else {
        for (uint32_t frame = 0; frame < kFrameCount; ++frame) {
            SceCtrlData pad{};
            if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0 &&
                (pad.buttons & (SCE_CTRL_START | SCE_CTRL_SELECT)) ==
                    (SCE_CTRL_START | SCE_CTRL_SELECT)) {
                failure = "user_abort";
                break;
            }
            const uint64_t started = sceKernelGetProcessTimeWide();
            if (!renderer.begin_frame() ||
                !renderer.clear({0.015f, 0.025f, 0.055f, 1.f}, 1.f) ||
                !(mode == 1 ? mesh.draw() : nativeScene.submit(renderer)) ||
                !renderer.end_frame(true)) {
                const char* driverError = renderer.last_error();
                failure = driverError && *driverError ? driverError : "gxm_frame_submission_failed";
                break;
            }
            const uint64_t elapsed = sceKernelGetProcessTimeWide() - started;
            if (frame >= kWarmupFrames)
                timings[recorded++] = static_cast<uint32_t>(elapsed > UINT32_MAX ? UINT32_MAX : elapsed);
        }
        complete = recorded == kMeasuredFrames;
    }

    if (!complete && failure == nullptr)
        failure = "incomplete_capture";
    const bool saved = save_capture(timings, recorded, complete, failure, mode,
                                    mode == 1 ? 1 : nativeScene.draw_count(),
                                    mode == 1 ? 1 : nativeScene.mesh_count());
    mesh.release();
    nativeScene.shutdown(renderer);
    renderer.shutdown();
    return complete && saved ? 0 : 1;
}

} // namespace port
#endif
