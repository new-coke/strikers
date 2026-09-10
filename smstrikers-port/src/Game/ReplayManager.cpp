#include "NL/nlLexicalCast.h"
#include "NL/nlConfig.h"
#include "Game/ReplayManager.h"
#include "Game/Camera/CameraMan.h"
#include "Game/FixedUpdateTask.h"
#include "NL/nlTask.h"
#include "NL/nlMemory.h"
#include "NL/globalpad.h"
#include "PowerPC_EABI_Support/Runtime/MWCPlusLib.h"
#include "Game/main.h"
#include "NL/nlAlgorithm.h"
#include "Game/TrophyTextures.h"

extern float g_fSimulationTick;
extern float g_fFixedUpdateTick;
extern bool g_bEnableGamecubePadMonkey;

static f32 CANT_COLLIDE = HUGE_VALF;

ReplayManager::ReplayManager()
    : mCurrent(mSnapshots)
    , mPrevious(mSnapshots + 1)
    , mRender(NULL)
    , mDebugCamera(cFollowCamera::FOLLOW_SELECTABLE)
    , mEvents(0)
    , mSpeed(1.0f)
    , mSpeedUp(0.0f)
    , mDeltaTime(0.0f)
    , mTime(0.0f)
    , mReplay(NULL)
    , mMemory(NULL)
{
}

/**
 * Offset/Address/Size: 0x964 | 0x801126D4 | size: 0x8C
 */
inline ReplayManager::~ReplayManager()
{
}

/**
 * Offset/Address/Size: 0x89C | 0x8011260C | size: 0xC8
 */
ReplayManager* ReplayManager::Instance()
{
    static ReplayManager rm;
    return &rm;
}

/**
 * Offset/Address/Size: 0x828 | 0x80112598 | size: 0x74
 */
void ReplayManager::Initialize()
{
    mMemory = (u8*)nlVirtualAlloc(0x1C0000, false);
    mReplay = new (nlMalloc(sizeof(Replay), 8, false)) Replay((char*)mMemory, 0x1C0000, 0x8000);
    mTime = 0.0f;
}

/**
 * Offset/Address/Size: 0x7DC | 0x8011254C | size: 0x4C
 */
void ReplayManager::InitializeSnapshots()
{
    for (int i = 0; i < 3; i++)
    {
        mSnapshots[i].Initialize();
    }
}

/**
 * Offset/Address/Size: 0x760 | 0x801124D0 | size: 0x7C
 */
void ReplayManager::Uninitialize()
{
    nlVirtualFree(mMemory);
    mMemory = nullptr;

    for (int i = 0; i < 3; i++)
    {
        mSnapshots[i].Free();
    }

    delete mReplay;
    mReplay = nullptr;
}

/**
 * Offset/Address/Size: 0x6E0 | 0x80112450 | size: 0x80
 */
void ReplayManager::GrabSnapshot()
{
    SwapPreviousAndCurrent();

    mCurrent->Grab();

    if (nlTaskManager::m_pInstance->m_CurrState == 2)
    {
        float time = mReplay->EndTime() + g_fSimulationTick;
        mTime = time;
        mReplay->Record<RenderSnapshot>(mTime, *mCurrent, mEvents);
        mEvents = 0;
    }
}

/**
 * Offset/Address/Size: 0x6B4 | 0x80112424 | size: 0x2C
 */
RenderSnapshot& ReplayManager::GetMutableRenderSnapshot()
{
    mRender = mCurrent;
    return mRender->GetMutable();
}

void ReplayManager::SwapPreviousAndCurrent()
{
    RenderSnapshot* tmp = mCurrent;
    mCurrent = mPrevious;
    mPrevious = tmp;
}

/**
 * Offset/Address/Size: 0x5C4 | 0x80112334 | size: 0xF0
 */
void ReplayManager::Flush()
{
    delete mReplay;
    mReplay = new (nlMalloc(sizeof(Replay), 8, false)) Replay((char*)mMemory, 0x1C0000, 0x8000);

    ResetSnapshots();
}

void ReplayManager::DoPotentialDebugReplay(float& deltaTime)
{
if (!g_bEnableGamecubePadMonkey)
{
    static bool debugReplay = GetConfigBool(Config::Global(), "debug_replay_in_release", false);

    if (debugReplay && !g_bTweaking && !g_bProfiling)
    {
        if (cPadManager::GetPad(0)->JustPressed(4, true))
        {
            if (nlTaskManager::m_pInstance->m_CurrState == 0x20000)
            {
                nlTaskManager::SetNextState(2);
            }
            else if (nlTaskManager::m_pInstance->m_CurrState == 2)
            {
                mTime = mReplay->EndTime();
                nlTaskManager::SetNextState(0x20000);
            }
        }
    }

    if (nlTaskManager::m_pInstance->m_CurrState == 0x20000)
    {
        if (!cCameraManager::HasCamera(&mDebugCamera))
        {
            cCameraManager::PushCamera(&mDebugCamera);
        }

        mDeltaTime = 0.0f;
        mDeltaTime -= 0.02f * cPadManager::GetPad(0)->GetPressure(5, true);
        mDeltaTime += 0.02f * cPadManager::GetPad(0)->GetPressure(6, true);

        f32 time = mTime + mDeltaTime;
        if (time < mReplay->BeginTime())
        {
            time = mReplay->BeginTime();
        }
        if (time > mReplay->EndTime())
        {
            time = mReplay->EndTime();
        }

        mDeltaTime = time - mTime;
        mTime = time;
        mReplay->Play<RenderSnapshot>(mTime, *mPrevious, *mCurrent, mBlend);
    }
}
}

void ReplayManager::DoPotentialAutoReplay(float deltaTime)
{
    if (nlTaskManager::m_pInstance->m_CurrState == 0x10)
    {
        mSpeed = mSpeedUp * deltaTime + mSpeed;
        if (mSpeed < 0.1f)
        {
            mSpeed = 0.1f;
        }
        mDeltaTime = mSpeed * deltaTime;
        mTime = mTime + mDeltaTime;
        mReplay->Play<RenderSnapshot>(mTime, *mPrevious, *mCurrent, mBlend);
    }
}

/**
 * Offset/Address/Size: 0x514 | 0x80112284 | size: 0xB0
 */
void ReplayManager::ResetSnapshots()
{
    for (int i = 0; i < 3; i++)
    {
        mSnapshots[i].Invalidate();
    }

    GrabSnapshot();
}

/**
 * Offset/Address/Size: 0x4C0 | 0x80112230 | size: 0x54
 */
void ReplayManager::PrepareForRecording()
{
    cCameraManager::Remove(mDebugCamera);
    mTime = mReplay->EndTime();
    mPrevious->Invalidate();
    mCurrent->Invalidate();
    mRender = nullptr;
}

/**
 * Offset/Address/Size: 0x454 | 0x801121C4 | size: 0x6C
 */
void ReplayManager::SetCurrentTime(float time)
{
    mTime = time;

    if (mTime < mReplay->BeginTime())
    {
        mTime = mReplay->BeginTime();
    }

    if (mTime > mReplay->EndTime())
    {
        mTime = mReplay->EndTime();
    }
}

/**
 * Offset/Address/Size: 0x3C0 | 0x80112130 | size: 0x94
 */
void ReplayManager::EventHandler(Event* event)
{
    if (event->m_uEventID == 0xD)
    {
        mEvents |= 4;
    }
    if (event->m_uEventID == 0x14)
    {
        mEvents |= 2;
    }
    if (event->m_uEventID == 0xE)
    {
        mEvents |= 8;
    }
    if (event->m_uEventID == 5)
    {
        mEvents |= 1;
    }
    if (event->m_uEventID == 0xF)
    {
        mEvents |= 0x16;
    }
    if (event->m_uEventID == 0xF)
    {
        mEvents |= 1;
    }
}

static bool NisOverridesReplayBuffer()
{
    return (nlTaskManager::m_pInstance->m_CurrState == 0x100) || (nlTaskManager::m_pInstance->m_PrevState == 0x100 && nlTaskManager::m_pInstance->m_CurrState == 1);
}

/**
 * Offset/Address/Size: 0x0 | 0x80111D70 | size: 0x3C0
 */
void ReplayManager::RenderSnapshotAt(float deltaTime)
{
    mBlend[0] = FixedUpdateTask::mAccumulatedDeltaT / g_fFixedUpdateTick;
    mBlend[1] = FixedUpdateTask::mAccumulatedDeltaT / g_fFixedUpdateTick;
    mBlend[2] = FixedUpdateTask::mAccumulatedDeltaT / g_fFixedUpdateTick;
    mDeltaTime = FixedUpdateTask::mAccumulatedDeltaT;

    DoPotentialDebugReplay(deltaTime);

    DoPotentialAutoReplay(deltaTime);

    mRender = mCurrent;

    bool transitioning = NisOverridesReplayBuffer();

    if (!transitioning && mPrevious->mValid)
    {
        Blend<RenderSnapshot>(mBlend, *mPrevious, *mCurrent, mSnapshots[2]);
        mRender = &mSnapshots[2];
    }

    mRender->Render(deltaTime);

    if (nlTaskManager::m_pInstance->m_CurrState == 0x20000)
    {
        mSnapshots[2].RenderDebugInfo(*mPrevious, *mCurrent, mBlend[0]);
    }
}
