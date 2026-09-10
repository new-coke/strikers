#include "Game/SH/SHCupCheater.h"
#include "Game/GameInfo.h"
#include "Game/SH/SHCupHub.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feInput.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/tlTextInstance.h"
#include "NL/nlBasicString.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlMath.h"

#include "NL/nlBind.h"

/**
 * Offset/Address/Size: 0x1C00 | 0x800E9350 | size: 0x4C
 */
CupCheaterScene::CupCheaterScene()
    : BaseSceneHandler()
{
    mSniper = 0;
    mStriker = 0;
    mTactician = 0;
    mParamedic = 0;
    mVeteran = 0;
}

/**
 * Offset/Address/Size: 0x1B80 | 0x800E92D0 | size: 0x80
 */
CupCheaterScene::~CupCheaterScene()
{
    if (m_SlideMenu != NULL)
    {
        delete m_SlideMenu;
    }
}

/**
 * Offset/Address/Size: 0x14D4 | 0x800E8C24 | size: 0x6AC
 */
void CupCheaterScene::SceneCreated()
{
    typedef Detail::MemFunImpl<void, void (CupCheaterScene::*)()> MemFunImpl_CupCheaterScene_v;
    typedef BindExp1<void, MemFunImpl_CupCheaterScene_v, CupCheaterScene*> BindExp1_vfmfcp;

    void* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    TLComponentInstance* comp = FEFinder<TLComponentInstance, 4>::Find(
        (FEPresentation*)presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Menu")),
        InlineHasher(0),
        InlineHasher(0),
        InlineHasher(0));

    m_SlideMenu = new ((FESlideMenu*)nlMalloc(sizeof(FESlideMenu), 8, false)) FESlideMenu(comp);

    {
        Function<FnVoidVoid> callback(Bind<void, MemFunImpl_CupCheaterScene_v, CupCheaterScene*>(
            MemFun<CupCheaterScene, void>(&CupCheaterScene::OnSelectGameplay), this));
        m_SlideMenu->AddMenuItem("Slide1", callback);
    }

    {
        Function<FnVoidVoid> callback(Bind<void, MemFunImpl_CupCheaterScene_v, CupCheaterScene*>(
            MemFun<CupCheaterScene, void>(&CupCheaterScene::OnSelectHomeWin), this));
        m_SlideMenu->AddMenuItem("Slide2", callback);
    }

    {
        Function<FnVoidVoid> callback(Bind<void, MemFunImpl_CupCheaterScene_v, CupCheaterScene*>(
            MemFun<CupCheaterScene, void>(&CupCheaterScene::OnSelectAwayWin), this));
        m_SlideMenu->AddMenuItem("Slide3", callback);
    }

    {
        Function<FnVoidVoid> callback(Bind<void, MemFunImpl_CupCheaterScene_v, CupCheaterScene*>(
            MemFun<CupCheaterScene, void>(&CupCheaterScene::OnSelectHomeOTWin), this));
        m_SlideMenu->AddMenuItem("Slide4", callback);
    }

    {
        Function<FnVoidVoid> callback(Bind<void, MemFunImpl_CupCheaterScene_v, CupCheaterScene*>(
            MemFun<CupCheaterScene, void>(&CupCheaterScene::OnSelectAwayOTWin), this));
        m_SlideMenu->AddMenuItem("Slide5", callback);
    }

    m_SlideMenu->AddMenuItem("Slide6");
    m_SlideMenu->AddMenuItem("Slide7");
    m_SlideMenu->AddMenuItem("Slide8");
    m_SlideMenu->AddMenuItem("Slide9");

    m_SlideMenu->m_doWrapAround = true;
    m_SlideMenu->UpdatePresentation();
}

/**
 * Offset/Address/Size: 0x1140 | 0x800E8890 | size: 0x394
 */
void CupCheaterScene::Update(float dt)
{
    BaseSceneHandler::Update(dt);

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        if (m_SlideMenu->ApplyFunction())
        {
            FEAudio::PlayAnimAudioEvent("sfx_accept", false);
        }
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        if (m_SlideMenu->PrevItem())
        {
            FEAudio::PlayAnimAudioEvent("sfx_menu_move_up", false);
        }
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        if (m_SlideMenu->NextItem())
        {
            FEAudio::PlayAnimAudioEvent("sfx_menu_move_down", false);
        }
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xB, true, NULL))
    {
        switch (m_SlideMenu->m_currentSlide)
        {
        case 5:
            mSniper--;
            mSniper &= ~(mSniper >> 31);
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
            UpdateSlides();
            break;
        case 6:
            mStriker--;
            mStriker &= ~(mStriker >> 31);
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
            UpdateSlides();
            break;
        case 7:
            mTactician--;
            mTactician &= ~(mTactician >> 31);
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
            UpdateSlides();
            break;
        case 8:
            mParamedic--;
            mParamedic &= ~(mParamedic >> 31);
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
            UpdateSlides();
            break;
        }
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xC, true, NULL))
    {
        switch (m_SlideMenu->m_currentSlide)
        {
        case 5:
            mSniper++;
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_right", false);
            UpdateSlides();
            break;
        case 6:
            mStriker++;
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_right", false);
            UpdateSlides();
            break;
        case 7:
            mTactician++;
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_right", false);
            UpdateSlides();
            break;
        case 8:
            mParamedic++;
            FEAudio::PlayAnimAudioEvent("sfx_option_scroll_right", false);
            UpdateSlides();
            break;
        }
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x40, false, NULL))
    {
        mVeteran--;
        mVeteran &= ~(mVeteran >> 31);
        UpdateSlides();
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x20, false, NULL))
    {
        mVeteran++;
        UpdateSlides();
    }
}

/**
 * Offset/Address/Size: 0x10FC | 0x800E884C | size: 0x44
 */
void CupCheaterScene::OnSelectGameplay()
{
    CupCheaterScene* scene = (CupCheaterScene*)nlSingleton<GameSceneManager>::s_pInstance->Push(SCENE_SUPER_LOADING, SCREEN_NOTHING, true);
    scene->m_SlideMenu = NULL;
}

void CupCheaterScene::AddMilestoneStats()
{
    BasicGameInfo* gameInfo;
    GameInfoManager* gameInfoManagerCup = nlSingleton<GameInfoManager>::s_pInstance;
    gameInfo = gameInfoManagerCup->mGameInfo[gameInfoManagerCup->mCurrentMode];

    if (gameInfoManagerCup->IsInCupMode())
    {
        int team0 = gameInfo->mTeamIndex[0];
        bool homeAway = team0 != gameInfoManagerCup->GetUserSelectedCupTeam();

        if (mSniper > 0)
        {
            nlSingleton<StatsTracker>::s_pInstance->AddStat(STATS_GOALS_FOR, homeAway, 0, mSniper);
        }

        if (mStriker > 0)
        {
            nlSingleton<StatsTracker>::s_pInstance->AddStat(STATS_STS_ATTEMPTS, homeAway, 0, mStriker);
        }

        if (mTactician > 0)
        {
            nlSingleton<StatsTracker>::s_pInstance->AddStat(STATS_PERFECT_PASSES, homeAway, 0, mTactician);
        }

        if (mParamedic > 0)
        {
            nlSingleton<StatsTracker>::s_pInstance->AddStat(STATS_HITS_MADE, homeAway, 0, mParamedic);
        }

        if (mVeteran > 0)
        {
            nlSingleton<StatsTracker>::s_pInstance->AddMilestoneUserStat(STATS_GAMES_PLAYED, mVeteran);
        }
    }
}

static inline void TrackHomeWinResult()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<StatsTracker>::s_pInstance->SetBasicGameInfoPointer(gameInfoManager->mGameInfo[gameInfoManager->mCurrentMode], true);

    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_GOALS_FOR, 0, nlRandom(4, &nlDefaultSeed), -1, 0, 1, 0);
    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_WIN, 0, 0, 1, 0, 0, 0);

    gameInfoManager->SetRoundResult(false, 0);
}

static inline void TrackHomeOTWinResult()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<StatsTracker>::s_pInstance->SetBasicGameInfoPointer(gameInfoManager->mGameInfo[gameInfoManager->mCurrentMode], true);

    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_GOALS_FOR, 0, nlRandom(4, &nlDefaultSeed), -1, 0, 1, 0);
    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_OT_WIN, 0, 0, 1, 0, 0, 0);

    gameInfoManager->SetRoundResult(true, 0);
}

static inline void TrackAwayWinResult()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<StatsTracker>::s_pInstance->SetBasicGameInfoPointer(gameInfoManager->mGameInfo[gameInfoManager->mCurrentMode], true);

    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_GOALS_FOR, 1, nlRandom(4, &nlDefaultSeed), -1, 0, 1, 0);
    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_WIN, 1, 0, 0, 1, 0, 0);

    gameInfoManager->SetRoundResult(false, 1);
}

static inline void TrackAwayOTWinResult()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<StatsTracker>::s_pInstance->SetBasicGameInfoPointer(gameInfoManager->mGameInfo[gameInfoManager->mCurrentMode], true);

    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_GOALS_FOR, 1, nlRandom(4, &nlDefaultSeed), -1, 0, 1, 0);
    nlSingleton<StatsTracker>::s_pInstance->TrackStat(STATS_OT_WIN, 1, 0, 0, 1, 0, 0);

    gameInfoManager->SetRoundResult(true, 1);
}

/**
 * Offset/Address/Size: 0xE90 | 0x800E85E0 | size: 0x26C
 */
void CupCheaterScene::OnSelectHomeWin()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;
    CupHubScene* hubScene;

    if (gameInfoManager->mCurrentMode >= GameInfoManager::GM_MUSHROOM_CUP
        && gameInfoManager->mCurrentMode <= GameInfoManager::GM_SUPER_BOWSER_CUP)
    {
        nlSingleton<StatsTracker>::s_pInstance->SimulateRemainingGames();
    }

    if (nlSingleton<GameInfoManager>::s_pInstance->IsInCupMode())
    {
        nlSingleton<GameInfoManager>::s_pInstance->OnPreCupGameState();
    }

    TrackHomeWinResult();
    GameInfoManager* gameInfoManagerPost = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<GameSceneManager>::s_pInstance->PopEntireStack();

    AddMilestoneStats();

    nlSingleton<StatsTracker>::s_pInstance->CompileEndOfGameStats();

    if (gameInfoManagerPost->IsInCupMode())
    {
        gameInfoManagerPost->OnPostCupGameState();
    }
    else
    {
        gameInfoManagerPost->IncreaseGameNumber(true);

        while ((s16)gameInfoManagerPost->GetCurrentRoundNumber() != -5)
        {
            if (gameInfoManagerPost->DetermineNextMatchups(27))
            {
                break;
            }

            gameInfoManagerPost->IncreaseRoundNumber();
        }

        hubScene = (CupHubScene*)nlSingleton<GameSceneManager>::s_pInstance->Push((SceneList)0x18, SCREEN_NOTHING, false);
        hubScene->mDoAutoSave = true;
    }
}

/**
 * Offset/Address/Size: 0xC24 | 0x800E8374 | size: 0x26C
 */
void CupCheaterScene::OnSelectAwayWin()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;
    CupHubScene* hubScene;

    if (gameInfoManager->mCurrentMode >= GameInfoManager::GM_MUSHROOM_CUP
        && gameInfoManager->mCurrentMode <= GameInfoManager::GM_SUPER_BOWSER_CUP)
    {
        nlSingleton<StatsTracker>::s_pInstance->SimulateRemainingGames();
    }

    if (nlSingleton<GameInfoManager>::s_pInstance->IsInCupMode())
    {
        nlSingleton<GameInfoManager>::s_pInstance->OnPreCupGameState();
    }

    TrackAwayWinResult();
    GameInfoManager* gameInfoManagerPost = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<GameSceneManager>::s_pInstance->PopEntireStack();

    AddMilestoneStats();

    nlSingleton<StatsTracker>::s_pInstance->CompileEndOfGameStats();

    if (gameInfoManagerPost->IsInCupMode())
    {
        gameInfoManagerPost->OnPostCupGameState();
    }
    else
    {
        gameInfoManagerPost->IncreaseGameNumber(true);

        while ((s16)gameInfoManagerPost->GetCurrentRoundNumber() != -5)
        {
            if (gameInfoManagerPost->DetermineNextMatchups(27))
            {
                break;
            }

            gameInfoManagerPost->IncreaseRoundNumber();
        }

        hubScene = (CupHubScene*)nlSingleton<GameSceneManager>::s_pInstance->Push((SceneList)0x18, SCREEN_NOTHING, false);
        hubScene->mDoAutoSave = true;
    }
}

/**
 * Offset/Address/Size: 0x9B8 | 0x800E8108 | size: 0x26C
 */
void CupCheaterScene::OnSelectHomeOTWin()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;
    CupHubScene* hubScene;

    if (gameInfoManager->mCurrentMode >= GameInfoManager::GM_MUSHROOM_CUP
        && gameInfoManager->mCurrentMode <= GameInfoManager::GM_SUPER_BOWSER_CUP)
    {
        nlSingleton<StatsTracker>::s_pInstance->SimulateRemainingGames();
    }

    if (nlSingleton<GameInfoManager>::s_pInstance->IsInCupMode())
    {
        nlSingleton<GameInfoManager>::s_pInstance->OnPreCupGameState();
    }

    TrackHomeOTWinResult();
    GameInfoManager* gameInfoManagerPost = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<GameSceneManager>::s_pInstance->PopEntireStack();

    AddMilestoneStats();

    nlSingleton<StatsTracker>::s_pInstance->CompileEndOfGameStats();

    if (gameInfoManagerPost->IsInCupMode())
    {
        gameInfoManagerPost->OnPostCupGameState();
    }
    else
    {
        gameInfoManagerPost->IncreaseGameNumber(true);

        while ((s16)gameInfoManagerPost->GetCurrentRoundNumber() != -5)
        {
            if (gameInfoManagerPost->DetermineNextMatchups(27))
            {
                break;
            }

            gameInfoManagerPost->IncreaseRoundNumber();
        }

        hubScene = (CupHubScene*)nlSingleton<GameSceneManager>::s_pInstance->Push((SceneList)0x18, SCREEN_NOTHING, false);
        hubScene->mDoAutoSave = true;
    }
}

/**
 * Offset/Address/Size: 0x74C | 0x800E7E9C | size: 0x26C
 */
void CupCheaterScene::OnSelectAwayOTWin()
{
    GameInfoManager* gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;
    CupHubScene* hubScene;

    if (gameInfoManager->mCurrentMode >= GameInfoManager::GM_MUSHROOM_CUP
        && gameInfoManager->mCurrentMode <= GameInfoManager::GM_SUPER_BOWSER_CUP)
    {
        nlSingleton<StatsTracker>::s_pInstance->SimulateRemainingGames();
    }

    if (nlSingleton<GameInfoManager>::s_pInstance->IsInCupMode())
    {
        nlSingleton<GameInfoManager>::s_pInstance->OnPreCupGameState();
    }

    TrackAwayOTWinResult();
    GameInfoManager* gameInfoManagerPost = nlSingleton<GameInfoManager>::s_pInstance;

    nlSingleton<GameSceneManager>::s_pInstance->PopEntireStack();

    AddMilestoneStats();

    nlSingleton<StatsTracker>::s_pInstance->CompileEndOfGameStats();

    if (gameInfoManagerPost->IsInCupMode())
    {
        gameInfoManagerPost->OnPostCupGameState();
    }
    else
    {
        gameInfoManagerPost->IncreaseGameNumber(true);

        while ((s16)gameInfoManagerPost->GetCurrentRoundNumber() != -5)
        {
            if (gameInfoManagerPost->DetermineNextMatchups(27))
            {
                break;
            }

            gameInfoManagerPost->IncreaseRoundNumber();
        }

        hubScene = (CupHubScene*)nlSingleton<GameSceneManager>::s_pInstance->Push((SceneList)0x18, SCREEN_NOTHING, false);
        hubScene->mDoAutoSave = true;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800E7750 | size: 0x74C
 */
void CupCheaterScene::UpdateSlides()
{
    CupCheaterScene* const self = this;
    struct Locals
    {
        TLTextInstance* text;
        TLComponentInstance* comp;
    };
    const unsigned char currentSlide = self->m_SlideMenu->m_currentSlide;
    Locals locals;
    locals.comp = self->m_SlideMenu->m_pMenuComp;
    TLSlide* pSlide;

    for (int i = 0; i < 9; i++)
    {
        self->m_SlideMenu->SetSlideByIndex((unsigned char)i);
        pSlide = locals.comp->GetActiveSlide();

        TLTextInstance* foundText;
        foundText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("number1")));
        locals.text = foundText;

        BasicString<char, Detail::TempStringAllocator> Sniper(
            LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(self->mSniper));
        nlStrToWcs(Sniper.c_str(), self->mSniperBuffer, 10);
        locals.text->SetString(self->mSniperBuffer);

        locals.text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("number2")));

        BasicString<char, Detail::TempStringAllocator> Striker(
            LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(self->mStriker));
        nlStrToWcs(Striker.c_str(), self->mStrikerBuffer, 10);
        locals.text->SetString(self->mStrikerBuffer);

        locals.text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("number3")));

        BasicString<char, Detail::TempStringAllocator> Tactician(
            LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(self->mTactician));
        nlStrToWcs(Tactician.c_str(), self->mTacticianBuffer, 10);
        locals.text->SetString(self->mTacticianBuffer);

        locals.text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("number4")));

        BasicString<char, Detail::TempStringAllocator> Paramedic(
            LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(self->mParamedic));
        nlStrToWcs(Paramedic.c_str(), self->mParamedicBuffer, 10);
        locals.text->SetString(self->mParamedicBuffer);

        locals.text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pSlide,
            InlineHasher(nlStringLowerHash("number5")));

        BasicString<char, Detail::TempStringAllocator> Veteran(
            LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(self->mVeteran));
        nlStrToWcs(Veteran.c_str(), self->mVeteranBuffer, 10);
        locals.text->SetString(self->mVeteranBuffer);
    }

    self->m_SlideMenu->SetSlideByIndex((unsigned char)currentSlide);
}
