#include "NL/nlLexicalCast.h"
#include "Game/FE/BraggingRights.h"
#include "Game/DB/StatsTracker.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feManager.h"
#include "Game/FE/feMusic.h"
#include "Game/FE/feScrollText.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "Game/SH/SHMainMenu.h"
#include "Game/SH/SHSaveLoad.h"
#include "NL/gl/glStruct.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlColour.h"
#include "NL/nlMath.h"
#include "NL/nlPrint.h"
#include "NL/nlFormat.h"
#include "NL/nlLocalization.h"
#include "NL/nlString.h"

static char* TEXT_NAMES[] = { "PLAYER", "PLAYER2", "PLAYER3", "PLAYER4", "PLAYER5" };

static char* CUP_BRAG_TEXT[5][4] = {
    { "Goals", "CUP_MILESTONEUPDATE_VETERAN", "Goals_Detail", "Goals_Detail2" },
    { "Hits", "CUP_MILESTONEUPDATE_SNIPER", "Hits_Detail", "Hits_Detail2" },
    { "Steals", "CUP_MILESTONEUPDATE_SUPERSTRIKER", "Steals_Detail", "Steals_Detail2" },
    { "Weapons", "CUP_MILESTONEUPDATE_TACTICIAN", "Weapons_Detail", "Weapons_Detail2" },
    { "Buttons2", "CUP_MILESTONEUPDATE_PARAMEDIC", "Buttons_Detail", "Buttons_Detail2" },
};

static const unsigned long DETAIL_NAMES[6][2] = {
    { 0x1583A4AF, 0x779A26F3 },
    { 0x14C01771, 0x1EF8D3F5 },
    { 0x361BDF85, 0x88E8F289 },
    { 0x311076D6, 0x994F54FA },
    { 0xB3863DA8, 0x2C4EE80C },
    { 0x724E4E75, 0xF19F2F79 },
};
static const int MILESTONE_GOALS[5] = { 100, 300, 100, 300, 1000 };
extern u32 CONTROLLER_TEXT[4];

namespace SingleHighlite
{
void OpenItem(TLComponentInstance*);
void CloseItem(TLComponentInstance*);
} // namespace SingleHighlite

static inline const unsigned short* LookupLocHash(nlLocalization* loc, unsigned long hash)
{
    if (loc->m_LookupTable == 0)
    {
        return LocalizationTableNotFound;
    }

    nlLocalization::StringLookup* entry = nlBSearch<nlLocalization::StringLookup, unsigned long>(hash, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry)
    {
        return loc->m_FirstString + entry->StringOffset;
    }

    return MissingLocString;
}

static inline const unsigned short* LookupLocHash(unsigned long hash)
{
    return LookupLocHash(g_pLocalization, hash);
}

/**
 * Offset/Address/Size: 0x34C8 | 0x800D54C4 | size: 0xA8
 */
BraggingRightsOverlay::BraggingRightsOverlay()
    : mIsTournamentScene(0)
{
}

/**
 * Offset/Address/Size: 0x3368 | 0x800D5364 | size: 0x160
 */
BraggingRightsOverlay::~BraggingRightsOverlay()
{
    if (mTicker != NULL)
    {
        delete mTicker;
    }
    nlSingleton<GameInfoManager>::Instance()->IsInTournamentMode();
}

/**
 * Offset/Address/Size: 0x2E3C | 0x800D4E38 | size: 0x52C
 */
void BraggingRightsOverlay::SceneCreated()
{
    MenuItem<TLComponentInstance>* menuItem;
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    TLComponentInstance* pTickerComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Component")));

    TLTextInstance* scrollText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pTickerComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Group")),
        InlineHasher(nlStringLowerHash("TickerText")));

    const gl_ScreenInfo* screenInfo = glGetScreenInfo();

    mTicker = new (nlMalloc(sizeof(FEScrollText), 8, false))
        FEScrollText(scrollText, 0, screenInfo->ScreenWidth + 50);

    char menuName[64];

    for (int i = 0; i < 5; i++)
    {
        nlSNPrintf(menuName, 64, "MENU ITEM%d", i + 1);

        TLComponentInstance* instance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(menuName)));

        instance->SetActiveSlide((i == 0) ? SingleHighlite::SLIDE_IN : SingleHighlite::SLIDE_OUT);

        menuItem = mMenuItems.AddItem(instance);

        menuItem->SetCallback(ON_HIGHLIGHT, SingleHighlite::OpenItem);
        menuItem->SetCallback(ON_UNHIGHLIGHT, SingleHighlite::CloseItem);

        menuItem->RunCallback((i == 0) ? ON_HIGHLIGHT : ON_UNHIGHLIGHT);
    }

    mMenuItems.SetFlag(1);

    mButtons.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    if (nlSingleton<GameInfoManager>::Instance()->IsInTournamentMode())
    {
        mIsTournamentScene = 1;
        TournamentSceneCreated();
        mButtons.SetState(ButtonComponent::BS_A_AND_B);
    }
    else
    {
        IngameSceneCreated();
        mButtons.SetState(ButtonComponent::BS_A_ONLY);
    }

    ChangeTicker(0);
}

/**
 * Offset/Address/Size: 0x29A8 | 0x800D49A4 | size: 0x494
 */
void BraggingRightsOverlay::IngameSceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

    PlayerStats userStats[4];
    for (int i = 0; i < 4; i++)
    {
        PlayerStats playerStats = nlSingleton<StatsTracker>::Instance()->mCumulativeUserStats[i];
        userStats[i] = playerStats;
    }

    int highestTieBreaker[5];

    for (int award = 0; award < 5; award++)
    {
        mHighestStats[award] = -1;
        highestTieBreaker[award] = -1;
        for (int user = 0; user < 4; user++)
        {
            int mainStat;
            int tieBreaker;

            if ((short)nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((unsigned short)user) == -1)
                continue;

            switch (award)
            {
            case 0:
            {
                unsigned short shotsOnGoal = userStats[user].mNumShotsOnGoal;
                mainStat = shotsOnGoal;
                if (shotsOnGoal == 0)
                {
                    tieBreaker = 0;
                }
                else
                {
                    tieBreaker = (int)(((float)userStats[user].mNumGoalsFor / (float)shotsOnGoal) * 100.0f);
                }
                break;
            }
            case 1:
                mainStat = userStats[user].mNumHitsMade;
                tieBreaker = userStats[user].mNumFouls;
                break;
            case 2:
            {
                unsigned short steals = userStats[user].mNumSteals;
                unsigned short passesIntercepted = userStats[user].mNumPassesIntercepted;
                tieBreaker = steals;
                mainStat = passesIntercepted + steals;
                break;
            }
            case 3:
                mainStat = userStats[user].mNumPowerupsUsed;
                tieBreaker = userStats[user].mNumPowerupsHit;
                break;
            case 4:
                mainStat = userStats[user].mNumButtonPresses;
                tieBreaker = nlRandom(100, &nlDefaultSeed);
                break;
            }

            if (mainStat > mHighestStats[award]
                || (mainStat == mHighestStats[award] && tieBreaker > highestTieBreaker[award]))
            {
                mHighestStats[award] = mainStat;
                highestTieBreaker[award] = tieBreaker;
                mAwardWinners[award] = user;
            }
        }

        TLTextInstance* pText = FEFinder<TLTextInstance, 3>::Find(
            presentation, InlineHasher(nlStringLowerHash("Slide1")), InlineHasher(nlStringLowerHash("Layer")), InlineHasher(nlStringLowerHash(TEXT_NAMES[award])));

        if (mHighestStats[award] == 0)
        {
            pText->m_LocStrId = 0x0316C79C;
            pText->m_OverloadFlags |= 0x8;
        }
        else
        {
            pText->m_LocStrId = CONTROLLER_TEXT[mAwardWinners[award]];
            pText->m_OverloadFlags |= 0x8;

            nlColour colour;
            const unsigned char* padColour = PAD_COLOURS[mAwardWinners[award]];
            unsigned char blue = padColour[2];
            unsigned char green = padColour[1];
            unsigned char red = padColour[0];

            colour.c[3] = 0xFF;
            colour.c[1] = green;
            colour.c[0] = red;
            colour.c[2] = blue;

            pText->SetAssetColour(colour);
        }
    }

    TLTextInstance* pPlacement = FEFinder<TLTextInstance, 3>::Find(
        presentation, InlineHasher(nlStringLowerHash("Slide1")), InlineHasher(nlStringLowerHash("Layer")), InlineHasher(nlStringLowerHash("Placement")));
    pPlacement->m_bVisible = 0;

    TLTextInstance* pPlacementTitle = FEFinder<TLTextInstance, 3>::Find(
        presentation, InlineHasher(nlStringLowerHash("Slide1")), InlineHasher(nlStringLowerHash("Layer")), InlineHasher(nlStringLowerHash("Placement_Title")));
    pPlacementTitle->m_bVisible = 0;
}

/**
 * Offset/Address/Size: 0x23EC | 0x800D43E8 | size: 0x5BC
 */
void BraggingRightsOverlay::TournamentSceneCreated()
{
    PlayerStats stats[8];
    TLTextInstance* pText;
    int highestTieBreaker[5];
    int i;
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    GameInfoManager* info = nlSingleton<GameInfoManager>::Instance();

    for (i = 0; i < (int)info->GetNumPlayingTeams(); i++)
    {
        stats[i] = info->GetTeamStatsByIndex((unsigned short)i).mPlayerTotalStats;
    }

    for (int award = 0; award < 5; award++)
    {
        mHighestStats[award] = -1;
        highestTieBreaker[award] = -1;

        int user = 0;
        int mainStat;
        int tieBreaker;

        while (user < (int)info->GetNumPlayingTeams())
        {
            int teamIndex = (int)info->GetTeamStatsByIndex((unsigned short)user).mTeamIndex;
            if (info->mCurrentCup->mHumanTeams & (1 << teamIndex))
            {
                switch (award)
                {
                case 0:
                {
                    unsigned short shotsOnGoal = stats[user].mNumShotsOnGoal;
                    mainStat = shotsOnGoal;
                    if (shotsOnGoal == 0)
                    {
                        tieBreaker = 0;
                    }
                    else
                    {
                        tieBreaker = (int)(((float)stats[user].mNumGoalsFor / (float)shotsOnGoal) * 100.0f);
                    }
                    break;
                }

                case 1:
                    mainStat = stats[user].mNumHitsMade;
                    tieBreaker = stats[user].mNumFouls;
                    break;

                case 2:
                {
                    unsigned short steals = stats[user].mNumSteals;
                    unsigned short passesIntercepted = stats[user].mNumPassesIntercepted;
                    tieBreaker = steals;
                    mainStat = passesIntercepted + steals;
                    break;
                }

                case 3:
                    mainStat = stats[user].mNumPowerupsUsed;
                    tieBreaker = stats[user].mNumPowerupsHit;
                    break;

                case 4:
                    mainStat = stats[user].mNumPerfectPasses;
                    tieBreaker = stats[user].mNumPassesReceived;
                    break;
                }

                if (mainStat > mHighestStats[award]
                    || (mainStat == mHighestStats[award] && tieBreaker > highestTieBreaker[award]))
                {
                    mHighestStats[award] = mainStat;
                    highestTieBreaker[award] = tieBreaker;
                    mAwardWinners[award] = user;
                }
            }

            user++;
        }

        pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
            presentation,
            InlineHasher(nlStringLowerHash("Slide1")),
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(TEXT_NAMES[award])));

        if (mHighestStats[award] == 0)
        {
            pText->m_LocStrId = 0x0316C79C;
            pText->m_OverloadFlags |= 0x8;
        }
        else
        {
            pText->m_LocStrId = GetLOCCharacterName(
                (eTeamID)info->GetTeamStatsByIndex((unsigned short)mAwardWinners[award]).mTeamIndex,
                false,
                false);
            pText->m_OverloadFlags |= 0x8;
        }
    }

    pText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Placement")));

    unsigned long winningNameId = GetLOCCharacterName(nlSingleton<GameInfoManager>::Instance()->FindWinningTeam(), false, false);
    pText->SetString(LookupLocHash(winningNameId));

    TLComponentInstance* pButtonComp = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("MENU ITEM5")));

    pButtonComp->SetActiveSlide("IN");

    TLTextInstance* pAwardText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pButtonComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("R JUST")));
    pAwardText->m_LocStrId = 0xB6940BE2;
    pAwardText->m_OverloadFlags |= 0x8;

    pButtonComp->SetActiveSlide("OUT");

    pAwardText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        pButtonComp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("R JUST")));
    pAwardText->m_LocStrId = 0xB6940BE2;
    pAwardText->m_OverloadFlags |= 0x8;
}

/**
 * Offset/Address/Size: 0x1EA0 | 0x800D3E9C | size: 0x54C
 */
void BraggingRightsOverlay::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    f32 fadeEndTime = presentation->m_currentSlide->m_start + presentation->m_currentSlide->m_duration;
    if (presentation->m_currentSlide->m_time < fadeEndTime)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL)
            || g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            presentation->m_fadeDuration = fadeEndTime;
        }
        return;
    }
    TLComponentInstance* pTickerComp = FEFinder<TLComponentInstance, 4>::Find(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Component")),
        InlineHasher(0),
        InlineHasher(0),
        InlineHasher(0));
    TLSlide* slide = pTickerComp->GetActiveSlide();
    f32 slideEndTime = slide->m_start + slide->m_duration;
    if (pTickerComp->GetActiveSlide()->m_time >= slideEndTime)
    {
        mTicker->Update(fDeltaT);
    }
    if (!mIsTournamentScene)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            FrontEnd::ReturnToFE();
            FEAudio::PlayAnimAudioEvent("sfx_screen_forward", false);
            return;
        }
    }
    if (mIsTournamentScene)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            if (nlSingleton<GameInfoManager>::Instance()->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TOURNAMENT_STANDINGS_FINAL_ANIM, SCREEN_BACK, true);
            }
            else
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TOURNAMENT_STANDINGS, SCREEN_BACK, true);
            }
            return;
        }
    }
    if (mIsTournamentScene)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
        {
            nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
            if (SaveLoadScene::IsIOEnabled())
            {
                SaveLoadScene* scene = (SaveLoadScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SAVE, SCREEN_FORWARD, false);
                scene->mNextScene = SCENE_MAIN_MENU;
            }
            else
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, SCREEN_FORWARD, false);
            }
            SHMainMenu::mSnapMenuIntoPosition = true;
            SHMainMenu::mLastMenuItem = 0;
            return;
        }
    }
    if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0x0E, true, NULL))
    {
        mMenuItems.NextItem();
        ChangeTicker(mMenuItems.GetActiveItemIndex());
    }
    else if (g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0x0D, true, NULL))
    {
        mMenuItems.PreviousItem();
        ChangeTicker(mMenuItems.GetActiveItemIndex());
    }
}

/**
 * Offset/Address/Size: 0x15A4 | 0x800D35A0 | size: 0x8FC
 */
void BraggingRightsOverlay::ChangeTicker(int tickerRow)
{
    BasicString<char, Detail::TempStringAllocator> statString
        = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(mHighestStats[tickerRow]);
    unsigned short statWideString[16];

    nlStrToWcs(statString.c_str(), statWideString, 16);

    BasicString<unsigned short, Detail::TempStringAllocator> formatted;
    bool useDetailName2 = (mHighestStats[tickerRow] == 1);

    if (mHighestStats[tickerRow] == 0)
    {
        formatted = BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(0xBA05BFBD));
    }
    else
    {
        if (mIsTournamentScene)
        {
            eTeamID winningTeam = nlSingleton<GameInfoManager>::Instance()->GetTeamStatsByIndex((unsigned short)mAwardWinners[tickerRow]).mTeamIndex;

            if (tickerRow == 4)
            {
                tickerRow++;
            }

            formatted = Format<BasicString<unsigned short, Detail::TempStringAllocator>, const unsigned short*, unsigned short[16]>(
                BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(DETAIL_NAMES[tickerRow][useDetailName2 != false])),
                LookupLocHash(GetLOCCharacterName(winningTeam, false, false)),
                statWideString);
        }
        else
        {
            formatted = Format<BasicString<unsigned short, Detail::TempStringAllocator>, const unsigned short*, unsigned short[16]>(
                BasicString<unsigned short, Detail::TempStringAllocator>(LookupLocHash(DETAIL_NAMES[tickerRow][useDetailName2 != false])),
                LookupLocHash(CONTROLLER_TEXT[mAwardWinners[tickerRow]]),
                statWideString);
        }
    }

    memcpy(mBuffer, formatted.c_str(), sizeof(mBuffer));

    mTicker->SetDisplayMessage(BasicString<unsigned short, Detail::TempStringAllocator>(mBuffer));
}

/**
 * Offset/Address/Size: 0x153C | 0x800D3538 | size: 0x68
 */
BraggingRightsScene::BraggingRightsScene()
{
}

/**
 * Offset/Address/Size: 0x14C4 | 0x800D34C0 | size: 0x78
 */
BraggingRightsScene::~BraggingRightsScene()
{
}

/**
 * Offset/Address/Size: 0x2E4 | 0x800D22E0 | size: 0x11E0
 */
void BraggingRightsScene::SceneCreated()
{
    FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    UserInfo& ui = nlSingleton<GameInfoManager>::Instance()->mUserInfo;
    GameInfoManager* info = nlSingleton<GameInfoManager>::Instance();

    int wins;
    int losses;
    TLTextInstance* pText;
    int totalStats[5];
    int currentStats[5];
    TeamStats userStats;
    unsigned char complete[5] = { 0, 0, 0, 0, 0 };
    int i;

    for (i = 0; i < info->GetNumPlayingTeams(); i++)
    {
        TeamStats teamStats = info->GetTeamStatsByIndex((unsigned short)i);
        if (teamStats.mTeamIndex == info->GetUserSelectedCupTeam())
        {
            userStats = teamStats;
            break;
        }
    }

    totalStats[0] = ui.mNumGamesPlayed;
    currentStats[0] = userStats.mPlayerTotalStats.mNumGamesPlayed;
    if (totalStats[0] >= 100)
    {
        complete[0] = 1;
    }

    totalStats[1] = ui.mNumGoalsScored;
    currentStats[1] = userStats.mPlayerTotalStats.mNumGoalsFor;
    if (totalStats[1] >= 300)
    {
        complete[1] = 1;
    }

    totalStats[2] = ui.mNumSTSAttempts;
    currentStats[2] = userStats.mPlayerTotalStats.mNumSTSAttempts;
    if (totalStats[2] >= 100)
    {
        complete[2] = 1;
    }

    totalStats[3] = ui.mNumPerfectPasses;
    currentStats[3] = userStats.mPlayerTotalStats.mNumPerfectPasses;
    if (totalStats[3] >= 300)
    {
        complete[3] = 1;
    }

    totalStats[4] = ui.mNumHits;
    currentStats[4] = userStats.mPlayerTotalStats.mNumHitsMade;
    if (totalStats[4] >= 1000)
    {
        complete[4] = 1;
    }

    TLTextInstance* pTitleText = FEFinder<TLTextInstance, 3>::Find(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Title")));
    pTitleText->m_LocStrId = 0xB1829C7A;
    pTitleText->m_OverloadFlags |= 0x8;

    TLComponentInstance* buttonComponent = FEFinder<TLComponentInstance, 4>::Find(
        presentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));
    mButtons.mButtonInstance = buttonComponent;
    mButtons.SetState(ButtonComponent::BS_A_AND_B);

    for (i = 0; i < 5; i++)
    {
        TLTextInstance* pLabelText = FEFinder<TLTextInstance, 3>::Find(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(CUP_BRAG_TEXT[i][0])));
        pLabelText->SetStringId(CUP_BRAG_TEXT[i][1]);

        TLTextInstance* pStatText = FEFinder<TLTextInstance, 3>::Find(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(CUP_BRAG_TEXT[i][3])));

        if (!complete[i])
        {
            BasicString<unsigned short, Detail::TempStringAllocator> unformatted((const unsigned short*)L"{0}/{1}");
            BasicString<char, Detail::TempStringAllocator> statString
                = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(totalStats[i]);
            BasicString<char, Detail::TempStringAllocator> currentStatString
                = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(MILESTONE_GOALS[i]);
            unsigned short statWideString[32];
            unsigned short currentStatWideString[32];

            nlStrToWcs(statString.c_str(), statWideString, 32);
            nlStrToWcs(currentStatString.c_str(), currentStatWideString, 32);

            BasicString<unsigned short, Detail::TempStringAllocator> formatted = Format<BasicString<unsigned short, Detail::TempStringAllocator>, unsigned short[32], unsigned short[32]>(
                unformatted, statWideString, currentStatWideString);

            memcpy(mBuffer[i], formatted.c_str(), sizeof(mBuffer[i]));
            pStatText->SetString(mBuffer[i]);
        }
        else
        {
            BasicString<char, Detail::TempStringAllocator> statString
                = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(totalStats[i]);
            nlStrToWcs(statString.c_str(), mBuffer[i], 256);
            pStatText->SetString(mBuffer[i]);
        }

        pText = FEFinder<TLTextInstance, 3>::Find(
            presentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(CUP_BRAG_TEXT[i][2])));

        BasicString<char, Detail::TempStringAllocator> currentStatString
            = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(currentStats[i]);
        nlStrToWcs(currentStatString.c_str(), mThisCupBuffer[i], 256);
        pText->SetString(mThisCupBuffer[i]);
    }

    for (i = 0; i < info->GetNumPlayingTeams(); i++)
    {
        if (info->GetTeamStatsByIndex((unsigned short)i).mTeamIndex == info->GetUserSelectedCupTeam())
        {
            TeamStats selectedTeamStats = info->GetTeamStatsByIndex((unsigned short)i);
            wins = selectedTeamStats.mNumWins;
            losses = selectedTeamStats.mNumLosses + selectedTeamStats.mNumOTLosses;
            break;
        }
    }

    mUserPlace = nlSingleton<GameInfoManager>::Instance()->DetermineUserPlacement(NULL);

    BasicString<char, Detail::TempStringAllocator> winString
        = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(wins);
    BasicString<char, Detail::TempStringAllocator> lossString
        = LexicalCast<BasicString<char, Detail::TempStringAllocator>, int>(losses);
    unsigned short winWideString[32];
    unsigned short lossWideString[32];

    nlStrToWcs(winString.c_str(), winWideString, 32);
    nlStrToWcs(lossString.c_str(), lossWideString, 32);

    nlLocalization* loc = g_pLocalization;
    BasicString<unsigned short, Detail::TempStringAllocator> unformatted(
        LookupLocHash(loc, nlStringLowerHash("BRAG_RATIO")));

    BasicString<unsigned short, Detail::TempStringAllocator> formatted = Format<BasicString<unsigned short, Detail::TempStringAllocator>, unsigned short[32], unsigned short[32]>(
        unformatted, winWideString, lossWideString);
    memcpy(mRatioBuffer, formatted.c_str(), sizeof(mRatioBuffer));

    pText = FEFinder<TLTextInstance, 3>::Find(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Win_Loss_Record")));
    pText->SetString(mRatioBuffer);

    TLComponentInstance* placementComponent = FEFinder<TLComponentInstance, 4>::Find(
        presentation,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Placement")));

    pText = FEFinder<TLTextInstance, 3>::Find(
        placementComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Placement")));

    if (mUserPlace == -2)
    {
        pText->m_LocStrId = 0x813370B6;
        pText->m_OverloadFlags |= 0x8;
    }
    else
    {
        pText->m_LocStrId = GetLOCRank(mUserPlace);
        pText->m_OverloadFlags |= 0x8;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800D1FFC | size: 0x2E4
 */
void BraggingRightsScene::Update(float fDeltaT)
{
    FEPresentation* presentation;
    GameInfoManager* info;
    SaveLoadScene* scene;
    float fadeEndTime;

    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    presentation = m_pFEScene->m_pFEPackage->GetPresentation();
    fadeEndTime = presentation->m_currentSlide->m_start + presentation->m_currentSlide->m_duration;

    if (presentation->m_currentSlide->m_time < fadeEndTime)
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL)
            || g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            presentation->m_fadeDuration = fadeEndTime;
        }
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        SceneList nextScene;

        nlSingleton<GameSceneManager>::Instance()->PopEntireStack();

        info = nlSingleton<GameInfoManager>::s_pInstance;
        nextScene = SCENE_MAIN_MENU;
        if (info->mCurrentMode == GameInfoManager::GM_BOWSER_CUP
            || info->mCurrentMode == GameInfoManager::GM_SUPER_BOWSER_CUP)
        {
            if (mUserPlace == 0)
            {
                nextScene = SCENE_CREDITS;
                FEMusic::StopStream();
            }
        }

        if (SaveLoadScene::IsIOEnabled())
        {
            scene = (SaveLoadScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SAVE, SCREEN_FORWARD, false);
            scene->mNextScene = nextScene;
        }
        else
        {
            nlSingleton<GameSceneManager>::Instance()->Push(nextScene, SCREEN_FORWARD, false);
        }

        SHMainMenu::mSnapMenuIntoPosition = false;
        SHMainMenu::mLastMenuItem = 0;
        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        info = nlSingleton<GameInfoManager>::s_pInstance;
        if (info->mCurrentMode == GameInfoManager::GM_BOWSER_CUP)
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_STANDINGS_FINAL_ANIM, SCREEN_BACK, true);
        }
        else if (info->mCurrentMode == GameInfoManager::GM_SUPER_BOWSER_CUP)
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SUPER_CUP_STANDINGS_FINAL_ANIM, SCREEN_BACK, true);
        }
        else if (info->IsInTournamentMode() && info->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT)
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TOURNAMENT_STANDINGS_FINAL_ANIM, SCREEN_BACK, true);
        }
        else if (info->IsInRegularCupMode())
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_STANDINGS, SCREEN_BACK, true);
        }
        else if (info->IsInSuperCupMode())
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SUPER_CUP_STANDINGS, SCREEN_BACK, true);
        }
        else
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TOURNAMENT_STANDINGS, SCREEN_BACK, true);
        }

        FEAudio::PlayAnimAudioEvent("sfx_back", false);
    }
}
