// LOAD-BEARING INCLUDE ORDER: nlFormat.h must be included FIRST, ahead of
// SHCupTrophy.h. It is not needed here to compile -- it is what puts the
// nlLexicalCast.h and nlFormat.h linkonce groups ahead of nlFunction.h's, which
// is retail's group order. Moving it into sorted position still COMPILES but
// silently breaks the DOL (this TU is Matching). Do not let an include sort or
// IWYU pass move it.
#include "NL/nlFormat.h"
#include "Game/SH/SHCupTrophy.h"

#include "Game/GameSceneManager.h"
#include "Game/GameInfo.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/FE/tlComponentInstance.h"

#include "NL/nlAlgorithm.h"
#include "NL/nlBind.h"
#include "NL/nlFunction.h"
#include "NL/nlLocalization.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"

// Declared, never defined: retail references this specialization instead of
// emitting it (SHLoading.o owns the weak definition in both trees).
template <>
WideBasicString Format<WideBasicString, unsigned short[16]>(
    const WideBasicString& string, const unsigned short (&t0)[16]);

/**
 * Offset/Address/Size: 0x27B0 | 0x800CBE64 | size: 0xD4
 */
CupTrophyScene::CupTrophyScene()
    : mTrophy((eTrophyType)-1)
    , mCreated(false)
    , mIsNew(false)
    , mFirstSlideChange(true)
    , mScrollOffset(0)
    , mRow(0)
    , mDoBlockLoad(true)
    , mButtonState(ButtonComponent::BS_A_AND_B)
{
    const char* asyncPath = "art/fe/TrophiesUI.res";
    AsyncImage* image = new (nlMalloc(sizeof(AsyncImage), 0x20, true)) AsyncImage(asyncPath, NULL);
    mAsyncTrophy = image;
}

/**
 * Offset/Address/Size: 0x270C | 0x800CBDC0 | size: 0xA4
 */
CupTrophyScene::~CupTrophyScene()
{
    delete mAsyncTrophy;
}

static const char* CUP_TROPHY_TEXT_NAME = "CUP TITLE";
static const char* CUP_TROPHY_IMAGE_NAME = "Trophy";
static const char* CUP_FIRST_TEXT_NAME_LEFT = "FIRST WON TIME";
static const char* CUP_FIRST_TEXT_NAME_RIGHT = "FIRST WON TIME2";

static const char* TROPHY_RECORD_ROW_NAMES[3] = {
    "THE HISTORY",
    "THE HISTORY2",
    "THE HISTORY3",
};

static const nlColour SPOILS_COLOUR_HIGHLIGHT = { 0xFE, 0xEE, 0x00, 0xFF };
static const nlColour SPOILS_COLOUR_NORMAL = { 0xFF, 0xFF, 0xFF, 0xFF };
static const nlColour TROPHY_BLACK_CUP = { 0x00, 0x00, 0x00, 0xFF };

static inline const unsigned short* LookupCupTrophyLoc(unsigned long key)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* entry = nlBSearch(key, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry != 0)
    {
        return loc->m_FirstString + entry->StringOffset;
    }

    return MissingLocString;
}

/**
 * Offset/Address/Size: 0x1F54 | 0x800CB608 | size: 0x7B8
 */
void CupTrophyScene::SceneCreated()
{
    mScrollOffset = 0;
    mRow = 0;

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    UserInfo* userInfo = &nlSingleton<GameInfoManager>::Instance()->mUserInfo;
    Spoil* pSpoil = &userInfo->mSpoils[(int)mTrophy];

    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(CUP_TROPHY_TEXT_NAME)));

    pText->m_LocStrId = GetLOCTrophyName(mTrophy);
    pText->m_OverloadFlags |= 0x8;

    TLImageInstance* pTrophyImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(CUP_TROPHY_IMAGE_NAME)));

    mAsyncTrophy->mImageInstance = pTrophyImage;
    mAsyncTrophy->QueueLoad(TROPHY_TEXTURE_FILENAMES[(int)mTrophy], mDoBlockLoad);
    mDoBlockLoad = false;

    TLComponentInstance* arrowComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("ARROWS2")));
    if (pSpoil->mNumRecords <= 3)
    {
        arrowComp->m_bVisible = false;
    }
    else
    {
        arrowComp->m_bVisible = true;
    }

    if (mIsNew)
    {
        TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("ARROWS")));

        pComp->m_bVisible = false;
        nlSingleton<GameInfoManager>::Instance()->mDisplayTrophy[0] = false;
    }

    if (nlSingleton<GameInfoManager>::Instance()->HasTrophy(mTrophy))
    {
        nlColour trophyColour = ((FELibObject*)pTrophyImage->m_component)->GetColour();
        pTrophyImage->SetAssetColour(trophyColour);
    }
    else
    {
        pTrophyImage->SetAssetColour(TROPHY_BLACK_CUP);
    }

    SetWinRecord(*pSpoil);
    SetLossRecord(*pSpoil);
    SetHistory(*pSpoil);

    if (!mButtons.mAlreadyCentred)
    {
        mButtons.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash("IN")),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));

        mButtons.SetState(mButtonState);
    }

    if (!mButtons2.mAlreadyCentred)
    {
        mButtons2.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash("IN2")),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));

        mButtons2.SetState(mButtonState);
    }

    BasicString<char, Detail::TempStringAllocator> timesWon = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>((int)pSpoil->mNumCupWins);

    unsigned short timesWonWide[32];
    nlStrToWcs(timesWon.c_str(), timesWonWide, 32);

    BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupCupTrophyLoc(0x103642ED));
    BasicString<unsigned short, Detail::TempStringAllocator> formatted = Format(unformatted, timesWonWide);

    memcpy(mWonBuffer, formatted.c_str(), 0x100);

    TLTextInstance* pTotal = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("TOTAL CUPS")));
    pTotal->SetString(mWonBuffer);
}

/**
 * Offset/Address/Size: 0x1D70 | 0x800CB424 | size: 0x1E4
 */
void CupTrophyScene::HandleUnlockedTriggers()
{
    static const ePopupMenu PopupMap[] = {
        POPUP_UNLOCKED_KONGA_STADIUM,
        POPUP_UNLOCKED_YOSHI_STADIUM,
        POPUP_UNLOCKED_FORBIDDEN_STADIUM,
        POPUP_UNLOCKED_SUPER_CUPS,
        POPUP_UNLOCKED_SUPER_TEAM,
        POPUP_UNLOCKED_LEGEND_DIFFICULTY,
        POPUP_UNLOCKED_SUPER_STADIUM,
        POPUP_UNLOCKED_ALL_STS,
        POPUP_UNLOCKED_CHEAT_TILT,
        POPUP_UNLOCKED_CHEAT_INFINITE,
        POPUP_UNLOCKED_FLOWER_CUP,
        POPUP_UNLOCKED_STAR_CUP,
        POPUP_UNLOCKED_BOWSER_CUP,
        POPUP_UNLOCKED_CHEAT_GOALIE,
        NUM_POPUP_MENUS,
    };

    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;
    int i = 0;
    FEPopupMenu* popup;

    while (i < 0x20)
    {
        if ((gameInfo->mUnlockedTriggers & (1 << i)) != 0)
        {
            popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
            popup->Create(
                PopupMap[i],
                Bind<void>(MemFun<CupTrophyScene, void>(&CupTrophyScene::HandleUnlockedTriggers), this));

            gameInfo->mUnlockedTriggers &= ((unsigned int)-2 << i);
            return;
        }

        i++;
    }

    nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_BRAG, SCREEN_NOTHING, true);
}
extern FEInput* g_pFEInput;

/**
 * Offset/Address/Size: 0x1970 | 0x800CB024 | size: 0x400
 */
void CupTrophyScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();
    mButtons2.CentreButtons();
    mAsyncTrophy->Update(true);

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    UserInfo* userInfo = &nlSingleton<GameInfoManager>::Instance()->mUserInfo;
    Spoil* pSpoil = &userInfo->mSpoils[(int)mTrophy];
    bool canAccept = true;
    int buttonState = mButtonState;

    if (buttonState != ButtonComponent::BS_A_AND_B && buttonState != ButtonComponent::BS_A_ONLY)
    {
        canAccept = false;
    }

    bool canBack = true;
    if (buttonState != ButtonComponent::BS_A_AND_B && buttonState != ButtonComponent::BS_B_ONLY)
    {
        canBack = false;
    }

    TLSlide* slide = presentation->m_currentSlide;
    if (presentation->m_fadeDuration < slide->m_start + slide->m_duration)
    {
        return;
    }

    if (mIsNew == true && canAccept)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            if (nlSingleton<GameInfoManager>::Instance()->mUnlockedTriggers != 0)
            {
                HandleUnlockedTriggers();
            }
            else
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_BRAG, SCREEN_FORWARD, true);
            }

            FEAudio::PlayAnimAudioEvent("sfx_accept", false);
            return;
        }
    }

    if (mIsNew == false && canBack)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TROPHY_ROOM, SCREEN_BACK, true);
            FEAudio::PlayAnimAudioEvent("sfx_back", false);
            return;
        }
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xE, true, NULL))
    {
        if (pSpoil->mNumRecords > 3)
        {
            UserInfo* userInfo = &nlSingleton<GameInfoManager>::Instance()->mUserInfo;
            Spoil* pSpoil = &userInfo->mSpoils[(int)mTrophy];
            unsigned char numRecords = pSpoil->mNumRecords;

            if (numRecords <= 1)
            {
                return;
            }

            if (mRow < 2 && mRow < (int)(numRecords - 1))
            {
                mRow += 1;
                FEAudio::PlayAnimAudioEvent("sfx_option_scroll_down", false);
            }
            else if (numRecords > 3 && mScrollOffset < (int)(numRecords - 3))
            {
                mScrollOffset += 1;
                FEAudio::PlayAnimAudioEvent("sfx_option_scroll_down", false);
            }
            else
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }

            SetHistory(*pSpoil);
            return;
        }
    }

    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xD, true, NULL))
    {
        unsigned char numRecords = pSpoil->mNumRecords;
        if (numRecords > 3)
        {
            if (numRecords <= 1)
            {
                return;
            }

            if (mRow > 0)
            {
                mRow -= 1;
                FEAudio::PlayAnimAudioEvent("sfx_option_scroll_up", false);
            }
            else if (mScrollOffset > 0)
            {
                mScrollOffset -= 1;
                FEAudio::PlayAnimAudioEvent("sfx_option_scroll_up", false);
            }
            else
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }

            SetHistory(*pSpoil);
            return;
        }
    }

    if (mIsNew == false)
    {
        if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xC, true, NULL))
        {
            if (mTrophy == mLastTrophy)
            {
                mTrophy = mFirstTrophy;
            }
            else
            {
                mTrophy = (eTrophyType)((int)mTrophy + 1);
            }

            ChangeSlides();
            FEAudio::PlayAnimAudioEvent("sfx_cup_toggle_right", false);
            return;
        }
    }

    if (mIsNew == false)
    {
        if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xB, true, NULL))
        {
            if (mTrophy == mFirstTrophy)
            {
                mTrophy = mLastTrophy;
            }
            else
            {
                mTrophy = (eTrophyType)((int)mTrophy - 1);
            }

            ChangeSlides();
            FEAudio::PlayAnimAudioEvent("sfx_cup_toggle_left", false);
        }
    }
}

/**
 * Offset/Address/Size: 0x191C | 0x800CAFD0 | size: 0x54
 */
void CupTrophyScene::CreateTrophyScene(eTrophyType trophy, ButtonComponent::ButtonState buttonState, bool isNew)
{
    mTrophy = trophy;
    mIsNew = isNew;
    mCreated = 1;

    if (!mIsNew)
    {
        if (mTrophy < 4)
        {
            mFirstTrophy = TROPHY_MUSHROOM_CUP;
            mLastTrophy = TROPHY_BOWSER_CUP;
        }
        else
        {
            mFirstTrophy = TROPHY_SUPER_MUSHROOM_CUP;
            mLastTrophy = TROPHY_SUPER_BOWSER_CUP;
        }
    }

    mButtonState = buttonState;
}

/**
 * Offset/Address/Size: 0x1524 | 0x800CABD8 | size: 0x3F8
 */
void CupTrophyScene::SetWinRecord(Spoil& spoil)
{
    FEPresentation* presentation = GetPresentation();
    BasicString<char, Detail::TempStringAllocator> winString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(spoil.mNumWins);
    unsigned short winBuf[16];
    nlStrToWcs(winString.c_str(), winBuf, 16);

    BasicString<unsigned short, Detail::TempStringAllocator> msg(LookupCupTrophyLoc(0x49466bc6));
    BasicString<unsigned short, Detail::TempStringAllocator> formattedResult = Format(msg, winBuf);

    memcpy(mFirstWinBuffer, formattedResult.c_str(), 0x100);

    TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(CUP_FIRST_TEXT_NAME_LEFT)));

    text->SetString(mFirstWinBuffer);
}

/**
 * Offset/Address/Size: 0x112C | 0x800CA7E0 | size: 0x3F8
 */
void CupTrophyScene::SetLossRecord(Spoil& spoil)
{
    FEPresentation* presentation = GetPresentation();
    BasicString<char, Detail::TempStringAllocator> lossString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(spoil.mNumLosses);
    unsigned short lossBuf[16];
    nlStrToWcs(lossString.c_str(), lossBuf, 16);

    BasicString<unsigned short, Detail::TempStringAllocator> msg(LookupCupTrophyLoc(0x720DF6F9));
    BasicString<unsigned short, Detail::TempStringAllocator> formattedResult = Format(msg, lossBuf);

    memcpy(mHistoryBuffer, formattedResult.c_str(), 0x100);

    TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(CUP_FIRST_TEXT_NAME_RIGHT)));

    text->SetString(mHistoryBuffer);
}

/**
 * Offset/Address/Size: 0x170 | 0x800C9824 | size: 0xFBC
 */
void CupTrophyScene::SetHistory(Spoil& spoil)
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    for (int i = 0; i < 3; i++)
    {
        int record = i + mScrollOffset;
        TLSlide* currentSlide = m_pFEScene->m_pFEPackage->GetPresentation()->m_currentSlide;

        TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find(
            currentSlide, nlStringLowerHash("Layer"), nlStringLowerHash("HISTORY"));

        TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find(pComp->GetActiveSlide(), nlStringLowerHash(TROPHY_RECORD_ROW_NAMES[i]));

        nlVector2 boxSize;
        boxSize.x = 999.9f;
        boxSize.y = pText->m_OverloadedAttributes.BoxSize.y;
        pText->m_OverloadedAttributes.BoxSize = boxSize;
        pText->m_OverloadFlags |= 0x4;

        if (i == mRow && (spoil.mNumRecords == 0 || spoil.mNumRecords > 3))
            ((TLInstance*)pText)->SetAssetColour(SPOILS_COLOUR_HIGHLIGHT);
        else
            ((TLInstance*)pText)->SetAssetColour(SPOILS_COLOUR_NORMAL);

        if (record == 0 && spoil.mNumRecords == 0)
        {
            TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find(pComp->GetActiveSlide(), nlStringLowerHash(TROPHY_RECORD_ROW_NAMES[i]));
            pText->m_LocStrId = 0x745863BC;
            pText->m_OverloadFlags |= 0x8;
            ((TLInstance*)pText)->m_bVisible = true;
            continue;
        }

        if (i >= (int)spoil.mNumRecords)
        {
            ((TLInstance*)pText)->m_bVisible = false;
            continue;
        }

        ((TLInstance*)pText)->m_bVisible = true;

        BasicString<char, Detail::TempStringAllocator> zero("0");

        BasicString<char, Detail::TempStringAllocator> dayString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(spoil.mCupHistory[record].mDate.mday);
        dayString = (spoil.mCupHistory[record].mDate.mday < 10) ? zero.Append(dayString) : dayString;

        BasicString<char, Detail::TempStringAllocator> monthString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(spoil.mCupHistory[record].mDate.mon + 1);
        monthString = (spoil.mCupHistory[record].mDate.mon + 1 < 10) ? zero.Append(monthString) : monthString;

        BasicString<char, Detail::TempStringAllocator> yearString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(spoil.mCupHistory[record].mDate.year);

        unsigned short dayWideString[16];
        nlStrToWcs(dayString.c_str(), dayWideString, 16);
        unsigned short monthWideString[16];
        nlStrToWcs(monthString.c_str(), monthWideString, 16);
        unsigned short yearWideString[16];
        nlStrToWcs(yearString.c_str(), yearWideString, 16);

        BasicString<unsigned short, Detail::TempStringAllocator> formatted;

        if (spoil.mCupHistory[record].mPlace != -2)
        {
            BasicString<unsigned short, Detail::TempStringAllocator> unformatted = LookupCupTrophyLoc(0x4EEF03CB);

            formatted = Format(unformatted, monthWideString, dayWideString, yearWideString, LookupCupTrophyLoc(GetLOCCharacterName(spoil.mCupHistory[record].mTeam, true, false)), LookupCupTrophyLoc(GetLOCRank(spoil.mCupHistory[record].mPlace)), LookupCupTrophyLoc(GetLOCDifficultyName(spoil.mCupHistory[record].mDifficulty)));
        }
        else
        {
            BasicString<unsigned short, Detail::TempStringAllocator> unformatted = LookupCupTrophyLoc(0xB4B37E9E);

            formatted = Format(unformatted, monthWideString, dayWideString, yearWideString, LookupCupTrophyLoc(GetLOCCharacterName(spoil.mCupHistory[record].mTeam, true, false)), LookupCupTrophyLoc(GetLOCDifficultyName(spoil.mCupHistory[record].mDifficulty)));
        }

        memcpy(mRecordBuffer[i], formatted.c_str(), 0x100);
        pText->SetString(mRecordBuffer[i]);
    }

    TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find(
        presentation->m_currentSlide, nlStringLowerHash("Layer"), nlStringLowerHash("ARROWS2"));

    int currentRecord = mRow + mScrollOffset;

    TLImageInstance* pArrow = FEFinder<TLImageInstance, 2>::Find(pComp->GetActiveSlide(), nlStringLowerHash("arrow"));
    if (currentRecord < (int)spoil.mNumRecords - 1)
        pArrow->m_bVisible = true;
    else
        pArrow->m_bVisible = false;

    pArrow = FEFinder<TLImageInstance, 2>::Find(pComp->GetActiveSlide(), nlStringLowerHash("arrow2"));
    if (currentRecord > 0)
        pArrow->m_bVisible = true;
    else
        pArrow->m_bVisible = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x800C96B4 | size: 0x170
 */
void CupTrophyScene::ChangeSlides()
{
    FEPresentation* presentation = GetPresentation();
    float starTime;

    if (mFirstSlideChange)
    {
        TLComponentInstance* starComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("star rotation")));
        starTime = starComp->GetActiveSlide()->m_time;
    }

    presentation->SetActiveSlide("IN2");
    presentation->Update(0.0f);

    if (mFirstSlideChange)
    {
        TLComponentInstance* starComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("star rotation")));
        starComp->Update(starTime);
        mFirstSlideChange = false;
    }

    SceneCreated();
}
