#include "port/region.h"  // PORT: one binary, three discs
#include "Game/SH/SHCupHub.h"
// PORT: defined in GameInfo.cpp.
extern "C" void PortLogCupState(const char* tag);
#include "Game/FE/feText.h"

#include "Game/GameSceneManager.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/GameInfo.h"
#include "Game/SH/SHCupTrophy.h"
#include "Game/SH/SHSaveLoad.h"
#include "NL/nlFormat.h"
#include "NL/nlMath.h"
#include "NL/nlMemory.h"
#include "NL/nlPrint.h"
#include "NL/nlLocalization.h"

static const char* HUB_LEAGUE_SLIDE_NAME = "LEAGUE";
static const char* HUB_BOWSER_SLIDE_NAME = "LEAGUE2KNOCKOUT";
static const char* HUB_KNOCKOUT2_SLIDE_NAME = "KNOCKOUT2";
static const char* HUB_KNOCKOUT4_SLIDE_NAME = "KNOCKOUT4";
static const char* HUB_KNOCKOUT8_SLIDE_NAME = "KNOCKOUT 8";
static const char* CUP_HUB_LAYER_NAME = "Layer";
static const char* CUP_PROGRESS_NAME = "PROGRESS2";
static const char* CUP_HIGHLIGHT_NAME = "highlite";
static const unsigned short* HUB_COLOUR_HIGHLIGHT_STRING = (const unsigned short*)L"{clr:FEEE00}";
static const unsigned short* HUB_COLOUR_HIGHLIGHT_JPN_STRING = (const unsigned short*)L"{clr:E4821A}";

static char* HUBstandingsRowNames[10] = {
    "LINE_0", "LINE_1", "LINE_2", "LINE_3", "LINE_4", "LINE_5", "LINE_6", "LINE_7", "LINE_8", "LINE_9"
};
static char* HUB_ROWS[8] = {
    "row1", "row2", "row3", "row4", "row5", "row6", "row7", "row8"
};
static char* TEAM_NAMES[8] = {
    "Team1", "Team2", "Team3", "Team4", "Team5", "Team6", "Team7", "Team8"
};
static char* PROGRESS_IMAGE_NAMES[16] = {
    "PROGRESS_WHITE", "PROGRESS_WHITE2", "PROGRESS_WHITE3", "PROGRESS_WHITE4", "PROGRESS_WHITE5", "PROGRESS_WHITE6", "PROGRESS_WHITE7", "PROGRESS_WHITE8", "PROGRESS_WHITE9", "PROGRESS_WHITE10", "PROGRESS_WHITE11", "PROGRESS_WHITE12", "PROGRESS_WHITE13", "PROGRESS_WHITE14", "PROGRESS_WHITE15", "PROGRESS_WHITE16"
};
static char* PROGRESS_TEXT_NAMES[16] = {
    "Text", "Text2", "Text3", "Text4", "Text5", "Text6", "Text7", "Text8", "Text9", "Text10", "Text11", "Text12", "Text13", "Text14", "Text15", "Text16"
};

static const nlColour HUB_COLOUR_BLACK = { { 0x00, 0x00, 0x00, 0xFF } };
static const nlColour HUB_COLOUR_WHITE = { { 0xFF, 0xFF, 0xFF, 0xFF } };
static const nlColour HUB_COLOUR_HIGHLIGHT = { { 0xFE, 0xEE, 0x00, 0xFF } };
static const nlColour HUB_COLOUR_HIGHLIGHT_JPN = { { 0xE4, 0x82, 0x1A, 0xFF } };
// PORT: two reds, chosen by disc.
static const nlColour HIGHLIGHT_COLOUR_RED_JPN = { { 0xFF, 0x88, 0x88, 0xFF } };
static const nlColour HIGHLIGHT_COLOUR_RED_INT = { { 0x91, 0x1C, 0x21, 0xFF } };
static nlColour HIGHLIGHT_COLOUR_RED()
{
    return port_region() == PORT_REGION_JAPAN ? HIGHLIGHT_COLOUR_RED_JPN
                                              : HIGHLIGHT_COLOUR_RED_INT;
}
static const nlColour HIGHLIGHT_COLOUR_GREEN = { { 0x61, 0xBA, 0x36, 0xFF } };
static const nlColour HIGHLIGHT_COLOUR_BLUE = { { 0x00, 0xA4, 0xE3, 0xFF } };
static const nlColour HIGHLIGHT_COLOUR_YELLOW = { { 0xFF, 0xFF, 0x00, 0xFF } };
static const nlColour CUP_NUMBER_COLOUR = { { 0xFF, 0xFF, 0xFF, 0xFF } };
static const nlColour CUP_SEMI_COLOUR = { { 0xFE, 0xEE, 0x00, 0xFF } };
static const nlColour CUP_FINAL_COLOUR = { { 0xFF, 0xFF, 0x00, 0xFF } };

namespace
{
unsigned char gHubLeagueMovementSoundIsPlaying;
unsigned char gHubKnockoutMovementSoundIsPlaying;
void StopAllHubMovementSounds();
void StopHubKnockoutMovementSound();
void StartHubKnockoutMovementSound();
void StopHubLeagueMovementSound();
void StartHubLeagueMovementSound();
} // namespace

namespace
{
nlColour GetUserHighlightColour()
{
    if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
    {
        return HUB_COLOUR_HIGHLIGHT_JPN;
    }

    return HUB_COLOUR_HIGHLIGHT;
}

const unsigned short* GetUserHighlightColourAsString()
{
    if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
    {
        return HUB_COLOUR_HIGHLIGHT_JPN_STRING;
    }

    return HUB_COLOUR_HIGHLIGHT_STRING;
}

void StopAllHubMovementSounds()
{
    StopHubLeagueMovementSound();
    StopHubKnockoutMovementSound();
}

void StopHubKnockoutMovementSound()
{
    if (gHubKnockoutMovementSoundIsPlaying)
    {
        Audio::gWorldSFX.Stop(Audio::WORLDSFX_FE_LETTER_ROTATE_LOOP, cGameSFX::SFX_STOP_FIRST);
    }
    gHubKnockoutMovementSoundIsPlaying = false;
}

void StartHubKnockoutMovementSound()
{
    if (!gHubKnockoutMovementSoundIsPlaying)
    {
        FEAudio::PlayAnimAudioEvent("sfx_hub_knockout_movment", true);
    }
    gHubKnockoutMovementSoundIsPlaying = true;
}

void StopHubLeagueMovementSound()
{
    if (gHubLeagueMovementSoundIsPlaying)
    {
        Audio::gWorldSFX.Stop(Audio::WORLDSFX_FE_LETTER_ROTATE_LOOP, cGameSFX::SFX_STOP_FIRST);
    }
    gHubLeagueMovementSoundIsPlaying = false;
}

void StartHubLeagueMovementSound()
{
    if (!gHubLeagueMovementSoundIsPlaying)
    {
        FEAudio::PlayAnimAudioEvent("sfx_hub_league_movement", true);
    }
    gHubLeagueMovementSoundIsPlaying = true;
}
} // namespace

static inline const unsigned short* LookupLocHash(unsigned long key)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == NULL)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* lookup = nlBSearch<nlLocalization::StringLookup, unsigned long>(key, loc->m_LookupTable, loc->m_pFile->StringCount);
    if (lookup != NULL)
    {
        return loc->m_FirstString + lookup->StringOffset;
    }

    return MissingLocString;
}

static inline const unsigned short* LookupLocHash(unsigned long key, nlLocalization* loc)
{
    if (loc->m_LookupTable == NULL)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* lookup = nlBSearch<nlLocalization::StringLookup, unsigned long>(key, loc->m_LookupTable, loc->m_pFile->StringCount);
    if (lookup != NULL)
    {
        return loc->m_FirstString + lookup->StringOffset;
    }

    return MissingLocString;
}

#include "NL/nlBind.h"

typedef Detail::MemFunImpl<void, void (CupHubScene::*)()> MemFunImpl_CupHubScene_v;
typedef BindExp1<void, MemFunImpl_CupHubScene_v, CupHubScene*> BindExp1_vfmfcp;
typedef Function0<void>::FunctorImpl<BindExp1_vfmfcp> FunctorImpl_vfmfcp;

/**
 * Offset/Address/Size: 0x72C8 | 0x800F1024 | size: 0x6E4
 */
CupHubScene::CupHubScene(bool doAnimations, bool playAllKnockoutAnimations)
    : mTextColour(HUB_COLOUR_WHITE)
    , mDoAnimations(doAnimations)
    , mUpdatingStats(false)
    , mAllKnockoutAnimations(playAllKnockoutAnimations)
    , mSuperTeamAnimation(false)
    , mDoAutoSave(false)
    , mPlayPopSound(true)
    , mStatUpdateDelay(0.0f)
    , mSlideSwitchDelay(0.0f)
    , mHubState(HUB_INVALID)
{
    GameInfoManager* gameInfo;
    eUserGameResult lastResult;
    int i;
    int round;

    AsyncImage* captainImage = (AsyncImage*)nlMalloc(sizeof(AsyncImage), 0x20, true);
    captainImage = new (captainImage) AsyncImage("art/fe/CupLoadingScreensUI.res", 0);
    mCaptainImage = captainImage;

    lastResult = (gameInfo = nlSingleton<GameInfoManager>::Instance())->GetResultsOfLastUserGame();
    mHasHumanTeamPlayed = gameInfo->HasHumanGameBeenPlayed();

    i = 0;
    while (i < gameInfo->GetNumPlayingTeams())
    {
        if (mDoAnimations && (gameInfo->GetCurrentRoundNumber() != 0 || (gameInfo->GetCurrentRoundNumber() == 0 && gameInfo->mCurrentCup->mGameNumber != 0)))
        {
            TeamStats teamStats = gameInfo->mPreviousTeamStats[(u16)i];
            mAllTeamStats[i] = teamStats;
        }
        else
        {
            mAllTeamStats[i] = gameInfo->GetTeamStatsByIndex(i);
        }

        i++;
    }

    gameInfo->SetPreviousTeamStats();

    if (gameInfo->IsInTournamentMode())
    {
        gameInfo->DetermineNextMatchups(1);
    }
    else
    {
        gameInfo->DetermineNextMatchups(3);
    }

    round = gameInfo->GetCurrentRoundNumber();

    for (i = 0; i < 8; i++)
    {
        mRowMovement[i] = 0.0f;
        mAnimComponents[i] = NULL;
    }

    if (gameInfo->IsInTournamentMode() && gameInfo->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
    {
        if (!gameInfo->mDidRoundJustEnd)
        {
            mDoAnimations = false;
        }

        if (round == -2 && mDoAnimations)
        {
            mHubState = HUB_KNOCKOUT4;
        }
        else if (round == -2 && !mDoAnimations)
        {
            mHubState = HUB_KNOCKOUT2;
        }
        else if (round == -3 && mDoAnimations)
        {
            mHubState = HUB_KNOCKOUT8;
        }
        else if (round == -3 && !mDoAnimations)
        {
            mHubState = HUB_KNOCKOUT4;
        }
        else if (round == -4)
        {
            mHubState = HUB_KNOCKOUT8;
        }
        else if (round == -5 && mDoAnimations)
        {
            if (lastResult == RESULT_USER_ELIMINATED_QUARTER)
            {
                mHubState = HUB_KNOCKOUT8;
            }
            else if (lastResult == RESULT_USER_ELIMINATED_SEMI)
            {
                mHubState = HUB_KNOCKOUT4;
            }
            else
            {
                mHubState = HUB_KNOCKOUT2;
            }
        }
        else
        {
            mHubState = HUB_KNOCKOUT2;
        }
    }
    else
    {
        if (gameInfo->GetCurrentMode() == GameInfoManager::GM_BOWSER_CUP || gameInfo->GetCurrentMode() == GameInfoManager::GM_SUPER_BOWSER_CUP)
        {
            if ((round == -3 && mDoAnimations) || (round == -5 && gameInfo->GetResultsOfLastUserGame() == RESULT_USER_DOES_NOT_PLAYOFF_QUALIFY))
            {
                mHubState = HUB_BOWSER_TRANSITION;
            }
            else if (round == -3 && !mDoAnimations)
            {
                mHubState = HUB_KNOCKOUT4;
            }
            else if (round == -2 && mDoAnimations)
            {
                mHubState = HUB_KNOCKOUT4;
            }
            else if (round == -2 && !mDoAnimations)
            {
                mHubState = HUB_KNOCKOUT2;
            }
            else if (round == -5 && lastResult == RESULT_USER_ELIMINATED_SEMI)
            {
                mHubState = HUB_KNOCKOUT4;
            }
            else if (round == -5)
            {
                mHubState = HUB_KNOCKOUT2;
            }
            else if (round == -1 && mDoAnimations)
            {
                mHubState = HUB_KNOCKOUT2;
            }
            else if (round == -1 && !mDoAnimations)
            {
                mHubState = HUB_KNOCKOUT2;
            }
            else
            {
                mHubState = HUB_LEAGUE;
            }
        }
        else
        {
            mHubState = HUB_LEAGUE;
        }
    }

    if (mAllKnockoutAnimations && !mDoAnimations)
    {
        if (gameInfo->GetCurrentMode() == GameInfoManager::GM_BOWSER_CUP || gameInfo->GetCurrentMode() == GameInfoManager::GM_SUPER_BOWSER_CUP)
        {
            mCurrentKnockoutAnimationRound = -3;
            mHubState = HUB_KNOCKOUT4;
        }
        else if (gameInfo->IsInTournamentMode() && gameInfo->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
        {
            u16 numRounds = gameInfo->mCurrentCup->GetNumRounds();
            s32 knockoutRound = -4;
            if (numRounds == 2)
            {
                knockoutRound = -3;
            }
            mCurrentKnockoutAnimationRound = knockoutRound;
        }
    }
    else if (mDoAnimations)
    {
        if (lastResult == RESULT_USER_ELIMINATED_SEMI)
        {
            mAllKnockoutAnimations = true;
            mCurrentKnockoutAnimationRound = -2;
        }
        else if (lastResult == RESULT_USER_ELIMINATED_QUARTER || lastResult == RESULT_USER_DOES_NOT_PLAYOFF_QUALIFY)
        {
            mAllKnockoutAnimations = true;
            mCurrentKnockoutAnimationRound = -3;
        }
    }
}

/**
 * Offset/Address/Size: 0x7224 | 0x800F0F80 | size: 0xA4
 */
CupHubScene::~CupHubScene()
{
    delete mCaptainImage;
}

// PORT: the numbers last reported, so entering the hub and the hub advancing on the same frame print one line.
static int gPortHubMode = -2;
static int gPortHubRound = -99;
static int gPortHubGame = -99;

static void port_HubReport(bool always)
{
    GameInfoManager* pCupInfo = nlSingleton<GameInfoManager>::s_pInstance;
    BaseCup* pCup = (pCupInfo != NULL) ? pCupInfo->GetCurrentCup() : NULL;
    const int mode = (pCupInfo != NULL) ? (int)pCupInfo->GetCurrentMode() : -2;
    const int round = (pCup != NULL) ? (int)pCup->mRoundNumber : -99;
    const int game = (pCup != NULL) ? (int)pCup->mGameNumber : -99;
    if (!always && mode == gPortHubMode && round == gPortHubRound
        && game == gPortHubGame)
        return;
    gPortHubMode = mode;
    gPortHubRound = round;
    gPortHubGame = game;
    PortLogCupState("hub");
}

/**
 * Offset/Address/Size: 0x71C0 | 0x800F0F1C | size: 0x64
 */
void CupHubScene::SceneCreated()
{
    port_HubReport(true);
    LoadCaptainImage();
    eHubState state = mHubState;
    switch (state)
    {
    case HUB_LEAGUE:
    case HUB_BOWSER_TRANSITION:
        CreateLeague();
        break;
    case HUB_KNOCKOUT2:
    case HUB_KNOCKOUT4:
    case HUB_KNOCKOUT8:
        CreateKnockout();
        break;
    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0x69A4 | 0x800F0700 | size: 0x81C
 */
void CupHubScene::Update(float fDeltaT)
{

    GameInfoManager* gameInfo;
    SaveLoadScene* scene;
    eFEINPUT_PAD userPad;
    unsigned char inputAllowed;
    FEPresentation* presentation;
    TLComponentInstance* pComp;
    TLTextInstance* pText;
    BasicGameInfo* game;
    int i;
    SceneList curSceneType;
    SceneList sideScene;
    FEPopupMenu* pPopup;

    BaseSceneHandler::Update(fDeltaT);

    // PORT: only a change is reported, since the hub updates every frame.
    port_HubReport(false);

    if (mCaptainImage->Update(true) && mDoAutoSave && SaveLoadScene::IsIOEnabled())
    {
        scene = (SaveLoadScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SAVE, SCREEN_NOTHING, false);
        scene->mIsAutoSaving = true;
        // PORT: here because SaveLoadScene cannot tell an autosave from an options-screen save.
        PortLogCupState("autosave");
        mDoAutoSave = false;
        return;
    }

    gameInfo = nlSingleton<GameInfoManager>::s_pInstance;

    if (!mHasHumanTeamPlayed && mHubState != HUB_LEAGUE)
    {
        if (mSlideSwitchDelay > 0.0f)
        {
            mSlideSwitchDelay -= fDeltaT;
            return;
        }

        mSlideSwitchDelay = 0.0f;
        mHasHumanTeamPlayed = true;
        CreateKnockout();
    }

    if (mDoAnimations)
    {
        switch (mHubState)
        {
        case HUB_LEAGUE:
        case HUB_BOWSER_TRANSITION:
            inputAllowed = UpdateLeague(fDeltaT);
            break;
        case HUB_KNOCKOUT8:
            inputAllowed = UpdateKnockout8(fDeltaT);
            break;
        case HUB_KNOCKOUT4:
            inputAllowed = UpdateKnockout4(fDeltaT);
            break;
        case HUB_KNOCKOUT2:
            inputAllowed = UpdateKnockout2(fDeltaT);
            break;
        }
    }
    else
    {
        inputAllowed = true;
    }

    if (!inputAllowed)
    {
        return;
    }

    if (mHubState == HUB_BOWSER_TRANSITION)
    {
        presentation = m_pFEScene->m_pFEPackage->GetPresentation();

        if (presentation->m_currentSlide->m_time >= (presentation->m_currentSlide->m_start + presentation->m_currentSlide->m_duration))
        {
        }
        else
        {
            return;
        }

        UpdateProgressIndicator();
        mHubState = HUB_KNOCKOUT4;
        presentation = m_pFEScene->m_pFEPackage->GetPresentation();

        pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("message")));

        pComp->SetActiveSlide("Slide1");
        pComp->Update(0.0f);
        pComp->m_bVisible = true;

        pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("Text")));

        pText->SetStringId("STANDINGS_SEMI");

        mSlideSwitchDelay = 1.0f;

        Audio::gWorldSFX.Stop((Audio::eWorldSFX)0x11, cGameSFX::SFX_STOP_ALL);
        StopAllHubMovementSounds();
        FEAudio::PlayAnimAudioEvent("sfx_standings_round_pop", false);
        return;
    }

    if (mSlideSwitchDelay > 0.0f)
    {
        mSlideSwitchDelay -= fDeltaT;
        return;
    }

    if (mAllKnockoutAnimations)
    {
        if (mCurrentKnockoutAnimationRound == -4)
        {
            mDoAnimations = true;
            mCurrentKnockoutAnimationRound = -3;
            mHubState = HUB_KNOCKOUT8;
            CreateKnockout();
            return;
        }

        if (mCurrentKnockoutAnimationRound == -3)
        {
            mDoAnimations = true;
            mCurrentKnockoutAnimationRound = -2;
            mHubState = HUB_KNOCKOUT4;
            CreateKnockout();
            return;
        }

        if (mCurrentKnockoutAnimationRound != -2)
            return;

        mDoAnimations = true;
        mHubState = HUB_KNOCKOUT2;

        game = gameInfo->GetMatchupInfo(-2, 0);

        if (!mSuperTeamAnimation
            && game->mTeamIndex[0] == TEAM_MYSTERY
            && gameInfo->GetUserSelectedCupTeam() != TEAM_MYSTERY
            && gameInfo->IsInRegularCupMode())
        {
            mSuperTeamAnimation = true;
            CreateKnockout();
            return;
        }

        mCurrentKnockoutAnimationRound = -5;
        mAllKnockoutAnimations = false;
        mSuperTeamAnimation = false;
        CreateKnockout();
        return;
    }

    mTickerManager.Update(fDeltaT);

    BaseSceneHandler* handler = nlSingleton<GameSceneManager>::Instance()->GetCurrentScene();

    if (handler == this)
    {
        mButtons.CentreButtons();
    }

    StopAllHubMovementSounds();

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, &userPad)
        && gameInfo->GetCurrentRoundNumber() != -5)
    {
        for (i = 0; i < 4; i++)
        {
            gameInfo->SetPlayingSide((u16)i, -1);
        }

        curSceneType = (SceneList)nlSingleton<GameSceneManager>::Instance()->GetSceneType(this);

        switch (curSceneType)
        {
        case SCENE_CUP_STANDINGS:
        case SCENE_CUP_STANDINGS_ANIM:
            sideScene = SCENE_CHOOSE_SIDES_CUP;
            break;
        case SCENE_SUPER_CUP_STANDINGS:
        case SCENE_SUPER_CUP_STANDINGS_ANIM:
            sideScene = SCENE_CHOOSE_SIDES_SUPER_CUP;
            break;
        case SCENE_TOURNAMENT_STANDINGS:
        case SCENE_TOURNAMENT_STANDINGS_ANIM:
            sideScene = SCENE_CHOOSE_SIDES_TOURNAMENT;
            break;
        }

        nlSingleton<GameSceneManager>::Instance()->Push(sideScene, SCREEN_FORWARD, true);
        return;
    }

    if (gameInfo->GetCurrentRoundNumber() == -5)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            EndCup();
            return;
        }
    }

    if (gameInfo->GetCurrentRoundNumber() == -5)
        return;

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        FEAudio::PlayAnimAudioEvent("sfx_back", false);

        pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(
            (ePopupMenu)0,
            Bind<void>(MemFun<CupHubScene, void>(&CupHubScene::ReturnToMainMenu), this),
            FEPopupMenu::Nothing);
    }
}

/**
 * Offset/Address/Size: 0x670C | 0x800F0468 | size: 0x298
 */
void CupHubScene::EndCup()
{
    PortLogCupState("end");
    if (nlSingleton<GameInfoManager>::Instance()->mDisplayTrophy[0] && nlSingleton<GameInfoManager>::Instance()->IsInCupMode())
    {
        nlSingleton<GameSceneManager>::Instance()->PopEntireStack();

        CupTrophyScene* trophyScene = (CupTrophyScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_TROPHY, SCREEN_FORWARD, false);

        eTrophyType trophyType = nlSingleton<GameInfoManager>::Instance()->GetTrophyTypeByCurrentMode();
        trophyScene->CreateTrophyScene(trophyType, ButtonComponent::BS_A_ONLY, true);
    }
    else if (nlSingleton<GameInfoManager>::Instance()->IsInTournamentMode())
    {
        if (nlSingleton<GameInfoManager>::Instance()->GetNumHumanTeams() > 1)
        {
            nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TOURNEY_BRAG, SCREEN_FORWARD, false);
        }
        else
        {
            FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

            popup->Create(
                POPUP_TOURNEY_OVER,
                Bind<void>(MemFun<CupHubScene, void>(&CupHubScene::ReturnToMainMenu), this),
                FEPopupMenu::Nothing);
        }
    }
    else
    {
        nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_BRAG, SCREEN_FORWARD, false);
    }
}

/**
 * Offset/Address/Size: 0x66C8 | 0x800F0424 | size: 0x44
 */
void CupHubScene::ReturnToMainMenu()
{
    nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
    nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, SCREEN_FORWARD, false);
}

/**
 * Offset/Address/Size: 0x5F08 | 0x800EFC64 | size: 0x7C0
 */
unsigned char CupHubScene::UpdateDisplayedStat()
{
    TLSlide* pSlide;
    TLTextInstance* pTextInstance;
    int standingsIndices[8];
    int numTeams = nlSingleton<GameInfoManager>::Instance()->GetNumPlayingTeams();
    int i;

    nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(mAllTeamStats, numTeams, standingsIndices, numTeams);

    for (i = 0; i < numTeams; i++)
    {
        pSlide = mAnimComponents[i]->GetActiveSlide();

        if (mOldStats[i][0] != mAllTeamStats[mStandingsIndices[i]].mNumWins)
        {
            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("wins")));

            BasicString<char, Detail::TempStringAllocator> winsString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mAllTeamStats[mStandingsIndices[i]].mNumWins);

            nlStrToWcs(winsString.c_str(), mColumnsByRowsBuffers[1][i], ARRAY_SIZE(mColumnsByRowsBuffers[1][i]));
            pTextInstance->SetString(mColumnsByRowsBuffers[1][i]);
            mOldStats[i][0] = mAllTeamStats[mStandingsIndices[i]].mNumWins;
            FEAudio::PlayAnimAudioEvent("sfx_stat", false);
            return 1;
        }

        if (mOldStats[i][1] != mAllTeamStats[mStandingsIndices[i]].mNumOTLosses)
        {
            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("draws")));

            BasicString<char, Detail::TempStringAllocator> drawsString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mAllTeamStats[mStandingsIndices[i]].mNumOTLosses);

            nlStrToWcs(drawsString.c_str(), mColumnsByRowsBuffers[2][i], ARRAY_SIZE(mColumnsByRowsBuffers[2][i]));
            pTextInstance->SetString(mColumnsByRowsBuffers[2][i]);
            mOldStats[i][1] = mAllTeamStats[mStandingsIndices[i]].mNumOTLosses;
            FEAudio::PlayAnimAudioEvent("sfx_stat", false);
            return 1;
        }

        if (mOldStats[i][2] != mAllTeamStats[mStandingsIndices[i]].mNumLosses)
        {
            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("losses")));

            BasicString<char, Detail::TempStringAllocator> lossesString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mAllTeamStats[mStandingsIndices[i]].mNumLosses);

            nlStrToWcs(lossesString.c_str(), mColumnsByRowsBuffers[3][i], ARRAY_SIZE(mColumnsByRowsBuffers[3][i]));
            pTextInstance->SetString(mColumnsByRowsBuffers[3][i]);
            mOldStats[i][2] = mAllTeamStats[mStandingsIndices[i]].mNumLosses;
            FEAudio::PlayAnimAudioEvent("sfx_stat", false);
            return 1;
        }

        if (mOldStats[i][3] != mAllTeamStats[mStandingsIndices[i]].mNumPoints)
        {
            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("points")));

            BasicString<char, Detail::TempStringAllocator> pointsString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mAllTeamStats[mStandingsIndices[i]].mNumPoints);

            nlStrToWcs(pointsString.c_str(), mColumnsByRowsBuffers[4][i], ARRAY_SIZE(mColumnsByRowsBuffers[4][i]));
            pTextInstance->SetString(mColumnsByRowsBuffers[4][i]);
            mOldStats[i][3] = mAllTeamStats[mStandingsIndices[i]].mNumPoints;
            FEAudio::PlayAnimAudioEvent("sfx_stat", false);
            return 1;
        }
    }

    return 0;
}

/**
 * Offset/Address/Size: 0x4E34 | 0x800EEB90 | size: 0x10D4
 */
void CupHubScene::CreateLeague()
{
    GameInfoManager* gameInfo;
    GameInfoManager::eGameModes mode = (gameInfo = nlSingleton<GameInfoManager>::Instance())->mCurrentMode;
    u16 numTeams = gameInfo->GetNumPlayingTeams();
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLTextInstance* pTextInstance;
    TLComponentInstance* pComp;
    TLSlide* pSlide;
    TLTextInstance* title;
    int posOffset;

    gameInfo->GetUserSelectedCupTeam();

    presentation->SetActiveSlide(nlStringLowerHash(HUB_LEAGUE_SLIDE_NAME));
    UpdateProgressIndicator();

    title = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("STANDINGS SCREEN")));

    title->m_LocStrId = GetLOCStandingsName(mode);
    title->m_OverloadFlags |= 8;
    posOffset = (8 - numTeams) * 12;
    feVector3 position = title->GetAssetPosition();
    feVector3 rowPosition;

    if (mode == 5)
    {
        title->SetAssetPosition(position.f.x, position.f.y - (float)posOffset + 12.0f, position.f.z);
    }
    else
    {
        title->SetAssetPosition(position.f.x, position.f.y - (float)posOffset, position.f.z);
    }

    pComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUB_LEAGUE_SLIDE_NAME)),
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("ranks")));

    position = pComp->GetAssetPosition();
    pComp->SetAssetPosition(position.f.x, position.f.y - (float)posOffset, position.f.z);

    pComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUB_LEAGUE_SLIDE_NAME)),
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("titles")));

    position = pComp->GetAssetPosition();
    pComp->SetAssetPosition(position.f.x, position.f.y - (float)posOffset, position.f.z);

    nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(mAllTeamStats, numTeams, mStandingsIndices, numTeams);

    unsigned char useHighlight;
    eTeamID currentTeam;
    int row;
    int standingsIndices[8];

    for (row = 0; row < 8; row++)
    {
        useHighlight = false;
        pComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash(HUB_LEAGUE_SLIDE_NAME)),
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("ranks")));

        pSlide = pComp->GetActiveSlide();
        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash(HUBstandingsRowNames[row])));

        if (row < numTeams)
        {
            currentTeam = mAllTeamStats[mStandingsIndices[row]].mTeamIndex;
            useHighlight = IsUserRow(currentTeam);

            if (useHighlight && !mDoAnimations)
            {
                pTextInstance->SetAssetColour(GetUserHighlightColour());
            }
            else
            {
                pTextInstance->SetAssetColour(mTextColour);
            }

            if (mDoAnimations)
            {
                mOldRanks[mAllTeamStats[mStandingsIndices[row]].mTeamIndex] = row;
            }
        }
        else
        {
            pTextInstance->m_bVisible = false;
        }

        pComp = mAnimComponents[row];
        if (pComp == NULL)
        {
            pComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
                presentation,
                InlineHasher(nlStringLowerHash(HUB_LEAGUE_SLIDE_NAME)),
                InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
                InlineHasher(nlStringLowerHash(HUB_ROWS[row])));

            mAnimComponents[row] = pComp;
            rowPosition = pComp->GetAssetPosition();
            pComp->SetAssetPosition(rowPosition.f.x, rowPosition.f.y - (float)posOffset, rowPosition.f.z);
        }

        if (row >= numTeams)
        {
            pComp->m_bVisible = false;
            mAnimComponents[row] = NULL;
            continue;
        }

        pSlide = pComp->GetActiveSlide();
        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("team")));

        if (useHighlight)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        const unsigned short* locString = LookupLocHash(GetLOCTeamName(mAllTeamStats[mStandingsIndices[row]].mTeamIndex));

        BasicString<unsigned short, Detail::TempStringAllocator> teamNameWideString(locString);
        memcpy(mColumnsByRowsBuffers[0][row], teamNameWideString.c_str(), sizeof(mColumnsByRowsBuffers[0][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[0][row]);

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("wins")));
        mOldStats[row][0] = mAllTeamStats[mStandingsIndices[row]].mNumWins;
        BasicString<char, Detail::TempStringAllocator> winsStr = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][0]);
        nlStrToWcs(winsStr.c_str(), mColumnsByRowsBuffers[1][row], ARRAY_SIZE(mColumnsByRowsBuffers[1][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[1][row]);

        if (useHighlight)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("draws")));

        if (useHighlight)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        mOldStats[row][1] = mAllTeamStats[mStandingsIndices[row]].mNumOTLosses;
        BasicString<char, Detail::TempStringAllocator> drawsStr = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][1]);
        nlStrToWcs(drawsStr.c_str(), mColumnsByRowsBuffers[2][row], ARRAY_SIZE(mColumnsByRowsBuffers[2][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[2][row]);

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("losses")));

        if (useHighlight)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        mOldStats[row][2] = mAllTeamStats[mStandingsIndices[row]].mNumLosses;
        BasicString<char, Detail::TempStringAllocator> lossesStr = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][2]);
        nlStrToWcs(lossesStr.c_str(), mColumnsByRowsBuffers[3][row], ARRAY_SIZE(mColumnsByRowsBuffers[3][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[3][row]);

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("points")));

        if (useHighlight)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        mOldStats[row][3] = mAllTeamStats[mStandingsIndices[row]].mNumPoints;
        BasicString<char, Detail::TempStringAllocator> pointsStr = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][3]);
        nlStrToWcs(pointsStr.c_str(), mColumnsByRowsBuffers[4][row], ARRAY_SIZE(mColumnsByRowsBuffers[4][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[4][row]);
    }

    if (mDoAnimations)
    {
        for (int i = 0; i < gameInfo->GetNumPlayingTeams(); i++)
        {
            mAllTeamStats[i] = gameInfo->GetTeamStatsByIndex((u16)i);
        }

        nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(mAllTeamStats, numTeams, standingsIndices, numTeams);

        for (int i = 0; i < gameInfo->GetNumPlayingTeams(); i++)
        {
            mNewRanks[mAllTeamStats[standingsIndices[i]].mTeamIndex] = i;
        }

        mUpdatingStats = true;
    }

    TLSlide* tickerSlide = presentation->m_currentSlide;
    pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        tickerSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("TICKER2")));

    pSlide = pComp->GetActiveSlide();
    pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pSlide,
        InlineHasher(nlStringLowerHash("Group")),
        InlineHasher(nlStringLowerHash("TickerText")));

    mTickerManager.SetTickerTextInstance(pTextInstance);
    HandleButtonComponent();
}

/**
 * Offset/Address/Size: 0x4018 | 0x800EDD74 | size: 0xE1C
 */
void CupHubScene::CreateBowserLeague()
{
    GameInfoManager* const gameInfo = nlSingleton<GameInfoManager>::s_pInstance;
    GameInfoManager::eGameModes mode = gameInfo->GetCurrentMode();
    u16 numTeams = gameInfo->GetNumPlayingTeams();
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    gameInfo->GetUserSelectedCupTeam();
    TLTextInstance* pTextInstance;
    TLComponentInstance* pComp;
    TLSlide* pSlide;
    int standingsIndices[8];
    TLComponentInstance* starComp;
    TLTextInstance* title;
    int row;

    starComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("star rotation")));

    f32 starTime = starComp->GetActiveSlide()->m_time;

    presentation->SetActiveSlide(nlStringLowerHash(HUB_BOWSER_SLIDE_NAME));

    UpdateProgressIndicator();

    starComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("star rotation")));

    starComp->SetActiveSlide("Slide1");
    starComp->Update(starTime);

    title = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("STANDINGS SCREEN")));

    title->m_LocStrId = GetLOCStandingsName(mode);
    title->m_OverloadFlags |= 8;

    nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(mAllTeamStats, numTeams, standingsIndices, numTeams);

    for (row = 0; row < 8; row++)
    {
        pComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash(HUB_LEAGUE_SLIDE_NAME)),
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("ranks")));

        pSlide = pComp->GetActiveSlide();

        title = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash(HUBstandingsRowNames[row])));

        eTeamID currentTeam = mAllTeamStats[standingsIndices[row]].mTeamIndex;
        unsigned char useHighlightColour = IsUserRow(currentTeam);

        if (useHighlightColour)
        {
            title->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            title->SetAssetColour(mTextColour);
        }

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash(TEAM_NAMES[row])));

        if (useHighlightColour)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        unsigned long locString = GetLOCTeamName((eTeamID)mAllTeamStats[standingsIndices[row]].mTeamIndex);
        BasicString<unsigned short, Detail::TempStringAllocator> teamNameWideString(LookupLocHash(locString));
        memcpy(mColumnsByRowsBuffers[0][row], teamNameWideString.c_str(), sizeof(mColumnsByRowsBuffers[0][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[0][row]);

        pComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash(HUB_BOWSER_SLIDE_NAME)),
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash(HUB_ROWS[row])));

        pSlide = pComp->GetActiveSlide();

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("wins")));
        mOldStats[row][0] = mAllTeamStats[standingsIndices[row]].mNumWins;
        BasicString<char, Detail::TempStringAllocator> winsString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][0]);
        nlStrToWcs(winsString.c_str(), mColumnsByRowsBuffers[1][row], ARRAY_SIZE(mColumnsByRowsBuffers[1][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[1][row]);

        if (useHighlightColour)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        title = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("draws")));

        if (useHighlightColour)
        {
            title->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            title->SetAssetColour(mTextColour);
        }

        mOldStats[row][1] = mAllTeamStats[standingsIndices[row]].mNumOTLosses;
        BasicString<char, Detail::TempStringAllocator> drawsString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][1]);
        nlStrToWcs(drawsString.c_str(), mColumnsByRowsBuffers[2][row], ARRAY_SIZE(mColumnsByRowsBuffers[2][row]));
        title->SetString(mColumnsByRowsBuffers[2][row]);

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("losses")));

        if (useHighlightColour)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        mOldStats[row][2] = mAllTeamStats[standingsIndices[row]].mNumLosses;
        BasicString<char, Detail::TempStringAllocator> lossesString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][2]);
        nlStrToWcs(lossesString.c_str(), mColumnsByRowsBuffers[3][row], ARRAY_SIZE(mColumnsByRowsBuffers[3][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[3][row]);

        pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(pSlide, InlineHasher(nlStringLowerHash("points")));

        if (useHighlightColour)
        {
            pTextInstance->SetAssetColour(GetUserHighlightColour());
        }
        else
        {
            pTextInstance->SetAssetColour(mTextColour);
        }

        mOldStats[row][3] = mAllTeamStats[standingsIndices[row]].mNumPoints;
        BasicString<char, Detail::TempStringAllocator> pointsString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)mOldStats[row][3]);
        nlStrToWcs(pointsString.c_str(), mColumnsByRowsBuffers[4][row], ARRAY_SIZE(mColumnsByRowsBuffers[4][row]));
        pTextInstance->SetString(mColumnsByRowsBuffers[4][row]);
    }

    pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("message")));

    pComp->m_bVisible = false;

    TLSlide* tickerSlide = presentation->m_currentSlide;
    pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        tickerSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("TICKER2")));

    pSlide = pComp->GetActiveSlide();

    pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pSlide,
        InlineHasher(nlStringLowerHash("Group")),
        InlineHasher(nlStringLowerHash("TickerText")));

    mTickerManager.SetTickerTextInstance(pTextInstance);

    HandleButtonComponent();
}

/**
 * Offset/Address/Size: 0x32FC | 0x800ED058 | size: 0xD1C
 */
void CupHubScene::CreateKnockout()
{
    GameInfoManager* gameInfo;
    GameInfoManager::eGameModes currentMode = (gameInfo = nlSingleton<GameInfoManager>::Instance())->GetCurrentMode();
    u16 numTeams;
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    eTeamID knockoutTeams[8];
    eTeamID loserTeams[4] = {
        TEAM_INVALID,
        TEAM_INVALID,
        TEAM_INVALID,
        TEAM_INVALID,
    };
    int round;
    TLComponentInstance* starComp;
    TLTextInstance* pTextInstance;
    TLComponentInstance* pComp;
    TLSlide* pSlide;
    TLTextInstance* title;
    int i;
    TLComponentInstance* pXComponent;
    TLTextInstance* pText;
    BasicGameInfo* pGame;

    starComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("star rotation")));

    f32 starTime = starComp->GetActiveSlide()->m_time;

    if (!mHasHumanTeamPlayed)
    {
        mSlideSwitchDelay = 1.0f;
    }

    if (mHubState == HUB_KNOCKOUT8)
    {
        presentation->SetActiveSlide(nlStringLowerHash(HUB_KNOCKOUT8_SLIDE_NAME));

        round = -4;
        numTeams = 8;

        pGame = gameInfo->GetMatchupInfo(-4, 2);
        knockoutTeams[4] = pGame->mTeamIndex[0];
        knockoutTeams[5] = pGame->mTeamIndex[1];
        if (pGame->mFinalScore[0] > pGame->mFinalScore[1])
        {
            loserTeams[2] = pGame->mTeamIndex[1];
        }
        else if (pGame->mFinalScore[1] > pGame->mFinalScore[0])
        {
            loserTeams[2] = pGame->mTeamIndex[0];
        }

        pGame = gameInfo->GetMatchupInfo(-4, 3);
        knockoutTeams[6] = pGame->mTeamIndex[0];
        knockoutTeams[7] = pGame->mTeamIndex[1];
        if (pGame->mFinalScore[0] > pGame->mFinalScore[1])
        {
            loserTeams[3] = pGame->mTeamIndex[1];
        }
        else if (pGame->mFinalScore[1] > pGame->mFinalScore[0])
        {
            loserTeams[3] = pGame->mTeamIndex[0];
        }
    }
    else if (mHubState == HUB_KNOCKOUT4)
    {
        presentation->SetActiveSlide(nlStringLowerHash(HUB_KNOCKOUT4_SLIDE_NAME));
        round = -3;
        numTeams = 4;
    }
    else if (mHubState == HUB_KNOCKOUT2)
    {
        presentation->SetActiveSlide(nlStringLowerHash(HUB_KNOCKOUT2_SLIDE_NAME));
        round = -2;
        numTeams = 2;
    }

    starComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("star rotation")));

    starComp->SetActiveSlide("Slide1");
    starComp->Update(starTime);

    if (mHubState == HUB_KNOCKOUT8 || mHubState == HUB_KNOCKOUT4)
    {
        pGame = gameInfo->GetMatchupInfo(round, 1);
        knockoutTeams[2] = pGame->mTeamIndex[0];
        knockoutTeams[3] = pGame->mTeamIndex[1];
        if (pGame->mFinalScore[0] > pGame->mFinalScore[1])
        {
            loserTeams[1] = pGame->mTeamIndex[1];
        }
        else if (pGame->mFinalScore[1] > pGame->mFinalScore[0])
        {
            loserTeams[1] = pGame->mTeamIndex[0];
        }
    }

    if (mHubState == HUB_KNOCKOUT2 && mDoAnimations && gameInfo->GetCurrentRoundNumber() == -1)
    {
        pGame = &gameInfo->mUserInfo.mBowserCupFinalRound;
    }
    else if (mSuperTeamAnimation == true)
    {
        pGame = &gameInfo->mUserInfo.mBowserCupFinalRound;
    }
    else
    {
        pGame = gameInfo->GetMatchupInfo(round, 0);
    }

    knockoutTeams[0] = pGame->mTeamIndex[0];
    knockoutTeams[1] = pGame->mTeamIndex[1];
    if (pGame->mFinalScore[0] > pGame->mFinalScore[1])
    {
        loserTeams[0] = pGame->mTeamIndex[1];
    }
    else if (pGame->mFinalScore[1] > pGame->mFinalScore[0])
    {
        loserTeams[0] = pGame->mTeamIndex[0];
    }

    UpdateProgressIndicator();
    gameInfo->GetUserSelectedCupTeam();
    TLSlide* currentSlide = presentation->m_currentSlide;

    title = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("STANDINGS SCREEN")));

    title->m_LocStrId = GetLOCStandingsName(currentMode);
    title->m_OverloadFlags |= 8;

    {

        for (i = 0; i < numTeams; i++)
        {
            nlColour colour;
            if (IsUserRow(knockoutTeams[i]))
            {
                colour = GetUserHighlightColour();
            }
            else
            {
                colour = mTextColour;
            }

            pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                currentSlide,
                InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
                InlineHasher(nlStringLowerHash(TEAM_NAMES[i])));

            mAnimComponents[i] = pComp;
            pComp->SetActiveSlide("Eliminated");
            pComp->Update(0.0f);

            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("team")));

            pTextInstance->m_LocStrId = GetLOCTeamName(knockoutTeams[i]);
            pTextInstance->m_OverloadFlags |= 8;
            pTextInstance->SetAssetColour(colour);

            pXComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                pComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("theXfactor")));

            if ((loserTeams[i / 2] == knockoutTeams[i]) && mHasHumanTeamPlayed)
            {
                pXComponent->m_bVisible = true;
            }
            else
            {
                pXComponent->m_bVisible = false;
            }

            pComp->SetActiveSlide("Move");
            pComp->Update(0.0f);

            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("team")));

            pTextInstance->m_LocStrId = GetLOCTeamName(knockoutTeams[i]);
            pTextInstance->m_OverloadFlags |= 8;
            pTextInstance->SetAssetColour(colour);

            pXComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                pComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("theXfactor")));

            pXComponent->m_bVisible = false;

            pComp->SetActiveSlide("Neutral");
            pComp->Update(0.0f);

            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("team")));

            pTextInstance->m_LocStrId = GetLOCTeamName(knockoutTeams[i]);
            pTextInstance->m_OverloadFlags |= 8;
            pTextInstance->SetAssetColour(colour);

            pXComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                pComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("theXfactor")));

            if ((loserTeams[i / 2] == knockoutTeams[i]) && mHasHumanTeamPlayed)
            {
                pXComponent->m_bVisible = true;
            }
            else
            {
                pXComponent->m_bVisible = false;
            }
        }

        if (mDoAnimations)
        {
            if (mHubState == HUB_KNOCKOUT8)
            {
                pGame = gameInfo->GetMatchupInfo(round, 2);
                mAnimatingKnockoutTeams[2] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 5 : 4;

                pGame = gameInfo->GetMatchupInfo(round, 3);
                mAnimatingKnockoutTeams[3] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 7 : 6;
            }

            if (mHubState == HUB_KNOCKOUT8 || mHubState == HUB_KNOCKOUT4)
            {
                pGame = gameInfo->GetMatchupInfo(round, 1);
                mAnimatingKnockoutTeams[1] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 3 : 2;
            }

            if (mHubState == HUB_KNOCKOUT2 && mDoAnimations && gameInfo->GetCurrentRoundNumber() == -1)
            {
                pGame = &gameInfo->mUserInfo.mBowserCupFinalRound;
            }
            else if (mSuperTeamAnimation == true)
            {
                pGame = &gameInfo->mUserInfo.mBowserCupFinalRound;
            }
            else
            {
                pGame = gameInfo->GetMatchupInfo(round, 0);
            }

            mAnimatingKnockoutTeams[0] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 1 : 0;
            mKnockoutLoserAnimations = true;

            pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                currentSlide,
                InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
                InlineHasher(nlStringLowerHash("message")));

            pComp->m_bVisible = false;
            mSlideSwitchDelay = 2.0f;
        }
        else
        {
            pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                currentSlide,
                InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
                InlineHasher(nlStringLowerHash("message")));

            pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("Text")));

            switch (mHubState)
            {
            case HUB_KNOCKOUT8:
                pText->SetStringId("STANDINGS_QUARTER");
                title->m_bVisible = false;
                break;
            case HUB_KNOCKOUT4:
                pText->SetStringId("STANDINGS_SEMI");
                break;
            case HUB_KNOCKOUT2:
                pText->SetStringId("STANDINGS_FINAL");
                break;
            }

            if (!mAllKnockoutAnimations && mPlayPopSound)
            {
                FEAudio::PlayAnimAudioEvent("sfx_standings_round_pop", false);
                mPlayPopSound = false;
            }
        }
    }

    if (round == -2)
    {
        pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("message2")));

        pComp->m_bVisible = false;
    }

    TLSlide* tickerSlide = presentation->m_currentSlide;

    pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        tickerSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("TICKER2")));

    pSlide = pComp->GetActiveSlide();

    pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pSlide,
        InlineHasher(nlStringLowerHash("Group")),
        InlineHasher(nlStringLowerHash("TickerText")));

    mTickerManager.SetTickerTextInstance(pText);

    HandleButtonComponent();
}

/**
 * Erased (inlined into UpdateLeague)
 */
void CupHubScene::Animate()
{
    int oldRank;
    int newRank;

    for (int i = 0; i < nlSingleton<GameInfoManager>::s_pInstance->GetNumPlayingTeams(); i++)
    {
        oldRank = mOldRanks[mAllTeamStats[i].mTeamIndex];
        newRank = mNewRanks[mAllTeamStats[i].mTeamIndex];

        if (oldRank != newRank)
        {
            mRowMovement[oldRank] = (float)((oldRank - newRank) * 24);
        }
    }
}

/**
 * Offset/Address/Size: 0x2FF4 | 0x800ECD50 | size: 0x308
 */
unsigned char CupHubScene::UpdateLeague(float fDeltaT)
{
    unsigned char shouldStartSound = 0;

    mStatUpdateDelay += fDeltaT;

    if (mUpdatingStats)
    {
        if (mStatUpdateDelay >= 0.2)
        {
            mStatUpdateDelay = 0.0f;

            if (UpdateDisplayedStat())
            {
                return 0;
            }

            Animate();

            mUpdatingStats = false;
            shouldStartSound = 1;
        }
        else
        {
            return 0;
        }
    }

    unsigned char shouldBreak = 0;
    unsigned int i = 0;

    while (i < 8)
    {
        if (mRowMovement[i] > 1.0)
        {
            float movement = 1.0f;
            mRowMovement[i] -= movement;

            feVector3 position = mAnimComponents[i]->GetAssetPosition();
            mAnimComponents[i]->SetAssetPosition(position.f.x, position.f.y + movement, position.f.z);

            shouldBreak = 1;
        }
        else if (mRowMovement[i] < -1.0)
        {
            mRowMovement[i] += 1.0f;

            feVector3 position = mAnimComponents[i]->GetAssetPosition();
            mAnimComponents[i]->SetAssetPosition(position.f.x, position.f.y - 1.0f, position.f.z);

            shouldBreak = 1;
        }

        i++;
    }

    if (shouldBreak)
    {
        if (shouldStartSound)
        {
            StartHubLeagueMovementSound();
        }

        return 0;
    }

    ColourUserRow();

    if (mHubState == HUB_BOWSER_TRANSITION)
    {
        CreateBowserLeague();
        ColourUserRow();
        mDoAnimations = false;
    }
    else
    {
        mDoAnimations = false;
        UpdateProgressIndicator();
    }

    StopHubLeagueMovementSound();

    if (nlSingleton<GameInfoManager>::Instance()->GetCurrentRoundNumber() != 0)
    {
        int i = 0;

        while (i < nlSingleton<GameInfoManager>::Instance()->GetNumPlayingTeams())
        {
            eTeamID userTeam = mAllTeamStats[i].mTeamIndex;
            if (mNewRanks[userTeam] == 0)
            {
                FECharacterSound::PlayCaptainName(mAllTeamStats[i].mTeamIndex);
                break;
            }

            i++;
        }
    }

    return 1;
}

/**
 * Offset/Address/Size: 0x2A64 | 0x800EC7C0 | size: 0x590
 */
unsigned char CupHubScene::UpdateKnockout8(float fDeltaT)
{

    TLSlide* pSlide1 = mAnimComponents[mAnimatingKnockoutTeams[0]]->GetActiveSlide();
    TLSlide* pSlide2 = mAnimComponents[mAnimatingKnockoutTeams[1]]->GetActiveSlide();
    TLSlide* pSlide3 = mAnimComponents[mAnimatingKnockoutTeams[2]]->GetActiveSlide();
    TLSlide* pSlide4 = mAnimComponents[mAnimatingKnockoutTeams[3]]->GetActiveSlide();
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;

    if (mKnockoutLoserAnimations && mSlideSwitchDelay > 0.0f)
    {
        mSlideSwitchDelay -= fDeltaT;
        mAnimComponents[mAnimatingKnockoutTeams[0]]->SetActiveSlide("Eliminated");
        mAnimComponents[mAnimatingKnockoutTeams[1]]->SetActiveSlide("Eliminated");
        mAnimComponents[mAnimatingKnockoutTeams[2]]->SetActiveSlide("Eliminated");
        mAnimComponents[mAnimatingKnockoutTeams[3]]->SetActiveSlide("Eliminated");

        if (mSlideSwitchDelay <= 0.0f)
            FEAudio::PlayAnimAudioEvent("sfx_hub_knockout_elimination", false);

        return 0;
    }

    mSlideSwitchDelay = 0.0f;

    if ((pSlide1->m_time < (pSlide1->m_start + pSlide1->m_duration)) || (pSlide2->m_time < (pSlide2->m_start + pSlide2->m_duration)) || (pSlide3->m_time < (pSlide3->m_start + pSlide3->m_duration)) || (pSlide4->m_time < (pSlide4->m_start + pSlide4->m_duration)))
        return 0;

    if (mKnockoutLoserAnimations)
    {
        BasicGameInfo* pGame = gameInfo->GetMatchupInfo(-4, 0);
        mAnimatingKnockoutTeams[0] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 0 : 1;
        mAnimComponents[mAnimatingKnockoutTeams[0]]->SetActiveSlide("Move");
        mAnimComponents[mAnimatingKnockoutTeams[0]]->GetActiveSlide()->Update(0.0f);

        pGame = gameInfo->GetMatchupInfo(-4, 1);
        mAnimatingKnockoutTeams[1] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 2 : 3;
        mAnimComponents[mAnimatingKnockoutTeams[1]]->SetActiveSlide("Move");
        mAnimComponents[mAnimatingKnockoutTeams[1]]->GetActiveSlide()->Update(0.0f);

        pGame = gameInfo->GetMatchupInfo(-4, 2);
        mAnimatingKnockoutTeams[2] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 4 : 5;
        mAnimComponents[mAnimatingKnockoutTeams[2]]->SetActiveSlide("Move");
        mAnimComponents[mAnimatingKnockoutTeams[2]]->GetActiveSlide()->Update(0.0f);

        pGame = gameInfo->GetMatchupInfo(-4, 3);
        mAnimatingKnockoutTeams[3] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 6 : 7;
        mAnimComponents[mAnimatingKnockoutTeams[3]]->SetActiveSlide("Move");
        mAnimComponents[mAnimatingKnockoutTeams[3]]->GetActiveSlide()->Update(0.0f);

        FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

        TLComponentInstance* vsComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("vs")));

        vsComp->SetActiveSlide("Slide2");

        mKnockoutLoserAnimations = false;
        StartHubKnockoutMovementSound();
        return 0;
    }

    {
        mDoAnimations = false;
        UpdateProgressIndicator();

        FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

        TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("message")));

        pComp->SetActiveSlide("Slide1");
        pComp->Update(0.0f);
        pComp->m_bVisible = true;

        TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("Text")));

        pText->SetStringId("STANDINGS_SEMI");

        mHubState = HUB_KNOCKOUT4;
        CreateKnockout();
        StopHubKnockoutMovementSound();

        if (!mAllKnockoutAnimations && mPlayPopSound)
        {
            FEAudio::PlayAnimAudioEvent("sfx_standings_round_pop", false);
            mPlayPopSound = false;
        }
        return 1;
    }
}

/**
 * Offset/Address/Size: 0x2600 | 0x800EC35C | size: 0x464
 */
unsigned char CupHubScene::UpdateKnockout4(float fDeltaT)
{

    TLSlide* pSlide1 = mAnimComponents[mAnimatingKnockoutTeams[0]]->GetActiveSlide();
    TLSlide* pSlide2 = mAnimComponents[mAnimatingKnockoutTeams[1]]->GetActiveSlide();
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;

    if (mKnockoutLoserAnimations && mSlideSwitchDelay > 0.0f)
    {
        mSlideSwitchDelay -= fDeltaT;
        mAnimComponents[mAnimatingKnockoutTeams[0]]->SetActiveSlide("Eliminated");
        mAnimComponents[mAnimatingKnockoutTeams[1]]->SetActiveSlide("Eliminated");

        if (mSlideSwitchDelay <= 0.0f)
            FEAudio::PlayAnimAudioEvent("sfx_hub_knockout_elimination", false);

        return 0;
    }

    mSlideSwitchDelay = 0.0f;

    if ((pSlide1->m_time < (pSlide1->m_start + pSlide1->m_duration)) || (pSlide2->m_time < (pSlide2->m_start + pSlide2->m_duration)))
        return 0;

    if (mKnockoutLoserAnimations)
    {
        BasicGameInfo* pGame = gameInfo->GetMatchupInfo(-3, 0);
        mAnimatingKnockoutTeams[0] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 0 : 1;
        mAnimComponents[mAnimatingKnockoutTeams[0]]->SetActiveSlide("Move");
        mAnimComponents[mAnimatingKnockoutTeams[0]]->GetActiveSlide()->Update(0.0f);

        pGame = gameInfo->GetMatchupInfo(-3, 1);
        mAnimatingKnockoutTeams[1] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 2 : 3;
        mAnimComponents[mAnimatingKnockoutTeams[1]]->SetActiveSlide("Move");
        mAnimComponents[mAnimatingKnockoutTeams[1]]->GetActiveSlide()->Update(0.0f);

        FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

        TLComponentInstance* vsComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("vs")));

        vsComp->SetActiveSlide("Slide2");

        mKnockoutLoserAnimations = false;
        StartHubKnockoutMovementSound();
        return 0;
    }

    mDoAnimations = false;
    UpdateProgressIndicator();

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLComponentInstance* pComp;

    {

        pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash("message")));
    }

    pComp->SetActiveSlide("Slide1");
    pComp->Update(0.0f);
    pComp->m_bVisible = true;

    feVector3 position = pComp->GetAssetPosition();
    pComp->SetAssetPosition(position.f.x, 60.0f, position.f.z);

    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Text")));

    pText->SetStringId("STANDINGS_FINAL");
    StopHubKnockoutMovementSound();

    if (!mAllKnockoutAnimations && mPlayPopSound)
    {
        FEAudio::PlayAnimAudioEvent("sfx_standings_round_pop", false);
        mPlayPopSound = false;
    }
    return 1;
}

/**
 * Offset/Address/Size: 0x1F6C | 0x800EBCC8 | size: 0x694
 */
unsigned char CupHubScene::UpdateKnockout2(float fDeltaT)
{

    TLSlide* pSlide1 = mAnimComponents[mAnimatingKnockoutTeams[0]]->GetActiveSlide();
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;

    if (mKnockoutLoserAnimations && mSlideSwitchDelay > 0.0f)
    {
        mSlideSwitchDelay -= fDeltaT;
        mAnimComponents[mAnimatingKnockoutTeams[0]]->SetActiveSlide("Eliminated");

        if (mSlideSwitchDelay <= 0.0f)
            FEAudio::PlayAnimAudioEvent("sfx_hub_knockout_elimination", false);

        return 0;
    }

    mSlideSwitchDelay = 0.0f;

    if (pSlide1->m_time < (pSlide1->m_start + pSlide1->m_duration))
        return 0;

    BasicGameInfo* pGame;

    if (gameInfo->GetCurrentRoundNumber() == -1)
    {
        pGame = &gameInfo->mUserInfo.mBowserCupFinalRound;
    }
    else
    {
        pGame = gameInfo->GetMatchupInfo(-2, 0);
    }

    mAnimatingKnockoutTeams[0] = (pGame->mFinalScore[0] > pGame->mFinalScore[1]) ? 0 : 1;

    if (mKnockoutLoserAnimations)
    {
        if (!mSuperTeamAnimation)
        {
            mAnimComponents[mAnimatingKnockoutTeams[0]]->SetActiveSlide("Move");
            mAnimComponents[mAnimatingKnockoutTeams[0]]->GetActiveSlide()->Update(0.0f);

            FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

            TLComponentInstance* vsComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                presentation->m_currentSlide,
                InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
                InlineHasher(nlStringLowerHash("vs")));

            vsComp->SetActiveSlide("Slide2");

            mKnockoutLoserAnimations = false;
            StartHubKnockoutMovementSound();
            return 0;
        }
    }

    if (mSuperTeamAnimation)
    {
        mDoAnimations = false;
        mKnockoutLoserAnimations = false;
        return 0;
    }

    mDoAnimations = false;
    UpdateProgressIndicator();

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("message2")));

    pComp->SetActiveSlide("Slide1");
    pComp->Update(0.0f);
    pComp->m_bVisible = true;

    FEAudio::PlayAnimAudioEvent("sfx_message_wins", false);

    eTeamID winnerTeam;
    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Text")));

    s16 winnerIndex = (s16)mAnimatingKnockoutTeams[0];
    winnerTeam = pGame->GetTeam(winnerIndex);
    nlLocalization::nlLanguage currentLanguage = g_pLocalization->m_CurrentLanguage;
    if (currentLanguage == nlLocalization::LangJapanese && winnerTeam == TEAM_MYSTERY)
    {
        BasicString<unsigned short, Detail::TempStringAllocator> winnerString = Format(
            BasicString<unsigned short, Detail::TempStringAllocator>(
                LookupLocHash(nlStringLowerHash("STANDINGS_WINNER"), g_pLocalization)),
            LookupLocHash(GetLOCCharacterName(TEAM_MYSTERY, true, false)));

        memcpy(mColumnsByRowsBuffers[0][0], winnerString.c_str(), sizeof(mColumnsByRowsBuffers[0][0]));
    }
    else
    {
        BasicString<unsigned short, Detail::TempStringAllocator> winnerString = Format(
            BasicString<unsigned short, Detail::TempStringAllocator>(
                LookupLocHash(nlStringLowerHash("STANDINGS_WINNER"), g_pLocalization)),
            LookupLocHash(GetLOCCharacterName(winnerTeam, false, false)));

        memcpy(mColumnsByRowsBuffers[0][0], winnerString.c_str(), sizeof(mColumnsByRowsBuffers[0][0]));
    }
    pText->SetString(mColumnsByRowsBuffers[0][0]);

    mAnimComponents[mAnimatingKnockoutTeams[0]]->m_bVisible = false;
    FECharacterSound::PlayCaptainName(winnerTeam);
    StopHubKnockoutMovementSound();
    return 1;
}

/**
 * Offset/Address/Size: 0x1860 | 0x800EB5BC | size: 0x70C
 */
void CupHubScene::UpdateProgressIndicator()
{

    GameInfoManager* gameInfo;
    int numRounds;
    int round;
    int currentRound;
    int displayRounds[16];
    eHubColour nodeColours[16];
    TLSlide* pSlide;
    TLComponentInstance* highlight;
    TLImageInstance* nodeImage;
    feVector3 position;

    numRounds = (gameInfo = nlSingleton<GameInfoManager>::Instance())->GetNumRounds();
    if (gameInfo->mDidRoundJustEnd && mDoAnimations && gameInfo->GetCurrentRoundNumber() != -5)
    {
        round = gameInfo->GetPreviousRoundNumber(-7);
        gameInfo->mDidRoundJustEnd = false;
        UpdateRoundMessage(true);
    }
    else
    {
        round = gameInfo->GetCurrentRoundNumber();
        UpdateRoundMessage(false);
    }

    currentRound = round;
    SetRoundColours(nodeColours, 16);

    TLSlide* initialSlide = m_pFEScene->m_pFEPackage->GetPresentation()->m_currentSlide;

    {

        TLComponentInstance* progress = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            initialSlide,
            InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
            InlineHasher(nlStringLowerHash(CUP_PROGRESS_NAME)));

        pSlide = progress->GetActiveSlide();
    }

    {

        highlight = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash(CUP_HIGHLIGHT_NAME)));
    }

    TLSlide* highlightSlide = highlight->GetActiveSlide();
    highlightSlide->m_uPlayMode = TLPM_LOOPING;

    if (gameInfo->GetCurrentMode() == GameInfoManager::GM_BOWSER_CUP)
    {
        numRounds = 9;
        if (round == -3)
        {
            currentRound = 14;
        }
        else if (round == -2 || round == -1)
        {
            currentRound = 15;
        }
    }
    else if (gameInfo->GetCurrentMode() == GameInfoManager::GM_SUPER_BOWSER_CUP)
    {
        numRounds = 16;
        if (round == -3)
        {
            currentRound = 14;
        }
        else if (round == -2 || round == -1)
        {
            currentRound = 15;
        }
    }
    else if (gameInfo->IsInTournamentMode() && gameInfo->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
    {
        if (numRounds == 2 && round == -3)
        {
            currentRound = 0;
        }
        else if (numRounds == 2 && round == -2)
        {
            currentRound = 15;
        }
        else if (numRounds == 3 && round == -4)
        {
            currentRound = 0;
        }
        else if (numRounds == 3 && round == -3)
        {
            currentRound = 7;
        }
        else if (numRounds == 3 && round == -2)
        {
            currentRound = 15;
        }
    }

    if (numRounds == 5 || numRounds == 7 || numRounds == 10 || numRounds == 14)
    {

        TLComponentInstance* joiner = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("progress_joiner")));

        switch (numRounds)
        {
        case 5:
        case 7:
            joiner->SetActiveSlide("Slide2");
            break;
        case 10:
            joiner->SetActiveSlide("10");
            break;
        case 14:
            joiner->SetActiveSlide("14");
            break;
        }
    }

    for (int i = 0; i < 16; i++)
    {
        displayRounds[i] = -10;

        if (numRounds == 2)
        {
            if (i == 0)
            {
                displayRounds[i] = i;
            }
            else if (i == 15)
            {
                displayRounds[i] = i;
            }
        }
        else if (numRounds == 3)
        {
            if (gameInfo->IsInTournamentMode() && gameInfo->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
            {
                if (i == 0)
                {
                    displayRounds[i] = i;
                }
                else if (i == 7)
                {
                    displayRounds[i] = i;
                }
                else if (i == 15)
                {
                    displayRounds[i] = i;
                }
            }
            else
            {
                if (i == 0)
                {
                    displayRounds[i] = 0;
                }
                else if (i == 7)
                {
                    displayRounds[i] = 1;
                }
                else if (i == 15)
                {
                    displayRounds[i] = 2;
                }
            }
        }
        else if (numRounds == 5 || numRounds == 6)
        {
            if (i == 0)
            {
                displayRounds[i] = 0;
            }
            else if (i == 3)
            {
                displayRounds[i] = 1;
            }
            else if (i == 6)
            {
                displayRounds[i] = 2;
            }
            else if (i == 9)
            {
                displayRounds[i] = 3;
            }
            else if (i == 12)
            {
                displayRounds[i] = 4;
            }
            else if (i == 15 && numRounds == 6)
            {
                displayRounds[i] = 5;
            }
        }
        else if (numRounds == 7)
        {
            if (i <= 12)
            {
                if ((i % 2) == 0)
                {
                    displayRounds[i] = i / 2;
                }
            }
        }
        else if (numRounds == 9)
        {
            if (i <= 12 && (i % 2) == 0)
            {
                displayRounds[i] = i / 2;
            }
            else if (i == 14 || i == 15)
            {
                displayRounds[i] = i;
            }
        }
        else if (i < numRounds)
        {
            displayRounds[i] = i;
        }
    }

    if (round == -5)
    {
        if (gameInfo->IsInTournamentMode() && gameInfo->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
        {
            currentRound = 15;
        }
        else if (gameInfo->GetCurrentMode() == GameInfoManager::GM_BOWSER_CUP)
        {
            currentRound = 15;
        }
        else
        {
            currentRound = numRounds - 1;
        }

        highlight->m_bVisible = false;
    }

    int nodeColourIndex = 0;

    for (int i = 0; i < 16; i++)
    {

        nodeImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash(PROGRESS_IMAGE_NAMES[i])));

        if (displayRounds[i] != -10)
        {
            if ((displayRounds[i] >= 0 && displayRounds[i] < currentRound) || (round == -5 && currentRound == displayRounds[i]))
            {
                if (nodeColours[nodeColourIndex] == (eHubColour)0)
                {
                    nodeImage->SetAssetColour(HIGHLIGHT_COLOUR_RED());
                }
                else if (nodeColours[nodeColourIndex] == (eHubColour)1)
                {
                    nodeImage->SetAssetColour(HIGHLIGHT_COLOUR_GREEN);
                }
                else if (nodeColours[nodeColourIndex] == (eHubColour)2)
                {
                    nodeImage->SetAssetColour(HIGHLIGHT_COLOUR_BLUE);
                }
                else if (nodeColours[nodeColourIndex] == (eHubColour)3)
                {
                    nodeImage->SetAssetColour(HIGHLIGHT_COLOUR_YELLOW);
                }

                nodeColourIndex++;
            }

            if (currentRound == displayRounds[i])
            {
                position = nodeImage->GetAssetPosition();
                highlight->SetAssetPosition(position.f.x, position.f.y, position.f.z);
            }
        }
        else
        {
            nodeImage->m_bVisible = false;
        }
    }
}
/**
 * Erased (inlined into ColourUserRow)
 */
unsigned char CupHubScene::IsUserRow(eTeamID teamInRow)
{
    GameInfoManager* gameInfo;
    eTeamID userTeam = (gameInfo = nlSingleton<GameInfoManager>::Instance())->GetUserSelectedCupTeam();

    if ((gameInfo->mCurrentCup->mHumanTeams & (1 << teamInRow)) == 0)
        return 0;

    if ((gameInfo->GetNumHumanTeams() == 1) && (teamInRow == userTeam))
        return 1;

    return 0;
}

/**
 * Erased
 */
void CupHubScene::MakeTextBoxReallyWide(TLTextInstance& textInstance)
{
    nlVector2& boxSize = ((textInstance.m_OverloadFlags & 0x4) != 0)
                           ? textInstance.m_OverloadedAttributes.BoxSize
                           : ((FEText*)textInstance.m_component)->m_TextAttributes.BoxSize;
    nlVector2 bb = boxSize;
    bb.x = 999.9f;
    textInstance.m_OverloadedAttributes.BoxSize = bb;
    textInstance.m_OverloadFlags |= 0x4;
}

/**
 * Offset/Address/Size: 0x1698 | 0x800EB3F4 | size: 0x1C8
 */
void CupHubScene::ColourUserRow()
{

    FEPresentation* pres = GetPresentation();
    TLComponentInstance* pComp;
    TLTextInstance* pTextInstance;
    TLSlide* pSlide;
    int standingsIndices[8];
    int numTeams = nlSingleton<GameInfoManager>::Instance()->GetNumPlayingTeams();
    int row;
    eTeamID currentTeam;

    nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(mAllTeamStats, numTeams, standingsIndices, numTeams);

    for (row = 0; row < numTeams; row++)
    {

        {

            pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                pres->m_currentSlide,
                InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
                InlineHasher(nlStringLowerHash("ranks")));
        }

        pSlide = pComp->GetActiveSlide();

        {

            pTextInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                pSlide,
                InlineHasher(nlStringLowerHash(HUBstandingsRowNames[row])));
        }

        if (row < numTeams)
        {
            currentTeam = mAllTeamStats[standingsIndices[row]].mTeamIndex;
            if (IsUserRow(currentTeam))
            {
                pTextInstance->SetAssetColour(GetUserHighlightColour());
                break;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x15C4 | 0x800EB320 | size: 0xD4
 */
void CupHubScene::HandleButtonComponent()
{

    FEPresentation* pres = m_pFEPresentation;

    TLComponentInstance* inst = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pres->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtons.mButtonInstance = inst;
    inst->m_bVisible = false;

    s16 roundNum = nlSingleton<GameInfoManager>::Instance()->GetCurrentRoundNumber();
    if (roundNum == -5)
    {
        mButtons.SetState(ButtonComponent::BS_A_ONLY);
    }
    else
    {
        mButtons.SetState(ButtonComponent::BS_A_AND_B);
    }
}

/**
 * Offset/Address/Size: 0x12EC | 0x800EB048 | size: 0x2D8
 */
void CupHubScene::SetRoundColours(eHubColour* coloursArray, int sizeOfArray)
{
    GameInfoManager* gameInfo;
    int i;
    BaseCup* cup;

    gameInfo = nlSingleton<GameInfoManager>::Instance();

    for (i = 0; i < sizeOfArray; i++)
    {
        coloursArray[i] = HUB_COLOUR_BLUE;
    }

    if (!gameInfo->IsInTournamentMode())
    {
        if (!gameInfo->mDoingKnockout)
        {
            int lastPlayedRound = gameInfo->GetCurrentRoundNumber();
            if (lastPlayedRound != gameInfo->GetFirstRoundNumber())
            {
                lastPlayedRound = gameInfo->GetPreviousRoundNumber((s16)lastPlayedRound);
                cup = gameInfo->mCurrentCup;

                for (int k = 0; k <= lastPlayedRound; k++)
                {
                    int roundResult = *cup->GetRoundResults(k);
                    if (roundResult == 0)
                    {
                        coloursArray[k] = HUB_COLOUR_GREEN;
                    }
                    else if (roundResult == 1)
                    {
                        coloursArray[k] = HUB_COLOUR_RED;
                    }
                    else if (roundResult == 2)
                    {
                        coloursArray[k] = HUB_COLOUR_YELLOW;
                    }
                }
            }
        }
        else
        {
            cup = gameInfo->GetPreviousCup();
            int numRounds = cup->GetNumRounds();
            for (int k = 0; k < numRounds; k++)
            {
                int roundResult = *cup->GetRoundResults(k);
                if (roundResult == 0)
                {
                    coloursArray[k] = HUB_COLOUR_GREEN;
                }
                else if (roundResult == 1)
                {
                    coloursArray[k] = HUB_COLOUR_RED;
                }
                else if (roundResult == 2)
                {
                    coloursArray[k] = HUB_COLOUR_YELLOW;
                }
            }

            int round;
            cup = gameInfo->GetCurrentCup();
            round = gameInfo->GetCurrentRoundNumber();
            if (((u32)(round + 2) <= 1) || (round == -5))
            {
                int roundResult = *cup->GetRoundResults(0);
                if (roundResult == 0)
                {
                    coloursArray[numRounds] = HUB_COLOUR_GREEN;
                }
                else if ((roundResult == 1) || (roundResult == 2))
                {
                    coloursArray[numRounds] = HUB_COLOUR_RED;
                }
            }

            if (round == -5)
            {
                int roundResult = *cup->GetRoundResults(1);
                if (roundResult == 0)
                {
                    coloursArray[numRounds + 1] = HUB_COLOUR_GREEN;
                }
                else if ((roundResult == 1) || (roundResult == 2))
                {
                    coloursArray[numRounds + 1] = HUB_COLOUR_RED;
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x1DC | 0x800E9F38 | size: 0x1110
 */
void CupHubScene::UpdateRoundMessage(bool hideMessage)
{
    typedef BasicString<char, Detail::TempStringAllocator> NarrowString;

    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::Instance();
    int roundNumber = gameInfo->GetCurrentRoundNumber();

    TLSlide* pCurrentSlide = m_pFEScene->m_pFEPackage->GetPresentation()->m_currentSlide;

    TLComponentInstance* progress = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pCurrentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash(CUP_PROGRESS_NAME)));
    TLSlide* pSlide = progress->GetActiveSlide();

    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pSlide,
        InlineHasher(nlStringLowerHash("Text")));

    {
        nlVector2& boxSize = ((pText->m_OverloadFlags & 0x4) != 0) ? pText->m_OverloadedAttributes.BoxSize : ((FEText*)pText->m_component)->m_TextAttributes.BoxSize;
        nlVector2 bb = boxSize;
        bb.x = 999.9f;
        pText->m_OverloadedAttributes.BoxSize = bb;
        pText->m_OverloadFlags |= 0x4;
    }

    if (hideMessage || roundNumber == -5)
    {
        pText->m_bVisible = false;
        return;
    }

    pText->m_bVisible = true;

    BasicString<unsigned short, Detail::TempStringAllocator> leftTeam((const unsigned short*)L" ");
    BasicString<unsigned short, Detail::TempStringAllocator> rightTeam((const unsigned short*)L" ");
    BasicString<unsigned short, Detail::TempStringAllocator> roundWideString;
    BasicString<unsigned short, Detail::TempStringAllocator> unformatted;
    unsigned short roundWide[32] = { };

    BasicGameInfo* pGame = gameInfo->mGameInfo[gameInfo->GetCurrentMode()];

    if (gameInfo->IsInTournamentMode() && gameInfo->GetNumHumanTeams() > 1)
    {
        leftTeam = BasicString<unsigned short, Detail::TempStringAllocator>(
            LookupLocHash(GetLOCTeamName(pGame->mTeamIndex[0])));
        rightTeam = BasicString<unsigned short, Detail::TempStringAllocator>(
            LookupLocHash(GetLOCTeamName(pGame->mTeamIndex[1])));
    }
    else
    {
        eTeamID userTeam = gameInfo->GetUserSelectedCupTeam();

        if (userTeam == pGame->mTeamIndex[0])
        {
            leftTeam = BasicString<unsigned short, Detail::TempStringAllocator>(GetUserHighlightColourAsString());
            leftTeam = leftTeam.AppendInPlace(LookupLocHash(GetLOCTeamName(pGame->mTeamIndex[0])));
            leftTeam = leftTeam.AppendInPlace((const unsigned short*)L"{clr:pop}");

            rightTeam = BasicString<unsigned short, Detail::TempStringAllocator>(
                LookupLocHash(GetLOCTeamName(pGame->mTeamIndex[1])));
        }
        else if (userTeam == pGame->mTeamIndex[1])
        {
            leftTeam = BasicString<unsigned short, Detail::TempStringAllocator>(
                LookupLocHash(GetLOCTeamName(pGame->mTeamIndex[0])));

            rightTeam = BasicString<unsigned short, Detail::TempStringAllocator>(GetUserHighlightColourAsString());
            rightTeam = rightTeam.AppendInPlace(LookupLocHash(GetLOCTeamName(pGame->mTeamIndex[1])));
            rightTeam = rightTeam.AppendInPlace((const unsigned short*)L"{clr:pop}");
        }
    }

    {
        if (roundNumber == -4)
        {
            roundWideString = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0xFB611DAD));
            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x81CA8086));
        }
        else if (roundNumber == -3)
        {
            roundWideString = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0xB70B2037));
            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x81CA8086));
        }
        else if (roundNumber == -2 || roundNumber == -1)
        {
            roundWideString = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x97861DB3));
            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0x81CA8086));
        }
        else
        {
            NarrowString roundString = LexicalCast<NarrowString, int>(roundNumber + 1);

            nlStrToWcs(roundString.c_str(), roundWide, 32);
            roundWideString = BasicString<unsigned short, Detail::TempStringAllocator>(roundWide);

            unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0xC806357D));
        }
    }

    BasicString<unsigned short, Detail::TempStringAllocator> formatted = Format(unformatted, roundWideString, leftTeam, rightTeam);

    memcpy(mProgressBuffer, formatted.c_str(), sizeof(mProgressBuffer));
    mProgressBuffer[127] = 0;
    pText->SetString(mProgressBuffer);
}

/**
 * Offset/Address/Size: 0x0 | 0x800E9D5C | size: 0x1DC
 */
void CupHubScene::LoadCaptainImage()
{

    GameInfoManager* gameInfoMgr = nlSingleton<GameInfoManager>::Instance();

    TLImageInstance* imageInst = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash(CUP_HUB_LAYER_NAME)),
        InlineHasher(nlStringLowerHash("WALUIGI_L")));

    eTeamID teamId;
    if (gameInfoMgr->IsInCupMode() || gameInfoMgr->GetNumHumanTeams() == 1)
    {
        teamId = gameInfoMgr->GetUserSelectedCupTeam();
    }
    else
    {
        s16 roundNum = gameInfoMgr->GetCurrentRoundNumber();
        if (roundNum != -5)
        {
            u16 numTeams = gameInfoMgr->GetNumPlayingTeams();
            u32 randomResult = nlRandom(numTeams, &nlDefaultSeed);
            u16 randomIndex = (u16)randomResult;
            TeamStats stats = gameInfoMgr->GetTeamStatsByIndex(randomIndex);
            teamId = stats.mTeamIndex;
        }
        else
        {
            teamId = gameInfoMgr->FindWinningTeam();
        }
    }

    const char* teamName = GetTeamName(teamId);
    char buffer[0x80];
    nlSNPrintf(buffer, 0x80, "fe/cup_loadingscreens/%s_l", teamName);

    mCaptainImage->mImageInstance = imageInst;
    mCaptainImage->QueueLoad(buffer, true);
}
