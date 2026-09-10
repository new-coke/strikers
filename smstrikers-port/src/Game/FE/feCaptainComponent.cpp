#include "Game/FE/feCaptainComponent.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlSlide.h"

extern bool g_e3_Build;

static const char* const SLIDE_IN = "in";

/**
 * Offset/Address/Size: 0x1DF4 | 0x800BF790 | size: 0x14
 */
IChooseCaptain::IChooseCaptain()
{
    mIsSinglePlayerInput = true;
    mNumTotalPushedPlayers = 0;
}

/**
 * Offset/Address/Size: 0x1CE0 | 0x800BF67C | size: 0x114
 */
IChooseCaptain::~IChooseCaptain()
{
    // PORT: was a walk over `this` with hardcoded 0xC and 4 byte strides, three pointers and one pointer.
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            delete mAsyncImage[i][j];
            mAsyncImage[i][j] = NULL;
        }
    }

    delete mCaptainGridComponents[0];
    delete mCaptainGridComponents[1];
    delete mSidekickGridComponents[0];
    delete mSidekickGridComponents[1];
}

/**
 * Offset/Address/Size: 0x1B4C | 0x800BF4E8 | size: 0x194
 */
void IChooseCaptain::Initialize(const char* captainfilename, const char* sidekickfilename)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        mAsyncImage[i][0] = new (0x20, true) AsyncImage(captainfilename, NULL);
        mAsyncImage[i][1] = new (0x20, true) AsyncImage(captainfilename, NULL);
        mAsyncImage[i][2] = new (0x20, true) AsyncImage(captainfilename, NULL);
    }

    mAllPushedPlayers[0] = FE_ALL_PADS;
    mAllPushedPlayerSides[0] = -1;
    mAllPushedPlayers[1] = FE_ALL_PADS;
    mAllPushedPlayerSides[1] = -1;
    mAllPushedPlayers[2] = FE_ALL_PADS;
    mAllPushedPlayerSides[2] = -1;
    mAllPushedPlayers[3] = FE_ALL_PADS;
    mAllPushedPlayerSides[3] = -1;
    mNumTotalPushedPlayers = 0;

    mHomeAwayTeam[0] = nlSingleton<GameInfoManager>::Instance()->GetTeam(0);
    mHomeAwayTeam[1] = nlSingleton<GameInfoManager>::Instance()->GetTeam(1);
    mHomeAwaySidekicks[0] = nlSingleton<GameInfoManager>::Instance()->GetSidekick(0);
    mHomeAwaySidekicks[1] = nlSingleton<GameInfoManager>::Instance()->GetSidekick(1);

    mDidSwapCaptains[1] = false;
    mDidSwapCaptains[0] = false;
    mDidSwapSidekicks[1] = false;
    mDidSwapSidekicks[0] = false;

    mComponentState[0].mCurrentPhase = PHASE_IDLE;
    mComponentState[0].mParent = this;
    mComponentState[0].mHomeAway = 0;
    mComponentState[1].mCurrentPhase = PHASE_IDLE;
    mComponentState[1].mParent = this;
    mComponentState[1].mHomeAway = 1;

    mCaptainSoundDelay[0] = 0.0f;
    mCaptainSoundDelay[1] = 0.0f;

    mLastCaptainSelectSoundStrPlayed[0] = NULL;
    mLastCaptainSelectSoundStrPlayed[1] = NULL;
}

/**
 * Offset/Address/Size: 0x1AB0 | 0x800BF44C | size: 0x9C
 */
void IChooseCaptain::UpdateSound(float dt)
{
    for (s32 i = 0; i < 2; i++)
    {
        if (mCaptainSoundDelay[i] > 0.0f)
        {
            mCaptainSoundDelay[i] -= dt;
            if (mCaptainSoundDelay[i] <= 0.0f)
            {
                mCaptainSoundDelay[i] = 0.0f;
                FECharacterSound::PlayCaptainSlideIn((eTeamID)mHomeAwayTeam[i]);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x141C | 0x800BEDB8 | size: 0x694
 */
UpdateResult IChooseCaptain::Update(float dt)
{
    CheckForDisconnectedHumanPlayers();
    FindAliveHumanPlayers();

    UpdateSinglePlayerState();

    unsigned char goback;
    unsigned char isdoneanimating;

    for (int i = 0; i < 4; i++)
    {
        eFEINPUT_PAD inputpad = (eFEINPUT_PAD)i;
        int side = GetSide(i);

        if (side == -1)
        {
            continue;
        }

        if (g_pFEInput->JustPressed(inputpad, 0x200, false, NULL))
        {
            goback = 0;

            switch (mComponentState[side].mCurrentPhase)
            {
            case PHASE_READY:
                mComponentState[side].GotoPreviousPhase();
                break;

            case PHASE_CHOOSING_SIDEKICK:
                mComponentState[side].GotoPreviousPhase();
                break;

            case PHASE_CHOOSING_CAPTAIN:
                if (mIsSinglePlayerInput)
                {
                    if (side == 1 && mIsSinglePlayerInput)
                    {
                        mComponentState[1].GotoPreviousPhase();
                        mComponentState[0].GotoPreviousPhase();
                    }
                    else
                    {
                        goback = true;
                    }
                }
                else
                {
                    goback = true;
                }
                break;
            }

            if (goback)
            {
                FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", false);
                return UPDATE_GO_BACK;
            }
        }
        else if (g_pFEInput->JustPressed(inputpad, 0x100, false, &inputpad))
        {
            isdoneanimating = 1;

            switch (mComponentState[side].mCurrentPhase)
            {
            case PHASE_CHOOSING_CAPTAIN:
            {
                TLSlide* slide = mCaptainGridComponents[side]->mParentComponent->GetActiveSlide();
                unsigned char done;

                if (slide == NULL)
                {
                    done = 1;
                }
                else if (slide->m_time >= slide->m_start + slide->m_duration)
                {
                    done = 1;
                }
                else
                {
                    done = 0;
                }
                isdoneanimating = done;
                break;
            }

            case PHASE_CHOOSING_SIDEKICK:
            {
                TLSlide* slide = mSidekickGridComponents[side]->mParentComponent->GetActiveSlide();
                unsigned char done;

                if (slide == NULL)
                {
                    done = 1;
                }
                else if (slide->m_time >= slide->m_start + slide->m_duration)
                {
                    done = 1;
                }
                else
                {
                    done = 0;
                }
                isdoneanimating = done;
                break;
            }
            }

            if (isdoneanimating)
            {
                int side2 = GetSide(inputpad);

                mComponentState[side2].GotoNextPhase();

                if (mIsSinglePlayerInput && mComponentState[0].mCurrentPhase == PHASE_READY && mComponentState[1].mCurrentPhase == PHASE_IDLE)
                {
                    mComponentState[1].SetCurrentPhase(PHASE_CHOOSING_CAPTAIN);
                }
            }
        }
        else
        {
            switch (mComponentState[side].mCurrentPhase)
            {
            case PHASE_CHOOSING_CAPTAIN:
                mCaptainGridComponents[side]->Update(inputpad);
                if (mCaptainGridComponents[side]->mHasChangedSinceLastUpdate)
                {
                    mNameComponents[side].mComponent->SetActiveSlide("Slide1");
                    mNameComponents[side].mComponent->Update(0.0f);
                    mNameComponents[side].SetCaptainName(GetLOCCharacterName(mCaptainGridComponents[side]->GetSelectedItem(), false, true));
                    mNameComponents[side].SetCaptainLogo(GetTeamName(mCaptainGridComponents[side]->GetSelectedItem()));
                }
                break;

            case PHASE_CHOOSING_SIDEKICK:
                mSidekickGridComponents[side]->Update(inputpad);
                if (mSidekickGridComponents[side]->mHasChangedSinceLastUpdate)
                {
                    mNameComponents[side].mComponent->SetActiveSlide("Slide2");
                    mNameComponents[side].mComponent->Update(0.0f);
                    mNameComponents[side].SetCaptainName(GetLOCCharacterName((eTeamID)mHomeAwayTeam[side], false, false));
                    mNameComponents[side].SetSidekickName(GetLOCSidekickName(mSidekickGridComponents[side]->GetSelectedItem()));
                    mNameComponents[side].SetCaptainLogo(GetTeamName((eTeamID)mHomeAwayTeam[side]));
                }
                break;
            }
        }
    }

    if (mComponentState[0].mCurrentPhase == PHASE_READY && mComponentState[1].mCurrentPhase == PHASE_READY)
    {
        int playerIndex;
        GameInfoManager* const gim = nlSingleton<GameInfoManager>::s_pInstance;

        gim->SetTeam(0, (eTeamID)mHomeAwayTeam[0]);
        gim->SetTeam(1, (eTeamID)mHomeAwayTeam[1]);
        gim->SetSidekick(0, (eSidekickID)mHomeAwaySidekicks[0]);
        gim->SetSidekick(1, (eSidekickID)mHomeAwaySidekicks[1]);

        for (playerIndex = 0; playerIndex < mNumTotalPushedPlayers; playerIndex++)
        {
            gim->SetPlayingSide((unsigned short)mAllPushedPlayers[playerIndex], (short)mAllPushedPlayerSides[playerIndex]);
        }

        return UPDATE_GO_FORWARD;
    }

    UpdateAsyncImages();

    return UPDATE_OK;
}

/**
 * Offset/Address/Size: 0x12F0 | 0x800BEC8C | size: 0x12C
 */
void IChooseCaptain::UpdateAsyncImages()
{
    int j;
    int i;
    bool canswapcaptains;

    for (j = 0; j < 3; j++)
    {
        mAsyncImage[0][j]->Update(false);
        mAsyncImage[1][j]->Update(false);
    }

    for (i = 0; i < 2; i++)
    {
        if (mComponentState[i].mCurrentPhase != PHASE_READY)
        {
            mCaptainSoundDelay[i] = 0.0f;
        }
        else
        {
            canswapcaptains = false;
            if (!mDidSwapCaptains[i])
            {
                if (mAsyncImage[i][0]->CanSwapTextures() && mAsyncImage[i][1]->CanSwapTextures() && mAsyncImage[i][2]->CanSwapTextures())
                {
                    canswapcaptains = true;
                }
            }

            if (canswapcaptains)
            {
                mCaptainComponents[i]->SetActiveSlide("Slide1");
                mCaptainComponents[i]->m_bVisible = true;
                mAsyncImage[i][0]->Update(true);
                mAsyncImage[i][1]->Update(true);
                mAsyncImage[i][2]->Update(true);
                mDidSwapCaptains[i] = true;
                mCaptainSoundDelay[i] = mCaptainSlideDurations[0];
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xAB0 | 0x800BE44C | size: 0x840
 */
void IChooseCaptain::SceneCreated(FEPresentation* presentation)
{
    TLComponentInstance* compinstance;
    char filenameC2[0x80];
    char filenameC1[0x80];
    char filenameC0[0x80];
    char filenameS2[0x80];
    char filenameS1[0x80];
    char filenameS0[0x80];

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("LEFT_CAPT")));
    SetupCaptainComponent(compinstance, 0);
    compinstance->m_bVisible = false;

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("RIGHT_CAPT")));
    SetupCaptainComponent(compinstance, 1);
    compinstance->m_bVisible = false;

    {
        int team0 = mHomeAwayTeam[0];
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN, filenameC0, 0x80, team0, 0);
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_OUTLINE, filenameC1, 0x80, team0, 0);
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_FLASH, filenameC2, 0x80, team0, 0);
    }
    mAsyncImage[0][0]->QueueLoad(filenameC0, true);
    mAsyncImage[0][1]->QueueLoad(filenameC1, true);
    mAsyncImage[0][2]->QueueLoad(filenameC2, true);
    mDidSwapCaptains[0] = false;

    {
        int team1 = mHomeAwayTeam[1];
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN, filenameS0, 0x80, team1, 1);
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_OUTLINE, filenameS1, 0x80, team1, 1);
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_FLASH, filenameS2, 0x80, team1, 1);
    }
    mAsyncImage[1][0]->QueueLoad(filenameS0, true);
    mAsyncImage[1][1]->QueueLoad(filenameS1, true);
    mAsyncImage[1][2]->QueueLoad(filenameS2, true);
    mDidSwapCaptains[1] = false;

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("LEFT_SK")));
    mSidekickComponents[0] = compinstance;
    {
        TLSlide* slide = compinstance->GetActiveSlide();
        mSidekickSlideDurations[0] = (slide->m_start + slide->m_duration) / 2.0f;
    }
    compinstance->m_bVisible = false;
    mSidekickComponents[0]->m_bVisible = false;

    compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("RIGHT_SK")));
    mSidekickComponents[1] = compinstance;
    {
        TLSlide* slide = compinstance->GetActiveSlide();
        mSidekickSlideDurations[1] = (slide->m_start + slide->m_duration) / 2.0f;
    }
    compinstance->m_bVisible = false;
    mSidekickComponents[1]->m_bVisible = false;

    mCaptainGridComponents[0] = new (8, false) ICaptainGridComponent(
        FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CAPTAIN_CHOOSER_LEFT"))),
        false);

    mCaptainGridComponents[1] = new (8, false) ICaptainGridComponent(
        FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CAPTAIN_CHOOSER_RIGHT"))),
        true);

    mCaptainGridComponents[0]->BuildMapMenu();
    mCaptainGridComponents[1]->BuildMapMenu();

    mSidekickGridComponents[0] = new (8, false) ISidekickGridComponent(
        FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CHOOSE_SIDEKICKS_LEFT"))),
        false);

    mSidekickGridComponents[1] = new (8, false) ISidekickGridComponent(
        FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("CHOOSE_SIDEKICKS_RIGHT"))),
        true);

    mSidekickGridComponents[0]->mParentComponent->m_bVisible = false;
    mSidekickGridComponents[1]->mParentComponent->m_bVisible = false;
    mSidekickGridComponents[0]->BuildMapMenu();
    mSidekickGridComponents[1]->BuildMapMenu();

    mSidekickMiniHeadComponents[0] = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("sidekick icon left")));

    mSidekickMiniHeadComponents[1] = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("sidekick icon right")));

    mNameComponents[0].mComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME_LEFT")));

    mNameComponents[0].mCaptainObjName = "CAPTAIN_NAME";
    mNameComponents[0].mSidekickObjName = "SIDEKICK_NAME";

    mNameComponents[1].mComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME_RIGHT")));

    mNameComponents[1].mCaptainObjName = "CAPTAIN_NAME";
    mNameComponents[1].mSidekickObjName = "SIDEKICK_NAME";

    mComponentState[0].SetCurrentPhase(PHASE_CHOOSING_CAPTAIN);
    mComponentState[1].SetCurrentPhase(PHASE_IDLE);
}

/**
 * Offset/Address/Size: 0x890 | 0x800BE22C | size: 0x220
 */
void IChooseCaptain::SetupCaptainComponent(TLComponentInstance* compinstance, int homeaway)
{
    mCaptainComponents[homeaway] = compinstance;

    TLSlide* slide = compinstance->GetActiveSlide();
    mCaptainSlideDurations[homeaway] = (slide->m_start + slide->m_duration) / 2.0f;

    mAsyncImage[homeaway][0]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash((homeaway == 0) ? "CAPT_L" : "CAPT_R")));

    mAsyncImage[homeaway][1]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash((homeaway == 0) ? "CAPT_L_OUT" : "CAPT_R_OUT")));

    mAsyncImage[homeaway][2]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        compinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash((homeaway == 0) ? "CAPT_L_WHITE" : "CAPT_R_WHITE")));
}

/**
 * Offset/Address/Size: 0x70C | 0x800BE0A8 | size: 0x184
 */
void IChooseCaptain::StartSidekickMiniHead(int homeaway, eSidekickID sidekick)
{
    static const char* SidekickImageNames[] = {
        "choose_sidek_toad",
        "choose_sidek_koopa",
        "choose_sidek_hammer",
        "choose_sidek_birdo",
    };

    static const char* SidekickDestImageNames[] = {
        "sidekick left",
        "sidekick right",
    };

    FETextureResource* sourceres;
    TLComponentInstance* component;
    TLSlide* activeslide;
    TLImageInstance* sourceimage;
    TLImageInstance* destimage;

    if (sidekick == SK_MYSTERY)
    {
        mSidekickMiniHeadComponents[homeaway]->m_bVisible = false;
        return;
    }

    component = mSidekickMiniHeadComponents[homeaway];
    activeslide = component->GetActiveSlide();
    component->SetActiveSlide(activeslide);
    component->Update(0.0f);
    component->m_bVisible = true;

    TLComponentInstance* sourcecomp = mSidekickGridComponents[homeaway]->GetParentComponent();
    sourceimage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        sourcecomp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash(SidekickImageNames[sidekick])));
    sourceres = sourceimage->m_pTextureResource;

    destimage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        component->GetActiveSlide(),
        InlineHasher(nlStringLowerHash(SidekickDestImageNames[homeaway])));

    destimage->m_component->pChildren = (TLSlide*)sourceres;
}

/**
 * Offset/Address/Size: 0x670 | 0x800BE00C | size: 0x9C
 */
void IChooseCaptain::CheckForDisconnectedHumanPlayers()
{
    for (int i = 0; i < 4; i++)
    {
        if (IsPlayerPushed(i))
        {
            if (!g_pFEInput->IsConnected((eFEINPUT_PAD)i))
            {
                PopPlayer((eFEINPUT_PAD)i);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x4E0 | 0x800BDE7C | size: 0x190
 */
void IChooseCaptain::FindAliveHumanPlayers()
{
    for (int i = 0; i < 4; i++)
    {
        eFEINPUT_PAD pad = (eFEINPUT_PAD)i;

        if (g_pFEInput->IsAutoPressed(pad, 0xB, true, NULL) || g_pFEInput->IsAutoPressed(pad, 0xC, true, NULL)
            || g_pFEInput->IsAutoPressed(pad, 0xD, true, NULL) || g_pFEInput->IsAutoPressed(pad, 0xE, true, NULL)
            || g_pFEInput->JustPressed(pad, 0x100, false, NULL))
        {
            int numPushedPlayers = mNumTotalPushedPlayers;

            if (!IsPlayerPushed(i))
            {
                int side = numPushedPlayers & 1;
                if (mComponentState[side].mCurrentPhase == PHASE_IDLE)
                {
                    mComponentState[side].SetCurrentPhase(PHASE_CHOOSING_CAPTAIN);
                }

                mAllPushedPlayers[mNumTotalPushedPlayers] = (eFEINPUT_PAD)i;
                if (side != -1)
                {
                    mAllPushedPlayerSides[mNumTotalPushedPlayers] = side;
                }
                else
                {
                    mAllPushedPlayerSides[mNumTotalPushedPlayers] = mNumTotalPushedPlayers & 1;
                }

                mNumTotalPushedPlayers++;
            }
        }
    }
}

int IChooseCaptain::GetSide(int padid)
{
    if (mIsSinglePlayerInput)
    {
        if (mComponentState[0].mCurrentPhase < PHASE_READY)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        for (int i = 0; i < mNumTotalPushedPlayers; i++)
        {
            if (mAllPushedPlayers[i] == padid)
            {
                return mAllPushedPlayerSides[i];
            }
        }
        return -1;
    }
}

void IChooseCaptain::UpdateSinglePlayerState()
{
    int numSide1;
    int numSide0;
    mIsSinglePlayerInput = numSide0 = 0;

    if (mNumTotalPushedPlayers == 1)
    {
        mIsSinglePlayerInput = true;
    }
    else
    {
        int numSide1 = numSide0;
        for (int i = 0; i < mNumTotalPushedPlayers; i++)
        {
            if (mAllPushedPlayerSides[i] == 0)
            {
                numSide0++;
            }
            else if (mAllPushedPlayerSides[i] == 1)
            {
                numSide1++;
            }
        }
        if (numSide0 == 0 || numSide1 == 0)
        {
            mIsSinglePlayerInput = true;
        }
    }
}

/**
 * Offset/Address/Size: 0x388 | 0x800BDD24 | size: 0x158
 */
void IChooseCaptain::SetupForLastPhase(eFEINPUT_PAD pad)
{
    UpdateSinglePlayerState();

    if (mIsSinglePlayerInput)
    {
        mComponentState[1].GotoPreviousPhase();
        return;
    }

    if (pad == FE_ALL_PADS)
    {
        mComponentState[0].GotoPreviousPhase();
        mComponentState[1].GotoPreviousPhase();
        return;
    }

    int side = GetSide(pad);

    if (side == -1)
    {
        mComponentState[1].GotoPreviousPhase();
    }
    else
    {
        mComponentState[side].GotoPreviousPhase();
    }
}

/**
 * Offset/Address/Size: 0x338 | 0x800BDCD4 | size: 0x50
 */
void IChooseCaptain::PushPlayer(eFEINPUT_PAD pad, int side)
{
    mAllPushedPlayers[mNumTotalPushedPlayers] = pad;
    if (side != -1)
    {
        mAllPushedPlayerSides[mNumTotalPushedPlayers] = side;
    }
    else
    {
        mAllPushedPlayerSides[mNumTotalPushedPlayers] = mNumTotalPushedPlayers & 1;
    }
    mNumTotalPushedPlayers++;
}

/**
 * Offset/Address/Size: 0x1DC | 0x800BDB78 | size: 0x15C
 */
void IChooseCaptain::PopPlayer(eFEINPUT_PAD pad)
{
    int foundIndex = 0;
    int idx = 0;
    for (int i = 0; i < mNumTotalPushedPlayers; i++, idx++)
    {
        if (mAllPushedPlayers[i] == pad)
        {
            foundIndex = idx;
            break;
        }
    }

    for (int i = foundIndex; i < mNumTotalPushedPlayers - 1; i++)
    {
        mAllPushedPlayers[i] = mAllPushedPlayers[i + 1];
        mAllPushedPlayerSides[i] = mAllPushedPlayerSides[i + 1];
    }

    mNumTotalPushedPlayers--;
    UpdateSinglePlayerState();

    if (mNumTotalPushedPlayers != 0 && mIsSinglePlayerInput && mComponentState[1].mCurrentPhase != PHASE_READY)
    {
        if (mComponentState[0].mCurrentPhase != PHASE_READY)
        {
            mComponentState[1].SetCurrentPhase(PHASE_IDLE);
        }

        if (mNumTotalPushedPlayers == 1)
        {
            mAllPushedPlayerSides[0] = 0;
        }
    }
}

/**
 * Offset/Address/Size: 0x1A8 | 0x800BDB44 | size: 0x34
 */
void IChooseCaptain::ResetPushPlayerData()
{
    mAllPushedPlayers[0] = FE_ALL_PADS;
    mAllPushedPlayerSides[0] = -1;
    mAllPushedPlayers[1] = FE_ALL_PADS;
    mAllPushedPlayerSides[1] = -1;
    mAllPushedPlayers[2] = FE_ALL_PADS;
    mAllPushedPlayerSides[2] = -1;
    mAllPushedPlayers[3] = FE_ALL_PADS;
    mAllPushedPlayerSides[3] = -1;
    mNumTotalPushedPlayers = 0;
}

/**
 * Offset/Address/Size: 0xD8 | 0x800BDA74 | size: 0xD0
 */
void IChooseCaptain::PushPlayerWithGameInfoDB()
{
    int i;
    int side;

    for (i = 0; i < 4; i++)
    {
        side = nlSingleton<GameInfoManager>::Instance()->GetPlayingSide(i);
        if (g_pFEInput->IsConnected((eFEINPUT_PAD)i))
        {
            if (side != -1)
            {
                mAllPushedPlayers[mNumTotalPushedPlayers] = (eFEINPUT_PAD)i;
                if (side != -1)
                {
                    mAllPushedPlayerSides[mNumTotalPushedPlayers] = side;
                }
                else
                {
                    mAllPushedPlayerSides[mNumTotalPushedPlayers] = mNumTotalPushedPlayers & 1;
                }
                mNumTotalPushedPlayers++;
            }
        }
        else
        {
            nlSingleton<GameInfoManager>::Instance()->SetPlayingSide(i, -1);
        }
    }
}

/**
 * Offset/Address/Size: 0x8C | 0x800BDA28 | size: 0x4C
 */
void IChooseCaptain::MoveHighlightToCurrentCaptain(int which)
{
    if (which == 0 || which == 1)
    {
        mCaptainGridComponents[which]->MoveHighlightToTarget((eTeamID)mHomeAwayTeam[which]);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800BD99C | size: 0x8C
 */
void IChooseCaptain::SetupNameComponentToCurrentCaptain(int slot)
{
    if (slot == 0 || slot == 1)
    {
        mNameComponents[slot].SetCaptainName(GetLOCCharacterName((eTeamID)mHomeAwayTeam[slot], false, false));
        mNameComponents[slot].SetCaptainLogo(GetTeamName((eTeamID)mHomeAwayTeam[slot]));
    }
}
