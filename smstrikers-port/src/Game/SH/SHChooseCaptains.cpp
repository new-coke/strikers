#include "Game/SH/SHChooseCaptains.h"

#include "Game/BaseGameSceneManager.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feManager.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/GameSceneManager.h"
#include "Game/TrophyTextures.h"
#include "Game/main.h"
#include "NL/nlConfig.h"
#include "NL/nlLocalization.h"
#include "NL/nlPrint.h"


/**
 * Offset/Address/Size: 0xE64 | 0x800D78AC | size: 0x9C
 */
ChooseCaptainsSceneV2::ChooseCaptainsSceneV2(ChooseCaptainsSceneV2::SceneType scenetype)
    : mSceneType(scenetype)
    , mDesiredSceneType(scenetype)
    , mTicker(NULL)
    , mMoveForwardFrameDelay(-1)
{
}

/**
 * Offset/Address/Size: 0xD20 | 0x800D7768 | size: 0x144
 */
ChooseCaptainsSceneV2::~ChooseCaptainsSceneV2()
{
    if (mTicker != NULL)
    {
        delete mTicker;
    }
}

/**
 * Offset/Address/Size: 0xC28 | 0x800D7670 | size: 0xF8
 */
void ChooseCaptainsSceneV2::SceneCreated()
{
    mChooseCaptain.Initialize("art/fe/LoadingScreensUI.res", "art/fe/LoadingScreensSidekicksUI.res");
    mChooseCaptain.SceneCreated(m_pFEPresentation);

    if (mDesiredSceneType == ST_CHOOSE_SIDES)
    {
        mChooseCaptain.SetPhaseReady(0);
        mChooseCaptain.SetPhaseReady(1);
    }
    else
    {
        eFEINPUT_PAD pad = nlSingleton<GameInfoManager>::Instance()->mMainUserPadNumber;
        if (g_pFEInput->IsConnected(pad))
        {
            mChooseCaptain.PushPlayer(pad, -1);
        }

        GameInfoManager* gim = nlSingleton<GameInfoManager>::s_pInstance;
        gim->mCurGameGameplayOptions.SkillLevel = gim->mUserInfo.mGameplayOptions.SkillLevel;
        gim->mCurGameGameplayOptions.GameTime = gim->mUserInfo.mGameplayOptions.GameTime;
        gim->mCurGameGameplayOptions.PowerUps = gim->mUserInfo.mGameplayOptions.PowerUps;
        gim->mCurGameGameplayOptions.Shoot2Score = gim->mUserInfo.mGameplayOptions.Shoot2Score;
        gim->mCurGameGameplayOptions.BowserAttackEnabled = gim->mUserInfo.mGameplayOptions.BowserAttackEnabled;
        gim->mCurGameGameplayOptions.RumbleEnabled = gim->mUserInfo.mGameplayOptions.RumbleEnabled;
    }

    BindChooseSideInstances();
    CreateTicker();
    ChangeSceneType(mDesiredSceneType);
}

/**
 * Offset/Address/Size: 0xBE0 | 0x800D7628 | size: 0x48
 */
void ChooseCaptainsSceneV2::ChangeSceneType(ChooseCaptainsSceneV2::SceneType newtype)
{
    mSceneType = newtype;
    switch (mSceneType)
    {
    case ST_CHOOSE_CAPTAINS:
        ResetForCHOOSECAPTAINS();
        break;
    case ST_CHOOSE_SIDES:
        ResetForCHOOSESIDES();
        break;
    }
}

/**
 * Offset/Address/Size: 0xA7C | 0x800D74C4 | size: 0x164
 */
void ChooseCaptainsSceneV2::ResetForCHOOSECAPTAINS()
{
    TLComponentInstance* compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CHOOSE_SIDE")));

    compinstance->m_bVisible = false;
    mTicker->SetDisplayMessage((unsigned long)0x4B67A61F);

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtons.mButtonInstance = compinstance;
    mButtons.SetState(ButtonComponent::BS_A_AND_B);

    mChooseCaptain.MoveHighlightToCurrentCaptain(0);
    mChooseCaptain.SetupNameComponentToCurrentCaptain(0);
    mChooseCaptain.MoveHighlightToCurrentCaptain(1);

    if (mChooseCaptain.mNumTotalPushedPlayers > 1)
    {
        mChooseCaptain.SetupNameComponentToCurrentCaptain(1);
    }
}

/**
 * Offset/Address/Size: 0x930 | 0x800D7378 | size: 0x14C
 */
void ChooseCaptainsSceneV2::ResetForCHOOSESIDES()
{
    TLComponentInstance* compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CHOOSE_SIDE")));

    compinstance->m_bVisible = true;

    mTicker->SetDisplayMessage((unsigned long)0x53B23764);

    if (!nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode && !g_e3_Build)
    {
        compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));

        compinstance->m_bVisible = false;
        mButtons.mButtonInstance = compinstance;
        mButtons.SetState(ButtonComponent::BS_A_AND_B_AND_Y);
    }
}

/**
 * Offset/Address/Size: 0x560 | 0x800D6FA8 | size: 0x3D0
 */
void ChooseCaptainsSceneV2::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();
    mChooseCaptain.UpdateSound(fDeltaT);
    FEPresentation* pPresentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLSlide* pCurrentSlide = pPresentation->m_currentSlide;

    if (mMoveForwardFrameDelay > 0)
    {
        mMoveForwardFrameDelay--;
        if (mMoveForwardFrameDelay != 0)
            return;
        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            nlSingleton<GameSceneManager>::Instance()->PushLoadingScene(true);
            nlSingleton<GameInfoManager>::Instance()->SetStadium((eStadiumID)0);
        }
        else if (g_e3_Build)
        {
            nlSingleton<GameSceneManager>::Instance()->PushLoadingScene(true);
            nlSingleton<GameInfoManager>::Instance()->SetStadium((eStadiumID)3);
        }
        else
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_STADIUM_SELECT, SCREEN_FORWARD, true);
        }
        mChooseSide.SaveChanges();
        FrontEnd::SetControllerState();
        mMoveForwardFrameDelay = -1;
        return;
    }

    if (pCurrentSlide->m_time >= 1.0)
    {
        mTicker->Update(fDeltaT);
    }

    if (pCurrentSlide->m_time <= 1.15 && mSceneType == ST_CHOOSE_SIDES)
    {
        mChooseCaptain.UpdateAsyncImages();
        return;
    }

    switch (mSceneType)
    {
    case ST_CHOOSE_CAPTAINS:
        switch (mChooseCaptain.Update(fDeltaT))
        {
        case UPDATE_GO_BACK:
            nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, SCREEN_BACK, false);
            return;
        case UPDATE_GO_FORWARD:
            ChangeSceneType(ST_CHOOSE_SIDES);
            mChooseSide.ResetAndPositionControllers(true);
        default:
            return;
        }
    case ST_CHOOSE_SIDES:
    {
        eFEINPUT_PAD pad = FE_ALL_PADS;
        if (!nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x800, false, NULL))
            {
                if (!g_e3_Build)
                {
                    nlSingleton<GameSceneManager>::Instance()->Push(SCENE_QUICK_GAMEPLAY_OPTIONS, SCREEN_FORWARD, false);
                    return;
                }
            }
        }
        UpdateResult result = mChooseSide.Update(fDeltaT, &pad, -1);
        switch (result)
        {
        case UPDATE_GO_BACK:
            ChangeSceneType(ST_CHOOSE_CAPTAINS);
            mChooseSide.SaveChanges();
            mChooseCaptain.ResetPushPlayerData();
            mChooseCaptain.PushPlayerWithGameInfoDB();
            mChooseCaptain.SetupForLastPhase(pad);
            break;
        case UPDATE_GO_FORWARD:
        {
            if (mChooseSide.AllPlayersReady() || mChooseSide.AllPluggedInAreReady() || GetConfigBool(Config::Global(), "no_humans", false))
            {
                TLInstance* instance = mChooseSide.mInstanceTable[16];
                if (instance != NULL)
                {
                    instance->m_bVisible = false;
                }
                mMoveForwardFrameDelay = 2;
                return;
            }
            if (!mChooseSide.AtLeastOnePlayerReady())
            {
                FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
                popup->Create(POPUP_NO_SIDES_CHOSEN);
            }
            break;
        }
        }
        mChooseCaptain.UpdateAsyncImages();
        return;
    }
    }
}

/**
 * Offset/Address/Size: 0xE4 | 0x800D6B2C | size: 0x47C
 */
void ChooseCaptainsSceneV2::BindChooseSideInstances()
{
    FEPresentation* pPres = GetPresentation();
    nlColour colour;

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

        if (mChooseSide.mInstanceTable[i + 4] != NULL)
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

        nlColourSet(colour, PAD_COLOURS[i][0], PAD_COLOURS[i][1], PAD_COLOURS[i][2], 0xFF);
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

    mChooseSide.ResetAndPositionControllers(mChooseCaptain.mNumTotalPushedPlayers != 1);
}

/**
 * Offset/Address/Size: 0x0 | 0x800D6A48 | size: 0xE4
 */
void ChooseCaptainsSceneV2::CreateTicker()
{
    FEPresentation* pres = m_pFEPresentation;
    TLTextInstance* textinstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pres->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("TickerText")));

    const gl_ScreenInfo* screenInfo = glGetScreenInfo();
    mTicker = new (nlMalloc(sizeof(FEScrollText), 8, false)) FEScrollText(textinstance, 0, screenInfo->ScreenWidth + 0x32);
}

void ChooseCaptainsSceneV2::DestroyTicker()
{
    if (mTicker != NULL)
    {
        delete mTicker;
    }
}
