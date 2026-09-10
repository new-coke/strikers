#include "Game/OverlayHandlerHUD.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/DB/StatsTracker.h"
#include "Game/EventDataTypes.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/Team.h"
#include "Game/Sys/eventman.h"
#include "Game/FE/tlComponentInstance.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlFormat.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlLocalization.h"
#include "NL/nlPrint.h"

static inline const unsigned short* LookupLocHash(unsigned long key)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* entry = nlBSearch<nlLocalization::StringLookup, unsigned long>(
        key, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry)
    {
        return loc->m_FirstString + entry->StringOffset;
    }

    return MissingLocString;
}

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
unsigned char g_hudVisible = 1;
static char* LEFT_POWER_UP_IMAGE_NAMES[2] = {
    "left_powerup1",
    "left_powerup2",
};
static char* RIGHT_POWER_UP_IMAGE_NAMES[2] = {
    "right_powerup1",
    "right_powerup2",
};
static char* LEFT_FLARE_IMAGE_NAMES[2] = {
    "left_flare1",
    "left_flare2",
};
static char* RIGHT_FLARE_IMAGE_NAMES[2] = {
    "right_flare1",
    "right_flare2",
};
static char* LEFT_POWER_UP_TEXT_NAMES[2] = {
    "POWERUP NUMBER LEFT 1",
    "POWERUP NUMBER LEFT 2",
};
static char* RIGHT_POWER_UP_TEXT_NAMES[2] = {
    "POWERUP NUMBER RIGHT 1",
    "POWERUP NUMBER RIGHT 2",
};
static const char* HUD_SLIDE_IN_NAME = "IN";
static const char* HUD_SLIDE_OUT_NAME = "OUT";
static const char* LAYER_NAME = "Layer";

#define FIND_IMAGE_PRESENTATION(presentation, name, slideName) FEFinder<TLImageInstance, 2>::Find<FEPresentation>( \
    presentation,                                                                                                  \
    InlineHasher(nlStringLowerHash(slideName)),                                                                    \
    InlineHasher(nlStringLowerHash(LAYER_NAME)),                                                                   \
    InlineHasher(nlStringLowerHash(name)))

#define FIND_COMPONENT_PRESENTATION(presentation, name, slideName) FEFinder<TLComponentInstance, 4>::Find<FEPresentation>( \
    presentation,                                                                                                          \
    InlineHasher(nlStringLowerHash(slideName)),                                                                            \
    InlineHasher(nlStringLowerHash(LAYER_NAME)),                                                                           \
    InlineHasher(nlStringLowerHash(name)))

/**
 * Offset/Address/Size: 0x36B8 | 0x800F9998 | size: 0xB4
 */
HUDOverlay::HUDOverlay()
    : BaseOverlayHandler(2)
{
    mSeconds = -1;
    mTenths = 0;
    mClockColourChanged = false;
    mOvertimeSFXPlayed = false;
    mStartScoreAnimation = false;
    mNumFlareCycles[0][0] = -1;
    mScore[0] = 0;
    mNewScore[0] = 0;
    mScoreUpdateDelay[0] = 0.0f;
    mNumFlareCycles[0][1] = -1;
    mScore[0] = 0;
    mNewScore[0] = 0;
    mScoreUpdateDelay[0] = 0.0f;
    mNumFlareCycles[0][2] = -1;
    mScore[1] = 0;
    mNewScore[1] = 0;
    mScoreUpdateDelay[1] = 0.0f;
    mNumFlareCycles[0][3] = -1;
    mScore[1] = 0;
    mNewScore[1] = 0;
    mScoreUpdateDelay[1] = 0.0f;
}

/**
 * Offset/Address/Size: 0x35FC | 0x800F98DC | size: 0xBC
 */
HUDOverlay::~HUDOverlay()
{
    delete this->mAsyncImage[0];
    delete this->mAsyncImage[1];
}

/**
 * Offset/Address/Size: 0x2A1C | 0x800F8CFC | size: 0xBE0
 */
void HUDOverlay::Update(float fDeltaT)
{
    typedef BasicString<unsigned short, Detail::TempStringAllocator> WideString;

    BaseSceneHandler::Update(fDeltaT);
    mAsyncImage[0]->Update(true);
    mAsyncImage[1]->Update(true);

    if (!g_hudVisible)
    {
        SetVisible(false);
    }

    for (int i = 0; i < 2; ++i)
    {
        TLSlide* currentSlide = m_pFEPresentation->m_currentSlide;
        if (mIsHUDSlideIn && mScoreUpdateDelay[i] > 0.0f && currentSlide->m_time >= 1.0f)
        {
            if (mStartScoreAnimation)
            {
                TLComponentInstance* pScoreComp;
                if (i == 0)
                {
                    pScoreComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
                        m_pFEPresentation,
                        InlineHasher(nlStringLowerHash("IN")),
                        InlineHasher(nlStringLowerHash(LAYER_NAME)),
                        InlineHasher(nlStringLowerHash("left_score")));
                }
                else
                {
                    pScoreComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
                        m_pFEPresentation,
                        InlineHasher(nlStringLowerHash("IN")),
                        InlineHasher(nlStringLowerHash(LAYER_NAME)),
                        InlineHasher(nlStringLowerHash("right_score")));
                }

                TLSlide* activeSlide = pScoreComp->GetActiveSlide();
                float endTime = activeSlide->m_start + activeSlide->m_duration;
                if (pScoreComp->GetActiveSlide()->m_time >= endTime)
                {
                    pScoreComp->SetActiveSlide("Slide1");
                    pScoreComp->Update(0.0f);
                    mStartScoreAnimation = false;
                }
            }
            else
            {
                mScoreUpdateDelay[i] -= fDeltaT;

                if (mScoreUpdateDelay[i] <= 0.0f)
                {
                    mScoreUpdateDelay[i] = 0.0f;
                    mScore[i] += 1;

                    if (mScore[i] < mNewScore[i])
                    {
                        mScoreUpdateDelay[i] = 0.5f;
                        mStartScoreAnimation = true;
                    }

                    BasicString<char, Detail::TempStringAllocator> scoreString = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(mScore[i]);
                    nlStrToWcs(scoreString.c_str(), mScoreBuffer[i], 0x20);
                    m_pTextInstanceScore[0][i]->SetString(mScoreBuffer[i]);
                    m_pTextInstanceScore[1][i]->SetString(mScoreBuffer[i]);
                }
            }
            break;
        }
    }

    if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
    {
        DisplayPowerUps();
        return;
    }

    unsigned long time;
    bool isOvertime = nlSingleton<StatsTracker>::Instance()->IsOvertime();

    float fTime = g_pGame->GetGameTime();
    float overtimeTime = 59999.0f;
    float fRemainingTime = g_pGame->m_pGameTweaks->fGameDuration - fTime;
    fTime -= g_pGame->m_pGameTweaks->fGameDuration;

    overtimeTime = (fTime > overtimeTime) ? overtimeTime : fTime;

    time = (unsigned long)fRemainingTime;
    unsigned long remainingTime = (unsigned long)(isOvertime ? overtimeTime : (float)time);
    unsigned long newMinutes = remainingTime / 60;
    unsigned long newSeconds = remainingTime - (newMinutes * 60);
    unsigned long newTenths = 0;

    if (fRemainingTime <= 30.0f || isOvertime)
    {
        if (!mClockColourChanged)
        {
            mClockColourChanged = true;

            nlColour clockColour;
            clockColour.c[0] = 0xCC;
            clockColour.c[1] = 0x33;
            clockColour.c[2] = 0x33;
            clockColour.c[3] = 0xFF;

            m_pTextInstanceClock[0]->SetAssetColour(clockColour);
            m_pTextInstanceClock[1]->SetAssetColour(clockColour);

            Audio::gWorldSFX.Play(Audio::WORLDSFX_HUD_ACCEPT, 100.0f, -1.0f, true, 100.0f);
            Audio::gWorldSFX.Play(Audio::WORLDSFX_HUD_ACCEPT, 100.0f, 0.25f, true, 100.0f);
        }
    }

    if (newMinutes == 0 && fRemainingTime < 30.0f && !isOvertime)
    {
        newTenths = (unsigned long)((fRemainingTime - (float)newSeconds) * 10.0f);
    }

    if (!isOvertime && (float)remainingTime == g_pGame->m_pGameTweaks->fGameDuration && mClockColourChanged)
    {
        mClockColourChanged = false;
        mOvertimeSFXPlayed = false;
        m_pTextInstanceClock[0]->SetAssetColour(mOriginalClockColour);
        m_pTextInstanceClock[1]->SetAssetColour(mOriginalClockColour);
    }

    if (isOvertime)
    {
        if (!mOvertimeSFXPlayed)
        {
            Audio::gWorldSFX.Play((Audio::eWorldSFX)0xCB, 100.0f, -1.0f, true, 100.0f);
            mOvertimeSFXPlayed = true;
        }

        mSuddenDeath[0]->m_bVisible = true;
        mSuddenDeath[1]->m_bVisible = true;
    }
    else
    {
        mSuddenDeath[0]->m_bVisible = false;
        mSuddenDeath[1]->m_bVisible = false;
    }

    if (newSeconds != mSeconds || newMinutes != mMinutes || newTenths != mTenths)
    {
        if (time < 5 && !isOvertime && newSeconds != mSeconds && mTenths == 0 && mSeconds != 0)
        {
            Audio::gWorldSFX.Play(Audio::WORLDSFX_HUD_ACCEPT, 100.0f, -1.0f, true, 100.0f);
        }

        WideString unformatted;
        WideString formatted;

        mSeconds = newSeconds;
        mMinutes = newMinutes;
        mTenths = newTenths;

        char minutesString[8];
        char secondsString[8];
        unsigned short minutesWideString[8];
        unsigned short secondsWideString[8];
        const unsigned short* formatLocString;

        if (mMinutes == 0 && fRemainingTime < 30.0f && !isOvertime)
        {
            nlSNPrintf(minutesString, 8, "%d", newSeconds);
            nlSNPrintf(secondsString, 8, "%d", newTenths);

            nlStrToWcs(minutesString, minutesWideString, 8);
            nlStrToWcs(secondsString, secondsWideString, 8);

            formatLocString = LookupLocHash(0xA1D5611D);

            unformatted = WideString(formatLocString);
            formatted = Format(unformatted, minutesWideString, secondsWideString);
        }
        else
        {
            if (mSeconds < 10)
            {
                nlSNPrintf(secondsString, 8, "0%d", newSeconds);
            }
            else
            {
                nlSNPrintf(secondsString, 8, "%d", newSeconds);
            }

            nlSNPrintf(minutesString, 8, "%d", newMinutes);

            nlStrToWcs(minutesString, minutesWideString, 8);
            nlStrToWcs(secondsString, secondsWideString, 8);

            formatLocString = LookupLocHash(0x04E76F8B);

            unformatted = WideString(formatLocString);
            formatted = Format(unformatted, minutesWideString, secondsWideString);
        }

        memcpy(mClockBuffer, formatted.c_str(), sizeof(mClockBuffer));
        m_pTextInstanceClock[0]->SetString(mClockBuffer);
        m_pTextInstanceClock[1]->SetString(mClockBuffer);
    }

    DisplayPowerUps();
}
/**
 * Offset/Address/Size: 0x2120 | 0x800F8400 | size: 0x8FC
 */
void HUDOverlay::SceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLComponentInstance* pScoreComp;
    eTeamID team;
    TLTextInstance* pTeamName;

    m_pTextInstanceClock[0] = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("clock")));

    m_pTextInstanceClock[1] = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_OUT_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("clock")));

    mOriginalClockColour = m_pTextInstanceClock[0]->GetColour();

    pScoreComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("left_score")));
    {
        TLSlide* pSlide = pScoreComp->GetActiveSlide();
        pScoreComp->Update(pSlide->m_start + pSlide->m_duration);
    }
    m_pTextInstanceScore[0][0] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pScoreComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("left_score")));

    pScoreComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("right_score")));
    {
        TLSlide* pSlide = pScoreComp->GetActiveSlide();
        pScoreComp->Update(pSlide->m_start + pSlide->m_duration);
    }
    m_pTextInstanceScore[0][1] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pScoreComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("right_score")));

    m_pTextInstanceScore[1][0] = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_OUT_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("left_score")));

    m_pTextInstanceScore[1][1] = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_OUT_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("right_score")));

    mSuddenDeath[0] = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("SUDDEN DEATH")));

    mSuddenDeath[1] = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_OUT_NAME)),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("SUDDEN DEATH")));

    mSuddenDeath[0]->m_bVisible = false;
    mSuddenDeath[1]->m_bVisible = false;

    LoadHUDTextures();
    SetTeamIcons();

    team = nlSingleton<GameInfoManager>::Instance()->GetTeam(0);

    pTeamName = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("LEFT NAME")));
    pTeamName->m_LocStrId = GetLOCCharacterName(team, true, false);
    pTeamName->m_OverloadFlags |= 8;

    pTeamName = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_OUT_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("LEFT NAME")));
    pTeamName->m_LocStrId = GetLOCCharacterName(team, true, false);
    pTeamName->m_OverloadFlags |= 8;

    team = nlSingleton<GameInfoManager>::Instance()->GetTeam(1);

    pTeamName = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("RIGHT NAME")));
    pTeamName->m_LocStrId = GetLOCCharacterName(team, true, false);
    pTeamName->m_OverloadFlags |= 8;

    pTeamName = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_OUT_NAME)),
        InlineHasher(nlStringLowerHash(LAYER_NAME)),
        InlineHasher(nlStringLowerHash("RIGHT NAME")));
    pTeamName->m_LocStrId = GetLOCCharacterName(team, true, false);
    pTeamName->m_OverloadFlags |= 8;

    if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
    {
        m_pTextInstanceClock[0]->m_bVisible = false;
        m_pTextInstanceClock[1]->m_bVisible = false;
    }

    m_pFEScene->m_pFEPackage->GetPresentation()->SetActiveSlide("OUT");
    mIsHUDSlideIn = false;
    ResetScores();
}

/**
 * Offset/Address/Size: 0x20DC | 0x800F83BC | size: 0x44
 */
void HUDOverlay::SetSlideIn()
{
    FEPresentation* presentation;
    presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    presentation->SetActiveSlide("IN");
    mIsHUDSlideIn = true;
}

/**
 * Offset/Address/Size: 0x2098 | 0x800F8378 | size: 0x44
 */
void HUDOverlay::SetSlideOut()
{
    FEPresentation* presentation;
    presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    presentation->SetActiveSlide("OUT");
    mIsHUDSlideIn = false;
}

/**
 * Offset/Address/Size: 0x14BC | 0x800F779C | size: 0xBDC
 */
void HUDOverlay::LoadHUDTextures()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    TLImageInstance* pImageInstance;
    int i;
    TLComponentInstance* pComp;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "star", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pStar = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "shell_green", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pShellGreen = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "shell_red", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pShellRed = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "banana", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pBanana = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "mushroom", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pMushroom = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "shell_blue", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pShellBlue = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "shell_spike", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pShellSpike = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "bobomb", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pBobomb = pImageInstance->m_pTextureResource;

    pImageInstance = FIND_IMAGE_PRESENTATION(presentation, "chomp", HUD_SLIDE_IN_NAME);
    pImageInstance->m_bVisible = false;
    m_pChomp = pImageInstance->m_pTextureResource;

    for (i = 0; i < 2; i++)
    {
        pComp = FIND_COMPONENT_PRESENTATION(presentation, LEFT_POWER_UP_IMAGE_NAMES[i], HUD_SLIDE_IN_NAME);
        m_pImagePowerUps[0][0][i] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash(LEFT_POWER_UP_IMAGE_NAMES[i])));

        pComp = FIND_COMPONENT_PRESENTATION(presentation, RIGHT_POWER_UP_IMAGE_NAMES[i], HUD_SLIDE_IN_NAME);
        m_pImagePowerUps[0][1][i] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash(RIGHT_POWER_UP_IMAGE_NAMES[i])));

        pComp = FIND_COMPONENT_PRESENTATION(presentation, LEFT_FLARE_IMAGE_NAMES[i], HUD_SLIDE_IN_NAME);
        m_pComponentFlares[0][i] = pComp;
        m_pImageFlares[0][0][i] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash(LEFT_FLARE_IMAGE_NAMES[i])));
        m_pImageFlares[0][0][i]->m_bVisible = false;

        pComp = FIND_COMPONENT_PRESENTATION(presentation, RIGHT_FLARE_IMAGE_NAMES[i], HUD_SLIDE_IN_NAME);
        m_pComponentFlares[1][i] = pComp;
        m_pImageFlares[0][1][i] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pComp->GetActiveSlide(),
            InlineHasher(nlStringLowerHash(RIGHT_FLARE_IMAGE_NAMES[i])));
        m_pImageFlares[0][1][i]->m_bVisible = false;

        m_pPowerupTextComponents[0][0][i] = FIND_COMPONENT_PRESENTATION(presentation, LEFT_POWER_UP_TEXT_NAMES[i], HUD_SLIDE_IN_NAME);
        m_pPowerupTextComponents[0][1][i] = FIND_COMPONENT_PRESENTATION(presentation, RIGHT_POWER_UP_TEXT_NAMES[i], HUD_SLIDE_IN_NAME);

        m_pImagePowerUps[1][0][i] = FIND_IMAGE_PRESENTATION(presentation, LEFT_POWER_UP_IMAGE_NAMES[i], HUD_SLIDE_OUT_NAME);
        m_pImagePowerUps[1][1][i] = FIND_IMAGE_PRESENTATION(presentation, RIGHT_POWER_UP_IMAGE_NAMES[i], HUD_SLIDE_OUT_NAME);

        m_pImageFlares[1][0][i] = FIND_IMAGE_PRESENTATION(presentation, LEFT_FLARE_IMAGE_NAMES[i], HUD_SLIDE_OUT_NAME);
        m_pImageFlares[1][0][i]->m_bVisible = false;

        m_pImageFlares[1][1][i] = FIND_IMAGE_PRESENTATION(presentation, RIGHT_FLARE_IMAGE_NAMES[i], HUD_SLIDE_OUT_NAME);
        m_pImageFlares[1][1][i]->m_bVisible = false;

        m_pPowerupTextComponents[1][0][i] = FIND_COMPONENT_PRESENTATION(presentation, LEFT_POWER_UP_TEXT_NAMES[i], HUD_SLIDE_OUT_NAME);
        m_pPowerupTextComponents[1][1][i] = FIND_COMPONENT_PRESENTATION(presentation, RIGHT_POWER_UP_TEXT_NAMES[i], HUD_SLIDE_OUT_NAME);
    }
}

#undef FIND_IMAGE_PRESENTATION
#undef FIND_COMPONENT_PRESENTATION

/**
 * Offset/Address/Size: 0x1168 | 0x800F7448 | size: 0x354
 */
void HUDOverlay::DisplayPowerUps()
{
    FETextureResource* texture[2];

    for (int homeAway = 0; homeAway < 2; homeAway++)
    {
        for (int i = 0; i < 2; i++)
        {
            int numPowerups = g_pTeams[homeAway]->GetPowerUpByIndex(i).nnumOfPowerups;

            switch (g_pTeams[homeAway]->GetPowerUpByIndex(i).eType)
            {
            case POWER_UP_NONE:
                texture[i] = NULL;
                break;
            case POWER_UP_GREEN_SHELL:
                texture[i] = m_pShellGreen;
                break;
            case POWER_UP_SPINY_SHELL:
                texture[i] = m_pShellSpike;
                break;
            case POWER_UP_FREEZE_SHELL:
                texture[i] = m_pShellBlue;
                break;
            case POWER_UP_RED_SHELL:
                texture[i] = m_pShellRed;
                break;
            case POWER_UP_MUSHROOM:
                texture[i] = m_pMushroom;
                break;
            case POWER_UP_BANANA:
                texture[i] = m_pBanana;
                break;
            case POWER_UP_BOBOMB:
                texture[i] = m_pBobomb;
                break;
            case POWER_UP_STAR:
                texture[i] = m_pStar;
                break;
            case POWER_UP_CHAIN_CHOMP:
                texture[i] = m_pChomp;
                break;
            }

            if (texture[i] == NULL)
            {
                m_pImagePowerUps[0][homeAway][i]->m_bVisible = false;
                m_pImagePowerUps[1][homeAway][i]->m_bVisible = false;
                if (mNumFlareCycles[homeAway][i] >= 0)
                {
                    mNumFlareCycles[homeAway][i] = -1;
                    m_pImageFlares[0][homeAway][i]->m_bVisible = false;
                }
                m_pPowerupTextComponents[0][homeAway][i]->SetActiveSlide("1");
                m_pPowerupTextComponents[1][homeAway][i]->SetActiveSlide("1");
            }
            else
            {
                if (g_pTeams[homeAway]->GetPowerUpByIndex(i).bIsNew && mNumFlareCycles[homeAway][i] == -1)
                {
                    m_pImageFlares[0][homeAway][i]->m_bVisible = true;
                    m_pComponentFlares[homeAway][i]->SetActiveSlide("Slide1");
                    m_pComponentFlares[homeAway][i]->Update(0.0f);
                    mNumFlareCycles[homeAway][i] = 20;
                }
                else if (mNumFlareCycles[homeAway][i] != -1)
                {
                    TLSlide* activeSlide = m_pComponentFlares[homeAway][i]->GetActiveSlide();
                    if (activeSlide->m_time >= activeSlide->m_start + activeSlide->m_duration - 0.1f)
                    {
                        m_pImagePowerUps[0][homeAway][i]->m_bVisible = true;
                        m_pImagePowerUps[1][homeAway][i]->m_bVisible = true;
                        m_pImageFlares[0][homeAway][i]->m_bVisible = false;
                        m_pImageFlares[1][homeAway][i]->m_bVisible = false;
                        g_pTeams[homeAway]->SetIsPowerUpNew(i, false);
                        mNumFlareCycles[homeAway][i] = -1;

                        if (mIsHUDSlideIn)
                        {
                            PowerupAcquireEventData* data = new (/* PORT: m_data is at 0x18 here. */ (u8*)&g_pEventManager->CreateValidEvent(0x69, 0x1C)->m_data) PowerupAcquireEventData();
                            data->mHomeAway = homeAway;
                        }
                    }
                }
            }

            m_pImagePowerUps[0][homeAway][i]->m_component->pChildren = (TLSlide*)texture[i];
            m_pImagePowerUps[1][homeAway][i]->m_component->pChildren = (TLSlide*)texture[i];

            if (mNumFlareCycles[homeAway][i] == -1 && texture[i] != NULL)
            {
                m_pImagePowerUps[0][homeAway][i]->m_bVisible = true;
                m_pImagePowerUps[1][homeAway][i]->m_bVisible = true;
            }

            if (mNumFlareCycles[homeAway][i] != -1 || numPowerups == 1 || numPowerups == 0)
            {
                m_pPowerupTextComponents[0][homeAway][i]->SetActiveSlide("1");
                m_pPowerupTextComponents[1][homeAway][i]->SetActiveSlide("1");
            }
            else if (numPowerups == 3)
            {
                m_pPowerupTextComponents[0][homeAway][i]->SetActiveSlide("X3");
                m_pPowerupTextComponents[1][homeAway][i]->SetActiveSlide("X3");
            }
            else if (numPowerups == 5)
            {
                m_pPowerupTextComponents[0][homeAway][i]->SetActiveSlide("X5");
                m_pPowerupTextComponents[1][homeAway][i]->SetActiveSlide("X5");
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x3C0 | 0x800F66A0 | size: 0xDA8
 */
void HUDOverlay::SetTeamIcons()
{
    TLComponentInstance* pCompLeft = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN SYMBOL L")));

    TLComponentInstance* pCompRight = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash(HUD_SLIDE_IN_NAME)),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("CAPTAIN SYMBOL R")));

    const char* filename = "art/fe/CaptainIconsUI.res";

    AsyncImage* pImage = (AsyncImage*)nlMalloc(sizeof(AsyncImage), 8, false);
    pImage = new (pImage) AsyncImage(filename, 0);
    mAsyncImage[0] = pImage;
    mAsyncImage[0]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        pCompLeft->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CAPTAIN_ICONS_MARIO")));

    pImage = (AsyncImage*)nlMalloc(sizeof(AsyncImage), 8, false);
    pImage = new (pImage) AsyncImage(filename, 0);
    mAsyncImage[1] = pImage;
    mAsyncImage[1]->mImageInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        pCompRight->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("CAPTAIN_ICONS_DK_r")));

    NLString iconfilename[2];

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
        }

        mAsyncImage[i]->QueueLoad(iconfilename[i].c_str(), true);
    }
}

/**
 * Offset/Address/Size: 0x39C | 0x800F667C | size: 0x24
 */
void HUDOverlay::UpdateScore()
{
    mNewScore[0] = g_pTeams[0]->m_nScore;
    mNewScore[1] = g_pTeams[1]->m_nScore;
}

/**
 * Offset/Address/Size: 0x2F0 | 0x800F65D0 | size: 0xAC
 */
void HUDOverlay::DisplayNewScore()
{
    for (int team = 0; team < 2; team++)
    {
        if (mNewScore[team] != mScore[team])
        {
            mScoreUpdateDelay[team] = 0.5f;
            mStartScoreAnimation = true;
        }

        for (int flare = 0; flare < 2; flare++)
        {
            if (mNumFlareCycles[team][flare] != -1)
            {
                mNumFlareCycles[team][flare] = 20;
                m_pComponentFlares[team][flare]->SetActiveSlide("Slide1");
                m_pComponentFlares[team][flare]->Update(0.0f);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x188 | 0x800F6468 | size: 0x168
 */
void HUDOverlay::ResetScores()
{
    for (int i = 0; i < 2; i++)
    {
        mScore[i] = 0;
        mNewScore[i] = 0;
        BasicString<char, Detail::TempStringAllocator> scoreStr = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(mScore[i]);
        nlStrToWcs(scoreStr.c_str(), mScoreBuffer[i], 0x20);
        m_pTextInstanceScore[0][i]->SetString(mScoreBuffer[i]);
        m_pTextInstanceScore[1][i]->SetString(mScoreBuffer[i]);
    }
    mStartScoreAnimation = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x800F62E0 | size: 0x188
 */
void HUDOverlay::SwapPowerUps(int homeAway)
{
    int firstFlareCycleCount = mNumFlareCycles[homeAway][0];
    mNumFlareCycles[homeAway][0] = mNumFlareCycles[homeAway][1];
    mNumFlareCycles[homeAway][1] = firstFlareCycleCount;

    f32 firstFlareTime = m_pComponentFlares[homeAway][0]->GetActiveSlide()->m_time;
    f32 secondFlareTime = m_pComponentFlares[homeAway][1]->GetActiveSlide()->m_time;

    m_pComponentFlares[homeAway][0]->SetActiveSlide("Slide1");
    m_pComponentFlares[homeAway][0]->Update(secondFlareTime);
    m_pComponentFlares[homeAway][1]->SetActiveSlide("Slide1");
    m_pComponentFlares[homeAway][1]->Update(firstFlareTime);

    for (int i = 0; i < 2; i++)
    {
        if (mNumFlareCycles[homeAway][i] == -1)
        {
            m_pImageFlares[0][homeAway][i]->m_bVisible = false;
            m_pImageFlares[1][homeAway][i]->m_bVisible = false;
            m_pImagePowerUps[0][homeAway][i]->m_bVisible = true;
            m_pImagePowerUps[1][homeAway][i]->m_bVisible = true;
        }
        else
        {
            m_pImageFlares[0][homeAway][i]->m_bVisible = true;
            m_pImageFlares[1][homeAway][i]->m_bVisible = true;
            m_pImagePowerUps[0][homeAway][i]->m_bVisible = false;
            m_pImagePowerUps[1][homeAway][i]->m_bVisible = false;
        }
    }
}
