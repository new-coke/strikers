#pragma once
#include "port/benchmark.h"
#include "port/renderer.hpp"
#include <array>
#include <string>
#include <optional>
#include <vector>

namespace port::render
{
// Diagnostic producer wall time. Phases partition a packet callback; waits
// and renderer-owner time are subsets/overlapping work, never extra phases.
class PacketProfile
{
  public:
    enum Phase { Invalidation, Arrays, Skin, Material, Geometry, Submit, Cleanup, PhaseCount };
    enum Counter
    {
        Attempts, Accepted, Rejected, ResidentHits, ResidentBuilds, DynamicDraws,
        InputReferences, DecodedVertices, DecodedIndices, VertexBytes, IndexBytes,
        PacketWaitNs, CounterCount
    };
    static constexpr unsigned Views = 35; // GLV 0..33, then invalid/unknown.
    struct Stats
    {
        std::array<uint64_t, PhaseCount> ns{};
        std::array<uint64_t, CounterCount> counts{};
        void add(const Stats&) noexcept;
    };
    enum NetCounter
    {
        NetCensus, NetEligible, NetFast, NetFallback, NetAccepted, NetRejected,
        NetReferences, NetVertices, NetIndices, NetGeometryNs, NetCounterCount
    };
    struct NetFrame
    {
        bool valid = false, enabled = false;
        uint64_t tagOverflow = 0, tagMismatch = 0;
        std::array<uint64_t, 2> registered{};
        std::array<std::array<uint64_t, NetCounterCount>, 2> sources{};
    };
    enum MaterialPart { ViewportsMatrices, StateLighting, TextureResolution, MaterialPartCount };
    enum MaterialCounter
    {
        MaterialAttempts, ViewportOverrides, ViewportMatrixOverrides, ModelConcats, ViewOnlyMatrices,
        InvalidMatrices, PointLights, DirectionalLights, SpecularLights,
        TextureRequests, TextureResolutions, TextureFailures, MaterialCounterCount
    };
    struct MaterialFrame
    {
        std::array<uint64_t, MaterialPartCount> ns{};
        std::array<uint64_t, MaterialCounterCount> counts{};
    };
    struct Frame
    {
        size_t sample = 0;
        unsigned long liveFrame = 0;
        uint32_t engineFrame = 0;
        bool discarded = false;
        Stats producer{};
        Renderer::FrameTiming renderer{};
        NetFrame nets{};
        FrameTimeline timeline{};
        MaterialFrame materials{};
    };
    class Scope
    {
      public:
        using Wait = uint64_t (*)();
        Scope(PacketProfile&, unsigned view, uint32_t references, Wait) noexcept;
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        void next(Phase) noexcept;
        // Only the two internal boundaries read an additional clock. Leaving
        // Material closes its last child with next()'s existing timestamp.
        void material_next(MaterialPart) noexcept;
        void material_count(MaterialCounter, uint64_t count = 1) noexcept;
        void submitted(uint32_t model, size_t vertices, size_t indices) noexcept;
        void net_source(unsigned source) noexcept;
        void net_route(bool eligible, bool fast, bool fallback) noexcept;
      private:
        PacketProfile* owner = nullptr;
        Wait wait = nullptr;
        uint64_t previous = 0, initialWait = 0;
        unsigned view = 0;
        Phase phase = Invalidation;
        std::optional<Stats> stats;
        unsigned netSource;
        bool netEligible, netFast, netFallback;
        uint64_t materialPrevious;
        MaterialPart materialPart;
    };
    bool configure(const char* capturePath, size_t requested) noexcept;
    bool configure_nets(bool conversionEnabled) noexcept;
    void net_registered(unsigned source) noexcept;
    void net_tag_failure(bool overflow) noexcept;
    bool enabled() const noexcept { return goal != 0; }
    bool recording() const noexcept { return armed; }
    FrameTimeline* capture_timeline() noexcept { return armed ? &currentTimeline : nullptr; }
    void begin_frame(const PortBenchCaptureStatus&) noexcept;
    void end_frame(const PortBenchCaptureStatus&, uint32_t engineFrame, bool discarded,
                   const Renderer::FrameTiming&) noexcept;
    bool write() noexcept;
    int output_error() const noexcept { return error; }
    const std::vector<Frame>& samples() const noexcept { return frames; }
    const auto& views() const noexcept { return totals; }
  private:
    std::string path;
    size_t goal = 0, before = 0;
    bool armed = false, finished = false;
    int error = 0;
    Stats current{};
    std::array<Stats, Views> currentViews{}, totals{};
    std::vector<Frame> frames;
    bool netCensus = false, netConversion = false;
    NetFrame currentNets{};
    FrameTimeline currentTimeline{};
    MaterialFrame currentMaterials{};
};
} // namespace port::render
