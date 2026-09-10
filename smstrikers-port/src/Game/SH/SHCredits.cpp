#include "Game/SH/SHCredits.h"
#include "Game/Audio/AudioLoaderCore.h"
#include "Game/SH/SHMoviePlayer.h"
#include "Game/TrophyTextures.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/feInput.h"
#include "Game/FE/tlImageInstance.h"
#include "NL/nlMemory.h"

static char* CREDITS_LINE_NAMES[] = {
    "Line1",
    "Line2",
    "Line3",
    "Line4",
    "Line5",
    "Line6",
    "Line7",
    "Line8",
    "Line9",
    "Line10",
};

SceneList CreditScene::mNextScene = SCENE_MAIN_MENU;

/**
 * Offset/Address/Size: 0xBCC | 0x8010FD28 | size: 0xDC
 */
CreditScene::CreditScene()
    : mAreCreditsOver(false)
    , mFinalMessageDisplayed(false)
    , mFadeStarted(false)
    , mPhase(0)
{
    mTimeElapsed = 0.0f;

    for (int i = 0; i < 10; i++)
    {
        m_pTextLines[i] = NULL;
        mLineOnScreen[i] = false;
    }
}

/**
 * Offset/Address/Size: 0xB40 | 0x8010FC9C | size: 0x8C
 */
CreditScene::~CreditScene()
{
}

/**
 * Offset/Address/Size: 0xB20 | 0x8010FC7C | size: 0x20
 */
void CreditScene::SceneCreated()
{
    SetupForPhase();
}

/**
 * Offset/Address/Size: 0xA80 | 0x8010FBDC | size: 0xA0
 */
void CreditScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    switch (mPhase)
    {
    case 2:
        UpdateForCredits(fDeltaT);
        break;
    case 3:
        UpdateForCopyrightMessage(fDeltaT);
        break;
    case 1:
        UpdateForNintendoLogo(fDeltaT);
        break;
    case 0:
        UpdateForNLGMovie(fDeltaT);
        break;
    default:
        break;
    }
}

void CreditScene::DisplayFinalMessage()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("CREDITS")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Final Message")));
    pText->m_bVisible = true;

    mFinalMessageDisplayed = true;
}

/**
 * Offset/Address/Size: 0x978 | 0x8010FAD4 | size: 0x108
 */
void CreditScene::SetupForPhase()
{
    mFadeStarted = 0;
    mTimeElapsed = 0.0f;

    switch (mPhase)
    {
    case 0:
        SetupForNLGMovie();
        break;
    case 1:
        m_pFEPresentation->SetActiveSlide("NINTENDO");
        m_pFEPresentation->m_currentSlide->Update(0.0f);
        break;
    case 2:
        SetupForCredits();
        break;
    case 3:
        m_pFEPresentation->SetActiveSlide("COPYRIGHTS");
        m_pFEPresentation->m_currentSlide->Update(0.0f);
        break;
    case 4:
    {
        GameSceneManager::GetInstance()->Push(mNextScene, SCREEN_NOTHING, true);
        if (mNextScene == SCENE_OPTIONS)
        {
            FEMusic::StartStreamIfDifferent(7);
        }
        else
        {
            FEMusic::StartStreamIfDifferent(0);
        }
        mNextScene = SCENE_MAIN_MENU;
        break;
    }
    }
}

/**
 * Offset/Address/Size: 0x94C | 0x8010FAA8 | size: 0x2C
 */
void CreditScene::GotoNextPhase()
{
    mPhase++;
    SetupForPhase();
}

void CreditScene::SetupForCopyrightMessage()
{
    m_pFEPresentation->SetActiveSlide("COPYRIGHTS");
    m_pFEPresentation->m_currentSlide->Update(0.0f);
}

/**
 * Offset/Address/Size: 0x6E4 | 0x8010F840 | size: 0x268
 */
void CreditScene::SetupForCredits()
{
    m_pFEPresentation->SetActiveSlide("CREDITS");
    m_pFEPresentation->m_currentSlide->Update(0.0f);

    s32 yOffset;
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    TLTextInstance* pFinalText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("CREDITS")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Final Message")));
    pFinalText->m_bVisible = false;

    mCreditParser.mFileData = (char*)nlLoadEntireFile("credits.txt", &mCreditParser.mFileSize, 0x20, (eAllocType)1);
    mCreditParser.mParser.StartParsing(mCreditParser.mFileData, mCreditParser.mFileSize, false);

    nlVector2 boxsize = { 1280.0f, 480.0f };

    s32 i;
    for (i = 0, yOffset = 0; i < 10; i++, yOffset += 46)
    {
        m_pTextLines[i] = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash("CREDITS")),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(CREDITS_LINE_NAMES[i])));

        m_pTextLines[i]->SetAssetScale(0.75f, 0.75f, 1.0f);

        TLTextInstance* pText = m_pTextLines[i];
        pText->m_OverloadFlags |= 0x10;
        pText->m_DrawOptions |= 0x10;

        pText = m_pTextLines[i];
        pText->m_OverloadedAttributes.BoxSize = boxsize;
        pText->m_OverloadFlags |= 0x4;

        feVector3 position = m_pTextLines[i]->GetAssetPosition();
        m_pTextLines[i]->SetAssetPosition(position.f.x, (f32)(-230 - yOffset), position.f.z);
    }

    AudioLoader::StartFEStream("FE_Credits", true, "FE");
}

void CreditScene::SetupForNintendoLogo()
{
    m_pFEPresentation->SetActiveSlide("NINTENDO");
    m_pFEPresentation->m_currentSlide->Update(0.0f);
}

/**
 * Offset/Address/Size: 0x60C | 0x8010F768 | size: 0xD8
 */
void CreditScene::SetupForNLGMovie()
{
    MoviePlayerScene* pScene = (MoviePlayerScene*)GameSceneManager::GetInstance()->Push(SCENE_NLG_MOVIE, SCREEN_NOTHING, false);
    pScene->mPushWithPop = false;

    FESceneManager::GetInstance()->ForceImmediateStackProcessing();

    m_pFEPresentation->SetActiveSlide("NLG");

    TLImageInstance* pImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("A_button")));
    pImage->m_bVisible = false;
}

/**
 * Offset/Address/Size: 0x554 | 0x8010F6B0 | size: 0xB8
 */
void CreditScene::UpdateForCopyrightMessage(float fDeltaT)
{
    TLComponentInstance* pWhiteFade = GetWhiteFadeComponent();
    f32 timeElapsed = mTimeElapsed;
    timeElapsed += fDeltaT;
    mTimeElapsed = timeElapsed;
    if (timeElapsed < 3.0f)
    {
        return;
    }
    TLSlide* pSlide = pWhiteFade->GetActiveSlide();
    f32 slideEnd = pSlide->m_start + pSlide->m_duration;
    TLSlide* pSlide2 = pWhiteFade->GetActiveSlide();
    if (pSlide2->m_time >= slideEnd)
    {
        if (!mFadeStarted)
        {
            pWhiteFade->SetActiveSlide("FADEIN");
            mFadeStarted = 1;
        }
        else
        {
            GotoNextPhase();
        }
    }
}

static inline void CopyCreditLine(CreditScene& scene, s32 i, const char* pToken)
{
    if (pToken[0] == '+')
    {
        const unsigned char* pSrc;
        u32 count;
        count = 64;
        pSrc = (const unsigned char*)" ";
        s32 k = 0;
        while (count-- && (scene.mStrings[i][k] = *pSrc) != 0)
        {
            ++pSrc;
            ++k;
        }
        scene.mStrings[i][63] = 0;
    }
    else
    {
        const unsigned char* pSrc = (const unsigned char*)pToken;
        u32 count = 64;
        s32 ch = 0;
        while (count-- && (scene.mStrings[i][ch] = pSrc[ch]) != 0)
        {
            ch++;
        }
        scene.mStrings[i][63] = 0;
    }
}

/**
 * Offset/Address/Size: 0x1BC | 0x8010F318 | size: 0x398
 */
void CreditScene::UpdateForCredits(float fDeltaT)
{
    s32 movement = (s32)(460.0f * (fDeltaT / 5.0f));
    f32 resetY = -230.0f;
    s32 numonscreen = 0;

    for (s32 i = 0; i < 10; i++)
    {
        feVector3 position = m_pTextLines[i]->GetAssetPosition();

        if (position.f.y >= resetY && !mLineOnScreen[i])
        {
            bool hasToken;
            const char* pToken = mCreditParser.mParser.NextToken(false);

            if (pToken != NULL)
            {
                CopyCreditLine(*this, i, pToken);

                mCreditParser.mParser.AdvanceLine();
                hasToken = true;
            }
            else
            {
                hasToken = false;
            }

            if (hasToken)
            {
                m_pTextLines[i]->SetString(mStrings[i]);
                mLineOnScreen[i] = true;
                position.f.y += (float)movement;
                m_pTextLines[i]->SetAssetPosition(position.f.x, position.f.y, position.f.z);
            }
        }
        else
        {
            if (position.f.y >= 230.0f && mLineOnScreen[i] == true)
            {
                mLineOnScreen[i] = false;
                position.f.y = resetY;
                m_pTextLines[i]->SetAssetPosition(position.f.x, position.f.y, position.f.z);
            }
            else
            {
                position.f.y += (float)movement;
                m_pTextLines[i]->SetAssetPosition(position.f.x, position.f.y, position.f.z);
            }
        }

        if (mLineOnScreen[i])
        {
            numonscreen++;
        }
    }

    if (numonscreen == 0)
    {
        mAreCreditsOver = true;
    }

    if (!mFadeStarted)
    {
        bool quitcredits = false;

        if (mAreCreditsOver == true)
        {
            mTimeElapsed += fDeltaT;

            if (mTimeElapsed >= 0.0 && !mFinalMessageDisplayed)
            {
                quitcredits = true;
            }
        }
        else
        {
            if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x24, true, NULL)
                || g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
            {
                quitcredits = true;
            }
        }

        if (quitcredits)
        {
            mFadeStarted = 1;

            TLComponentInstance* pWhiteFade = GetWhiteFadeComponent();
            pWhiteFade->SetActiveSlide("FADEIN");
            pWhiteFade->Update(0.0f);
        }
    }
    else
    {
        TLComponentInstance* pWhiteFade = GetWhiteFadeComponent();
        TLSlide* pSlide = pWhiteFade->GetActiveSlide();
        f32 slideEnd = pSlide->m_start + pSlide->m_duration;

        if (pWhiteFade->GetActiveSlide()->m_time >= slideEnd)
        {
            if (mCreditParser.mFileData != NULL)
            {
                nlFree(mCreditParser.mFileData);
                mCreditParser.mFileData = NULL;
            }

            GotoNextPhase();
        }
    }
}

/**
 * Offset/Address/Size: 0x104 | 0x8010F260 | size: 0xB8
 */
void CreditScene::UpdateForNintendoLogo(float fDeltaT)
{
    TLComponentInstance* pWhiteFade = GetWhiteFadeComponent();
    f32 timeElapsed = mTimeElapsed;
    timeElapsed += fDeltaT;
    mTimeElapsed = timeElapsed;
    if (timeElapsed < 3.0f)
    {
        return;
    }
    TLSlide* pSlide = pWhiteFade->GetActiveSlide();
    f32 slideEnd = pSlide->m_start + pSlide->m_duration;
    TLSlide* pSlide2 = pWhiteFade->GetActiveSlide();
    if (pSlide2->m_time >= slideEnd)
    {
        if (!mFadeStarted)
        {
            pWhiteFade->SetActiveSlide("FADEIN");
            mFadeStarted = 1;
        }
        else
        {
            GotoNextPhase();
        }
    }
}

/**
 * Offset/Address/Size: 0x98 | 0x8010F1F4 | size: 0x6C
 */
void CreditScene::UpdateForNLGMovie(float fDeltaT)
{
    if (FESceneManager::GetInstance()->AreAllScenesValid())
    {
        GameSceneManager* pGameSceneMgr = GameSceneManager::GetInstance();
        BaseSceneHandler* pCurrentScene = pGameSceneMgr->GetCurrentScene();
        if (pCurrentScene == this)
        {
            GotoNextPhase();
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8010F15C | size: 0x98
 */
TLComponentInstance* CreditScene::GetWhiteFadeComponent()
{

    return FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("WHITE FADE")));
}
