#include "Game/SH/SHChooseSides.h"

#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feManager.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/GameRenderTask.h"
#include "Game/GameSceneManager.h"
#include "Game/OverlayManager.h"
#include "Game/SH/SHPause.h"
#include "Game/Team.h"
#include "NL/nlColour.h"
#include "NL/nlConfig.h"
#include "NL/nlLocalization.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/gl/glStruct.h"

extern bool g_e3_Build;

/**
 * Offset/Address/Size: 0x21C8 | 0x800C6D88 | size: 0x194
 */
SHChooseSides2::SHChooseSides2(SHChooseSides2::eCSContext context)
    : BaseSceneHandler()
    , mContext(context)
    , m_pTicker(NULL)
    , mSoundDelay(0.0f)
    , mNextScene(SCENE_INVALID)
    , mBackScene(SCENE_INVALID)
    , mChooseSide()
    , mProceedDelay(-1)
    , mButtons()
{
    switch (mContext)
    {
    case FRIENDLY:
        if (g_e3_Build)
        {
            mNextScene = SCENE_SUPER_LOADING;
            mBackScene = SCENE_MAIN_MENU;
        }
        else
        {
            mNextScene = SCENE_STADIUM_SELECT;
            mBackScene = SCENE_CHOOSE_CAPTAINS;
        }
        break;
    case CUP:
        mNextScene = SCENE_SUPER_LOADING;
        mBackScene = SCENE_CUP_STANDINGS;
        break;
    case SUPERCUP:
        mNextScene = SCENE_SUPER_LOADING;
        mBackScene = SCENE_SUPER_CUP_STANDINGS;
        break;
    case TOURNAMENT:
        mNextScene = SCENE_SUPER_LOADING;
        mBackScene = SCENE_TOURNAMENT_STANDINGS;
        break;
    case PAUSE:
        mNextScene = SCENE_INVALID;
        mBackScene = IGSCENE_PAUSE;
        break;
    }

    mChooseSide.mPlayingSides[0] = -1;
    mChooseSide.mPlayingSides[1] = -1;
    mChooseSide.mPlayingSides[2] = -1;
    mChooseSide.mPlayingSides[3] = -1;

    if (mContext == PAUSE)
    {
        mChooseSide.mContext = CONTEXT_PAUSE;
    }
    else
    {
        mChooseSide.mContext = CONTEXT_FE;
    }

    mAsyncImage[0][0] = NULL;
    mAsyncImage[1][0] = NULL;
    mAsyncImage[0][1] = NULL;
    mAsyncImage[1][1] = NULL;
    mAsyncImage[0][2] = NULL;
    mAsyncImage[1][2] = NULL;
}

/**
 * Offset/Address/Size: 0x201C | 0x800C6BDC | size: 0x1AC
 */
SHChooseSides2::~SHChooseSides2()
{
    if (m_pTicker != NULL)
    {
        delete m_pTicker;
    }

    for (int i = 0; i < 3; i++)
    {
        if (mAsyncImage[0][i] != NULL)
        {
            delete mAsyncImage[0][i];
        }

        if (mAsyncImage[1][i] != NULL)
        {
            delete mAsyncImage[1][i];
        }
    }

    if (mContext == PAUSE)
    {
        g_bRenderWorld = true;
    }
}

/**
 * Offset/Address/Size: 0xCFC | 0x800C58BC | size: 0x1320
 */
void SHChooseSides2::SceneCreated()
{
    BindChooseSideInstances();

    TLSlide* currentSlide = m_pFEPresentation->m_currentSlide;

    TLTextInstance* scrollText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("TickerText")));

    const gl_ScreenInfo* screenInfo = glGetScreenInfo();

    FEScrollText* ticker = new (nlMalloc(sizeof(FEScrollText), 8, false)) FEScrollText(scrollText, 0, screenInfo->ScreenWidth + 0x32);
    m_pTicker = ticker;

    if (mContext == PAUSE)
    {
        m_pTicker->SetDisplayMessage(0x53B23764);
    }
    else
    {
        m_pTicker->SetDisplayMessage(0xACCB0957);
    }

    if (mContext != PAUSE)
    {
        const char* filename = "art/fe/LoadingScreensUI.res";

        for (int i = 0; i < 3; i++)
        {
            mAsyncImage[0][i] = new (nlMalloc(sizeof(AsyncImage), 8, false)) AsyncImage(filename, NULL);
            mAsyncImage[1][i] = new (nlMalloc(sizeof(AsyncImage), 8, false)) AsyncImage(filename, NULL);
        }

        TLComponentInstance* captaincomponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("LEFT_CAPT")));

        TLSlide* captainactiveslide = captaincomponent->GetActiveSlide();

        mAsyncImage[0][0]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            captainactiveslide,
            InlineHasher(nlStringLowerHash("CAPT_L")));

        mAsyncImage[0][2]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            captainactiveslide,
            InlineHasher(nlStringLowerHash("CAPT_L_WHITE")));

        mAsyncImage[0][1]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            captainactiveslide,
            InlineHasher(nlStringLowerHash("CAPT_L_OUT")));

        mSoundDelay = (captainactiveslide->m_start + captainactiveslide->m_duration) / 2.0f;

        captaincomponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("RIGHT_CAPT")));

        captainactiveslide = captaincomponent->GetActiveSlide();

        mAsyncImage[1][0]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            captainactiveslide,
            InlineHasher(nlStringLowerHash("CAPT_R")));

        mAsyncImage[1][2]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            captainactiveslide,
            InlineHasher(nlStringLowerHash("CAPT_R_WHITE")));

        mAsyncImage[1][1]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            captainactiveslide,
            InlineHasher(nlStringLowerHash("CAPT_R_OUT")));

        char mainfilename[128] = { };
        char outlinefilename[128] = { };
        char flashfilename[128] = { };

        for (int i = 0; i < 2; i++)
        {
            eTeamID teamid = nlSingleton<GameInfoManager>::Instance()->GetTeam((short)i);
            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN, mainfilename, 0x80, teamid, i);
            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_OUTLINE, outlinefilename, 0x80, teamid, i);
            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_FLASH, flashfilename, 0x80, teamid, i);

            mAsyncImage[i][0]->QueueLoad(mainfilename, true);
            mAsyncImage[i][1]->QueueLoad(outlinefilename, true);
            mAsyncImage[i][2]->QueueLoad(flashfilename, true);
        }

        TLComponentInstance* pNameComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CAPTAIN_NAME_LEFT")));

        TLComponentInstance* pIconComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pNameComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("COMPONENT")));

        pIconComp->SetActiveSlide(GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((short)0)));

        pNameComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CAPTAIN_NAME_RIGHT")));

        pIconComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pNameComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("COMPONENT")));

        pIconComp->SetActiveSlide(GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((short)1)));
    }
    else
    {
        const char* filename = "art/fe/CaptainIconsUI.res";

        AsyncImage* image0 = new (nlMalloc(sizeof(AsyncImage), 8, false)) AsyncImage(filename, NULL);
        mAsyncImage[0][0] = image0;

        mAsyncImage[0][0]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("HOME_AWAY")));

        AsyncImage* image1 = new (nlMalloc(sizeof(AsyncImage), 8, false)) AsyncImage(filename, NULL);
        mAsyncImage[1][0] = image1;

        mAsyncImage[1][0]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("HOME_AWAY2")));

        BasicString<char, Detail::TempStringAllocator> iconfilename[2];

        for (int i = 0; i < 2; i++)
        {
            eTeamID teamid = nlSingleton<GameInfoManager>::Instance()->GetTeam((short)i);
            switch (teamid)
            {
            case TEAM_DAISY:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_daisy");
                break;
            case TEAM_DONKEYKONG:
                if (i == 0)
                {
                    iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_dk");
                }
                else
                {
                    iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_dk_r");
                }
                break;
            case TEAM_LUIGI:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_luigi");
                break;
            case TEAM_MARIO:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_mario");
                break;
            case TEAM_PEACH:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_peach");
                break;
            case TEAM_WALUIGI:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_waluigi");
                break;
            case TEAM_WARIO:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_wario");
                break;
            case TEAM_YOSHI:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_yoshi");
                break;
            case TEAM_MYSTERY:
                iconfilename[i] = BasicString<char, Detail::TempStringAllocator>("fe/captain_icons/captain_icons_super");
                break;
            default:
                break;
            }

            mAsyncImage[i][0]->QueueLoad(iconfilename[i].c_str(), true);
        }
    }

    TLComponentInstance* captainnamecomponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME_LEFT")));

    TLTextInstance* captaintext = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        captainnamecomponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME")));

    captaintext->m_LocStrId = GetLOCTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((short)0));
    captaintext->m_OverloadFlags |= 8;

    captainnamecomponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME_RIGHT")));

    captaintext = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        captainnamecomponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME")));

    captaintext->m_LocStrId = GetLOCTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((short)1));
    captaintext->m_OverloadFlags |= 8;

    TLComponentInstance* buttons = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    if (buttons != NULL)
    {
        mButtons.mButtonInstance = buttons;

        if (mContext == PAUSE)
        {
            mButtons.SetState(ButtonComponent::BS_B_ONLY);
        }
        else
        {
            mButtons.SetState(ButtonComponent::BS_A_AND_B);
        }
    }

    GameInfoManager* const gim = nlSingleton<GameInfoManager>::s_pInstance;

    if (gim->IsInCupMode())
    {
        TLComponentInstance* sidesComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("choose_side")));

        int lockedSide = (!(gim->GetUserSelectedCupTeam() - gim->GetTeam(0))) ? 1 : 0;

        for (int i = 0; i < 4; i++)
        {
            char arrowName[64];
            nlSNPrintf(arrowName, 64, "arrows%d", i + 1);

            TLComponentInstance* arrowComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                sidesComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("Group")),
                InlineHasher(nlStringLowerHash(arrowName)));

            TLImageInstance* arrow1 = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
                arrowComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("p1arrows1")),
                InlineHasher(nlStringLowerHash("arrow")));

            TLImageInstance* arrow2 = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
                arrowComp->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("p1arrows1")),
                InlineHasher(nlStringLowerHash("arrow2")));

            if (lockedSide == 0)
            {
                arrow1->m_bVisible = true;
                arrow2->m_bVisible = false;
            }
            else if (lockedSide == 1)
            {
                arrow1->m_bVisible = false;
                arrow2->m_bVisible = true;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x7BC | 0x800C537C | size: 0x540
 */
void SHChooseSides2::UpdateChooseSideComponent(float fDeltaT)
{
    eFEINPUT_PAD pad = FE_ALL_PADS;
    GameInfoManager* gim = nlSingleton<GameInfoManager>::Instance();

    UpdateResult result;
    if (gim->IsInCupMode())
    {
        eTeamID cupTeam = gim->GetUserSelectedCupTeam();
        result = mChooseSide.Update(fDeltaT, &pad, (!(cupTeam - gim->GetTeam(0))) ? 1 : 0);
    }
    else
    {
        result = mChooseSide.Update(fDeltaT, &pad, -1);
    }

    switch (result)
    {
    case UPDATE_GO_FORWARD:
    {
        if (mContext == PAUSE)
        {
            if (mChooseSide.AllPlayersReady() || mChooseSide.AllPluggedInAreReady() || GetConfigBool(Config::Global(), "no_humans", false))
            {
                FEAudio::PlayAnimAudioEvent("sfx_back", false);

                SaveChanges();

                nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_PAUSE, SCREEN_BACK, true);
                break;
            }
        }

        if (mChooseSide.mPlayerReady[pad])
        {
            if (mChooseSide.AllPlayersReady())
            {
                mProceedDelay = 2;
                TLInstance* continueInst = mChooseSide.mInstanceTable[16];
                if (continueInst != NULL)
                {
                    continueInst->m_bVisible = false;
                }
            }
        }
        else
        {
            FEAudio::PlayAnimAudioEvent("sfx_accept", false);

            if (mChooseSide.AllPluggedInAreReady())
            {
                mProceedDelay = 2;
                TLInstance* continueInst = mChooseSide.mInstanceTable[16];
                if (continueInst != NULL)
                {
                    continueInst->m_bVisible = false;
                }
            }
            else
            {
                if (mContext != PAUSE)
                {
                    if (!mChooseSide.AtLeastOnePlayerReady())
                    {
                        FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
                        popup->Create(POPUP_NO_SIDES_CHOSEN);
                    }
                }
            }
        }
        break;
    }

    case UPDATE_GO_BACK:
    {
        FEAudio::PlayAnimAudioEvent("sfx_back", false);

        if (mContext == PAUSE)
        {
            if (mChooseSide.AllControllersAreCentred())
            {
                if (!GetConfigBool(Config::Global(), "no_humans", false))
                {
                    FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<OverlayManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
                    popup->Create(POPUP_NO_SIDES_CHOSEN);
                    break;
                }
            }

            SaveChanges();

            if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
            {
                nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_STRIKERS_101_PAUSE, SCREEN_BACK, true);
            }
            else
            {
                PauseMenuScene* scene = (PauseMenuScene*)nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_PAUSE, SCREEN_BACK, true);
                scene->mStartAnimAtEnd = true;
            }
        }
        else
        {
            nlSingleton<GameSceneManager>::Instance()->Push(mBackScene, SCREEN_BACK, true);
        }
        break;
    }
    }
}

void SHChooseSides2::SaveChanges()
{
    if (mContext == PAUSE)
    {
        for (int i = 0; i < 4; i++)
        {
            nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((u16)i);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        nlSingleton<GameInfoManager>::Instance()->SetPlayingSide((u16)i, (short)mChooseSide.mPlayingSides[i]);
    }

    if (mContext == PAUSE)
    {
        g_pTeams[0]->UpdateControllers();
        g_pTeams[1]->UpdateControllers();
        nlSingleton<GameInfoManager>::Instance()->ApplyDifficultySettings();
        g_pGame->SetDifficulty(
            nlSingleton<GameInfoManager>::s_pInstance->GetDifficulty(0),
            nlSingleton<GameInfoManager>::s_pInstance->GetDifficulty(1),
            (eDifficultyID)3);
    }
}

void SHChooseSides2::Proceed()
{
    FEAudio::PlayAnimAudioEvent("sfx_accept", false);

    SaveChanges();

    FrontEnd::SetControllerState();

    if (mNextScene == SCENE_SUPER_LOADING)
    {
        nlSingleton<GameSceneManager>::Instance()->PushLoadingScene(true);
    }
    else
    {
        nlSingleton<GameSceneManager>::Instance()->Push(mNextScene, SCREEN_FORWARD, true);
    }
}

/**
 * Offset/Address/Size: 0x350 | 0x800C4F10 | size: 0x46C
 */
void SHChooseSides2::BindChooseSideInstances()
{
    FEPresentation* pPres = GetPresentation();

    TLComponentInstance* choosesidecomponent = (TLComponentInstance*)FEFinder<TLInstance, 4>::Find<TLSlide>(
        pPres->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CHOOSE_SIDE")));

    TLSlide* activeslide = choosesidecomponent->GetActiveSlide();

    for (int i = 0; i < 4; i++)
    {
        char tempstring[64];
        nlSNPrintf(tempstring, 64, "controller%d", i + 1);

        mChooseSide.mInstanceTable[i] = FEFinder<TLInstance, 5>::Find<TLSlide>(
            activeslide,
            InlineHasher(nlStringLowerHash("group")),
            InlineHasher(nlStringLowerHash(tempstring)));

        mChooseSide.mInstanceTable[i + 4] = FEFinder<TLInstance, 3>::Find<TLInstance>(
            mChooseSide.mInstanceTable[i],
            InlineHasher(nlStringLowerHash("ready")));

        if (mChooseSide.mInstanceTable[i + 4])
        {
            mChooseSide.mInstanceTable[i + 4]->m_bVisible = false;
        }

        nlSNPrintf(tempstring, 64, "arrows%d", i + 1);

        mChooseSide.mInstanceTable[i + 12] = (TLInstance*)FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            activeslide,
            InlineHasher(nlStringLowerHash("group")),
            InlineHasher(nlStringLowerHash(tempstring)));

        nlSNPrintf(tempstring, 64, "p%d", i + 1);

        mChooseSide.mInstanceTable[i + 8] = (TLInstance*)FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            activeslide,
            InlineHasher(nlStringLowerHash("group")),
            InlineHasher(nlStringLowerHash(tempstring)));

        nlColour colour;
        colour.c[0] = PAD_COLOURS[i][0];
        colour.c[1] = PAD_COLOURS[i][1];
        colour.c[2] = PAD_COLOURS[i][2];
        colour.c[3] = 0xFF;
        mChooseSide.mInstanceTable[i + 8]->SetAssetColour(colour);

        if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
        {
            nlSNPrintf(tempstring, 64, "%dp_controller", i + 1);

            mChooseSide.mInstanceTable[i + 17] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
                activeslide,
                InlineHasher(nlStringLowerHash("group")),
                InlineHasher(nlStringLowerHash(tempstring)));
        }
        else
        {
            mChooseSide.mInstanceTable[i + 17] = NULL;
        }
    }

    TLInstance* object = FEFinder<TLInstance, 2>::Find<TLSlide>(
        activeslide,
        InlineHasher(nlStringLowerHash("group")),
        InlineHasher(nlStringLowerHash("homex")));
    mChooseSide.mControllerDestPos[0] = object->GetAssetPosition().f.x;
    object->m_bVisible = false;

    object = FEFinder<TLInstance, 2>::Find<TLSlide>(
        activeslide,
        InlineHasher(nlStringLowerHash("group")),
        InlineHasher(nlStringLowerHash("awayx")));
    mChooseSide.mControllerDestPos[1] = object->GetAssetPosition().f.x;
    object->m_bVisible = false;

    mChooseSide.mControllerDestPos[2] = mChooseSide.mInstanceTable[0]->GetAssetPosition().f.x;

    mChooseSide.mInstanceTable[16] = FEFinder<TLInstance, 4>::Find<TLSlide>(
        pPres->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("continue")));

    mChooseSide.ResetAndPositionControllers(false);
}

/**
 * Offset/Address/Size: 0x0 | 0x800C4BC0 | size: 0x350
 */
void SHChooseSides2::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    if (mProceedDelay > 0)
    {
        mProceedDelay--;

        if (mProceedDelay == 0)
        {
            Proceed();
            mProceedDelay = -1;
        }
    }
    else
    {
        if (mSoundDelay > 0.0f && mContext != PAUSE)
        {
            mSoundDelay -= fDeltaT;

            if (mSoundDelay <= 0.0f)
            {
                mSoundDelay = 0.0f;

                for (int i = 0; i < 2; i++)
                {
                    eTeamID teamid = nlSingleton<GameInfoManager>::Instance()->GetTeam((short)i);
                    FECharacterSound::PlayCaptainSlideIn(teamid);
                }
            }
        }

        if (g_bRenderWorld && mContext == PAUSE)
        {
            g_bRenderWorld = false;
        }

        for (int i = 0; i < 3; i++)
        {
            if (mAsyncImage[0][i] != NULL)
            {
                bool swapresult = mAsyncImage[0][i]->Update(true);

                if (swapresult && mContext == PAUSE)
                {
                    mAsyncImage[0][i]->FreeLoadBuffer();
                }
            }

            if (mAsyncImage[1][i] != NULL)
            {
                bool swapresult = mAsyncImage[1][i]->Update(true);

                if (swapresult && mContext == PAUSE)
                {
                    mAsyncImage[1][i]->FreeLoadBuffer();
                }
            }
        }

        TLSlide* currentSlide = m_pFEPresentation->m_currentSlide;

        if (currentSlide->m_time < (currentSlide->m_start + currentSlide->m_duration))
        {
            TLInstance* instance;
            for (int i = 0; i < 4; i++)
            {
                instance = mChooseSide.mInstanceTable[i];

                if (g_pFEInput->IsConnected((eFEINPUT_PAD)i))
                {
                    instance->m_bVisible = true;
                    mChooseSide.PositionController(i, false, true);
                }
                else
                {
                    instance->m_bVisible = false;
                    mChooseSide.mPlayingSides[i] = -1;

                    instance = mChooseSide.mInstanceTable[i + 8];
                    if (instance != NULL)
                    {
                        instance->m_bVisible = true;
                    }

                    instance = mChooseSide.mInstanceTable[i + 12];
                    if (instance != NULL)
                    {
                        instance->m_bVisible = false;
                    }

                    mChooseSide.PositionController(i, false, false);
                    mChooseSide.SetReady(i, false);
                }
            }
        }
        else
        {
            m_pTicker->Update(fDeltaT);
            UpdateChooseSideComponent(fDeltaT);
        }
    }
}
