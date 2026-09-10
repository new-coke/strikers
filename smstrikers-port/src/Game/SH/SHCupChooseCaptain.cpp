#include "Game/SH/SHCupChooseCaptain.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "Game/FE/feHelpFuncs.h"
#include "NL/gl/glStruct.h"
#include "NL/nlFormat.h"
#include "NL/nlLocalization.h"
#include "NL/nlAlgorithm.h"

static inline const unsigned short* LookupCupCaptainLoc(unsigned long hash)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }
    nlLocalization::StringLookup* entry = nlBSearch(hash, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry != 0)
    {
        return loc->m_FirstString + entry->StringOffset;
    }
    return MissingLocString;
}

static unsigned long CAPTAIN_DESCRIPTIONS[] = {
    0xFF68ABBA,
    0xE2D37C19,
    0x000465BA,
    0x000BAD38,
    0x0043DF21,
    0x330C3072,
    0x00C0A242,
    0x00EC84AC,
    0x69BFAF9D,
};

/**
 * Offset/Address/Size: 0x237C | 0x800DF248 | size: 0x144
 */
CupChooseCaptainSceneV2::CupChooseCaptainSceneV2(bool isSuperCup)
    : mSidekickMiniHead(NULL)
    , mCurrentCaptain((eTeamID)3)
    , mCurrentSK((eSidekickID)0)
    , mState(CUP_STATE_CAPTAIN)
    , mAnimationDelay(0.75f)
    , mSoundDelay(0.0f)
    , mRemainingSoundDelay(0.0f)
    , mTicker(NULL)
    , mIsSuperCup(isSuperCup)
    , mCupStartString()
{
    const char* asyncPath = mIsSuperCup ? "art/fe/SuperCupLoadingScreensUI.res" : "art/fe/CupLoadingScreensUI.res";

    AsyncImage* image = new (0x20, true) AsyncImage(asyncPath, NULL);
    mCaptainImageMain = image;

    image = new (0x20, true) AsyncImage(asyncPath, NULL);
    mCaptainImageBG = image;

    image = new (0x20, true) AsyncImage(asyncPath, NULL);
    mCaptainImageFlash = image;
}

/**
 * Offset/Address/Size: 0x2168 | 0x800DF034 | size: 0x214
 */
CupChooseCaptainSceneV2::~CupChooseCaptainSceneV2()
{
    delete mCaptainImageMain;
    delete mCaptainImageBG;
    delete mCaptainImageFlash;
    delete mCaptainGrid;
    delete mSKGrid;

    if (mTicker)
    {
        delete mTicker;
    }
}

void CupChooseCaptainSceneV2::UpdateImages()
{
    char filename[128];
    char filenamebg[128];
    char filenameflash[128];
    char skfilename[128];
    char skfilenamebg[128];
    const char* captainname = GetTeamName(mCurrentCaptain);
    const char* sidekickname = GetSidekickName(mCurrentSK);

    if (!mIsSuperCup)
    {
        nlSNPrintf(filename, 0x80, "fe/cup_loadingscreens/%s_l", captainname);
        nlSNPrintf(filenamebg, 0x80, "fe/cup_loadingscreens/%s_l_bg", captainname);
        nlSNPrintf(filenameflash, 0x80, "fe/cup_loadingscreens/%s_l_white", captainname);
        nlSNPrintf(skfilename, 0x80, "fe/cup_loadingscreenssidekicks/%s_%s_l", sidekickname, captainname);
        nlSNPrintf(skfilenamebg, 0x80, "fe/cup_loadingscreenssidekicks/%s_%s_l_bg", sidekickname, captainname);
    }
    else
    {
        nlSNPrintf(filename, 0x80, "fe/supercup_loadingscreens/%s_l", captainname);
        nlSNPrintf(filenamebg, 0x80, "fe/supercup_loadingscreens/%s_l_bg", captainname);
        nlSNPrintf(filenameflash, 0x80, "fe/supercup_loadingscreens/%s_l_white", captainname);
        nlSNPrintf(skfilename, 0x80, "fe/supercup_loadingscreenssidekicks/%s_%s_l", sidekickname, captainname);
        nlSNPrintf(skfilenamebg, 0x80, "fe/supercup_loadingscreenssidekicks/%s_%s_l_bg", sidekickname, captainname);
    }

    mCaptainImageMain->QueueLoad(filename, false);
    mCaptainImageBG->QueueLoad(filenamebg, false);
    mCaptainImageFlash->QueueLoad(filenameflash, false);
}

/**
 * Offset/Address/Size: 0x1654 | 0x800DE520 | size: 0xB14
 */
void CupChooseCaptainSceneV2::SceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLSlide* slide = presentation->GetActiveSlide();

    TLComponentInstance* comp;

    comp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CHOOSE_SIDEKICKS_LEFT")));
    comp->m_bVisible = false;
    mComponents[3] = comp;

    comp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("LEFT_SK")));
    comp->m_bVisible = false;
    mComponents[4] = comp;

    comp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("LEFT_CAPT")));
    comp->m_bVisible = false;
    mComponents[2] = comp;

    {
        TLSlide* active = comp->GetActiveSlide();
        mSoundDelay = (active->m_start + active->m_duration) / 1.75f;
    }

    mComponents[1] = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN_CHOOSER_LEFT")));

    FEAudio::PlayAnimAudioEvent("sfx_character_group_left_enter", false);

    mComponents[0] = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME_RIGHT")));

    UpdateCaptainName();

    {
        TLTextInstance* scrollText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            slide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("TickerText")));

        const gl_ScreenInfo* screenInfo = glGetScreenInfo();

        FEScrollText* ticker = new (nlMalloc(sizeof(FEScrollText), 0x20, true)) FEScrollText(scrollText, 0, screenInfo->ScreenWidth + 0x32);
        mTicker = ticker;
        mTicker->SetDisplayMessage("CHOOSE_CAPTAIN_TICKER_CHOOSE_CAPTAIN");
    }

    {
        TLSlide* active = mComponents[2]->GetActiveSlide();

        TLImageInstance* image = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            active,
            InlineHasher(nlStringLowerHash("CAPT_L")));
        mCaptainImageMain->mImageInstance = image;

        image = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            active,
            InlineHasher(nlStringLowerHash("CAPT_L_WHITE")));
        mCaptainImageFlash->mImageInstance = image;

        image = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            active,
            InlineHasher(nlStringLowerHash("CAPT_L_OUT")));
        mCaptainImageBG->mImageInstance = image;
    }

    {
        TLSlide* active = mComponents[4]->GetActiveSlide();

        TLImageInstance* image = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            active,
            InlineHasher(nlStringLowerHash("SK_L")));
        image->m_bVisible = false;

        image = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            active,
            InlineHasher(nlStringLowerHash("SK_L_OUT")));
        image->m_bVisible = false;
    }

    mSidekickMiniHead = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("sidekick icon right")));
    mSidekickMiniHead->m_bVisible = false;

    mCaptainGrid = new (nlMalloc(sizeof(ICaptainGridComponent), 8, false)) ICaptainGridComponent(mComponents[1], false);
    mCaptainGrid->BuildMapMenu();

    mSKGrid = new (nlMalloc(sizeof(ISidekickGridComponent), 8, false)) ISidekickGridComponent(mComponents[3], false);
    mSKGrid->BuildMapMenu();

    UpdateImages();
    mNumImagesLoaded = 0;

    mPressAComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEScene->m_pFEPackage->GetPresentation()->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("continue")));
    mPressAComponent->m_bVisible = false;

    {
        GameInfoManager::eGameModes gameMode = nlSingleton<GameInfoManager>::Instance()->GetCurrentMode();
        mCupStartString = Format(
            BasicString<unsigned short, Detail::TempStringAllocator>(LookupCupCaptainLoc(0xB862AB94)),
            LookupCupCaptainLoc(GetLOCModeName(gameMode)));
    }

    mButtons.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));
    mButtons.SetState(ButtonComponent::BS_A_AND_B);
}

/**
 * Offset/Address/Size: 0x109C | 0x800DDF68 | size: 0x5B8
 */
void CupChooseCaptainSceneV2::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    if (mCaptainImageMain->Update(true))
    {
        mNumImagesLoaded++;
    }
    if (mCaptainImageBG->Update(true))
    {
        mNumImagesLoaded++;
    }
    if (mCaptainImageFlash->Update(true))
    {
        mNumImagesLoaded++;
    }

    static const char* lastCaptainSelectSoundStrPlayed = 0;

    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();
    TLSlide* currentSlide = pres->m_currentSlide;

    if (mRemainingSoundDelay > 0.0f)
    {
        mRemainingSoundDelay -= fDeltaT;
        if (mRemainingSoundDelay <= 0.0f)
        {
            mRemainingSoundDelay = 0.0f;
            FECharacterSound::PlayCaptainSlideIn(mCurrentCaptain);
        }
    }

    if (currentSlide->m_time >= 1.0)
    {
        mTicker->Update(fDeltaT);
    }
    mCaptainGrid->UpdateSuperTeamIconState();

    if (mAnimationDelay > 0.0f)
    {
        mAnimationDelay -= fDeltaT;
        return;
    }

    switch (mState)
    {
    case CUP_STATE_CAPTAIN:
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            if (mCurrentCaptain != TEAM_MYSTERY)
            {
                ChangeState(mState, CUP_STATE_SK);
                FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
                lastCaptainSelectSoundStrPlayed = FECharacterSound::PlayCaptainName(mCurrentCaptain);
            }
            else if (mCurrentCaptain == TEAM_MYSTERY && nlSingleton<GameInfoManager>::Instance()->IsSuperTeamUnlocked())
            {
                ChangeState(mState, CUP_STATE_READY);
                FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
                lastCaptainSelectSoundStrPlayed = FECharacterSound::PlayCaptainName(mCurrentCaptain);
            }
            else
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }
        }
        else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            if (mIsSuperCup)
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_OPTIONS_INITIAL_SUPER, SCREEN_BACK, true);
            }
            else
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_OPTIONS_INITIAL_CUP, SCREEN_BACK, true);
            }
            FEAudio::PlayAnimAudioEvent("sfx_back", false);
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
        break;
    }
    case CUP_STATE_SK:
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            ChangeState(mState, CUP_STATE_READY);
            FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
            if (lastCaptainSelectSoundStrPlayed)
            {
                FEAudio::StopAnimAudioEvent(lastCaptainSelectSoundStrPlayed);
                lastCaptainSelectSoundStrPlayed = NULL;
            }
            FECharacterSound::PlaySidekickName(mCurrentSK);
        }
        else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            ChangeState(mState, CUP_STATE_CAPTAIN);
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
        break;
    }
    case CUP_STATE_READY:
    {
        if (mNumImagesLoaded != 3)
        {
            mComponents[2]->SetActiveSlide(mComponents[2]->GetActiveSlide());
            mComponents[2]->Update(0.0f);
            mComponents[4]->SetActiveSlide(mComponents[4]->GetActiveSlide());
            mComponents[4]->Update(0.0f);
            if (mCurrentCaptain != TEAM_MYSTERY)
            {
                StartSidekickMiniHead(mCurrentSK);
                mSidekickMiniHead->m_bVisible = true;
            }
            else
            {
                mSidekickMiniHead->m_bVisible = false;
            }
        }
        else
        {
            if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
            {
                GameInfoManager* const pGameInfo = nlSingleton<GameInfoManager>::s_pInstance;
                pGameInfo->SetUserSelectedCupTeam(mCurrentCaptain);
                pGameInfo->SetUserSelectedCupSidekick(mCurrentSK);
                CreateLineup();
                pGameInfo->IncreaseRoundNumber();
                pGameInfo->SetResultsOfLastUserGame(RESULT_CUP_START);
                nlSingleton<GameSceneManager>::Instance()->Pop();
                SceneList nextScene = mIsSuperCup ? SCENE_SUPER_CUP_STANDINGS : SCENE_CUP_STANDINGS;
                BaseSceneHandler* hubScene = nlSingleton<GameSceneManager>::Instance()->Push(nextScene, SCREEN_FORWARD, false);
                *(u8*)((u8*)hubScene + 0x226) = 1;
                FEAudio::PlayAnimAudioEvent("sfx_accept", false);
            }
            else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
            {
                if (mCurrentCaptain != TEAM_MYSTERY)
                {
                    ChangeState(mState, CUP_STATE_SK);
                }
                else
                {
                    ChangeState(mState, CUP_STATE_CAPTAIN);
                }
                FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", false);
            }
        }
        break;
    }
    }
}

/**
 * Offset/Address/Size: 0xE9C | 0x800DDD68 | size: 0x200
 */
void CupChooseCaptainSceneV2::UpdateCaptainName()
{

    TLSlide* slide = mComponents[0]->GetActiveSlide();

    TLTextInstance* captainNameText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME")));

    captainNameText->m_LocStrId = GetLOCCharacterName(mCurrentCaptain, false, true);
    captainNameText->m_OverloadFlags |= 0x8;

    TLTextInstance* captainDescText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("CAPTAIN_NAME2")));

    if (mCurrentCaptain == 8 && !nlSingleton<GameInfoManager>::Instance()->IsSuperTeamUnlocked())
    {
        captainDescText->SetStringId("CUP_ATTR_MYSTERY_LOCKED");
    }
    else
    {
        captainDescText->m_LocStrId = CAPTAIN_DESCRIPTIONS[mCurrentCaptain];
        captainDescText->m_OverloadFlags |= 0x8;
    }

    TLComponentInstance* teamComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("COMPONENT")));

    teamComp->SetActiveSlide(GetTeamName(mCurrentCaptain));
}

/**
 * Offset/Address/Size: 0xC20 | 0x800DDAEC | size: 0x27C
 */
void CupChooseCaptainSceneV2::UpdateSKName()
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

    if (mCurrentCaptain == 8 && !nlSingleton<GameInfoManager>::Instance()->IsSuperTeamUnlocked())
    {
        captainDescText->SetStringId("CUP_ATTR_MYSTERY_LOCKED");
    }
    else
    {
        captainDescText->m_LocStrId = CAPTAIN_DESCRIPTIONS[mCurrentCaptain];
        captainDescText->m_OverloadFlags |= 0x8;
    }

    TLComponentInstance* teamComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("COMPONENT")));

    teamComp->SetActiveSlide(GetTeamName(mCurrentCaptain));
}

static inline void UpdateImages(CupChooseCaptainSceneV2* self)
{
    char filename[128];
    char filenamebg[128];
    char filenameflash[128];
    char skfilename[128];
    char skfilenamebg[128];
    const char* captainname = GetTeamName(self->mCurrentCaptain);
    const char* sidekickname = GetSidekickName(self->mCurrentSK);

    if (!self->mIsSuperCup)
    {
        nlSNPrintf(filename, 0x80, "fe/cup_loadingscreens/%s_l", captainname);
        nlSNPrintf(filenamebg, 0x80, "fe/cup_loadingscreens/%s_l_bg", captainname);
        nlSNPrintf(filenameflash, 0x80, "fe/cup_loadingscreens/%s_l_white", captainname);
        nlSNPrintf(skfilename, 0x80, "fe/cup_loadingscreenssidekicks/%s_%s_l", sidekickname, captainname);
        nlSNPrintf(skfilenamebg, 0x80, "fe/cup_loadingscreenssidekicks/%s_%s_l_bg", sidekickname, captainname);
    }
    else
    {
        nlSNPrintf(filename, 0x80, "fe/supercup_loadingscreens/%s_l", captainname);
        nlSNPrintf(filenamebg, 0x80, "fe/supercup_loadingscreens/%s_l_bg", captainname);
        nlSNPrintf(filenameflash, 0x80, "fe/supercup_loadingscreens/%s_l_white", captainname);
        nlSNPrintf(skfilename, 0x80, "fe/supercup_loadingscreenssidekicks/%s_%s_l", sidekickname, captainname);
        nlSNPrintf(skfilenamebg, 0x80, "fe/supercup_loadingscreenssidekicks/%s_%s_l_bg", sidekickname, captainname);
    }

    self->mCaptainImageMain->QueueLoad(filename, false);
    self->mCaptainImageBG->QueueLoad(filenamebg, false);
    self->mCaptainImageFlash->QueueLoad(filenameflash, false);
}

/**
 * Offset/Address/Size: 0x330 | 0x800DD1FC | size: 0x8F0
 */
void CupChooseCaptainSceneV2::ChangeState(CupChooseCaptainSceneV2::eCupCaptainState from, CupChooseCaptainSceneV2::eCupCaptainState to)
{

    TLComponentInstance* pCursorComp;
    if (from == CUP_STATE_CAPTAIN && to == CUP_STATE_SK)
    {
        mComponents[0]->SetActiveSlide("Slide2");
        UpdateSKName();
        mComponents[0]->Update(0.0f);

        mComponents[1]->SetActiveSlide("OUT");
        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_exit", false);
        mCaptainGrid->RebuildInstanceTable();
        mCaptainGrid->UpdateSuperTeamIconState();

        {

            pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                mComponents[1]->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        }

        pCursorComp->m_bVisible = false;

        mComponents[3]->SetActiveSlide("IN");
        mComponents[3]->Update(0.0f);
        mComponents[3]->m_bVisible = true;

        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_enter", false);

        mSKGrid->RebindHighliteComponent("HIGHLIGHT");
        mSKGrid->RebuildInstanceTable();
    }
    else if (from == CUP_STATE_CAPTAIN && to == CUP_STATE_READY)
    {
        mComponents[1]->SetActiveSlide("OUT");
        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_exit", false);

        {

            pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                mComponents[3]->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        }

        pCursorComp->m_bVisible = false;

        mComponents[2]->SetActiveSlide("Slide1");
        mComponents[2]->m_bVisible = true;
        mComponents[2]->Update(0.0f);

        mPressAComponent->m_bVisible = true;

        {
            ::UpdateImages(this);
        }

        mNumImagesLoaded = 0;
        mTicker->SetDisplayMessage(mCupStartString);
        mRemainingSoundDelay = mSoundDelay;
    }
    else if (from == CUP_STATE_SK && to == CUP_STATE_CAPTAIN)
    {
        mComponents[0]->SetActiveSlide("Slide1");
        mComponents[1]->SetActiveSlide("SELECT");
        mCaptainGrid->RebuildInstanceTable();
        mCaptainGrid->UpdateSuperTeamIconState();

        {

            pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                mComponents[1]->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        }

        pCursorComp->m_bVisible = true;
        mCaptainGrid->RebindHighliteComponent("HIGHLIGHT");
        mComponents[3]->m_bVisible = false;
    }
    else if (from == CUP_STATE_READY && to == CUP_STATE_CAPTAIN)
    {
        mComponents[1]->SetActiveSlide("SELECT");

        {

            pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                mComponents[3]->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        }

        pCursorComp->m_bVisible = true;

        mComponents[2]->m_bVisible = false;
        mPressAComponent->m_bVisible = false;
        mCaptainGrid->RebindHighliteComponent("HIGHLIGHT");

        mTicker->SetDisplayMessage(0x4B67A61F);
        mRemainingSoundDelay = 0.0f;
    }
    else if (from == CUP_STATE_SK && to == CUP_STATE_READY)
    {
        mComponents[3]->SetActiveSlide("OUT");
        FEAudio::PlayAnimAudioEvent("sfx_character_group_left_exit", false);

        {

            pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                mComponents[3]->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        }

        pCursorComp->m_bVisible = false;

        mComponents[2]->SetActiveSlide("Slide1");
        mComponents[2]->m_bVisible = true;
        mComponents[2]->Update(0.0f);

        mComponents[4]->SetActiveSlide("Slide1");
        mComponents[4]->m_bVisible = true;
        mComponents[4]->Update(0.0f);

        mPressAComponent->m_bVisible = true;

        {
            ::UpdateImages(this);
        }

        mNumImagesLoaded = 0;
        mTicker->SetDisplayMessage(mCupStartString);
        mRemainingSoundDelay = mSoundDelay;
    }
    else if (from == CUP_STATE_READY && to == CUP_STATE_SK)
    {
        mComponents[3]->SetActiveSlide("SELECT");

        {

            pCursorComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                mComponents[3]->GetActiveSlide(),
                InlineHasher(nlStringLowerHash("HIGHLIGHT")));
        }

        pCursorComp->m_bVisible = true;

        mSKGrid->RebindHighliteComponent("HIGHLIGHT");
        mSKGrid->RebuildInstanceTable();
        mComponents[2]->m_bVisible = false;
        mComponents[4]->m_bVisible = false;
        mPressAComponent->m_bVisible = false;

        mTicker->SetDisplayMessage(0x4B67A61F);
        mRemainingSoundDelay = 0.0f;
    }

    mState = to;
    mSidekickMiniHead->m_bVisible = false;
}

/**
 * Offset/Address/Size: 0x15C | 0x800DD028 | size: 0x1D4
 */
void CupChooseCaptainSceneV2::CreateLineup()
{
    eTeamID chosenCaptain;
    GameInfoManager* pGameInfo;
    eSidekickID chosenSidekick;
    int numPlayingTeams;
    u32 slot;

    chosenCaptain = mCurrentCaptain;
    chosenSidekick = mCurrentSK;

    numPlayingTeams = (pGameInfo = nlSingleton<GameInfoManager>::Instance())->GetNumPlayingTeams();
    slot = nlRandom((u32)numPlayingTeams, &nlDefaultSeed);

    eTeamID lineup[8];
    eSidekickID sklineup[8];

    int i;
    for (i = 0; i < numPlayingTeams; i++)
    {
        lineup[i] = (eTeamID)-1;
        sklineup[i] = (eSidekickID)-1;
    }

    lineup[slot] = chosenCaptain;
    sklineup[slot] = chosenSidekick;

    for (i = 0; i < numPlayingTeams; i++)
    {
        if ((u32)i != slot)
        {
            eTeamID teamChoice;
            u8 notAlreadyChosen;
            do
            {
                if (pGameInfo->IsInSuperCupMode())
                {
                    teamChoice = (eTeamID)nlRandom(9, &nlDefaultSeed);
                }
                else
                {
                    teamChoice = (eTeamID)nlRandom(8, &nlDefaultSeed);
                }

                notAlreadyChosen = 1;
                for (int k = 0; k < numPlayingTeams; k++)
                {
                    if (teamChoice == lineup[k])
                    {
                        notAlreadyChosen = 0;
                        break;
                    }
                }
            } while (!notAlreadyChosen);

            lineup[i] = teamChoice;
            sklineup[i] = (eSidekickID)nlRandom(4, &nlDefaultSeed);
        }
    }

    pGameInfo->SetupRoundRobinSchedule(lineup, sklineup);
}

/**
 * Offset/Address/Size: 0x0 | 0x800DCECC | size: 0x15C
 */
void CupChooseCaptainSceneV2::StartSidekickMiniHead(eSidekickID sidekickId)
{
    static const char* SidekickImageNames[] = {
        "choose_sidek_toad",
        "choose_sidek_koopa",
        "choose_sidek_hammer",
        "choose_sidek_birdo",
    };

    if (sidekickId == SK_MYSTERY)
    {
        mSidekickMiniHead->m_bVisible = false;
        return;
    }

    TLComponentInstance* parentComp = mSKGrid->mParentComponent;
    TLImageInstance* sourceimage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        parentComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash(SidekickImageNames[sidekickId])));
    FETextureResource* sourceres = sourceimage->m_pTextureResource;

    TLImageInstance* destimage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        mSidekickMiniHead->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("sidekick right")));

    destimage->m_component->pChildren = (TLSlide*)sourceres;

    TLSlide* activeslide = mSidekickMiniHead->GetActiveSlide();
    mSidekickMiniHead->SetActiveSlide(activeslide);
    mSidekickMiniHead->Update(0.0f);
    mSidekickMiniHead->m_bVisible = true;
}
