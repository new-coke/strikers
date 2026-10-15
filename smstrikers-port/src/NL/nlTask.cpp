#include "NL/nlTask.h"
#include "NL/nlMemory.h"
#include "NL/nlTicker.h"
#include "NL/nlDLRing.h"
#include "port/host.h"
#include "port/config.h"
#include "port/vita_profiler.h"
#include "port/framerate.h" // PORT: PortTaskClockFrame
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/determinism.h"
#include "port/simulation_clock.hpp"
#endif

#include <cstdio>
#include <cstdlib>

#if defined(PORT_VITA) && !defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include <aurora_vita_backend.hpp>
#include "port/profile_output.hpp"
#include "NL/glx/glxSend.h"
extern "C" uint32_t aurora_vita_debug_runtime_flags(void) noexcept;

static port::ProfileOutput& PortTaskProfileOutput()
{
    static const bool buffered = []() {
        const char* value = std::getenv("STRIKERS_TASK_PROFILE_BUFFERED");
        return value != nullptr && value[0] == '1';
    }();
    static port::ProfileOutput output("ux0:data/strikersVita/task_profile.log", "task", buffered);
    return output;
}
#endif

// PORT: profiling output that survives no-log builds (stderr goes nowhere on Vita).
static FILE* PortProfileOut()
{
#if defined(PORT_VITA) && !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    return PortTaskProfileOutput().get();
#endif
    return stderr;
}

#define assert(condition) ((condition) ? ((void)0) : ((void)0))

u8 g_StackWatermarkFiller = 0x78;
float g_fTaskTimeUpperBound = 0.1f;

nlTaskManager* nlTaskManager::m_pInstance = nullptr;
u8 g_DoStackWatermarkTests;
float g_fTaskTimeLowerBound;

namespace
{
struct TaskProfileSlot
{
    nlTask* task;
    unsigned long long totalNs;
    unsigned long long maxNs;
    unsigned int calls;
};

TaskProfileSlot s_TaskProfile[32]{};
unsigned int s_TaskProfileFrames;
int s_TaskProfileEnabled = -1;
unsigned int s_TaskProfileWindow = 120;

bool TaskProfileEnabled()
{
    if (!PortDiagnosticsEnabled())
        return false;
    if (s_TaskProfileEnabled < 0)
    {
        const char* value = std::getenv("STRIKERS_TASK_PROFILE");
        s_TaskProfileEnabled = value != nullptr && *value != '\0' && *value != '0';
        const char* window = std::getenv("STRIKERS_TASK_PROFILE_FRAMES");
        if (window != nullptr && *window != '\0')
        {
            const unsigned long parsed = std::strtoul(window, nullptr, 10);
            if (parsed >= 10 && parsed <= 600)
                s_TaskProfileWindow = (unsigned int)parsed;
        }
    }
    return s_TaskProfileEnabled != 0;
}

TaskProfileSlot* TaskProfileGet(nlTask* task)
{
    TaskProfileSlot* empty = nullptr;
    for (TaskProfileSlot& slot : s_TaskProfile)
    {
        if (slot.task == task)
            return &slot;
        if (slot.task == nullptr && empty == nullptr)
            empty = &slot;
    }
    if (empty != nullptr)
        empty->task = task;
    return empty;
}

void TaskProfileReport()
{
    FILE* const out = PortProfileOut();
    std::fprintf(out, "[task-profile] frames=%u\n", s_TaskProfileFrames);
    bool emitted[32]{};
    for (unsigned int rank = 0; rank < 12; ++rank)
    {
        int best = -1;
        for (unsigned int i = 0; i < 32; ++i)
        {
            if (emitted[i] || s_TaskProfile[i].task == nullptr || s_TaskProfile[i].calls == 0)
                continue;
            if (best < 0 || s_TaskProfile[i].totalNs > s_TaskProfile[best].totalNs)
                best = (int)i;
        }
        if (best < 0)
            break;
        emitted[best] = true;
        const TaskProfileSlot& slot = s_TaskProfile[best];
        const char* name = slot.task->GetName();
        std::fprintf(out, "[task-profile] %-24s total_us=%llu mean_us=%llu max_us=%llu calls=%u\n",
                     name != nullptr ? name : "?",
                     slot.totalNs / 1000ull,
                     slot.calls != 0 ? slot.totalNs / (1000ull * slot.calls) : 0ull,
                     slot.maxNs / 1000ull,
                     slot.calls);
    }
#if defined(PORT_VITA) && !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    {
        const aurora::vita::PerformanceSnapshot perf = aurora::vita::performance_snapshot();
        static uint64_t s_lastChunks = 0;
        static uint64_t s_lastDenied = 0;
        static uint64_t s_lastTelemetryFailures = 0;
        static uint64_t s_lastOverruns = 0;
        static uint64_t s_lastTotalChunkUs = 0;
        static uint64_t s_lastVertexCalls = 0;
        static uint64_t s_lastVertexDynamicCalls = 0;
        static uint64_t s_lastVertexWallUs = 0;
        static uint64_t s_lastVertexWaitUs = 0;
        static uint64_t s_lastGeometryHits = 0;
        static uint64_t s_lastGeometryMisses = 0;
        static uint64_t s_lastGeometryFallbacks = 0;
        static uint64_t s_lastGeometryPreflightRejects = 0;
        static uint64_t s_lastFragmentPrepareHits = 0;
        static uint64_t s_lastFragmentPrepareMisses = 0;
        static uint64_t s_lastFixedPoolAllocations = 0;
        static uint64_t s_lastFixedPoolReuses = 0;
        static uint64_t s_lastFixedPoolFallbacks = 0;
        static uint64_t s_lastVertexLaneItems[4]{};
        static uint64_t s_lastVertexLaneChunks[4]{};
        static uint64_t s_lastVertexLaneWorkUs[4]{};

        const auto deltaCounter = [](uint64_t current, uint64_t previous) -> uint64_t {
            return current >= previous ? current - previous : current;
        };
        const uint64_t deltaChunks = deltaCounter(perf.core3Chunks, s_lastChunks);
        const uint64_t deltaDenied = deltaCounter(perf.core3Denied, s_lastDenied);
        const uint64_t deltaTelemetryFailures = deltaCounter(perf.core3TelemetryFailures, s_lastTelemetryFailures);
        const uint64_t deltaOverruns = deltaCounter(perf.core3Overruns, s_lastOverruns);
        const uint64_t deltaTotalChunkUs = deltaCounter(perf.core3TotalChunkUs, s_lastTotalChunkUs);
        const uint64_t deltaVertexCalls = deltaCounter(perf.vertexParallelCalls, s_lastVertexCalls);
        const uint64_t deltaVertexDynamicCalls = deltaCounter(perf.vertexParallelDynamicCalls, s_lastVertexDynamicCalls);
        const uint64_t deltaVertexWallUs = deltaCounter(perf.vertexParallelTotalWallUs, s_lastVertexWallUs);
        const uint64_t deltaVertexWaitUs = deltaCounter(perf.vertexParallelCallerWaitUs, s_lastVertexWaitUs);
        uint64_t deltaVertexLaneItems[4]{};
        uint64_t deltaVertexLaneChunks[4]{};
        uint64_t deltaVertexLaneWorkUs[4]{};
        for (unsigned int lane = 0; lane < 4; ++lane)
        {
            deltaVertexLaneItems[lane] = deltaCounter(perf.vertexLaneItems[lane], s_lastVertexLaneItems[lane]);
            deltaVertexLaneChunks[lane] = deltaCounter(perf.vertexLaneChunks[lane], s_lastVertexLaneChunks[lane]);
            deltaVertexLaneWorkUs[lane] = deltaCounter(perf.vertexLaneWorkUs[lane], s_lastVertexLaneWorkUs[lane]);
        }

        std::fprintf(out,
                     "[cpu3-profile] available=%u budget_configured=%u telemetry=%u dispatch=%u target_pct=%u "
                     "total_pct_x100=%u short_credit_us=%llu long_credit_us=%llu "
                     "chunks=%llu chunks_delta=%llu denied=%llu denied_delta=%llu "
                     "telemetry_failures=%llu telemetry_failures_delta=%llu overruns=%llu overruns_delta=%llu "
                     "total_chunk_us=%llu total_chunk_us_delta=%llu max_chunk_us=%u\n",
                     perf.core3Available ? 1u : 0u,
                     perf.core3BudgetConfigured ? 1u : 0u,
                     perf.core3TelemetryValid ? 1u : 0u,
                     perf.core3DispatchAllowed ? 1u : 0u,
                     perf.core3TargetPercent,
                     perf.core3LastTotalPercentX100,
                     static_cast<unsigned long long>(perf.core3ShortCreditUs),
                     static_cast<unsigned long long>(perf.core3LongCreditUs),
                     static_cast<unsigned long long>(perf.core3Chunks),
                     static_cast<unsigned long long>(deltaChunks),
                     static_cast<unsigned long long>(perf.core3Denied),
                     static_cast<unsigned long long>(deltaDenied),
                     static_cast<unsigned long long>(perf.core3TelemetryFailures),
                     static_cast<unsigned long long>(deltaTelemetryFailures),
                     static_cast<unsigned long long>(perf.core3Overruns),
                     static_cast<unsigned long long>(deltaOverruns),
                     static_cast<unsigned long long>(perf.core3TotalChunkUs),
                     static_cast<unsigned long long>(deltaTotalChunkUs),
                     perf.core3MaxChunkUs);

        std::fprintf(out,
                     "[vertex-parallel] calls_delta=%llu dynamic_delta=%llu wall_us_delta=%llu wait_us_delta=%llu "
                     "l0_items_delta=%llu l0_chunks_delta=%llu l0_work_us_delta=%llu "
                     "l1_items_delta=%llu l1_chunks_delta=%llu l1_work_us_delta=%llu "
                     "l2_items_delta=%llu l2_chunks_delta=%llu l2_work_us_delta=%llu "
                     "l3_items_delta=%llu l3_chunks_delta=%llu l3_work_us_delta=%llu\n",
                     static_cast<unsigned long long>(deltaVertexCalls),
                     static_cast<unsigned long long>(deltaVertexDynamicCalls),
                     static_cast<unsigned long long>(deltaVertexWallUs),
                     static_cast<unsigned long long>(deltaVertexWaitUs),
                     static_cast<unsigned long long>(deltaVertexLaneItems[0]),
                     static_cast<unsigned long long>(deltaVertexLaneChunks[0]),
                     static_cast<unsigned long long>(deltaVertexLaneWorkUs[0]),
                     static_cast<unsigned long long>(deltaVertexLaneItems[1]),
                     static_cast<unsigned long long>(deltaVertexLaneChunks[1]),
                     static_cast<unsigned long long>(deltaVertexLaneWorkUs[1]),
                     static_cast<unsigned long long>(deltaVertexLaneItems[2]),
                     static_cast<unsigned long long>(deltaVertexLaneChunks[2]),
                     static_cast<unsigned long long>(deltaVertexLaneWorkUs[2]),
                     static_cast<unsigned long long>(deltaVertexLaneItems[3]),
                     static_cast<unsigned long long>(deltaVertexLaneChunks[3]),
                     static_cast<unsigned long long>(deltaVertexLaneWorkUs[3]));

        std::fprintf(out,
                     "[geometry-profile] hits_delta=%llu misses_delta=%llu fallbacks_delta=%llu "
                     "entries=%llu bytes=%llu runtime_flags=0x%x\n",
                     static_cast<unsigned long long>(deltaCounter(perf.staticGeometryHits, s_lastGeometryHits)),
                     static_cast<unsigned long long>(deltaCounter(perf.staticGeometryMisses, s_lastGeometryMisses)),
                     static_cast<unsigned long long>(deltaCounter(perf.staticGeometryLookupFallbacks, s_lastGeometryFallbacks)),
                     static_cast<unsigned long long>(perf.staticGeometryEntries),
                     static_cast<unsigned long long>(perf.staticGeometryBytes),
                     static_cast<unsigned int>(aurora_vita_debug_runtime_flags()));
        s_lastGeometryHits = perf.staticGeometryHits;
        s_lastGeometryMisses = perf.staticGeometryMisses;
        s_lastGeometryFallbacks = perf.staticGeometryLookupFallbacks;

        std::fprintf(out,
                     "[geometry-preflight] enabled=%u rejects_delta=%llu\n",
                     perf.geometryPreflightEnabled ? 1u : 0u,
                     static_cast<unsigned long long>(deltaCounter(perf.geometryPreflightRejects, s_lastGeometryPreflightRejects)));
        s_lastGeometryPreflightRejects = perf.geometryPreflightRejects;

        std::fprintf(out,
                     "[fragment-prepare-profile] hits_delta=%llu misses_delta=%llu\n",
                     static_cast<unsigned long long>(deltaCounter(perf.nativeFragmentPrepareHits, s_lastFragmentPrepareHits)),
                     static_cast<unsigned long long>(deltaCounter(perf.nativeFragmentPrepareMisses, s_lastFragmentPrepareMisses)));
        s_lastFragmentPrepareHits = perf.nativeFragmentPrepareHits;
        s_lastFragmentPrepareMisses = perf.nativeFragmentPrepareMisses;

        std::fprintf(out,
                     "[fixed-uniform-pool] enabled=%u allocations_delta=%llu reuses_delta=%llu "
                     "fallbacks_delta=%llu retained_bytes=%llu\n",
                     perf.fixedUniformPoolEnabled ? 1u : 0u,
                     static_cast<unsigned long long>(deltaCounter(perf.fixedUniformPoolAllocations, s_lastFixedPoolAllocations)),
                     static_cast<unsigned long long>(deltaCounter(perf.fixedUniformPoolReuses, s_lastFixedPoolReuses)),
                     static_cast<unsigned long long>(deltaCounter(perf.fixedUniformPoolFallbacks, s_lastFixedPoolFallbacks)),
                     static_cast<unsigned long long>(perf.fixedUniformPoolBytes));
        s_lastFixedPoolAllocations = perf.fixedUniformPoolAllocations;
        s_lastFixedPoolReuses = perf.fixedUniformPoolReuses;
        s_lastFixedPoolFallbacks = perf.fixedUniformPoolFallbacks;

        // These are counts for one completed renderer frame, not 120-frame
        // totals. Reading the published snapshot never drains the GX worker.
        const aurora::vita::PerformanceSnapshot completed = aurora::vita::completed_performance_snapshot();
        if (completed.completedFrame)
        {
            std::fprintf(out,
                         "[native-state-profile] completed_frame=%llu gxm_disable=0x%x "
                         "pipeline_setters=%u pipeline_setters_skipped=%u uniform_upload_calls=%u "
                         "uniform_upload_bytes=%llu\n",
                         static_cast<unsigned long long>(completed.frameIndex),
                         static_cast<unsigned int>(aurora::vita::gfx::gxm_disable_mask()),
                         completed.nativePipelineSetters,
                         completed.nativePipelineSettersSkipped,
                         completed.nativeUniformUploadCalls,
                         static_cast<unsigned long long>(completed.nativeUniformUploadBytes));
        }

        s_lastChunks = perf.core3Chunks;
        s_lastDenied = perf.core3Denied;
        s_lastTelemetryFailures = perf.core3TelemetryFailures;
        s_lastOverruns = perf.core3Overruns;
        s_lastTotalChunkUs = perf.core3TotalChunkUs;
        s_lastVertexCalls = perf.vertexParallelCalls;
        s_lastVertexDynamicCalls = perf.vertexParallelDynamicCalls;
        s_lastVertexWallUs = perf.vertexParallelTotalWallUs;
        s_lastVertexWaitUs = perf.vertexParallelCallerWaitUs;
        for (unsigned int lane = 0; lane < 4; ++lane)
        {
            s_lastVertexLaneItems[lane] = perf.vertexLaneItems[lane];
            s_lastVertexLaneChunks[lane] = perf.vertexLaneChunks[lane];
            s_lastVertexLaneWorkUs[lane] = perf.vertexLaneWorkUs[lane];
        }
    }
    glx_ReportSkinPackets(out);
    glx_ReportPacketProfile(out);
    PortTaskProfileOutput().flush_report();
#endif
    for (TaskProfileSlot& slot : s_TaskProfile)
    {
        slot.totalNs = 0;
        slot.maxNs = 0;
        slot.calls = 0;
    }
    s_TaskProfileFrames = 0;
}
}

/**
 * Offset/Address/Size: 0x0 | 0x801D28FC | size: 0xC
 */
void nlTaskManager::SetTimeDilation(float timeDilation)
{
    m_pInstance->m_TimeDilation = timeDilation;
}

/**
 * Offset/Address/Size: 0xC | 0x801D2908 | size: 0xC
 */
void nlTaskManager::SetNextState(unsigned int nextState)
{
    m_pInstance->m_PendingState = nextState;
}

/**
 * Offset/Address/Size: 0x18 | 0x801D2914 | size: 0x150
 */
void nlTaskManager::RunAllTasks()
{
    f32 tickerDifference;
    f32 deltaTime;
    f32 clampedDeltaTime;
    nlTask* currentTask;
    nlTask* taskIterator;
    s32 currentTicker;
    const bool profileTasks = TaskProfileEnabled();
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    static port::SimulationFrameClock simulationClock;
    const bool realtime = !PortFixedTimestep();
    if (!realtime) simulationClock.reset();
    const float frameDelta = realtime ? simulationClock.sample(port_monotonic_ns()) : 0.f;
#endif

    currentTask = nlDLRingGetStart<nlTask>(m_pInstance->m_lTaskList);
    if (currentTask != NULL)
    {
        if ((u32)m_pInstance->m_CurrState != (u32)m_pInstance->m_PendingState)
        {
            for (;;)
            {
                PortProfilerTaskTransitionBegin(currentTask);
                currentTask->StateTransition(m_pInstance->m_CurrState, m_pInstance->m_PendingState);
                PortProfilerTaskTransitionEnd();
                if (nlDLRingIsEnd<nlTask>(m_pInstance->m_lTaskList, currentTask) != 0)
                    break;
                currentTask = currentTask->m_next;
            }
            m_pInstance->m_PrevState = (u32)m_pInstance->m_CurrState;
            m_pInstance->m_CurrState = (u32)m_pInstance->m_PendingState;
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
            // Synchronous state changes may load a scene. Exclude that work
            // from the next animation/physics interval, including warm loads
            // shorter than the suspend threshold. Rendering hitches still
            // contribute their full time on ordinary passes.
            if (realtime) (void)simulationClock.sample(port_monotonic_ns());
#endif
        }

        taskIterator = nlDLRingGetStart<nlTask>(m_pInstance->m_lTaskList);
        // PORT: tasks step by whole display periods while vsync paces the frame; when the clock changes, each steps by the clock it last recorded.
        u32 frameTicker = 0;
        const int clock = PortTaskClockFrame(&frameTicker);
        const bool stepByDisplay = clock == PORT_TASK_CLOCK_DISPLAY || clock == PORT_TASK_CLOCK_LEAVING;
        const bool recordDisplay = clock == PORT_TASK_CLOCK_DISPLAY || clock == PORT_TASK_CLOCK_ENTERING;
        for (;;)
        {
            const s32 hostTicker = nlGetTicker();
            currentTicker = stepByDisplay ? (s32)frameTicker : hostTicker;
            tickerDifference = nlGetTickerDifference(taskIterator->nPrevTicker, currentTicker);
            taskIterator->nPrevTicker = recordDisplay ? (s32)frameTicker : hostTicker;
            if (taskIterator->statesActive & m_pInstance->m_CurrState)
            {
                clampedDeltaTime = tickerDifference / 1000.f;
                if (clampedDeltaTime < g_fTaskTimeLowerBound)
                {
                    clampedDeltaTime = g_fTaskTimeLowerBound;
                }
                else if (clampedDeltaTime > g_fTaskTimeUpperBound)
                {
                    clampedDeltaTime = g_fTaskTimeUpperBound;
                }
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
                deltaTime = (realtime ? frameDelta : clampedDeltaTime) * m_pInstance->m_TimeDilation;
#else
                deltaTime = clampedDeltaTime * m_pInstance->m_TimeDilation;
#endif
                m_pInstance->m_fCurrentTimeDelta = deltaTime;
                if (profileTasks)
                {
                    const unsigned long long started = port_monotonic_ns();
                    PortProfilerTaskRunBegin(taskIterator);
                    taskIterator->Run(deltaTime);
                    PortProfilerTaskRunEnd();
                    const unsigned long long elapsed = port_monotonic_ns() - started;
                    TaskProfileSlot* slot = TaskProfileGet(taskIterator);
                    if (slot != nullptr)
                    {
                        slot->totalNs += elapsed;
                        if (elapsed > slot->maxNs)
                            slot->maxNs = elapsed;
                        slot->calls++;
                    }
                }
                else
                {
                    PortProfilerTaskRunBegin(taskIterator);
                    taskIterator->Run(deltaTime);
                    PortProfilerTaskRunEnd();
                }
            }
            if (taskIterator == m_pInstance->m_lTaskList)
                break;
            taskIterator = taskIterator->m_next;
        }
        if (profileTasks && ++s_TaskProfileFrames >= s_TaskProfileWindow)
            TaskProfileReport();
    }
}

/**
 * Offset/Address/Size: 0x168 | 0x801D2A64 | size: 0xC4
 */
void nlTaskManager::AddTask(nlTask* task, unsigned int priority, unsigned int statesActive)
{
    task->nPriority = priority;
    task->statesActive = statesActive;
    // PORT: the clock RunAllTasks recorded this frame, so a task added mid-frame starts on it.
    u32 ticker;
    if (!PortTaskClockCurrent(&ticker))
        ticker = nlGetTicker();
    task->nPrevTicker = ticker;

    if (m_pInstance->m_lTaskList == nullptr)
    {
        nlDLRingAddStart<nlTask>(&m_pInstance->m_lTaskList, task);
        return;
    }

    // Find the appropriate position to insert the task based on priority
    nlTask* currentTask = nlDLRingGetStart<nlTask>(m_pInstance->m_lTaskList);
    while (currentTask != nullptr)
    {
        if (currentTask->nPriority >= priority)
        {
            currentTask = currentTask->m_prev;
            break;
        }
        else if (!nlDLRingIsEnd<nlTask>(m_pInstance->m_lTaskList, currentTask))
        {
            currentTask = currentTask->m_next;
        }
        else
        {
            break;
        }
    }

    nlDLRingInsert<nlTask>(&m_pInstance->m_lTaskList, currentTask, task);
}

/**
 * Offset/Address/Size: 0x22C | 0x801D2B28 | size: 0x74
 */
void nlTaskManager::Startup(unsigned int initialState)
{
    m_pInstance = new (8, false) nlTaskManager;
    m_pInstance->m_PrevState = initialState;
    m_pInstance->m_CurrState = initialState;
    m_pInstance->m_PendingState = initialState;
    m_pInstance->m_lTaskList = nullptr; // Initialize task ring head to null
    m_pInstance->m_TimeDilation = 1.0f;
    m_pInstance->m_Locked = 0;
}
