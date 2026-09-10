#include "Game/FE/feOptionsSubMenus.h"

#include "NL/platpad.h"
#include "NL/nlTask.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/FE/feSlideMenu.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/feInput.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/SH/SHSaveLoad.h"
#include "Game/GameSceneManager.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/tlImageInstance.h"
#include "types.h"

typedef void FnTLComponentInstanceCb(TLComponentInstance*);

extern nlColour MenuHighliteColour;
extern nlColour SubMenuHighliteColour;
extern nlColour SubMenuUnhighliteColour;

static const char* MAIN_MENU_SLIDE = "Slide1";
static const char* AUDIO_MENU_SLIDE = "Slide2";
static const char* GAMEPLAY_MENU_SLIDE = "Slide3";
static const char* SAVE_LOAD_SLIDE = "Slide4";
static const char* CHEATS_MENU_SLIDE = "Slide5";
static const char* VISUAL_MENU_SLIDE = "Slide6";

/**
 * Offset/Address/Size: 0x5FD4 | 0x800BB018 | size: 0xDC
 */
OptionsSubMenu::~OptionsSubMenu()
{
    for (int i = 0; i < 8; i++)
    {
        if (mSlideMenuLists[i] != NULL)
        {
            delete mSlideMenuLists[i];
        }
    }
}

/**
 * Offset/Address/Size: 0x55A4 | 0x800BA5E8 | size: 0xA30
 */
void OptionsSubMenu::Update(float dt)
{
    mButtons.CentreButtons();

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        bool locked;
        int menuIndex = mMenuItems.GetActiveItemIndex();
        if (mSlideMenuLists[menuIndex] != NULL
            && ((SlideMenuList*)mSlideMenuLists[menuIndex])->GetMenuItem()->IsLocked())
        {
            locked = true;
        }
        else
        {
            locked = false;
        }

        if (!locked)
        {
            ColourAllText(SubMenuUnhighliteColour, mMenuItems.GetActiveItemIndex());
        }

        mMenuItems.PreviousItem();

        menuIndex = mMenuItems.GetActiveItemIndex();
        if (mSlideMenuLists[menuIndex] != NULL
            && ((SlideMenuList*)mSlideMenuLists[menuIndex])->GetMenuItem()->IsLocked())
        {
            locked = true;
        }
        else
        {
            locked = false;
        }

        if (!locked)
        {
            ColourAllText(SubMenuHighliteColour, mMenuItems.GetActiveItemIndex());
        }

        return;
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        bool locked;
        int menuIndex = mMenuItems.GetActiveItemIndex();
        if (mSlideMenuLists[menuIndex] != NULL
            && ((SlideMenuList*)mSlideMenuLists[menuIndex])->GetMenuItem()->IsLocked())
        {
            locked = true;
        }
        else
        {
            locked = false;
        }

        if (!locked)
        {
            ColourAllText(SubMenuUnhighliteColour, mMenuItems.GetActiveItemIndex());
        }

        mMenuItems.NextItem();

        menuIndex = mMenuItems.GetActiveItemIndex();
        if (mSlideMenuLists[menuIndex] != NULL
            && ((SlideMenuList*)mSlideMenuLists[menuIndex])->GetMenuItem()->IsLocked())
        {
            locked = true;
        }
        else
        {
            locked = false;
        }

        if (!locked)
        {
            ColourAllText(SubMenuHighliteColour, mMenuItems.GetActiveItemIndex());
        }

        return;
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xB, true, NULL))
    {
        if (mSlideMenuLists[mMenuItems.GetActiveItemIndex()] != NULL)
        {
            MenuResult res = mSlideMenuLists[mMenuItems.GetActiveItemIndex()]->PreviousItem();

            if (res == RES_OK)
            {
                FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
                Save();
            }
            else if (res == RES_NOT_CHANGED)
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }
        }

        return;
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xC, true, NULL))
    {
        if (mSlideMenuLists[mMenuItems.GetActiveItemIndex()] != NULL)
        {
            MenuResult res = mSlideMenuLists[mMenuItems.GetActiveItemIndex()]->NextItem();

            if (res == RES_OK)
            {
                FEAudio::PlayAnimAudioEvent("sfx_option_scroll_right", false);
                Save();
            }
            else if (res == RES_NOT_CHANGED)
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
                Save();
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x5480 | 0x800BA4C4 | size: 0x124
 */
void OptionsSubMenu::GoBack()
{
    ColourAllText(SubMenuUnhighliteColour, mMenuItems.GetActiveItemIndex());

    m_pres->SetActiveSlide(MAIN_MENU_SLIDE);
    m_pres->Update(0.0f);
    SetButtonState(ButtonComponent::BS_A_AND_B);
}

/**
 * Offset/Address/Size: 0x504C | 0x800BA090 | size: 0x434
 */
void OptionsSubMenu::BuildSubMenuList(int menuitem, TLComponentInstance* compinstance, bool wraps, int startindex)
{
    extern int nlSNPrintf(char*, unsigned long, const char*, ...);

    SlideMenuList* list = new (nlMalloc(sizeof(SlideMenuList), 8, false)) SlideMenuList(compinstance);
    mSlideMenuLists[menuitem] = list;

    MenuItem<SlideMenuItem>* menuItem;
    char slidename[64] = { 0 };

    int slidenum = 0;
    do
    {
        nlSNPrintf(slidename, 64, "Slide%d", slidenum + 1);
        compinstance->SetActiveSlide(slidename);

        if (compinstance->GetActiveSlide() == NULL)
        {
            break;
        }

        unsigned long slideHash = compinstance->GetActiveSlide()->m_hash;

        SlideMenuList* sml = (SlideMenuList*)mSlideMenuLists[menuitem];

        menuItem = sml->AddItem(slideHash, slidenum);
    } while (++slidenum);

    list = (SlideMenuList*)mSlideMenuLists[menuitem];
    list->SetItem(startindex);

    if (wraps)
    {
        ((SlideMenuList*)mSlideMenuLists[menuitem])->SetFlag(1);
    }
}

/**
 * Offset/Address/Size: 0x4F84 | 0x800B9FC8 | size: 0xC8
 */
void OptionsSubMenu::SetButtonState(ButtonComponent::ButtonState buttonState)
{
    m_buttons = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pres->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtons.mButtonInstance = m_buttons;
    mButtons.SetState(buttonState);
    m_currentButtonState = buttonState;

    SetAButtonLOC(0x527F4084);
}

/**
 * Offset/Address/Size: 0x4ECC | 0x800B9F10 | size: 0xB8
 */
void OptionsSubMenu::SetAButtonLOC(unsigned long locStrId)
{
    if (m_buttons == NULL)
    {
        return;
    }

    TLTextInstance* textInstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        m_buttons->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("accept")));

    if (textInstance != NULL)
    {
        textInstance->m_LocStrId = locStrId;
        textInstance->m_OverloadFlags |= 0x8u;
    }
}

/**
 * Offset/Address/Size: 0x47AC | 0x800B97F0 | size: 0x720
 */
OptionsCheatsMenu::OptionsCheatsMenu(FEPresentation* pres, ButtonComponent::ButtonState btnState, CheatSettings& settings)
    : OptionsSubMenu(pres, btnState)
    , mSettings(settings)
{
    extern int nlSNPrintf(char*, unsigned long, const char*, ...);

    char menuname[64];
    int i;
    TLInstance* instance;
    TLComponentInstance* compinstance;

    pres->SetActiveSlide(CHEATS_MENU_SLIDE);
    pres->Update(0.0f);

    SetButtonState(btnState);
    if (btnState == ButtonComponent::BS_A_AND_B)
    {
        SetAButtonLOC(0x9C81A82F);
    }

    TLSlide* currentSlide = pres->GetActiveSlide();

    for (i = 0; i < 5; i++)
    {
        nlSNPrintf(menuname, 64, "MENU ITEM%d", i + 1);

        instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuname)));

        MenuItem<TLComponentInstance>* menuItem = mMenuItems.AddItem((TLComponentInstance*)instance);

        menuItem->SetCallback(ON_HIGHLIGHT, SingleHighlite::OpenItem);

        menuItem->SetCallback(ON_UNHIGHLIGHT, SingleHighlite::CloseItem);

        if (i == 0)
        {
            SingleHighlite::TempDisableSound();
        }

        menuItem->RunCallback((i == 0) ? ON_HIGHLIGHT : ON_UNHIGHLIGHT);

        mSlideMenuLists[i] = NULL;
    }

    mMenuItems.SetFlag(1);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CHOICES")));
    BuildCustomPowerupsList(compinstance, mSettings.mCustomPowerups, pres);

    GameInfoManager* gm = nlSingleton<GameInfoManager>::Instance();

    bool stunnedGoalies;
    TLComponentInstance* comp3 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ON/OFF3")));
    stunnedGoalies = mSettings.mStunnedGoalies;
    BuildLockableSubMenuList(1, comp3, pres, gm->IsGlassJawGoalieUnlocked(), stunnedGoalies);

    bool infinitePowerups;
    TLComponentInstance* comp4 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ON/OFF4")));
    infinitePowerups = mSettings.mInfinitePowerups;
    BuildLockableSubMenuList(2, comp4, pres, gm->IsUnlimtedPowerupsUnlocked(), infinitePowerups);

    bool cheatTBD1;
    TLComponentInstance* comp5 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ON/OFF5")));
    cheatTBD1 = mSettings.mCheatTBD1Enabled;
    BuildLockableSubMenuList(3, comp5, pres, gm->IsTiltCheatUnlocked(), cheatTBD1);

    bool cheatTBD2;
    TLComponentInstance* comp6 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ON/OFF6")));
    cheatTBD2 = mSettings.mCheatTBD2Enabled;
    BuildLockableSubMenuList(4, comp6, pres, gm->IsAllSTSCheatUnlocked(), cheatTBD2);

    memcpy(&mBackupSettings, &mSettings, sizeof(CheatSettings));
    mSettingsCRC = nlChecksum32(&mBackupSettings, sizeof(CheatSettings));
}

/**
 * Offset/Address/Size: 0x46C0 | 0x800B9704 | size: 0xEC
 */
OptionsCheatsMenu::~OptionsCheatsMenu()
{
}

/**
 * Offset/Address/Size: 0x4550 | 0x800B9594 | size: 0x170
 */
void OptionsCheatsMenu::Save()
{
    CheatSettings localSettings;
    SlideMenuList* list;
    int val;

    list = (SlideMenuList*)mSlideMenuLists[0];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.mCustomPowerups = (CustomPowerups)val;

    list = (SlideMenuList*)mSlideMenuLists[1];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.mStunnedGoalies = (val != 0);

    list = (SlideMenuList*)mSlideMenuLists[2];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.mInfinitePowerups = (val != 0);

    list = (SlideMenuList*)mSlideMenuLists[3];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.mCheatTBD1Enabled = (val != 0);

    list = (SlideMenuList*)mSlideMenuLists[4];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.mCheatTBD2Enabled = (val != 0);

    mSettings = localSettings;
    mSettings.OnSettingsUpdated();
}

/**
 * Offset/Address/Size: 0x4510 | 0x800B9554 | size: 0x40
 */
void OptionsCheatsMenu::Revert()
{
    memcpy(&mSettings, &mBackupSettings, sizeof(CheatSettings));
    mSettings.OnSettingsUpdated();
}

/**
 * Offset/Address/Size: 0x3D3C | 0x800B8D80 | size: 0x7D4
 */
void OptionsCheatsMenu::BuildLockableSubMenuList(int menuitem, TLComponentInstance* compinstance, FEPresentation* presentation, bool unlocked, int startindex)
{
    extern int nlSNPrintf(char*, unsigned long, const char*, ...);
    nlColour lockColour;

    SlideMenuList* list = new (nlMalloc(sizeof(SlideMenuList), 8, false)) SlideMenuList(compinstance);
    mSlideMenuLists[menuitem] = list;

    compinstance->SetActiveSlide("Slide2");

    unsigned long hash = compinstance->GetActiveSlide()->m_hash;
    SlideMenuList* sml = (SlideMenuList*)mSlideMenuLists[menuitem];
    MenuItem<SlideMenuItem>* menuItem = sml->AddItem(hash, 0);

    bool wraps = true;
    if (!unlocked)
    {
        startindex = 0;
        wraps = false;

        TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            compinstance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("OFF")));

        pText->SetStringId("CHEAT_LOCKED");

        lockColour.c[0] = 0xFE;
        lockColour.c[1] = 0xEE;
        lockColour.c[2] = 0x00;
        lockColour.c[3] = 0xFF;
        pText->SetAssetColour(lockColour);

        char slidename[64] = { 0 };
        nlSNPrintf(slidename, 64, "MENU ITEM%d", menuitem + 1);

        TLComponentInstance* pMenuComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(slidename)));

        pMenuComp->SetActiveSlide("IN");

        TLComponentInstance* pArrowComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pMenuComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("ARROWS")));

        pArrowComp->m_bVisible = false;
        pMenuComp->SetActiveSlide("OUT");

        ((SlideMenuList*)mSlideMenuLists[menuitem])->GetMenuItem()->SetLockedFlag(true);
    }
    else
    {
        compinstance->SetActiveSlide("Slide1");

        hash = compinstance->GetActiveSlide()->m_hash;
        sml = (SlideMenuList*)mSlideMenuLists[menuitem];
        menuItem = sml->AddItem(hash, 1);
    }

    {
        SlideMenuList* s = (SlideMenuList*)mSlideMenuLists[menuitem];
        s->SetItem(startindex);

        if (wraps)
        {
            ((SlideMenuList*)mSlideMenuLists[menuitem])->SetFlag(1);
        }
    }
}

/**
 * Offset/Address/Size: 0x34B0 | 0x800B84F4 | size: 0x88C
 */
void OptionsCheatsMenu::BuildCustomPowerupsList(TLComponentInstance* compinstance, CustomPowerups startOption, FEPresentation* presentation)
{
    extern int nlSNPrintf(char*, unsigned long, const char*, ...);
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;
    int slidesAdded = 1;
    int startindex = 0;
    char slidename[64];
    int i;
    TLTextInstance* pText;
    nlColour lockColour;
    TLComponentInstance* pMenuComp;
    TLComponentInstance* pArrowComp;
    MenuItem<SlideMenuItem>* menuItem;
    unsigned long slideHash;
    bool unlocked;
    SlideMenuList* sml;

    mSlideMenuLists[0] = new (nlMalloc(sizeof(SlideMenuList), 8, false)) SlideMenuList(compinstance);

    compinstance->SetActiveSlide("Slide1");
    {
        slideHash = compinstance->GetActiveSlide()->m_hash;
        sml = (SlideMenuList*)mSlideMenuLists[0];
        menuItem = sml->AddItem(slideHash, 0);
    }

    for (i = 1; i < 6; i++)
    {
        switch (i)
        {
        case 1:
            unlocked = gameInfo->IsCustomExplosiveUnlocked();
            break;
        case 2:
            unlocked = gameInfo->IsCustomFreezingUnlocked();
            break;
        case 3:
            unlocked = gameInfo->IsCustomShellsUnlocked();
            break;
        case 4:
            unlocked = gameInfo->IsCustomGiantUnlocked();
            break;
        case 5:
            unlocked = gameInfo->IsCustomEnhanceUnlocked();
            break;
        }

        if (unlocked)
        {
            nlSNPrintf(slidename, 64, "Slide%d", i + 1);
            compinstance->SetActiveSlide(slidename);

            slideHash = compinstance->GetActiveSlide()->m_hash;
            sml = (SlideMenuList*)mSlideMenuLists[0];
            menuItem = sml->AddItem(slideHash, i);

            if (startOption == i)
            {
                startindex = slidesAdded;
            }

            slidesAdded++;
        }
    }

    {
        SlideMenuList* sml = (SlideMenuList*)mSlideMenuLists[0];
        sml->SetItem(startindex);

        if (slidesAdded > 1)
        {
            ((SlideMenuList*)mSlideMenuLists[0])->SetFlag(1);
            ColourAllText(SubMenuHighliteColour, 0);
        }
        else
        {
            pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                compinstance->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("EXPLOSIVE")));

            pText->SetStringId("CHEAT_LOCKED");

            lockColour.c[0] = 0xFE;
            lockColour.c[1] = 0xEE;
            lockColour.c[2] = 0x00;
            lockColour.c[3] = 0xFF;
            pText->SetAssetColour(lockColour);

            pMenuComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                presentation->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("Layer")),
                InlineHasher(nlStringLowerHash("MENU ITEM1")));

            pArrowComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                pMenuComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("ARROWS")));

            pArrowComp->m_bVisible = false;

            ((SlideMenuList*)mSlideMenuLists[0])->GetMenuItem()->SetLockedFlag(true);
        }
    }
}

/**
 * Offset/Address/Size: 0x2D44 | 0x800B7D88 | size: 0x76C
 */
OptionsAudioMenuV2::OptionsAudioMenuV2(FEPresentation* presentation, ButtonComponent::ButtonState buttonstate, AudioSettings& settings)
    : OptionsSubMenu(presentation, buttonstate)
    , mSettings(settings)
{
    extern int nlSNPrintf(char*, unsigned long, const char*, ...);

    bool inpausestate;
    char menuname[64];
    TLInstance* instance;
    TLComponentInstance* compinstance;

    mbUpdateMode = false;

    inpausestate = (nlTaskManager::m_pInstance->m_CurrState == 1);
    if (inpausestate)
    {
        presentation->SetActiveSlide("Slide1");
    }
    else
    {
        presentation->SetActiveSlide(AUDIO_MENU_SLIDE);
    }

    presentation->Update(0.0f);

    SetButtonState(buttonstate);
    if (buttonstate == ButtonComponent::BS_A_AND_B)
    {
        SetAButtonLOC(0x9C81A82F);
    }

    TLSlide* currentSlide = presentation->GetActiveSlide();

    for (int i = 0; i < 4; i++)
    {
        nlSNPrintf(menuname, 64, "MENU ITEM%d", i + 1);

        instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuname)));

        MenuItem<TLComponentInstance>* menuItem = mMenuItems.AddItem((TLComponentInstance*)instance);

        menuItem->SetCallback(ON_HIGHLIGHT, SingleHighlite::OpenItem);

        menuItem->SetCallback(ON_UNHIGHLIGHT, SingleHighlite::CloseItem);

        if (i == 0)
        {
            SingleHighlite::TempDisableSound();
            menuItem->RunCallback(ON_HIGHLIGHT);
            menuItem->SetDisabledFlag(false);
        }
        else
        {
            menuItem->RunCallback(ON_UNHIGHLIGHT);
        }

        if (inpausestate && i == 3)
        {
            menuItem->SetDisabledFlag(true);
            menuItem->GetType()->m_bVisible = false;
        }
    }

    mMenuItems.SetFlag(3);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("volume1")));
    BuildSubMenuList(0, compinstance, false, mSettings.MusicVolume);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("volume2")));
    BuildSubMenuList(1, compinstance, false, mSettings.SFXVolume);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("volume3")));
    BuildSubMenuList(2, compinstance, false, mSettings.VoiceVolume);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("mode")));
    BuildSubMenuList(3, compinstance, true, mSettings.Mode);
    ((SlideMenuList*)mSlideMenuLists[3])->SetFlag(1);

    if (inpausestate)
    {
        compinstance->m_bVisible = false;
    }

    ColourAllText(SubMenuHighliteColour, mMenuItems.GetActiveItemIndex());

    memcpy(&mBackupSettings, &mSettings, sizeof(AudioSettings));
    mSettingsCRC = nlChecksum32(&mBackupSettings, sizeof(AudioSettings));
}

/**
 * Offset/Address/Size: 0x2C58 | 0x800B7C9C | size: 0xEC
 */
OptionsAudioMenuV2::~OptionsAudioMenuV2()
{
}

/**
 * Offset/Address/Size: 0x2B48 | 0x800B7B8C | size: 0x110
 */
void OptionsAudioMenuV2::Save()
{
    SlideMenuList* list;
    int val;

    list = (SlideMenuList*)mSlideMenuLists[0];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    mSettings.MusicVolume = val;

    list = (SlideMenuList*)mSlideMenuLists[1];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    mSettings.SFXVolume = val;

    list = (SlideMenuList*)mSlideMenuLists[2];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    mSettings.VoiceVolume = val;

    list = (SlideMenuList*)mSlideMenuLists[3];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    mSettings.Mode = (eAudioMode)val;

    mSettings.ApplySettings(mbUpdateMode, false);
    if (mbUpdateMode)
    {
        AudioLoader::PlayFEMenuMusic();
        mbUpdateMode = false;
    }
}

/**
 * Offset/Address/Size: 0x2B04 | 0x800B7B48 | size: 0x44
 */
void OptionsAudioMenuV2::Revert()
{
    memcpy(&mSettings, &mBackupSettings, sizeof(AudioSettings));
    mSettings.ForceApplySettings(false);
}

/**
 * Offset/Address/Size: 0x20C8 | 0x800B710C | size: 0xA3C
 */
void OptionsAudioMenuV2::Update(float dt)
{
    bool ischaractervol = (mMenuItems.GetActiveItemIndex() == 2);
    mButtons.CentreButtons();

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        ColourAllText(SubMenuUnhighliteColour, mMenuItems.GetActiveItemIndex());

        mMenuItems.PreviousItem();

        ColourAllText(SubMenuHighliteColour, mMenuItems.GetActiveItemIndex());
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        ColourAllText(SubMenuUnhighliteColour, mMenuItems.GetActiveItemIndex());

        mMenuItems.NextItem();

        ColourAllText(SubMenuHighliteColour, mMenuItems.GetActiveItemIndex());
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xB, true, NULL))
    {
        SlideMenuList* slideMenuList = (SlideMenuList*)mSlideMenuLists[mMenuItems.GetActiveItemIndex()];
        if (slideMenuList != NULL)
        {
            MenuResult res = slideMenuList->PreviousItem();

            if (res == RES_OK)
            {
                if (ischaractervol)
                {
                    FEAudio::PlayRandomVoiceToggleSFX();
                }
                else
                {
                    FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
                }

                if (mMenuItems.GetActiveItemIndex() != 3)
                {
                    bool bModeChanged = mbUpdateMode;
                    mbUpdateMode = false;
                    Save();
                    mbUpdateMode = bModeChanged;
                }
                else
                {
                    int mode;
                    SlideMenuList* list = (SlideMenuList*)mSlideMenuLists[3];
                    if (list != NULL)
                    {
                        mode = list->GetMenuItem()->GetType()->GetUserEnumType();
                    }
                    else
                    {
                        mode = -1;
                    }

                    mSettings.Mode = (eAudioMode)mode;
                    if (mSettings.Mode != mSettings.DefaultMode)
                    {
                        mbUpdateMode = true;
                    }
                    else
                    {
                        mbUpdateMode = false;
                    }
                }
            }
            else if (res == RES_NOT_CHANGED)
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }
        }
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xC, true, NULL))
    {
        if (mSlideMenuLists[mMenuItems.GetActiveItemIndex()] != NULL)
        {
            MenuResult res = mSlideMenuLists[mMenuItems.GetActiveItemIndex()]->NextItem();

            if (res == RES_OK)
            {
                if (ischaractervol)
                {
                    FEAudio::PlayRandomVoiceToggleSFX();
                }
                else
                {
                    FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
                }

                if (mMenuItems.GetActiveItemIndex() != 3)
                {
                    bool bModeChanged = mbUpdateMode;
                    mbUpdateMode = false;
                    Save();
                    mbUpdateMode = bModeChanged;
                }
                else
                {
                    int mode;
                    SlideMenuList* list = (SlideMenuList*)mSlideMenuLists[3];
                    if (list != NULL)
                    {
                        mode = list->GetMenuItem()->GetType()->GetUserEnumType();
                    }
                    else
                    {
                        mode = -1;
                    }

                    mSettings.Mode = (eAudioMode)mode;
                    if (mSettings.Mode != mSettings.DefaultMode)
                    {
                        mbUpdateMode = true;
                    }
                    else
                    {
                        mbUpdateMode = false;
                    }
                }
            }
            else if (res == RES_NOT_CHANGED)
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x19E0 | 0x800B6A24 | size: 0x6E8
 */
OptionsVisualMenuV2::OptionsVisualMenuV2(FEPresentation* pres, ButtonComponent::ButtonState btnState, VisualSettings& settings)
    : OptionsSubMenu(pres, btnState)
    , mSettings(settings)
{
    static char* MENU_ITEMS[] = {
        "MENU ITEM3",
        "MENU ITEM6",
        "MENU ITEM4",
    };

    if (nlTaskManager::m_pInstance->m_CurrState == 1)
    {
        pres->SetActiveSlide("Slide2");
    }
    else
    {
        pres->SetActiveSlide(VISUAL_MENU_SLIDE);
    }

    pres->Update(0.0f);

    SetButtonState(btnState);
    if (btnState == ButtonComponent::BS_A_AND_B)
    {
        SetAButtonLOC(0x9C81A82F);
    }

    TLSlide* currentSlide = pres->GetActiveSlide();

    for (int i = 0; i < 3; i++)
    {
        TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(MENU_ITEMS[i])));

        MenuItem<TLComponentInstance>* menuItem = mMenuItems.AddItem((TLComponentInstance*)instance);

        menuItem->SetCallback(ON_HIGHLIGHT, SingleHighlite::OpenItem);

        menuItem->SetCallback(ON_UNHIGHLIGHT, SingleHighlite::CloseItem);

        if (i == 0)
        {
            SingleHighlite::TempDisableSound();
            menuItem->RunCallback(ON_HIGHLIGHT);
        }
        else
        {
            menuItem->RunCallback(ON_UNHIGHLIGHT);
        }

        mSlideMenuLists[i] = NULL;
    }

    mMenuItems.SetFlag(3);

    TLComponentInstance* compinstance;

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAMERA")));

    BuildSubMenuList(0, compinstance, true, mSettings.mIsAutoZoomCamera ? 0 : 1);
    compinstance->m_bVisible = true;
    mMenuItems.GetMenuItem(0)->GetType()->m_bVisible = true;

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ZOOM")));

    BuildSubMenuList(1, compinstance, false, (int)(10.0f * mSettings.mCameraZoomLevel));
    compinstance->m_bVisible = true;
    mMenuItems.GetMenuItem(1)->GetType()->m_bVisible = true;

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ASPECT")));

    BuildSubMenuList(2, compinstance, true, mSettings.mIsWidescreen ? 1 : 0);
    compinstance->m_bVisible = true;
    mMenuItems.GetMenuItem(2)->GetType()->m_bVisible = true;

    ColourAllText(SubMenuHighliteColour, mMenuItems.GetActiveItemIndex());

    memcpy(&mBackupSettings, &mSettings, sizeof(VisualSettings));
    mSettingsCRC = nlChecksum32(&mBackupSettings, sizeof(VisualSettings));
}

/**
 * Offset/Address/Size: 0x18F4 | 0x800B6938 | size: 0xEC
 */
OptionsVisualMenuV2::~OptionsVisualMenuV2()
{
}

/**
 * Offset/Address/Size: 0x1834 | 0x800B6878 | size: 0xC0
 */
void OptionsVisualMenuV2::Update(float dt)
{
    OptionsSubMenu::Update(dt);

    MenuItem<TLComponentInstance>* menuItem = mMenuItems.GetMenuItem(1);
    if (menuItem == NULL)
        return;

    int userEnumType;
    SlideMenuList* list = (SlideMenuList*)mSlideMenuLists[0];
    if (list != NULL)
    {
        userEnumType = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        userEnumType = -1;
    }

    if (userEnumType == 0)
    {
        TLComponentInstance* compInstance = menuItem->GetType();
        if (compInstance->m_bVisible != false)
        {
            compInstance->m_bVisible = false;
            menuItem->SetDisabledFlag(true);

            SlideMenuList* list2 = (SlideMenuList*)mSlideMenuLists[1];
            TLComponentInstance* compInstance2 = list2->GetComponentInstance();
            compInstance2->m_bVisible = false;
        }
    }
    else
    {
        TLComponentInstance* compInstance = menuItem->GetType();
        if (compInstance->m_bVisible == false)
        {
            compInstance->m_bVisible = true;
            menuItem->SetDisabledFlag(false);

            SlideMenuList* list2 = (SlideMenuList*)mSlideMenuLists[1];
            TLComponentInstance* compInstance2 = list2->GetComponentInstance();
            compInstance2->m_bVisible = true;
        }
    }
}

/**
 * Offset/Address/Size: 0x1760 | 0x800B67A4 | size: 0xD4
 */
void OptionsVisualMenuV2::Save()
{
    SlideMenuList* list;
    int val;

    list = (SlideMenuList*)mSlideMenuLists[0];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    mSettings.mIsAutoZoomCamera = (val == 0);

    list = (SlideMenuList*)mSlideMenuLists[1];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    mSettings.mCameraZoomLevel = (float)val / 10.0f;

    list = (SlideMenuList*)mSlideMenuLists[2];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    mSettings.mIsWidescreen = (val != 0);
}

/**
 * Offset/Address/Size: 0x1730 | 0x800B6774 | size: 0x30
 */
void OptionsVisualMenuV2::Revert()
{
    memcpy(&mSettings, &mBackupSettings, sizeof(VisualSettings));
}

/**
 * Offset/Address/Size: 0xEE0 | 0x800B5F24 | size: 0x850
 */
OptionsGameplayMenuV2::OptionsGameplayMenuV2(FEPresentation* presentation, ButtonComponent::ButtonState buttonstate, GameplaySettings& settings, int skilltoskip)
    : OptionsSubMenu(presentation, buttonstate)
    , mSettings(settings)
{
    extern int nlSNPrintf(char*, unsigned long, const char*, ...);

    char menuname[64];
    int i;
    TLInstance* instance;
    TLComponentInstance* compinstance;
    int gtindex;

    presentation->SetActiveSlide(GAMEPLAY_MENU_SLIDE);
    presentation->Update(0.0f);

    SetButtonState(buttonstate);
    if (buttonstate == ButtonComponent::BS_A_AND_B)
    {
        SetAButtonLOC(0x9C81A82F);
    }

    TLSlide* currentSlide = presentation->GetActiveSlide();
    for (i = 0; i < 6; i++)
    {
        nlSNPrintf(menuname, 64, "MENU ITEM%d", i + 1);

        instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuname)));

        TLComponentInstance* componentinstance = (TLComponentInstance*)instance;
        MenuItem<TLComponentInstance>* menuItem = mMenuItems.AddItem(componentinstance);

        menuItem->SetCallback(ON_HIGHLIGHT, SingleHighlite::OpenItem);

        menuItem->SetCallback(ON_UNHIGHLIGHT, SingleHighlite::CloseItem);

        if (i == 0)
        {
            SingleHighlite::TempDisableSound();
            menuItem->RunCallback(ON_HIGHLIGHT);
            menuItem->SetDisabledFlag(false);
        }
        else
        {
            CloseItem(componentinstance);
        }

        mSlideMenuLists[i] = NULL;
    }

    mMenuItems.SetFlag(1);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("SKILLLEVEL")));
    BuildSkillLevelMenu(compinstance, settings.SkillLevel, skilltoskip);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("GAME TIME")));

    gtindex = 0;
    switch (settings.GameTime)
    {
    case 120:
        gtindex = 0;
        break;
    case 180:
        gtindex = 1;
        break;
    case 240:
        gtindex = 2;
        break;
    case 300:
        gtindex = 3;
        break;
    case 600:
        gtindex = 4;
        break;
    case 900:
        gtindex = 5;
        break;
    }
    BuildSubMenuList(1, compinstance, true, gtindex);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("PUPS")));
    BuildSubMenuList(2, compinstance, true, settings.PowerUps ? 0 : 1);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("S2S")));
    BuildSubMenuList(3, compinstance, true, settings.Shoot2Score ? 0 : 1);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("RUMBLE")));
    BuildSubMenuList(4, compinstance, true, settings.RumbleEnabled ? 0 : 1);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("BOWSER")));
    BuildSubMenuList(5, compinstance, true, settings.BowserAttackEnabled ? 0 : 1);

    ColourAllText(SubMenuHighliteColour, mMenuItems.GetActiveItemIndex());

    memcpy(&mBackupSettings, &mSettings, sizeof(GameplaySettings));
    mSettingsCRC = nlChecksum32(&mBackupSettings, sizeof(GameplaySettings));
}

/**
 * Offset/Address/Size: 0xAA8 | 0x800B5AEC | size: 0x438
 */
void OptionsGameplayMenuV2::BuildSkillLevelMenu(TLComponentInstance* compinstance, int startindex, int skilltoskip)
{
    extern int nlSNPrintf(char*, unsigned long, const char*, ...);
    nlColour lockColour;

    SlideMenuList* list = new ((SlideMenuList*)nlMalloc(sizeof(SlideMenuList), 8, false)) SlideMenuList(compinstance);
    mSlideMenuLists[0] = list;

    char slidename[64] = { 0 };
    MenuItem<SlideMenuItem>* menuItem;
    int slidenum = 1;
    do
    {
        nlSNPrintf(slidename, 64, "Slide%d", slidenum);
        compinstance->SetActiveSlide(slidename);
        if (compinstance->GetActiveSlide() == NULL)
        {
            break;
        }
        unsigned long slideHash = compinstance->GetActiveSlide()->m_hash;

        SlideMenuList* sml = (SlideMenuList*)mSlideMenuLists[0];

        menuItem = sml->AddItem(slideHash, slidenum);

        if (skilltoskip == slidenum)
            menuItem->SetDisabledFlag(true);
        else
            menuItem->SetDisabledFlag(false);

        slidenum++;
    } while (slidenum);

    SlideMenuList* sml = (SlideMenuList*)mSlideMenuLists[0];
    sml->SetItem(startindex - 1);
    ((SlideMenuList*)mSlideMenuLists[0])->SetFlag(3);
}

/**
 * Offset/Address/Size: 0x9BC | 0x800B5A00 | size: 0xEC
 */
OptionsGameplayMenuV2::~OptionsGameplayMenuV2()
{
}

/**
 * Offset/Address/Size: 0x798 | 0x800B57DC | size: 0x224
 */
void OptionsGameplayMenuV2::Save()
{
    GameplaySettings localSettings;
    SlideMenuList* list;
    int val;

    list = (SlideMenuList*)mSlideMenuLists[0];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.SkillLevel = (GameplaySettings::eSkillLevel)val;

    // Convert the menu index to seconds.
    list = (SlideMenuList*)mSlideMenuLists[1];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    switch (val)
    {
    case 0:
        localSettings.GameTime = 120;
        break;
    case 1:
        localSettings.GameTime = 180;
        break;
    case 2:
        localSettings.GameTime = 240;
        break;
    case 3:
        localSettings.GameTime = 300;
        break;
    case 4:
        localSettings.GameTime = 600;
        break;
    case 5:
        localSettings.GameTime = 900;
        break;
    default:
        localSettings.GameTime = 120;
        break;
    }

    list = (SlideMenuList*)mSlideMenuLists[2];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.PowerUps = (val == 0);

    list = (SlideMenuList*)mSlideMenuLists[3];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.Shoot2Score = (val == 0);

    list = (SlideMenuList*)mSlideMenuLists[4];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.RumbleEnabled = (val == 0);

    list = (SlideMenuList*)mSlideMenuLists[5];
    if (list != NULL)
    {
        val = list->GetMenuItem()->GetType()->GetUserEnumType();
    }
    else
    {
        val = -1;
    }
    localSettings.BowserAttackEnabled = (val == 0);

    mSettings = localSettings;
    mSettings.OnSettingsUpdated();

    cPlatPad::m_bDisableRumble = !localSettings.RumbleEnabled;
}

/**
 * Offset/Address/Size: 0x744 | 0x800B5788 | size: 0x54
 */
void OptionsGameplayMenuV2::Revert()
{
    memcpy(&mSettings, &mBackupSettings, sizeof(GameplaySettings));
    mSettings.OnSettingsUpdated();
    cPlatPad::m_bDisableRumble = !mSettings.RumbleEnabled;
}

void OptionsGameplayMenuV2::OpenItem(TLComponentInstance* compinstance)
{
    TLComponentInstance* highlight;
    TLInstance* highlightimage;
    TLTextInstance* text;

    compinstance->SetActiveSlide("in");

    highlight = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("high")));

    highlight->SetActiveSlide("in");

    text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        highlight->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CENTER")));
    if (!mMenuItems.GetMenuItem()->IsLocked())
    {
        text->SetAssetColour(MenuHighliteColour);
    }

    highlightimage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        highlight->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite")));
    highlightimage->SetAssetColour(MenuHighliteColour);

    compinstance->Update(0.0f);

    if (SingleHighlite::TEMPDISABLESOUND == false)
    {
        FEAudio::PlayAnimAudioEvent("sfx_menu_highlight_open", false);
    }

    SingleHighlite::TEMPDISABLESOUND = false;
}

/**
 * Offset/Address/Size: 0x620 | 0x800B5664 | size: 0x124
 */
void OptionsGameplayMenuV2::CloseItem(TLComponentInstance* compinstance)
{
    compinstance->SetActiveSlide("out");
    compinstance->Update(0.0f);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("high")));

    compinstance->SetActiveSlide("out");
    compinstance->Update(0.0f);

    FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("may_highlite")))
        ->SetAssetColour(MenuHighliteColour);
}

/**
 * Offset/Address/Size: 0x210 | 0x800B5254 | size: 0x410
 */
OptionsSaveLoad::OptionsSaveLoad(FEPresentation* presentation, ButtonComponent::ButtonState buttonstate)
    : OptionsSubMenu(presentation, buttonstate)
{
    static char* MENU_ITEMS[] = { "MENU ITEM2", "MENU ITEM3" };

    presentation->SetActiveSlide(SAVE_LOAD_SLIDE);
    presentation->Update(0.0f);

    SetButtonState(buttonstate);
    mButtons.CentreButtons();

    TLSlide* slide = presentation->GetActiveSlide();
    char** menuName;

    void (*openItem)(TLComponentInstance*) = DoubleHighlite::OpenItem;
    void (*closeItem)(TLComponentInstance*) = DoubleHighlite::CloseItem;
    MenuItem<TLComponentInstance>* menuItem;

    int i;
    for (i = 0, menuName = MENU_ITEMS; i < 2; i++, menuName++)
    {
        TLComponentInstance* instance = FEFinder<TLComponentInstance, 4>::Find(
            slide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(*menuName)),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0));

        instance->SetActiveSlide((i == 0) ? DoubleHighlite::SLIDE_IN : DoubleHighlite::SLIDE_OUT);
        instance->Update(0.0f);

        menuItem = mMenuItems.AddItem(instance);

        menuItem->SetCallback(ON_HIGHLIGHT, openItem);

        menuItem->SetCallback(ON_UNHIGHLIGHT, closeItem);

        if (i == 0)
            DoubleHighlite::TempDisableSound();

        menuItem->RunCallback((i == 0) ? ON_HIGHLIGHT : ON_UNHIGHLIGHT);
    }

    mMenuItems.SetFlag(1);
}

/**
 * Offset/Address/Size: 0x124 | 0x800B5168 | size: 0xEC
 */
OptionsSaveLoad::~OptionsSaveLoad()
{
}

/**
 * Offset/Address/Size: 0x8 | 0x800B504C | size: 0x11C
 */
void OptionsSaveLoad::Update(float dt)
{
    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        if (mMenuItems.GetActiveItemIndex() == 0)
        {
            ResetEnableSaveLoadFlag();
            if (SaveLoadScene::IsIOEnabled())
            {
                nlSingleton<GameSceneManager>::Instance()->Pop();
                SaveLoadScene* handler = (SaveLoadScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_ASK_SAVE, SCREEN_FORWARD, false);
                handler->mNextScene = SCENE_OPTIONS;
            }
        }
        else
        {
            ResetEnableSaveLoadFlag();
            if (SaveLoadScene::IsIOEnabled())
            {
                nlSingleton<GameSceneManager>::Instance()->Pop();
                SaveLoadScene* handler = (SaveLoadScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_ASK_LOAD, SCREEN_FORWARD, false);
                handler->mNextScene = SCENE_OPTIONS;
            }
        }
        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
    }
    else
    {
        OptionsSubMenu::Update(dt);
    }
}

/**
 * Offset/Address/Size: 0x4 | 0x800B5048 | size: 0x4
 */
void OptionsSaveLoad::Save()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x800B5044 | size: 0x4
 */
void OptionsSaveLoad::Revert()
{
}
