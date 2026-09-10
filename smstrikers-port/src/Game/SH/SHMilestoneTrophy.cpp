#include "NL/nlFormat.h"
#include "Game/SH/SHMilestoneTrophy.h"

#include "Game/FE/FEAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feLibObject.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlLocalization.h"

typedef BasicString<unsigned short, Detail::TempStringAllocator> WideBasicString;
typedef BasicString<char, Detail::TempStringAllocator> CharBasicString;

extern unsigned long GetLOCTrophyName(eTrophyType);

static const char* MILESTONE_TROPHY_TEXT_NAME = "CUP TITLE";
static const char* MILESTONE_TROPHY_IMAGE_NAME = "TROPHY";
static const char* MILESTONE_STAT_TEXT_NAME = "THE TOTAL";
static const char* MILESTONE_TOTAL_TEXT_NAME = "TOTALS";
static const char* MILESTONE_DESCRIPTION_TEXT_NAME = "THE TOTAL2";

static const nlColour TROPHY_BLACK_MILESTONE = { 0x00, 0x00, 0x00, 0xFF };

static inline const unsigned short* LookupMilestoneTrophyLoc(unsigned long key)
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
 * Offset/Address/Size: 0x19F4 | 0x800CF4F4 | size: 0xCC
 */
MilestoneTrophyScene::MilestoneTrophyScene()
    : BaseSceneHandler()
    , mTrophy((eTrophyType)-1)
    , mCreated(false)
    , mIsNew(false)
    , mFirstSlideChange(true)
    , mDoBlockLoad(true)
    , mButtonState(ButtonComponent::BS_A_AND_B)
{
    const char* trophyImagePath = "art/fe/TrophiesUI.res";

    AsyncImage* trophyImage = new (nlMalloc(sizeof(AsyncImage), 0x20, true)) AsyncImage(trophyImagePath, NULL);
    mAsyncTrophy = trophyImage;
}

/**
 * Offset/Address/Size: 0x1950 | 0x800CF450 | size: 0xA4
 */
MilestoneTrophyScene::~MilestoneTrophyScene()
{
    delete mAsyncTrophy;
}

/**
 * Offset/Address/Size: 0x3CC | 0x800CDECC | size: 0x1584
 */
void MilestoneTrophyScene::SceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;

    int statAccumulated = 0;
    int statNeeded = 0;
    int bronzeStat = 0;
    int silverStat = 0;
    int goldStat = 0;
    TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(MILESTONE_TROPHY_TEXT_NAME)));

    pText->m_LocStrId = GetLOCTrophyName(mTrophy);
    pText->m_OverloadFlags |= 0x8;

    TLImageInstance* pTrophyImage = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(MILESTONE_TROPHY_IMAGE_NAME)));

    mAsyncTrophy->mImageInstance = pTrophyImage;
    pTrophyImage->m_bVisible = true;

    BasicString<unsigned short, Detail::TempStringAllocator> unformatted(LookupMilestoneTrophyLoc(0x3C479468));
    BasicString<unsigned short, Detail::TempStringAllocator> stat;
    BasicString<unsigned short, Detail::TempStringAllocator> unlockable;
    BasicString<unsigned short, Detail::TempStringAllocator> description;

    switch (mTrophy)
    {
    case TROPHY_VETERAN_CUP:
        statAccumulated = gameInfo->mUserInfo.mNumGamesPlayed;
        bronzeStat = 25;
        silverStat = 50;
        goldStat = 100;

        stat = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x8A5C9314));

        unlockable = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x759CD858));
        break;

    case TROPHY_SNIPER_CUP:
        statAccumulated = gameInfo->mUserInfo.mNumGoalsScored;
        bronzeStat = 75;
        silverStat = 150;
        goldStat = 300;

        stat = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x49772A70));

        unlockable = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0xE3FA84B4));
        break;

    case TROPHY_STRIKER_CUP:
        statAccumulated = gameInfo->mUserInfo.mNumSTSAttempts;
        bronzeStat = 25;
        silverStat = 50;
        goldStat = 100;

        stat = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x593E7EE3));

        unlockable = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x1D5A2367));
        break;

    case TROPHY_TACTICIAN_CUP:
        statAccumulated = gameInfo->mUserInfo.mNumPerfectPasses;
        bronzeStat = 75;
        silverStat = 150;
        goldStat = 300;

        stat = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x243FB12F));

        unlockable = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x1F42DEB3));
        break;

    case TROPHY_PARAMEDIC_CUP:
        statAccumulated = gameInfo->mUserInfo.mNumHits;
        bronzeStat = 250;
        silverStat = 500;
        goldStat = 1000;

        stat = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0xD9A2F4C5));

        unlockable = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0xAB6BFAC9));
        break;
    }

    BasicString<unsigned short, Detail::TempStringAllocator> formatted = Format(unformatted, stat.c_str());

    memcpy(mTotalBuffer, formatted.c_str(), 0x100);

    TLTextInstance* pTotal = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(MILESTONE_TOTAL_TEXT_NAME)));
    pTotal->SetString(mTotalBuffer);

    if (mIsNew == true)
    {
        TLComponentInstance* pComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("ARROWS")));
        pComp->m_bVisible = false;
    }

    eMilestoneColour levelReached = nlSingleton<GameInfoManager>::Instance()->GetMilestoneLevel(mTrophy);
    BasicString<char, Detail::TempStringAllocator> fileName(TROPHY_TEXTURE_FILENAMES[(int)mTrophy]);
    if (levelReached == MILESTONE_BLACK)
    {
        mAsyncTrophy->QueueLoad(fileName.c_str(), mDoBlockLoad);
        pTrophyImage->SetAssetColour(TROPHY_BLACK_MILESTONE);

        statNeeded = bronzeStat;
        unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x138E19E5));
    }
    else if (levelReached == MILESTONE_BRONZE)
    {
        fileName = fileName.Append("_bronze");
        mAsyncTrophy->QueueLoad(fileName.c_str(), mDoBlockLoad);
        nlColour colour = ((FELibObject*)pTrophyImage->m_component)->GetColour();
        pTrophyImage->SetAssetColour(colour);

        statNeeded = silverStat;
        unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x3A916A4A));
    }
    else if (levelReached == MILESTONE_SILVER)
    {
        fileName = fileName.Append("_silver");
        mAsyncTrophy->QueueLoad(fileName.c_str(), mDoBlockLoad);
        nlColour colour = ((FELibObject*)pTrophyImage->m_component)->GetColour();
        pTrophyImage->SetAssetColour(colour);

        statNeeded = goldStat;
        unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x0AD790FB));
    }
    else
    {
        mAsyncTrophy->QueueLoad(fileName.c_str(), mDoBlockLoad);
        nlColour colour = ((FELibObject*)pTrophyImage->m_component)->GetColour();
        pTrophyImage->SetAssetColour(colour);
    }
    mDoBlockLoad = false;

    if (nlSingleton<GameInfoManager>::Instance()->HasTrophy(mTrophy))
    {
        CharBasicString accumulatedString = LexicalCast<CharBasicString, int>(statAccumulated);

        unsigned short accumulatedWideString[128];
        nlStrToWcs(accumulatedString.c_str(), accumulatedWideString, 128);

        unformatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x59B161FF));

        formatted = Format(unformatted, accumulatedWideString);

        memcpy(mStatBuffer, formatted.c_str(), 0x100);

        pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(MILESTONE_STAT_TEXT_NAME)));
        pText->SetString(mStatBuffer);

        description = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0xF8710578));

        formatted = Format(description, unlockable);
    }
    else
    {
        CharBasicString accumulatedString = LexicalCast<CharBasicString, int>(statAccumulated);
        CharBasicString neededString = LexicalCast<CharBasicString, int>(statNeeded);

        unsigned short accumulatedWideString[128];
        unsigned short neededWideString[128];
        nlStrToWcs(accumulatedString.c_str(), accumulatedWideString, 128);
        nlStrToWcs(neededString.c_str(), neededWideString, 128);

        formatted = Format(unformatted, accumulatedWideString, neededWideString);

        memcpy(mStatBuffer, formatted.c_str(), 0x100);

        pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(MILESTONE_STAT_TEXT_NAME)));
        pText->SetString(mStatBuffer);

        CharBasicString bronzeString = LexicalCast<CharBasicString, int>(bronzeStat);
        CharBasicString silverString = LexicalCast<CharBasicString, int>(silverStat);
        CharBasicString goldString = LexicalCast<CharBasicString, int>(goldStat);

        unsigned short bronzeWideString[16];
        unsigned short silverWideString[16];
        unsigned short goldWideString[16];
        nlStrToWcs(bronzeString.c_str(), bronzeWideString, 16);
        nlStrToWcs(silverString.c_str(), silverWideString, 16);
        nlStrToWcs(goldString.c_str(), goldWideString, 16);

        description = BasicString<unsigned short, Detail::TempStringAllocator>(LookupMilestoneTrophyLoc(0x42FDAE95));

        formatted = Format(description, bronzeWideString, silverWideString, goldWideString);
    }

    memcpy(mDescriptionBuffer, formatted.c_str(), 0x100);

    pText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash(MILESTONE_DESCRIPTION_TEXT_NAME)));
    pText->SetString(mDescriptionBuffer);

    if (!mButtons.mAlreadyCentred)
    {
        TLComponentInstance* buttons = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash("IN")),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));
        mButtons.mButtonInstance = buttons;

        mButtons.SetState(mButtonState);
    }

    if (!mButtons2.mAlreadyCentred)
    {
        TLComponentInstance* buttons = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash("change")),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));
        mButtons2.mButtonInstance = buttons;

        mButtons2.SetState(mButtonState);
    }
}

/**
 * Offset/Address/Size: 0x188 | 0x800CDC88 | size: 0x244
 */
void MilestoneTrophyScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();
    mButtons2.CentreButtons();
    mAsyncTrophy->Update(true);

    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    bool canAccept = true;
    if (mButtonState != ButtonComponent::BS_A_AND_B && mButtonState != ButtonComponent::BS_A_ONLY)
    {
        canAccept = false;
    }

    bool canBack = true;
    if (mButtonState != ButtonComponent::BS_A_AND_B && mButtonState != ButtonComponent::BS_B_ONLY)
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
            nlSingleton<GameSceneManager>::Instance()->Pop();
            nlSingleton<GameInfoManager>::Instance()->DetermineNextCupScreen();
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

    if (mIsNew == false && g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0x0C, true, NULL))
    {
        if (mTrophy == TROPHY_PARAMEDIC_CUP)
        {
            mTrophy = TROPHY_VETERAN_CUP;
        }
        else
        {
            mTrophy = (eTrophyType)((int)mTrophy + 1);
        }

        ChangeSlides();
        FEAudio::PlayAnimAudioEvent("sfx_milestone_scroll_right", false);
        return;
    }

    if (mIsNew == false && g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0x0B, true, NULL))
    {
        if (mTrophy == TROPHY_VETERAN_CUP)
        {
            mTrophy = TROPHY_PARAMEDIC_CUP;
        }
        else
        {
            mTrophy = (eTrophyType)((int)mTrophy - 1);
        }

        ChangeSlides();
        FEAudio::PlayAnimAudioEvent("sfx_milestone_scroll_left", false);
    }
}

/**
 * Offset/Address/Size: 0x170 | 0x800CDC70 | size: 0x18
 */
void MilestoneTrophyScene::CreateTrophyScene(eTrophyType trophy, ButtonComponent::ButtonState state, bool isNew)
{
    mTrophy = trophy;
    mIsNew = isNew;
    mCreated = true;
    mButtonState = state;
}

/**
 * Offset/Address/Size: 0x0 | 0x800CDB00 | size: 0x170
 */
void MilestoneTrophyScene::ChangeSlides()
{
    FEPresentation* presentation = m_pFEPresentation;
    float starTime;

    if (mFirstSlideChange)
    {
        TLComponentInstance* starComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("star rotation")));
        starTime = starComp->GetActiveSlide()->m_time;
    }

    presentation->SetActiveSlide("CHANGE");
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
