#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"
#include "NL/gl/glMatrix.h"
#include "NL/nlTask.h"
#include "port/config.h"
#include "port/host.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern u8 glx_bLoadingIndicator;

namespace port::directmatch::detail
{
void MatchRuntime::trace_present(bool presented)
{
    if (!PortDiagnosticsEnabled())
        return;
    static const bool enabled = [] {
        const auto* value = std::getenv("STRIKERS_VITAGL_TRANSITION_TRACE");
        return value && value[0] == '1';
    }();
    if (!enabled)
        return;
    static FILE* file = std::fopen("ux0:data/strikersVita/native_transition_trace.csv", "w");
    static bool header = false;
    static unsigned frames = 0;
    if (!file)
        return;
    if (++frames > 2400)
    {
        std::fclose(file);
        file = nullptr;
        return;
    }
    if (!header)
    {
        std::fprintf(file, "frame,start_ns,end_ns,state,loading,discarded,presented,draws\n");
        header = true;
    }
    std::fprintf(file, "%u,%llu,%llu,%u,%u,%u,%u,%llu\n", unsigned(glGetCurrentFrame()),
                 frameStartNs, port_monotonic_ns(),
                 unsigned(nlTaskManager::m_pInstance->m_CurrState), unsigned(glx_bLoadingIndicator),
                 unsigned(frameDiscarded), unsigned(presented), draws - frameDrawStart);
    std::fflush(file);
}

void MatchRuntime::report()
{
    auto* f = std::fopen("ux0:data/strikersVita/vitagl_match_status.csv", "wb");
    if (!f)
        return;
    std::fprintf(f,
                 "# vitagl_match_v9 assets=native_graphics_v1 backend=VitaGL deferred=command_ring "
                 "aurora=0 gx_fifo=0 texture_decoder=owned skinning=gpu "
                 "views=34\nframe,draws,textured,hud,characters,invalid,native_records,native_"
                 "decoded,resident,dynamic,gpu_error,operation,failure\n%u,%llu,%llu,%llu,%llu,%"
                 "llu,%llu,%llu,%llu,%llu,%d,%s,%s\n",
                 unsigned(glGetCurrentFrame()), draws, textured, hud, characters, invalid, records,
                 decoded, resident, dynamic, gpu.error_code(), gpu.last_error(), failure);
    std::fclose(f);
    if (timingFrames)
    {
        f = std::fopen("ux0:data/strikersVita/native_renderer_timing.csv", "w");
        if (f)
        {
            std::fprintf(
                f,
                "frames,fence_us,upload_us,draw_calls_us,queue_us,draws,dynamic_bytes,frontend_"
                "wait_us,command_bytes,slot_waits\n%llu,%llu,%llu,%llu,%llu,%u,%llu,%llu,%llu,%u\n",
                timingFrames, timings.fenceUs, timings.uploadUs, timings.drawUs, timings.queueUs,
                timings.draws, timings.dynamicVertexBytes, timings.frontendWaitUs,
                timings.commandBytes, timings.slotWaits);
            std::fclose(f);
        }
    }
}

void MatchRuntime::note_ball_packets(const glModel* model) noexcept
{
    if (!model || !PortDiagnosticsEnabled())
        return;
    const unsigned frame = glGetCurrentFrame();
    if (frame != ballPacketFrame)
    {
        ballPacketCount = 0;
        ballPacketFrame = frame;
    }
    for (unsigned i = 0; i < model->numPackets && ballPacketCount < ballPackets.size(); ++i)
        ballPackets[ballPacketCount++] = model->packets + i;
}

MatchRuntime::PacketTrace::~PacketTrace()
{
    if (!PortDiagnosticsEnabled() || invalid == initial)
        return;
    static unsigned recorded = 0;
    if (recorded++ >= 32)
        return;
    auto* f =
        std::fopen("ux0:data/strikersVita/native_rejected_packets.log", recorded == 1 ? "w" : "a");
    if (!f)
        return;
    std::fprintf(f,
                 "frame=%u view=%u reason=%s count=%u primitive=%u program=%08x texconfig=%02x "
                 "raster=%08x gpu=%d\n",
                 unsigned(glGetCurrentFrame()), unsigned(view), failure, packet.numVertices,
                 packet.primType, packet.state.program, packet.state.texconfig, packet.state.raster,
                 gpu.error_code());
    if (packet.streams && packet.numStreams <= 16)
        for (unsigned i = 0; i < packet.numStreams; ++i)
        {
            const auto& a = packet.streams[i];
            std::fprintf(f, "  stream id=%u address=%08lx stride=%u bytes=%u endian=%u\n", a.id,
                         (unsigned long)a.address, a.stride, a.dataSize, a.beData);
        }
    std::fclose(f);
}

void MatchRuntime::trace_invalid_matrix(eGLView view, const glModelPacket& p, const nlMatrix4& v,
                                        const nlMatrix4& projection, const nlMatrix4& mv,
                                        const std::array<float, 16>& matrix, bool matrixOwned)
{
    if (!PortDiagnosticsEnabled())
        return;
    static unsigned logged = 0;
    if (logged++ < 4)
    {
        auto* f = std::fopen("ux0:data/strikersVita/native_invalid_matrices.log",
                             logged == 1 ? "w" : "a");
        if (f)
        {
            std::fprintf(f, "frame=%u view=%u handle=%08lx owned=%u identity=%08lx\n",
                         unsigned(glGetCurrentFrame()), unsigned(view),
                         (unsigned long)p.state.matrix, unsigned(matrixOwned),
                         (unsigned long)glGetIdentityMatrix());
            const float* matrices[]{ v.e, projection.e, mv.e, matrix.data(),
                                     matrixOwned ? reinterpret_cast<const float*>(p.state.matrix)
                                                 : nullptr };
            const char* names[]{ "view", "projection", "model_view", "mvp", "packet_model" };
            for (unsigned i = 0; i < 5; ++i)
                if (matrices[i])
                {
                    std::fprintf(f, "%s", names[i]);
                    for (unsigned k = 0; k < 16; ++k)
                    {
                        uint32_t bits;
                        std::memcpy(&bits, &matrices[i][k], 4);
                        std::fprintf(f, " %08x", bits);
                    }
                    std::fputc('\n', f);
                }
            std::fclose(f);
        }
    }
}

void MatchRuntime::trace_ball_packet(eGLView view, const glModelPacket& p, const glModelPacket* ptr,
                                     uint32_t modelId, const PacketGeometry& data,
                                     const PreparedMaterial& material)
{
    const auto& streams = data.streams;
    const auto original = data.indices;
    const auto& m = material.scene;
    const auto& raster = material.raster;
    const auto& matrix = material.matrix;
    const bool matrixOwned = material.matrixOwned;
    if (PortDiagnosticsEnabled() && ballPacketFrame == glGetCurrentFrame() &&
        glGetCurrentFrame() % 60 == 0 &&
        std::find(ballPackets.begin(), ballPackets.begin() + ballPacketCount, ptr) !=
            ballPackets.begin() + ballPacketCount)
    {
        static bool first = true;
        auto* f = std::fopen("ux0:data/strikersVita/native_ball_packets.log", first ? "w" : "a");
        first = false;
        if (f)
        {
            std::array<float, 4> low{ 1e30f, 1e30f, 1e30f, 1e30f },
                high{ -1e30f, -1e30f, -1e30f, -1e30f };
            for (unsigned i = 0; i < p.numVertices; ++i)
            {
                const uint32_t index = original.empty() ? i : original[i];
                const uint8_t* a = nullptr;
                if (!model_array_at(streams.position, index, streams.position.stride, a))
                    continue;
                float pos[4]{ 0, 0, 0, 1 };
                for (unsigned k = 0; k < 3; ++k)
                    pos[k] =
                        streams.position.stride == 6
                            ? model_short(a + 2 * k, streams.position.littleEndian, 1.f / 256.f)
                            : model_float(a + 4 * k, streams.position.littleEndian);
                float c[4]{};
                for (unsigned r = 0; r < 4; ++r)
                    for (unsigned k = 0; k < 4; ++k)
                        c[r] += matrix[k * 4 + r] * pos[k];
                for (unsigned k = 0; k < 3; ++k)
                    c[k] /= c[3];
                for (unsigned k = 0; k < 4; ++k)
                {
                    low[k] = std::min(low[k], c[k]);
                    high[k] = std::max(high[k], c[k]);
                }
            }
            std::fprintf(f,
                         "frame=%u view=%u count=%u resident=%u owned=%u matrix=%08lx "
                         "modelpos=%.3f,%.3f,%.3f cull=%u depth=%u,%u,%u blend=%u texmask=%u "
                         "test=%.3f alpha=%.3f env=%.3f,%.3f,%.3f "
                         "clip=%.3f,%.3f,%.3f..%.3f,%.3f,%.3f w=%.3f..%.3f\n",
                         unsigned(glGetCurrentFrame()), unsigned(view), p.numVertices, modelId,
                         unsigned(matrixOwned), (unsigned long)p.state.matrix,
                         matrixOwned ? reinterpret_cast<const float*>(p.state.matrix)[12] : 0,
                         matrixOwned ? reinterpret_cast<const float*>(p.state.matrix)[13] : 0,
                         matrixOwned ? reinterpret_cast<const float*>(p.state.matrix)[14] : 0,
                         raster.cull, raster.depthTest, raster.depthWrite, raster.depthFunction,
                         raster.blend, p.state.texconfig, m.controls[1], m.effects[1],
                         m.environment[0], m.environment[1], m.environment[2], low[0], low[1],
                         low[2], high[0], high[1], high[2], low[3], high[3]);
            std::fclose(f);
        }
    }
}
}   // namespace port::directmatch::detail
#endif
