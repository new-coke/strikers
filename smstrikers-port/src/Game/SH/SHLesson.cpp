#include "Game/SH/SHLesson.h"
#include "Game/SH/SHLessonSelect.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feManager.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/OverlayManager.h"
#include "Game/SH/SHMoviePlayer.h"
#include "NL/nlLocalization.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlPrint.h"
#include "NL/nlSingleton.h"
#include "types.h"

static inline const unsigned short* LookupLocString(const char* stringId)
{
    nlLocalization* loc = g_pLocalization;
    unsigned long key = nlStringLowerHash(stringId);

    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    if (nlLocalization::StringLookup* entry = nlBSearch<nlLocalization::StringLookup, unsigned long>(key, loc->m_LookupTable, (int)loc->m_pFile->StringCount))
    {
        return loc->m_FirstString + entry->StringOffset;
    }

    return MissingLocString;
}

int LessonScene::mLessonIndex = -1;

/**
 * Offset/Address/Size: 0x508 | 0x8010A9B4 | size: 0x6C
 */
LessonScene::LessonScene()
    : mHUDScene(NULL)
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x490 | 0x8010A93C | size: 0x78
 */
LessonScene::~LessonScene()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x1DC | 0x8010A688 | size: 0x2B4
 */
void LessonScene::SceneCreated()
{
    char title[64];
    char body[64];
    TLTextInstance* titletextinstance;
    TLTextInstance* bodytextinstance;
    TLComponentInstance* buttonComponent;

    nlSNPrintf(title, 64, "LOC_TUTORIAL_INSTRUCTION_TITLE_%d", mLessonIndex);
    nlSNPrintf(body, 64, "LOC_TUTORIAL_INSTRUCTION_BODY_%d", mLessonIndex);

    titletextinstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("title")));

    bodytextinstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("body")));

    if (LookupLocString(title) != 0)
    {
        titletextinstance->SetStringId(title);
    }

    if (LookupLocString(body) != 0)
    {
        bodytextinstance->SetStringId(body);
    }

    buttonComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtons.mButtonInstance = buttonComponent;
    mButtons.SetState(ButtonComponent::BS_A_AND_B_AND_Y);
    buttonComponent->m_bVisible = false;
}

/**
 * Offset/Address/Size: 0x8 | 0x8010A4B4 | size: 0x1D4
 */
void LessonScene::Update(float fDeltaT)
{
    MoviePlayerScene* scene;
    char filename[128];

    BaseSceneHandler::Update(fDeltaT);
    this->mButtons.CentreButtons();
    scene = (MoviePlayerScene*)nlSingleton<OverlayManager>::Instance()->GetCurrentScene();

    if (scene == (MoviePlayerScene*)this)
    {
        if (this->mButtons.mButtonInstance)
        {
            this->mButtons.mButtonInstance->m_bVisible = true;
        }
    }
    else
    {
        if (this->mButtons.mButtonInstance)
        {
            this->mButtons.mButtonInstance->m_bVisible = false;
        }
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        MoviePlayerScene* movieScene = (MoviePlayerScene*)nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_LESSON_MOVIE_PLAYER, SCREEN_FORWARD, false);
        if (mLessonIndex == 6 && g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
        {
            nlSNPrintf(filename, 128, "movies/lesson%djpn.thp", mLessonIndex);
        }
        else
        {
            nlSNPrintf(filename, 128, "movies/lesson%d.thp", mLessonIndex);
        }
        movieScene->SetMovieDetails(filename, true, false);
        movieScene->mNextScene = IGSCENE_LESSON;
        movieScene->mPushWithPop = false;
        g_pFEInput->PushExclusiveInputLock(movieScene, -1);
        FEAudio::PlayAnimAudioEvent("sfx_accept", NULL);
        return;
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        LessonSelectScene* lessonScene = (LessonSelectScene*)nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_LESSON_SELECT, SCREEN_BACK, true);
        lessonScene->mStartAnimAtEnd = true;
        FEAudio::PlayAnimAudioEvent("sfx_back", NULL);
        return;
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x800, false, NULL))
    {
        FrontEnd::ExitMenuState();
        FEAudio::PlayAnimAudioEvent("sfx_back", NULL);
        FEAudio::PlayAnimAudioEvent("sfx_screen_back", NULL);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8010A4AC | size: 0x8
 */
void LessonScene::SetLesson(int index)
{
    LessonScene::mLessonIndex = index;
}
