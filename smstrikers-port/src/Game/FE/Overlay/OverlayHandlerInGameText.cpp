#include "NL/nlAlgorithm.h"
#include "Game/OverlayHandlerInGameText.h"
#include "Game/BaseSceneHandler.h"
#include "Game/DB/StatsTracker.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/tlSlide.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/OverlayManager.h"
// PORT: the Format declarations below leave instantiation to another file, and an inlined one is not emitted.
#include "NL/nlFormat.h"
#include "NL/nlLocalization.h"
#include "NL/nlSingleton.h"
#include "NL/nlTask.h"
#include "Game/FE/feInput.h"

#include "Game/FE/Overlay/OverlayHandlerSummary.h"
#include "Game/FE/feFinder.h"

#include "types.h"

extern FEInput* g_pFEInput;

template <typename StringType, typename ValueType>
StringType Format(const StringType&, const ValueType&);

template <typename StringType, typename T1, typename T2>
StringType Format(const StringType&, const T1&, const T2&);

unsigned long GetLOCTeamName(eTeamID);

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

static const char* OVERLAY_HANDLER_LAYER_NAME = "Layer"; // size: 0x4, address: 0x80395EAC

static const struct InGameTextEntry IGTTable[8] = {
    { SLIDE_NAME_TEXT_GOAL, "GOAL!", 0 },
    { SLIDE_NAME_TEXT_KICKOFF, "KICKOFF!", 0 },
    { SLIDE_NAME_TEXT_WINNER, "WINNER!", 1 },
    { SLIDE_NAME_TEXT_PAUSE, "Pause", 1 },
    { SLIDE_NAME_TEXT_TIE, "TIE!", 1 },
    { SLIDE_NAME_TEXT_LOADING, "LOADING...", 1 },
    { SLIDE_NAME_TEXT_SHOOT, "Shoot!", 2 },
    { SLIDE_NAME_TEXT_REPLAY, "REPLAY", 16 }
}; // size: 0x60, address: 0x802AD8E0

/**
 * Offset/Address/Size: 0x18EC | 0x800FC998 | size: 0x208
 */
/*
void BasicString<unsigned short, Detail::TempStringAllocator>::AppendInPlace<Detail::TempStringAllocator>(const BasicString<unsigned short, Detail::TempStringAllocator>&)
{
}
*/

/**
 * Offset/Address/Size: 0xF44 | 0x800FBFF0 | size: 0xA8
 */
InGameTextOverlay::InGameTextOverlay()
    : BaseOverlayHandler(2, POSITION_ALL)
{
    mCurrentSlideName = SLIDE_NAME_INVALID;
    mPendingSlideName = SLIDE_NAME_INVALID;
    this->SetVisible(false);
}

/**
 * Offset/Address/Size: 0xED8 | 0x800FBF84 | size: 0x6C
 */
InGameTextOverlay::~InGameTextOverlay()
{
}

/**
 * Offset/Address/Size: 0xE1C | 0x800FBEC8 | size: 0xBC
 */
void InGameTextOverlay::SetSlide(OverlaySlideName slideName)
{
    this->mPendingSlideName = slideName;
    if (this->mCurrentSlideName != this->mPendingSlideName)
    {
        this->m_pFEScene->m_pFEPackage->GetPresentation()->SetActiveSlide(IGTTable[this->mPendingSlideName].mSlideName);
        TLSlide* CurrentSlide = this->m_pFEScene->m_pFEPackage->GetPresentation()->m_currentSlide;
        if (CurrentSlide != NULL)
        {
            CurrentSlide->m_time = 0.0f;
            CurrentSlide->m_start = 0.0f;
            CurrentSlide->Update(0.0f);
        }
        if (mCurrentSlideName != SLIDE_NAME_INVALID)
        {
            this->m_pFEScene->m_pFEPackage->GetPresentation()->SetActiveSlide(IGTTable[mCurrentSlideName].mSlideName);
        }
    }
}

/**
 * Offset/Address/Size: 0xCB0 | 0x800FBD5C | size: 0x16C
 */
void InGameTextOverlay::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    if (this->mCurrentSlideName != this->mPendingSlideName)
    {
        this->mCurrentSlideName = this->mPendingSlideName;
        this->m_pFEScene->m_pFEPackage->GetPresentation()->SetActiveSlide(IGTTable[this->mCurrentSlideName].mSlideName);
        this->mVisibilityMask = IGTTable[this->mCurrentSlideName].mTaskVisibility;
        if (this->mVisibilityMask & nlTaskManager::m_pInstance->m_CurrState)
        {
            if (mWasLastVisible)
            {
                this->SetVisible(true);
            }
        }
        else
        {
            mWasLastVisible = m_bVisible;
            this->SetVisible(false);
        }

        switch (this->mCurrentSlideName)
        {
        case SLIDE_NAME_TEXT_WINNER:
            DisplayFinalScore();
            break;
        }
    }
    if (this->mCurrentSlideName == SLIDE_NAME_TEXT_WINNER && g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL) && m_bVisible)
    {
        nlSingleton<OverlayManager>::Instance()->SetVisible(OVERLAY_TEXT, false, false);
        SummaryOverlay* handler = (SummaryOverlay*)nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_SUMMARY, SCREEN_NOTHING, false);
        handler->mButtonState = ButtonComponent::BS_A_ONLY;
    }
}

/**
 * Offset/Address/Size: 0xCAC | 0x800FBD58 | size: 0x4
 */
void InGameTextOverlay::SceneCreated()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x800FB0AC | size: 0xCAC
 */
void InGameTextOverlay::DisplayFinalScore()
{
    typedef BasicString<char, Detail::TempStringAllocator> NarrowString;
    typedef BasicString<unsigned short, Detail::TempStringAllocator> WideString;

    int scoreLeft = g_pTeams[0]->m_nScore;
    int scoreRight = g_pTeams[1]->m_nScore;

    NarrowString scoreLeftString(LexicalCast<NarrowString, int>(scoreLeft));
    NarrowString scoreRightString(LexicalCast<NarrowString, int>(scoreRight));

    unsigned short scoreLeftWideString[32];
    unsigned short scoreRightWideString[32];

    nlStrToWcs(scoreLeftString.c_str(), scoreLeftWideString, 32);
    nlStrToWcs(scoreRightString.c_str(), scoreRightWideString, 32);

    const unsigned short* formatLocString = LookupLocHash(0x8C4180A4);

    WideString unformatted(formatLocString);
    WideString formatted(Format(unformatted, scoreLeftWideString, scoreRightWideString));

    FEPresentation* presentation = this->m_pFEScene->m_pFEPackage->GetPresentation();
    TLTextInstance* pTextInstance;
    const char* WINNER_SLIDE_NAME = IGTTable[SLIDE_NAME_TEXT_WINNER].mSlideName;
    long winningSide;

    if (this->mCurrentSlideName == SLIDE_NAME_TEXT_WINNER)
    {
        pTextInstance = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash(WINNER_SLIDE_NAME)),
            InlineHasher(nlStringLowerHash(OVERLAY_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("Score")));

        winningSide = (scoreLeft > scoreRight) ? 0 : 1;

        eTeamID winningTeam = nlSingleton<GameInfoManager>::Instance()->GetTeam((short)winningSide);

        const unsigned short* winnerNameLookup = LookupLocHash(GetLOCTeamName(winningTeam));
        WideString winnerNameWideString(winnerNameLookup);

        if (winningTeam == 3)
        {
            WideString space((const unsigned short*)L" ");
            winnerNameWideString = space.Append(winnerNameWideString);
        }

        const unsigned short* winnerFormatLocString = LookupLocHash(0x8610A152);

        WideString unformattedName(winnerFormatLocString);
        WideString formattedName(Format(unformattedName, winnerNameWideString.c_str()));

        TLInstance* winnerNameInstance = FEFinder<TLInstance, 3>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash(WINNER_SLIDE_NAME)),
            InlineHasher(nlStringLowerHash(OVERLAY_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("name")));

        TLTextInstance* winnerNameTextInstance = (TLTextInstance*)winnerNameInstance;

        memcpy(mWinnerBuffer, formattedName.c_str(), sizeof(mWinnerBuffer));
        winnerNameTextInstance->SetString(mWinnerBuffer);

        eTeamID team = nlSingleton<GameInfoManager>::Instance()->GetTeam(0);

        TLComponentInstance* pComponentInstance = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash(WINNER_SLIDE_NAME)),
            InlineHasher(nlStringLowerHash(OVERLAY_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("left_face")));

        pComponentInstance->SetActiveSlide(TEAM_SLIDE_NAMES[team]);

        team = nlSingleton<GameInfoManager>::Instance()->GetTeam(1);

        pComponentInstance = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash(WINNER_SLIDE_NAME)),
            InlineHasher(nlStringLowerHash(OVERLAY_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("right_face")));

        pComponentInstance->SetActiveSlide(TEAM_SLIDE_NAMES[team]);

        if (nlSingleton<GameInfoManager>::Instance()->mCurrentMode != 0)
        {
            if (g_pGame->m_eGameState == GS_OVERTIME)
            {
                ((void (*)(ePlayerStats, int, int, int, int, int, int))StatsTracker::Track)(STATS_OT_WIN, winningSide, 0, scoreLeft, scoreRight, 0, 0);
            }
            else
            {
                ((void (*)(ePlayerStats, int, int, int, int, int, int))StatsTracker::Track)(STATS_WIN, winningSide, 0, scoreLeft, scoreRight, 0, 0);
            }
        }
        else
        {
            nlSingleton<StatsTracker>::Instance()->mNumGamesWon[winningSide]++;
        }
    }

    memcpy(mScoresBuffer, formatted.c_str(), sizeof(mScoresBuffer));
    pTextInstance->SetString(mScoresBuffer);
}
