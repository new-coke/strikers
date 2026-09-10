#include "Game/ReplayChoreo.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Game.h"
#include "Game/Render/NetMesh.h"
#include "Game/Sys/audio.h"
#include "Game/Team.h"
#include "NL/nlDebug.h"
#include "NL/nlFormat.h"
#include "NL/nlTask.h"

struct GoalScoredDataExt
{
    GoalScoredData data;
    int sideOfInterest;
};

unsigned int nlRandom(unsigned int range, unsigned int* seed);
extern unsigned int nlDefaultSeed;
double fabs(double);

namespace
{
char* replayTypeNames[9] = {
    "NORMAL_SHOT",
    "ONE_TIMER",
    "SHOOT_TO_SCORE",
    "DEFLECTION",
    "LOOSE_BALL",
    "OWN_GOAL",
    "CAPTAIN_SHOOT_TO_SCORE",
    "HIGHLIGHT",
    "HYPER_STRIKE",
};
char* zoneDepthNames[3] = { "MID", "CLOSE", "DEEP" };
char* zoneInWidthNames[3] = { "CENTER", "FRONT", "BACK" };
char scriptName[128] = "";
int cameraPick = -1;
} // namespace

#include "ReplayChoreo_interp.cpp"

/**
 * Offset/Address/Size: 0xFDC | 0x80128648 | size: 0xE8
 */
ReplayChoreo& ReplayChoreo::Instance()
{
    static ReplayChoreo instance;
    return instance;
}

ReplayChoreo::ReplayChoreo()
    : InterpreterCore(10)
    , mReplayManager(NULL)
    , mRunForTimeLeft(0.0f)
    , mRunningFor(false)
    , mByteCode(NULL)
    , mHighlightIndex(-1)
    , mNumHighlights(0)
{
    LoadScript();
}

template <typename StringType, typename T1, typename T2, typename T3, typename T4>
void Format(StringType& result, const StringType& format, const T1& value1, const T2& value2, const T3& value3, const T4& value4);

/**
 * Offset/Address/Size: 0xCC8 | 0x80128334 | size: 0x314
 */
void ReplayChoreo::LoadScript()
{
    if (mByteCode != 0)
    {
        nlFree(mByteCode);
    }

    unsigned long fileSize = 0;
    mByteCode = nlLoadEntireFile("art/presentation/replay_choreo.byte_code", &fileSize, 0x20, (eAllocType)0);
    LoadByteCode(mByteCode);

    int d;
    int w;
    int t;
    int j;

    for (d = 0; d < 3; d++)
    {
        for (w = 0; w < 3; w++)
        {
            for (t = 0; t < 9; t++)
            {
                mNumScripts[d][w][t] = 0;

                for (j = 0; j < 8; j++)
                {
                    BasicString<char, Detail::TempStringAllocator> name = Format<BasicString<char, Detail::TempStringAllocator>, const char*, const char*, const char*, int>(
                        BasicString<char, Detail::TempStringAllocator>("{0}_{1}_{2}_{3}"),
                        zoneDepthNames[d],
                        zoneInWidthNames[w],
                        replayTypeNames[t],
                        j);

                    if (FunctionExists(nlStringHash(name.c_str())))
                    {
                        mNumScripts[d][w][t] = mNumScripts[d][w][t] + 1;
                    }
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xA9C | 0x80128108 | size: 0x22C
 */
void ReplayChoreo::EventHandler(Event* event)
{
    if (!g_pGame)
        return;

    if (event->m_uEventID == 5)
    {
        GoalScoredDataExt* gsd;
        s32 id = port_event_data_id(&event->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            gsd = 0;
        }
        else
        {
            id = port_event_data_id(&event->m_data);
            if (id != 0x18A)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                gsd = 0;
            }
            else
            {
                gsd = (GoalScoredDataExt*)&event->m_data;
            }
        }

        if (gsd != 0)
        {
            mGoalScoredData = gsd->data;
            mReplayPad = gsd->sideOfInterest;

            if ((mGoalScoredData.uGoalType == 6 || mGoalScoredData.uGoalType == 2) && mGoalScoredData.uIsHyper)
            {
                mGoalScoredData.uGoalType = 8;
            }

            mCamera.SetSideOfInterest((gsd->data.uTeamIndex + 1) % 2);
        }
    }

    if (event->m_uEventID == 0xF)
    {
        GoalieSaveData* gsd;
        s32 id = port_event_data_id(&event->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            gsd = 0;
        }
        else
        {
            id = port_event_data_id(&event->m_data);
            if (id != 0x13C)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                gsd = 0;
            }
            else
            {
                gsd = (GoalieSaveData*)&event->m_data;
            }
        }

        if (gsd != 0)
        {
            if (gsd->isSTS)
                return;

            int teamSide = gsd->pGoalie->m_pTeam->m_nSide;
            mGoalScoredData.uTeamIndex = (teamSide + 1) % 2;
            mGoalScoredData.uGoalType = gsd->saveType;
            mGoalScoredData.pScorer = gsd->pShooter;
            mCamera.SetSideOfInterest(teamSide);
        }
    }
}

/**
 * Offset/Address/Size: 0xA78 | 0x801280E4 | size: 0x24
 */
void ReplayChoreo::Reset()
{
    cCameraManager::Remove(mCamera);
}

/**
 * Offset/Address/Size: 0x698 | 0x80127D04 | size: 0x3E0
 */
BasicString<char, Detail::TempStringAllocator> ReplayChoreo::CalcAutoReplayScriptName(ReplayType rt) const
{
    BasicString<char, Detail::TempStringAllocator> format("{0}_{1}_{2}_{3}");

    int zoneInWidth = 0;
    int zoneDepth = 0;
    int replayType = mGoalScoredData.uGoalType;

    // PORT: a guard; the value is wrong at its source. replayType indexes mNumScripts[..][9] and replayTypeNames[9].
    if ((unsigned int)replayType >= 9u)
    {
        static int s_reportedBadGoalType;
        if (!s_reportedBadGoalType)
        {
            s_reportedBadGoalType = 1;
            OSReport("[replay] uGoalType=%d is outside 0..8; using 0. The value "
                     "is wrong at its source.\n",
                     replayType);
        }
        replayType = 0;
    }

    f32 y = 0.5f * (2.0f * cField::mv3FieldPosition.y) + mGoalScoredData.v3ShotPosition.y;
    f32 fieldDepth = 2.0f * cField::mv3FieldPosition.y;

    if (y > 0.66f * fieldDepth)
    {
        zoneInWidth = 2;
    }
    else if (y < 0.33f * fieldDepth)
    {
        zoneInWidth = 1;
    }

    f32 negGoalLineX = -cField::GetGoalLineX((unsigned int)mGoalScoredData.uTeamIndex);

    if ((f32)fabs(negGoalLineX - mGoalScoredData.v3ShotPosition.x) < 0.33f * (f32)fabs(negGoalLineX))
    {
        zoneDepth = 1;
    }
    if ((f32)fabs(negGoalLineX - mGoalScoredData.v3ShotPosition.x) > 0.66f * (f32)fabs(negGoalLineX))
    {
        zoneDepth = 2;
    }

    while (true)
    {
        if (mNumScripts[zoneDepth][zoneInWidth][replayType] > 0)
            break;

        if (mNumScripts[zoneDepth][0][replayType] > 0)
        {
            zoneInWidth = 0;
            break;
        }
        if (mNumScripts[0][zoneInWidth][replayType] > 0)
        {
            zoneDepth = 0;
            break;
        }
        if (mNumScripts[0][0][replayType] > 0)
        {
            zoneInWidth = 0;
            zoneDepth = 0;
            break;
        }
        replayType = 0;
    }

    if (!mReplay->DidOccurInLastNumSeconds(2, 6.0f))
    {
        return BasicString<char, Detail::TempStringAllocator>("MID_CENTER_OWN_GOAL_0");
    }

    int pick = nlRandom(mNumScripts[zoneDepth][zoneInWidth][replayType], &nlDefaultSeed);
    if (cameraPick > -1)
    {
        pick = cameraPick % mNumScripts[zoneDepth][zoneInWidth][replayType];
    }

    return Format<BasicString<char, Detail::TempStringAllocator>, const char*, const char*, const char*, int>(
        format, zoneDepthNames[zoneDepth], zoneInWidthNames[zoneInWidth], replayTypeNames[replayType], pick);
}

/**
 * Offset/Address/Size: 0x428 | 0x80127A94 | size: 0x270
 */
void ReplayChoreo::StartAutoReplay(ReplayType rt)
{
    mReplayManager = ReplayManager::Instance();
    mReplay = mReplayManager->mReplay;

    NetMesh::spNegativeXNetMesh->UpdateUntilRelaxed();
    NetMesh::spPositiveXNetMesh->UpdateUntilRelaxed();

    if (!cCameraManager::HasCamera(&mCamera))
    {
        mCamera.mNoDampenForOneUpdate = true;
        cCameraManager::PushCamera(&mCamera);
    }

    if (rt == REPLAY_TYPE_HIGHLIGHT)
    {
        int reelIdx = NumHighlights();

        mNumHighlights = mNumHighlights - 1;
        if (mNumHighlights < 0)
        {
            mNumHighlights = reelIdx - 1;
        }

        do
        {
            mHighlightIndex = mHighlightIndex + 1;
            mHighlightIndex = mHighlightIndex % 3;
        } while (!mReplay->IsReelValid(mHighlightIndex + 1));

        mReplay->PlayReel(mHighlightIndex + 1);
        mCamera.SetSideOfInterest(mHighlights[mHighlightIndex].mReplayPad);

        GoalScoredDataExt* goal = (GoalScoredDataExt*)&mHighlights[mHighlightIndex].mGoalScoredData;
        mGoalScoredData = goal->data;
        mReplayPad = goal->sideOfInterest;

        rt = REPLAY_TYPE_GOAL;
    }
    else
    {
        mReplay->mReelIdx = 0;
    }

    nlStrNCpy(scriptName, CalcAutoReplayScriptName(rt).c_str(), 0x80);
    CallFunction(nlStringHash(scriptName));
}

/**
 * Offset/Address/Size: 0x408 | 0x80127A74 | size: 0x20
 */
void ReplayChoreo::FlushHighlights()
{
    mHighlightIndex = -1;
    mNumHighlights = 0;
    mHighlights[0].mSideOfInterest = 0;
    mHighlights[1].mSideOfInterest = 0;
    mHighlights[2].mSideOfInterest = 0;
}

/**
 * Offset/Address/Size: 0x390 | 0x801279FC | size: 0x78
 */
void ReplayChoreo::Update(float deltaT)
{
    if (nlTaskManager::m_pInstance->m_CurrState == 0x10)
    {
        if (mRunningFor)
        {
            mRunForTimeLeft -= deltaT;
        }
        Run();
        mCamera.ManualUpdate(deltaT);
    }
}

void ReplayChoreo::SetCamera(ReplayCameraPosition position)
{
    mCamera.CutTo(position);
}

void ReplayChoreo::FreezeCamera()
{
    mCamera.mFrozen = true;
}

void ReplayChoreo::SetCameraFov(float fov)
{
    mCamera.mFov = fov;
    mCamera.mDeltaFov = 0.0f;
}

void ReplayChoreo::SetCameraFocus(ReplayCameraFocus focus)
{
    mCamera.mFocus = focus;
}

void ReplayChoreo::AddCameraFocus(ReplayCameraFocus focus)
{
    mCamera.mFocus |= focus;
}

void ReplayChoreo::Speed(float speed)
{
    mReplayManager->mSpeed = speed;
    mReplayManager->mSpeedUp = 0.0f;
}

void ReplayChoreo::StartSpeedUp(float deltaSpeed)
{
    mReplayManager->mSpeedUp = deltaSpeed;
}

void ReplayChoreo::Rewind(ReplayEvent event, float timeOffset)
{
    mReplayManager->SetCurrentTime(timeOffset + mReplay->TimeOfLastOccurence(event));
    float endTime = mReplayManager->mReplay->EndTime();
    if (endTime - mReplayManager->mTime > 5.0f)
    {
        mReplayManager->SetCurrentTime(mReplayManager->mReplay->EndTime() - 5.0f);
    }
}

void ReplayChoreo::RunTill(ReplayEvent event, float timeOffset)
{
    if (!IsFinished())
    {
        float currentTime = mReplayManager->mTime;
        float lastOccurence = mReplay->TimeOfLastOccurence(event);
        if (currentTime < timeOffset + lastOccurence)
        {
            StopWithUndo();
        }
    }
}

/**
 * Offset/Address/Size: 0x344 | 0x801279B0 | size: 0x4C
 */
bool ReplayChoreo::Done() const
{
    bool result = false;
    if (IsFinished())
    {
        if (nlTaskManager::m_pInstance->m_CurrState == 0x10)
        {
            result = true;
        }
    }
    return result;
}

void ReplayChoreo::StartCameraZoom(float deltaFov)
{
    mCamera.mDeltaFov = deltaFov;
}

void ReplayChoreo::RunFor(float time)
{
    if (mRunningFor)
    {
        if (0.0f >= mRunForTimeLeft)
        {
            mRunForTimeLeft = 0.0f;
            mRunningFor = false;
            return;
        }
    }
    if (!mRunningFor)
    {
        mRunForTimeLeft = time;
        mRunningFor = true;
    }
    StopWithUndo();
}

/**
 * Offset/Address/Size: 0x108 | 0x80127774 | size: 0x23C
 */
void ReplayChoreo::SaveHighlight(ReplayChoreo::HighlightQuality quality)
{
    extern bool g_e3_Build;

    mReplayManager = ReplayManager::Instance();
    mReplay = mReplayManager->mReplay;

    if (g_e3_Build != 0)
    {
        return;
    }

    if (nlTaskManager::m_pInstance->m_CurrState != 2)
    {
        return;
    }

    int idx = -1;

    if (quality == HIGHLIGHT_QUALITY_GOAL_EQUALIZER)
    {
        idx = 1;
    }
    else if (quality == HIGHLIGHT_QUALITY_GOAL_INCREASE_DIFF)
    {
        idx = 2;
    }
    else
    {
        for (int i = 0; i < 3; i++)
        {
            if (!mReplay->IsReelValid(i + 1) || mHighlights[i].mSideOfInterest == 0)
            {
                idx = i;
                break;
            }
        }

        if (idx < 0)
        {
            if (mHighlights[0].mSideOfInterest < quality)
            {
                idx = 0;
            }
            else if (mHighlights[1].mSideOfInterest < quality)
            {
                idx = 1;
            }
            else if (mHighlights[2].mSideOfInterest < quality)
            {
                idx = 2;
            }
        }

        if (idx < 0)
        {
            int age = 0;
            for (int i = 0; i < 3; i++)
            {
                int dt = (int)(mReplayManager->mTime - mHighlights[i].mTime);
                if (mHighlights[i].mSideOfInterest == quality && dt > age)
                {
                    idx = i;
                    age = dt;
                }
            }
        }
    }

    if (idx >= 0)
    {
        if (mReplay->LockReel(4.0f, idx + 1, quality))
        {
            // PORT: was five stores at console offsets from `this`, with mHighlights taken to start at 0x230.
            Highlight& h = mHighlights[idx];
            h.mSideOfInterest = quality;
            h.mTime = mReplayManager->mTime;
            h.mGoalScoredData = mGoalScoredData;
            h.mSavedReplayPad = mReplayPad;
            h.mReplayPad = mCamera.mSideOfInterest;
        }
    }
}

void ReplayChoreo::PlayWorldSfx(const char* name, const char* target)
{
    Audio::PlayWorldSFXbyStr(name, 100.0f, -1.0f, false, true, NULL, NULL, NULL);
}

void ReplayChoreo::PlayWorldSfxWithVol(const char* name, float volume, const char* target)
{
    Audio::PlayWorldSFXbyStr(name, volume, -1.0f, false, true, NULL, NULL, NULL);
}

void ReplayChoreo::StopWorldSfx(const char* name)
{
    Audio::StopWorldSFXbyStr(name);
}

void ReplayChoreo::FilterOn(float fadeTime)
{
    GameTweaks* tweaks = g_pGame->m_pGameTweaks;
    Audio::FadeFilter(tweaks->fFadeFilterFreqMax, tweaks->fFadeFilterFreqMin, fadeTime, 0.0f);
}

void ReplayChoreo::FilterOff(float fadeTime)
{
    GameTweaks* tweaks = g_pGame->m_pGameTweaks;
    Audio::FadeFilter(tweaks->fFadeFilterFreqMin, tweaks->fFadeFilterFreqMax, fadeTime, 0.0f);
}

/**
 * Offset/Address/Size: 0x8C | 0x801276F8 | size: 0x7C
 */
int ReplayChoreo::NumHighlights() const
{
    mReplayManager = ReplayManager::Instance();
    int count = 0;
    mReplay = mReplayManager->mReplay;
    for (int i = 0; i < 3; i++)
    {
        if (mReplay->IsReelValid(i + 1))
        {
            count++;
        }
    }
    return count;
}

void ReplayChoreo::Finish()
{
    Reset();
    mReplayManager->mSpeed = 1.0f;
}
