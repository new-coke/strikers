#include "port/region.h"  // PORT: one binary, three discs
#include "Game/FE/Cup/CupTickerManager.h"

#include "Game/DB/StatsTracker.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feScrollText.h"
#include "Game/GameInfo.h"

#include "NL/gl/glStruct.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlFormat.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlLocalization.h"
#include "NL/nlSingleton.h"
#include "NL/nlString.h"

#include "NL/nlBind.h"

typedef Detail::MemFunImpl<void, void (CupTickerManager::*)()> MemFunImpl_CupTickerManager_v;
typedef BindExp1<void, MemFunImpl_CupTickerManager_v, CupTickerManager*> BindExp1_vfmfcp;
typedef Function0<void>::FunctorImpl<BindExp1_vfmfcp> FunctorImpl_vfmfcp;

struct GameInfoAccessor_CupTicker
{
    char _pad6C[0x6C];
    unsigned char mDoingKnockout;
    char _pad4948[0x48DB];
    int mTournamentMode;
    char _pad4954[8];
    int mCurrentMode;
    char _pad4960[8];
    void* mCurrentCup;
};

struct BaseCupAccessor_CupTicker
{
    char _padA[0xA];
    short mGameNumber;
};

/**
 * Offset/Address/Size: 0x1D90 | 0x800F3D58 | size: 0x10
 */
CupTickerManager::CupTickerManager()
{
    mTicker = 0;
    mState = CUP_TICKER_STATE_0;
}

/**
 * Offset/Address/Size: 0x1C90 | 0x800F3C58 | size: 0x100
 */
CupTickerManager::~CupTickerManager()
{
    if (mTicker != 0)
    {
        delete mTicker;
    }
}

/**
 * Offset/Address/Size: 0x1968 | 0x800F3930 | size: 0x328
 */
void CupTickerManager::SetTickerTextInstance(TLTextInstance* tickerText)
{
    if (mTicker)
    {
        mTicker->ApplyNewTextInstancePointer(tickerText, 8000.0f, 100.0f);
    }
    else
    {
        gl_ScreenInfo* screenInfo = glGetScreenInfo();
        mTicker = new ((FEScrollText*)nlMalloc(sizeof(FEScrollText), 0x20, true))
            FEScrollText(tickerText, 0, screenInfo->ScreenWidth + 0x32);

        {
            Function<FnVoidVoid> callback(Bind<void, MemFunImpl_CupTickerManager_v, CupTickerManager*>(
                MemFun<CupTickerManager, void>(&CupTickerManager::CreateNewMessage), this));
            mTicker->m_messageFinishedCB = callback;
        }

        this->CreateNewMessage();
    }

    mTicker->SetDisplayMessage(BasicString<unsigned short, Detail::TempStringAllocator>(mMessageBuffer));
}

#define LOC_LOOKUP(_hashExpr, _locVar)                                                                                     \
    {                                                                                                                      \
        unsigned long _hash = (_hashExpr);                                                                                 \
        nlLocalization* _loc = g_pLocalization;                                                                            \
        if (_loc->m_LookupTable == 0)                                                                                      \
        {                                                                                                                  \
            (_locVar) = LocalizationTableNotFound;                                                                         \
        }                                                                                                                  \
        else                                                                                                               \
        {                                                                                                                  \
            nlLocalization::StringLookup* _entry = nlBSearch(_hash, _loc->m_LookupTable, (int)_loc->m_pFile->StringCount); \
            if (_entry != 0)                                                                                               \
            {                                                                                                              \
                (_locVar) = _loc->m_FirstString + _entry->StringOffset;                                                    \
            }                                                                                                              \
            else                                                                                                           \
            {                                                                                                              \
                (_locVar) = MissingLocString;                                                                              \
            }                                                                                                              \
        }                                                                                                                  \
    }

static inline const unsigned short* CupTickerLookupLocString(unsigned long hash)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* entry = nlBSearch(hash, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry != 0)
    {
        return loc->m_FirstString + entry->StringOffset;
    }
    return MissingLocString;
}

static inline const unsigned short* CupTickerLookupLocStringFrom(
    unsigned long hash, nlLocalization* loc)
{
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* entry = nlBSearch(hash, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry != 0)
    {
        return loc->m_FirstString + entry->StringOffset;
    }
    return MissingLocString;
}

static inline const unsigned short* CupTickerLookupLocName(const char* name)
{
    return CupTickerLookupLocStringFrom(
        nlStringLowerHash(name), g_pLocalization);
}

// TODO: Verify the retail provenance of this TU's inline_max_total_size(5120) build setting.
/**
 * Offset/Address/Size: 0x680 | 0x800F2648 | size: 0x12E8
 */
void CupTickerManager::CreateNewMessage()
{
    WideBasicString tickerMessage;
    bool messageDisplayed = false;
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::Instance();
    // PORT: the view is gone; the members are named.
    bool tournamentLeague = gameInfo->IsInTournamentMode() && gameInfo->mCustomTournamentInfo.m_tournMode == 0;
    const unsigned short* locString;

    if (mTicker == 0)
    {
        return;
    }

    while (!messageDisplayed)
    {
        if (gameInfo->GetCurrentRoundNumber() == -5)
        {
            if (mState != 5)
            {
                mState = (eCupTickerState)5;

                unsigned long modeHash = GetLOCModeName((GameInfoManager::eGameModes)gameInfo->mCurrentMode);
                locString = CupTickerLookupLocString(modeHash);
                WideBasicString modeName(locString);

                unsigned long charHash = GetLOCCharacterName(
                    gameInfo->FindWinningTeam(), false, false);
                locString = CupTickerLookupLocString(charHash);
                WideBasicString characterName(locString);

                tickerMessage = Format<WideBasicString, WideBasicString, WideBasicString>(
                    WideBasicString(CupTickerLookupLocString(0x273CF730UL)), modeName, characterName);
                break;
            }
            else if (mState != 2)
            {
                mState = (eCupTickerState)2;
                if (gameInfo->mCustomTournamentInfo.m_tournMode != 0)
                {
                    continue;
                }
                BuildGoalTotalTickerMessage(tickerMessage, false);
                break;
            }
        }

        if (mState == 0)
        {
            if (gameInfo->mDoingKnockout)
            {
                locString = CupTickerLookupLocString(0x474DA1D4UL);
                tickerMessage = WideBasicString(locString);
            }
            else
            {
                locString = CupTickerLookupLocString(0x67493499UL);
                tickerMessage = WideBasicString(locString);
            }
            messageDisplayed = true;
        }

        if (mState == 1)
        {
            if (!tournamentLeague)
            {
                mState = (eCupTickerState)3;
            }
            else
            {
                locString = CupTickerLookupLocString(0xA81870D8UL);
                tickerMessage = WideBasicString(locString);
                messageDisplayed = true;
            }
        }

        if (mState == 2)
        {
            if (!tournamentLeague
                || (gameInfo->GetCurrentRoundNumber() == 0
                    && gameInfo->mCurrentCup->mGameNumber == 0))
            {
                mState = (eCupTickerState)3;
            }
            else
            {
                BuildGoalTotalTickerMessage(tickerMessage, false);
                messageDisplayed = true;
            }
        }

        if (mState == 3)
        {
            if (gameInfo->GetCurrentRoundNumber() == -5)
            {
                mState = (eCupTickerState)4;
            }
            else
            {
                {
                    unsigned long team0NameHash = GetLOCTeamName(gameInfo->GetTeam(0));
                    unsigned long team1NameHash = GetLOCTeamName(gameInfo->GetTeam(1));

                    tickerMessage = Format<WideBasicString, const unsigned short*, const unsigned short*>(
                        WideBasicString(CupTickerLookupLocName("CUPHUB_TICKER_NEXT_MATCH")),
                        CupTickerLookupLocString(team0NameHash),
                        CupTickerLookupLocString(team1NameHash));
                }
                messageDisplayed = true;
            }
        }

        if (mState == 4)
        {
            GameInfoManager::eGameModes mode = gameInfo->mCurrentMode;
            if ((mode == GameInfoManager::GM_MUSHROOM_CUP
                    && !gameInfo->IsUserQualified(GameInfoManager::GM_FLOWER_CUP))
                || (mode == GameInfoManager::GM_FLOWER_CUP
                    && !gameInfo->IsUserQualified(GameInfoManager::GM_STAR_CUP)))
            {
                locString = CupTickerLookupLocString(0x751FA62FUL);
                tickerMessage = WideBasicString(locString);
                messageDisplayed = true;
            }
            else if (gameInfo->IsUserQualified(GameInfoManager::GM_FLOWER_CUP)
                     && gameInfo->IsUserQualified(GameInfoManager::GM_STAR_CUP)
                     && !gameInfo->IsUserQualified(GameInfoManager::GM_BOWSER_CUP)
                     && gameInfo->IsInCupMode())
            {
                locString = CupTickerLookupLocString(
                    port_region() == PORT_REGION_JAPAN ? 0x906BC349UL
                                                       : 0xEEC22902UL);
                tickerMessage = WideBasicString(locString);
                messageDisplayed = true;
            }
            else if ((mode == GameInfoManager::GM_BOWSER_CUP || mode == GameInfoManager::GM_SUPER_BOWSER_CUP)
                     && !gameInfo->mDoingKnockout)
            {
                locString = CupTickerLookupLocString(0x4B50DF6AUL);
                tickerMessage = WideBasicString(locString);
                messageDisplayed = true;
            }
            else
            {
                mState = (eCupTickerState)5;
            }
        }

        if (mState == 5)
        {
            if (gameInfo->GetCurrentRoundNumber() == gameInfo->GetFirstRoundNumber())
            {
                mState = CUP_TICKER_STATE_0;
            }
            else
            {
                int roundNumber = gameInfo->GetPreviousRoundNumber(-7);
                int numGames = (int)gameInfo->GetNumGamesPerRound(roundNumber);
                for (int gameNumber = 0; gameNumber < numGames; gameNumber++)
                {
                    BasicGameInfo* game;
                    if (gameInfo->GetCurrentRoundNumber() == -1)
                    {
                        game = &gameInfo->mUserInfo.mBowserCupFinalRound;
                    }
                    else
                    {
                        game = gameInfo->GetMatchupInfo(roundNumber, (unsigned short)gameNumber);
                    }

                    unsigned long team0NameHash = GetLOCTeamName(game->mTeamIndex[0]);
                    unsigned long team1NameHash = GetLOCTeamName(game->mTeamIndex[1]);

                    unsigned long formatHash;
                    if (gameNumber == 0)
                    {
                        formatHash = 0x97372F0FUL;
                    }
                    else
                    {
                        formatHash = 0x3E3B44CAUL;
                    }

                    NLString score0String = LexicalCast<NLString, int>((int)game->mFinalScore[0]);
                    NLString score1String = LexicalCast<NLString, int>((int)game->mFinalScore[1]);

                    unsigned short wideScore0[16];
                    nlStrToWcs(score0String.c_str(), wideScore0, 16);

                    unsigned short wideScore1[16];
                    nlStrToWcs(score1String.c_str(), wideScore1, 16);

                    if (game->mFinalScore[0] > game->mFinalScore[1])
                    {
                        tickerMessage = tickerMessage.Append(
                            Format<WideBasicString, const unsigned short*, const unsigned short*, unsigned short[16], unsigned short[16]>(
                                WideBasicString(CupTickerLookupLocString(formatHash)),
                                CupTickerLookupLocString(team0NameHash),
                                CupTickerLookupLocString(team1NameHash),
                                wideScore0,
                                wideScore1));
                    }
                    else
                    {
                        tickerMessage = tickerMessage.Append(
                            Format<WideBasicString, const unsigned short*, const unsigned short*, unsigned short[16], unsigned short[16]>(
                                WideBasicString(CupTickerLookupLocString(formatHash)),
                                CupTickerLookupLocString(team1NameHash),
                                CupTickerLookupLocString(team0NameHash),
                                wideScore1,
                                wideScore0));
                    }
                }
                messageDisplayed = true;
            }
        }

        mState = (eCupTickerState)(((int)mState + 1) % 6);
    }

    memcpy(mMessageBuffer, tickerMessage.c_str(), 0x400);
    mTicker->SetDisplayMessage(WideBasicString(mMessageBuffer));
}

/**
 * Offset/Address/Size: 0x654 | 0x800F261C | size: 0x2C
 */
void CupTickerManager::Update(float dt)
{
    if (mTicker != NULL)
    {
        mTicker->Update(dt);
    }
}

static inline int CupTickerGetTeamCount(GameInfoManager* gameInfo, bool bIsHuman)
{
    unsigned int teamCount;
    if (bIsHuman)
    {
        teamCount = gameInfo->GetNumHumanTeams();
    }
    else
    {
        teamCount = gameInfo->GetNumPlayingTeams();
    }
    return teamCount;
}

/**
 * Offset/Address/Size: 0x0 | 0x800F1FC8 | size: 0x654
 */
void CupTickerManager::BuildGoalTotalTickerMessage(
    BasicString<unsigned short, Detail::TempStringAllocator>& result, bool bIsHuman)
{
    int i;
    int j;
    int numValid = 0;
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;

    int numTeams = CupTickerGetTeamCount(gameInfo, bIsHuman);

    PlayerStats playerStats[8];

    for (i = 0; i < (int)gameInfo->GetNumPlayingTeams(); i++)
    {
        TeamStats teamStats = gameInfo->GetTeamStatsByIndex((unsigned short)i);

        if ((unsigned char)bIsHuman)
        {
            unsigned short humanTeams = gameInfo->mCurrentCup->mHumanTeams;
            if (humanTeams & (1 << (int)teamStats.mTeamIndex))
            {
                playerStats[numValid++] = teamStats.mPlayerTotalStats;
            }
        }
        else
        {
            playerStats[numValid++] = teamStats.mPlayerTotalStats;
        }
    }

    int sortedIndices[8];
    nlSingleton<StatsTracker>::Instance()->GetSortedStats(
        playerStats, numTeams, sortedIndices, numTeams, (ePlayerStats)1, (eSortOrder)1);

    for (j = 0; j < numTeams; j++)
    {
        unsigned long teamNameHash = GetLOCTeamName(playerStats[sortedIndices[j]].mRecordType.mTeamID);

        unsigned long formatHash;
        if (j == 0)
        {
            formatHash = 0xD517194DUL;
        }
        else
        {
            formatHash = 0x1DB17A6DUL;
        }

        NLString goalsString = LexicalCast<NLString, int>((int)playerStats[sortedIndices[j]].mNumGoalsFor);

        unsigned short wideGoals[16];
        nlStrToWcs(goalsString.c_str(), wideGoals, 16);

        result = result.Append(Format<WideBasicString, const unsigned short*, unsigned short[16]>(
            WideBasicString(CupTickerLookupLocString(formatHash)), CupTickerLookupLocString(teamNameHash), wideGoals));
    }
}
