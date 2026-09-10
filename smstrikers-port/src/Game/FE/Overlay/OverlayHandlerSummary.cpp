#include "Game/FE/Overlay/OverlayHandlerSummary.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feSlideMenu.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feManager.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/GameInfo.h"
#include "Game/OverlayManager.h"
#include "Game/SH/SHPause.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlFormat.h"
#include "NL/nlLocalization.h"

extern bool g_e3_Build;

static const char* SUMMARY_SLIDE_NAME = "SUMMARY";
static const char* TOTAL_SUMMARY_SLIDE_NAME = "TOTAL SUMMARY";
static const char* USER_SLIDE_NAME = "USER";
static const char* TOTAL_USER_SLIDE_NAME = "TOTAL USER";

static char* SUMMARY_ROW_NAMES[6] = {
    "LINE_0",
    "LINE_1",
    "LINE_2",
    "LINE_3",
    "LINE_4",
    "LINE_5",
};

static char* SUMMARY_COL_NAMES[4] = {
    "numeric_column_sm",
    "numeric_column_sm2",
    "numeric_column_sm3",
    "numeric_column_sm4",
};

/**
 * Offset/Address/Size: 0x1D88 | 0x800FE928 | size: 0xB08
 */
SummaryOverlay::SummaryOverlay(SummaryOverlay::eSummaryContext context)
    : BaseOverlayHandler(-1, POSITION_ALL)
    , mContext(context)
    , m_controllingInput(FE_ALL_PADS)
    , mSummaryDisplayed(false)
    , mCurrentSummaryIndex(0)
    , mButtonState(ButtonComponent::BS_INVALID)
{
    StatsTracker* tracker = StatsTracker::GetInstance();

    mShowUserStats = false;
    mShowCumulativeStats = false;

    {
        TeamStats currentTeam = tracker->mCurrentTeamStats[0];
        mCurrentPlayerStats[0] = currentTeam.mPlayerTotalStats;
    }
    {
        TeamStats currentTeam = tracker->mCurrentTeamStats[1];
        mCurrentPlayerStats[1] = currentTeam.mPlayerTotalStats;
    }

    if (mShowUserStats)
    {
        for (int i = 0; i < 4; i++)
        {
            PlayerStats currentUserStats = tracker->mCurrentUserStats[i];
            mCurrentUserStats[i] = currentUserStats;

            if (mShowCumulativeStats)
            {
                PlayerStats cumulativeUserStats = tracker->mCumulativeUserStats[i];
                mCumulativeUserStats[i] = cumulativeUserStats;
            }
        }
    }

    if (mShowCumulativeStats)
    {
        {
            TeamStats cumulativeTeamA = tracker->mCumulativeTeamStats[0];
            mCumulativePlayerStats[0] = cumulativeTeamA.mPlayerTotalStats;
        }
        {
            TeamStats cumulativeTeamB = tracker->mCumulativeTeamStats[1];
            mCumulativePlayerStats[1] = cumulativeTeamB.mPlayerTotalStats;
        }
    }

    {
        TeamStats cumulativeTeamA = tracker->mCumulativeTeamStats[0];
        mCumulativePlayerStats[0] = cumulativeTeamA.mPlayerTotalStats;
    }
    {
        TeamStats cumulativeTeamB = tracker->mCumulativeTeamStats[1];
        mCumulativePlayerStats[1] = cumulativeTeamB.mPlayerTotalStats;
    }

    summaryOrder[0] = SUMMARY_MATCH;
    summaryOrder[1] = SUMMARY_MATCH;
    summaryOrder[2] = SUMMARY_MATCH;
    summaryOrder[3] = SUMMARY_MATCH;
}

/**
 * Offset/Address/Size: 0x1CEC | 0x800FE88C | size: 0x9C
 */
SummaryOverlay::~SummaryOverlay()
{
    if (mSlideMenu != NULL)
    {
        delete mSlideMenu;
    }
}

/**
 * Offset/Address/Size: 0x1B00 | 0x800FE6A0 | size: 0x1EC
 */
void SummaryOverlay::SceneCreated()
{
    CrowdMood::Purge(true);

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("layer")),
        InlineHasher(nlStringLowerHash("summary")));

    mSlideMenu = new (nlMalloc(sizeof(FESlideMenu), 8, false)) FESlideMenu(pComp);

    mSlideMenu->AddMenuItem(SUMMARY_SLIDE_NAME);

    if (mShowCumulativeStats)
    {
        mSlideMenu->AddMenuItem(TOTAL_SUMMARY_SLIDE_NAME);
        summaryOrder[1] = SUMMARY_CUMULATIVE_MATCH;
        summaryOrder[3] = SUMMARY_CUMULATIVE_MATCH;
    }

    if (mShowUserStats)
    {
        mSlideMenu->AddMenuItem(USER_SLIDE_NAME);
        if (mShowCumulativeStats)
        {
            mSlideMenu->AddMenuItem(TOTAL_USER_SLIDE_NAME);
            summaryOrder[2] = SUMMARY_USER;
            summaryOrder[3] = SUMMARY_CUMULATIVE_USER;
        }
        else
        {
            summaryOrder[1] = SUMMARY_USER;
            summaryOrder[3] = SUMMARY_USER;
        }
    }

    mSlideMenu->m_doWrapAround = true;

    mButtons.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtons.SetState(mButtonState);
}

/**
 * Offset/Address/Size: 0x1834 | 0x800FE3D4 | size: 0x2CC
 */
void SummaryOverlay::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    if (!mSummaryDisplayed)
    {
        DisplayMatchSummary(SUMMARY_MATCH);
        mSummaryDisplayed = true;
    }

    if (g_pFEInput->JustPressed(m_controllingInput, PAD_TRIGGER_L, false, NULL)
        && (FrontEnd::m_feStateCurrent != FE_ALL_PADS || mContext == PAUSE))
    {
        if (mSlideMenu->PrevItem())
        {
            mCurrentSummaryIndex--;
            if (mCurrentSummaryIndex < 0)
            {
                mCurrentSummaryIndex = NUM_SUMMARIES - 1;
            }

            eSummaryType summaryType = summaryOrder[mCurrentSummaryIndex];
            if (summaryType == SUMMARY_MATCH || summaryType == SUMMARY_CUMULATIVE_MATCH)
            {
                DisplayMatchSummary(summaryType);
            }
            else
            {
                DisplayUserSummary(summaryType);
            }
        }
    }
    else if (g_pFEInput->JustPressed(m_controllingInput, PAD_TRIGGER_R, false, NULL)
             && (FrontEnd::m_feStateCurrent != FE_ALL_PADS || mContext == PAUSE))
    {
        if (mSlideMenu->NextItem())
        {
            mCurrentSummaryIndex++;
            if (mCurrentSummaryIndex == NUM_SUMMARIES)
            {
                mCurrentSummaryIndex = 0;
            }

            eSummaryType summaryType = summaryOrder[mCurrentSummaryIndex];
            if (summaryType == SUMMARY_MATCH || summaryType == SUMMARY_CUMULATIVE_MATCH)
            {
                DisplayMatchSummary(summaryType);
            }
            else
            {
                DisplayUserSummary(summaryType);
            }
        }
    }

    bool showAButton = mButtonState == ButtonComponent::BS_A_AND_B || mButtonState == ButtonComponent::BS_A_ONLY;
    bool showBButton = mButtonState == ButtonComponent::BS_A_AND_B || mButtonState == ButtonComponent::BS_B_ONLY;

    if (mContext == PAUSE)
    {
        if (showBButton && g_pFEInput->JustPressed(m_controllingInput, PAD_BUTTON_B, false, NULL))
        {
            PauseMenuScene* pauseScene = (PauseMenuScene*)nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_PAUSE, SCREEN_BACK, true);
            pauseScene->mStartAnimAtEnd = true;
            FEAudio::PlayAnimAudioEvent("sfx_back", false);
        }
    }
    else if (mContext == ENDGAME)
    {
        if (showAButton && g_pFEInput->JustPressed(FE_ALL_PADS, PAD_BUTTON_A, false, NULL))
        {
            if (FrontEnd::m_feStateCurrent != FE_ALL_PADS)
            {
                if (nlSingleton<GameInfoManager>::Instance()->mCurrentMode == GameInfoManager::GM_FRIENDLY)
                {
                    if (g_e3_Build)
                    {
                        FrontEnd::ReturnToFE();
                    }
                    else
                    {
                        nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_PAUSE_POST_GAME, SCREEN_FORWARD, true);
                    }
                }
                else
                {
                    FrontEnd::ReturnToFE();
                }

                FEAudio::PlayAnimAudioEvent("sfx_accept", false);
            }
        }
    }
}

static inline const unsigned short* LookupSummaryLocString(const char* stringId)
{

    nlLocalization* loc = g_pLocalization;
    unsigned long key = nlStringLowerHash(stringId);

    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    if (nlLocalization::StringLookup* entry = nlBSearch<nlLocalization::StringLookup, unsigned long>(key, loc->m_LookupTable, (int)loc->m_pFile->StringCount))
    {
        return loc->m_FirstString + entry->StringOffset;
    }

    return MissingLocString;
}

/**
 * Offset/Address/Size: 0xA74 | 0x800FD614 | size: 0xDC0
 */
void SummaryOverlay::DisplayMatchSummary(eSummaryType matchSummaryType)
{
    extern unsigned long GetLOCCharacterName(eTeamID, bool, bool);

    m_pFEScene->m_pFEPackage->GetPresentation();
    TLComponentInstance* pComponentInstances[2] = { 0, 0 };
    TLSlide* pSlide = mSlideMenu->m_pMenuComp->GetActiveSlide();
    TLTextInstance* pTextInstanceRows[6];
    TLTextInstance* pTextInstanceRowHeading;
    int numRows;
    PlayerStats* displayedStats[2];

    for (int row = 0; row < 6; row++)
    {
        pTextInstanceRows[row] = 0;
    }

    if (matchSummaryType == SUMMARY_MATCH)
    {
        displayedStats[0] = &mCurrentPlayerStats[0];
        displayedStats[1] = &mCurrentPlayerStats[1];
        numRows = 6;
    }
    else
    {
        displayedStats[0] = &mCumulativePlayerStats[0];
        displayedStats[1] = &mCumulativePlayerStats[1];
        numRows = 6;

        WideBasicString unformatted(LookupSummaryLocString("TOTAL_MATCH_SUMMARY"));
        NLString numGamesString(LexicalCast<NLString, int>((int)nlSingleton<StatsTracker>::Instance()->mNumConsecutiveGamesPlayed));
        unsigned short tempBuffer[32];
        nlStrToWcs(numGamesString.c_str(), tempBuffer, 0x20);
        WideBasicString formatted(Format(unformatted, tempBuffer));
        memcpy(mTitleBuffer, formatted.c_str(), sizeof(mTitleBuffer));

        TLTextInstance* pTitleText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("Title")));
        pTitleText->SetString(mTitleBuffer);
    }

    TLTextInstance* pTitleText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pSlide,
        InlineHasher(nlStringLowerHash("Title")));

    if (mContext == ENDGAME)
    {
        pTitleText->m_LocStrId = 0xC6C81979;
        pTitleText->m_OverloadFlags |= 0x8;
    }
    else if (mContext == PAUSE)
    {
        pTitleText->m_LocStrId = 0x776AF58A;
        pTitleText->m_OverloadFlags |= 0x8;
    }

    int team;
    for (team = 0; team < 2; team++)
    {
        NLString statsStrings[6];

        if (matchSummaryType == SUMMARY_MATCH)
        {
            statsStrings[0] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumGoalsFor);
            statsStrings[1] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumShotsOnGoal);
            statsStrings[2] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumHitsMade);
            statsStrings[3] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumSteals);
            statsStrings[4] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumSTSAttempts);
            statsStrings[5] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumPerfectPasses);
        }
        else
        {
            statsStrings[0] = LexicalCast<NLString, int>((int)nlSingleton<StatsTracker>::Instance()->mNumGamesWon[team]);
            statsStrings[1] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumGoalsFor);
            statsStrings[2] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumShotsOnGoal);
            statsStrings[3] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumFouls);
            statsStrings[4] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumPowerupsHit);
            statsStrings[5] = LexicalCast<NLString, int>((int)displayedStats[team]->mNumSTSAttempts);
        }

        pSlide = mSlideMenu->m_pMenuComp->GetActiveSlide();
        pComponentInstances[team] = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash(SUMMARY_COL_NAMES[team])));

        pSlide = pComponentInstances[team]->GetActiveSlide();

        pTextInstanceRowHeading = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("TEAM")));
        pTextInstanceRowHeading->m_LocStrId = GetLOCCharacterName((eTeamID)displayedStats[team]->mRecordType.mTeamID, false, false);
        pTextInstanceRowHeading->m_OverloadFlags |= 0x8;

        int k;
        for (k = 0; k < numRows; k++)
        {
            pTextInstanceRows[k] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pSlide,
                InlineHasher(nlStringLowerHash(SUMMARY_ROW_NAMES[k])));

            nlStrToWcs(statsStrings[k].c_str(), mBuffersColBySide[team][k], 0x20);
            pTextInstanceRows[k]->SetString(mBuffersColBySide[team][k]);
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800FCBA0 | size: 0xA74
 */
void SummaryOverlay::DisplayUserSummary(eSummaryType matchSummaryType)
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLComponentInstance* pComponentInstances[4] = { 0, 0, 0, 0 };
    TLSlide* pSlide = mSlideMenu->m_pMenuComp->GetActiveSlide();
    TLTextInstance* pTextInstanceRows[6];
    PlayerStats* displayedStats[4];
    TLTextInstance* pTitleText;
    int j;
    int user;

    for (int row = 0; row < 6; row++)
    {
        pTextInstanceRows[row] = 0;
    }

    for (j = 0; j < 4; j++)
    {
        if (matchSummaryType == SUMMARY_USER)
        {
            displayedStats[j] = &mCurrentUserStats[j];
        }
        else
        {
            displayedStats[j] = &mCumulativeUserStats[j];

            WideBasicString unformatted(LookupSummaryLocString("TOTAL_USER_STATS"));
            NLString numGamesString(LexicalCast<NLString, int>((int)nlSingleton<StatsTracker>::Instance()->mNumConsecutiveGamesPlayed));
            unsigned short tempBuffer[32];
            nlStrToWcs(numGamesString.c_str(), tempBuffer, 0x20);
            WideBasicString formatted(Format(unformatted, tempBuffer));
            memcpy(mTitleBuffer, formatted.c_str(), sizeof(mTitleBuffer));

            pTitleText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pSlide,
                InlineHasher(nlStringLowerHash("Title")));
            pTitleText->SetString(mTitleBuffer);
        }
    }

    for (user = 0; user < 4; user++)
    {
        NLString statsStrings[6];

        statsStrings[0] = LexicalCast<NLString, int>((int)displayedStats[user]->mNumGoalsFor);
        statsStrings[1] = LexicalCast<NLString, int>((int)displayedStats[user]->mNumShotsOnGoal);
        statsStrings[2] = LexicalCast<NLString, int>((int)displayedStats[user]->mNumFouls);
        statsStrings[3] = LexicalCast<NLString, int>((int)displayedStats[user]->mNumPowerupsUsed);
        statsStrings[4] = LexicalCast<NLString, int>((int)displayedStats[user]->mNumPowerupsHit);
        statsStrings[5] = LexicalCast<NLString, int>((int)displayedStats[user]->mNumSTSAttempts);

        pSlide = mSlideMenu->m_pMenuComp->GetActiveSlide();
        pComponentInstances[user] = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash(SUMMARY_COL_NAMES[user])));

        if (nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((unsigned short)user) == -1)
        {
            pComponentInstances[user]->m_bVisible = false;
        }
        else
        {
            pComponentInstances[user]->m_bVisible = true;
        }

        pSlide = pComponentInstances[user]->GetActiveSlide();

        int k;
        for (k = 0; k < 6; k++)
        {
            pTextInstanceRows[k] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pSlide,
                InlineHasher(nlStringLowerHash(SUMMARY_ROW_NAMES[k])));

            nlStrToWcs(statsStrings[k].c_str(), mBuffersColByUser[user][k], 0x20);
            pTextInstanceRows[k]->SetString(mBuffersColByUser[user][k]);
        }
    }

    (void)presentation;
}
