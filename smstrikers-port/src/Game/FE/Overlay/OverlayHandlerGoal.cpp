#include "Game/OverlayHandlerGoal.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/Goalie.h"
#include "Game/Team.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlBundleFile.h"
#include "NL/nlFormat.h"
#include "NL/nlLocalization.h"

extern cTeam* g_pTeams[];

static inline const unsigned short* LookupLocHash(unsigned long key)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* entry = nlBSearch<nlLocalization::StringLookup, unsigned long>(key, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry)
    {
        return loc->m_FirstString + entry->StringOffset;
    }

    return MissingLocString;
}
extern "C" double ceil(double);
extern "C" double floor(double);

/**
 * Offset/Address/Size: 0x3150 | 0x801031C0 | size: 0x100
 */
GoalOverlay::GoalOverlay()
    : BaseOverlayHandler(0x110, POSITION_BOTTOM)
{
    mEventHandler = NULL;
    mIsCreated = false;
    mIsInOvertime = false;

    mEventHandler = g_pEventManager->AddEventHandler(eventHandler, this, 1);

    if (GameInfoManager::Instance()->IsInFriendlyMode() || GameInfoManager::Instance()->IsInTournamentMode())
    {
        mHasSniperCup = true;
    }
    else
    {
        mHasSniperCup = GameInfoManager::Instance()->HasTrophy(TROPHY_SNIPER_CUP);
    }

    mCaptainGoals[0] = 0;
    mCaptainGoals[1] = 0;
    mSidekickGoals[0] = 0;
    mSidekickGoals[1] = 0;
}

/**
 * Offset/Address/Size: 0x30B8 | 0x80103128 | size: 0x98
 */
GoalOverlay::~GoalOverlay()
{
    if (mEventHandler != nullptr)
    {
        g_pEventManager->RemoveEventHandler(mEventHandler);
        mEventHandler = nullptr;
    }
}

/**
 * Offset/Address/Size: 0x30AC | 0x8010311C | size: 0xC
 */
void GoalOverlay::SceneCreated()
{
    mIsCreated = true;
}

/**
 * Offset/Address/Size: 0x305C | 0x801030CC | size: 0x50
 */
void GoalOverlay::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);

    if (!mIsInOvertime)
    {
        if (g_pGame->m_eGameState == GS_OVERTIME)
        {
            mIsInOvertime = true;
        }
    }
}

void GoalOverlay::CreateEventHandler()
{
    mEventHandler = g_pEventManager->AddEventHandler(eventHandler, this, 1);
}

void GoalOverlay::DestroyEventHandler()
{
    if (mEventHandler)
    {
        g_pEventManager->RemoveEventHandler(mEventHandler);
        mEventHandler = NULL;
    }
}

/**
 * Offset/Address/Size: 0x2F44 | 0x80102FB4 | size: 0x118
 */
void GoalOverlay::eventHandler(Event* event, void* param)
{
    GoalOverlay* pGoal = (GoalOverlay*)param;

    if (event->m_uEventID == 5)
    {
        GoalScoredData* data;
        s32 id = port_event_data_id(&event->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            data = 0;
        }
        else
        {
            id = port_event_data_id(&event->m_data);
            if (id != 0x18A)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                data = 0;
            }
            else
            {
                data = (GoalScoredData*)&event->m_data;
            }
        }

        bool isCaptainS2S = (data->uGoalType == 6);
        int playerIndex;

        if (data->uGoalType == 5)
        {
            playerIndex = data->pLastTouch[data->uTeamIndex]->m_ID;
        }
        else
        {
            playerIndex = data->pScorer->m_ID;
        }

        pGoal->UpdateGoalInfo((int)data->uTeamIndex, playerIndex, isCaptainS2S, (int)data->uNumGoalsScored);
    }
    else if (event->m_uEventID == 3)
    {
        pGoal->mCaptainGoals[0] = 0;
        pGoal->mCaptainGoals[1] = 0;
        pGoal->mSidekickGoals[0] = 0;
        pGoal->mSidekickGoals[1] = 0;
        pGoal->mIsInOvertime = false;
    }
}

void GoalOverlay::Reset()
{
    mCaptainGoals[0] = 0;
    mCaptainGoals[1] = 0;
    mSidekickGoals[0] = 0;
    mSidekickGoals[1] = 0;
    mIsInOvertime = false;
}

/**
 * Offset/Address/Size: 0x19A8 | 0x80101A18 | size: 0x159C
 */
void GoalOverlay::UpdateGoalInfo(int homeAway, int playerIndex, bool isCaptainS2S, int numGoals)
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Name")));

    GameInfoManager* const gameInfo = GameInfoManager::Instance();
    eTeamID team = (eTeamID)gameInfo->GetTeam((short)homeAway);
    GameInfoManager::Instance()->GetTeam(0);
    GameInfoManager::Instance()->GetTeam(1);

    float time = g_pGame->GetGameTime();
    StatsTracker* stats = StatsTracker::Instance();
    if (!stats->mIsOvertime)
    {
        float fGameDuration = g_pGame->m_pGameTweaks->fGameDuration;
        if (time > fGameDuration)
        {
            time = fGameDuration;
        }
    }

    float remainingTime = g_pGame->m_pGameTweaks->fGameDuration - time;
    unsigned long minutes = (unsigned long)(time / 60.0f);

    float fSeconds = time - (float)(minutes * 60);
    int wholeSeconds;
    if (remainingTime >= 30.0)
    {
        float roundedSeconds = (float)ceil((double)fSeconds);
        wholeSeconds = (int)roundedSeconds;
    }
    else
    {
        float roundedSeconds = (float)floor((double)fSeconds);
        wholeSeconds = (int)roundedSeconds;
    }
    unsigned long seconds = (unsigned long)wholeSeconds;

    if (seconds == 60)
    {
        seconds = 0;
        minutes = minutes + 1;
    }

    BasicString<char, Detail::TempStringAllocator> minutesString(
        LexicalCast<BasicString<char, Detail::TempStringAllocator>, unsigned long>(minutes));

    BasicString<char, Detail::TempStringAllocator> secondsString;
    BasicString<unsigned short, Detail::TempStringAllocator> formatted;
    BasicString<unsigned short, Detail::TempStringAllocator> unformatted;

    int oldScore[2] = {
        mCaptainGoals[0] + mSidekickGoals[0],
        mCaptainGoals[1] + mSidekickGoals[1]
    };

    bool isSuperTeam = (GameInfoManager::Instance()->GetTeam((short)homeAway) == TEAM_MYSTERY);
    if (isSuperTeam)
    {
        playerIndex = 0;
    }

    unsigned long teamNameID = GetLOCTeamName((eTeamID)team);
    pText->m_LocStrId = teamNameID;
    pText->m_OverloadFlags |= 8;

    if (seconds < 10)
    {
        secondsString = BasicString<char, Detail::TempStringAllocator>("0");
        secondsString = secondsString.Append(
            LexicalCast<BasicString<char, Detail::TempStringAllocator>, unsigned long>(seconds));
    }
    else
    {
        secondsString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, unsigned long>(seconds);
    }

    unsigned short minutesWideString[16];
    unsigned short secondsWideString[16];
    // PORT: maxLen counts elements, not bytes.
    nlStrToWcs(minutesString.c_str(), minutesWideString, 16);
    nlStrToWcs(secondsString.c_str(), secondsWideString, 16);

    unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x04E76F8B));
    formatted = Format(unformatted, minutesWideString, secondsWideString);

    memcpy(mClockBuffer, formatted.c_str(), sizeof(mClockBuffer));

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Time")));

    pText->SetString(mClockBuffer);
    pText->m_bVisible = true;

    if (playerIndex > 0)
    {
        mSidekickGoals[homeAway] += numGoals;
    }
    else
    {
        mCaptainGoals[homeAway] += numGoals;
    }

    int scoreLeft = mCaptainGoals[0] + mSidekickGoals[0],
        scoreRight = mCaptainGoals[1] + mSidekickGoals[1];

    if (mIsInOvertime)
    {
        formatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0xAD90B5E0));
    }
    else
    {
        if (!mHasSniperCup && gameInfo->HasTrophy(TROPHY_SNIPER_CUP) == 1)
        {
            formatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x25801546));
            mHasSniperCup = true;
        }
        else if (isCaptainS2S == 1)
        {
            formatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x831AAC58));
        }
        else if (oldScore[0] == 0 && oldScore[1] == 0)
        {
            formatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x80675849));
        }
        else if (scoreLeft == scoreRight)
        {
            formatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x43AB49F3));
        }
        else if ((oldScore[0] >= oldScore[1] && scoreLeft < scoreRight) || (oldScore[1] >= oldScore[0] && scoreRight < scoreLeft))
        {
            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x78446837));

            if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese && isSuperTeam)
            {
                BasicString<unsigned short, Detail::TempStringAllocator> captain(
                    LookupLocHash(GetLOCCharacterName(TEAM_MYSTERY, true, false)));
                formatted = Format(unformatted, captain);
            }
            else
            {
                unsigned long captainKey = GetLOCTeamName(team);
                const unsigned short* captainString = LookupLocHash(captainKey);
                BasicString<unsigned short, Detail::TempStringAllocator> captain(captainString);
                formatted = Format(unformatted, captain);
            }
        }
        else if (playerIndex == 0 && mCaptainGoals[homeAway] == 3 && !isSuperTeam)
        {
            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0xD8976F68));

            BasicString<unsigned short, Detail::TempStringAllocator> captain(
                LookupLocHash(GetLOCTeamName((eTeamID)team)));
            formatted = Format(unformatted, captain);
        }
        else if (playerIndex == 0)
        {
            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x3DE2ABC1));

            BasicString<unsigned short, Detail::TempStringAllocator> captain(
                LookupLocHash(GetLOCTeamName((eTeamID)team)));

            BasicString<char, Detail::TempStringAllocator> numGoalsString(
                LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(mCaptainGoals[homeAway]));
            unsigned short goalsWideString[32];
            nlStrToWcs(numGoalsString.c_str(), goalsWideString, 32);

            formatted = Format(unformatted, goalsWideString, captain);
        }
        else
        {
            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x091F7BA8));

            BasicString<unsigned short, Detail::TempStringAllocator> captain(
                LookupLocHash(GetLOCTeamName((eTeamID)team)));

            BasicString<char, Detail::TempStringAllocator> numGoalsString(
                LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(mSidekickGoals[homeAway]));
            unsigned short goalsWideString[32];
            nlStrToWcs(numGoalsString.c_str(), goalsWideString, 32);

            formatted = Format(unformatted, goalsWideString, captain);
        }
    }

    memcpy(mDescriptionBuffer, formatted.c_str(), sizeof(mDescriptionBuffer));

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Description")));

    MakeTextBoxReallyWide(*pText);
    pText->SetString(mDescriptionBuffer);
}

/**
 * Offset/Address/Size: 0x1590 | 0x80101600 | size: 0x418
 */
void GoalOverlay::SetHighlightNumber(int highlight)
{
    SetWinnerTitle();

    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Description")));

    MakeTextBoxReallyWide(*pText);

    if (highlight == 0)
    {
        pText->SetStringId("HIGHLIGHTS1");
        return;
    }

    BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupLocHash(0xF3DDE99C));

    BasicString<char, Detail::TempStringAllocator> highlightString(
        LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(highlight + 1));
    unsigned short highlightWideString[16];
    nlStrToWcs(highlightString.c_str(), highlightWideString, 16);

    BasicString<unsigned short, Detail::TempStringAllocator> formatted(
        Format(unformatted, highlightWideString));

    memcpy(mDescriptionBuffer, formatted.c_str(), sizeof(mDescriptionBuffer));
    pText->SetString(mDescriptionBuffer);
}

/**
 * Offset/Address/Size: 0xDA4 | 0x80100E14 | size: 0x7EC
 */
void GoalOverlay::DoMatchEndOverlay()
{
    SetWinnerTitle();

    GameInfoManager* const gameInfo = GameInfoManager::Instance();
    BasicString<unsigned short, Detail::TempStringAllocator> formatted;
    eTeamID winner = TEAM_INVALID;
    unsigned char isFinalGame = false;
    int scoreLeft;
    int scoreRight;

    if (gameInfo->IsInCupMode())
    {
        int round = gameInfo->GetCurrentRoundNumber();
        GameInfoManager::eGameModes mode = gameInfo->mCurrentMode;

        if (mode == GameInfoManager::GM_BOWSER_CUP && ((round == -2 && gameInfo->IsSuperTeamUnlocked()) || round == -1))
        {
            isFinalGame = true;
            winner = gameInfo->FindWinningTeam();
        }
        else if (mode == GameInfoManager::GM_BOWSER_CUP && round == -2 && !gameInfo->IsSuperTeamUnlocked())
        {
            winner = gameInfo->FindWinningTeam();
            if (winner != gameInfo->GetUserSelectedCupTeam())
            {
                isFinalGame = true;
            }
        }
        else if (mode == GameInfoManager::GM_SUPER_BOWSER_CUP && round == -2)
        {
            isFinalGame = true;
            winner = gameInfo->FindWinningTeam();
        }
        else if (mode != GameInfoManager::GM_BOWSER_CUP && mode != GameInfoManager::GM_SUPER_BOWSER_CUP)
        {
            if (round == (u16)gameInfo->GetNumRounds() - 1)
            {
                isFinalGame = true;
                winner = gameInfo->FindWinningTeam();
            }
        }
    }

    if (isFinalGame && (winner == gameInfo->GetTeam(0) || winner == gameInfo->GetTeam(1)))
    {
        if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese && winner == TEAM_MYSTERY)
        {
            BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupLocHash(0x736E7F17));
            eTrophyType cup = GameInfoManager::Instance()->GetTrophyTypeByCurrentMode();

            formatted = Format(unformatted, LookupLocHash(GetLOCCharacterName(TEAM_MYSTERY, true, false)), LookupLocHash(GetLOCTrophyName(cup)));
        }
        else
        {
            BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupLocHash(0x736E7F17));
            eTrophyType cup = GameInfoManager::Instance()->GetTrophyTypeByCurrentMode();

            formatted = Format(unformatted, LookupLocHash(GetLOCTeamName(winner)), LookupLocHash(GetLOCTrophyName(cup)));
        }
    }
    else
    {
        scoreLeft = g_pTeams[0]->m_nScore;
        scoreRight = g_pTeams[1]->m_nScore;

        eTeamID winnerID = scoreLeft > scoreRight
                             ? GameInfoManager::Instance()->GetTeam(0)
                             : GameInfoManager::Instance()->GetTeam(1);
        eTeamID loserID = scoreLeft < scoreRight
                            ? GameInfoManager::Instance()->GetTeam(0)
                            : GameInfoManager::Instance()->GetTeam(1);

        BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupLocHash(0x09B4BC7C));

        bool useShortSuperTeam = true;
        if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
        {
            useShortSuperTeam = false;
        }

        formatted = Format(unformatted, LookupLocHash(GetLOCCharacterName(winnerID, useShortSuperTeam, false)), LookupLocHash(GetLOCCharacterName(loserID, useShortSuperTeam, false)));
    }

    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Description")));

    MakeTextBoxReallyWide(*pText);

    memcpy(mDescriptionBuffer, formatted.c_str(), sizeof(mDescriptionBuffer));
    mDescriptionBuffer[127] = 0;
    pText->SetString(mDescriptionBuffer);
}

/**
 * Offset/Address/Size: 0x6E8 | 0x80100758 | size: 0x6BC
 */
void GoalOverlay::SetWinnerTitle()
{
    int scoreLeft = g_pTeams[0]->m_nScore;
    int scoreRight = g_pTeams[1]->m_nScore;

    BasicString<char, Detail::TempStringAllocator> scoreLeftString(
        LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(scoreLeft));
    BasicString<char, Detail::TempStringAllocator> scoreRightString(
        LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(scoreRight));

    unsigned short scoreLeftWideString[32];
    unsigned short scoreRightWideString[32];

    nlStrToWcs(scoreLeftString.c_str(), scoreLeftWideString, 32);
    nlStrToWcs(scoreRightString.c_str(), scoreRightWideString, 32);

    BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupLocHash(0x4543196B));
    BasicString<unsigned short, Detail::TempStringAllocator> formatted;

    eTeamID winningTeam = GameInfoManager::Instance()->GetTeam((scoreLeft <= scoreRight) ? 1 : 0);

    if (scoreLeft > scoreRight)
    {
        formatted = Format(unformatted, LookupLocHash(GetLOCTeamName(winningTeam)), scoreLeftWideString, scoreRightWideString);
    }
    else
    {
        formatted = Format(unformatted, LookupLocHash(GetLOCTeamName(winningTeam)), scoreRightWideString, scoreLeftWideString);
    }

    memcpy(mScoresBuffer, formatted.c_str(), sizeof(mScoresBuffer));

    TLTextInstance* pText;

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Name")));

    pText->SetString(mScoresBuffer);

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Time")));

    pText->m_bVisible = false;
}

/**
 * Offset/Address/Size: 0x20 | 0x80100090 | size: 0x6C8
 */
void GoalOverlay::DoCupWinOverlay()
{
    eTeamID winners = (eTeamID)GameInfoManager::Instance()->GetUserSelectedCupTeam();

    BasicString<unsigned short, Detail::TempStringAllocator> formatted(
        Format(BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0xB49CF8B5)),
            LookupLocHash(GetLOCCharacterName(winners, true, false))));

    memcpy(mScoresBuffer, formatted.c_str(), sizeof(mScoresBuffer));

    TLTextInstance* pText;

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Name")));

    pText->SetString(mScoresBuffer);

    eTrophyType cup = (eTrophyType)GameInfoManager::Instance()->GetTrophyTypeByCurrentMode();

    formatted = Format(BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x4E704897)),
        LookupLocHash(GetLOCTrophyName(cup)));

    memcpy(mDescriptionBuffer, formatted.c_str(), sizeof(mDescriptionBuffer));

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Description")));

    MakeTextBoxReallyWide(*pText);
    pText->SetString(mDescriptionBuffer);

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Time")));

    pText->m_bVisible = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x80100070 | size: 0x20
 */
void GoalOverlay::Restart()
{
    if (mIsCreated)
    {
        m_pFEPresentation->m_fadeDuration = m_pFEPresentation->m_currentSlide->m_start;
    }
}
