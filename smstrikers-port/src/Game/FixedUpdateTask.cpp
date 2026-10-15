#include "Game/FixedUpdateTask.h"

#include "Game/Ball.h"
#include "Game/BasicStadium.h"
#include "Game/CharacterTemplate.h"
#include "Game/Field.h"
#include "Game/FlickDetection.h"
#include "Game/Game.h"
#include "Game/Physics/Physics.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Render/NetMesh.h"
#include "Game/Render/SidelineExplodable.h"
#include "Game/ReplayManager.h"
#include "Game/Team.h"
#include "Game/Player.h"
#include "NL/platpad.h"
#include "NL/gl/gl.h"
#include "port/determinism.h"
#include "port/framerate.h"
#include "port/host.h"

#include <algorithm>
#include <cstdlib>
#if defined(PORT_VITA)
#include "port/native_jobs.hpp"
#include "port/config.h"
#include "port/simulation_clock.hpp"
#include <cstdio>
#endif

float g_fFixedUpdateTick = 0.02f;
extern PhysicsWorld* g_PhysicsWorld;

bool g_bRunSimAndRenderInLockStep;
float g_fSimulationTick = g_fFixedUpdateTick;

float FixedUpdateTask::mAccumulatedDeltaT;
float FixedUpdateTask::mSimulationTime;
float FixedUpdateTask::mfFrameLockTime;
float FixedUpdateTask::mTimeScale = 1.0f;

namespace
{
#if defined(PORT_VITA)
struct PrePhysicsPoseJob
{
    bool poseLocal[10];
};

bool PreparePlayerPoses(void* opaque, size_t begin, size_t end, uint32_t) noexcept
{
    PrePhysicsPoseJob& job = *static_cast<PrePhysicsPoseJob*>(opaque);
    for (size_t i = begin; i < end; ++i)
        job.poseLocal[i] = static_cast<cPlayer*>(g_pCharacters[i])->PreparePrePhysicsPose(g_fSimulationTick);
    return true;
}

bool PortGameParallelEnabled()
{
    static const bool enabled = [] {
        const char* value = getenv("STRIKERS_VITA_GAME_PARALLEL");
        return value == NULL || value[0] != '0';
    }();
    return enabled;
}

void ReportSimulationTime(float dt, float expectedDt, unsigned steps, uint64_t now, bool active, uint64_t cpuNs)
{
    if (!PortDiagnosticsEnabled()) return;
    static uint64_t previous = 0, window = 0;
    static double wall = 0, supplied = 0, expected = 0, simulated = 0, cpu = 0;
    static unsigned frames = 0, ticks = 0, maxSteps = 0;
    static bool first = true;
    if (!active)
    {
        previous = window = 0;
        wall = supplied = expected = simulated = cpu = 0;
        frames = ticks = maxSteps = 0;
        return;
    }
    if (!previous) { previous = window = now; return; }
    wall += double(now - previous) * 1.e-9;
    previous = now;
    supplied += dt;
    expected += expectedDt;
    simulated += steps * double(g_fSimulationTick);
    cpu += double(cpuNs) * 1.e-6;
    ++frames; ticks += steps; maxSteps = std::max(maxSteps, steps);
    if (now - window < 2000000000ull) return;
    if (auto* f = std::fopen("ux0:data/strikersVita/native_simulation_timing.csv", first ? "w" : "a"))
    {
        if (first) std::fputs("wall_seconds,task_seconds,simulation_seconds,render_frames,physics_steps,max_steps_per_frame,pending_seconds,deterministic,simulation_cpu_ms,expected_simulation_seconds\n", f);
        std::fprintf(f,"%.6f,%.6f,%.6f,%u,%u,%u,%.6f,%d,%.3f,%.6f\n", wall, supplied, simulated,
            frames, ticks, maxSteps, FixedUpdateTask::mAccumulatedDeltaT, PortFixedTimestep(),cpu,expected);
        std::fclose(f); first = false;
    }
    window = now; wall = supplied = expected = simulated = cpu = 0; frames = ticks = maxSteps = 0;
}


#endif
} // namespace

extern "C" float* PortSimTimeScalePtr(void)
{
    return &FixedUpdateTask::mTimeScale;
}

/**
 * Offset/Address/Size: 0x2D8 | 0x8016E608 | size: 0x30
 */
FixedUpdateTask::FixedUpdateTask()
{
    mAccumulatedDeltaT = g_fFixedUpdateTick;
    mSimulationTime = 0.f;
    mfFrameLockTime = 0.f;
}

/**
 * Offset/Address/Size: 0x2CC | 0x8016E5FC | size: 0xC
 */
const char* FixedUpdateTask::GetName()
{
    return "Game Fixed Update";
}

/**
 * Offset/Address/Size: 0x2C4 | 0x8016E5F4 | size: 0x8
 */
float FixedUpdateTask::GetPhysicsUpdateTick()
{
    return g_fSimulationTick;
}

/**
 * Offset/Address/Size: 0x280 | 0x8016E5B0 | size: 0x44
 */
void FixedUpdateTask::DecrementFrameLock(float fDeltaT)
{
    mfFrameLockTime -= fDeltaT;
    if (mfFrameLockTime < 0.f)
    {
        nlTaskManager::SetNextState(2);
        mfFrameLockTime = 0.f;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8016E330 | size: 0x280
 */
void FixedUpdateTask::Run(float dt)
{
#if defined(PORT_VITA)
    const uint64_t simulationNow = port_monotonic_ns();
    unsigned steps = 0;
    float expectedDt = 0;
#endif
    if (nlTaskManager::m_pInstance->m_CurrState == 2)
    {
        float simulationTick;

        const float gameplayDt = dt;

        const float scaledGameplayDt = gameplayDt * mTimeScale;
        mAccumulatedDeltaT += scaledGameplayDt;
#if defined(PORT_VITA)
        expectedDt = scaledGameplayDt;
#endif

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
        // Carry any remaining debt into the next frame instead of discarding
        // a presentation or allowing an unbounded catch-up stall.
        while (mAccumulatedDeltaT >= g_fFixedUpdateTick && port::simulation_step_budget(steps))
#else
        while (g_bRunSimAndRenderInLockStep || mAccumulatedDeltaT >= g_fFixedUpdateTick)
#endif
        {
            UseFixedUpdatePad();

            UpdatePlatPad(simulationTick = g_fFixedUpdateTick);
            cPadManager::Update(simulationTick);
            FlickDetection::Update();

            mAccumulatedDeltaT -= g_fFixedUpdateTick;
            if (g_bRunSimAndRenderInLockStep)
            {
                mAccumulatedDeltaT = 0.0f;
            }

            CallFixedUpdateTasks();
#if defined(PORT_VITA)
            ++steps;
#endif

            if (g_bRunSimAndRenderInLockStep)
            {
                break;
            }
        }
    }

#if defined(PORT_VITA)
    ReportSimulationTime(dt, expectedDt, steps, simulationNow, nlTaskManager::m_pInstance->m_CurrState == 2,
        port_monotonic_ns()-simulationNow);
#endif

    UseDefaultPad();
    UpdatePlatPad(dt);
    cPadManager::Update(dt);
    FlickDetection::Update();
}

void FixedUpdateTask::AIUpdateTask(float fDeltaT)
{
    g_pGame->PreUpdate(fDeltaT);
    g_pGame->Update(fDeltaT);
}

void FixedUpdateTask::PrePhysicsAITask(float fDeltaT)
{
#if defined(PORT_VITA)
    if (PortGameParallelEnabled() && port::jobs::worker_threads() != 0)
    {
        PrePhysicsPoseJob job = {};
        (void)port::jobs::parallel_for(10, port::jobs::game_execution_lanes()>=4 ? 2 : 3, PreparePlayerPoses, &job);

        // Physics objects and the held ball are deliberately committed in the
        // original deterministic character order on CPU0.
        for (int i = 0; i < 10; ++i)
            static_cast<cPlayer*>(g_pCharacters[i])->CommitPrePhysicsUpdate(job.poseLocal[i]);
        return;
    }
#endif
    int i;
    for (i = 0; i < 10; i++)
    {
        g_pCharacters[i]->PrePhysicsUpdate(fDeltaT);
    }
}

void FixedUpdateTask::PostPhysicsAITask(float fDeltaT)
{
    int i;
    for (i = 0; i < 10; i++)
    {
        g_pCharacters[i]->PostPhysicsUpdate();
    }
    g_pBall->PostPhysicsUpdate(fDeltaT);
}

void FixedUpdateTask::CallFixedUpdateTasks()
{
    mSimulationTime += g_fSimulationTick;
    AIUpdateTask(g_fSimulationTick);
    BasicStadium::GetCurrentStadium()->mpNPCManager->UpdateAINPCs(g_fSimulationTick);
    PrePhysicsAITask(g_fSimulationTick);
    PhysicsUpdate(g_PhysicsWorld, g_fSimulationTick);
    PostPhysicsAITask(g_fSimulationTick);

    if (NetMesh::s_bAnimatedNetMeshEnabled)
    {
        bool i = true;
        float goalieX = (float)fabs(g_pTeams[0]->GetGoalie()->m_v3Position.x);
        if (goalieX > cField::GetGoalLineX(1U))
        {
        }
        else
        {
            goalieX = (float)fabs(g_pTeams[1]->GetGoalie()->m_v3Position.x);
            if (goalieX > cField::GetGoalLineX(1U))
            {
            }
            else
            {
                i = false;
            }
        }

        cBall* pBall = g_pBall;
        PhysicsAIBall* pPhysicsBall = pBall->m_pPhysicsBall;
        NetMesh::spPositiveXNetMesh->Update(g_fSimulationTick, pBall->m_v3Position, pBall->m_v3PrevPosition, i, pPhysicsBall);
        pPhysicsBall = (pBall = g_pBall)->m_pPhysicsBall;
        NetMesh::spNegativeXNetMesh->Update(g_fSimulationTick, pBall->m_v3Position, pBall->m_v3PrevPosition, i, pPhysicsBall);
    }

    SidelineExplodableManager::Update(g_fSimulationTick);
    ReplayManager::Instance()->GrabSnapshot();
}
