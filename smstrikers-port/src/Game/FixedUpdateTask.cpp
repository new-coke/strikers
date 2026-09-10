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
#include "NL/platpad.h"

float g_fFixedUpdateTick = 0.02f;
extern PhysicsWorld* g_PhysicsWorld;

bool g_bRunSimAndRenderInLockStep;
float g_fSimulationTick = g_fFixedUpdateTick;

float FixedUpdateTask::mAccumulatedDeltaT;
float FixedUpdateTask::mSimulationTime;
float FixedUpdateTask::mfFrameLockTime;
float FixedUpdateTask::mTimeScale = 1.0f;

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
    if (nlTaskManager::m_pInstance->m_CurrState == 2)
    {
        float simulationTick;

        mAccumulatedDeltaT += dt * mTimeScale;

        while (g_bRunSimAndRenderInLockStep || mAccumulatedDeltaT >= g_fFixedUpdateTick)
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

            if (g_bRunSimAndRenderInLockStep)
            {
                break;
            }
        }
    }

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
