#include "port/region.h"  // PORT: one binary, three discs
#include "Game/SH/SHOptions.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feMusic.h"
#include "Game/FE/feScene.h"
#include "Game/GameInfo.h"
#include "Game/SH/SHCredits.h"
#include "Game/SH/SHSaveLoad.h"
#include "Game/TrophyInfo.h"
#include "NL/glx/glxSwap.h"
#include "NL/nlLocalization.h"
#include "NL/nlPrint.h"

#include "NL/nlBind.h"


static const eMenuState MenuToMenuStateMap[] = {
    MS_AUDIO,
    MS_VISUAL,
    MS_GAMEPLAY,
    MS_CHEATS,
    MS_SAVE_LOAD,
    MS_NUMMENUSTATES,
};

s32 OptionsScene::mLastSelectedIndex;
u32 OptionsScene::mUserInfoCRC;

/**
 * Offset/Address/Size: 0x1460 | 0x800B4A1C | size: 0x8C
 */
static void ApplyChangesCB()
{
    OptionsScene* scene = (OptionsScene*)nlSingleton<GameSceneManager>::Instance()->GetScene(SCENE_OPTIONS);

    if (scene->m_curMenuState == MS_AUDIO)
    {
        OptionsAudioMenuV2* subMenuBytes = (OptionsAudioMenuV2*)scene->m_subMenu;
        if (subMenuBytes->mbUpdateMode)
        {
            FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
            popup->Create(POPUP_APPLYING_AUDIO);
            scene->mPopupResult = PR_APPLY_DELAYED_AUDIO_CHANGES;
            return;
        }
    }

    scene->mPopupResult = PR_APPLY_CHANGES;
}

/**
 * Offset/Address/Size: 0x1430 | 0x800B49EC | size: 0x30
 */
static void RevertChangesCB()
{
    OptionsScene* scene = (OptionsScene*)nlSingleton<GameSceneManager>::Instance()->GetScene(SCENE_OPTIONS);
    scene->mPopupResult = PR_REVERT_CHANGES;
}

/**
 * Offset/Address/Size: 0x1304 | 0x800B48C0 | size: 0x12C
 */
OptionsScene::OptionsScene()
    : BaseSceneHandler()
    , m_subMenu(NULL)
    , m_curMenuState(MS_INVALID)
    , mButtons()
    , mMenuItems()
    , mPopupResult(PR_DO_NOTHING)
{
    eMenuState menuState = MenuToMenuStateMap[mLastSelectedIndex];

    if (menuState != MS_NUMMENUSTATES)
    {
        if (menuState == MS_SAVE_LOAD)
        {
            if ((SaveLoadScene::mLastSaveLoadSuccess != 0) && (DidContinueWithoutOperation() == false))
            {
                OptionsScene::mUserInfoCRC = nlChecksum32(&(nlSingleton<GameInfoManager>::Instance()->mUserInfo), sizeof(UserInfo));
            }
        }
        else if (mLastSelectedIndex == 0)
        {
            OptionsScene::mUserInfoCRC = nlChecksum32(&(nlSingleton<GameInfoManager>::Instance()->mUserInfo), sizeof(UserInfo));
        }
    }

    SaveLoadScene::mLastSaveLoadSuccess = 0;
}

/**
 * Offset/Address/Size: 0x123C | 0x800B47F8 | size: 0xC8
 */
OptionsScene::~OptionsScene()
{
    if (m_subMenu != NULL)
    {
        delete m_subMenu;
    }
}

/**
 * Offset/Address/Size: 0xB5C | 0x800B4118 | size: 0x6E0
 */
void OptionsScene::SceneCreated()
{
    FEAudio::EnableSounds(false);

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    for (int i = 0; i < 6; i++)
    {
        char menuname[64];
        nlSNPrintf(menuname, sizeof(menuname), "MENU ITEM%d", i + 1);

        TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuname)));
        TLComponentInstance* compinstance = (TLComponentInstance*)instance;

        if (MenuToMenuStateMap[i] == MS_NUMMENUSTATES)
        {
            if (!nlSingleton<GameInfoManager>::Instance()->HasTrophy(TROPHY_BOWSER_CUP))
            {
                compinstance->m_bVisible = false;
                continue;
            }
        }

        compinstance->SetActiveSlide(i == mLastSelectedIndex ? DoubleHighlite::SLIDE_IN : DoubleHighlite::SLIDE_OUT);

        MenuItem<TLComponentInstance>* item = mMenuItems.AddItem(compinstance);

        {
            MenuItem<TLComponentInstance>::Callback openFunc(
                Bind<void>(MemFun<OptionsScene, void, TLComponentInstance*>(&OptionsScene::OpenItem), this, placeholder0));
            item->SetCallback(ON_HIGHLIGHT, openFunc);
        }

        {
            MenuItem<TLComponentInstance>::Callback closeFunc(
                Bind<void>(MemFun<OptionsScene, void, TLComponentInstance*>(&OptionsScene::CloseItem), this, placeholder0));
            item->SetCallback(ON_UNHIGHLIGHT, closeFunc);
        }

        {
            MenuItem<TLComponentInstance>::Callback callback(
                Bind<void>(MemFun(&OptionsScene::ChangeMenuState), this, MenuToMenuStateMap[i]));
            item->SetCallback(ON_APPLY, callback);
        }

        FindComponent(compinstance->GetActiveSlide(), "highlite");

        if (i == 3 && port_region() == PORT_REGION_JAPAN)
        {
            if (AllCheatsLocked())
            {
                item->SetLockedFlag(true);
            }
        }

        if (i == mLastSelectedIndex)
        {
            item->RunCallback(ON_HIGHLIGHT);
        }
        else
        {
            item->RunCallback(ON_UNHIGHLIGHT);

            TLSlide* slide = compinstance->GetActiveSlide();
            compinstance->Update(1.0f + (slide->m_start + slide->m_duration));
        }

        if (i == 3 && port_region() == PORT_REGION_JAPAN)
        {
            TLComponentInstance* lockedComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                compinstance->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("locked")));

            if (lockedComp != NULL)
            {
                lockedComp->m_bVisible = item->IsLocked();
            }
        }
    }

    mMenuItems.SetFlag(1);
    mMenuItems.SetActiveItemIndex(mLastSelectedIndex);
    mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);

    ChangeMenuState(MS_MAIN);

    TLComponentInstance* buttonComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));
    mButtons.mButtonInstance = buttonComponent;

    mButtons.SetState(ButtonComponent::BS_A_AND_B);

    FEAudio::EnableSounds(true);
}

/**
 * Offset/Address/Size: 0xAF4 | 0x800B40B0 | size: 0x68
 */
void OptionsScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    if (m_curMenuState == MS_MAIN)
    {
        UpdateForMain(fDeltaT);
    }
    else
    {
        UpdateForSubOptionMenus(fDeltaT);
    }
}

/**
 * Offset/Address/Size: 0x6EC | 0x800B3CA8 | size: 0x408
 */
void OptionsScene::UpdateForMain(float fDeltaT)
{
    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
        FEAudio::PlayAnimAudioEvent("sfx_screen_forward", false);

        mMenuItems.RunCallbackOnCurrent(ON_APPLY);
        mLastSelectedIndex = mMenuItems.GetActiveItemIndex();
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        nlSingleton<GameSceneManager>::Instance()->PopEntireStack();

        if (SaveLoadScene::IsIOEnabled())
        {
            unsigned long currentcrc = nlChecksum32(&(nlSingleton<GameInfoManager>::Instance()->mUserInfo), sizeof(UserInfo));

            if (currentcrc != OptionsScene::mUserInfoCRC)
            {
                SaveLoadScene* scene = (SaveLoadScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SAVE, SCREEN_NOTHING, false);
                scene->mNextScene = SCENE_MAIN_MENU;
            }
            else
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, SCREEN_BACK, false);
            }
        }
        else
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, SCREEN_BACK, false);
        }

        mLastSelectedIndex = 0;
        FEAudio::PlayAnimAudioEvent("sfx_back", false);
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        mMenuItems.PreviousItem();
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        mMenuItems.NextItem();
    }
}

/**
 * Offset/Address/Size: 0x268 | 0x800B3824 | size: 0x484
 */
void OptionsScene::UpdateForSubOptionMenus(float fDeltaT)
{
    static bool gStartLoadingBar = false;

    if (mPopupResult != PR_DO_NOTHING)
    {
        if (mPopupResult == PR_APPLY_CHANGES)
        {
            if (gStartLoadingBar)
            {
                glxSwapLoading(true, false);
            }
            m_subMenu->Save();
            if (gStartLoadingBar)
            {
                glxSwapLoading(false, false);
            }
            gStartLoadingBar = false;
        }
        else if (mPopupResult == PR_REVERT_CHANGES)
        {
            m_subMenu->Revert();
        }
        else if (mPopupResult == PR_APPLY_DELAYED_AUDIO_CHANGES)
        {
            FEPopupMenu* scene = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->GetCurrentScene();

            if (!scene->m_pFEScene->m_bValid)
            {
                return;
            }

            f32 endTime = scene->m_pFEPresentation->m_currentSlide->m_start + scene->m_pFEPresentation->m_currentSlide->m_duration;
            f32 curTime = scene->m_pFEPresentation->m_currentSlide->m_time;

            static float HACK_DELAY_UNTIL_APPLY = 0.0f;

            if (curTime != endTime)
            {
                scene->m_pFEPresentation->m_fadeDuration = 999.9f;
                scene->m_pFEPresentation->Update(0.0f);
                HACK_DELAY_UNTIL_APPLY = 0.0f;
                return;
            }

            HACK_DELAY_UNTIL_APPLY += fDeltaT;
            if (HACK_DELAY_UNTIL_APPLY >= 0.5f)
            {
                mPopupResult = PR_APPLY_CHANGES;
                nlSingleton<GameSceneManager>::Instance()->Pop();
                gStartLoadingBar = true;
            }
            return;
        }

        m_subMenu->GoBack();
        ChangeMenuState(MS_MAIN);

        FEAudio::PlayAnimAudioEvent((mPopupResult == PR_APPLY_CHANGES) ? "sfx_accept" : "sfx_back", false);

        mPopupResult = PR_DO_NOTHING;
        return;
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        if (m_subMenu->ChangesMade())
        {
            FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
            popup->Create(POPUP_REVERT_OPTION_CHANGES, ApplyChangesCB, RevertChangesCB);
        }
        else
        {
            m_subMenu->GoBack();
            ChangeMenuState(MS_MAIN);
            FEAudio::PlayAnimAudioEvent("sfx_back", false);
            FEAudio::PlayAnimAudioEvent("sfx_screen_back", false);
            return;
        }
    }

    m_subMenu->Update(fDeltaT);
}

/**
 * Offset/Address/Size: 0x0 | 0x800B35BC | size: 0x268
 */
void OptionsScene::ChangeMenuState(eMenuState newState)
{
    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();

    if (m_subMenu != NULL)
    {
        delete m_subMenu;
        m_subMenu = NULL;
    }

    switch (newState)
    {
    case MS_MAIN:
    {
        mMenuItems.SetActiveItemIndex(mLastSelectedIndex);
        mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
        break;
    }
    case MS_AUDIO:
        m_subMenu = new (nlMalloc(sizeof(OptionsAudioMenuV2), 8, false))
            OptionsAudioMenuV2(pres, ButtonComponent::BS_B_ONLY, nlSingleton<GameInfoManager>::Instance()->mUserInfo.mAudioOptions);
        break;
    case MS_VISUAL:
        m_subMenu = new (nlMalloc(sizeof(OptionsVisualMenuV2), 8, false))
            OptionsVisualMenuV2(pres, ButtonComponent::BS_B_ONLY, nlSingleton<GameInfoManager>::Instance()->mUserInfo.mVisualOptions);
        break;
    case MS_GAMEPLAY:
    {
        bool showlegend = nlSingleton<GameInfoManager>::Instance()->IsLegendSkillUnlocked();
        m_subMenu = new (nlMalloc(sizeof(OptionsGameplayMenuV2), 8, false))
            OptionsGameplayMenuV2(pres,
                ButtonComponent::BS_B_ONLY,
                nlSingleton<GameInfoManager>::Instance()->mUserInfo.mGameplayOptions,
                !showlegend ? 4 : -1);
        break;
    }
    case MS_CHEATS:
        if (port_region() == PORT_REGION_JAPAN && AllCheatsLocked())
        {
            FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_FORWARD, false);
            popup->Create(POPUP_NO_CHEATS_UNLOCKED);
            newState = MS_MAIN;
            break;
        }
        m_subMenu = new (nlMalloc(sizeof(OptionsCheatsMenu), 8, false))
            OptionsCheatsMenu(pres, ButtonComponent::BS_B_ONLY, nlSingleton<GameInfoManager>::Instance()->mUserInfo.mCheatOptions);
        break;
    case MS_SAVE_LOAD:
        m_subMenu = new (nlMalloc(sizeof(OptionsSaveLoad), 8, false))
            OptionsSaveLoad(pres, ButtonComponent::BS_A_AND_B);
        break;
    case MS_NUMMENUSTATES:
        nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CREDITS, SCREEN_NOTHING, false);
        CreditScene::mNextScene = SCENE_OPTIONS;
        FEMusic::StopStream();
        break;
    default:
        break;
    }

    m_curMenuState = newState;
}

bool OptionsScene::AllCheatsLocked()
{
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::Instance();

    if (gameInfo->IsCustomExplosiveUnlocked())
    {
        return false;
    }
    if (gameInfo->IsCustomFreezingUnlocked())
    {
        return false;
    }
    if (gameInfo->IsCustomShellsUnlocked())
    {
        return false;
    }
    if (gameInfo->IsCustomGiantUnlocked())
    {
        return false;
    }
    if (gameInfo->IsCustomEnhanceUnlocked())
    {
        return false;
    }
    if (gameInfo->IsGlassJawGoalieUnlocked())
    {
        return false;
    }
    if (gameInfo->IsUnlimtedPowerupsUnlocked())
    {
        return false;
    }
    if (gameInfo->IsTiltCheatUnlocked())
    {
        return false;
    }
    if (gameInfo->IsAllSTSCheatUnlocked())
    {
        return false;
    }

    return true;
}

void OptionsScene::OpenItem(TLComponentInstance* compinstance)
{
    DoubleHighlite::OpenItem(compinstance);

    if (mMenuItems.GetMenuItem()->IsLocked())
    {
        TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            compinstance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("pauseresume")));

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
    }

    TLComponentInstance* lockedComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("locked")));

    if (lockedComp != NULL)
    {
        u8 locked = mMenuItems.GetMenuItem()->IsLocked();
        lockedComp->m_bVisible = (bool)locked;
    }
}

void OptionsScene::CloseItem(TLComponentInstance* compinstance)
{
    DoubleHighlite::CloseItem(compinstance);

    TLComponentInstance* lockedComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("locked")));

    if (lockedComp != NULL)
    {
        u8 locked = mMenuItems.GetMenuItem()->IsLocked();
        lockedComp->m_bVisible = (bool)locked;
    }
}
