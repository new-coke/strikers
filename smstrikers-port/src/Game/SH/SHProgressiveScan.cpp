#include "Game/SH/SHProgressiveScan.h"

#include "Game/FE/feInput.h"
#include "Game/GameSceneManager.h"
#include "NL/gl/glPlat.h"
#include "dolphin/os/OSRtc.h"
#include "Game/main.h"

/**
 * Offset/Address/Size: 0x1148 | 0x80111618 | size: 0x148
 */
ProgressiveScanScene::ProgressiveScanScene(bool doRGB60Instead)
{
    const char* MESSAGES_FILE_NAME = "art/fe/HealthSafetyUI.res";

    mConfirmationImage = NULL;
    mHasChoiceBeenMade = false;
    mUseProgressiveMode = true;
    mCanProceed = false;
    mFadingOut = false;
    mDoRGB60Instead = doRGB60Instead;
    mSelectorComponent = NULL;
    mElapsedTime = 0.0f;

    AsyncImage* useProgressiveImage0 = new (nlMalloc(sizeof(AsyncImage), 0x20, 1)) AsyncImage(MESSAGES_FILE_NAME, NULL);
    mUseProgressiveImage[0] = useProgressiveImage0;

    AsyncImage* useProgressiveImage1 = new (nlMalloc(sizeof(AsyncImage), 0x20, 1)) AsyncImage(MESSAGES_FILE_NAME, NULL);
    mUseProgressiveImage[1] = useProgressiveImage1;

    AsyncImage* confirmationImage = new (nlMalloc(sizeof(AsyncImage), 0x20, 1)) AsyncImage(MESSAGES_FILE_NAME, NULL);
    mConfirmationImage = confirmationImage;

    if (mDoRGB60Instead)
    {
        mUseProgressiveMode = OSGetEuRgb60Mode() != 0;
    }
    else
    {
        mUseProgressiveMode = true;
    }
}

/**
 * Offset/Address/Size: 0x107C | 0x8011154C | size: 0xCC
 */
ProgressiveScanScene::~ProgressiveScanScene()
{
    delete mUseProgressiveImage[0];
    delete mUseProgressiveImage[1];
    delete mConfirmationImage;
}

extern u32 nlStringLowerHash(const char*);
extern int nlSNPrintf(char*, unsigned long, const char*, ...);

/**
 * Offset/Address/Size: 0xD08 | 0x801111D8 | size: 0x374
 */
void ProgressiveScanScene::SceneCreated()
{

    FEPresentation* presentation = m_pFEPresentation;

    mSelectorComponent = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("highlite")));

    mSelectorComponent->m_bVisible = false;

    if (mUseProgressiveMode)
    {
        mSelectorComponent->SetActiveSlide("Slide1");
    }
    else
    {
        mSelectorComponent->SetActiveSlide("Slide2");
    }

    mSelectorComponent->Update(0.0f);

    TLImageInstance* img = FEFinder<TLImageInstance, 2>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ProgressiveScan_deu")));
    mUseProgressiveImage[0]->mImageInstance = img;
    img->m_bVisible = false;

    mUseProgressiveImage[1]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide3")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ProgressiveScan_deu")));

    mConfirmationImage->mImageInstance = FEFinder<TLImageInstance, 2>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide2")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ProgressiveScan_deu")));

    char texturePath[0x40] = "";
    const char* language;

    switch (g_Language)
    {
    case 0:
        language = "eng";
        break;
    case 1:
        language = "fre";
        break;
    case 2:
        language = "deu";
        break;
    case 3:
        language = "spa";
        break;
    case 4:
        language = "ita";
        break;
    case 5:
        language = "jpn";
        break;
    case 6:
        language = "uke";
        break;
    default:
        language = "eng";
        break;
    }

    nlSNPrintf(texturePath, sizeof(texturePath), "fe/health_and_safety/ProgressiveScan_%s", language);

    mUseProgressiveImage[0]->QueueLoad(texturePath, true);
    mUseProgressiveImage[1]->QueueLoad(texturePath, true);
}

/**
 * Offset/Address/Size: 0x940 | 0x80110E10 | size: 0x3C8
 */
void ProgressiveScanScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    if (mUseProgressiveImage[0]->Update(true))
    {
        TLImageInstance* img = FEFinder<TLImageInstance, 2>::Find<FEPresentation>(
            m_pFEPresentation,
            InlineHasher(nlStringLowerHash("Slide1")),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("ProgressiveScan_deu")));
        img->m_bVisible = true;
    }
    mUseProgressiveImage[1]->Update(true);
    mConfirmationImage->Update(true);
    if (mHasChoiceBeenMade && mFadingOut)
    {
        TLSlide* slide = m_pFEPresentation->m_currentSlide;
        if (slide->m_time < slide->m_start + slide->m_duration)
        {
            return;
        }
        mFadingOut = false;
        if (mUseProgressiveMode)
        {
            if (!mDoRGB60Instead)
            {
                OSSetProgressiveMode(1);
                glx_SetProgressiveMode();
            }
            else
            {
                OSSetEuRgb60Mode(1);
                glx_SetRGB60Mode();
            }
        }
        else
        {
            if (!mDoRGB60Instead)
            {
                OSSetProgressiveMode(0);
                glx_SetInterlacedMode();
            }
            else
            {
                OSSetEuRgb60Mode(0);
            }
            SwitchMessageImage();
            mCanProceed = true;
            mElapsedTime = 0.0f;
        }
    }
    if (!mHasChoiceBeenMade)
    {
        if ((mElapsedTime += fDeltaT) >= 0.5f)
        {
            mSelectorComponent->m_bVisible = true;
            if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 14, true, NULL) || g_pFEInput->IsAutoPressed(FE_ALL_PADS, 13, true, NULL))
            {
                mElapsedTime = 0.5f;
                mUseProgressiveMode = (mUseProgressiveMode != 1);
                if (mUseProgressiveMode)
                {
                    mSelectorComponent->SetActiveSlide("Slide1");
                    mSelectorComponent->Update(0.0f);
                }
                else
                {
                    mSelectorComponent->SetActiveSlide("Slide2");
                    mSelectorComponent->Update(0.0f);
                }
            }
            else
            {
                if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL) || mElapsedTime >= 10.5f)
                {
                    mHasChoiceBeenMade = true;
                    mFadingOut = true;
                    mElapsedTime = 0.0f;
                    mSelectorComponent->m_bVisible = false;
                    FEPresentation* pres = m_pFEPresentation;
                    pres->SetActiveSlide("Slide3");
                    pres->Update(0.0f);
                    return;
                }
            }
        }
    }
    if (!mCanProceed && mHasChoiceBeenMade && mUseProgressiveMode)
    {
        if ((mElapsedTime += fDeltaT) >= 3.0f)
        {
            SwitchMessageImage();
            mCanProceed = true;
            mElapsedTime = 0.0f;
        }
    }
    if (mCanProceed)
    {
        mElapsedTime += fDeltaT;
        if (mElapsedTime >= 3.5f || g_pFEInput->JustReleased(FE_ALL_PADS, 0x100, false, NULL) || g_pFEInput->JustReleased(FE_ALL_PADS, 0x200, false, NULL))
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_HEALTH_WARNING, SCREEN_NOTHING, true);
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801104D0 | size: 0x940
 */
void ProgressiveScanScene::SwitchMessageImage()
{
    const char* confirmationText = mUseProgressiveMode ? "Yes" : "No";

    BasicString<char, Detail::TempStringAllocator> languageString;
    FEPresentation* presentation = m_pFEPresentation;

    presentation->SetActiveSlide("Slide1");
    presentation->Update(0.0f);

    switch (g_Language)
    {
    case 0:
        languageString = "eng";
        break;
    case 1:
        languageString = "fre";
        break;
    case 2:
        languageString = "deu";
        break;
    case 3:
        languageString = "spa";
        break;
    case 4:
        languageString = "ita";
        break;
    case 5:
        languageString = "jpn";
        break;
    case 6:
        languageString = "uke";
        break;
    default:
        languageString = "eng";
        break;
    }

    char textureName[64];
    nlSNPrintf(textureName, sizeof(textureName), "fe/health_and_safety/Progressive%s_%s", confirmationText, languageString.c_str());
    textureName[sizeof(textureName) - 1] = '\0';

    mConfirmationImage->QueueLoad(textureName, true);
}
