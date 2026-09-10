#include "Game/SH/SHLessonSelect.h"
#include "Game/OverlayManager.h"
#include "Game/SH/SHLesson.h"
#include "Game/FE/feNSNMessenger.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/tlTextInstance.h"

#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlBasicString.h"

#include "Game/SH/SHPause.h"

#include "NL/nlBind.h"

typedef Detail::MemFunImpl<void, void (LessonSelectScene::*)()> MemFunImpl_LessonSelect_t;
typedef BindExp1<void, MemFunImpl_LessonSelect_t, LessonSelectScene*> BindExp1_LessonSelect_t;

static int sRowOffset;
static int sCurrentRow;

typedef void FnTLComponentInstanceCb(TLComponentInstance*);

/**
 * Offset/Address/Size: 0x1830 | 0x8010C680 | size: 0x24
 */
void LessonTickerDoneCB()
{
    SetTickerLesson(-1);
}

/**
 * Offset/Address/Size: 0x1774 | 0x8010C5C4 | size: 0xBC
 */
LessonSelectScene::LessonSelectScene()
    : BaseSceneHandler()
    , mMenuItems()
    , mDoSlideIn(true)
    , mStartAnimAtEnd(false)
    , mButtons()
    , mUpArrow(NULL)
    , mDownArrow(NULL)
{
}

/**
 * Offset/Address/Size: 0x16D0 | 0x8010C520 | size: 0xA4
 */
LessonSelectScene::~LessonSelectScene()
{
}

/**
 * Offset/Address/Size: 0xE98 | 0x8010BCE8 | size: 0x838
 */
void LessonSelectScene::SceneCreated()
{
    MenuItem<TLComponentInstance>* menuItem;
    FEPresentation* presentation = m_pFEPresentation;
    FEAudio::EnableSounds(false);

    typedef MenuItem<TLComponentInstance>::Callback MenuCallback;

    for (int i = 0; i < 4; i++)
    {
        char menuname[64];
        nlSNPrintf(menuname, 64, "MENU ITEM%d", i + 1);

        TLComponentInstance* compinstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuname)));

        compinstance->SetActiveSlide(i == 0 ? DoubleHighlite::SLIDE_IN : DoubleHighlite::SLIDE_OUT);

        if (mDoSlideIn)
        {
            menuItem = mMenuItems.AddItem(compinstance);

            {
                MenuCallback openFunc(DoubleHighlite::OpenItem);
                menuItem->SetCallback(ON_HIGHLIGHT, openFunc);
            }

            {
                MenuCallback closeFunc(DoubleHighlite::CloseItem);
                menuItem->SetCallback(ON_UNHIGHLIGHT, closeFunc);
            }

            {
                MenuCallback applyFunc(Bind<void, MemFunImpl_LessonSelect_t, LessonSelectScene*>(
                    MemFun<LessonSelectScene, void>(&LessonSelectScene::StartLesson), this));
                menuItem->SetCallback(ON_APPLY, applyFunc);
            }

            if (i == 0)
            {
                DoubleHighlite::TempDisableSound();
            }

            menuItem->RunCallback((i == 0) ? ON_HIGHLIGHT : ON_UNHIGHLIGHT);

            TLSlide* slide = compinstance->GetActiveSlide();
            compinstance->Update(slide->m_start + slide->m_duration);
        }

        if (i == sCurrentRow)
        {
            DoubleHighlite::TempDisableSound();
            DoubleHighlite::OpenItem(compinstance);
        }
        else
        {
            DoubleHighlite::CloseItem(compinstance);
        }
    }

    DoubleHighlite::TempDisableSound();

    int newIndex = sCurrentRow - sRowOffset;

    mMenuItems.SetItem(newIndex);

    for (int i = 0; i < 4; i++)
    {
        UpdateRow(i, false);
    }

    TLComponentInstance* tempComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ARROWS")));

    mUpArrow = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        tempComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrow")));

    mDownArrow = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        tempComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("arrow2")));

    if (sCurrentRow == 0)
    {
        mUpArrow->m_bVisible = false;
        mDownArrow->m_bVisible = true;
    }
    else if (sCurrentRow == 11)
    {
        mUpArrow->m_bVisible = true;
        mDownArrow->m_bVisible = false;
    }
    else
    {
        mUpArrow->m_bVisible = true;
        mDownArrow->m_bVisible = true;
    }

    TLComponentInstance* buttonComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtons.mButtonInstance = buttonComponent;
    mButtons.SetState(ButtonComponent::BS_A_AND_B);

    FEAudio::EnableSounds(true);
}

/**
 * Offset/Address/Size: 0x7E0 | 0x8010B630 | size: 0x6B8
 */
void LessonSelectScene::Update(float fDeltaT)
{
    if (mStartAnimAtEnd)
    {
        m_pFEPresentation->m_fadeDuration = 999.9f;
        mStartAnimAtEnd = false;
    }

    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        mMenuItems.RunCallbackOnCurrent(ON_APPLY);
        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        PauseMenuScene* pauseScene = (PauseMenuScene*)nlSingleton<OverlayManager>::Instance()->Push(
            IGSCENE_STRIKERS_101_PAUSE, SCREEN_BACK, true);
        pauseScene->mStartAnimAtEnd = true;
        FEAudio::PlayAnimAudioEvent("sfx_back", false);
        sRowOffset = 0;
        sCurrentRow = 0;
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        FEAudio::EnableSounds(false);

        MenuResult result = mMenuItems.PreviousItem();

        FEAudio::EnableSounds(true);

        sCurrentRow = sRowOffset + mMenuItems.GetActiveItemIndex();
        bool updatearrows = true;

        if (result == RES_NOT_CHANGED && sRowOffset > 0)
        {
            sRowOffset = sRowOffset - 1;
            sCurrentRow = sRowOffset + mMenuItems.GetActiveItemIndex();
            mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
        }
        else
        {
            if (result == RES_NOT_CHANGED)
            {
                updatearrows = false;
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }
            else if (result == RES_OK)
            {
                mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
            }
        }

        if (updatearrows)
        {
            for (int i = 0; i < 4; i++)
            {
                UpdateRow(i, false);
            }
        }
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        FEAudio::EnableSounds(false);

        MenuResult result = mMenuItems.NextItem();

        FEAudio::EnableSounds(true);

        sCurrentRow = sRowOffset + mMenuItems.GetActiveItemIndex();
        bool updatearrows = true;

        if (result == RES_NOT_CHANGED && (sRowOffset + 3) < 11)
        {
            sRowOffset = sRowOffset + 1;
            sCurrentRow = sRowOffset + mMenuItems.GetActiveItemIndex();
            mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
        }
        else
        {
            if (result == RES_NOT_CHANGED)
            {
                updatearrows = false;
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }
            else if (result == RES_OK)
            {
                mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
            }
        }

        if (updatearrows)
        {
            for (int i = 0; i < 4; i++)
            {
                UpdateRow(i, false);
            }
        }
    }

    if (sCurrentRow == 0)
    {
        mUpArrow->m_bVisible = false;
        mDownArrow->m_bVisible = true;
    }
    else if (sCurrentRow == 11)
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
 * Offset/Address/Size: 0x334 | 0x8010B184 | size: 0x4AC
 */
void LessonSelectScene::UpdateRow(int onScreenRow, bool playsound)
{
    if (!playsound)
    {
        FEAudio::EnableSounds(false);
    }

    int currentRow = onScreenRow + sRowOffset;
    FEPresentation* presentation = m_pFEPresentation;
    char menuname[64];
    nlSNPrintf(menuname, 64, "MENU ITEM%d", onScreenRow + 1);

    TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(menuname)));

    pComp->SetActiveSlide("IN");
    TLSlide* slide = pComp->GetActiveSlide();
    pComp->Update(slide->m_start + slide->m_duration);

    TLTextInstance* pText1 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Text")));

    TLTextInstance* pNameText1 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("pauseresume")));

    pComp->SetActiveSlide("OUT");
    slide = pComp->GetActiveSlide();
    pComp->Update(slide->m_start + slide->m_duration);

    TLTextInstance* pText2 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Text")));

    TLTextInstance* pNameText2 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("pauseresume")));

    if (onScreenRow == sCurrentRow - sRowOffset)
    {
        pComp->SetActiveSlide("IN");
        if (!playsound)
        {
            DoubleHighlite::TempDisableSound();
        }
        mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
    }

    BasicString<char, Detail::TempStringAllocator> rowString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(currentRow + 1);

    const char* rowStringC = rowString.c_str();
    unsigned short* numBuf = mNumberBuffers[onScreenRow];
    nlStrToWcs(rowStringC, numBuf, 16);

    pText1->SetString(numBuf);
    pText2->SetString(numBuf);

    char lessonTitleName[64];
    nlSNPrintf(lessonTitleName, 64, "TUTORIAL_INSTRUCTION_TITLE_%d", currentRow + 1);
    pNameText1->SetStringId(lessonTitleName);
    pNameText2->SetStringId(lessonTitleName);

    if (!playsound)
    {
        FEAudio::EnableSounds(true);
    }
}

/**
 * Offset/Address/Size: 0x2E4 | 0x8010B134 | size: 0x50
 */
void LessonSelectScene::StartLesson()
{
    OverlayManager::Instance()->Push(IGSCENE_LESSON, SCREEN_FORWARD, true);
    LessonScene::SetLesson(sCurrentRow + 1);
    SetTickerLesson(sCurrentRow);
}

/**
 * Offset/Address/Size: 0x0 | 0x8010AE50 | size: 0x2E4
 */
void SetTickerLesson(int lesson)
{
    static unsigned char ResetHistory = 1;
    static signed char PreviousHistory[5];
    static signed char InsertPoint = 0;

    NSNMessengerScene* ticker;
    int randomlesson;
    char lessonTickerName[64];

    if (ResetHistory)
    {
        PreviousHistory[0] = -1;
        PreviousHistory[1] = -1;
        PreviousHistory[2] = -1;
        PreviousHistory[3] = -1;
        PreviousHistory[4] = -1;
        ResetHistory = 0;
    }

    BaseSceneHandler* scene = OverlayManager::Instance()->GetScene(OVERLAY_LESSON_TICKER);
    if (scene != 0)
    {
        scene = (BaseSceneHandler*)((char*)scene - 4);
    }

    ticker = (NSNMessengerScene*)(void*)scene;
    if (ticker == 0)
    {
        return;
    }

    if (lesson < 0)
    {
        do
        {
            randomlesson = nlRandom(12, &nlDefaultSeed);

            signed char* previous = PreviousHistory;
            for (int i = 0; i < 5; i++, previous++)
            {
                if (*previous == randomlesson)
                {
                    randomlesson = -1;
                    break;
                }
            }
        } while (randomlesson == -1);

        PreviousHistory[InsertPoint] = randomlesson;
        lesson = randomlesson;
        InsertPoint = (InsertPoint + 1) % 5;

        Function<FnVoidVoid> doneCB(LessonTickerDoneCB);

        FEScrollText* scrollText = ticker->m_scrollText;
        if (scrollText != 0)
        {
            scrollText->m_messageFinishedCB = doneCB;
        }
    }
    else
    {
        FEScrollText* scrollText = ticker->m_scrollText;
        if (scrollText != 0)
        {
            scrollText->m_messageFinishedCB.Clear();
        }
    }

    nlSNPrintf(lessonTickerName, 64, "TUTORIAL_INSTRUCTION_TICKER_%d", lesson + 1);
    ticker->EnableScrolling(true);
    ticker->SetDisplayMessage(lessonTickerName);
    ticker->OpenMessengerNow();
}
