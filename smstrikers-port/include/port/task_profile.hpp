#pragma once
#include "port/benchmark.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace port
{
// Finite task attribution shares the benchmark's selected-frame identity.
// Task scopes are subsets of manager wall time; they are not extra frame time.
class TaskProfile
{
  public:
    static constexpr unsigned MaxFrames = 8192, MaxTasks = 32, NameBytes = 96;
    enum Phase { Run, Transition };
    enum Detail : unsigned
    {
        FixedControl, FixedInput, FixedAi, FixedNpc, FixedPrePhysics, FixedPhysics,
        FixedPostPhysics, FixedNet, FixedSideline, FixedSnapshot,
        RenderLighting, RenderPresentation, RenderPreparation, RenderWorld,
        RenderSnapshot, RenderInventory, RenderEffects, DetailCount
    };
    struct Stats
    {
        uint64_t runNs = 0, transitionNs = 0;
        uint32_t runCalls = 0, transitionCalls = 0;
    };
    struct Task
    {
        const void* key = nullptr;
        std::array<char, NameBytes> name{};
    };
    struct DetailStats
    {
        uint64_t elapsedNs = 0;
        uint32_t calls = 0, parentTask = MaxTasks;
    };
    struct Simulation
    {
        uint32_t steps = 0, parentTask = MaxTasks;
        float scaledDtSeconds = 0, debtBeforeSeconds = 0, debtAfterSeconds = 0, tickSeconds = 0;
        bool present = false, lockstep = false;
    };
    struct Frame
    {
        unsigned sample = 0;
        unsigned long liveFrame = 0;
        uint32_t engineFrame = 0;
        uint64_t managerNs = 0;
        std::array<Stats, MaxTasks> tasks{};
        std::array<DetailStats, DetailCount> details{};
        Simulation simulation;
    };

    // One producer cursor partitions a task Run into disjoint phases. Advancing
    // takes one clock shared by the closing/opening boundary; return closes it.
    class PhaseScope
    {
      public:
        PhaseScope(TaskProfile&, const void* parent, Detail first) noexcept;
        ~PhaseScope();
        PhaseScope(const PhaseScope&) = delete;
        PhaseScope& operator=(const PhaseScope&) = delete;

      private:
        TaskProfile* owner = nullptr;
        uint64_t serial = 0;
    };

    bool configure(const char* capturePath, size_t requested) noexcept;
    bool enabled() const noexcept { return goal != 0; }
    bool recording() const noexcept { return armed && !tasksEnded && !pendingError; }
    bool complete() const noexcept { return enabled() && !collectionError && frames.size() == goal; }
    void begin_frame(const PortBenchCaptureStatus&) noexcept;
    void end_tasks() noexcept;
    void record(const void* task, const char* name, Phase, uint64_t elapsedNs) noexcept;
    void next_phase(Detail) noexcept;
    void simulation_steps(unsigned steps, float scaledDtSeconds, float debtBeforeSeconds,
                          float debtAfterSeconds, float tickSeconds, bool lockstep) noexcept;
    void end_frame(const PortBenchCaptureStatus&, uint32_t engineFrame) noexcept;
    bool write() noexcept;
    int error_code() const noexcept { return collectionError ? collectionError : outputError; }
    const std::vector<Frame>& samples() const noexcept { return frames; }
    const std::array<Task, MaxTasks>& tasks() const noexcept { return dictionary; }
    unsigned task_count() const noexcept { return dictionaryCount; }

  private:
    struct PendingDetail
    {
        const void* parent = nullptr;
        uint64_t elapsedNs = 0;
        uint32_t calls = 0;
    };
    void fail(int error) noexcept;
    bool begin_scope(const void*, Detail) noexcept;
    void finish_scope(uint64_t serial) noexcept;
    void finish_detail(uint64_t now) noexcept;
    std::string outputPath, temporaryPath, phasePath, phaseTemporaryPath;
    std::vector<Frame> frames;
    std::array<Task, MaxTasks> dictionary{}, pendingTasks{};
    std::array<Stats, MaxTasks> pendingStats{};
    std::array<PendingDetail, DetailCount> pendingDetails{};
    Simulation pendingSimulation;
    const void* simulationParent = nullptr;
    const void* phaseParent = nullptr;
    Detail phase = FixedControl;
    uint64_t phaseStart = 0, scopeSerial = 0;
    unsigned goal = 0, dictionaryCount = 0, pendingCount = 0;
    size_t before = 0;
    uint64_t managerStart = 0, managerNs = 0;
    bool armed = false, tasksEnded = false, finished = false;
    int pendingError = 0, collectionError = 0, outputError = 0;
};

TaskProfile& task_profile() noexcept;
// Called after PortBenchInit, before the frame window. It shares the explicit
// packet-profile opt-in and remains independent of the legacy task profiler.
bool configure_task_profile() noexcept;
}   // namespace port
