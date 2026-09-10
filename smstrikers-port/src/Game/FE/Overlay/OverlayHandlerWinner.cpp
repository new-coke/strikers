#include "Game/FE/Overlay/OverlayHandlerWinner.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feInput.h"
#include "Game/FE/Overlay/OverlayHandlerSummary.h"
#include "Game/GameInfo.h"
#include "Game/OverlayManager.h"
#include "NL/nlAlgorithm.h"
// PORT: the Format declarations below leave instantiation to another file, and an inlined one is not emitted.
#include "NL/nlFormat.h"
#include "NL/nlLocalization.h"

static char* WINNER_TEXTURES[9][3] = {
    { "fe/winners/daisy_action", "fe/winners/daisy_action_OUTLINE", "fe/winners/daisy_action_WHITE" },
    { "fe/winners/dk_action", "fe/winners/dk_action_OUTLINE", "fe/winners/dk_action_WHITE" },
    { "fe/winners/luigi_action", "fe/winners/luigi_action_OUTLINE", "fe/winners/luigi_action_WHITE" },
    { "fe/winners/mario_action", "fe/winners/mario_action_OUTLINE", "fe/winners/mario_action_WHITE" },
    { "fe/winners/peach_action", "fe/winners/peach_action_OUTLINE", "fe/winners/peach_action_WHITE" },
    { "fe/winners/waluigi_action", "fe/winners/waluigi_action_OUTLINE", "fe/winners/waluigi_action_WHITE" },
    { "fe/winners/wario_action", "fe/winners/wario_action_OUTLINE", "fe/winners/wario_action_WHITE" },
    { "fe/winners/yoshi_action", "fe/winners/yoshi_action_OUTLINE", "fe/winners/yoshi_action_WHITE" },
    { "fe/winners/mario_action", "fe/winners/mario_action_OUTLINE", "fe/winners/mario_action_WHITE" },
};

static const char* WINNER_HANDLER_LAYER_NAME = "Layer";

/**
 * Offset/Address/Size: 0x10C0 | 0x8010672C | size: 0x68
 */
WinnerOverlay::WinnerOverlay()
    : BaseOverlayHandler(-1, POSITION_ALL)
{
    mInputDelay = 1.0f;
    mDoingOutTransition = false;
    mWinningTeam = TEAM_INVALID;
}

/**
 * Offset/Address/Size: 0xFE4 | 0x80106650 | size: 0xDC
 */
WinnerOverlay::~WinnerOverlay()
{
    delete mWinnerActionWhite;
    delete mWinnerAction;
    delete mWinnerActionOutline;
}

template <typename StringType, typename ValueType>
StringType Format(const StringType&, const ValueType&);

template <typename StringType, typename ValueType1, typename ValueType2>
StringType Format(const StringType&, const ValueType1&, const ValueType2&);

static inline const unsigned short* LookupWinnerLocHash(unsigned long key)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* entry = nlBSearch<nlLocalization::StringLookup, unsigned long>(
        key, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry != 0)
    {
        return loc->m_FirstString + entry->StringOffset;
    }

    return MissingLocString;
}

/**
 * Offset/Address/Size: 0x304 | 0x80105970 | size: 0xCE0
 */
void WinnerOverlay::SceneCreated()
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

    BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupWinnerLocHash(0x8C4180A4));
    BasicString<unsigned short, Detail::TempStringAllocator> formatted;

    if (scoreLeft > scoreRight)
    {
        formatted = Format(unformatted, scoreLeftWideString, scoreRightWideString);
    }
    else
    {
        formatted = Format(unformatted, scoreRightWideString, scoreLeftWideString);
    }

    memcpy(mScoresBuffer, formatted.c_str(), sizeof(mScoresBuffer));

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    presentation->SetActiveSlide("MENU IN2");

    short winnerSide = (scoreLeft > scoreRight) ? 0 : 1;
    mWinningTeam = (eTeamID)nlSingleton<GameInfoManager>::Instance()->GetTeam(winnerSide);

    unsigned long winnerLocID = GetLOCTeamName((eTeamID)mWinningTeam);
    BasicString<unsigned short, Detail::TempStringAllocator> winnerNameWideString(LookupWinnerLocHash(winnerLocID));

    BasicString<unsigned short, Detail::TempStringAllocator> unformattedName(LookupWinnerLocHash(0x8610A152));
    BasicString<unsigned short, Detail::TempStringAllocator> formattedName(Format(unformattedName, winnerNameWideString.c_str()));

    memcpy(mWinnerBuffer, formattedName.c_str(), sizeof(mWinnerBuffer));

    for (int i = 0; i < 2; i++)
    {
        TLTextInstance* pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("Score")));
        pTextInstance->SetString(mScoresBuffer);

        TLTextInstance* winnerNameTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("name")));
        winnerNameTextInstance->SetString(mWinnerBuffer);

        presentation->SetActiveSlide("MENU IN");
    }

    TLImageInstance* pImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("action")));

    mWinnerAction = new (nlMalloc(sizeof(AsyncImage), 0x20, true)) AsyncImage("art/fe/WinnersUI.res", NULL);
    mWinnerAction->mImageInstance = pImage;
    mWinnerAction->QueueLoad(WINNER_TEXTURES[mWinningTeam][0], false);

    pImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("action_OUTLINE")));

    mWinnerActionOutline = new (nlMalloc(sizeof(AsyncImage), 0x20, true)) AsyncImage("art/fe/WinnersUI.res", NULL);
    mWinnerActionOutline->mImageInstance = pImage;
    mWinnerActionOutline->QueueLoad(WINNER_TEXTURES[mWinningTeam][1], false);

    pImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("action_WHITE")));

    mWinnerActionWhite = new (nlMalloc(sizeof(AsyncImage), 0x20, true)) AsyncImage("art/fe/WinnersUI.res", NULL);
    mWinnerActionWhite->mImageInstance = pImage;
    mWinnerActionWhite->QueueLoad(WINNER_TEXTURES[mWinningTeam][2], false);

    if (nlSingleton<GameInfoManager>::Instance()->IsInDemoMode())
    {
        TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("buttons")));

        pComp->m_bVisible = false;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8010566C | size: 0x304
 */
void WinnerOverlay::Update(float fDeltaT)
{

    BaseSceneHandler::Update(fDeltaT);

    mWinnerAction->Update(true);
    mWinnerActionOutline->Update(true);
    mWinnerActionWhite->Update(true);

    mInputDelay -= fDeltaT;

    TLSlide* slide = m_pFEPresentation->m_currentSlide;

    if (mDoingOutTransition)
    {
        if (slide->m_time >= (slide->m_start + slide->m_duration))
        {
            SummaryOverlay* pSummary = (SummaryOverlay*)nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_SUMMARY, SCREEN_NOTHING, true);
            pSummary->mButtonState = (ButtonComponent::ButtonState)1;
            return;
        }
    }

    if (!mDoingOutTransition)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL) == false)
        {
            return;
        }

        if (slide->m_time >= (slide->m_start + slide->m_duration))
        {
        }
        else
        {
            return;
        }

        if (mInputDelay <= 0.0)
        {
        }
        else
        {
            return;
        }

        mDoingOutTransition = true;
        m_pFEPresentation->SetActiveSlide("MENU IN2");

        mWinnerAction->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("action")));

        mWinnerAction->QueueLoad(WINNER_TEXTURES[mWinningTeam][0], false);

        mWinnerActionOutline->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("action_OUTLINE")));

        mWinnerActionOutline->QueueLoad(WINNER_TEXTURES[mWinningTeam][1], false);

        mWinnerActionWhite->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(WINNER_HANDLER_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("action_WHITE")));

        mWinnerActionWhite->QueueLoad(WINNER_TEXTURES[mWinningTeam][2], false);
    }
}
