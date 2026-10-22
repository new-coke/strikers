// Exercise the actual task scheduler, including render work inside a pass.
#include "NL/nlTask.h"
#include "port/framerate.h"
#include "port/simulation_clock.hpp"
#include "port/task_profile.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <filesystem>
#include <unistd.h>

namespace {
uint64_t now = 1000000000;
bool deterministic = false;
bool diagnostics = false;
unsigned clockCalls = 0;
PortBenchCaptureStatus captureStatus{};
uint32_t virtualTicks = 0;
struct Task : nlTask {
    float supplied = 0;
    double accumulator = 0, simulation = 0;
    unsigned steps = 0, draws = 0;
    uint64_t cost = 0, transitionCost = 0;
    bool physics = false;
    std::vector<float> deltas;
    const char* GetName() override { return physics ? "physics" : "render"; }
    void StateTransition(unsigned int, unsigned int) override { now += transitionCost; }
    void Run(float dt) override {
        supplied += dt; deltas.push_back(dt);
        if (physics) {
            accumulator += dt;
            unsigned frameSteps = 0;
            while (accumulator + 1.e-9 >= .02 && port::simulation_step_budget(frameSteps)) {
                accumulator -= .02; simulation += .02; ++steps;
                ++frameSteps;
            }
        } else ++draws;
        now += cost;
    }
};
}
extern "C" {
unsigned long long port_monotonic_ns() { ++clockCalls; return now; }
int PortDiagnosticsEnabled() { return diagnostics; }
void PortBenchGetCaptureStatus(PortBenchCaptureStatus* out) { *out = captureStatus; }
int PortFixedTimestep() { return deterministic; }
int PortTaskClockFrame(unsigned int*) { return PORT_TASK_CLOCK_HOST; }
int PortTaskClockCurrent(unsigned int*) { return 0; }
void PortProfilerTaskRunBegin(const void*) {}
void PortProfilerTaskRunEnd() {}
void PortProfilerTaskTransitionBegin(const void*) {}
void PortProfilerTaskTransitionEnd() {}
}
uint32_t nlGetTicker() { return deterministic ? virtualTicks : uint32_t(now * 405 / 10000); }
float nlGetTickerDifference(unsigned int a, unsigned int b) { return float(b-a) / 40500.f; }
void* nlMalloc(size_t bytes, unsigned int, bool) { return std::malloc(bytes); }

int main() {
    port::SimulationFrameClock clock;
    assert(clock.sample(0)==0 && std::fabs(clock.sample(50000000)-.05f)<.00001f);
    assert(std::fabs(clock.sample(450000000)-.4f)<.00001f);
    Task catchup;
    catchup.physics=true;catchup.Run(.5f);
    assert(catchup.steps==16 && catchup.accumulator>.17);
    catchup.Run(.02f);
    assert(catchup.steps==26 && catchup.accumulator<.00001);
    nlTaskManager manager;
    nlTaskManager::m_pInstance = &manager;
    manager.m_lTaskList = nullptr;
    manager.m_CurrState = manager.m_PendingState = manager.m_PrevState = 2;
    manager.m_TimeDilation = 1;
    Task physics, render, audio;
    physics.physics = true;
    physics.cost = 2000000; render.cost = 43000000; audio.cost = 5000000;
    nlTaskManager::AddTask(&physics,7,2);
    nlTaskManager::AddTask(&render,10,2);
    nlTaskManager::AddTask(&audio,14,2);
    // 20 render FPS must still consume 50 physics ticks per real second.
    nlTaskManager::RunAllTasks();
    for (unsigned i=0;i<200;++i) nlTaskManager::RunAllTasks();
    assert(physics.steps == 500 && std::fabs(physics.simulation - 10) < .00001);
    assert(render.draws == 201 && audio.draws == 201);
    assert(physics.deltas == render.deltas && render.deltas == audio.deltas);
    // A 10 Hz rendered frame is also elapsed time, not one simulation tick.
    render.cost = 93000000;
    for (unsigned i=0;i<101;++i) nlTaskManager::RunAllTasks();
    assert(std::fabs(physics.supplied - 20.05) < .0001);
    assert(physics.steps == 1002);
    // Deliberate slow motion scales all tasks equally, not the render count.
    manager.m_TimeDilation = .5f;
    nlTaskManager::RunAllTasks();
    assert(std::fabs(physics.deltas.back() - .05f) < .00001);
    // A suspend/debugger stop doesn't request hundreds of catch-up updates.
    now += 10000000000ull;
    nlTaskManager::RunAllTasks();
    assert(physics.deltas.back() == 0);
    // Explicit deterministic captures retain their virtual clock, then return
    // to wall time without carrying the previous capture's epoch.
    deterministic = true;
    physics.nPrevTicker = render.nPrevTicker = audio.nPrevTicker = 0;
    virtualTicks = 675000;
    nlTaskManager::RunAllTasks();
    assert(std::fabs(physics.deltas.back() - 1.f/120.f) < .00001);
    deterministic = false;
    nlTaskManager::RunAllTasks();
    assert(physics.deltas.back() == 0);
    // A synchronous warm scene load is not time spent simulating the new
    // scene. It must not jump its first animation forward on the next pass.
    manager.m_TimeDilation = 1;
    physics.transitionCost = 750000000;
    manager.m_PendingState = 0x102;
    nlTaskManager::RunAllTasks();
    assert(std::fabs(physics.deltas.back() - .1f) < .00001);
    nlTaskManager::RunAllTasks();
    assert(std::fabs(physics.deltas.back() - .1f) < .00001);
    // An ordinary rendering hitch still contributes its full elapsed time.
    now += 400000000;
    nlTaskManager::RunAllTasks();
    assert(std::fabs(physics.deltas.back() - .5f) < .00001);
    assert(physics.deltas == render.deltas && render.deltas == audio.deltas);
    // The real scheduler adds no capture clocks while OFF or warming up. Use
    // deterministic simulation here to distinguish profiler clocks from the
    // scheduler's existing realtime simulation clock.
    deterministic = true;
    auto clocksBefore = clockCalls;
    nlTaskManager::RunAllTasks();
    assert(clockCalls == clocksBefore);
    const auto directory = std::filesystem::temp_directory_path() /
        ("strikers-task-scheduler-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto capturePath = (directory / "run.csv").string();
    setenv("STRIKERS_FRAME_CAPTURE", capturePath.c_str(), 1);
    setenv("STRIKERS_VITAGL_PACKET_PROFILE", "1", 1);
    captureStatus = { PORT_CAPTURE_COLLECTING, 0, 2, 1, 0, 0 };
    assert(port::configure_task_profile());
    auto& profile = port::task_profile();
    profile.begin_frame(captureStatus);
    nlTaskManager::RunAllTasks();
    profile.end_tasks();
    captureStatus.warmupRemaining = 0;
    profile.end_frame(captureStatus, 100);
    assert(clockCalls == clocksBefore && profile.samples().empty());
    physics.cost = 2000000; render.cost = 43000000; audio.cost = 5000000;
    physics.transitionCost = 1000000; render.transitionCost = 2000000; audio.transitionCost = 3000000;
    manager.m_PendingState ^= 0x100;
    profile.begin_frame(captureStatus);
    nlTaskManager::RunAllTasks();
    profile.end_tasks();
    captureStatus.samples = 1;
    captureStatus.lastFrame = 600;
    profile.end_frame(captureStatus, 101);
    assert(clockCalls - clocksBefore == 14); // manager + three Run/Transition pairs
    assert(profile.task_count() == 3 && profile.samples().size() == 1);
    const auto& taskFrame = profile.samples()[0];
    assert(taskFrame.managerNs == 56000000 && taskFrame.engineFrame == 101);
    for (unsigned i = 0; i < profile.task_count(); ++i)
    {
        const auto* task = static_cast<const Task*>(profile.tasks()[i].key);
        assert(taskFrame.tasks[i].runNs == task->cost && taskFrame.tasks[i].runCalls == 1);
        assert(taskFrame.tasks[i].transitionNs == task->transitionCost && taskFrame.tasks[i].transitionCalls == 1);
    }
    // Enabling the old task profiler must reuse the same Run interval, adding
    // no second pair of clocks or changes to its existing reporting window.
    setenv("STRIKERS_TASK_PROFILE", "1", 1);
    setenv("STRIKERS_TASK_PROFILE_FRAMES", "600", 1);
    diagnostics = true;
    clocksBefore = clockCalls;
    profile.begin_frame(captureStatus);
    nlTaskManager::RunAllTasks();
    profile.end_tasks();
    captureStatus.state = PORT_CAPTURE_COMPLETE;
    captureStatus.samples = 2;
    captureStatus.lastFrame = 601;
    profile.end_frame(captureStatus, 102);
    assert(clockCalls - clocksBefore == 8 && profile.complete() && !profile.error_code());
    assert(profile.samples()[1].managerNs == 50000000 && profile.samples()[1].liveFrame == 601);
    diagnostics = false;
    clocksBefore = clockCalls;
    profile.begin_frame(captureStatus);
    nlTaskManager::RunAllTasks();
    profile.end_tasks();
    assert(clockCalls == clocksBefore);
    std::filesystem::remove_all(directory);
    std::puts("Task scheduler: 20/10 render FPS, 50 Hz physics, shared elapsed time, dilation, suspend, warm loading and deterministic-clock transitions passed");
}
