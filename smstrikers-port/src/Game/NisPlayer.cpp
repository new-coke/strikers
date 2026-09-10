#include "Game/NisPlayer.h"
#include <stdlib.h>
#include "Game/Camera/CameraMan.h"
#include "Game/Character.h"
#include "Game/CharacterTemplate.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/Goalie.h"
#include "Game/ReplayManager.h"
#include "NL/nlConfig.h"
#include "NL/nlDebug.h"
#include "NL/nlFileGC.h"
#include "NL/nlFormat.h"
#include "NL/nlMemory.h"
#include "NL/nlTask.h"
#include "NL/nlString.h"
#include "string.h"

extern unsigned long cupTrophyHash;
extern "C" int sscanf(const char*, const char*, ...);

namespace
{
static unsigned char useAsyncLoading = true;
static char kNisEmpty[] = "";
} // namespace

#include "NisPlayer_interp.cpp"

/**
 * Offset/Address/Size: 0x3990 | 0x8011866C | size: 0x58
 */
NisPlayer* NisPlayer::Instance()
{
    static NisPlayer instance;
    return &instance;
}

namespace
{
static void* byteCode;
}

bool g_ForceDoubleBallTransition;

static inline void SkipLine(const char*& data)
{
    while (*data != '\n')
        data++;
    while (*data == '\n')
        data++;
}

/**
 * Offset/Address/Size: 0x3408 | 0x801180E4 | size: 0x588
 */
NisPlayer::NisPlayer()
    : InterpreterCore(10)
    , mActive(false)
    , mDictSize(0)
    , mMaxNumBallsVisible(1)
    , mLoadingFromBack(false)
    , mUsedFromFront(0)
    , mUsedFromBack(0x70800)
    , mGoalScorerCharIndex(-1)
{
    if (useAsyncLoading && !(OSGetConsoleType() & 0x20000000))
        mMemory = (char*)nlVirtualAlloc(0x70800, false);
    else
        mMemory = (char*)nlMalloc(0x70800);

    for (int i = 0; i < 4; i++)
    {
        mLoadQueue[i] = NULL;
        mPlaying[i] = NULL;
        mLoaded[i] = NULL;
        mAsyncStarted[i] = false;
    }

    unsigned long size = 0;
    char* data = (char*)nlLoadEntireFile("art/nis/nis_dict.txt", &size, 0x20, AllocateStart);
    if (data != NULL)
    {
        mDictSize = 0;
        const char* dictionaryCursor = data;
        while (mDictSize < 256)
        {
            NisHeader& header = mDict[mDictSize];
            if (sscanf(dictionaryCursor, "name %s", header.name) != 1)
                break;
            SkipLine(dictionaryCursor);
            if (sscanf(dictionaryCursor, "\tsize %d", &header.size) != 1)
                break;
            SkipLine(dictionaryCursor);
            if (sscanf(dictionaryCursor, "\thas_ball %d", &header.numBalls) != 1)
                break;
            SkipLine(dictionaryCursor);
            if (sscanf(dictionaryCursor, "\tnum_animations %d", &header.numAnimations) != 1)
                break;
            SkipLine(dictionaryCursor);
            if (sscanf(dictionaryCursor, "\tnum_cameras %d", &header.numCameras) != 1)
                break;
            SkipLine(dictionaryCursor);
            if (sscanf(dictionaryCursor, "\tcenter %f, %f, %f", &header.center.x, &header.center.y, &header.center.z) != 3)
                break;
            SkipLine(dictionaryCursor);
            if (sscanf(dictionaryCursor, "\tmin_bounds %f, %f, %f", &header.minBounds.x, &header.minBounds.y, &header.minBounds.z) != 3)
                break;
            SkipLine(dictionaryCursor);
            if (sscanf(dictionaryCursor, "\tmax_bounds %f, %f, %f", &header.maxBounds.x, &header.maxBounds.y, &header.maxBounds.z) != 3)
                break;
            SkipLine(dictionaryCursor);

            char* (*toLower)(char*) = nlToLower<char>;
            toLower(header.name);

            for (int i = 0; i < header.numAnimations; i++)
            {
                nlVector3 beginPos = { { 0, 0, 0 } };
                int parsedValueCount = sscanf(dictionaryCursor, "\tbegin_pos %f, %f, %f", &beginPos.x, &beginPos.y, &beginPos.z);
                if (parsedValueCount != 3)
                    break;
                SkipLine(dictionaryCursor);
                header.beginPositions[i] = beginPos;
            }
            mDictSize++;
        }
        nlFree(data);
    }

    Reset();

    unsigned long fileSize = 0;
    byteCode = nlLoadEntireFile("art/presentation/nis_triggers.byte_code", &fileSize, 0x20, AllocateStart);
    LoadByteCode(byteCode);
    mCamera.m_LetManagerDoUpdate = false;
    mCamera.m_bCyclic = false;
}

/**
 * Offset/Address/Size: 0x3358 | 0x80118034 | size: 0xB0
 */
float NisPlayer::TimeLeft() const
{
    if (mPlaying[0] != NULL)
    {
        if (strstr(mPlaying[0]->Name(), "trophy") != NULL)
        {
            return 1.0f;
        }
    }

    cCameraData* pCameraData = mCamera.m_pActiveCameraData;
    if (pCameraData != NULL)
    {
        float animTime = mCamera.m_fAnimationTime;
        float duration;
        if (pCameraData != NULL)
        {
            duration = (float)(pCameraData->m_uKeyCount - 1) / 30.0f;
        }
        else
        {
            duration = 0.0f;
        }

        if (getenv("STRIKERS_LOG_NIS") != NULL)
        {
            static int nNisLog = 0;
            if ((nNisLog++ % 60) == 0)
            {
                OSReport("[nis] %s keys=%d animT=%.4f dur=%.2f left=%.2f\n",
                         mPlaying[0] != NULL ? mPlaying[0]->Name() : "(none)",
                         (int)pCameraData->m_uKeyCount, (double)animTime,
                         (double)duration,
                         (double)((1.0f - animTime) * duration));
            }
        }
        return (1.0f - animTime) * duration;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x327C | 0x80117F58 | size: 0xDC
 */
bool NisPlayer::WorldIsFrozen() const
{
    bool stateOK = (nlTaskManager::m_pInstance->m_CurrState == 0x100);
    if (stateOK)
    {
        return TimeLeft() == 0.0f;
    }
    return stateOK;
}

/**
 * Offset/Address/Size: 0x2F64 | 0x80117C40 | size: 0x318
 */
void NisPlayer::HandleAsyncs()
{
    for (int i = 0; i < 4; i++)
    {
        if (mLoadQueue[i] != 0)
        {
            if (!mAsyncStarted[i])
            {
                mAsyncStarted[i] = 1;

                if (mLoadingFromBack)
                {
                    mUsedFromBack -= mLoadQueue[i]->size;
                    mUsedFromBack -= 0x20;
                }

                int memoryOffset;
                if (mLoadingFromBack)
                {
                    memoryOffset = mUsedFromBack;
                }
                else
                {
                    memoryOffset = mUsedFromFront;
                }

                char* loadAt = mMemory + memoryOffset;
                loadAt = loadAt + (0x20 - ((uintptr_t)loadAt & 0x1F));

                if (!mLoadingFromBack)
                {
                    mUsedFromFront += mLoadQueue[i]->size;
                    mUsedFromFront += 0x20;
                }

                if (mUsedFromFront >= mUsedFromBack)
                {
                    nlBreak();
                }

                BasicString<char, Detail::TempStringAllocator> fileName("art/nis/");
                fileName.AppendInPlace(mLoadQueue[i]->name);

                if (useAsyncLoading)
                {
                    nlFile* file = nlOpen(fileName.c_str());
                    if ((OSGetConsoleType() & 0x20000000) != 0)
                    {
                        nlReadAsync(file, loadAt, mLoadQueue[i]->size, AsyncLoad, (uintptr_t)mLoadQueue[i]);
                    }
                    else
                    {
                        nlAsyncLoadFileToVirtualMemory(file, mLoadQueue[i]->size, loadAt, AsyncLoad, (uintptr_t)mLoadQueue[i]);
                    }
                }
                else
                {
                    int size = 0;
                    nlLoadEntireFileToVirtualMemory(fileName.c_str(), &size, 0x2000, loadAt, AllocateStart);
                    AsyncLoad(0, loadAt + mLoadQueue[i]->size, mLoadQueue[i]->size, (uintptr_t)mLoadQueue[i]);
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x2DA4 | 0x80117A80 | size: 0x1C0
 */
void NisPlayer::Update(float deltaT)
{
    HandleAsyncs();

    for (int i = 0; i < 4; i++)
    {
        if (mLoadQueue[i] != NULL)
        {
            mLoadQueue[i]->mTime += deltaT;
        }
    }

    if (nlTaskManager::m_pInstance->m_CurrState == 0x100)
    {
        float animTime = mCamera.m_fAnimationTime;
        if (mCamera.m_pActiveCameraData != NULL)
        {
            mCamera.ManualUpdate(deltaT);
        }

        for (int i = 0; i < 4; i++)
        {
            if (mPlaying[i] == NULL)
                continue;

            mPlaying[i]->Update(deltaT);

            float duration;
            if (mCamera.m_pActiveCameraData != NULL)
            {
                duration = (float)(mCamera.m_pActiveCameraData->m_uKeyCount - 1) / 30.0f;
            }
            else
            {
                duration = 0.0f;
            }

            mPlaying[i]->UpdateTriggers(animTime, mCamera.m_fAnimationTime, duration);

            mCamera.m_OffsetPos = mPlaying[i]->Offset();
        }
    }
}

/**
 * Offset/Address/Size: 0x2CCC | 0x801179A8 | size: 0xD8
 */
void NisPlayer::Reset()
{
    if (!mActive)
    {
        return;
    }

    for (int i = 0; i < 4; i++)
    {
        delete mPlaying[i];
        delete mLoaded[i];
        mPlaying[i] = NULL;
        mLoaded[i] = NULL;
        mLoadQueue[i] = NULL;
        mAsyncStarted[i] = false;
    }

    mActive = false;
    mLoadingFromBack = false;
    mUsedFromFront = 0;
    mUsedFromBack = 0x70800;
    mCamera.UnselectCameraAnimation();
    cCameraManager::Remove(mCamera);
}

/**
 * Offset/Address/Size: 0x2AD4 | 0x801177B0 | size: 0x1F8
 */
bool NisPlayer::Play()
{
    int i;
    mActive = true;

    for (i = 0; i < 4; i++)
    {
        if (mLoadQueue[i] != NULL)
        {
            if (mLoadQueue[i]->mTime > 10.0f)
            {
                mLoadQueue[i] = NULL;
            }
            else
            {
                return false;
            }
        }
    }

    for (int j = 0; j < 4; j++)
    {
        delete mPlaying[j];
        mPlaying[j] = mLoaded[j];
        mLoaded[j] = NULL;
    }

    EmissionManager::Destroy( reinterpret_cast<uintptr_t>(this), NULL);

    if (nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack) != (cBaseCamera*)&mCamera)
    {
        cCameraManager::Remove(mCamera);
        cCameraManager::PushCamera(&mCamera);
    }

    for (i = 0; i < 4; i++)
    {
        if (mPlaying[i] != NULL)
        {
            if (mPlaying[i]->SelectRandomCamera(mCamera))
            {
                break;
            }
        }
    }

    if (mLoadingFromBack)
    {
        mLoadingFromBack = false;
        mUsedFromFront = 0;
    }
    else
    {
        mLoadingFromBack = true;
        mUsedFromBack = 0x70800;
    }

    return true;
}

static inline void HideAllActors()
{
    RenderSnapshot& snapshot = ReplayManager::Instance()->GetMutableRenderSnapshot();

    for (int i = 0; i < 150; i++)
    {
        snapshot.mPowerups[i].mVisible = false;
    }

    for (int i = 0; i < 10; i++)
    {
        snapshot.mCharacters[i].mVisible = false;
    }
    snapshot.mBall.mVisible = false;
}

/**
 * Offset/Address/Size: 0x28F4 | 0x801175D0 | size: 0x1E0
 */
void NisPlayer::Render() const
{
    int i;
    nlTaskManager* taskManager = nlTaskManager::m_pInstance;
    unsigned long currentState = taskManager->m_CurrState;

    if (currentState != 0x100 || ((taskManager->m_PrevState == 0x100) && (currentState != 1)))
    {
        return;
    }

    HideAllActors();

    for (i = 0; i < 4; i++)
    {
        if (mPlaying[i] != NULL)
        {
            mPlaying[i]->Render();
        }
    }
}

/**
 * Offset/Address/Size: 0x27E8 | 0x801174C4 | size: 0x10C
 */
void NisPlayer::Load(char* buffer, unsigned int size, NisHeader& nisHeader)
{
    FORCE_DONT_INLINE;
    if (!mActive)
        return;

    for (int i = 0; i < 4; i++)
    {
        if (mLoaded[i] != NULL)
            continue;

        int j;
        for (j = 0; j < 4; j++)
        {
            if (&nisHeader == mLoadQueue[j])
            {
                mLoadQueue[j] = NULL;
                mAsyncStarted[j] = false;
                break;
            }
        }

        if (j >= 4)
            continue;

        Nis* nis = new (nlMalloc(sizeof(Nis), 8, false)) Nis(nisHeader, buffer, size);
        mLoaded[i] = nis;
        LoadTriggers(*mLoaded[i]);
        return;
    }
}

/**
 * Offset/Address/Size: 0x1AE0 | 0x801167BC | size: 0xD08
 */
void NisPlayer::LoadTriggers(Nis& nis)
{
    BasicString<char, Detail::TempStringAllocator> name(nis.Name());

    for (int i = name.size() - 1; i >= 0; --i)
    {
        if (name[i] == '.')
        {
            name[i] = '\0';
            break;
        }
    }

    unsigned long nisHash = nlStringHash(name.c_str());
    if (!FunctionExists(nisHash))
    {
        for (int i = 0; i < name.size(); ++i)
        {
            if (name[i] == '_')
            {
                name.erase(name.begin(), name.begin() + i);

                BasicString<char, Detail::TempStringAllocator> all("all");
                char* insertAt = name.begin();
                BasicString<char, Detail::TempStringAllocator>::Data* sourceData = all.mData;
                const char* insertBegin;
                if (sourceData)
                {
                    insertBegin = sourceData->mData.mData;
                }
                else
                {
                    insertBegin = 0;
                }
                name.insert(
                    insertAt,
                    insertBegin,
                    sourceData ? sourceData->mData.mData + sourceData->mData.mSize - 1 : 0);
                break;
            }
        }

        nisHash = nlStringHash(name.c_str());
        if (!FunctionExists(nisHash))
        {
            return;
        }
    }

    mNisForTriggerLoading = &nis;
    CallFunction(nisHash);
    mNisForTriggerLoading = NULL;
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x1A48 | 0x80116724 | size: 0x98
 */
void NisPlayer::AsyncLoad(nlFile* file, void* buffer, unsigned int size, uintptr_t param)
{
    if (file != NULL)
    {
        nlClose(file);
    }

    Instance()->Load((char*)buffer - size, size, *(NisHeader*)param);
}
#pragma dont_inline reset

static inline const char* GetStadiumFilterName(eStadiumID stadium)
{
    if (stadium == STAD_PEACH_TOAD_STADIUM)
    {
        return "the_palace";
    }
    if (stadium == STAD_MARIO_STADIUM)
    {
        return "pipeline_central";
    }
    if (stadium == STAD_WARIO_STADIUM)
    {
        return "wario_stadium";
    }
    if (stadium == STAD_DK_DAISY)
    {
        return "dk_daisy";
    }
    if (stadium == STAD_YOSHI_STADIUM)
    {
        return "yoshi_stadium";
    }
    if (stadium == STAD_SUPER_STADIUM)
    {
        return "super_stadium";
    }
    if (stadium == STAD_FORBIDDEN_DOME)
    {
        return "forbidden_dome";
    }
    return kNisEmpty;
}

static inline char* GetNisStadiumName()
{
    eStadiumID id = GameInfoManager::Instance()->GetStadium();
    if (id == STAD_PEACH_TOAD_STADIUM)
    {
        return "the_palace";
    }
    if (id == STAD_MARIO_STADIUM)
    {
        return "pipeline_central";
    }
    if (id == STAD_WARIO_STADIUM)
    {
        return "wario_stadium";
    }
    if (id == STAD_DK_DAISY)
    {
        return "dk_daisy";
    }
    if (id == STAD_YOSHI_STADIUM)
    {
        return "yoshi_stadium";
    }
    if (id == STAD_SUPER_STADIUM)
    {
        return "super_stadium";
    }
    if (id == STAD_FORBIDDEN_DOME)
    {
        return "forbidden_dome";
    }
    return kNisEmpty;
}

/**
 * Offset/Address/Size: 0xFAC | 0x80115C88 | size: 0xA9C
 */
BasicString<char, Detail::TempStringAllocator> NisPlayer::GetTargetFilter(NisTarget target, NisWinnerType winnerType) const
{
    if (target == NIS_TARGET_STADIUM)
    {
        eStadiumID stadium = nlSingleton<GameInfoManager>::Instance()->GetStadium();
        const char* stadiumName = GetStadiumFilterName(stadium);
        return BasicString<char, Detail::TempStringAllocator>(stadiumName);
    }

    if (target == NIS_TARGET_HOME_CAPTAIN)
    {
        return BasicString<char, Detail::TempStringAllocator>(GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam(0)));
    }

    if (target == NIS_TARGET_AWAY_CAPTAIN)
    {
        return BasicString<char, Detail::TempStringAllocator>(GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam(1)));
    }

    if (target == NIS_TARGET_HOME_SIDEKICK)
    {
        return BasicString<char, Detail::TempStringAllocator>(GetSidekickName(nlSingleton<GameInfoManager>::Instance()->GetSidekick(0)));
    }

    if (target == NIS_TARGET_AWAY_SIDEKICK)
    {
        return BasicString<char, Detail::TempStringAllocator>(GetSidekickName(nlSingleton<GameInfoManager>::Instance()->GetSidekick(1)));
    }

    if (target == NIS_TARGET_WINNER_SIDEKICK)
    {
        return BasicString<char, Detail::TempStringAllocator>(GetSidekickName(nlSingleton<GameInfoManager>::Instance()->GetSidekick((short)mWinnerSide[winnerType])));
    }

    if (target == NIS_TARGET_LOSER_SIDEKICK)
    {
        int side = (mWinnerSide[winnerType] + 1) % 2;
        return BasicString<char, Detail::TempStringAllocator>(GetSidekickName(nlSingleton<GameInfoManager>::Instance()->GetSidekick((short)side)));
    }

    if (target == NIS_TARGET_WINNER_CAPTAIN)
    {
        return BasicString<char, Detail::TempStringAllocator>(GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((short)mWinnerSide[winnerType])));
    }

    if (target == NIS_TARGET_LOSER_CAPTAIN)
    {
        int side = (mWinnerSide[winnerType] + 1) % 2;

        return BasicString<char, Detail::TempStringAllocator>(GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((short)side)));
    }

    if (target == NIS_TARGET_HOME_GOALIE || target == NIS_TARGET_AWAY_GOALIE || target == NIS_TARGET_WINNER_GOALIE || target == NIS_TARGET_LOSER_GOALIE)
    {
        return BasicString<char, Detail::TempStringAllocator>("goalie");
    }

    if (target == NIS_TARGET_AWAY_SIDEKICK)
    {
        return BasicString<char, Detail::TempStringAllocator>(GetSidekickName(nlSingleton<GameInfoManager>::Instance()->GetSidekick(1)));
    }

    return BasicString<char, Detail::TempStringAllocator>(kNisEmpty);
}

/**
 * Offset/Address/Size: 0x610 | 0x801152EC | size: 0x99C
 */
void NisPlayer::Load(const char* nisType, NisTarget target, NisUseStadiumOffset useStadiumOffset, NisUseFilter useFilter, NisWinnerType winnerType)
{
    mActive = true;

    BasicString<char, Detail::TempStringAllocator> filter = GetTargetFilter(target, winnerType);

    if (filter == "myst_sidekick" && strstr(nisType, "goal_winner") != NULL)
    {
        filter = BasicString<char, Detail::TempStringAllocator>("mystery");
    }

    if (nlStrCmp(nisType, "trophy") == 0 && cupTrophyHash == 0)
    {
        return;
    }

    int numAvailableNis = 0;
    NisHeader* availableNis[10] = { 0 };

    int dictionaryIndex;
    for (dictionaryIndex = 0; dictionaryIndex < mDictSize && numAvailableNis < 10; dictionaryIndex++)
    {
        if (strstr(mDict[dictionaryIndex].name, nisType) == NULL)
        {
            continue;
        }

        int filterLengthMinusNull = (filter.mData != NULL) ? (filter.mData->mData.mSize - 1) : 0;
        if (filterLengthMinusNull != 0)
        {
            const char* filterStr = filter.c_str();
            if (mDict[dictionaryIndex].name != strstr(mDict[dictionaryIndex].name, filterStr))
            {
                continue;
            }
        }

        if (useFilter != NIS_NO_FILTER && nlStrLen(mExtraNameFilter) != 0 && strstr(mDict[dictionaryIndex].name, mExtraNameFilter) == NULL)
        {
            continue;
        }

        availableNis[numAvailableNis++] = &mDict[dictionaryIndex];
    }

    if (numAvailableNis == 0)
    {
        return;
    }

    NisHeader& nisHeader = *availableNis[nlRandom(numAvailableNis, &nlDefaultSeed)];

    for (int i = 0; i < 4; i++)
    {
        if (mLoadQueue[i] != NULL)
        {
            continue;
        }

        nisHeader.target = target;
        nisHeader.winnerType = winnerType;
        nisHeader.mTime = 0.0f;
        mLoadQueue[i] = &nisHeader;

        if (useStadiumOffset == NIS_NO_STADIUM_OFFSET)
        {
            nisHeader.stadiumOffset.x = 0.0f;
            nisHeader.stadiumOffset.y = 0.0f;
            nisHeader.stadiumOffset.z = 0.0f;
        }
        else
        {
            float scale = (useStadiumOffset == NIS_AWAY_STADIUM_OFFSET) ? -1.0f : 1.0f;
            BasicString<char, Detail::TempStringAllocator> offsetConfigName = Format(BasicString<char, Detail::TempStringAllocator>("nisHeader/{0}_offset"), (const char*)GetNisStadiumName());

            float offset = GetConfigFloat(Config::Global(), offsetConfigName.c_str(), 0.0f);

            nisHeader.stadiumOffset.x = 0.0f;
            nisHeader.stadiumOffset.y = scale * offset;
            nisHeader.stadiumOffset.z = 0.0f;
        }

        bool mirrored = IsMirrored(target, nisHeader.name, winnerType);

        for (int i = 0; i < nisHeader.numAnimations; i++)
        {
            mBeginPositions[i] = nisHeader.beginPositions[i];
            if (mirrored)
            {
                mBeginPositions[i].x *= -1.0f;
            }
        }

        return;
    }
}

/**
 * Offset/Address/Size: 0x548 | 0x80115224 | size: 0xC8
 */
void NisPlayer::PlayCharacterDirection()
{
    Event* event = g_pEventManager->CreateValidEvent(7, 0x20);
    CharacterDirectionData* directionData = new (&event->m_data) CharacterDirectionData();
    directionData->home = &mBeginPositions[0];
    directionData->away = &mBeginPositions[4];
    for (int i = 0; i < 10; i++)
    {
        mBeginPositions[i].x = nlRandomf(-8.0f, 8.0f, &nlDefaultSeed);
        mBeginPositions[i].y = nlRandomf(-4.0f, 4.0f, &nlDefaultSeed);
        mBeginPositions[i].z = 0.0f;
    }
}

/**
 * Offset/Address/Size: 0x370 | 0x8011504C | size: 0x1D8
 */
void NisPlayer::EventHandler(Event* event)
{
    if (g_pGame == NULL)
    {
        return;
    }
    if (g_pGame->m_eGameState == 3)
    {
        return;
    }

    if (event->m_uEventID == 5)
    {
        GoalScoredData* goalScoredData;
        if ((s32)port_event_data_id(&event->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            goalScoredData = NULL;
        }
        else if ((s32)port_event_data_id(&event->m_data) != 0x18A)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            goalScoredData = NULL;
        }
        else
        {
            goalScoredData = (GoalScoredData*)&event->m_data;
        }

        if (goalScoredData != NULL)
        {
            if (goalScoredData->uGoalType == 6)
            {
                g_ForceDoubleBallTransition = 1;
            }

            if (!goalScoredData->pLastTouch[goalScoredData->uTeamIndex]->IsCaptain())
            {
                mGoalScorerCharIndex = GetCharacterIndex(goalScoredData->pLastTouch[goalScoredData->uTeamIndex]);
            }
        }
    }

    if (event->m_uEventID == 0xF)
    {
        GoalieSaveData* goalieSaveData;
        if ((s32)port_event_data_id(&event->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            goalieSaveData = NULL;
        }
        else if ((s32)port_event_data_id(&event->m_data) != 0x13C)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            goalieSaveData = NULL;
        }
        else
        {
            goalieSaveData = (GoalieSaveData*)&event->m_data;
        }

        if (goalieSaveData != NULL)
        {
            if (goalieSaveData->pGoalie == g_pCharacters[8])
            {
                mWinnerSide[1] = 0;
            }
            else
            {
                mWinnerSide[1] = 1;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x1F4 | 0x80114ED0 | size: 0x17C
 */
int NisPlayer::TargetToIndex(NisTarget target, int index, NisWinnerType winnerType) const
{
    if (target == NIS_TARGET_HOME_CAPTAIN)
    {
        return index;
    }
    if (target == NIS_TARGET_AWAY_CAPTAIN)
    {
        return index + 4;
    }
    if (target == NIS_TARGET_HOME_SIDEKICK)
    {
        return index + 1;
    }
    if (target == NIS_TARGET_AWAY_SIDEKICK)
    {
        return index + 5;
    }
    if (target == NIS_TARGET_HOME_GOALIE)
    {
        return 8;
    }
    if (target == NIS_TARGET_AWAY_GOALIE)
    {
        return 9;
    }
    if (target == NIS_TARGET_LOSER_SIDEKICK)
    {
        if (mWinnerSide[winnerType] == 0)
        {
            return index + 5;
        }
        return index + 1;
    }
    if (target == NIS_TARGET_WINNER_SIDEKICK)
    {
        if (mWinnerSide[winnerType] == 0)
        {
            return index + 1;
        }
        return index + 5;
    }
    if (target == NIS_TARGET_LOSER_GOALIE)
    {
        return (mWinnerSide[winnerType] == 0) ? 9 : 8;
    }
    if (target == NIS_TARGET_WINNER_GOALIE)
    {
        return (mWinnerSide[winnerType] == 0) ? 9 : 8;
    }
    if (target == NIS_TARGET_WINNER_CAPTAIN)
    {
        return (mWinnerSide[winnerType] == 0) ? 0 : 4;
    }
    if (target == NIS_TARGET_LOSER_CAPTAIN)
    {
        return (mWinnerSide[winnerType] == 0) ? 4 : 0;
    }
    return (target == NIS_TARGET_NONE) ? index : 0;
}

/**
 * Offset/Address/Size: 0xC4 | 0x80114DA0 | size: 0x130
 */
bool NisPlayer::IsMirrored(NisTarget target, const char* name, NisWinnerType winnerType) const
{
    if (target == NIS_TARGET_LOSER_CAPTAIN || target == NIS_TARGET_WINNER_CAPTAIN || target == NIS_TARGET_WINNER_SIDEKICK || target == NIS_TARGET_LOSER_GOALIE || target == NIS_TARGET_WINNER_GOALIE || target == NIS_TARGET_LOSER_SIDEKICK)
    {
        bool mirrored = true;
        if (strstr(name, "_goal_") == NULL && strstr(name, "goalie_loser") == NULL)
        {
            mirrored = false;
        }

        if (mWinnerSide[winnerType] == 0)
        {
            return mirrored;
        }
        else
        {
            return !mirrored;
        }
    }
    else
    {
        if (strstr(name, "home") != NULL || strstr(name, "run_to_center") != NULL)
        {
            if (target == NIS_TARGET_AWAY_CAPTAIN)
            {
                return true;
            }
            if (target == NIS_TARGET_AWAY_SIDEKICK)
            {
                return true;
            }
            if (target == NIS_TARGET_NONE)
            {
                return true;
            }
        }
        return false;
    }
}

void NisPlayer::Effect(float frame, const char* name, const char* target, NisEffectLifetime lifetime)
{
    Nis::TriggerParams params;
    params.float1 = -1.0f;
    params.param1 = -1;
    params.param2 = -1;
    params.param3 = -1;
    params.param4 = -1;
    params.param1 = lifetime;
    mNisForTriggerLoading->AddTrigger(NIS_TRIGGER_TYPE_EFFECT, frame, name, target, &params);
}

/**
 * Offset/Address/Size: 0xA0 | 0x80114D7C | size: 0x24
 */
void NisPlayer::ResetEffects()
{
    EmissionManager::Destroy( reinterpret_cast<uintptr_t>(this), nullptr);
}

/**
 * Offset/Address/Size: 0x74 | 0x80114D50 | size: 0x2C
 */
void NisPlayer::SetExtraNameFilter(const char* filter)
{
    nlStrNCpy(mExtraNameFilter, filter, 128);
}

/**
 * Offset/Address/Size: 0x0 | 0x80114CDC | size: 0x74
 */
NisPlayer::~NisPlayer()
{
}
