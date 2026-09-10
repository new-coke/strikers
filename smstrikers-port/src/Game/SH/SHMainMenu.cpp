#include "Game/SH/SHMainMenu.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feMusic.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/FE/feScene.h"
#include "Game/FE/tlImageInstance.h"
#include "Game/SH/SHBackground.h"
#include "Game/main.h"
#include "NL/gl/glStruct.h"
#include "NL/nlLocalization.h"
#include "NL/nlPrint.h"

#include "NL/nlBind.h"

typedef Detail::MemFunImpl<void, void (SHMainMenu::*)(TLComponentInstance*)> MainMenuMemFun_t;
typedef BindExp2<void, MainMenuMemFun_t, SHMainMenu*, Placeholder<0> > MainMenuBind_t;

extern nlColour MenuHighliteColour;


bool SHMainMenu::mSnapMenuIntoPosition = false;
int SHMainMenu::mLastMenuItem = 0;

static char sSlideIn[] = "in";
static char sSlideOut[] = "out";

static unsigned long sUnlockedTickerMessages[SHMainMenu::NUM_ITEMS] = {
    0x10B8D08F,
    0xDB24E3FA,
    0x0755A109,
    0x35AABB0D,
    0x1B176F3B,
    0x268EF6F5,
    0x7B1F3B7E,
};

static unsigned long sLockedTickerMessages[SHMainMenu::NUM_ITEMS] = {
    0x10B8D08F,
    0xDB24E3FA,
    0x0F4F37A4,
    0x35AABB0D,
    0x1B176F3B,
    0x268EF6F5,
    0x7B1F3B7E,
};

/**
 * Offset/Address/Size: 0x1B14 | 0x800AB570 | size: 0x78
 */
static void onSelectFriendly(TLComponentInstance*)
{
    GameInfoManager::Instance()->SetMode(GameInfoManager::GM_FRIENDLY);
    GameInfoManager::Instance()->SetTeam(0, (eTeamID)3);
    GameInfoManager::Instance()->SetTeam(1, (eTeamID)3);
    GameSceneManager::Instance()->PopEntireStack();
    GameInfoManager::Instance()->ResetPlayingSides();
    GameSceneManager::Instance()->Push(SCENE_CHOOSE_CAPTAINS, SCREEN_FORWARD, false);
}

/**
 * Offset/Address/Size: 0x1AD0 | 0x800AB52C | size: 0x44
 */
static void onSelectCup(TLComponentInstance*)
{
    GameSceneManager::Instance()->PopEntireStack();
    GameSceneManager::Instance()->Push(SCENE_CUP_CHOOSE_CUP, SCREEN_FORWARD, false);
}

/**
 * Offset/Address/Size: 0x1A50 | 0x800AB4AC | size: 0x80
 */
static void onSelectSuperCup(TLComponentInstance*)
{
    if (!GameInfoManager::Instance()->IsSuperCupModeUnlocked())
    {
        FEPopupMenu* menu = (FEPopupMenu*)GameSceneManager::Instance()->Push(SCENE_POPUP_MENU, SCREEN_FORWARD, false);
        menu->Create(POPUP_SUPER_CUPS_LOCKED);
    }
    else
    {
        GameSceneManager::Instance()->PopEntireStack();
        GameSceneManager::Instance()->Push(SCENE_SUPER_CUP_CHOOSE_CUP, SCREEN_FORWARD, false);
    }
}

/**
 * Offset/Address/Size: 0x19C4 | 0x800AB420 | size: 0x8C
 */
static void onSelect101(TLComponentInstance*)
{
    GameInfoManager::Instance()->SetMode(GameInfoManager::GM_FRIENDLY);
    GameInfoManager::Instance()->SetTeam(0, (eTeamID)3);
    GameInfoManager::Instance()->SetTeam(1, (eTeamID)3);
    GameSceneManager::Instance()->PopEntireStack();
    GameInfoManager::Instance()->ResetPlayingSides();
    GameSceneManager::Instance()->Push(SCENE_CHOOSE_CAPTAINS, SCREEN_FORWARD, false);
    GameInfoManager::Instance()->mIsInStrikers101Mode = true;
    FEMusic::StartStreamIfDifferent(5);
}

/**
 * Offset/Address/Size: 0x1964 | 0x800AB3C0 | size: 0x60
 */
static void newTourn()
{
    GameSceneManager::Instance()->PopEntireStack();
    GameSceneManager::Instance()->Push(SCENE_TOURN_SETPARAMS, SCREEN_FORWARD, false);
    if (GameInfoManager::Instance()->mCustomTournamentInfo.m_cupConstructed)
    {
        GameInfoManager::Instance()->mCustomTournamentInfo.m_cup->mCupStarted = false;
    }
}

/**
 * Offset/Address/Size: 0x1914 | 0x800AB370 | size: 0x50
 */
static void continueTourn()
{
    GameInfoManager::Instance()->SetMode(GameInfoManager::GM_TOURNAMENT);
    GameSceneManager::Instance()->PopEntireStack();
    GameSceneManager::Instance()->Push(SCENE_TOURNAMENT_STANDINGS, SCREEN_FORWARD, false);
}

/**
 * Offset/Address/Size: 0x182C | 0x800AB288 | size: 0xE8
 */
static void confirmNewTourn()
{
    FEPopupMenu* menu = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
    menu->Create(
        POPUP_REALLY_OVERWRITE,
        Function<FnVoidVoid>(newTourn),
        Function<FnVoidVoid>(FEPopupMenu::Nothing));
    menu->mUnknownAA4 = false;
}

/**
 * Offset/Address/Size: 0x1680 | 0x800AB0DC | size: 0x1AC
 */
static void onSelectTournament(TLComponentInstance*)
{
    if (GameInfoManager::Instance()->mCustomTournamentInfo.m_cupConstructed
        && GameInfoManager::Instance()->mCustomTournamentInfo.m_cup->mCupStarted)
    {
        FEPopupMenu* menu = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
        menu->Create(
            POPUP_START_NEW_TOURNAMENT,
            Function<FnVoidVoid>(continueTourn),
            Function<FnVoidVoid>(confirmNewTourn));
        menu->SetBackButtonCallback(Function<FnVoidVoid>(FEPopupMenu::Nothing));
        GameInfoManager::Instance()->SetMode(GameInfoManager::GM_TOURNAMENT);
    }
    else
    {
        GameSceneManager::Instance()->PopEntireStack();
        GameSceneManager::Instance()->Push(SCENE_TOURN_SETPARAMS, SCREEN_FORWARD, false);

        if (GameInfoManager::Instance()->mCustomTournamentInfo.m_cupConstructed)
        {
            GameInfoManager::Instance()->mCustomTournamentInfo.m_cup->mCupStarted = false;
        }
    }
}

/**
 * Offset/Address/Size: 0x1634 | 0x800AB090 | size: 0x4C
 */
static void onSelectTrophies(TLComponentInstance*)
{
    GameSceneManager::Instance()->PopEntireStack();
    GameSceneManager::Instance()->Push(SCENE_TROPHY_ROOM, SCREEN_FORWARD, false);
    FEMusic::StartStreamIfDifferent(6);
}

/**
 * Offset/Address/Size: 0x15E8 | 0x800AB044 | size: 0x4C
 */
static void onSelectOptions(TLComponentInstance*)
{
    GameSceneManager::Instance()->PopEntireStack();
    GameSceneManager::Instance()->Push(SCENE_OPTIONS, SCREEN_FORWARD, false);
    FEMusic::StartStreamIfDifferent(7);
}

/**
 * Offset/Address/Size: 0x1538 | 0x800AAF94 | size: 0xB0
 */
SHMainMenu::SHMainMenu()
    : BaseSceneHandler()
    , m_itemDescriptions(NULL)
    , mMenuItems()
    , mButtons()
{
    mHighlightColour = MenuHighliteColour;
}

/**
 * Offset/Address/Size: 0x1380 | 0x800AADDC | size: 0x158
 */
SHMainMenu::~SHMainMenu()
{
    if (m_itemDescriptions != NULL)
    {
        delete m_itemDescriptions;
    }
}

static BackgroundScene* GetBackgroundScene()
{
    BackgroundScene* bgscene = (BackgroundScene*)GameSceneManager::Instance()->GetScene(SCENE_MARIO_BACKGROUND);
    return bgscene;
}

/**
 * Offset/Address/Size: 0xA3C | 0x800AA498 | size: 0x8C0
 */
void SHMainMenu::SceneCreated()
{
    typedef MenuItem<TLComponentInstance>::Callback MenuCallback;
    static const char* MenuNameTable[] = { "MENU ITEM1", "MENU ITEM2", "MENU ITEM3", "MENU ITEM4", "MENU ITEM7", "MENU ITEM5", "MENU ITEM6" };
    static void (*const ApplyFuncTable[])(TLComponentInstance*) = { onSelectFriendly, onSelectCup, onSelectSuperCup, onSelectTournament, onSelect101, onSelectTrophies, onSelectOptions };
    FEMusic::StartStreamIfDifferent(0);
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLTextInstance* scrollText;
    const gl_ScreenInfo* screenInfo;
    char menuname[64];
    TLComponentInstance* buttons;
    MenuItem<TLComponentInstance>* item;
    BackgroundScene* scene;
    scrollText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("TickerText")));

    screenInfo = glGetScreenInfo();
    FEScrollText* scrollTextDisplay = new (nlMalloc(sizeof(FEScrollText), 8, false)) FEScrollText(scrollText, 0, screenInfo->ScreenWidth + 50);
    m_itemDescriptions = scrollTextDisplay;
    scene = GetBackgroundScene();
    scene->SetVisible(false);
    for (int i = 0; i < NUM_ITEMS; i++)
    {
        nlSNPrintf(menuname, 64, "MENU ITEM%d", i + 1);
        TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(MenuNameTable[i])));
        TLComponentInstance* compinstance = (TLComponentInstance*)instance;
        item = mMenuItems.AddItem(compinstance);

        {
            MenuCallback openFunc(Bind<void>(MemFun<SHMainMenu, void, TLComponentInstance*>(&SHMainMenu::OpenItem), this, placeholder0));
            item->SetCallback(ON_HIGHLIGHT, openFunc);
        }

        {
            MenuCallback closeFunc(Bind<void>(MemFun<SHMainMenu, void, TLComponentInstance*>(&SHMainMenu::CloseItem), this, placeholder0));
            item->SetCallback(ON_UNHIGHLIGHT, closeFunc);
        }

        {
            MenuItem<TLComponentInstance>::Callback applyFunc(ApplyFuncTable[i]);
            item->SetCallback(ON_APPLY, applyFunc);
        }

        item->SetLockedFlag(false);
        if (i == mLastMenuItem)
        {
            OpenItem(compinstance);
            item->SetDisabledFlag(false);
        }
        else if (i == 2 && !g_e3_Build && !nlSingleton<GameInfoManager>::Instance()->IsSuperCupModeUnlocked())
        {
            CloseItem(compinstance);
            item->SetLockedFlag(true);
        }
        else
        {
            CloseItem(compinstance);
            if (g_e3_Build)
            {
                item->SetDisabledFlag(true);
            }
        }
        if (i == 2)
        {
            TLComponentInstance* lockedType = item->GetType();
            TLComponentInstance* lockedComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                lockedType->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("locked")),
                InlineHasher(0));
            u8 locked = item->IsLocked();
            lockedComp->m_bVisible = (bool)locked;
        }
    }
    mMenuItems.SetItem(mLastMenuItem);
    mMenuItems.SetFlag(1);
    scene->SetVisible(true);
    scene->mDesiredPlayMode = PM_STOP_AT_END;
    buttons = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));
    mButtons.mButtonInstance = buttons;
    mButtons.SetState(ButtonComponent::BS_A_AND_B);
    if (mSnapMenuIntoPosition)
    {
        FEAudio::EnableSounds(false);
        TLSlide* slide = presentation->m_currentSlide;
        BaseSceneHandler::Update(slide->m_start + slide->m_duration);
        FEAudio::EnableSounds(true);
    }
    mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
    mSnapMenuIntoPosition = true;
}

/**
 * Offset/Address/Size: 0x60C | 0x800AA068 | size: 0x430
 */
void SHMainMenu::OpenItem(TLComponentInstance* compinstance)
{

    compinstance->SetActiveSlide(sSlideIn);
    compinstance->Update(0.0f);

    TLComponentInstance* highlight = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("high")));

    highlight->SetActiveSlide(sSlideIn);
    highlight->Update(0.0f);

    TLComponentInstance* flash = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("flasher")));

    flash->SetActiveSlide("Slide1");
    flash->Update(0.0f);

    FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        highlight->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite")))
        ->SetAssetColour(mHighlightColour);

    if (mMenuItems.GetMenuItem()->IsDisabled())
    {

        TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            compinstance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("R JUST")));

        text->m_LocStrId = 0x38202C30;
        text->m_OverloadFlags |= 8;
    }

    if (mMenuItems.GetMenuItem()->IsLocked())
    {

        TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            compinstance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("R JUST")));

        if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
        {
            text->m_LocStrId = 0x67452206;
            text->m_OverloadFlags |= 8;
        }
        else
        {
            text->m_LocStrId = 0x2A68AC55;
            text->m_OverloadFlags |= 8;
        }

        TLComponentInstance* lockedComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            compinstance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("locked")));

        if (lockedComp != NULL)
        {
            lockedComp->m_bVisible = true;
        }
    }
    else
    {

        TLComponentInstance* lockedComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            compinstance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("locked")));

        if (lockedComp != NULL)
        {
            lockedComp->m_bVisible = false;
        }
    }

    if (mMenuItems.GetMenuItem()->IsLocked())
    {
        m_itemDescriptions->SetDisplayMessage(sLockedTickerMessages[mMenuItems.GetActiveItemIndex()]);
    }
    else
    {
        m_itemDescriptions->SetDisplayMessage(sUnlockedTickerMessages[mMenuItems.GetActiveItemIndex()]);
    }

    BaseSceneHandler* scene = nlSingleton<GameSceneManager>::Instance()->GetScene(SCENE_MARIO_BACKGROUND);
    if (scene->m_bVisible)
    {
        FEAudio::PlayAnimAudioEvent("sfx_main_menu_highlight_open", false);
    }
}

/**
 * Offset/Address/Size: 0x404 | 0x800A9E60 | size: 0x208
 */
void SHMainMenu::CloseItem(TLComponentInstance* compinstance)
{

    compinstance->SetActiveSlide(sSlideOut);
    compinstance->Update(0.0f);

    TLComponentInstance* highlight = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("high")));

    highlight->SetActiveSlide(sSlideOut);
    highlight->Update(0.0f);

    FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        highlight->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite")))
        ->SetAssetColour(mHighlightColour);

    BaseSceneHandler* scene = nlSingleton<GameSceneManager>::Instance()->GetScene(SCENE_MARIO_BACKGROUND);
    if (scene->m_bVisible)
    {
        Audio::gWorldSFX.Stop((Audio::eWorldSFX)0xC, cGameSFX::SFX_STOP_FIRST);
        FEAudio::PlayAnimAudioEvent("sfx_main_menu_highlight_close", false);
    }

    TLComponentInstance* lockedComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("locked")));

    if (lockedComp != NULL)
    {
        if (mMenuItems.GetMenuItem()->IsLocked())
        {
            lockedComp->m_bVisible = true;
        }
        else
        {
            lockedComp->m_bVisible = false;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800A9A5C | size: 0x404
 */
void SHMainMenu::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLSlide* slide = presentation->m_currentSlide;

    if (presentation->m_fadeDuration >= slide->m_start + slide->m_duration)
    {
        m_itemDescriptions->Update(fDeltaT);
    }
    else
    {
        return;
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        mMenuItems.NextItem();
        return;
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        mMenuItems.PreviousItem();
        return;
    }

    eFEINPUT_PAD padused;
    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, &padused))
    {
        if (mMenuItems.RunCallbackOnCurrent(ON_APPLY) == RES_OK)
        {
            GameInfoManager::Instance()->mMainUserPadNumber = padused;
            FEAudio::PlayAnimAudioEvent("sfx_accept", false);
            mLastMenuItem = mMenuItems.GetActiveItemIndex();
        }
        else
        {
            FEAudio::PlayAnimAudioEvent("sfx_deny", false);
        }

        return;
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        GameSceneManager::Instance()->PopEntireStack();
        GameSceneManager::Instance()->Push(SCENE_TITLE, SCREEN_BACK, false);
    }
}
