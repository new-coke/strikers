#include "Game/SH/SHSpoils.h"
#include "Game/GameSceneManager.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feInput.h"
#include "Game/SH/SHCupTrophy.h"
#include "Game/SH/SHMilestoneTrophy.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"

#include "NL/nlBind.h"

typedef Detail::MemFunImpl<void, void (SpoilsScene::*)(SpoilsScene::eSpoils)> MemFunImpl_Spoils_t;
typedef BindExp2<void, MemFunImpl_Spoils_t, SpoilsScene*, SpoilsScene::eSpoils> BindExp2_Spoils_t;
typedef Function1<void, TLComponentInstance*>::FunctorImpl<BindExp2_Spoils_t> FunctorImpl_Spoils_t;

s32 SpoilsScene::mLastSelectedIndex = 0;

typedef TLInstance* (*FindInstByValue)(TLSlide*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
typedef TLInstance* (*FindInstByRef)(TLSlide*, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&);

typedef TLComponentInstance* (*FindCompByValue)(TLSlide*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
typedef TLComponentInstance* (*FindCompByRef)(TLSlide*, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&);

/**
 * Offset/Address/Size: 0xACC | 0x800D19BC | size: 0xA4
 */
SpoilsScene::SpoilsScene()
{
}

/**
 * Offset/Address/Size: 0xA28 | 0x800D1918 | size: 0xA4
 */
SpoilsScene::~SpoilsScene()
{
}

/**
 * Offset/Address/Size: 0x6B4 | 0x800D15A4 | size: 0x374
 */
void SpoilsScene::Update(float dt)
{
    BaseSceneHandler::Update(dt);
    mButtons.CentreButtons();

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        mMenuItems.RunCallbackOnCurrent(ON_APPLY);
        mLastSelectedIndex = mMenuItems.GetActiveItemIndex();
        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
        return;
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, SCREEN_BACK, true);
        mLastSelectedIndex = 0;
        return;
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        mMenuItems.PreviousItem();
        return;
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        mMenuItems.NextItem();
    }
}

/**
 * Offset/Address/Size: 0xC4 | 0x800D0FB4 | size: 0x5F0
 */
void SpoilsScene::SceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    char menuname[64];
    MenuItem<TLComponentInstance>* item;

    for (int i = 0; i < SPOILS_NUM_CHOICES; i++)
    {
        nlSNPrintf(menuname, 64, "MENU ITEM%d", i + 2);

        TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuname)));
        TLComponentInstance* compinstance = (TLComponentInstance*)instance;

        compinstance->SetActiveSlide(i == SpoilsScene::mLastSelectedIndex ? DoubleHighlite::SLIDE_IN : DoubleHighlite::SLIDE_OUT);

        item = mMenuItems.AddItem(compinstance);

        {
            MenuItem<TLComponentInstance>::Callback callback1(DoubleHighlite::OpenItem);
            item->SetCallback(ON_HIGHLIGHT, callback1);
        }

        {
            MenuItem<TLComponentInstance>::Callback callback2(DoubleHighlite::CloseItem);
            item->SetCallback(ON_UNHIGHLIGHT, callback2);
        }

        {
            MenuItem<TLComponentInstance>::Callback callback0(Bind<void, MemFunImpl_Spoils_t, SpoilsScene*, SpoilsScene::eSpoils>(
                MemFun<SpoilsScene, void, SpoilsScene::eSpoils>(&SpoilsScene::ShowSpoils),
                this,
                (SpoilsScene::eSpoils)i));
            item->SetCallback(ON_APPLY, callback0);
        }

        FindComponent(compinstance->GetActiveSlide(), "highlite");

        if (i == SpoilsScene::mLastSelectedIndex)
        {
            DoubleHighlite::TempDisableSound();
            item->RunCallback(ON_HIGHLIGHT);
        }
        else
        {
            item->RunCallback(ON_UNHIGHLIGHT);
            TLSlide* slide = compinstance->GetActiveSlide();
            compinstance->Update(1.0f + (slide->m_start + slide->m_duration));
        }
    }

    mMenuItems.SetFlag(1);
    mMenuItems.SetActiveItemIndex(SpoilsScene::mLastSelectedIndex);
    mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);

    TLComponentInstance* buttonComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));
    mButtons.mButtonInstance = buttonComponent;

    mButtons.SetState(ButtonComponent::BS_A_AND_B);
}

/**
 * Offset/Address/Size: 0x0 | 0x800D0EF0 | size: 0xC4
 */
void SpoilsScene::ShowSpoils(SpoilsScene::eSpoils spoils)
{
    if (spoils == SPOILS_CUP)
    {
        CupTrophyScene* pScene = (CupTrophyScene*)GameSceneManager::GetInstance()->Push(SCENE_CUP_TROPHY, SCREEN_FORWARD, true);
        pScene->CreateTrophyScene(TROPHY_MUSHROOM_CUP, ButtonComponent::BS_B_ONLY, false);
    }
    else if (spoils == SPOILS_SUPER_CUP)
    {
        CupTrophyScene* pScene = (CupTrophyScene*)GameSceneManager::GetInstance()->Push(SCENE_CUP_TROPHY, SCREEN_FORWARD, true);
        pScene->CreateTrophyScene(TROPHY_SUPER_MUSHROOM_CUP, ButtonComponent::BS_B_ONLY, false);
    }
    else
    {
        MilestoneTrophyScene* pScene = (MilestoneTrophyScene*)GameSceneManager::GetInstance()->Push(SCENE_MILESTONE_TROPHY, SCREEN_FORWARD, true);
        pScene->CreateTrophyScene(NUM_BATTLE_TROPHIES, ButtonComponent::BS_B_ONLY, false);
    }
}
