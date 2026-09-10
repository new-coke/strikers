#include "port/region.h"  // PORT: one binary, three discs
#include "Game/SH/SHTournTeamSetup.h"
#include "types.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/GameSceneManager.h"
#include "Game/SH/SHCupHub.h"
#include "NL/gl/glPlat.h"
#include "NL/gl/glStruct.h"

#include "NL/nlBind.h"

typedef Detail::MemFunImpl<void, void (TournTeamSetupSceneV2::*)(int)> MemFunImpl_Tourn_t;
typedef BindExp2<void, MemFunImpl_Tourn_t, TournTeamSetupSceneV2*, int> BindExp2_Tourn_t;

static unsigned long TOURN_CAPTAIN_DESCRIPTIONS[] = {
    0xFF68ABBA, 0xE2D37C19, 0x000465BA, 0x000BAD38, 0x0043DF21, 0x330C3072, 0x00C0A242, 0x00EC84AC, 0x69BFAF9D
};

/**
 * Offset/Address/Size: 0x4DBC | 0x800E6C60 | size: 0x124
 */
TournTeamSetupSceneV2::TournTeamSetupSceneV2()
    : BaseSceneHandler()
    , mMenuItems()
    , mRowOffset(0)
    , mCurrentRow(0)
    , mTournInfo(nlSingleton<GameInfoManager>::Instance()->mCustomTournamentInfo)
    , mCurrentState(STATE_IN)
    , mCurrentCaptain(TEAM_MARIO)
    , mCurrentSK(SK_TOAD)
    , mButtons1()
    , mButtons2()
{
    int i = 0;

    mHasTaggedHumanPlayer = false;

    for (; i < mTournInfo.m_numTeams; i++)
    {
        mTeamData[i].isEmpty = true;
        mTeamData[i].isHumanPlayer = false;
        mTeamData[i].captain = TEAM_MARIO;
        mTeamData[i].sidekick = SK_TOAD;
    }
}

/**
 * Offset/Address/Size: 0x4C18 | 0x800E6ABC | size: 0x1A4
 */
TournTeamSetupSceneV2::~TournTeamSetupSceneV2()
{
    delete mCaptainGrid;
    delete mSKGrid;

    if (mTicker != NULL)
    {
        delete mTicker;
    }
}

/**
 * Offset/Address/Size: 0x3ED4 | 0x800E5D78 | size: 0xD44
 */
void TournTeamSetupSceneV2::SceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    FEAudio::EnableSounds(false);

    typedef MenuItem<TLComponentInstance>::Callback MenuCallback;

    for (int i = 0; i < 4; i++)
    {
        MenuItem<TLComponentInstance>* menuItem;
        char menuname[64];
        nlSNPrintf(menuname, 64, "MENU ITEM%d", i + 1);

        TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuname)));
        TLComponentInstance* compinstance = (TLComponentInstance*)instance;

        if (i < mTournInfo.m_numTeams)
        {
            compinstance->SetActiveSlide((i == 0) ? DoubleHighlite::SLIDE_IN : DoubleHighlite::SLIDE_OUT);
            UpdateRow(i);

            if (mCurrentState == STATE_SCROLLING)
            {
                menuItem = mMenuItems.AddItem(compinstance);

                {
                    MenuCallback openFunction(DoubleHighlite::OpenItem);
                    menuItem->SetCallback(ON_HIGHLIGHT, openFunction);
                }

                {
                    MenuCallback closeFunction(DoubleHighlite::CloseItem);
                    menuItem->SetCallback(ON_UNHIGHLIGHT, closeFunction);
                }

                {
                    MenuCallback applyFunction(Bind<void, MemFunImpl_Tourn_t, TournTeamSetupSceneV2*, int>(
                        MemFun<TournTeamSetupSceneV2, void, int>(&TournTeamSetupSceneV2::StartChooseCaptain),
                        this,
                        i));
                    menuItem->SetCallback(ON_APPLY, applyFunction);
                }

                menuItem->RunCallback((i == 0) ? ON_HIGHLIGHT : ON_UNHIGHLIGHT);

                TLSlide* slide = compinstance->GetActiveSlide();
                compinstance->Update(slide->m_start + slide->m_duration);
            }
            else
            {
                if (i == 0)
                {
                    DoubleHighlite::OpenItem(compinstance);
                }
                else
                {
                    DoubleHighlite::CloseItem(compinstance);

                    TLSlide* slide = compinstance->GetActiveSlide();
                    compinstance->Update(slide->m_start + slide->m_duration);
                }
            }
        }
        else
        {
            DoubleHighlite::CloseItem(compinstance);

            TLSlide* slide = compinstance->GetActiveSlide();
            compinstance->Update(slide->m_start + slide->m_duration);
            compinstance->m_bVisible = false;
        }
    }

    if (mCurrentState == STATE_SCROLLING)
    {
        TLComponentInstance* comp0 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CAPTAIN_NAME_RIGHT")));
        comp0->m_bVisible = false;
        mComponents[0] = comp0;

        TLComponentInstance* comp1 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CAPTAIN_CHOOSER_LEFT")));
        comp1->m_bVisible = false;
        mComponents[1] = comp1;

        TLComponentInstance* comp2 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CHOOSE_SIDEKICKS_LEFT")));
        comp2->m_bVisible = false;
        mComponents[2] = comp2;

        TLComponentInstance* chooserComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[1]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        chooserComp->m_bVisible = false;

        chooserComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[2]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        chooserComp->m_bVisible = false;

        UpdateCaptainName();

        mCaptainGrid = new (nlMalloc(sizeof(ICaptainGridComponent), 8, false)) ICaptainGridComponent(mComponents[1], false);
        mCaptainGrid->BuildMapMenu();

        mSKGrid = new (nlMalloc(sizeof(ISidekickGridComponent), 8, false)) ISidekickGridComponent(mComponents[2], false);
        mSKGrid->BuildMapMenu();

        TLComponentInstance* tempComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("Component")));

        TLTextInstance* scrollText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            tempComponent->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("Group")),
            InlineHasher(nlStringLowerHash("TickerText")));

        gl_ScreenInfo* screenInfo = glGetScreenInfo();
        mTicker = new (nlMalloc(sizeof(FEScrollText), 0x20, true)) FEScrollText(scrollText, 0, screenInfo->ScreenWidth + 0x32);
        mTicker->SetDisplayMessage("CHOOSE_CAPTAIN_CUSTOM_TOURNAMENT");

        mPressStartComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            m_pFEScene->m_pFEPackage->GetPresentation()->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("START TO CONTINUE")));
        mPressStartComponent->m_bVisible = false;
    }
    else
    {
        presentation->SetActiveSlide("CHANGER");

        mButtons1.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));
        mButtons1.SetState((ButtonComponent::ButtonState)3);

        presentation->SetActiveSlide("MENU IN");

        mButtons2.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));
        mButtons2.SetState((ButtonComponent::ButtonState)3);
    }

    TLComponentInstance* tempComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ARROWS")));

    if (mTournInfo.m_numTeams < 5)
    {
        tempComponent->m_bVisible = false;
    }

    mUpArrow = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        tempComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrow2")));

    mDownArrow = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        tempComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrow")));

    if (port_region() == PORT_REGION_JAPAN
        && (mCurrentState == STATE_CAPTAIN || mCurrentState == STATE_SIDEKICK))
    {
        mUpArrow->m_bVisible = false;
        mDownArrow->m_bVisible = false;
    }
    else
        if (mCurrentRow == 0)
    {
        mUpArrow->m_bVisible = false;
        mDownArrow->m_bVisible = true;
    }
    else if (mCurrentRow == (int)mTournInfo.m_numTeams - 1)
    {
        mUpArrow->m_bVisible = true;
        mDownArrow->m_bVisible = false;
    }
    else
    {
        mUpArrow->m_bVisible = true;
        mDownArrow->m_bVisible = true;
    }

    FEAudio::EnableSounds(true);
}

/**
 * Offset/Address/Size: 0x32F4 | 0x800E5198 | size: 0xBE0
 */
void TournTeamSetupSceneV2::Update(float fDeltaT)
{
    static const char* lastCaptainSelectSoundStrPlayed;
    static signed char init;

    BaseSceneHandler::Update(fDeltaT);

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    if (!init)
    {
        lastCaptainSelectSoundStrPlayed = NULL;
        init = 1;
    }

    SetupButtonsBasedOnState(mCurrentState);

    if (mCurrentState == STATE_IN)
    {
        TLSlide* slide = presentation->m_currentSlide;
        if (presentation->m_fadeDuration < (slide->m_start + slide->m_duration))
        {
            return;
        }

        mCurrentState = STATE_SCROLLING;
        presentation->SetActiveSlide("CHANGER");
        SceneCreated();
    }

    TLComponentInstance* pTickerComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Component")));

    TLSlide* tickerSlide = pTickerComp->GetActiveSlide();
    float endTime = tickerSlide->m_start + tickerSlide->m_duration;
    if (pTickerComp->GetActiveSlide()->m_time >= endTime)
    {
        mTicker->Update(fDeltaT);
    }

    if (mCurrentState == STATE_SCROLLING)
    {
        presentation->SetActiveSlide(presentation->m_currentSlide);
        presentation->Update(0.0f);

        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            mMenuItems.RunCallbackOnCurrent(ON_APPLY);
            FEAudio::PlayAnimAudioEvent("sfx_accept", false);
        }
        else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_OPTIONS_INITIAL_TOURN, SCREEN_BACK, true);
            FEAudio::PlayAnimAudioEvent("sfx_back", false);
        }
        else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x800, false, NULL))
        {
            AutoFill();

            if (CanProceed() == 1)
            {
                mPressStartComponent->m_bVisible = true;
            }
            else
            {
                mPressStartComponent->m_bVisible = false;
            }
        }
        else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
        {
            ScrollUp(true);
        }
        else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
        {
            ScrollDown(true);
        }
        else
        {
            if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xB, true, NULL) || g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xC, true, NULL))
            {
                if (!mTeamData[mCurrentRow].isEmpty)
                {
                    mTeamData[mCurrentRow].isHumanPlayer = !mTeamData[mCurrentRow].isHumanPlayer;
                    UpdateControllerIcon(mMenuItems.GetActiveItemIndex());

                    if (CanProceed() == 1)
                    {
                        mPressStartComponent->m_bVisible = true;
                    }
                    else
                    {
                        mPressStartComponent->m_bVisible = false;
                    }

                    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xB, true, NULL))
                    {
                        FEAudio::PlayAnimAudioEvent("sfx_option_scroll_left", false);
                    }
                    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xC, true, NULL))
                    {
                        FEAudio::PlayAnimAudioEvent("sfx_option_scroll_right", false);
                    }
                }
            }
            else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x1000, false, NULL))
            {
                int result = CanProceed();

                if (result == 1)
                {
                    Proceed();
                    FEAudio::PlayAnimAudioEvent("sfx_accept", false);
                }
                else if (result == -1)
                {
                    FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
                    pPopup->Create(POPUP_FILLALLSLOTS);
                }
                else if (result == -2)
                {
                    FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
                    pPopup->Create(POPUP_NO_HUMAN_TOURNAMENT);
                }
            }
        }

        UpdateArrowVisibility();
    }
    else if (mCurrentState == STATE_CAPTAIN)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            if (mCaptainGrid->IsValid(mCurrentCaptain))
            {
                if (mCurrentCaptain == TEAM_MYSTERY)
                {
                    SetTeam();
                    AutoTagCurrentRowAsHumanPlayer();
                    UpdateRow(mMenuItems.GetActiveItemIndex());
                    ChangeState(mCurrentState, STATE_SCROLLING);
                    lastCaptainSelectSoundStrPlayed = FECharacterSound::PlayCaptainName(mCurrentCaptain);
                    ScrollDown(false);
                }
                else
                {
                    ChangeState(mCurrentState, STATE_SIDEKICK);
                    lastCaptainSelectSoundStrPlayed = FECharacterSound::PlayCaptainName(mCurrentCaptain);
                }
                FEAudio::PlayAnimAudioEvent("sfx_accept", false);
            }
            else
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }
        }
        else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            if (!mTeamData[mCurrentRow].isEmpty)
            {
                mCaptainGrid->SetValid(mTeamData[mCurrentRow].captain, false);
            }
            ChangeState(mCurrentState, STATE_SCROLLING);
            mCurrentCaptain = mTeamData[mCurrentRow].captain;
            mCaptainGrid->MoveHighlightToTarget(mCurrentCaptain);
            UpdateCaptainName();
            FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", false);
        }
        else
        {
            mCaptainGrid->Update(FE_ALL_PADS);
            if (mCaptainGrid->mHasChangedSinceLastUpdate)
            {
                mCurrentCaptain = mCaptainGrid->GetSelectedItem();
                mComponents[0]->SetActiveSlide("Slide1");
                UpdateCaptainName();
                mComponents[0]->Update(0.0f);
            }
        }
    }
    else if (mCurrentState == STATE_SIDEKICK)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            SetTeam();
            AutoTagCurrentRowAsHumanPlayer();
            UpdateRow(mMenuItems.GetActiveItemIndex());
            ChangeState(mCurrentState, STATE_SCROLLING);
            FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
            FEAudio::PlayAnimAudioEvent("sfx_character_group_left_exit", false);
            if (lastCaptainSelectSoundStrPlayed != NULL)
            {
                FEAudio::StopAnimAudioEvent(lastCaptainSelectSoundStrPlayed);
                lastCaptainSelectSoundStrPlayed = NULL;
            }
            FECharacterSound::PlaySidekickName(mCurrentSK);
            ScrollDown(false);
        }
        else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            ChangeState(mCurrentState, STATE_CAPTAIN);
            mCurrentSK = mTeamData[mCurrentRow].sidekick;
            mSKGrid->MoveHighlightToTarget(mCurrentSK);
            FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", false);
        }
        else
        {
            mSKGrid->Update(FE_ALL_PADS);
            if (mSKGrid->mHasChangedSinceLastUpdate)
            {
                mCurrentSK = mSKGrid->GetSelectedItem();
                mComponents[0]->SetActiveSlide("Slide2");
                UpdateSKName();
                mComponents[0]->Update(0.0f);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x2FB4 | 0x800E4E58 | size: 0x340
 */
void TournTeamSetupSceneV2::UpdateControllerIcon(int onScreenRow)
{
    int nlSNPrintf(char*, unsigned long, const char*, ...);

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    int row = onScreenRow + mRowOffset;

    char menuName[68];

    nlSNPrintf(menuName, 64, "MENU ITEM%d", onScreenRow + 1);

    TLComponentInstance* item = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(menuName)));

    item->SetActiveSlide("IN");
    TLSlide* active = item->GetActiveSlide();
    item->Update(active->m_start + active->m_duration);

    TLComponentInstance* controllerA = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        item->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CONTROLLER")));

    item->SetActiveSlide("OUT");
    active = item->GetActiveSlide();
    item->Update(active->m_start + active->m_duration);

    TLComponentInstance* controllerB = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        item->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CONTROLLER")));

    item->SetActiveSlide("IN");
    active = item->GetActiveSlide();
    item->Update(active->m_start + active->m_duration);

    if (mTeamData[row].isHumanPlayer)
    {
        controllerA->SetActiveSlide("CONTROLLER");
        controllerB->SetActiveSlide("CONTROLLER");
    }
    else
    {
        controllerA->SetActiveSlide("CPU");
        controllerB->SetActiveSlide("CPU");
    }

    TLComponentInstance* arrowsA = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        controllerA->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrows")));

    TLComponentInstance* arrowsB = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        controllerB->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrows")));

    arrowsA->m_bVisible = true;
    arrowsB->m_bVisible = true;
}

/**
 * Offset/Address/Size: 0x2500 | 0x800E43A4 | size: 0xAB4
 */
void TournTeamSetupSceneV2::UpdateRow(int onScreenRow)
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    int currentRow = onScreenRow + mRowOffset;

    char menuName[64];

    TLComponentInstance* pComp;
    TLTextInstance* pText1;
    TLComponentInstance* pCaptainComp1;
    TLComponentInstance* pSidekickComp1;
    TLComponentInstance* controller1;
    TLTextInstance* pNameText1;

    TLTextInstance* pText2;
    TLComponentInstance* pCaptainComp2;
    TLComponentInstance* pSidekickComp2;
    TLComponentInstance* controller2;
    TLTextInstance* pNameText2;

    TLComponentInstance* arrow1;
    TLComponentInstance* arrow2;

    nlSNPrintf(menuName, 64, "MENU ITEM%d", onScreenRow + 1);

    pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(menuName)));

    pComp->SetActiveSlide("IN");
    TLSlide* active = pComp->GetActiveSlide();
    pComp->Update(active->m_start + active->m_duration);

    pText1 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("NUMBER")));

    pCaptainComp1 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("captain heads")));

    pSidekickComp1 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("sidekick heads")));

    controller1 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CONTROLLER")));

    pNameText1 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("pauseresume")));

    pComp->SetActiveSlide("OUT");
    active = pComp->GetActiveSlide();
    pComp->Update(active->m_start + active->m_duration);

    pText2 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("NUMBER")));

    pCaptainComp2 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("captain heads")));

    pSidekickComp2 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("sidekick heads")));

    controller2 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CONTROLLER")));

    pNameText2 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("pauseresume")));

    if (mTeamData[currentRow].isHumanPlayer)
    {
        controller1->SetActiveSlide("CONTROLLER");
        controller2->SetActiveSlide("CONTROLLER");
    }
    else
    {
        controller1->SetActiveSlide("CPU");
        controller2->SetActiveSlide("CPU");
    }

    arrow1 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        controller1->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrows")));

    arrow2 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        controller2->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrows")));

    if (onScreenRow == (mCurrentRow - mRowOffset))
    {
        pComp->SetActiveSlide("IN");
        mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
        arrow1->m_bVisible = true;
        arrow2->m_bVisible = true;
    }
    else
    {
        arrow1->m_bVisible = false;
        arrow2->m_bVisible = false;
    }

    char numIDName[13] = "TOURNAMENT_?";
    numIDName[11] = (char)(currentRow + '1');

    pText1->SetStringId(numIDName);
    pText2->SetStringId(numIDName);

    if (mTeamData[currentRow].isEmpty)
    {
        pCaptainComp1->m_bVisible = false;
        pCaptainComp2->m_bVisible = false;
        pSidekickComp1->m_bVisible = false;
        pSidekickComp2->m_bVisible = false;

        pNameText1->SetStringId("FIELD_EMPTY");
        pNameText2->SetStringId("FIELD_EMPTY");

        controller1->m_bVisible = false;
        controller2->m_bVisible = false;
    }
    else
    {
        pCaptainComp1->m_bVisible = true;
        pCaptainComp2->m_bVisible = true;

        BasicString<char, Detail::TempStringAllocator> captainSlide = FindCaptainSlideName(mTeamData[currentRow].captain);
        pCaptainComp1->SetActiveSlide(captainSlide.c_str());
        pCaptainComp2->SetActiveSlide(captainSlide.c_str());

        controller1->m_bVisible = true;
        controller2->m_bVisible = true;

        if (mTeamData[currentRow].captain != TEAM_MYSTERY)
        {
            pSidekickComp1->m_bVisible = true;
            pSidekickComp2->m_bVisible = true;

            BasicString<char, Detail::TempStringAllocator> sidekickSlide = FindSidekickSlideName(mTeamData[currentRow].sidekick);
            pSidekickComp1->SetActiveSlide(sidekickSlide.c_str());
            pSidekickComp2->SetActiveSlide(sidekickSlide.c_str());
        }
        else
        {
            pSidekickComp1->m_bVisible = false;
            pSidekickComp2->m_bVisible = false;
        }

        pNameText1->m_LocStrId = GetLOCCharacterName(mTeamData[currentRow].captain, false, false);
        pNameText1->m_OverloadFlags |= 0x8;

        pNameText2->m_LocStrId = GetLOCCharacterName(mTeamData[currentRow].captain, false, false);
        pNameText2->m_OverloadFlags |= 0x8;
    }

    if (mTeamData[currentRow].isHumanPlayer)
    {
        controller1->SetActiveSlide("CONTROLLER");
        controller2->SetActiveSlide("CONTROLLER");
    }
    else
    {
        controller1->SetActiveSlide("CPU");
        controller2->SetActiveSlide("CPU");
    }
}

/**
 * Offset/Address/Size: 0x1F30 | 0x800E3DD4 | size: 0x5D0
 */
void TournTeamSetupSceneV2::ChangeState(TournTeamSetupSceneV2::eTeamChooserState from, TournTeamSetupSceneV2::eTeamChooserState to)
{
    TLComponentInstance* pCursorComp;

    if (from == STATE_CAPTAIN && to == STATE_SIDEKICK)
    {
        mComponents[0]->SetActiveSlide("Slide2");
        UpdateSKName();
        mComponents[0]->Update(0.0f);

        mComponents[1]->SetActiveSlide("OUT");
        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_exit", false);

        mCaptainGrid->RebuildInstanceTable();
        mCaptainGrid->UpdateSuperTeamIconState();

        pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[1]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        pCursorComp->m_bVisible = false;

        mComponents[2]->SetActiveSlide("IN");
        mComponents[2]->Update(0.0f);
        mComponents[2]->m_bVisible = true;

        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_enter", false);
        mSKGrid->RebindHighliteComponent("HIGHLIGHT");

        pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[2]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        pCursorComp->m_bVisible = true;
    }
    else if (from == STATE_CAPTAIN && to == STATE_SCROLLING)
    {
        mComponents[1]->SetActiveSlide("OUT");
        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_exit", false);
        mComponents[0]->m_bVisible = false;

        pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[2]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        pCursorComp->m_bVisible = false;
    }
    else if (from == STATE_SIDEKICK && to == STATE_CAPTAIN)
    {
        mComponents[0]->SetActiveSlide("Slide1");
        mComponents[1]->SetActiveSlide("SELECT");

        mCaptainGrid->RebuildInstanceTable();
        mCaptainGrid->UpdateSuperTeamIconState();

        pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[1]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        pCursorComp->m_bVisible = true;

        mCaptainGrid->RebindHighliteComponent("HIGHLIGHT");
        mComponents[2]->m_bVisible = false;
    }
    else if (from == STATE_SCROLLING && to == STATE_CAPTAIN)
    {
        mComponents[0]->m_bVisible = true;
        mComponents[1]->m_bVisible = true;

        mComponents[1]->SetActiveSlide("IN");
        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_enter", false);
        mComponents[1]->Update(0.0f);

        mCaptainGrid->RebuildInstanceTable();
        mCaptainGrid->UpdateSuperTeamIconState();
        mCaptainGrid->RebindHighliteComponent("HIGHLIGHT");

        pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[1]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        pCursorComp->m_bVisible = true;

        FEMapMenu* mapMenu = mCaptainGrid->mMapMenu;
        int oldSel = mapMenu->GetSelectedItem();
        int currentSel = oldSel;
        bool first = false;

        while (!mapMenu->IsSelectedItemActive())
        {
            mapMenu->MoveRight(!first);
            first = true;

            int newSel = mapMenu->GetSelectedItem();
            if (currentSel == newSel)
            {
                mapMenu->MoveDown(false);
                currentSel = mapMenu->GetSelectedItem();
                if (currentSel == oldSel)
                {
                    break;
                }
            }
        }

        mCurrentCaptain = mCaptainGrid->GetSelectedItem();
        UpdateCaptainName();
    }
    else if (from == STATE_SIDEKICK && to == STATE_SCROLLING)
    {
        mComponents[2]->SetActiveSlide("OUT");
        mComponents[0]->SetActiveSlide("Slide1");
        mComponents[0]->m_bVisible = false;

        pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mComponents[2]->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        pCursorComp->m_bVisible = false;
    }

    if (to == STATE_SCROLLING && CanProceed() == 1)
    {
        mPressStartComponent->m_bVisible = true;
    }
    else
    {
        mPressStartComponent->m_bVisible = false;
    }

    mCurrentState = to;
}

/**
 * Offset/Address/Size: 0x1EC4 | 0x800E3D68 | size: 0x6C
 */
void TournTeamSetupSceneV2::StartChooseCaptain(int onScreenRow)
{
    mCurrentRow = onScreenRow + mRowOffset;

    if (!mTeamData[mCurrentRow].isEmpty)
    {
        mCaptainGrid->SetValid(mTeamData[mCurrentRow].captain, true);
    }

    ChangeState(STATE_SCROLLING, STATE_CAPTAIN);
}

/**
 * Offset/Address/Size: 0x1CC4 | 0x800E3B68 | size: 0x200
 */
void TournTeamSetupSceneV2::UpdateCaptainName()
{
    TLSlide* slide = mComponents[0]->GetActiveSlide();

    TLTextInstance* captainNameText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME")));

    captainNameText->m_LocStrId = GetLOCCharacterName(mCurrentCaptain, false, true);
    captainNameText->m_OverloadFlags |= 0x8;

    TLTextInstance* captainDesc = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME2")));

    if (mCurrentCaptain == TEAM_MYSTERY && !nlSingleton<GameInfoManager>::Instance()->IsSuperTeamUnlocked())
    {
        captainDesc->SetStringId("CUP_ATTR_MYSTERY_LOCKED");
    }
    else
    {
        captainDesc->m_LocStrId = TOURN_CAPTAIN_DESCRIPTIONS[mCurrentCaptain];
        captainDesc->m_OverloadFlags |= 0x8;
    }

    TLComponentInstance* comp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("COMPONENT")));

    comp->SetActiveSlide(GetTeamName(mCurrentCaptain));
}

/**
 * Offset/Address/Size: 0x1A48 | 0x800E38EC | size: 0x27C
 */
void TournTeamSetupSceneV2::UpdateSKName()
{
    TLSlide* slide = mComponents[0]->GetActiveSlide();

    TLTextInstance* captainNameText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME")));

    captainNameText->m_LocStrId = GetLOCCharacterName(mCurrentCaptain, false, false);
    captainNameText->m_OverloadFlags |= 0x8;

    TLTextInstance* sidekickNameText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("SIDEKICK_NAME")));

    sidekickNameText->m_LocStrId = GetLOCSidekickName(mCurrentSK);
    sidekickNameText->m_OverloadFlags |= 0x8;

    TLTextInstance* captainDescText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME2")));

    if (mCurrentCaptain == TEAM_MYSTERY && !nlSingleton<GameInfoManager>::Instance()->IsSuperTeamUnlocked())
    {
        captainDescText->SetStringId("CUP_ATTR_MYSTERY_LOCKED");
    }
    else
    {
        captainDescText->m_LocStrId = TOURN_CAPTAIN_DESCRIPTIONS[mCurrentCaptain];
        captainDescText->m_OverloadFlags |= 0x8;
    }

    TLComponentInstance* teamComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("COMPONENT")));

    teamComp->SetActiveSlide(GetTeamName(mCurrentCaptain));
}

void TournTeamSetupSceneV2::SetTeam()
{
    mTeamData[mCurrentRow].captain = mCurrentCaptain;
    mTeamData[mCurrentRow].sidekick = mCurrentSK;
    mTeamData[mCurrentRow].isEmpty = false;
    mCaptainGrid->SetValid(mCurrentCaptain, false);
}

void TournTeamSetupSceneV2::UpdateAllRows()
{
    int i = 0;
    int numRows = ((u32)(3 - (u32)mTournInfo.m_numTeams) >> 31) + 3;
    for (; i < numRows; i++)
    {
        UpdateRow(i);
    }
}

int TournTeamSetupSceneV2::CanProceed() const
{
    int numHumans = 0;

    for (int i = 0; i < mTournInfo.m_numTeams; i++)
    {
        if (mTeamData[i].isEmpty)
        {
            return -1;
        }

        if (mTeamData[i].isHumanPlayer)
        {
            numHumans++;
        }
    }

    if (numHumans < 1)
    {
        return -2;
    }

    return 1;
}

/**
 * Offset/Address/Size: 0x15F4 | 0x800E3498 | size: 0x454
 */
void TournTeamSetupSceneV2::Proceed()
{
    GameInfoManager* pGameInfo = GameInfoManager::Instance();
    int i = 0;

    mTournInfo.m_cup->mHumanTeams = 0;

    for (; i < mTournInfo.m_numTeams; i++)
    {
        if (mTeamData[i].isHumanPlayer)
        {
            mTournInfo.m_cup->mHumanTeams = (u16)(mTournInfo.m_cup->mHumanTeams | (1 << mTeamData[i].captain));
        }
    }

    pGameInfo->SetMode(GameInfoManager::GM_TOURNAMENT);
    nlSingleton<GameInfoManager>::Instance()->GetCurrentRoundNumber();

    if (mTournInfo.m_tournMode == TM_LEAGUE)
    {
        CreateLeagueLineup();
        pGameInfo->SetPreviousTeamStats();
        pGameInfo->IncreaseRoundNumber();
    }
    else
    {
        BaseCup* pCup = pGameInfo->mCurrentCup;

        u16 numRounds = pCup->GetNumRounds();
        s16 roundNumber = -4;

        if (numRounds == 2)
        {
            roundNumber = -3;
        }

        pCup->mRoundNumber = roundNumber;

        CreateKnockout();
    }

    while (pGameInfo->GetCurrentRoundNumber() != -5)
    {
        if (pGameInfo->DetermineNextMatchups(0x1B))
        {
            break;
        }

        pGameInfo->IncreaseRoundNumber();
    }

    pGameInfo->SetResultsOfLastUserGame(RESULT_CUP_START);

    nlSingleton<GameSceneManager>::Instance()->Pop();

    if (pGameInfo->GetNumHumanTeams() == 1)
    {
        int j = 0;

        for (; j < mTournInfo.m_numTeams; j++)
        {
            if (mTeamData[j].isHumanPlayer)
            {
                pGameInfo->SetUserSelectedCupTeam(mTeamData[j].captain);
                break;
            }
        }
    }

    GameSceneManager* pSceneMgr = nlSingleton<GameSceneManager>::s_pInstance;
    SceneList nextScene = SCENE_TOURNAMENT_STANDINGS;

    if (mTournInfo.m_tournMode == TM_LEAGUE)
    {
        nextScene = SCENE_TOURNAMENT_STANDINGS_ANIM;
    }

    CupHubScene* pHubScene = (CupHubScene*)pSceneMgr->Push(nextScene, SCREEN_FORWARD, false);
    pHubScene->mDoAutoSave = true;
}

void TournTeamSetupSceneV2::CreateLeagueLineup()
{
    GameInfoManager* pTournamentInfo = GameInfoManager::Instance();
    int numPlayingTeams = pTournamentInfo->GetNumPlayingTeams();

    eTeamID lineup[8];
    eSidekickID sklineup[8];

    int i;
    for (i = 0; i < numPlayingTeams; i++)
    {
        lineup[i] = mTeamData[i].captain;
        sklineup[i] = mTeamData[i].sidekick;
    }

    pTournamentInfo->SetupRoundRobinSchedule(lineup, sklineup);
}

void TournTeamSetupSceneV2::CreateKnockout()
{
    GameInfoManager* pTournamentInfo = GameInfoManager::Instance();
    int numPlayingTeams = pTournamentInfo->GetNumPlayingTeams();

    eTeamID lineup[8];
    eSidekickID sklineup[8];

    int i;
    for (i = 0; i < numPlayingTeams; i++)
    {
        lineup[i] = mTeamData[i].captain;
        sklineup[i] = mTeamData[i].sidekick;
    }

    pTournamentInfo->SetupTournamentKnockout(lineup, sklineup);
}

/**
 * Offset/Address/Size: 0xC34 | 0x800E2AD8 | size: 0x9C0
 */
BasicString<char, Detail::TempStringAllocator> TournTeamSetupSceneV2::FindCaptainSlideName(eTeamID captain)
{
    BasicString<char, Detail::TempStringAllocator> returnValue;

    switch (captain)
    {
    case TEAM_DAISY:
        returnValue = BasicString<char, Detail::TempStringAllocator>("daisy");
        break;
    case TEAM_DONKEYKONG:
        returnValue = BasicString<char, Detail::TempStringAllocator>("dk");
        break;
    case TEAM_LUIGI:
        returnValue = BasicString<char, Detail::TempStringAllocator>("luigi");
        break;
    case TEAM_MARIO:
        returnValue = BasicString<char, Detail::TempStringAllocator>("mario");
        break;
    case TEAM_PEACH:
        returnValue = BasicString<char, Detail::TempStringAllocator>("peach");
        break;
    case TEAM_WALUIGI:
        returnValue = BasicString<char, Detail::TempStringAllocator>("waluigi");
        break;
    case TEAM_WARIO:
        returnValue = BasicString<char, Detail::TempStringAllocator>("wario");
        break;
    case TEAM_YOSHI:
        returnValue = BasicString<char, Detail::TempStringAllocator>("yoshi");
        break;
    case TEAM_MYSTERY:
        returnValue = BasicString<char, Detail::TempStringAllocator>("super");
        break;
    }

    return returnValue;
}

/**
 * Offset/Address/Size: 0x76C | 0x800E2610 | size: 0x4C8
 */
BasicString<char, Detail::TempStringAllocator> TournTeamSetupSceneV2::FindSidekickSlideName(eSidekickID sidekick)
{
    BasicString<char, Detail::TempStringAllocator> returnValue;

    switch (sidekick)
    {
    case SK_TOAD:
        returnValue = BasicString<char, Detail::TempStringAllocator>("toad");
        break;
    case SK_KOOPA:
        returnValue = BasicString<char, Detail::TempStringAllocator>("koopa");
        break;
    case SK_HAMMERBROS:
        returnValue = BasicString<char, Detail::TempStringAllocator>("hammer");
        break;
    case SK_BIRDO:
        returnValue = BasicString<char, Detail::TempStringAllocator>("birdo");
        break;
    }

    return returnValue;
}

bool TournTeamSetupSceneV2::CanAutoFill() const
{
    for (int i = 0; i < mTournInfo.m_numTeams; i++)
    {
        if (mTeamData[i].isEmpty)
        {
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x4B0 | 0x800E2354 | size: 0x2BC
 */
void TournTeamSetupSceneV2::AutoFill()
{
    bool changed = false;
    int i = 0;

    for (; i < mTournInfo.m_numTeams; i++)
    {
        if (mTeamData[i].isEmpty)
        {
            eTeamID randCapt;

            do
            {
                if (nlSingleton<GameInfoManager>::Instance()->IsSuperTeamUnlocked())
                {
                    randCapt = (eTeamID)nlRandom(9, &nlDefaultSeed);
                }
                else
                {
                    randCapt = (eTeamID)nlRandom(8, &nlDefaultSeed);
                }
            } while (IsAlreadySelected(randCapt));

            AutoTagCurrentRowAsHumanPlayer();

            mTeamData[i].isEmpty = false;
            mTeamData[i].sidekick = (eSidekickID)nlRandom(4, &nlDefaultSeed);
            mTeamData[i].captain = randCapt;
            mCaptainGrid->SetValid(randCapt, false);
            changed = true;
        }
    }

    if (changed)
    {
        int numRows = mTournInfo.m_numTeams == 3 ? 3 : 4;
        mRowOffset = mTournInfo.m_numTeams - numRows;

        mMenuItems.SetItem(numRows - 1);

        mCurrentRow = mRowOffset + mMenuItems.GetActiveItemIndex();
        mCurrentCaptain = mTeamData[mCurrentRow].captain;
        mCurrentSK = mTeamData[mCurrentRow].sidekick;

        mCaptainGrid->MoveHighlightToTarget(mCurrentCaptain);
        mSKGrid->MoveHighlightToTarget(mCurrentSK);
        UpdateCaptainName();

        UpdateAllRows();
    }
}

unsigned char TournTeamSetupSceneV2::IsAlreadySelected(eTeamID captain) const
{
    for (int i = 0; i < mTournInfo.m_numTeams; i++)
    {
        if (!mTeamData[i].isEmpty && mTeamData[i].captain == captain)
        {
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x3E4 | 0x800E2288 | size: 0xCC
 */
void TournTeamSetupSceneV2::UpdateForCurrentRow()
{
    UpdateAllRows();

    mCurrentCaptain = mTeamData[mCurrentRow].captain;
    mCurrentSK = mTeamData[mCurrentRow].sidekick;

    mCaptainGrid->MoveHighlightToTarget(mCurrentCaptain);
    mSKGrid->MoveHighlightToTarget(mCurrentSK);
    UpdateCaptainName();
}

void TournTeamSetupSceneV2::UpdateArrowVisibility()
{
    if (port_region() == PORT_REGION_JAPAN
        && (mCurrentState == STATE_CAPTAIN || mCurrentState == STATE_SIDEKICK))
    {
        mUpArrow->m_bVisible = false;
        mDownArrow->m_bVisible = false;
    }
    else
        if (mCurrentRow == 0)
    {
        mUpArrow->m_bVisible = false;
        mDownArrow->m_bVisible = true;
    }
    else if (mCurrentRow == (int)mTournInfo.m_numTeams - 1)
    {
        mUpArrow->m_bVisible = true;
        mDownArrow->m_bVisible = false;
    }
    else
    {
        mUpArrow->m_bVisible = true;
        mDownArrow->m_bVisible = true;
    }
}

/**
 * Offset/Address/Size: 0x1FC | 0x800E20A0 | size: 0x1E8
 */
void TournTeamSetupSceneV2::ScrollUp(bool bPlaySound)
{
    MenuResult result;
    bool doUpdate;

    if (!bPlaySound)
    {
        FEAudio::EnableSounds(false);
    }

    result = mMenuItems.PreviousItem();
    mCurrentRow = mRowOffset + mMenuItems.GetActiveItemIndex();
    FEAudio::EnableSounds(false);

    doUpdate = true;
    if (result == RES_NOT_CHANGED && mRowOffset > 0)
    {
        mRowOffset--;
        mCurrentRow = mRowOffset + mMenuItems.GetActiveItemIndex();
    }
    else if (result == RES_NOT_CHANGED)
    {
        doUpdate = false;
        FEAudio::PlayAnimAudioEvent("sfx_deny", false);
    }

    if (doUpdate)
    {
        if (result == RES_NOT_CHANGED)
        {
            FEAudio::EnableSounds(true);
        }
        UpdateForCurrentRow();
    }

    FEAudio::EnableSounds(true);
}

/**
 * Offset/Address/Size: 0x0 | 0x800E1EA4 | size: 0x1FC
 */
void TournTeamSetupSceneV2::ScrollDown(bool bPlaySound)
{
    MenuResult result;
    bool doUpdate;

    if (!bPlaySound)
    {
        FEAudio::EnableSounds(false);
    }

    result = mMenuItems.NextItem();
    mCurrentRow = mRowOffset + mMenuItems.GetActiveItemIndex();
    FEAudio::EnableSounds(false);

    doUpdate = true;
    if (result == RES_NOT_CHANGED && mRowOffset + 3 < (int)mTournInfo.m_numTeams - 1)
    {
        mRowOffset++;
        mCurrentRow = mRowOffset + mMenuItems.GetActiveItemIndex();
    }
    else if (result == RES_NOT_CHANGED)
    {
        doUpdate = false;
        FEAudio::PlayAnimAudioEvent("sfx_deny", false);
    }

    if (doUpdate)
    {
        if (result == RES_NOT_CHANGED)
        {
            FEAudio::EnableSounds(true);
        }
        UpdateForCurrentRow();
    }

    FEAudio::EnableSounds(true);
}

void TournTeamSetupSceneV2::AutoTagCurrentRowAsHumanPlayer()
{
    if (!mHasTaggedHumanPlayer)
    {
        mTeamData[mCurrentRow].isHumanPlayer = true;
        mHasTaggedHumanPlayer = true;
    }
}

void TournTeamSetupSceneV2::SetupButtonsBasedOnState(eTeamChooserState state)
{
    switch (state)
    {
    case STATE_IN:
        mButtons1.SetState(ButtonComponent::BS_A_AND_B_AND_Y);
        mButtons1.CentreButtons();
        mButtons2.SetState(ButtonComponent::BS_A_AND_B_AND_Y);
        mButtons2.CentreButtons();
        break;

    case STATE_SCROLLING:
        if (CanAutoFill())
        {
            mButtons1.SetState(ButtonComponent::BS_A_AND_B_AND_Y);
            mButtons2.SetState(ButtonComponent::BS_A_AND_B_AND_Y);
        }
        else
        {
            int result = CanProceed();

            if (result == 1)
            {
                mButtons1.SetState((ButtonComponent::ButtonState)4);
                mButtons2.SetState((ButtonComponent::ButtonState)4);
            }
            else
            {
                mButtons1.SetState(ButtonComponent::BS_A_AND_B);
                mButtons2.SetState(ButtonComponent::BS_A_AND_B);
            }
        }

        mButtons1.CentreButtons();
        mButtons2.CentreButtons();
        break;

    case STATE_CAPTAIN:
    case STATE_SIDEKICK:
        mButtons1.SetState(ButtonComponent::BS_A_AND_B);
        mButtons2.SetState(ButtonComponent::BS_A_AND_B);
        mButtons1.CentreButtons();
        mButtons2.CentreButtons();
        break;

    default:
        break;
    }
}
