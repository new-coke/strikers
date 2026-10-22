#include "NL/nlTask.h"
#include "NL/nlMemory.h"
#include "NL/nlTicker.h"
#include "NL/nlDLRing.h"
#include "port/host.h"
#include "port/config.h"
#include "port/vita_profiler.h"
#include "port/native_watchdog.hpp"
#include "port/framerate.h" // PORT: PortTaskClockFrame
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/determinism.h"
#include "port/simulation_clock.hpp"
#include "port/task_profile.hpp"
#endif

#include <cstdio>
#include <cstdlib>


// PORT: profiling output that survives no-log builds (stderr goes nowhere on Vita).
static FILE* PortProfileOut()
{
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
                port::watchdog::task(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(currentTask)),
                                     m_pInstance->m_CurrState, m_pInstance->m_PendingState);
                port::watchdog::Scope watchdogTask(port::watchdog::Role::Frontend,
                                                    port::watchdog::Stage::TaskTransition);
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
                const bool captureTransition = port::task_profile().recording();
                const auto transitionStart = captureTransition ? port_monotonic_ns() : 0;
#endif
                PortProfilerTaskTransitionBegin(currentTask);
                currentTask->StateTransition(m_pInstance->m_CurrState, m_pInstance->m_PendingState);
                PortProfilerTaskTransitionEnd();
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
                if (captureTransition)
                {
                    const auto elapsed = port_monotonic_ns() - transitionStart;
                    port::task_profile().record(currentTask, currentTask->GetName(),
                                                port::TaskProfile::Transition, elapsed);
                }
#endif
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
                port::watchdog::task(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(taskIterator)),
                                     m_pInstance->m_CurrState, m_pInstance->m_PendingState);
                port::watchdog::Scope watchdogTask(port::watchdog::Role::Frontend,
                                                    port::watchdog::Stage::TaskRun);
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
                bool timeTask = profileTasks;
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
                timeTask = timeTask || port::task_profile().recording();
#endif
                if (timeTask)
                {
                    const unsigned long long started = port_monotonic_ns();
                    PortProfilerTaskRunBegin(taskIterator);
                    taskIterator->Run(deltaTime);
                    PortProfilerTaskRunEnd();
                    const unsigned long long elapsed = port_monotonic_ns() - started;
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
                    if (port::task_profile().recording())
                        port::task_profile().record(taskIterator, taskIterator->GetName(),
                                                    port::TaskProfile::Run, elapsed);
#endif
                    if (profileTasks)
                    {
                        TaskProfileSlot* slot = TaskProfileGet(taskIterator);
                        if (slot != nullptr)
                        {
                            slot->totalNs += elapsed;
                            if (elapsed > slot->maxNs)
                                slot->maxNs = elapsed;
                            slot->calls++;
                        }
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
    port::watchdog::task(0, m_pInstance->m_CurrState, m_pInstance->m_PendingState);
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
