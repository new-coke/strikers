#include "NL/nlLexicalCast.h"
#include "Game/GameInfo.h"
#include "port/overlay.h"
#include <stdlib.h>
#include "NL/nlString.h"
#include "NL/nlDebug.h"
#include "dolphin/os.h"
#include "dolphin/types.h"
#include "NL/nlMemory.h"

#include "Game/GameSceneManager.h"
#include "Game/SH/SHMilestoneTrophy.h"
#include "Game/SH/SHCupHub.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/World/WorldLoader.h"
#include "Game/main.h"
#include "NL/nlMain.h"

// PORT: STRIKERS_UNLOCK_ALL=1, the game's own switch, defined in src/platform/debughooks.cpp.
bool PortUnlockAll();

static bool isKongaUnlocked = false;
static bool isYoshiUnlocked = false;
static bool isForbiddenUnlocked = false;
static bool isSuperStadUnlocked = false;
static bool isSuperTeamUnlocked = false;
static bool isAllSTSUnlocked = false;
static bool isTiltUnlocked = false;
static bool isGoalieUnlocked = false;
static bool isUnlimitedUnlocked = false;
static bool isShellsUnlocked = false;
static bool isEnhanceUnlocked = false;
static bool isGiantUnlocked = false;
static bool isExplosiveUnlocked = false;
static bool isFreezingUnlocked = false;

template <>
GameInfoManager* nlSingleton<GameInfoManager>::s_pInstance = 0;

// PORT: the [user] and [cup] lines a headless run reads back.
extern "C" void PortLogUserInfo(const char* tag)
{
    GameInfoManager* pInfo = nlSingleton<GameInfoManager>::s_pInstance;
    if (pInfo == NULL)
    {
        OSReport("[user] %s unavailable\n", tag);
        return;
    }

    const UserInfo& u = pInfo->mUserInfo;
    const GameplaySettings& g = u.mGameplayOptions;
    const AudioSettings& a = u.mAudioOptions;

    OSReport("[user] %s saveid=%lu games=%u goals=%u sts=%u passes=%u hits=%u "
             "trophies=%02x,%02x flower=%d star=%d powerups=%d s2s=%d bowser=%d "
             "rumble=%d skill=%d music=%d sfx=%d voice=%d audiomode=%d crc=%08lx\n",
             tag,
             (unsigned long)u.mSaveID,
             (unsigned int)u.mNumGamesPlayed, (unsigned int)u.mNumGoalsScored,
             (unsigned int)u.mNumSTSAttempts, (unsigned int)u.mNumPerfectPasses,
             (unsigned int)u.mNumHits,
             (unsigned int)u.mTrophies[0], (unsigned int)u.mTrophies[1],
             u.mIsFlowerCupUnlocked ? 1 : 0, u.mIsStarCupUnlocked ? 1 : 0,
             g.PowerUps ? 1 : 0, g.Shoot2Score ? 1 : 0,
             g.BowserAttackEnabled ? 1 : 0, g.RumbleEnabled ? 1 : 0,
             (int)g.SkillLevel,
             a.MusicVolume, a.SFXVolume, a.VoiceVolume, (int)a.Mode,
             (unsigned long)nlChecksum32(&pInfo->mUserInfo, sizeof(UserInfo)));
}

extern "C" void PortLogCupState(const char* tag)
{
    GameInfoManager* pInfo = nlSingleton<GameInfoManager>::s_pInstance;
    if (pInfo == NULL)
    {
        OSReport("[cup] %s unavailable\n", tag);
        return;
    }

    // Read straight off the cup: GetCurrentRoundNumber and GetNumPlayingTeams dereference mCurrentCup, which is NULL for a friendly or the demo.
    BaseCup* pCup = pInfo->GetCurrentCup();
    OSReport("[cup] %s mode=%d round=%d game=%d teams=%d human=%d started=%d "
             "roundjustend=%d lastresult=%d games=%u\n",
             tag,
             (int)pInfo->GetCurrentMode(),
             (pCup != NULL) ? (int)pCup->mRoundNumber : -99,
             (pCup != NULL) ? (int)pCup->mGameNumber : -99,
             (pCup != NULL) ? (int)pInfo->GetNumPlayingTeams() : -1,
             (pCup != NULL) ? (int)pCup->mHumanTeams : -1,
             (pCup != NULL && pCup->mCupStarted) ? 1 : 0,
             pInfo->mDidRoundJustEnd ? 1 : 0,
             (int)pInfo->GetResultsOfLastUserGame(),
             (unsigned int)pInfo->mUserInfo.mNumGamesPlayed);
}

bool inline CheckUnlockStatus(const bool& globalFlag, const unsigned char& trophyValue, const unsigned int bit)
{
    if (!globalFlag)
    {
        if ((PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false)))
        {
            return true;
        }
        else
        {
            return (trophyValue >> bit) & 0x01;
        }
    }
    return globalFlag;
}

static inline bool CheckUnlockStatusPickStadium(const bool& globalFlag, const unsigned char& trophyValue, const unsigned int bit)
{
    bool returnValue = globalFlag;
    if (!returnValue)
    {
        if ((PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false)))
        {
            returnValue = true;
        }
        else
        {
            returnValue = (trophyValue >> bit) & 0x01;
        }
    }
    return returnValue;
}

static inline bool CheckUnlockStatusNoGlobal(const unsigned char& trophyValue, const unsigned int bit)
{
    if ((PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false)))
    {
        return true;
    }
    else
    {
        return (trophyValue >> bit) & 0x01;
    }
}

/**
 * Offset/Address/Size: 0x9E90 | 0x8017F534 | size: 0xB84
 */
GameInfoManager::GameInfoManager()
    : mCurrentMode(GM_INVALID)
    , mDemoEnabled(true)
    , mIsInStrikers101Mode(false)
    , mGoToChooseCaptains(false)
    , mMainUserPadNumber((eFEINPUT_PAD)0)
    , mCurrentCup(NULL)
    , mPreviousCup(NULL)
    , mDoingKnockout(false)
    , mDidRoundJustEnd(false)
    , mUseCurGameSettings(false)
    , mLastHumanStadium((eStadiumID)-1)
{
    for (int i = 0; i < GM_NUM_MODES; i++)
    {
        mGameInfo[i] = NULL;
    }

    for (int i = 0; i < 8; i++)
    {
        mUserInfo.mSpoils[i].mNumRecords = 0;
        mUserInfo.mSpoils[i].mCurrentChamp = (eTeamID)-1;
        mUserInfo.mSpoils[i].mNumLosses = 0;
        mUserInfo.mSpoils[i].mNumWins = 0;
    }

    BasicGameInfo* pFriendly = new (nlMalloc(sizeof(BasicGameInfo), 8, false)) BasicGameInfo();
    mGameInfo[GM_FRIENDLY] = pFriendly;
    BasicGameInfo* pFriendlyInfo = mGameInfo[GM_FRIENDLY];
    pFriendlyInfo->mTeamIndex[0] = TEAM_MARIO;
    pFriendlyInfo->mTeamIndex[1] = TEAM_LUIGI;
    pFriendlyInfo->mSidekickIndex[0] = SK_TOAD;
    pFriendlyInfo->mSidekickIndex[1] = SK_KOOPA;
    pFriendlyInfo->mFinalScore[1] = 0;
    pFriendlyInfo->mFinalScore[0] = 0;
    pFriendlyInfo->mPadSides[0] = -1;
    pFriendlyInfo->mPadSides[1] = -1;
    pFriendlyInfo->mPadSides[2] = -1;
    pFriendlyInfo->mPadSides[3] = -1;
    pFriendlyInfo->mStadiumIndex = STAD_MARIO_STADIUM;

    BasicGameInfo* pDemo = new (nlMalloc(sizeof(BasicGameInfo), 8, false)) BasicGameInfo();
    mGameInfo[GM_DEMO] = pDemo;
    BasicGameInfo* pDemoInfo = mGameInfo[GM_DEMO];
    pDemoInfo->mTeamIndex[0] = TEAM_MARIO;
    pDemoInfo->mTeamIndex[1] = TEAM_LUIGI;
    pDemoInfo->mSidekickIndex[0] = SK_TOAD;
    pDemoInfo->mSidekickIndex[1] = SK_KOOPA;
    pDemoInfo->mFinalScore[1] = 0;
    pDemoInfo->mFinalScore[0] = 0;
    pDemoInfo->mPadSides[0] = -1;
    pDemoInfo->mPadSides[1] = -1;
    pDemoInfo->mPadSides[2] = -1;
    pDemoInfo->mPadSides[3] = -1;
    pDemoInfo->mStadiumIndex = STAD_MARIO_STADIUM;

    mMushroomCupSeries.mRoundNumber = -6;
    mFlowerCupSeries.mRoundNumber = -6;
    mStarCupSeries.mRoundNumber = -6;
    mBowserCupSeries.mRoundNumber = -6;
    mSuperMushroomCupSeries.mRoundNumber = -6;
    mSuperFlowerCupSeries.mRoundNumber = -6;
    mSuperStarCupSeries.mRoundNumber = -6;
    mSuperBowserCupSeries.mRoundNumber = -6;

    bool skipFE = GetConfigBool(Config::Global(), "skipfe", false);
    if (skipFE)
    {
        SetMode(GM_FRIENDLY);
        BasicGameInfo* skipFEInfo = mGameInfo[mCurrentMode];
        skipFEInfo->mTeamIndex[0] = TEAM_MARIO;
        skipFEInfo = mGameInfo[mCurrentMode];
        skipFEInfo->mTeamIndex[1] = TEAM_LUIGI;
        skipFEInfo = mGameInfo[mCurrentMode];
        skipFEInfo->mStadiumIndex = STAD_PEACH_TOAD_STADIUM;
        bool dontSetSides = GetConfigBool(Config::Global(), "dont_set_sides_when_skipfe", false);
        if (!dontSetSides)
        {
            if (g_pFEInput->IsConnected((eFEINPUT_PAD)0))
            {
                skipFEInfo = mGameInfo[mCurrentMode];
                skipFEInfo->mPadSides[0] = 0;
            }
            if (g_pFEInput->IsConnected((eFEINPUT_PAD)1))
            {
                skipFEInfo = mGameInfo[mCurrentMode];
                skipFEInfo->mPadSides[1] = 1;
            }
            if (g_pFEInput->IsConnected((eFEINPUT_PAD)2))
            {
                skipFEInfo = mGameInfo[mCurrentMode];
                skipFEInfo->mPadSides[2] = 0;
            }
            if (g_pFEInput->IsConnected((eFEINPUT_PAD)3))
            {
                skipFEInfo = mGameInfo[mCurrentMode];
                skipFEInfo->mPadSides[3] = 1;
            }
        }
    }

    mUserInfo.mSaveID = nlRandom(0xFFFFFFFF, &nlDefaultSeed);
    memset(mUserInfo.mTrophies, 0, sizeof(mUserInfo.mTrophies));
    mUserInfo.mIsFlowerCupUnlocked = false;
    mUserInfo.mIsStarCupUnlocked = false;
    mUserInfo.mNumGamesPlayed = 0;
    mUserInfo.mNumGoalsScored = 0;
    mUserInfo.mNumHits = 0;
    mUserInfo.mNumPerfectPasses = 0;
    mUserInfo.mNumSTSAttempts = 0;

    for (int i = 0; i < 4; i++)
    {
        memset(&mUserStats[i], 0, sizeof(PlayerStats));
        mUserStats[i].mRecordType.mControllerID = i;
        mUserStats[i].mType = TYPE_USER;
    }

    mUserInfo.mAudioOptions.InitializeDefaults();
    mUserInfo.mVisualOptions.InitializeDefaults();
    mUserInfo.mGameplayOptions.InitializeDefaults();
    mUserInfo.mPowerupOptions.InitializeDefaults();
    mUserInfo.mCheatOptions.InitializeDefaults();
}

/**
 * Offset/Address/Size: 0x9E1C | 0x8017F4C0 | size: 0x74
 */
GameInfoManager::~GameInfoManager()
{
    delete mGameInfo[0];
    delete mGameInfo[10];
}

/**
 * Offset/Address/Size: 0x9DEC | 0x8017F490 | size: 0x30
 */
eTeamID GameInfoManager::GetTeam(short homeaway) const
{
    if (mGameInfo[mCurrentMode] == nullptr)
        return TEAM_INVALID;
    return mGameInfo[mCurrentMode]->mTeamIndex[homeaway];
}

/**
 * Offset/Address/Size: 0x9DCC | 0x8017F470 | size: 0x20
 */
void GameInfoManager::SetTeam(short homeaway, eTeamID teamid)
{
    mGameInfo[mCurrentMode]->mTeamIndex[homeaway] = teamid;
}

/**
 * Offset/Address/Size: 0x9D94 | 0x8017F438 | size: 0x38
 */
eSidekickID GameInfoManager::GetSidekick(short homeaway) const
{
    if (mGameInfo[mCurrentMode]->mTeamIndex[homeaway] == TEAM_MYSTERY)
        return SK_MYSTERY;
    return mGameInfo[mCurrentMode]->mSidekickIndex[homeaway];
}

/**
 * Offset/Address/Size: 0x9D70 | 0x8017F414 | size: 0x24
 */
void GameInfoManager::SetSidekick(short homeaway, eSidekickID sidekickid)
{
    mGameInfo[mCurrentMode]->mSidekickIndex[homeaway] = sidekickid;
}

/**
 * Offset/Address/Size: 0x9D24 | 0x8017F3C8 | size: 0x4C
 */
u16 GameInfoManager::GetNumPlayingTeams() const
{
    if (mCurrentMode == 0x4 || mCurrentMode == 0x8)
    {
        return 8;
    }

    return mCurrentCup->GetNumTeams();
}

/**
 * Offset/Address/Size: 0x9CF4 | 0x8017F398 | size: 0x30
 */
u16 GameInfoManager::GetNumRounds() const
{
    return mCurrentCup->GetNumRounds();
}

/**
 * Offset/Address/Size: 0x9AB8 | 0x8017F15C | size: 0x23C
 */
TeamStats GameInfoManager::GetTeamStatsByIndex(unsigned short index)
{
    if (mCurrentMode == GM_BOWSER_CUP)
    {
        return *mBowserCupSeries.GetTeamStats(index);
    }
    else if (mCurrentMode == GM_SUPER_BOWSER_CUP)
    {
        return *mSuperBowserCupSeries.GetTeamStats(index);
    }
    else
    {
        return *mCurrentCup->GetTeamStats(index);
    }
}

/**
 * Offset/Address/Size: 0x9A38 | 0x8017F0DC | size: 0x80
 */
TeamStats* GameInfoManager::pGetTeamStatsByIndex(unsigned short index)
{
    if (mCurrentMode == GM_BOWSER_CUP)
    {
        return mBowserCupSeries.GetTeamStats(index);
    }
    else if (mCurrentMode == GM_SUPER_BOWSER_CUP)
    {
        return mSuperBowserCupSeries.GetTeamStats(index);
    }
    else
    {
        return mCurrentCup->GetTeamStats(index);
    }
}

/**
 * Offset/Address/Size: 0x96B4 | 0x8017ED58 | size: 0x384
 */
void GameInfoManager::SetPreviousTeamStats()
{
    int i;

    if (IsInCupOrTournamentMode())
    {
        for (i = 0; i < GetNumPlayingTeams(); i++)
        {
            mPreviousTeamStats[i] = GetTeamStatsByIndex(i);
        }
    }
}

/**
 * Offset/Address/Size: 0x969C | 0x8017ED40 | size: 0x18
 */
eStadiumID GameInfoManager::GetStadium() const
{
    return mGameInfo[mCurrentMode]->mStadiumIndex;
}

/**
 * Offset/Address/Size: 0x9574 | 0x8017EC18 | size: 0x128
 */
BasicGameInfo* GameInfoManager::GetMatchupInfo(short round, unsigned short matchup) const
{
    BaseCup* pCup;
    eGameModes mode;
    pCup = mCurrentCup;
    mode = mCurrentMode;

    if (mode == GM_BOWSER_CUP || mode == GM_SUPER_BOWSER_CUP)
    {
        if (round == -3)
        {
            round = 0;
        }
        else if (round == -2 || round == -5 || round == -1)
        {
            round = 1;
        }
        else
        {
            if (mDoingKnockout)
            {
                pCup = mPreviousCup;
            }
        }
    }
    else
    {
        if (round == -4)
        {
            round = mCurrentCup->GetNumRounds() - 3;
        }
        else if (round == -3)
        {
            round = mCurrentCup->GetNumRounds() - 2;
        }
        else if (round == -2)
        {
            round = mCurrentCup->GetNumRounds() - 1;
        }
    }

    return pCup->GetGameInfo(round, matchup);
}

/**
 * Offset/Address/Size: 0x953C | 0x8017EBE0 | size: 0x38
 */
void GameInfoManager::SetUserSelectedCupTeam(eTeamID team)
{
    mCurrentCup->mUserSelectedTeam = team;

    if (team != TEAM_INVALID)
    {
        mCurrentCup->mHumanTeams = 0;
        mCurrentCup->mHumanTeams = (s32)(mCurrentCup->mHumanTeams | (1 << team));
    }
}

/**
 * Offset/Address/Size: 0x9530 | 0x8017EBD4 | size: 0xC
 */
void GameInfoManager::SetUserSelectedCupSidekick(eSidekickID sidekick)
{
    mCurrentCup->mUserSelectedSidekick = sidekick;
}

/**
 * Offset/Address/Size: 0x951C | 0x8017EBC0 | size: 0x14
 */
eUserGameResult GameInfoManager::GetResultsOfLastUserGame() const
{
    return mUserLastResults[mCurrentMode];
}

/**
 * Offset/Address/Size: 0x9508 | 0x8017EBAC | size: 0x14
 */
void GameInfoManager::SetResultsOfLastUserGame(eUserGameResult result)
{
    mUserLastResults[mCurrentMode] = result;
}

/**
 * Offset/Address/Size: 0x94FC | 0x8017EBA0 | size: 0xC
 */
s16 GameInfoManager::GetCurrentRoundNumber() const
{
    return mCurrentCup->mRoundNumber;
}

/**
 * Offset/Address/Size: 0x9300 | 0x8017E9A4 | size: 0x1FC
 */
/**
 */
s16 GameInfoManager::GetNextRoundNumber(short roundParam) const
{
    s16 round;

    if (roundParam == -7)
    {
        round = mCurrentCup->mRoundNumber;
    }
    else
    {
        round = roundParam;
    }

    if (round == -6)
    {
        round = 0;
    }
    else if (round == -4)
    {
        round = -3;
    }
    else if (round == -3)
    {
        round = -2;
    }
    else if (round == -2)
    {
        if (mCurrentMode == GM_BOWSER_CUP && !CheckUnlockStatus(isSuperTeamUnlocked, mUserInfo.mTrophies[0], 3))
        {
            round = -1;
        }
        else
        {
            round = -5;
        }
    }
    else if (round == -1)
    {
        round = -5;
    }
    else
    {
        if ((mCurrentMode == GM_BOWSER_CUP || mCurrentMode == GM_SUPER_BOWSER_CUP) && mDoingKnockout && round == mPreviousCup->GetNumRounds() - 1)
        {
            round = -3;
        }
        else if (round + 1 == mCurrentCup->GetNumRounds())
        {
            round = -5;
        }
        else
        {
            round++;
        }
    }

    return round;
}

/**
 * Offset/Address/Size: 0x91E8 | 0x8017E88C | size: 0x118
 */
s16 GameInfoManager::GetPreviousRoundNumber(short roundParam) const
{
    eGameModes mode;

    if (roundParam == -7)
    {
        roundParam = mCurrentCup->mRoundNumber;
    }

    mode = mCurrentMode;

    if ((roundParam == -5) && (!mDoingKnockout))
    {
        roundParam = mCurrentCup->GetNumRounds() - 1;
    }
    else if ((roundParam == -5) && (mDoingKnockout))
    {
        roundParam = -2;
    }
    else if ((roundParam == -3) && (mode == GM_BOWSER_CUP || mode == GM_SUPER_BOWSER_CUP))
    {
        roundParam = mPreviousCup->GetNumRounds() - 1;
    }
    else if ((roundParam == -3) && (mode != GM_BOWSER_CUP && mode != GM_SUPER_BOWSER_CUP))
    {
        roundParam = -4;
    }
    else if (roundParam == -2)
    {
        roundParam = -3;
    }
    else if (roundParam == -1)
    {
        roundParam = -2;
    }
    else
    {
        roundParam--;
    }
    return roundParam;
}

/**
 * Offset/Address/Size: 0x9180 | 0x8017E824 | size: 0x68
 */
signed short GameInfoManager::GetFirstRoundNumber() const
{
    if ((mCurrentMode == GM_TOURNAMENT) && (mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT))
    {
        s16 result;
        u16 numRounds = mCurrentCup->GetNumRounds();
        result = -4;
        if (numRounds == 2)
        {
            result = -3;
        }
        return (s16)result;
    }
    return 0;
}

/**
 * Offset/Address/Size: 0x90AC | 0x8017E750 | size: 0xD4
 */
u16 GameInfoManager::GetNumGamesPerRound(int round) const
{
    unsigned short returnValue;

    if (round == -4)
    {
        returnValue = 4;
    }
    else if (round == -3)
    {
        returnValue = 2;
    }
    else if (round == -2 || round == -1)
    {
        returnValue = 1;
    }
    else if (round == -5 && mDoingKnockout)
    {
        returnValue = 1;
    }
    else if (mDoingKnockout)
    {
        returnValue = mPreviousCup->GetNumTeams() >> 1;
    }
    else
    {
        unsigned short numTeams;
        if (mCurrentMode == GM_BOWSER_CUP || mCurrentMode == GM_SUPER_BOWSER_CUP)
        {
            numTeams = 8;
        }
        else
        {
            numTeams = mCurrentCup->GetNumTeams();
        }
        returnValue = numTeams >> 1;
    }
    return returnValue;
}
/**
 * Offset/Address/Size: 0x90A0 | 0x8017E744 | size: 0xC
 */
eTeamID GameInfoManager::GetUserSelectedCupTeam() const
{
    return mCurrentCup->mUserSelectedTeam;
}

/**
 * Offset/Address/Size: 0x9088 | 0x8017E72C | size: 0x18
 */
void GameInfoManager::SetStadium(eStadiumID stadiumID)
{
    mGameInfo[mCurrentMode]->mStadiumIndex = stadiumID;

    {
        static const char* const kNames[] = {
            "mario_stadium", "peach_toad", "dk_daisy", "wario_stadium",
            "yoshi_stadium", "super_stadium", "forbidden_dome",
        };
        const int idx = (int)stadiumID;
        PortOverlaySetStadium(
            (idx >= 0 && idx < (int)(sizeof(kNames) / sizeof(kNames[0])))
                ? kNames[idx] : "unknown");
    }
}

/**
 * Offset/Address/Size: 0x8C28 | 0x8017E2CC | size: 0x460
 */
eStadiumID GameInfoManager::PickStadium(bool isLastRound, eStadiumID excludeStadium) const
{
    eStadiumID returnValue;
    eStadiumID lastRoundStadium;

    if (g_e3_Build)
    {
        return STAD_WARIO_STADIUM;
    }

    {
        static int s_forcedRead = 0;
        static int s_forced = -1;
        if (!s_forcedRead)
        {
            static const char* const kNames[] = {
                "mario_stadium", "peach_toad", "dk_daisy", "wario_stadium",
                "yoshi_stadium", "super_stadium", "forbidden_dome",
            };
            const char* want = getenv("STRIKERS_FORCE_STADIUM");
            s_forcedRead = 1;
            if (want != NULL && *want != '\0')
            {
                int i;
                for (i = 0; i < (int)(sizeof(kNames) / sizeof(kNames[0])); i++)
                {
                    if (nlStrCmp<char>(want, kNames[i]) == 0)
                    {
                        s_forced = i;
                        break;
                    }
                }
                if (s_forced < 0 && want[0] >= '0' && want[0] <= '6' && want[1] == '\0')
                    s_forced = want[0] - '0';
                if (s_forced < 0)
                {
                    OSReport("[stadium] STRIKERS_FORCE_STADIUM=%s is not a stadium; "
                             "expected one of mario_stadium peach_toad dk_daisy "
                             "wario_stadium yoshi_stadium super_stadium "
                             "forbidden_dome, or 0-6\n", want);
                    nlBreak();
                }
                else
                {
                    OSReport("[stadium] forced to %s\n", kNames[s_forced]);
                }
            }
        }
        // The menu can change it after boot; the platform seeds its copy from the same variable.
        {
            const int live = PortDebugForcedStadium();
            if (live >= 0)
                return (eStadiumID)live;
        }
        (void)s_forced;
    }

    lastRoundStadium = STAD_INVALID;

    bool kongaUnlocked = CheckUnlockStatusPickStadium(isKongaUnlocked, mUserInfo.mTrophies[0], 0);
    bool yoshiUnlocked = CheckUnlockStatus(isYoshiUnlocked, mUserInfo.mTrophies[0], 1);
    bool forbiddenUnlocked = CheckUnlockStatus(isForbiddenUnlocked, mUserInfo.mTrophies[0], 2);
    bool superStadiumUnlocked = CheckUnlockStatus(isSuperStadUnlocked, mUserInfo.mTrophies[0], 3);

    eGameModes mode = mCurrentMode;
    if (mode == GM_TOURNAMENT)
    {
        isLastRound = false;
    }

    switch (mCurrentMode)
    {
    case GM_MUSHROOM_CUP:
        lastRoundStadium = STAD_PEACH_TOAD_STADIUM;
        break;
    case GM_FLOWER_CUP:
        lastRoundStadium = STAD_MARIO_STADIUM;
        break;
    case GM_STAR_CUP:
        lastRoundStadium = STAD_WARIO_STADIUM;
        break;
    case GM_SUPER_MUSHROOM_CUP:
        lastRoundStadium = STAD_YOSHI_STADIUM;
        break;
    case GM_SUPER_FLOWER_CUP:
        lastRoundStadium = STAD_DK_DAISY;
        break;
    case GM_SUPER_STAR_CUP:
        lastRoundStadium = STAD_FORBIDDEN_DOME;
        break;
    case GM_BOWSER_CUP:
    case GM_SUPER_BOWSER_CUP:
        lastRoundStadium = STAD_SUPER_STADIUM;
        break;
    default:
        break;
    }

    if (isLastRound)
    {
        return lastRoundStadium;
    }

    while (true)
    {
        returnValue = (eStadiumID)nlRandom(7, &nlDefaultSeed);

        if (returnValue == lastRoundStadium)
        {
            bool shouldRepick;
            eGameModes mode2 = mCurrentMode;

            switch (mode2)
            {
            case GM_MUSHROOM_CUP:
            case GM_FLOWER_CUP:
            case GM_STAR_CUP:
            case GM_BOWSER_CUP:
                shouldRepick = true;
                break;
            default:
                shouldRepick = false;
                break;
            }

            if (shouldRepick == false)
            {
                switch (mode2)
                {
                case GM_SUPER_MUSHROOM_CUP:
                case GM_SUPER_FLOWER_CUP:
                case GM_SUPER_STAR_CUP:
                case GM_SUPER_BOWSER_CUP:
                    shouldRepick = true;
                    break;
                default:
                    shouldRepick = false;
                    break;
                }
            }

            if (shouldRepick)
            {
                continue;
            }
        }

        if (returnValue == excludeStadium)
        {
            continue;
        }

        if ((u32)returnValue <= STAD_PEACH_TOAD_STADIUM)
        {
            break;
        }

        if (returnValue == STAD_WARIO_STADIUM)
        {
            break;
        }

        if (returnValue == STAD_DK_DAISY)
        {
            if (kongaUnlocked)
            {
                break;
            }
        }

        if (returnValue == STAD_YOSHI_STADIUM)
        {
            if (yoshiUnlocked)
            {
                break;
            }
        }

        if (returnValue == STAD_FORBIDDEN_DOME)
        {
            if (forbiddenUnlocked)
            {
                break;
            }
        }

        if (returnValue != STAD_SUPER_STADIUM)
        {
            continue;
        }

        if (superStadiumUnlocked == false)
        {
            continue;
        }

        break;
    }

    return returnValue;
}

/**
 * Offset/Address/Size: 0x8C08 | 0x8017E2AC | size: 0x20
 */
s16 GameInfoManager::GetPlayingSide(unsigned short padnumber) const
{
    eGameModes mode;
    BasicGameInfo* gameInfo = mGameInfo[mode = mCurrentMode];
    return gameInfo->mPadSides[padnumber];
}

/**
 * Offset/Address/Size: 0x8BE8 | 0x8017E28C | size: 0x20
 */
void GameInfoManager::SetPlayingSide(unsigned short padnumber, short side)
{
    mGameInfo[mCurrentMode]->mPadSides[padnumber] = side;
}

/**
 * Offset/Address/Size: 0x8B38 | 0x8017E1DC | size: 0xB0
 */
u16 GameInfoManager::GetNumPlayers() const
{
    BasicGameInfo* const* pInfo = &mGameInfo[mCurrentMode];
    u16 count = 0;

    for (int i = 0; i < 4; i++)
    {
        if ((*pInfo)->mPadSides[(u16)i] != -1)
        {
            count++;
        }
    }

    return count;
}

/**
 * Offset/Address/Size: 0x8B10 | 0x8017E1B4 | size: 0x28
 */
void GameInfoManager::ResetPlayingSides()
{
    BasicGameInfo* gameInfo = mGameInfo[mCurrentMode];
    gameInfo->mPadSides[0] = -1;
    gameInfo->mPadSides[1] = -1;
    gameInfo->mPadSides[2] = -1;
    gameInfo->mPadSides[3] = -1;
}

static const int THREE_TEAM_MATCHUPS[3][2] = {
    { 0, 1 },
    { 1, 2 },
    { 2, 0 },
};
static const int FOUR_TEAM_MATCHUPS[6][2] = {
    { 0, 1 },
    { 2, 3 },
    { 3, 1 },
    { 2, 0 },
    { 0, 3 },
    { 1, 2 },
};
static const int FIVE_TEAM_MATCHUPS[10][2] = {
    { 0, 1 },
    { 2, 3 },
    { 4, 0 },
    { 1, 2 },
    { 3, 4 },
    { 0, 2 },
    { 1, 3 },
    { 4, 2 },
    { 3, 0 },
    { 1, 4 },
};
static const int SIX_TEAM_MATCHUPS[15][2] = {
    { 0, 1 },
    { 5, 4 },
    { 3, 2 },
    { 3, 5 },
    { 1, 4 },
    { 2, 0 },
    { 4, 2 },
    { 0, 3 },
    { 5, 1 },
    { 1, 3 },
    { 5, 2 },
    { 4, 0 },
    { 0, 5 },
    { 2, 1 },
    { 3, 4 },
};
static const int SEVEN_TEAM_MATCHUPS[21][2] = {
    { 0, 1 },
    { 5, 4 },
    { 3, 2 },
    { 3, 5 },
    { 6, 4 },
    { 2, 1 },
    { 3, 6 },
    { 4, 0 },
    { 5, 1 },
    { 3, 4 },
    { 2, 0 },
    { 1, 6 },
    { 0, 5 },
    { 6, 2 },
    { 1, 3 },
    { 1, 4 },
    { 5, 2 },
    { 6, 0 },
    { 4, 2 },
    { 0, 3 },
    { 5, 6 },
};
static const int EIGHT_TEAM_MATCHUPS[28][2] = {
    { 4, 0 },
    { 1, 5 },
    { 7, 3 },
    { 2, 6 },
    { 6, 1 },
    { 0, 5 },
    { 3, 4 },
    { 7, 2 },
    { 5, 3 },
    { 2, 4 },
    { 1, 7 },
    { 6, 0 },
    { 0, 7 },
    { 3, 6 },
    { 5, 2 },
    { 4, 1 },
    { 3, 2 },
    { 7, 6 },
    { 0, 1 },
    { 4, 5 },
    { 5, 7 },
    { 2, 0 },
    { 6, 4 },
    { 1, 3 },
    { 2, 1 },
    { 4, 7 },
    { 6, 5 },
    { 0, 3 },
};

/**
 * Offset/Address/Size: 0x8638 | 0x8017DCDC | size: 0x4D8
 */
void GameInfoManager::SetupRoundRobinSchedule(eTeamID* lineup, eSidekickID* sklineup)
{
    eGameModes gamemode = mCurrentMode;
    int numplayingteams = GetNumPlayingTeams();
    int numRounds = mCurrentCup->GetNumRounds();
    int numGamesPerRound = GetNumGamesPerRound(0);
    int numNormalRounds;
    int home;
    int away;
    unsigned char superRounds;
    int superOffset;
    eStadiumID lastHumanStadium;
    eStadiumID currentStadium;
    int i;
    int j;
    BasicGameInfo* g;

    mLastHumanStadium = STAD_INVALID;
    superRounds = 0;
    superOffset = 0;
    lastHumanStadium = STAD_INVALID;

    {
        bool isRegularCup;
        switch (mCurrentMode)
        {
        case GM_MUSHROOM_CUP:
        case GM_FLOWER_CUP:
        case GM_STAR_CUP:
        case GM_BOWSER_CUP:
            isRegularCup = true;
            break;
        default:
            isRegularCup = false;
            break;
        }
        if (!isRegularCup)
        {
            bool isSuperCup;
            switch (mCurrentMode)
            {
            case GM_SUPER_MUSHROOM_CUP:
            case GM_SUPER_FLOWER_CUP:
            case GM_SUPER_STAR_CUP:
            case GM_SUPER_BOWSER_CUP:
                isSuperCup = true;
                break;
            default:
                isSuperCup = false;
                break;
            }
            if (!isSuperCup)
            {
                if (gamemode != GM_TOURNAMENT)
                {
                    return;
                }
            }
        }
    }

    if (gamemode == GM_BOWSER_CUP)
    {
        mCurrentCup = &mBowserCupSeries;
    }
    else if (gamemode == GM_SUPER_BOWSER_CUP)
    {
        mCurrentCup = &mSuperBowserCupSeries;
    }

    mCurrentCup->mRoundNumber = -6;
    mDoingKnockout = false;
    numNormalRounds = (numplayingteams % 2 != 0) ? numplayingteams : numplayingteams - 1;

    for (i = 0; i < numRounds; i++)
    {
        if (i == numNormalRounds)
        {
            superRounds = 1;
            superOffset = numNormalRounds;
        }

        for (j = 0; j < numGamesPerRound; j++)
        {
            int matchupIdx = numGamesPerRound * (i - superOffset) + j;

            switch (GetNumPlayingTeams())
            {
            case 3:
                home = THREE_TEAM_MATCHUPS[matchupIdx][0];
                away = THREE_TEAM_MATCHUPS[matchupIdx][1];
                break;
            case 4:
                home = FOUR_TEAM_MATCHUPS[matchupIdx][0];
                away = FOUR_TEAM_MATCHUPS[matchupIdx][1];
                break;
            case 5:
                home = FIVE_TEAM_MATCHUPS[matchupIdx][0];
                away = FIVE_TEAM_MATCHUPS[matchupIdx][1];
                break;
            case 6:
                home = SIX_TEAM_MATCHUPS[matchupIdx][0];
                away = SIX_TEAM_MATCHUPS[matchupIdx][1];
                break;
            case 7:
                home = SEVEN_TEAM_MATCHUPS[matchupIdx][0];
                away = SEVEN_TEAM_MATCHUPS[matchupIdx][1];
                break;
            case 8:
                home = EIGHT_TEAM_MATCHUPS[matchupIdx][0];
                away = EIGHT_TEAM_MATCHUPS[matchupIdx][1];
                break;
            }

            g = mCurrentCup->GetGameInfo(i, j);
            g->mTeamIndex[0] = TEAM_MARIO;
            g->mTeamIndex[1] = TEAM_LUIGI;
            g->mSidekickIndex[0] = SK_TOAD;
            g->mSidekickIndex[1] = SK_KOOPA;
            g->mFinalScore[1] = 0;
            g->mFinalScore[0] = 0;
            g->mPadSides[0] = -1;
            g->mPadSides[1] = -1;
            g->mPadSides[2] = -1;
            g->mPadSides[3] = -1;
            g->mStadiumIndex = STAD_MARIO_STADIUM;

            if (!superRounds)
            {
                g->mTeamIndex[0] = lineup[home];
                g->mTeamIndex[1] = lineup[away];
                g->mSidekickIndex[0] = sklineup[home];
                g->mSidekickIndex[1] = sklineup[away];
            }
            else
            {
                g->mTeamIndex[0] = lineup[away];
                g->mTeamIndex[1] = lineup[home];
                g->mSidekickIndex[0] = sklineup[away];
                g->mSidekickIndex[1] = sklineup[home];
            }

            if (i == numRounds - 1 && gamemode != GM_BOWSER_CUP && gamemode != GM_SUPER_BOWSER_CUP)
            {
                currentStadium = PickStadium(true, lastHumanStadium);
            }
            else
            {
                currentStadium = PickStadium(false, lastHumanStadium);
            }
            g->mStadiumIndex = currentStadium;

            if ((mCurrentCup->mHumanTeams & (1 << g->mTeamIndex[0])) || (mCurrentCup->mHumanTeams & (1 << g->mTeamIndex[1])))
            {
                lastHumanStadium = currentStadium;
            }
        }

        *mCurrentCup->GetRoundResults(i) = 1;
    }

    {
        eTeamID teamID;
        TeamStats* returnedStats;
        TeamStats* teamstats;
        eTeamID* lineupPtr;
        int k;
        returnedStats = mCurrentCup->GetTeamStats(0);
        lineupPtr = lineup;
        k = 0;
        teamstats = returnedStats;
        for (; k < numplayingteams; k++)
        {
            teamID = *lineupPtr;
            memset(&teamstats->mPlayerTotalStats, 0, sizeof(PlayerStats));
            teamstats->mPlayerTotalStats.mRecordType.mTeamID = teamID;
            teamstats->mPlayerTotalStats.mType = TYPE_TEAM;
            teamstats->mTeamIndex = teamID;
            teamstats->mNumWins = 0;
            teamstats->mNumLosses = 0;
            teamstats->mNumOTLosses = 0;
            teamstats->mNumPoints = 0;
            lineupPtr++;
            teamstats++;
        }
    }

    mCurrentCup->mCupStarted = true;
}

static const eTrophyType MILESTONES[5] = {
    TROPHY_VETERAN_CUP,
    TROPHY_SNIPER_CUP,
    TROPHY_STRIKER_CUP,
    TROPHY_TACTICIAN_CUP,
    TROPHY_PARAMEDIC_CUP,
};

static const int BOWSER_KNOCKOUT_ORDER[4] = { 0, 3, 1, 2 };

/**
 * Offset/Address/Size: 0x81CC | 0x8017D870 | size: 0x46C
 */
unsigned char GameInfoManager::SetupBowserKnockout()
{
    int teamIndices[8];
    eSidekickID sidekicks[9];
    unsigned char returnValue = 0;
    int numPreviousTeams;
    int numTeams;
    StatsTracker* pStatsTracker;

    mLastHumanStadium = STAD_INVALID;

    for (int i = 0; i < (u16)mPreviousCup->GetNumTeams() / 2; i++)
    {
        BasicGameInfo* g = mPreviousCup->GetGameInfo(0, i);
        eTeamID index = g->mTeamIndex[0];
        sidekicks[index] = g->mSidekickIndex[0];
        index = g->mTeamIndex[1];
        sidekicks[index] = g->mSidekickIndex[1];
    }

    *mCurrentCup->GetRoundResults(0) = 1;
    *mCurrentCup->GetRoundResults(1) = 1;
    *mCurrentCup->GetRoundResults(2) = 1;

    mCurrentCup->mUserSelectedTeam = mPreviousCup->mUserSelectedTeam;
    mCurrentCup->mUserSelectedSidekick = mPreviousCup->mUserSelectedSidekick;

    pStatsTracker = nlSingleton<StatsTracker>::s_pInstance;
    numTeams = mCurrentCup->GetNumTeams();
    numPreviousTeams = mPreviousCup->GetNumTeams();

    pStatsTracker->GetSortedTeamStats(mPreviousCup->GetTeamStats(0), numPreviousTeams, teamIndices, numTeams);

    for (int i = 0; i < (u16)mCurrentCup->GetNumTeams(); i++)
    {
        TeamStats* pSourceStats = mPreviousCup->GetTeamStats(teamIndices[BOWSER_KNOCKOUT_ORDER[i]]);
        TeamStats* pDestStats = mCurrentCup->GetTeamStats(i);
        *pDestStats = *pSourceStats;

        if (mCurrentCup->GetTeamStats(i)->mTeamIndex == mCurrentCup->mUserSelectedTeam)
        {
            returnValue = 1;
        }
    }

    BasicGameInfo* g = mCurrentCup->GetGameInfo(0, 0);
    g->mTeamIndex[0] = TEAM_MARIO;
    g->mTeamIndex[1] = TEAM_LUIGI;
    g->mSidekickIndex[0] = SK_TOAD;
    g->mSidekickIndex[1] = SK_KOOPA;
    g->mFinalScore[1] = 0;
    g->mFinalScore[0] = 0;
    g->mPadSides[0] = -1;
    g->mPadSides[1] = -1;
    g->mPadSides[2] = -1;
    g->mPadSides[3] = -1;
    g->mStadiumIndex = STAD_MARIO_STADIUM;

    eTeamID home = mCurrentCup->GetTeamStats(0)->mTeamIndex;
    eTeamID away = mCurrentCup->GetTeamStats(1)->mTeamIndex;
    g->mTeamIndex[0] = home;
    g->mTeamIndex[1] = away;
    g->mSidekickIndex[0] = sidekicks[home];
    g->mSidekickIndex[1] = sidekicks[away];
    g->mStadiumIndex = PickStadium(true, STAD_INVALID);

    g = mCurrentCup->GetGameInfo(0, 1);
    g->mTeamIndex[0] = TEAM_MARIO;
    g->mTeamIndex[1] = TEAM_LUIGI;
    g->mSidekickIndex[0] = SK_TOAD;
    g->mSidekickIndex[1] = SK_KOOPA;
    g->mFinalScore[1] = 0;
    g->mFinalScore[0] = 0;
    g->mPadSides[0] = -1;
    g->mPadSides[1] = -1;
    g->mPadSides[2] = -1;
    g->mPadSides[3] = -1;
    g->mStadiumIndex = STAD_MARIO_STADIUM;

    home = mCurrentCup->GetTeamStats(2)->mTeamIndex;
    away = mCurrentCup->GetTeamStats(3)->mTeamIndex;
    g->mTeamIndex[0] = home;
    g->mTeamIndex[1] = away;
    g->mSidekickIndex[0] = sidekicks[home];
    g->mSidekickIndex[1] = sidekicks[away];
    g->mStadiumIndex = PickStadium(true, STAD_INVALID);

    mCurrentCup->mCupStarted = true;

    return returnValue;
}

/**
 * Offset/Address/Size: 0x7EF0 | 0x8017D594 | size: 0x2DC
 */
void GameInfoManager::SetupTournamentKnockout(eTeamID* lineup, eSidekickID* sklineup)
{
    s16 firstRound = mCurrentCup->mRoundNumber;
    int numGames = GetNumGamesPerRound(firstRound);

    mCurrentCup->mCupStarted = true;
    mCurrentCup->mGameNumber = 0;
    mLastHumanStadium = STAD_INVALID;

    int numplayingteams = mCurrentCup->GetNumTeams();

    *mCurrentCup->GetRoundResults(0) = 1;
    *mCurrentCup->GetRoundResults(1) = 1;
    *mCurrentCup->GetRoundResults(2) = 1;

    for (int i = 0; i < numGames; i++)
    {
        BasicGameInfo* g = mCurrentCup->GetGameInfo(0, i);

        g->mTeamIndex[0] = TEAM_MARIO;
        g->mTeamIndex[1] = TEAM_LUIGI;
        g->mSidekickIndex[0] = SK_TOAD;
        g->mSidekickIndex[1] = SK_KOOPA;
        g->mFinalScore[1] = 0;
        g->mFinalScore[0] = 0;
        g->mPadSides[0] = -1;
        g->mPadSides[1] = -1;
        g->mPadSides[2] = -1;
        g->mPadSides[3] = -1;
        g->mStadiumIndex = STAD_MARIO_STADIUM;

        {
            eTeamID away = lineup[i * 2 + 1];
            eSidekickID homeSK = sklineup[i * 2];
            eSidekickID awaySK = sklineup[i * 2 + 1];

            g->mTeamIndex[0] = lineup[i * 2];
            g->mTeamIndex[1] = away;
            g->mSidekickIndex[0] = homeSK;
            g->mSidekickIndex[1] = awaySK;
        }

        {
            eStadiumID currentStadium = PickStadium(false, mLastHumanStadium);
            g->mStadiumIndex = currentStadium;

            u16 humanTeams = mCurrentCup->mHumanTeams;
            eTeamID home = g->mTeamIndex[0];
            if ((humanTeams & (1 << home)) || (humanTeams & (1 << g->mTeamIndex[1])))
            {
                mLastHumanStadium = currentStadium;
            }
        }
    }

    {
        eTeamID teamID;
        TeamStats* returnedStats;
        TeamStats* teamstats;
        eTeamID* lineupPtr;
        int k;
        returnedStats = mCurrentCup->GetTeamStats(0);
        lineupPtr = lineup;
        k = 0;
        teamstats = returnedStats;
        for (; k < numplayingteams; k++)
        {
            teamID = *lineupPtr;
            memset(&teamstats->mPlayerTotalStats, 0, sizeof(PlayerStats));
            teamstats->mPlayerTotalStats.mRecordType.mTeamID = teamID;
            teamstats->mPlayerTotalStats.mType = TYPE_TEAM;
            teamstats->mTeamIndex = teamID;
            teamstats->mNumWins = 0;
            teamstats->mNumLosses = 0;
            teamstats->mNumOTLosses = 0;
            teamstats->mNumPoints = 0;
            lineupPtr++;
            teamstats++;
        }
    }
}

/**
 * Offset/Address/Size: 0x78D8 | 0x8017CF7C | size: 0x618
 */
unsigned char GameInfoManager::SetupKnockoutRound(short round)
{
    eSidekickID sidekicks[9];
    BaseCup* cup = mCurrentCup;
    int gamesPerRound = GetNumGamesPerRound(round);
    signed short previousRound;
    signed short currentRound;
    eTeamID home;
    eTeamID away;
    unsigned char returnValue = 0;
    eStadiumID currentStadium;

    if (mCurrentMode == GM_BOWSER_CUP || mCurrentMode == GM_SUPER_BOWSER_CUP)
    {
        cup = mPreviousCup;
    }

    for (int i = 0; i < (u16)cup->GetNumTeams() / 2; i++)
    {
        BasicGameInfo* g = cup->GetGameInfo(0, i);
        eTeamID index = g->mTeamIndex[0];
        sidekicks[index] = g->mSidekickIndex[0];
        index = g->mTeamIndex[1];
        sidekicks[index] = g->mSidekickIndex[1];
    }

    if (round == -3)
    {
        previousRound = (s16)(mCurrentCup->GetNumRounds() - 3);
        currentRound = (s16)(mCurrentCup->GetNumRounds() - 2);
    }
    else if (round == -2)
    {
        previousRound = (s16)(mCurrentCup->GetNumRounds() - 2);
        currentRound = (s16)(mCurrentCup->GetNumRounds() - 1);
    }
    else if (round == -1)
    {
        currentRound = (s16)(mCurrentCup->GetNumRounds() - 1);
        previousRound = currentRound;
    }

    if (round == -1)
    {
        BasicGameInfo* g = mCurrentCup->GetGameInfo(currentRound, 0);

        mUserInfo.mBowserCupFinalRound = *g;

        g = mCurrentCup->GetGameInfo(currentRound, 0);
        eTeamID losingTeam = (g->mFinalScore[0] < g->mFinalScore[1]) ? g->mTeamIndex[0] : g->mTeamIndex[1];

        eTeamID winner;
        if (g->mFinalScore[0] > g->mFinalScore[1])
        {
            winner = g->mTeamIndex[0];
        }
        else
        {
            winner = g->mTeamIndex[1];
        }

        if (winner == mCurrentCup->mUserSelectedTeam)
        {
            g->mTeamIndex[0] = (eTeamID)3;
            g->mTeamIndex[1] = (eTeamID)2;
            g->mSidekickIndex[0] = (eSidekickID)0;
            g->mSidekickIndex[1] = (eSidekickID)1;
            g->mFinalScore[1] = 0;
            g->mFinalScore[0] = 0;
            g->mPadSides[0] = -1;
            g->mPadSides[1] = -1;
            g->mPadSides[2] = -1;
            g->mPadSides[3] = -1;
            g->mStadiumIndex = (eStadiumID)0;

            g->mTeamIndex[0] = (eTeamID)8;
            g->mSidekickIndex[1] = (eSidekickID)0;
            g->mTeamIndex[1] = winner;
            g->mSidekickIndex[1] = sidekicks[winner];
            g->mStadiumIndex = PickStadium(true, STAD_INVALID);
            returnValue = 1;

            for (int i = 0; i < GetNumPlayingTeams(); i++)
            {
                TeamStats* ts = pGetTeamStatsByIndex((u16)i);

                if (ts->mTeamIndex == losingTeam)
                {
                    memset(&ts->mPlayerTotalStats, 0, sizeof(PlayerStats));
                    ts->mPlayerTotalStats.mRecordType.mTeamID = (eTeamID)8;
                    ts->mPlayerTotalStats.mType = TYPE_TEAM;
                    ts->mTeamIndex = (eTeamID)8;
                    ts->mNumWins = 0;
                    ts->mNumLosses = 0;
                    ts->mNumOTLosses = 0;
                    ts->mNumPoints = 0;
                    break;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < gamesPerRound; i++)
        {
            BasicGameInfo* g = mCurrentCup->GetGameInfo(previousRound, i * 2);
            home = (g->mFinalScore[0] > g->mFinalScore[1]) ? g->mTeamIndex[0] : g->mTeamIndex[1];

            g = mCurrentCup->GetGameInfo(previousRound, i * 2 + 1);
            away = (g->mFinalScore[0] > g->mFinalScore[1]) ? g->mTeamIndex[0] : g->mTeamIndex[1];

            g = mCurrentCup->GetGameInfo(currentRound, i);
            g->mTeamIndex[0] = (eTeamID)3;
            g->mTeamIndex[1] = (eTeamID)2;
            g->mSidekickIndex[0] = (eSidekickID)0;
            g->mSidekickIndex[1] = (eSidekickID)1;
            g->mFinalScore[1] = 0;
            g->mFinalScore[0] = 0;
            g->mPadSides[0] = -1;
            g->mPadSides[1] = -1;
            g->mPadSides[2] = -1;
            g->mPadSides[3] = -1;
            g->mStadiumIndex = (eStadiumID)0;

            g->mTeamIndex[0] = home;
            g->mTeamIndex[1] = away;
            g->mSidekickIndex[0] = sidekicks[home];
            g->mSidekickIndex[1] = sidekicks[away];
            currentStadium = PickStadium(true, mLastHumanStadium);
            g->mStadiumIndex = currentStadium;

            u16 humanTeams = mCurrentCup->mHumanTeams;
            if ((humanTeams & (1 << g->mTeamIndex[0])) || (humanTeams & (1 << g->mTeamIndex[1])))
            {
                mLastHumanStadium = currentStadium;
            }

            if (home == mCurrentCup->mUserSelectedTeam || away == mCurrentCup->mUserSelectedTeam)
            {
                returnValue = 1;
            }
            else
            {
                u16 humanTeams2 = mCurrentCup->mHumanTeams;
                if ((humanTeams2 & (1 << home)) || (humanTeams2 & (1 << away)))
                {
                    returnValue = 1;
                }
            }
        }
    }

    return returnValue;
}

/**
 * Offset/Address/Size: 0x7534 | 0x8017CBD8 | size: 0x3A4
 */
unsigned char GameInfoManager::DetermineNextMatchups(int dnmflags)
{
    int round = mCurrentCup->mRoundNumber;
    int numGames = GetNumGamesPerRound(round);

    int userPad = mMainUserPadNumber;

    while (round != -5)
    {
        u16 numRounds = mCurrentCup->GetNumRounds();

        if (round == -4)
        {
            round = (u16)numRounds - 3;
        }

        if (round == -3)
        {
            round = (u16)numRounds - 2;
        }

        if (round == -2 || round == -1)
        {
            round = (u16)numRounds - 1;
        }

        BasicGameInfo* gameinfo = mCurrentCup->GetGameInfo(round, mCurrentCup->mGameNumber);
        eTeamID home = gameinfo->mTeamIndex[0];
        BaseCup* cup;
        eTeamID away = gameinfo->mTeamIndex[1];
        mGameInfo[mCurrentMode] = gameinfo;

        if (dnmflags & 0x1)
        {
            cup = mCurrentCup;
            if ((cup->mHumanTeams & (1 << home)) || (cup->mHumanTeams & (1 << away)))
            {
                mGameInfo[mCurrentMode]->mPadSides[(u16)userPad] = (home != cup->mUserSelectedTeam);
                return 1;
            }
        }

        if (dnmflags & 0x10)
        {
            nlSingleton<StatsTracker>::Instance()->SetBasicGameInfoPointer(gameinfo, true);
            nlSingleton<StatsTracker>::Instance()->SimulateGame();
            nlSingleton<StatsTracker>::Instance()->CompileEndOfGameStats();
        }

        if (!(dnmflags & 0x2))
        {
            break;
        }

        IncreaseGameNumber(dnmflags & 0x4);

        if (mCurrentCup->mGameNumber == GetNumGamesPerRound(mCurrentCup->mRoundNumber))
        {
            if (dnmflags & 0x8)
            {
                break;
            }

            round = mCurrentCup->mRoundNumber;
        }
    }

    return 0;
}

/**
 * Offset/Address/Size: 0x71F4 | 0x8017C898 | size: 0x340
 */
void GameInfoManager::IncreaseRoundNumber()
{
    mCurrentCup->mRoundNumber = GetNextRoundNumber(mCurrentCup->mRoundNumber);
    mDidRoundJustEnd = true;

    s16 round = mCurrentCup->mRoundNumber;

    do
    {
        if (round != -3)
            if (round != -2)
                if (round != -1)
                    break;
        if (!SetupKnockoutRound(round))
        {
            if (mCurrentCup->mRoundNumber == -3)
            {
                mUserLastResults[mCurrentMode] = RESULT_USER_ELIMINATED_QUARTER;
                nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
                SetupKnockoutRound(-2);
                mCurrentCup->mRoundNumber = -2;
                nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
            }
            else if (mCurrentCup->mRoundNumber == -2)
            {
                mUserLastResults[mCurrentMode] = RESULT_USER_ELIMINATED_SEMI;
                nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
            }

            mCurrentCup->mRoundNumber = -5;
        }
    } while (false);

    if (mCurrentCup->mRoundNumber == -5)
    {
        mCurrentCup->mCupStarted = false;
    }

    if (mCurrentCup->mRoundNumber == -5 && !mDoingKnockout)
    {
        if (mCurrentMode == GM_BOWSER_CUP)
        {
            mPreviousCup = mCurrentCup;
            mCurrentCup = &mBowserCupKnockout;
            mCurrentCup->mUserSelectedTeam = mPreviousCup->mUserSelectedTeam;
            mCurrentCup->mUserSelectedSidekick = mPreviousCup->mUserSelectedSidekick;
            mCurrentCup->mHumanTeams = mPreviousCup->mHumanTeams;
            mCurrentCup->mRoundNumber = -3;
            mCurrentCup->mCupSettings = mPreviousCup->mCupSettings;

            unsigned char bowserResult = SetupBowserKnockout();
            mDoingKnockout = true;
            if (bowserResult)
            {
                mUserLastResults[mCurrentMode] = RESULT_USER_PLAYOFF_QUALIFIES;
            }
            else
            {
                mUserLastResults[mCurrentMode] = RESULT_USER_DOES_NOT_PLAYOFF_QUALIFY;
                nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
                mCurrentCup->mRoundNumber = -2;
                SetupKnockoutRound(-2);
                nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
                mCurrentCup->mRoundNumber = -5;
            }
        }

        if (mCurrentMode == GM_SUPER_BOWSER_CUP)
        {
            mPreviousCup = mCurrentCup;
            mCurrentCup = &mSuperBowserCupKnockout;
            mCurrentCup->mUserSelectedTeam = mPreviousCup->mUserSelectedTeam;
            mCurrentCup->mUserSelectedSidekick = mPreviousCup->mUserSelectedSidekick;
            mCurrentCup->mHumanTeams = mPreviousCup->mHumanTeams;
            mCurrentCup->mRoundNumber = -3;
            mCurrentCup->mCupSettings = mPreviousCup->mCupSettings;

            unsigned char superResult = SetupBowserKnockout();
            mDoingKnockout = true;
            if (superResult)
            {
                mUserLastResults[mCurrentMode] = RESULT_USER_PLAYOFF_QUALIFIES;
            }
            else
            {
                mUserLastResults[mCurrentMode] = RESULT_USER_DOES_NOT_PLAYOFF_QUALIFY;
                nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
                mCurrentCup->mRoundNumber = -2;
                SetupKnockoutRound(-2);
                nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
                mCurrentCup->mRoundNumber = -5;
            }
        }
    }

    mCurrentCup->mGameNumber = 0;
}

/**
 * Offset/Address/Size: 0x70D8 | 0x8017C77C | size: 0x11C
 */
void GameInfoManager::IncreaseGameNumber(bool shouldIncreaseRound)
{
    mCurrentCup->mGameNumber++;

    if (mCurrentCup->mGameNumber == GetNumGamesPerRound(mCurrentCup->mRoundNumber) && shouldIncreaseRound)
    {
        IncreaseRoundNumber();
    }
}

/**
 * Offset/Address/Size: 0x6E20 | 0x8017C4C4 | size: 0x2B8
 */
int GameInfoManager::GetNumHumanTeams()
{
    int numHumanTeams = 0;
    int i = 0;

    while (i < GetNumPlayingTeams())
    {
        eTeamID teamIndex = GetTeamStatsByIndex(i).mTeamIndex;
        if (mCurrentCup->mHumanTeams & (1 << teamIndex))
        {
            numHumanTeams++;
        }

        i++;
    }

    return numHumanTeams;
}

/**
 * Offset/Address/Size: 0x6D70 | 0x8017C414 | size: 0xB0
 */
BaseCup* GameInfoManager::GetCup(GameInfoManager::eGameModes mode)
{
    BaseCup* result = NULL;

    switch (mode)
    {
    case GM_MUSHROOM_CUP:
        result = &mMushroomCupSeries;
        break;
    case GM_FLOWER_CUP:
        result = &mFlowerCupSeries;
        break;
    case GM_STAR_CUP:
        result = &mStarCupSeries;
        break;
    case GM_BOWSER_CUP:
        if (mBowserCupSeries.mRoundNumber == -5 && mBowserCupKnockout.mRoundNumber != -5)
        {
            result = &mBowserCupKnockout;
        }
        else
        {
            result = &mBowserCupSeries;
        }
        break;
    case GM_SUPER_MUSHROOM_CUP:
        result = &mSuperMushroomCupSeries;
        break;
    case GM_SUPER_FLOWER_CUP:
        result = &mSuperFlowerCupSeries;
        break;
    case GM_SUPER_STAR_CUP:
        result = &mSuperStarCupSeries;
        break;
    case GM_SUPER_BOWSER_CUP:
        if (mSuperBowserCupSeries.mRoundNumber == -5 && mSuperBowserCupKnockout.mRoundNumber != -5)
        {
            result = &mSuperBowserCupKnockout;
        }
        else
        {
            result = &mSuperBowserCupSeries;
        }
        break;
    case GM_TOURNAMENT:
        result = mCustomTournamentInfo.m_cup;
        break;
    default:
        break;
    }

    return result;
}

/**
 * Offset/Address/Size: 0x6908 | 0x8017BFAC | size: 0x468
 */
bool GameInfoManager::IsUserQualified(GameInfoManager::eGameModes mode) const
{
    bool qualified = false;

    switch (mode)
    {
    case GM_FLOWER_CUP:
        qualified = mUserInfo.mIsFlowerCupUnlocked || (PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false));
        break;

    case GM_STAR_CUP:
        qualified = mUserInfo.mIsStarCupUnlocked;

        if (qualified == false)
        {
            qualified = (PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false));
        }

        break;

    case GM_BOWSER_CUP:
        qualified = (PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false));

        if (qualified)
        {
            qualified = true;
        }
        else
        {
            qualified = (mUserInfo.mTrophies[0] >> 0) & 0x1;
        }

        if (qualified)
        {
            qualified = (PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false));

            if (qualified)
            {
                qualified = true;
            }
            else
            {
                qualified = (mUserInfo.mTrophies[0] >> 1) & 0x1;
            }
        }

        if (qualified)
        {
            qualified = (PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false));

            if (qualified)
            {
                qualified = true;
            }
            else
            {
                qualified = (mUserInfo.mTrophies[0] >> 2) & 0x1;
            }
        }

        break;

    case GM_NUM_MODES:
        qualified = (PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false));

        if (qualified)
        {
            qualified = true;
        }
        else
        {
            qualified = (mUserInfo.mTrophies[0] >> 3) & 0x1;
        }

        break;

    default:
        break;
    }

    return qualified;
}

/**
 * Offset/Address/Size: 0x6794 | 0x8017BE38 | size: 0x174
 */
void GameInfoManager::SetMode(GameInfoManager::eGameModes mode)
{
    mCurrentMode = mode;
    mCupMatchRequirement = RESULT_INVALID;
    mIsInStrikers101Mode = false;
    mDidRoundJustEnd = false;
    mUserLastResults[mode] = RESULT_INVALID;

    switch (mCurrentMode)
    {
    case GM_MUSHROOM_CUP:
        mCurrentCup = &mMushroomCupSeries;
        mDoingKnockout = false;
        return;

    case GM_FLOWER_CUP:
        mCurrentCup = &mFlowerCupSeries;
        mDoingKnockout = false;
        return;

    case GM_STAR_CUP:
        mCurrentCup = &mStarCupSeries;
        mDoingKnockout = false;
        return;

    case GM_BOWSER_CUP:
        mCurrentCup = &mBowserCupSeries;
        mDoingKnockout = false;

        if (mCurrentCup->mRoundNumber == -5 && mBowserCupKnockout.mRoundNumber != -5)
        {
            mPreviousCup = mCurrentCup;
            mCurrentCup = &mBowserCupKnockout;
            mDoingKnockout = true;
        }

        return;

    case GM_SUPER_MUSHROOM_CUP:
        mCurrentCup = &mSuperMushroomCupSeries;
        mDoingKnockout = false;
        return;

    case GM_SUPER_FLOWER_CUP:
        mCurrentCup = &mSuperFlowerCupSeries;
        mDoingKnockout = false;
        return;

    case GM_SUPER_STAR_CUP:
        mCurrentCup = &mSuperStarCupSeries;
        mDoingKnockout = false;
        return;

    case GM_SUPER_BOWSER_CUP:
        mCurrentCup = &mSuperBowserCupSeries;
        mDoingKnockout = false;

        if (mCurrentCup->mRoundNumber == -5 && mSuperBowserCupKnockout.mRoundNumber != -5)
        {
            mPreviousCup = mCurrentCup;
            mCurrentCup = &mSuperBowserCupKnockout;
            mDoingKnockout = true;
        }

        return;

    case GM_TOURNAMENT:
        mCurrentCup = mCustomTournamentInfo.m_cup;

        if (mCustomTournamentInfo.m_tournMode == TM_LEAGUE)
        {
            mDoingKnockout = false;
        }
        else
        {
            mDoingKnockout = true;
        }

        if (!mCurrentCup->mCupStarted)
        {
            mCurrentCup->mUserSelectedTeam = TEAM_INVALID;
        }

        return;

    default:
        mCurrentCup = NULL;
        return;
    }
}

/**
 * Offset/Address/Size: 0x6664 | 0x8017BD08 | size: 0x130
 */
unsigned long GameInfoManager::GetMemoryCardDataSize() const
{
    unsigned long size = mMushroomCupSeries.GetSaveDataSize() + sizeof(UserInfo);
    size += mFlowerCupSeries.GetSaveDataSize();
    size += mStarCupSeries.GetSaveDataSize();
    size += mBowserCupSeries.GetSaveDataSize();
    size += mBowserCupKnockout.GetSaveDataSize();
    size += mSuperMushroomCupSeries.GetSaveDataSize();
    size += mSuperFlowerCupSeries.GetSaveDataSize();
    size += mSuperStarCupSeries.GetSaveDataSize();
    size += mSuperBowserCupSeries.GetSaveDataSize();
    size += mSuperBowserCupKnockout.GetSaveDataSize();
    size += mCustomTournamentInfo.GetSaveDataSize();
    return size;
}

/**
 * Offset/Address/Size: 0x642C | 0x8017BAD0 | size: 0x238
 */
void GameInfoManager::GetMemoryCardData(void* pData)
{
    memcpy(pData, &mUserInfo, sizeof(UserInfo));
    pData = (u8*)pData + sizeof(UserInfo);

    mMushroomCupSeries.SerializeData(pData);
    pData = (u8*)pData + mMushroomCupSeries.GetSaveDataSize();

    mFlowerCupSeries.SerializeData(pData);
    pData = (u8*)pData + mFlowerCupSeries.GetSaveDataSize();

    mStarCupSeries.SerializeData(pData);
    pData = (u8*)pData + mStarCupSeries.GetSaveDataSize();

    mBowserCupSeries.SerializeData(pData);
    pData = (u8*)pData + mBowserCupSeries.GetSaveDataSize();

    mBowserCupKnockout.SerializeData(pData);
    pData = (u8*)pData + mBowserCupKnockout.GetSaveDataSize();

    mSuperMushroomCupSeries.SerializeData(pData);
    pData = (u8*)pData + mSuperMushroomCupSeries.GetSaveDataSize();

    mSuperFlowerCupSeries.SerializeData(pData);
    pData = (u8*)pData + mSuperFlowerCupSeries.GetSaveDataSize();

    mSuperStarCupSeries.SerializeData(pData);
    pData = (u8*)pData + mSuperStarCupSeries.GetSaveDataSize();

    mSuperBowserCupSeries.SerializeData(pData);
    pData = (u8*)pData + mSuperBowserCupSeries.GetSaveDataSize();

    mSuperBowserCupKnockout.SerializeData(pData);
    pData = (u8*)pData + mSuperBowserCupKnockout.GetSaveDataSize();

    mCustomTournamentInfo.SerializeData(pData);
    mCustomTournamentInfo.GetSaveDataSize();
}

/**
 * Offset/Address/Size: 0x61F8 | 0x8017B89C | size: 0x234
 */
void GameInfoManager::SetMemoryCardData(void* pData)
{
    memcpy(&mUserInfo, pData, sizeof(UserInfo));
    pData = (u8*)pData + sizeof(UserInfo);

    mMushroomCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mMushroomCupSeries.GetSaveDataSize();

    mFlowerCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mFlowerCupSeries.GetSaveDataSize();

    mStarCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mStarCupSeries.GetSaveDataSize();

    mBowserCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mBowserCupSeries.GetSaveDataSize();

    mBowserCupKnockout.DeserializeData(pData);
    pData = (u8*)pData + mBowserCupKnockout.GetSaveDataSize();

    mSuperMushroomCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mSuperMushroomCupSeries.GetSaveDataSize();

    mSuperFlowerCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mSuperFlowerCupSeries.GetSaveDataSize();

    mSuperStarCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mSuperStarCupSeries.GetSaveDataSize();

    mSuperBowserCupSeries.DeserializeData(pData);
    pData = (u8*)pData + mSuperBowserCupSeries.GetSaveDataSize();

    mSuperBowserCupKnockout.DeserializeData(pData);
    pData = (u8*)pData + mSuperBowserCupKnockout.GetSaveDataSize();

    mCustomTournamentInfo.DeserializeData(pData);
    mCustomTournamentInfo.GetSaveDataSize();
}

/**
 * Offset/Address/Size: 0x61DC | 0x8017B880 | size: 0x1C
 */
u8 GameInfoManager::CheckSaveIDChanged(void* pData)
{
    return ((UserInfo*)pData)->mSaveID != mUserInfo.mSaveID;
}

/**
 * Offset/Address/Size: 0x60C0 | 0x8017B764 | size: 0x11C
 */
bool GameInfoManager::HasTrophy(eTrophyType trophyType) const
{
    if ((PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false)))
    {
        return true;
    }

    u8 trophyValue = mUserInfo.mTrophies[trophyType / 8];
    int bit = trophyType % 8;
    return (trophyValue & (1 << bit)) != 0;
}

/**
 * Offset/Address/Size: 0x5F60 | 0x8017B604 | size: 0x160
 */
eMilestoneColour GameInfoManager::GetMilestoneLevel(eTrophyType trophy) const
{
    eMilestoneColour returnValue = INVALID_MILESTONE_COLOUR;

    switch (trophy)
    {
    case TROPHY_VETERAN_CUP:
        if (mUserInfo.mNumGamesPlayed < 25)
            returnValue = MILESTONE_BLACK;
        else if (mUserInfo.mNumGamesPlayed < 50)
            returnValue = MILESTONE_BRONZE;
        else if (mUserInfo.mNumGamesPlayed < 100)
            returnValue = MILESTONE_SILVER;
        else
            returnValue = MILESTONE_GOLD;
        break;
    case TROPHY_SNIPER_CUP:
        if (mUserInfo.mNumGoalsScored < 75)
            returnValue = MILESTONE_BLACK;
        else if (mUserInfo.mNumGoalsScored < 150)
            returnValue = MILESTONE_BRONZE;
        else if (mUserInfo.mNumGoalsScored < 300)
            returnValue = MILESTONE_SILVER;
        else
            returnValue = MILESTONE_GOLD;
        break;
    case TROPHY_STRIKER_CUP:
        if (mUserInfo.mNumSTSAttempts < 25)
            returnValue = MILESTONE_BLACK;
        else if (mUserInfo.mNumSTSAttempts < 50)
            returnValue = MILESTONE_BRONZE;
        else if (mUserInfo.mNumSTSAttempts < 100)
            returnValue = MILESTONE_SILVER;
        else
            returnValue = MILESTONE_GOLD;
        break;
    case TROPHY_TACTICIAN_CUP:
        if (mUserInfo.mNumPerfectPasses < 75)
            returnValue = MILESTONE_BLACK;
        else if (mUserInfo.mNumPerfectPasses < 150)
            returnValue = MILESTONE_BRONZE;
        else if (mUserInfo.mNumPerfectPasses < 300)
            returnValue = MILESTONE_SILVER;
        else
            returnValue = MILESTONE_GOLD;
        break;
    case TROPHY_PARAMEDIC_CUP:
        if (mUserInfo.mNumHits < 250)
            returnValue = MILESTONE_BLACK;
        else if (mUserInfo.mNumHits < 500)
            returnValue = MILESTONE_BRONZE;
        else if (mUserInfo.mNumHits < 1000)
            returnValue = MILESTONE_SILVER;
        else
            returnValue = MILESTONE_GOLD;
        break;
    }

    return returnValue;
}

/**
 * Offset/Address/Size: 0x5F38 | 0x8017B5DC | size: 0x28
 */
bool GameInfoManager::IsInRegularCupMode() const
{
    switch (mCurrentMode)
    {
    case GM_MUSHROOM_CUP:
    case GM_FLOWER_CUP:
    case GM_STAR_CUP:
    case GM_BOWSER_CUP:
        return true;
    default:
        return false;
    }
}

/**
 * Offset/Address/Size: 0x5F10 | 0x8017B5B4 | size: 0x28
 */
bool GameInfoManager::IsInSuperCupMode() const
{
    switch (mCurrentMode)
    {
    case GM_SUPER_MUSHROOM_CUP:
    case GM_SUPER_FLOWER_CUP:
    case GM_SUPER_STAR_CUP:
    case GM_SUPER_BOWSER_CUP:
        return true;
    default:
        return false;
    }
}

/**
 * Offset/Address/Size: 0x5EC0 | 0x8017B564 | size: 0x50
 */
bool GameInfoManager::IsInCupMode() const
{
    return IsInRegularCupMode() || IsInSuperCupMode();
}

/**
 * Offset/Address/Size: 0x5E5C | 0x8017B500 | size: 0x64
 */
bool GameInfoManager::IsInCupOrTournamentMode() const
{
    return IsInTournamentMode() || IsInCupMode();
}

/**
 * Offset/Address/Size: 0x5E48 | 0x8017B4EC | size: 0x14
 */
bool GameInfoManager::IsInDemoMode() const
{
    return mCurrentMode == GM_DEMO;
}

/**
 * Offset/Address/Size: 0x5E38 | 0x8017B4DC | size: 0x10
 */
bool GameInfoManager::IsInFriendlyMode() const
{
    return mCurrentMode == GM_FRIENDLY;
}

/**
 * Offset/Address/Size: 0x5E24 | 0x8017B4C8 | size: 0x14
 */
bool GameInfoManager::IsInTournamentMode() const
{
    return mCurrentMode == GM_TOURNAMENT;
}

/**
 * Offset/Address/Size: 0x5E1C | 0x8017B4C0 | size: 0x8
 */
AudioSettings& GameInfoManager::GetAudioOptions()
{
    return mUserInfo.mAudioOptions;
}

/**
 * Offset/Address/Size: 0x5DE0 | 0x8017B484 | size: 0x3C
 */
const GameplaySettings& GameInfoManager::GetGameplayOptions() const
{
    if (mUseCurGameSettings)
    {
        return mCurGameGameplayOptions;
    }

    if (mCurrentMode == GM_FRIENDLY || mCurrentMode == GM_DEMO)
    {
        return mUserInfo.mGameplayOptions;
    }

    return mCurrentCup->mCupSettings;
}

/**
 * Offset/Address/Size: 0x5DD8 | 0x8017B47C | size: 0x8
 */
const PowerupSettings& GameInfoManager::GetPowerupOptions() const
{
    FORCE_DONT_INLINE;
    return mUserInfo.mPowerupOptions;
}

/**
 * Offset/Address/Size: 0x5040 | 0x8017A6E4 | size: 0xD98
 */
void GameInfoManager::OnPreGameState()
{
    mCurGameGameplayOptions = GetGameplayOptions();

    mUseCurGameSettings = true;

    if (Config::Global().Exists("team1"))
    {
        BasicString<char, Detail::TempStringAllocator> teamString = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >(
            "team1", BasicString<char, Detail::TempStringAllocator>("mario"));

        mGameInfo[mCurrentMode]->mTeamIndex[0] = ConvertToTeamID(teamString.c_str());
    }

    if (Config::Global().Exists("team2"))
    {
        BasicString<char, Detail::TempStringAllocator> teamString = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >(
            "team2", BasicString<char, Detail::TempStringAllocator>("luigi"));

        mGameInfo[mCurrentMode]->mTeamIndex[1] = ConvertToTeamID(teamString.c_str());
    }

    if (Config::Global().Exists("soak_diff"))
    {
        mCurGameGameplayOptions.SkillLevel = (GameplaySettings::eSkillLevel)GetConfigInt(Config::Global(), "soak_diff", 2);
    }

    if (Config::Global().Exists("sidekick1"))
    {
        BasicString<char, Detail::TempStringAllocator> sidekickString
            = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >(
                "sidekick1", BasicString<char, Detail::TempStringAllocator>("toad"));

        mGameInfo[mCurrentMode]->mSidekickIndex[0] = ConvertToSidekickID(sidekickString.c_str());
    }

    if (Config::Global().Exists("sidekick2"))
    {
        BasicString<char, Detail::TempStringAllocator> sidekickString
            = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >(
                "sidekick2", BasicString<char, Detail::TempStringAllocator>("koopa"));

        mGameInfo[mCurrentMode]->mSidekickIndex[1] = ConvertToSidekickID(sidekickString.c_str());
    }

    if (IsInCupMode())
    {
        OnPreCupGameState();
    }

    if (mCurrentMode == GM_DEMO)
    {
        mCurGameGameplayOptions.GameTime = 120;
        mCurGameGameplayOptions.SkillLevel = GameplaySettings::PROFESSIONAL;
        mCurGameGameplayOptions.PowerUps = true;
        mCurGameGameplayOptions.Shoot2Score = true;
        mCurGameGameplayOptions.BowserAttackEnabled = true;
    }
    else if (mIsInStrikers101Mode)
    {
        mCurGameGameplayOptions.GameTime = 59940;
        mCurGameGameplayOptions.PowerUps = true;
        mCurGameGameplayOptions.Shoot2Score = true;
        mCurGameGameplayOptions.BowserAttackEnabled = false;
    }
    else if (g_e3_Build)
    {
        mCurGameGameplayOptions.GameTime = 240;
        mCurGameGameplayOptions.SkillLevel = GameplaySettings::ROOKIE;
    }

    ApplyDifficultySettings();

    if (Config::Global().Exists("stadium"))
    {
        BasicString<char, Detail::TempStringAllocator> stadium;
        stadium = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >(
            "stadium", BasicString<char, Detail::TempStringAllocator>());
        const char* userStadium = stadium.c_str();

        mGameInfo[mCurrentMode]->mStadiumIndex = (eStadiumID)-1;
        for (int i = 0; i < MAX_STADIUMS; i++)
        {
            if (nlStrICmp(TheWorldLoader.GetStadiumFilename((eStadiumID)i), userStadium) == 0)
            {
                mGameInfo[mCurrentMode]->mStadiumIndex = (eStadiumID)i;
                break;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x500C | 0x8017A6B0 | size: 0x34
 */
void GameInfoManager::OnPostGameState()
{
    mUseCurGameSettings = false;
    mUserInfo.mAudioOptions.ApplySettings(true, false);
}

/**
 * Offset/Address/Size: 0x4E7C | 0x8017A520 | size: 0x190
 */
void GameInfoManager::ApplyDifficultySettings()
{
    static const eDifficultyID DifficultyMap[5][2] = {
        { DIFF_HUMAN, DIFF_BRAINDEAD },
        { DIFF_HUMAN, DIFF_EASY },
        { DIFF_HUMAN, DIFF_MEDIUM },
        { DIFF_HUMAN, DIFF_HARD },
        { DIFF_HUMAN, DIFF_VERYHARD },
    };
    unsigned char humansOnSide[2] = { 0, 0 };
    int i;

    for (i = 0; i < 4; i++)
    {
        if (GetPlayingSide(i) == 0)
        {
            humansOnSide[0] = 1;
        }
        else if (GetPlayingSide(i) == 1)
        {
            humansOnSide[1] = 1;
        }
    }

    GameplaySettings::eSkillLevel skillLevel;
    if (mIsInStrikers101Mode)
    {
        skillLevel = GameplaySettings::TRAINING;
    }
    else
    {
        GameplaySettings* settings;
        if (mUseCurGameSettings)
        {
            settings = &mCurGameGameplayOptions;
        }
        else if (mCurrentMode == GM_FRIENDLY || mCurrentMode == GM_DEMO)
        {
            settings = &mUserInfo.mGameplayOptions;
        }
        else
        {
            settings = &mCurrentCup->mCupSettings;
        }
        skillLevel = settings->SkillLevel;
    }

    mCurrentDifficulty[0] = DifficultyMap[skillLevel][humansOnSide[0] ? 0 : 1];
    mCurrentDifficulty[1] = DifficultyMap[skillLevel][humansOnSide[1] ? 0 : 1];
}

/**
 * Offset/Address/Size: 0x4E14 | 0x8017A4B8 | size: 0x68
 */
eTrophyType GameInfoManager::GetTrophyTypeByCurrentMode() const
{
    eTrophyType mode = INVALID_TROPHY;

    switch (mCurrentMode)
    {
    case GM_MUSHROOM_CUP:
        mode = TROPHY_MUSHROOM_CUP;
        break;
    case GM_FLOWER_CUP:
        mode = TROPHY_FLOWER_CUP;
        break;
    case GM_STAR_CUP:
        mode = TROPHY_STAR_CUP;
        break;
    case GM_BOWSER_CUP:
        mode = TROPHY_BOWSER_CUP;
        break;
    case GM_SUPER_MUSHROOM_CUP:
        mode = TROPHY_SUPER_MUSHROOM_CUP;
        break;
    case GM_SUPER_FLOWER_CUP:
        mode = TROPHY_SUPER_FLOWER_CUP;
        break;
    case GM_SUPER_STAR_CUP:
        mode = TROPHY_SUPER_STAR_CUP;
        break;
    case GM_SUPER_BOWSER_CUP:
        mode = TROPHY_SUPER_BOWSER_CUP;
        break;
    }

    return mode;
}

/**
 * Offset/Address/Size: 0x4DFC | 0x8017A4A0 | size: 0x18
 */
bool GameInfoManager::IsPossibleCupMatch() const
{
    return mCupMatchRequirement != RESULT_INVALID;
}

/**
 * Offset/Address/Size: 0x38E8 | 0x80178F8C | size: 0x1514
 */
void GameInfoManager::OnPreCupGameState()
{
    eTrophyType tourneyCup = INVALID_TROPHY;
    int i;

    switch (mCurrentMode)
    {
    case GM_MUSHROOM_CUP:
        tourneyCup = TROPHY_MUSHROOM_CUP;
        break;
    case GM_FLOWER_CUP:
        tourneyCup = TROPHY_FLOWER_CUP;
        break;
    case GM_STAR_CUP:
        tourneyCup = TROPHY_STAR_CUP;
        break;
    case GM_BOWSER_CUP:
        tourneyCup = TROPHY_BOWSER_CUP;
        break;
    case GM_SUPER_MUSHROOM_CUP:
        tourneyCup = TROPHY_SUPER_MUSHROOM_CUP;
        break;
    case GM_SUPER_FLOWER_CUP:
        tourneyCup = TROPHY_SUPER_FLOWER_CUP;
        break;
    case GM_SUPER_STAR_CUP:
        tourneyCup = TROPHY_SUPER_STAR_CUP;
        break;
    case GM_SUPER_BOWSER_CUP:
        tourneyCup = TROPHY_SUPER_BOWSER_CUP;
        break;
    }

    mPreGameUnlockedState = 0;

    {
        u32 unlockedState;
        if (CheckUnlockStatus(isKongaUnlocked, mUserInfo.mTrophies[0], 0))
        {
            unlockedState = mPreGameUnlockedState | 0x1;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (CheckUnlockStatus(isYoshiUnlocked, mUserInfo.mTrophies[0], 1))
        {
            unlockedState = mPreGameUnlockedState | 0x2;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (CheckUnlockStatus(isForbiddenUnlocked, mUserInfo.mTrophies[0], 2))
        {
            unlockedState = mPreGameUnlockedState | 0x4;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (IsSuperCupModeUnlocked())
        {
            unlockedState = mPreGameUnlockedState | 0x8;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (CheckUnlockStatus(isSuperTeamUnlocked, mUserInfo.mTrophies[0], 3))
        {
            unlockedState = mPreGameUnlockedState | 0x10;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }

    {
        u32 unlockedState = mPreGameUnlockedState | 0x20;
        mPreGameUnlockedState = unlockedState;
        mPreGameUnlockedState = unlockedState;
    }

    {
        u32 unlockedState;
        if (CheckUnlockStatus(isSuperStadUnlocked, mUserInfo.mTrophies[0], 3))
        {
            unlockedState = mPreGameUnlockedState | 0x40;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (CheckUnlockStatus(isAllSTSUnlocked, mUserInfo.mTrophies[0], 4))
        {
            unlockedState = mPreGameUnlockedState | 0x80;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (CheckUnlockStatus(isTiltUnlocked, mUserInfo.mTrophies[0], 5))
        {
            unlockedState = mPreGameUnlockedState | 0x100;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (CheckUnlockStatus(isGoalieUnlocked, mUserInfo.mTrophies[0], 6))
        {
            unlockedState = mPreGameUnlockedState | 0x2000;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (CheckUnlockStatus(isUnlimitedUnlocked, mUserInfo.mTrophies[0], 7))
        {
            unlockedState = mPreGameUnlockedState | 0x200;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }

    {
        u32 unlockedState;
        if (mUserInfo.mIsFlowerCupUnlocked)
        {
            unlockedState = mPreGameUnlockedState | 0x400;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }
    {
        u32 unlockedState;
        if (mUserInfo.mIsStarCupUnlocked)
        {
            unlockedState = mPreGameUnlockedState | 0x800;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }

    bool allBasicUnlocked = CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 0)
                         && CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 1)
                         && CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 2);
    {
        u32 unlockedState;
        if (allBasicUnlocked)
        {
            unlockedState = mPreGameUnlockedState | 0x1000;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }

    {
        u32 unlockedState;
        if (CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 3))
        {
            unlockedState = mPreGameUnlockedState | 0x4000;
            mPreGameUnlockedState = unlockedState;
        }
        else
        {
            unlockedState = mPreGameUnlockedState;
        }
        mPreGameUnlockedState = unlockedState;
    }

    {
        bool unlocked;
        if ((PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false)))
        {
            unlocked = true;
        }
        else
        {
            unlocked = mUserInfo.mTrophies[tourneyCup / 8] & (1 << (tourneyCup % 8));
        }
        if (unlocked)
        {
            mDisplayTrophy[0] = false;
        }
        else
        {
            mDisplayTrophy[0] = true;
        }
    }

    for (i = 0; i < 5; i++)
    {
        mTrophyColourState[i] = GetMilestoneLevel(MILESTONES[i]);
    }

    if ((mCurrentMode == GM_BOWSER_CUP
            && !CheckUnlockStatus(isSuperTeamUnlocked, mUserInfo.mTrophies[0], 3)
            && mCurrentCup->mRoundNumber == -1)
        || (CheckUnlockStatus(isSuperTeamUnlocked, mUserInfo.mTrophies[0], 3)
            && mCurrentCup->mRoundNumber == -2))
    {
        mCupMatchRequirement = RESULT_USER_OT_WINS;
    }
    else if (mCurrentMode == GM_SUPER_BOWSER_CUP && mCurrentCup->mRoundNumber == -2)
    {
        mCupMatchRequirement = RESULT_USER_OT_WINS;
    }
    else if (mCurrentMode != GM_BOWSER_CUP && mCurrentMode != GM_SUPER_BOWSER_CUP
             && mCurrentCup->mRoundNumber == mCurrentCup->GetNumRounds() - 1)
    {
        TeamStats userTeam;
        TeamStats opponentTeam;
        TeamStats highestTeam;
        int highPoints;
        int j;
        int teamBuf[16];
        int copiedStatsBuffer[16];
        TeamStats* team = (TeamStats*)teamBuf;
        TeamStats* copiedStats = (TeamStats*)copiedStatsBuffer;

        highPoints = 0;

        for (j = 0; j < GetNumPlayingTeams(); j++)
        {
            if (mCurrentMode == GM_BOWSER_CUP)
            {
                *copiedStats = *mBowserCupSeries.GetTeamStats((unsigned short)j);
            }
            else if (mCurrentMode == GM_SUPER_BOWSER_CUP)
            {
                *copiedStats = *mSuperBowserCupSeries.GetTeamStats((unsigned short)j);
            }
            else
            {
                *copiedStats = *mCurrentCup->GetTeamStats((unsigned short)j);
            }

            *team = *copiedStats;

            if (team->mNumPoints > highPoints)
            {
                highestTeam = *team;
                highPoints = team->mNumPoints;
            }

            if (team->mTeamIndex == mCurrentCup->mUserSelectedTeam)
            {
                userTeam = *team;
            }
            else
            {
                BasicGameInfo* gameInfo = mGameInfo[mCurrentMode];
                if (team->mTeamIndex == (gameInfo == NULL ? TEAM_INVALID : gameInfo->mTeamIndex[0])
                    || team->mTeamIndex == (gameInfo == NULL ? TEAM_INVALID : gameInfo->mTeamIndex[1]))
                {
                    opponentTeam = *team;
                }
            }
        }

        if (userTeam.mNumPoints >= highestTeam.mNumPoints && userTeam.mNumPoints >= opponentTeam.mNumPoints + 3)
        {
            mCupMatchRequirement = RESULT_CUP_WIN;
        }
        else if (userTeam.mNumPoints + 1 >= highestTeam.mNumPoints && userTeam.mNumPoints + 1 >= opponentTeam.mNumPoints + 3)
        {
            mCupMatchRequirement = RESULT_USER_OT_LOSES;
        }
        else if (userTeam.mNumPoints + 3 >= highestTeam.mNumPoints && userTeam.mNumPoints + 3 >= opponentTeam.mNumPoints + 1)
        {
            mCupMatchRequirement = RESULT_USER_OT_WINS;
        }
        else if (userTeam.mNumPoints + 3 >= highestTeam.mNumPoints && userTeam.mNumPoints + 3 >= opponentTeam.mNumPoints)
        {
            mCupMatchRequirement = RESULT_USER_WINS;
        }
        else
        {
            mCupMatchRequirement = RESULT_USER_LOSES;
        }
    }
    else
    {
        mCupMatchRequirement = RESULT_INVALID;
    }
}

/**
 * Offset/Address/Size: 0x256C | 0x80177C10 | size: 0x137C
 */
void GameInfoManager::OnPostCupGameState()
{
    eTrophyType tourneyCup = INVALID_TROPHY;
    int i;

    switch (mCurrentMode)
    {
    case GM_MUSHROOM_CUP:
        tourneyCup = TROPHY_MUSHROOM_CUP;
        break;
    case GM_FLOWER_CUP:
        tourneyCup = TROPHY_FLOWER_CUP;
        break;
    case GM_STAR_CUP:
        tourneyCup = TROPHY_STAR_CUP;
        break;
    case GM_BOWSER_CUP:
        tourneyCup = TROPHY_BOWSER_CUP;
        break;
    case GM_SUPER_MUSHROOM_CUP:
        tourneyCup = TROPHY_SUPER_MUSHROOM_CUP;
        break;
    case GM_SUPER_FLOWER_CUP:
        tourneyCup = TROPHY_SUPER_FLOWER_CUP;
        break;
    case GM_SUPER_STAR_CUP:
        tourneyCup = TROPHY_SUPER_STAR_CUP;
        break;
    case GM_SUPER_BOWSER_CUP:
        tourneyCup = TROPHY_SUPER_BOWSER_CUP;
        break;
    }

    bool hasTrophy = mDisplayTrophy[0];

    if (hasTrophy)
    {
        if ((PortUnlockAll() || GetConfigBool(Config::Global(), "givealltrophies", false)))
        {
            hasTrophy = true;
        }
        else
        {
            int trophy = (int)tourneyCup;
            hasTrophy = (mUserInfo.mTrophies[trophy / 8] & (1 << (trophy % 8))) != 0;
        }
    }

    mDisplayTrophy[0] = hasTrophy;

    for (i = 0; i < 5; i++)
    {
        eMilestoneColour level = GetMilestoneLevel(MILESTONES[i]);
        mDisplayTrophy[i + 1] = (mTrophyColourState[i] != level);
    }

    mCupMatchRequirement = RESULT_INVALID;

    if ((mCurrentMode == GM_MUSHROOM_CUP || mCurrentMode == GM_FLOWER_CUP)
        && !(mCurrentMode == GM_MUSHROOM_CUP && mUserInfo.mIsFlowerCupUnlocked)
        && !(mCurrentMode == GM_FLOWER_CUP && mUserInfo.mIsStarCupUnlocked))
    {
        if (mCurrentCup->mRoundNumber == (mCurrentCup->GetNumRounds() - 1))
        {
            TeamStats allStats[8];
            int userIndex = -1;
            int rankIndices[8];
            int userRank;
            int numTeams;
            int i;
            int j;
            int copiedStatsBuffer[16];
            TeamStats* copiedStats = (TeamStats*)copiedStatsBuffer;

            numTeams = GetNumPlayingTeams();

            for (i = 0; i < numTeams; i++)
            {
                if (mCurrentMode == GM_BOWSER_CUP)
                {
                    *copiedStats = *mBowserCupSeries.GetTeamStats((unsigned short)i);
                }
                else if (mCurrentMode == GM_SUPER_BOWSER_CUP)
                {
                    *copiedStats = *mSuperBowserCupSeries.GetTeamStats((unsigned short)i);
                }
                else
                {
                    *copiedStats = *mCurrentCup->GetTeamStats((unsigned short)i);
                }

                allStats[i] = *copiedStats;

                eTeamID teamIndex = allStats[i].mTeamIndex;

                if (teamIndex == mCurrentCup->mUserSelectedTeam)
                {
                    userIndex = i;
                }
            }

            nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(allStats, numTeams, rankIndices, numTeams);

            for (j = 0; j < numTeams; j++)
            {
                if (userIndex == rankIndices[j])
                {
                    userRank = j;
                }
            }

            if (mCurrentMode == GM_MUSHROOM_CUP && (unsigned int)userRank < 3)
            {
                mUserInfo.mIsFlowerCupUnlocked = true;
            }
            else if (mCurrentMode == GM_FLOWER_CUP && (unsigned int)userRank < 3)
            {
                mUserInfo.mIsStarCupUnlocked = true;
            }
        }
    }

    IncreaseRoundNumber();

    DetermineNextCupScreen();

    mUnlockedTriggers = 0;

    u32 unlockedState = CheckUnlockStatus(isKongaUnlocked, mUserInfo.mTrophies[0], 0) ? 0x1 : 0;
    if (CheckUnlockStatus(isYoshiUnlocked, mUserInfo.mTrophies[0], 1))
    {
        unlockedState |= 0x2;
    }
    if (CheckUnlockStatus(isForbiddenUnlocked, mUserInfo.mTrophies[0], 2))
    {
        unlockedState |= 0x4;
    }
    if (IsSuperCupModeUnlocked())
    {
        unlockedState |= 0x8;
    }
    if (CheckUnlockStatus(isSuperTeamUnlocked, mUserInfo.mTrophies[0], 3))
    {
        unlockedState |= 0x10;
    }

    u32 unlockedStateWithUnknown = unlockedState | 0x20;

    if (CheckUnlockStatus(isSuperStadUnlocked, mUserInfo.mTrophies[0], 3))
    {
        unlockedState = unlockedStateWithUnknown | 0x40;
    }
    else
    {
        unlockedState = unlockedStateWithUnknown;
    }
    if (CheckUnlockStatus(isAllSTSUnlocked, mUserInfo.mTrophies[0], 4))
    {
        unlockedState |= 0x80;
    }
    if (CheckUnlockStatus(isGoalieUnlocked, mUserInfo.mTrophies[0], 6))
    {
        unlockedState |= 0x2000;
    }
    if (CheckUnlockStatus(isTiltUnlocked, mUserInfo.mTrophies[0], 5))
    {
        unlockedState |= 0x100;
    }
    if (CheckUnlockStatus(isUnlimitedUnlocked, mUserInfo.mTrophies[0], 7))
    {
        unlockedState |= 0x200;
    }

    if (mUserInfo.mIsFlowerCupUnlocked)
    {
        unlockedState |= 0x400;
    }
    if (mUserInfo.mIsStarCupUnlocked)
    {
        unlockedState |= 0x800;
    }

    bool allBasicUnlocked = CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 0)
                         && CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 1)
                         && CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 2);

    if (allBasicUnlocked)
    {
        unlockedState |= 0x1000;
    }

    if (CheckUnlockStatusNoGlobal(mUserInfo.mTrophies[0], 3))
    {
        unlockedState |= 0x4000;
    }

    if ((mPreGameUnlockedState & 0x1) != (unlockedState & 0x1))
    {
        mUnlockedTriggers |= 0x1;
    }
    if ((mPreGameUnlockedState & 0x2) != (unlockedState & 0x2))
    {
        mUnlockedTriggers |= 0x2;
    }
    if ((mPreGameUnlockedState & 0x4) != (unlockedState & 0x4))
    {
        mUnlockedTriggers |= 0x4;
    }
    if ((mPreGameUnlockedState & 0x8) != (unlockedState & 0x8))
    {
        mUnlockedTriggers |= 0x8;
    }
    if ((mPreGameUnlockedState & 0x10) != (unlockedState & 0x10))
    {
        mUnlockedTriggers |= 0x10;
    }
    if ((mPreGameUnlockedState & 0x20) != (unlockedState & 0x20))
    {
        mUnlockedTriggers |= 0x20;
    }
    if ((mPreGameUnlockedState & 0x40) != (unlockedState & 0x40))
    {
        mUnlockedTriggers |= 0x40;
    }
    if ((mPreGameUnlockedState & 0x80) != (unlockedState & 0x80))
    {
        mUnlockedTriggers |= 0x80;
    }
    if ((mPreGameUnlockedState & 0x2000) != (unlockedState & 0x2000))
    {
        mUnlockedTriggers |= 0x2000;
    }
    if ((mPreGameUnlockedState & 0x100) != (unlockedState & 0x100))
    {
        mUnlockedTriggers |= 0x100;
    }
    if ((mPreGameUnlockedState & 0x200) != (unlockedState & 0x200))
    {
        mUnlockedTriggers |= 0x200;
    }
    if ((mPreGameUnlockedState & 0x400) != (unlockedState & 0x400))
    {
        mUnlockedTriggers |= 0x400;
    }
    if ((mPreGameUnlockedState & 0x800) != (unlockedState & 0x800))
    {
        mUnlockedTriggers |= 0x800;
    }
    if ((mPreGameUnlockedState & 0x1000) != (unlockedState & 0x1000))
    {
        mUnlockedTriggers |= 0x1000;
    }
    if ((mPreGameUnlockedState & 0x4000) != (unlockedState & 0x4000))
    {
        mUnlockedTriggers |= 0x4000;
    }
}

/**
 * Offset/Address/Size: 0x23FC | 0x80177AA0 | size: 0x170
 */
void GameInfoManager::DetermineNextCupScreen()
{
    FORCE_DONT_INLINE;

    int i = 0;
    while (i < 5)
    {
        if (mDisplayTrophy[i + 1] == 1)
        {
            MilestoneTrophyScene* scene = (MilestoneTrophyScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MILESTONE_TROPHY, SCREEN_NOTHING, false);
            scene->CreateTrophyScene(MILESTONES[i], ButtonComponent::BS_A_ONLY, true);
            mDisplayTrophy[i + 1] = false;
            Audio::gWorldSFX.Play(Audio::WORLDSFX_FE_ACCEPT_WARIO, 100.0f, -1.0f, true, 100.0f);
            return;
        }
        i++;
    }

    if (mCurrentCup->mRoundNumber == -5)
    {
        TimeStampCupEnd();
    }

    bool isSuper;
    mDisplayTrophy[0] = (isSuper = true);
    if (mCurrentMode < GM_TOURNAMENT)
    {
        if (mCurrentMode >= GM_SUPER_MUSHROOM_CUP)
        {
        }
        else
        {
            // Match target's shared false-path branch instead of duplicating the assignment.
            goto notSuperCup;
        }
        goto superCupDone;
    }
    else
    {
    notSuperCup:
        isSuper = false;
    }
superCupDone:

    SceneList nextScene = isSuper ? SCENE_SUPER_CUP_STANDINGS_ANIM : SCENE_CUP_STANDINGS_ANIM;
    if (mCurrentCup->mRoundNumber == -1)
    {
        nextScene = SCENE_CUP_SUPER_TEAM;
    }
    if (nextScene != SCENE_CUP_SUPER_TEAM)
    {
        CupHubScene* hub = (CupHubScene*)nlSingleton<GameSceneManager>::Instance()->Push(nextScene, SCREEN_NOTHING, false);
        hub->mDoAutoSave = true;
    }
    else
    {
        nlSingleton<GameSceneManager>::Instance()->Push(nextScene, SCREEN_NOTHING, false);
    }
}

/**
 * Offset/Address/Size: 0x1DA8 | 0x8017744C | size: 0x654
 */
signed char GameInfoManager::DetermineUserPlacement(Spoil* pSpoil)
{
    signed char userplace = 0;
    eGameModes mode = mCurrentMode;
    TeamStats allStats[8];
    int rankIndices[8];
    int userIndex = -1;
    int numTeams;
    int i;
    int j;

    numTeams = GetNumPlayingTeams();

    for (i = 0; i < (u16)numTeams; i++)
    {
        allStats[i] = GetTeamStatsByIndex(i);

        eTeamID teamIndex = allStats[i].mTeamIndex;
        if (teamIndex == mCurrentCup->mUserSelectedTeam)
        {
            userIndex = i;
        }
    }

    if ((mode == GM_BOWSER_CUP || mode == GM_SUPER_BOWSER_CUP) && mUserLastResults[mCurrentMode] != RESULT_USER_DOES_NOT_PLAYOFF_QUALIFY)
    {
        if (mUserLastResults[mCurrentMode] == RESULT_CUP_WIN)
        {
            userplace = 0;
        }
        else
        {
            userplace = -2;
        }
    }
    else if (mode == GM_TOURNAMENT && mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
    {
        if (mUserLastResults[mCurrentMode] == RESULT_CUP_WIN)
        {
            userplace = 0;
        }
        else
        {
            userplace = -2;
        }
    }
    else
    {
        StatsTracker::Instance()->GetSortedTeamStats(allStats, numTeams, rankIndices, numTeams);

        for (j = 0; j < numTeams; j++)
        {
            if (userIndex == rankIndices[j])
            {
                userplace = (signed char)j;
                break;
            }
        }
    }

    if (pSpoil != NULL)
    {
        pSpoil->mNumWins += allStats[userIndex].mNumWins;
        pSpoil->mNumLosses += allStats[userIndex].mNumLosses;
        pSpoil->mNumLosses += allStats[userIndex].mNumOTLosses;

        if (pSpoil->mNumWins > 999)
        {
            pSpoil->mNumWins = 999;
        }
        if (pSpoil->mNumLosses > 999)
        {
            pSpoil->mNumLosses = 999;
        }
        if (pSpoil->mNumCupWins > 999)
        {
            pSpoil->mNumCupWins = 999;
        }
    }

    return userplace;
}

/**
 * Offset/Address/Size: 0x196C | 0x80177010 | size: 0x43C
 */
void GameInfoManager::TimeStampCupEnd()
{
    struct CupRecordRaw
    {
        OSCalendarTime mDate;
        int mPlace;
        eTeamID mTeam;
        GameplaySettings::eSkillLevel mDifficulty;
    };

    eTrophyType trophy = INVALID_TROPHY;

    switch (mCurrentMode)
    {
    case GM_MUSHROOM_CUP:
        trophy = TROPHY_MUSHROOM_CUP;
        break;
    case GM_FLOWER_CUP:
        trophy = TROPHY_FLOWER_CUP;
        break;
    case GM_STAR_CUP:
        trophy = TROPHY_STAR_CUP;
        break;
    case GM_BOWSER_CUP:
        trophy = TROPHY_BOWSER_CUP;
        break;
    case GM_SUPER_MUSHROOM_CUP:
        trophy = TROPHY_SUPER_MUSHROOM_CUP;
        break;
    case GM_SUPER_FLOWER_CUP:
        trophy = TROPHY_SUPER_FLOWER_CUP;
        break;
    case GM_SUPER_STAR_CUP:
        trophy = TROPHY_SUPER_STAR_CUP;
        break;
    case GM_SUPER_BOWSER_CUP:
        trophy = TROPHY_SUPER_BOWSER_CUP;
        break;
    default:
        break;
    }

    UserInfo* userInfo = &mUserInfo;
    Spoil* pSpoil = &userInfo->mSpoils[trophy];
    CupRecord record;
    CupRecordRaw copyRecord;

    OSTicksToCalendarTime(OSGetTime(), &record.mDate);

    record.mTeam = mCurrentCup->mUserSelectedTeam;
    record.mPlace = DetermineUserPlacement(pSpoil);
    record.mDifficulty = mCurrentCup->mCupSettings.SkillLevel;

    copyRecord = *(CupRecordRaw*)&record;

    if (pSpoil->mNumRecords < 10)
    {
        pSpoil->mNumRecords++;
    }

    int i = pSpoil->mNumRecords - 1;
    CupRecord* src;
    CupRecord* dest = &pSpoil->mCupHistory[i];
    for (; i > 0; --i, --dest)
    {
        src = &pSpoil->mCupHistory[i - 1];
        *dest = *src;
    }

    pSpoil->mCupHistory[0] = *(CupRecord*)&copyRecord;

    if (pSpoil->mNumWins > 999)
    {
        pSpoil->mNumWins = 999;
    }

    if (pSpoil->mNumLosses > 999)
    {
        pSpoil->mNumLosses = 999;
    }

    if (pSpoil->mNumCupWins > 999)
    {
        pSpoil->mNumCupWins = 999;
    }

    pSpoil->mCurrentChamp = FindWinningTeam();

    pSpoil->mIsCPUChamp = (pSpoil->mCurrentChamp != mCurrentCup->mUserSelectedTeam);

    if (pSpoil->mCurrentChamp == mCurrentCup->mUserSelectedTeam)
    {
        pSpoil->mNumCupWins++;

        if (pSpoil->mNumWins > 999)
        {
            pSpoil->mNumWins = 999;
        }

        if (pSpoil->mNumLosses > 999)
        {
            pSpoil->mNumLosses = 999;
        }

        if (pSpoil->mNumCupWins > 999)
        {
            pSpoil->mNumCupWins = 999;
        }
    }
}

/**
 * Offset/Address/Size: 0x13E4 | 0x80176A88 | size: 0x588
 */
eTeamID GameInfoManager::FindWinningTeam()
{
    if (mDoingKnockout)
    {
        BaseCup* pCup = mCurrentCup;
        short lastRound;
        if (mCurrentMode == GM_BOWSER_CUP || mCurrentMode == GM_SUPER_BOWSER_CUP)
        {
            lastRound = 1;
        }
        else
        {
            lastRound = (short)(pCup->GetNumRounds() - 1);
        }
        BasicGameInfo* pGameInfo = pCup->GetGameInfo((int)lastRound, 0);
        if (pGameInfo->mFinalScore[0] > pGameInfo->mFinalScore[1])
        {
            return pGameInfo->mTeamIndex[0];
        }
        else
        {
            return pGameInfo->mTeamIndex[1];
        }
    }

    TeamStats allStats[8];
    int rankIndices[8];
    int copiedStatsBuffer[16];
    int numTeams;
    int i;

    unsigned short teamCount;
    if (mCurrentMode == GM_BOWSER_CUP || mCurrentMode == GM_SUPER_BOWSER_CUP)
    {
        teamCount = 8;
    }
    else
    {
        teamCount = mCurrentCup->GetNumTeams();
    }
    numTeams = teamCount;

    TeamStats* copiedStats = (TeamStats*)copiedStatsBuffer;
    for (i = 0; i < numTeams; i++)
    {
        if (mCurrentMode == GM_BOWSER_CUP)
        {
            *copiedStats = *mBowserCupSeries.GetTeamStats((u16)i);
        }
        else if (mCurrentMode == GM_SUPER_BOWSER_CUP)
        {
            *copiedStats = *mSuperBowserCupSeries.GetTeamStats((u16)i);
        }
        else
        {
            *copiedStats = *mCurrentCup->GetTeamStats((u16)i);
        }
        allStats[i] = *copiedStats;
    }

    nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(allStats, numTeams, rankIndices, numTeams);

    return allStats[rankIndices[0]].mTeamIndex;
}

/**
 * Offset/Address/Size: 0x12F8 | 0x8017699C | size: 0xEC
 */
bool GameInfoManager::IsKongaUnlocked() const
{
    return CheckUnlockStatus(isKongaUnlocked, mUserInfo.mTrophies[0], 0);
}

/**
 * Offset/Address/Size: 0x120C | 0x801768B0 | size: 0xEC
 */
bool GameInfoManager::IsYoshiUnlocked() const
{
    return CheckUnlockStatus(isYoshiUnlocked, mUserInfo.mTrophies[0], 1);
}

/**
 * Offset/Address/Size: 0x1120 | 0x801767C4 | size: 0xEC
 */
bool GameInfoManager::IsForbiddenUnlocked() const
{
    return CheckUnlockStatus(isForbiddenUnlocked, mUserInfo.mTrophies[0], 2);
}

/**
 * Offset/Address/Size: 0x1034 | 0x801766D8 | size: 0xEC
 */
bool GameInfoManager::IsSuperStadiumUnlocked() const
{
    return CheckUnlockStatus(isSuperStadUnlocked, mUserInfo.mTrophies[0], 3);
}

/**
 * Offset/Address/Size: 0xF48 | 0x801765EC | size: 0xEC
 */
bool GameInfoManager::IsSuperTeamUnlocked() const
{
    return CheckUnlockStatus(isSuperTeamUnlocked, mUserInfo.mTrophies[0], 3);
}

/**
 * Offset/Address/Size: 0xF24 | 0x801765C8 | size: 0x24
 */
bool GameInfoManager::IsSuperCupModeUnlocked() const
{
    FORCE_DONT_INLINE;
    return IsUserQualified(GM_NUM_MODES);
}

/**
 * Offset/Address/Size: 0xF1C | 0x801765C0 | size: 0x8
 */
bool GameInfoManager::IsLegendSkillUnlocked() const
{
    return true;
}

/**
 * Offset/Address/Size: 0xE30 | 0x801764D4 | size: 0xEC
 */
bool GameInfoManager::IsAllSTSCheatUnlocked() const
{
    return CheckUnlockStatus(isAllSTSUnlocked, mUserInfo.mTrophies[0], 4);
}

/**
 * Offset/Address/Size: 0xD44 | 0x801763E8 | size: 0xEC
 */
bool GameInfoManager::IsTiltCheatUnlocked() const
{
    return CheckUnlockStatus(isTiltUnlocked, mUserInfo.mTrophies[0], 5);
}

/**
 * Offset/Address/Size: 0xC58 | 0x801762FC | size: 0xEC
 */
bool GameInfoManager::IsGlassJawGoalieUnlocked() const
{
    return CheckUnlockStatus(isGoalieUnlocked, mUserInfo.mTrophies[0], 6);
}

/**
 * Offset/Address/Size: 0xB6C | 0x80176210 | size: 0xEC
 */
bool GameInfoManager::IsUnlimtedPowerupsUnlocked() const
{
    return CheckUnlockStatus(isUnlimitedUnlocked, mUserInfo.mTrophies[0], 7);
}

/**
 * Offset/Address/Size: 0xA80 | 0x80176124 | size: 0xEC
 */
bool GameInfoManager::IsCustomShellsUnlocked() const
{
    return CheckUnlockStatus(isShellsUnlocked, mUserInfo.mTrophies[1], 0);
}

/**
 * Offset/Address/Size: 0x994 | 0x80176038 | size: 0xEC
 */
bool GameInfoManager::IsCustomEnhanceUnlocked() const
{
    return CheckUnlockStatus(isEnhanceUnlocked, mUserInfo.mTrophies[1], 1);
}

/**
 * Offset/Address/Size: 0x8A8 | 0x80175F4C | size: 0xEC
 */
bool GameInfoManager::IsCustomGiantUnlocked() const
{
    return CheckUnlockStatus(isGiantUnlocked, mUserInfo.mTrophies[1], 4);
}

/**
 * Offset/Address/Size: 0x7BC | 0x80175E60 | size: 0xEC
 */
bool GameInfoManager::IsCustomExplosiveUnlocked() const
{
    return CheckUnlockStatus(isExplosiveUnlocked, mUserInfo.mTrophies[1], 2);
}

/**
 * Offset/Address/Size: 0x6D0 | 0x80175D74 | size: 0xEC
 */
bool GameInfoManager::IsCustomFreezingUnlocked() const
{
    return CheckUnlockStatus(isFreezingUnlocked, mUserInfo.mTrophies[1], 3);
}

/**
 * Offset/Address/Size: 0x41C | 0x80175AC0 | size: 0x2B4
 */
bool GameInfoManager::HasHumanGameBeenPlayed() const
{
    volatile const eGameModes* modePtr = &mCurrentMode;

    if (*modePtr != GM_TOURNAMENT)
    {
        return true;
    }

    eGameModes mode;
    BaseCup* currentCup;
    int currentRound;
    int currentGame;
    int round;
    int game;

    currentRound = mCurrentCup->mRoundNumber;
    currentGame = mCurrentCup->mGameNumber;

    round = GetFirstRoundNumber();

    currentCup = mCurrentCup;
    mode = mCurrentMode;
    game = 0;

    while (round != -5)
    {
        if ((round == currentRound) && (game == currentGame))
        {
            return false;
        }

        BaseCup* cup = currentCup;
        s16 lookupRound = round;

        if ((mode == GM_BOWSER_CUP) || (mode == GM_SUPER_BOWSER_CUP))
        {
            if (lookupRound == -3)
            {
                lookupRound = 0;
            }
            else if ((lookupRound == -2) || (lookupRound == -5) || (lookupRound == -1))
            {
                lookupRound = 1;
            }
            else if (mDoingKnockout)
            {
                cup = mPreviousCup;
            }
        }
        else
        {
            if (lookupRound == -4)
            {
                lookupRound = currentCup->GetNumRounds() - 3;
            }
            else if (lookupRound == -3)
            {
                lookupRound = currentCup->GetNumRounds() - 2;
            }
            else if (lookupRound == -2)
            {
                lookupRound = currentCup->GetNumRounds() - 1;
            }
        }

        BasicGameInfo* gameInfo = cup->GetGameInfo(lookupRound, (u16)game);
        u16 humanTeams = currentCup->mHumanTeams;

        if ((humanTeams & (1 << gameInfo->mTeamIndex[0])) || (humanTeams & (1 << gameInfo->mTeamIndex[1])))
        {
            return true;
        }

        game++;

        if (game == GetNumGamesPerRound(currentCup, round))
        {
            game = 0;
            round = GetNextRoundNumber(round);
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x2FC | 0x801759A0 | size: 0x120
 */
void GameInfoManager::SetRoundResult(bool inOvertime, int winningSide)
{
    BaseCup* cup = mCurrentCup;
    eGameModes mode = mCurrentMode;
    int roundNum = cup->mRoundNumber;
    BasicGameInfo* gameInfo = mGameInfo[mode];

    if (gameInfo == NULL)
    {
        winningSide = -1;
    }
    else
    {
        winningSide = (int)gameInfo->mTeamIndex[(s16)winningSide];
    }

    eTeamID userTeam = cup->mUserSelectedTeam;
    bool userWon = ((eTeamID)winningSide == userTeam);

    if (mode == GM_BOWSER_CUP || mode == GM_SUPER_BOWSER_CUP)
    {
        if (mDoingKnockout)
        {
            if (roundNum == -3)
            {
                roundNum = 0;
            }
            else if (roundNum == -2 || roundNum == -1)
            {
                roundNum = 1;
            }
        }
    }

    if (userWon)
    {
        *cup->GetRoundResults(roundNum) = 0;
    }
    else if (inOvertime)
    {
        *cup->GetRoundResults(roundNum) = 2;
    }
    else
    {
        *cup->GetRoundResults(roundNum) = 1;
    }
}

/**
 * Offset/Address/Size: 0x29C | 0x80175940 | size: 0x60
 */
bool GameInfoManager::IsStunnedGoaliesOn() const
{
    if (mIsInStrikers101Mode)
    {
        return false;
    }

    bool useCheatSettings;
    eGameModes currentMode = mCurrentMode;
    if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
    {
        useCheatSettings = false;
        if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
        {
            useCheatSettings = true;
        }
        if (useCheatSettings)
        {
            return mUserInfo.mCheatOptions.mStunnedGoalies;
        }
        return false;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x23C | 0x801758E0 | size: 0x60
 */
bool GameInfoManager::IsInfinitePowerupsOn() const
{
    if (mIsInStrikers101Mode)
    {
        return false;
    }

    bool useCheatSettings;
    eGameModes currentMode = mCurrentMode;
    if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
    {
        useCheatSettings = false;
        if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
        {
            useCheatSettings = true;
        }
        if (useCheatSettings)
        {
            return mUserInfo.mCheatOptions.mInfinitePowerups;
        }
        return false;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x1DC | 0x80175880 | size: 0x60
 */
bool GameInfoManager::IsTiltingFieldOn() const
{
    if (mIsInStrikers101Mode)
    {
        return false;
    }

    bool useCheatSettings;
    eGameModes currentMode = mCurrentMode;
    if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
    {
        useCheatSettings = false;
        if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
        {
            useCheatSettings = true;
        }
        if (useCheatSettings)
        {
            return mUserInfo.mCheatOptions.mCheatTBD1Enabled;
        }
        return false;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x17C | 0x80175820 | size: 0x60
 */
bool GameInfoManager::IsPerfectStrikesOn() const
{
    if (mIsInStrikers101Mode)
    {
        return false;
    }

    bool useCheatSettings;
    eGameModes currentMode = mCurrentMode;
    if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
    {
        useCheatSettings = false;
        if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
        {
            useCheatSettings = true;
        }
        if (useCheatSettings)
        {
            return mUserInfo.mCheatOptions.mCheatTBD2Enabled;
        }
        return false;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x14C | 0x801757F0 | size: 0x30
 */
bool GameInfoManager::IsBowserAttackEnabled() const
{
    if (g_e3_Build)
    {
        return false;
    }

    if (mIsInStrikers101Mode)
    {
        return false;
    }

    return mCurGameGameplayOptions.BowserAttackEnabled;
}

/**
 * Offset/Address/Size: 0xF8 | 0x8017579C | size: 0x54
 */
GameplaySettings::eSkillLevel GameInfoManager::GetSkillLevel()
{
    if (mIsInStrikers101Mode)
        return GameplaySettings::TRAINING;

    const GameplaySettings::eSkillLevel* p;

    if (mUseCurGameSettings)
    {
        p = &mCurGameGameplayOptions.SkillLevel;
    }
    else
    {
        if ((mCurrentMode == GM_FRIENDLY) || (mCurrentMode == GM_DEMO))
        {
            p = &mUserInfo.mGameplayOptions.SkillLevel;
        }
        else
        {
            p = &mCurrentCup->mCupSettings.SkillLevel;
        }
    }

    return *p;
}

/**
 * Offset/Address/Size: 0x60 | 0x80175704 | size: 0x98
 */
eDifficultyID GameInfoManager::GetSkillLevelAsDifficultyID()
{
    eDifficultyID skillToDifficulty[5] = {
        DIFF_BRAINDEAD,
        DIFF_EASY,
        DIFF_MEDIUM,
        DIFF_HARD,
        DIFF_VERYHARD
    };

    GameplaySettings::eSkillLevel level;
    if (mIsInStrikers101Mode)
    {
        level = GameplaySettings::TRAINING;
    }
    else
    {
        GameplaySettings* pSettings;
        if (mUseCurGameSettings)
        {
            pSettings = &mCurGameGameplayOptions;
        }
        else if (mCurrentMode == GM_FRIENDLY || mCurrentMode == GM_DEMO)
        {
            pSettings = &mUserInfo.mGameplayOptions;
        }
        else
        {
            pSettings = &mCurrentCup->mCupSettings;
        }
        level = pSettings->SkillLevel;
    }

    return skillToDifficulty[level];
}

/**
 * Offset/Address/Size: 0x0 | 0x801756A4 | size: 0x60
 */
CustomPowerups GameInfoManager::GetCustomPowerups() const
{
    if (mIsInStrikers101Mode)
    {
        return CheatSettings::CP_OFF;
    }

    bool useCheatSettings;
    eGameModes currentMode = mCurrentMode;
    if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
    {
        useCheatSettings = false;
        if ((currentMode == GM_FRIENDLY) || (currentMode == GM_TOURNAMENT))
        {
            useCheatSettings = true;
        }
        if (useCheatSettings)
        {
            return mUserInfo.mCheatOptions.mCustomPowerups;
        }
        return CheatSettings::CP_OFF;
    }
    return CheatSettings::CP_OFF;
}
