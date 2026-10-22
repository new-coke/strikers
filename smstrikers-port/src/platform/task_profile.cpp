#if defined(STRIKERS_VITA_NATIVE_RUNTIME) || defined(STRIKERS_TASK_PROFILE_TEST)
#include "port/task_profile.hpp"
#include "port/host.h"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace port
{
namespace
{
TaskProfile capture;
constexpr const char* detailNames[TaskProfile::DetailCount]{
    "Fixed control", "Fixed input", "Game AI", "NPC AI", "Pre-physics", "Physics",
    "Post-physics", "Animated nets", "Sideline", "Replay capture", "Object lighting",
    "Presentation", "Render preparation", "World render", "Snapshot render",
    "Inventory/indicators", "Post-effects"
};

bool quoted(FILE* file, const char* name)
{
    if (std::fputc('"', file) == EOF) return false;
    for (; *name; ++name)
    {
        if (*name == '"' && std::fputc('"', file) == EOF) return false;
        if (std::fputc(*name, file) == EOF) return false;
    }
    return std::fputc('"', file) != EOF;
}
}   // namespace

TaskProfile& task_profile() noexcept { return capture; }

bool configure_task_profile() noexcept
{
    const auto* enabled = std::getenv("STRIKERS_VITAGL_PACKET_PROFILE");
    if (!enabled || std::strcmp(enabled, "1")) return false;
    if (capture.enabled()) return true;
    PortBenchCaptureStatus status{};
    PortBenchGetCaptureStatus(&status);
    if (capture.configure(std::getenv("STRIKERS_FRAME_CAPTURE"), status.requested)) return true;
    std::fprintf(stderr, "[task-capture] configuration_error=%d requires finite frame_capture (1..8192)\n",
                 capture.error_code());
    return false;
}

bool TaskProfile::configure(const char* path, size_t requested) noexcept
{
    if (enabled()) return false;
    if (!path || !*path || !requested || requested > MaxFrames)
    {
        outputError = EINVAL;
        return false;
    }
    try
    {
        outputPath = std::string(path) + ".tasks.csv";
        temporaryPath = outputPath + ".tmp";
        phasePath = std::string(path) + ".phases.csv";
        phaseTemporaryPath = phasePath + ".tmp";
        frames.reserve(requested);
        goal = unsigned(requested);
        outputError = 0;
        return true;
    }
    catch (...) { outputError = ENOMEM; return false; }
}

void TaskProfile::fail(int error) noexcept
{
    collectionError = error;
    armed = false;
    finished = true;
    std::fprintf(stderr, "[task-capture] samples=%u collection_error=%d path=%s\n",
                 unsigned(frames.size()), collectionError, outputPath.c_str());
}

void TaskProfile::begin_frame(const PortBenchCaptureStatus& status) noexcept
{
    if (!enabled() || finished) return;
    if (armed || status.requested != goal || status.samples != frames.size())
    {
        fail(EINVAL);
        return;
    }
    before = status.samples;
    armed = status.state == PORT_CAPTURE_COLLECTING && !status.warmupRemaining;
    if (!armed) return;
    pendingCount = 0;
    pendingError = 0;
    pendingStats = {};
    pendingDetails = {};
    pendingSimulation = {};
    simulationParent = phaseParent = nullptr;
    managerNs = 0;
    tasksEnded = false;
    managerStart = port_monotonic_ns();
}

TaskProfile::PhaseScope::PhaseScope(TaskProfile& profile, const void* parent, Detail first) noexcept
{
    if (profile.begin_scope(parent, first))
    {
        owner = &profile;
        serial = profile.scopeSerial;
    }
}

TaskProfile::PhaseScope::~PhaseScope()
{
    if (owner) owner->finish_scope(serial);
}

bool TaskProfile::begin_scope(const void* parent, Detail first) noexcept
{
    if (!recording()) return false;
    if (!parent || first >= DetailCount || phaseParent)
    {
        pendingError = EINVAL;
        return false;
    }
    if (scopeSerial == UINT64_MAX)
    {
        pendingError = EOVERFLOW;
        return false;
    }
    ++scopeSerial;
    phaseParent = parent;
    phase = first;
    phaseStart = port_monotonic_ns();
    return true;
}

void TaskProfile::finish_detail(uint64_t now) noexcept
{
    auto& detail = pendingDetails[phase];
    if (now < phaseStart || (detail.parent && detail.parent != phaseParent))
    {
        pendingError = EINVAL;
        return;
    }
    const auto elapsed = now - phaseStart;
    if (elapsed > UINT64_MAX - detail.elapsedNs || detail.calls == UINT32_MAX)
    {
        pendingError = EOVERFLOW;
        return;
    }
    detail.parent = phaseParent;
    detail.elapsedNs += elapsed;
    ++detail.calls;
}

void TaskProfile::next_phase(Detail next) noexcept
{
    if (!recording() || !phaseParent) return;
    if (next >= DetailCount)
    {
        pendingError = EINVAL;
        return;
    }
    const auto now = port_monotonic_ns();
    finish_detail(now);
    phase = next;
    phaseStart = now;
}

void TaskProfile::finish_scope(uint64_t serial) noexcept
{
    if (serial != scopeSerial || !phaseParent) return;
    if (recording()) finish_detail(port_monotonic_ns());
    phaseParent = nullptr;
}

void TaskProfile::simulation_steps(unsigned steps, float scaledDt, float beforeDebt,
                                   float afterDebt, float tick, bool lockstep) noexcept
{
    if (!recording() || !phaseParent) return;
    if (pendingSimulation.present || steps > 16 || (lockstep && steps > 1) || !std::isfinite(scaledDt) ||
        !std::isfinite(beforeDebt) || !std::isfinite(afterDebt) || !std::isfinite(tick) || tick <= 0)
    {
        pendingError = EINVAL;
        return;
    }
    pendingSimulation = { steps, MaxTasks, scaledDt, beforeDebt, afterDebt, tick, true, lockstep };
    simulationParent = phaseParent;
}

void TaskProfile::record(const void* task, const char* name, Phase phase, uint64_t elapsedNs) noexcept
{
    if (!recording()) return;
    if (!task || (phase != Run && phase != Transition))
    {
        pendingError = EINVAL;
        return;
    }
    if (!name) name = "?";
    size_t length = 0;
    while (length < NameBytes && name[length]) ++length;
    if (length == NameBytes)
    {
        pendingError = ENAMETOOLONG;
        return;
    }
    unsigned slot = 0;
    for (; slot < pendingCount; ++slot)
        if (pendingTasks[slot].key == task && !std::strcmp(pendingTasks[slot].name.data(), name))
            break;
    if (slot == pendingCount)
    {
        if (pendingCount == MaxTasks)
        {
            pendingError = EOVERFLOW;
            return;
        }
        pendingTasks[slot] = {};
        pendingTasks[slot].key = task;
        std::memcpy(pendingTasks[slot].name.data(), name, length);
        ++pendingCount;
    }
    auto& stats = pendingStats[slot];
    auto& ns = phase == Run ? stats.runNs : stats.transitionNs;
    auto& calls = phase == Run ? stats.runCalls : stats.transitionCalls;
    if (elapsedNs > UINT64_MAX - ns || calls == UINT32_MAX)
    {
        pendingError = EOVERFLOW;
        return;
    }
    ns += elapsedNs;
    ++calls;
}

void TaskProfile::end_tasks() noexcept
{
    if (!armed || tasksEnded) return;
    if (phaseParent) pendingError = EINVAL;
    const auto now = port_monotonic_ns();
    if (now < managerStart) pendingError = EINVAL;
    else managerNs = now - managerStart;
    tasksEnded = true;
}

void TaskProfile::end_frame(const PortBenchCaptureStatus& status, uint32_t engineFrame) noexcept
{
    if (!enabled() || finished) return;
    const bool measured = armed;
    armed = false;
    if (status.requested != goal || status.samples < before || status.samples > before + 1)
    {
        fail(EINVAL);
        return;
    }
    if (status.samples == before) return; // Intro/replay: discard staging and its task names.
    if (!measured || !tasksEnded || before != frames.size() || frames.size() >= goal)
    {
        fail(EINVAL);
        return;
    }
    if (pendingError)
    {
        fail(pendingError);
        return;
    }
    uint64_t attributed = 0;
    for (unsigned i = 0; i < pendingCount; ++i)
        for (const auto ns : { pendingStats[i].runNs, pendingStats[i].transitionNs })
        {
            if (ns > managerNs - attributed)
            {
                fail(EINVAL);
                return;
            }
            attributed += ns;
        }
    std::array<unsigned, MaxTasks> slots{};
    unsigned nextCount = dictionaryCount;
    for (unsigned i = 0; i < pendingCount; ++i)
    {
        unsigned slot = 0;
        for (; slot < dictionaryCount; ++slot)
            if (dictionary[slot].key == pendingTasks[i].key &&
                !std::strcmp(dictionary[slot].name.data(), pendingTasks[i].name.data()))
                break;
        if (slot == dictionaryCount)
        {
            if (nextCount == MaxTasks)
            {
                fail(EOVERFLOW);
                return;
            }
            slot = nextCount++;
        }
        slots[i] = slot;
    }
    Frame row;
    row.sample = unsigned(before);
    row.liveFrame = status.lastFrame;
    row.engineFrame = engineFrame;
    row.managerNs = managerNs;
    const auto parent_slot = [&](const void* key) {
        unsigned found = MaxTasks;
        for (unsigned i = 0; i < pendingCount; ++i)
            if (pendingTasks[i].key == key)
            {
                if (found != MaxTasks) return MaxTasks; // A renamed parent is ambiguous.
                found = i;
            }
        return found != MaxTasks && pendingStats[found].runCalls ? found : MaxTasks;
    };
    std::array<uint64_t, MaxTasks> childNs{};
    for (unsigned i = 0; i < DetailCount; ++i)
    {
        const auto& detail = pendingDetails[i];
        if (!detail.calls) continue;
        const auto parent = parent_slot(detail.parent);
        if (parent == MaxTasks || detail.elapsedNs > pendingStats[parent].runNs - childNs[parent])
        {
            fail(EINVAL);
            return;
        }
        childNs[parent] += detail.elapsedNs;
        row.details[i] = { detail.elapsedNs, detail.calls, slots[parent] };
    }
    const auto simulationSlot = parent_slot(simulationParent);
    if (pendingSimulation.present &&
        (simulationSlot == MaxTasks || pendingStats[simulationSlot].runCalls != 1))
    {
        fail(EINVAL);
        return;
    }
    for (unsigned i = FixedControl; i < RenderLighting; ++i)
    {
        const auto& detail = pendingDetails[i];
        const unsigned expected = pendingSimulation.present
            ? pendingSimulation.steps + unsigned(i == FixedControl || i == FixedInput) : 0;
        if (detail.calls != expected || (detail.calls && detail.parent != simulationParent))
        {
            fail(EINVAL);
            return;
        }
    }
    row.simulation = pendingSimulation;
    if (pendingSimulation.present) row.simulation.parentTask = slots[simulationSlot];
    for (unsigned i = 0; i < pendingCount; ++i)
    {
        row.tasks[slots[i]] = pendingStats[i];
        if (slots[i] >= dictionaryCount) dictionary[slots[i]] = pendingTasks[i];
    }
    dictionaryCount = nextCount;
    frames.push_back(row); // Capacity was reserved before the measured window.
    if (frames.size() == goal)
    {
        finished = true;
        write();
    }
}

bool TaskProfile::write() noexcept
{
    if (!complete()) return false;
    errno = 0;
    FILE* file = std::fopen(temporaryPath.c_str(), "wb");
    bool ok = file != nullptr;
    if (ok)
    {
        ok = std::fprintf(file, "# strikers-task-profile-v1 attribution_only=1 task_scopes_are_subsets=1\n"
                               "sample,live_frame,engine_frame,record,task_id,task_name,task_count,manager_ns,run_ns,transition_ns,run_calls,transition_calls\n") >= 0;
        for (const auto& frame : frames)
        {
            if (!ok) break;
            unsigned taskCount = 0;
            for (unsigned i = 0; i < dictionaryCount; ++i)
                taskCount += frame.tasks[i].runCalls || frame.tasks[i].transitionCalls;
            ok = std::fprintf(file, "%u,%llu,%u,frame,0,\"\",%u,%llu,0,0,0,0\n", frame.sample,
                              (unsigned long long)frame.liveFrame, frame.engineFrame,
                              taskCount, (unsigned long long)frame.managerNs) >= 0;
            for (unsigned i = 0; i < dictionaryCount && ok; ++i)
            {
                const auto& stats = frame.tasks[i];
                if (!stats.runCalls && !stats.transitionCalls) continue;
                ok = std::fprintf(file, "%u,%llu,%u,task,%u,", frame.sample,
                                  (unsigned long long)frame.liveFrame, frame.engineFrame, i) >= 0 &&
                     quoted(file, dictionary[i].name.data()) &&
                     std::fprintf(file, ",0,0,%llu,%llu,%u,%u\n", (unsigned long long)stats.runNs,
                                  (unsigned long long)stats.transitionNs, stats.runCalls, stats.transitionCalls) >= 0;
            }
        }
        if (std::ferror(file)) ok = false;
        if (std::fclose(file)) ok = false;
    }
    if (ok)
    {
        file = std::fopen(phaseTemporaryPath.c_str(), "wb");
        ok = file != nullptr;
        if (ok)
        {
            ok = std::fprintf(file, "# strikers-task-phases-v1 attribution_only=1 disjoint_children_of_run=1 simulation_units=seconds\n"
                                   "sample,live_frame,engine_frame,record,parent_task_id,phase_id,phase_name,phase_count,simulation_count,elapsed_ns,calls,steps,scaled_dt_seconds,debt_before_seconds,debt_after_seconds,tick_seconds,lockstep\n") >= 0;
            for (const auto& frame : frames)
            {
                if (!ok) break;
                unsigned phaseCount = 0;
                for (const auto& detail : frame.details) phaseCount += detail.calls != 0;
                ok = std::fprintf(file, "%u,%llu,%u,frame,0,0,\"\",%u,%u,0,0,0,0,0,0,0,0\n",
                                  frame.sample, (unsigned long long)frame.liveFrame, frame.engineFrame,
                                  phaseCount, unsigned(frame.simulation.present)) >= 0;
                for (unsigned i = 0; i < DetailCount && ok; ++i)
                {
                    const auto& detail = frame.details[i];
                    if (!detail.calls) continue;
                    ok = std::fprintf(file, "%u,%llu,%u,phase,%u,%u,", frame.sample,
                                      (unsigned long long)frame.liveFrame, frame.engineFrame,
                                      detail.parentTask, i) >= 0 &&
                         quoted(file, detailNames[i]) &&
                         std::fprintf(file, ",0,0,%llu,%u,0,0,0,0,0,0\n",
                                      (unsigned long long)detail.elapsedNs, detail.calls) >= 0;
                }
                const auto& sim = frame.simulation;
                if (sim.present && ok)
                    ok = std::fprintf(file, "%u,%llu,%u,simulation,%u,0,\"\",0,0,0,1,%u,%.9g,%.9g,%.9g,%.9g,%u\n",
                                      frame.sample, (unsigned long long)frame.liveFrame, frame.engineFrame,
                                      sim.parentTask, sim.steps, double(sim.scaledDtSeconds),
                                      double(sim.debtBeforeSeconds), double(sim.debtAfterSeconds),
                                      double(sim.tickSeconds), unsigned(sim.lockstep)) >= 0;
            }
            if (std::ferror(file)) ok = false;
            if (std::fclose(file)) ok = false;
        }
    }
    // Publish the v1 task file last, after both complete sidecars are closed.
    // A failed rename keeps the same RAM samples available for explicit retry.
    if (ok && std::rename(phaseTemporaryPath.c_str(), phasePath.c_str())) ok = false;
    if (ok && std::rename(temporaryPath.c_str(), outputPath.c_str())) ok = false;
    outputError = ok ? 0 : (errno ? errno : EIO);
    if (!ok)
    {
        std::remove(temporaryPath.c_str());
        std::remove(phaseTemporaryPath.c_str());
    }
    std::fprintf(stderr, "[task-capture] samples=%u tasks=%u output_error=%d path=%s phases=%s\n",
                 unsigned(frames.size()), dictionaryCount, outputError, outputPath.c_str(), phasePath.c_str());
    return ok;
}
}   // namespace port
#endif
