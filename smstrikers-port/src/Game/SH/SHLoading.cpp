#include "port/region.h"  // PORT: one binary, three discs
#include "Game/SH/SHLoading.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
#include "Game/OverlayManager.h"
#include "Game/FE/feInput.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feMusic.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/GameInfo.h"
#include "Game/main.h"
#include "NL/nlBasicString.h"
#include "NL/nlBundleFile.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlTask.h"
#include "NL/nlLocalization.h"
#include "NL/nlFormat.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/gl/glState.h"
#include "NL/gl/glTexture.h"

// PORT: SHCupTrophy.cpp declares this specialization and never defines it; explicit, it survives inlining.
template WideBasicString Format<WideBasicString, unsigned short[16]>(
    const WideBasicString& string, const unsigned short (&t0)[16]);

extern u32 CONTROLLER_TEXT[4];

/**
 * Offset/Address/Size: 0x19A4 | 0x800A8114 | size: 0x5C
 */
SuperLoadingScene::SuperLoadingScene()
    : BaseSceneHandler()
{
    mType = TT_INVALID;
    mElapsedTime = 0.0f;
    mAlreadySwappedTextures = false;
    mImageInstances[0][0] = NULL;
    mImageInstances[1][0] = NULL;
    mTextureHandles[0][0] = -1;
    mTextureHandles[1][0] = -1;
}

/**
 * Offset/Address/Size: 0x1948 | 0x800A80B8 | size: 0x5C
 */
SuperLoadingScene::~SuperLoadingScene()
{
}

/**
 * Offset/Address/Size: 0x1428 | 0x800A7B98 | size: 0x520
 */
void SuperLoadingScene::SceneCreated()
{
    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();
    if (mType == TT_IN)
    {
        pres->SetActiveSlide("appear");
    }
    else if (mType == TT_OUT)
    {
        pres->SetActiveSlide("disappear");
    }

    BuildAndLoadPortraits(
        nlSingleton<GameInfoManager>::Instance()->GetTeam(0),
        nlSingleton<GameInfoManager>::Instance()->GetTeam(1));

    TLSlide* slide = pres->m_currentSlide;

    mImageInstances[0][0] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("leftimage")));

    mImageInstances[1][0] = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("rightimage")));

    TLTextInstance* stadiumText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("stadiumname")));
    if (stadiumText != NULL)
    {
        stadiumText->m_LocStrId = GetStadiumStringID(nlSingleton<GameInfoManager>::Instance()->GetStadium());
        stadiumText->m_OverloadFlags |= 0x8;
    }

    DisplayCupInfo();

    TLComponentInstance* periodComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("period")));
    if (periodComp != NULL)
    {
        if (g_Language == 2)
            periodComp->m_bVisible = true;
        else
            periodComp->m_bVisible = false;
    }

    TLTextInstance* playersLeft = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("playersleft")));
    if (playersLeft != NULL)
    {
        BuildPlayerStrings(playersLeft, 0, false);
    }

    TLTextInstance* playersRight = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        slide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("playersright")));
    if (playersRight != NULL)
    {
        BuildPlayerStrings(playersRight, 1, false);
    }

    FEMusic::StopStream();

    if (port_region() == PORT_REGION_JAPAN
        && nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
    {
        TLTextInstance* text3 = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            slide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("Text3")));
        if (text3 != NULL)
        {
            text3->m_bVisible = false;
        }
    }
}

/**
 * Offset/Address/Size: 0x1324 | 0x800A7A94 | size: 0x104
 */
void SuperLoadingScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);

    if (!mAlreadySwappedTextures)
    {
        bool allReady;
        FETextureResource* texRes;
        TLImageInstance* img0;
        allReady = false;
        texRes = (img0 = mImageInstances[0][0])->m_pTextureResource;
        if (texRes->m_bValid)
        {
            if (mImageInstances[1][0]->m_pTextureResource->m_bValid)
            {
                allReady = true;
            }
        }
        if (allReady)
        {
            texRes = img0->m_pTextureResource;
            texRes->m_glTextureHandle = mTextureHandles[0][0];
            mImageInstances[1][0]->m_pTextureResource->m_glTextureHandle = mTextureHandles[1][0];
            mAlreadySwappedTextures = true;
        }
    }

    TLSlide* slide = m_pFEScene->m_pFEPackage->GetPresentation()->m_currentSlide;
    if (slide->m_time >= slide->m_start + slide->m_duration)
    {
        if (mType == TT_IN)
        {
            AudioLoader::StopStreaming();
            Audio::ConfigureStreamBuffers(2);
            AudioLoader::PlayLoadLoopMusic();
            nlTaskManager::SetNextState(2);
        }
        else if (mType == TT_OUT)
        {
            nlSingleton<OverlayManager>::Instance()->Pop();
        }
    }
}

unsigned long SuperLoadingScene::LoadImage(BundleFile& bundlefile, eTeamID captain, int playingside, TextureType texturetype)
{
    BundleFileDirectoryEntry fileentry;
    char filename[128] = { };
    u32 texturehandle;
    CaptainSidekickFilename::Build(
        texturetype < TT_NUM_TYPES
            ? (CaptainSidekickFilename::Type)texturetype
            : CaptainSidekickFilename::TYPE_INVALID,
        filename,
        0x80,
        captain,
        playingside);
    bundlefile.GetFileInfo(filename, &fileentry, true);
    u8* fileData = (u8*)nlMalloc(fileentry.m_length, 0x20, true);
    bundlefile.ReadFile(filename, fileData, fileentry.m_length);
    glTextureAdd(nlStringHash(filename), fileData, fileentry.m_length);
    texturehandle = glGetTexture(filename);
    delete[] fileData;
    return texturehandle;
}

void SuperLoadingScene::BuildAndLoadPortraits(eTeamID homecaptain, eTeamID awaycaptain)
{
    BundleFile* bundleFile;

    bundleFile = new (nlMalloc(sizeof(BundleFile), 0x20, true)) BundleFile();
    bundleFile->Open("art/fe/LoadingScreensUI.Res");

    mTextureHandles[0][TT_MAIN] = LoadImage(*bundleFile, homecaptain, 0, TT_MAIN);
    mTextureHandles[1][TT_MAIN] = LoadImage(*bundleFile, awaycaptain, 1, TT_MAIN);

    bundleFile->Close();
    delete bundleFile;
}

bool SuperLoadingScene::TexturesAreValid() const
{
    return mImageInstances[0][0]->m_pTextureResource->m_bValid
        && mImageInstances[1][0]->m_pTextureResource->m_bValid;
}

static inline const unsigned short* LookupLocHash(unsigned long key)
{
    nlLocalization* loc = g_pLocalization;
    if (loc->m_LookupTable == NULL)
    {
        return LocalizationTableNotFound;
    }
    nlLocalization::StringLookup* entry = nlBSearch(key, loc->m_LookupTable, (int)loc->m_pFile->StringCount);
    if (entry != NULL)
    {
        return loc->m_FirstString + entry->StringOffset;
    }
    return MissingLocString;
}

/**
 * Offset/Address/Size: 0x3C4 | 0x800A6B34 | size: 0xF60
 */
void SuperLoadingScene::DisplayCupInfo()
{
    TLSlide* slide = m_pFEPresentation->m_currentSlide;
    GameInfoManager* gameInfo = nlSingleton<GameInfoManager>::s_pInstance;
    TLTextInstance* statsText[4];

    {
        statsText[0] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            slide, InlineHasher(nlStringLowerHash("Layer")), InlineHasher(nlStringLowerHash("stats_left1")));
    }
    {
        statsText[1] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            slide, InlineHasher(nlStringLowerHash("Layer")), InlineHasher(nlStringLowerHash("stats_left2")));
    }
    {
        statsText[2] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            slide, InlineHasher(nlStringLowerHash("Layer")), InlineHasher(nlStringLowerHash("stats_right1")));
    }
    {
        statsText[3] = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            slide, InlineHasher(nlStringLowerHash("Layer")), InlineHasher(nlStringLowerHash("stats_right2")));
    }

    if (!gameInfo->IsInCupOrTournamentMode() || (gameInfo->IsInCupMode() && gameInfo->mDoingKnockout)
        || (gameInfo->IsInTournamentMode() && gameInfo->mCustomTournamentInfo.m_tournMode == TM_KNOCKOUT))
    {
        statsText[0]->m_bVisible = false;
        statsText[1]->m_bVisible = false;
        statsText[2]->m_bVisible = false;
        statsText[3]->m_bVisible = false;
        return;
    }

    TeamStats allTeamStats[8];
    int numTeams = gameInfo->GetNumPlayingTeams();
    int standingsIndices[8];
    int homeAwayIndex[2] = { -1, -1 };
    int ranks[2] = { -1, -1 };
    int stats[4];

    typedef BasicString<unsigned short, Detail::TempStringAllocator> WideString;
    typedef BasicString<char, Detail::TempStringAllocator> NarrowString;

    WideString unformatted[4];

    unformatted[0] = WideString(LookupLocHash(0xF0BEFFA7));
    unformatted[1] = WideString(LookupLocHash(0x18CDE978));
    unformatted[2] = WideString(LookupLocHash(0xF0BEFFA7));
    unformatted[3] = WideString(LookupLocHash(0x18CDE978));

    for (int i = 0; i < numTeams; i++)
    {
        TeamStats teamStats = gameInfo->mPreviousTeamStats[(unsigned short)i];
        allTeamStats[i] = teamStats;

        if (allTeamStats[i].mTeamIndex == gameInfo->GetTeam(0))
        {
            homeAwayIndex[0] = i;
        }
        else if (allTeamStats[i].mTeamIndex == gameInfo->GetTeam(1))
        {
            homeAwayIndex[1] = i;
        }
    }

    nlSingleton<StatsTracker>::Instance()->GetSortedTeamStats(allTeamStats, numTeams, standingsIndices, numTeams);

    for (int i = 0; i < numTeams; i++)
    {
        if (homeAwayIndex[0] == standingsIndices[i])
        {
            ranks[0] = i + 1;
        }
        else if (homeAwayIndex[1] == standingsIndices[i])
        {
            ranks[1] = i + 1;
        }
    }

    stats[0] = ranks[0];
    stats[1] = allTeamStats[homeAwayIndex[0]].mNumPoints;
    stats[2] = ranks[1];
    stats[3] = allTeamStats[homeAwayIndex[1]].mNumPoints;

    for (int i = 0; i < 4; i++)
    {
        NarrowString statString = LexicalCast<NarrowString, int>(stats[i]);
        unsigned short statWideString[16];

        nlStrToWcs(statString.c_str(), statWideString, 16);

        WideString formatted;

        if (nlSingleton<GameInfoManager>::Instance()->GetCurrentRoundNumber() == 0 && (i == 0 || i == 2))
        {
            formatted = Format(unformatted[i], *(const unsigned short (*)[2])L"-");
        }
        else
        {
            formatted = Format(unformatted[i], statWideString);
        }

        memcpy(mStatsBuffers[i], formatted.c_str(), sizeof(mStatsBuffers[i]));
        statsText[i]->SetString(mStatsBuffers[i]);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800A6770 | size: 0x3C4
 */
void SuperLoadingScene::BuildPlayerStrings(TLTextInstance* pTextInst, int side, bool checkConnected)
{
    typedef BasicString<unsigned short, Detail::TempStringAllocator> WideString;
    WideString str;
    char narrowBuf[255] = { };
    unsigned short wideBuf[255] = { };

    for (int i = 0; i < 4; i++)
    {
        if (checkConnected)
        {
            if (!g_pFEInput->IsConnected((eFEINPUT_PAD)i))
                continue;
        }

        if (nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((unsigned short)i) != side)
            continue;

        nlSNPrintf(narrowBuf, 255, "{clr:%2x%2x%2x}", PAD_COLOURS[i][0], PAD_COLOURS[i][1], PAD_COLOURS[i][2]);
        nlStrToWcs(narrowBuf, wideBuf, 255);
        str = str.AppendInPlace(wideBuf);
        str = str.AppendInPlace(LookupLocHash(CONTROLLER_TEXT[i]));
        str = str.AppendInPlace((const unsigned short*)L"{clr:pop} ");
    }

    // PORT: bounded by the string and by the destination.
    {
        unsigned short* pDest = side == 0 ? mPlayerStrings[0] : mPlayerStrings[1];
        const unsigned short* pSrc = str.c_str();
        unsigned long n = 0;
        while (pSrc[n] != 0 && n < 254)
        {
            n++;
        }
        memcpy(pDest, pSrc, n * sizeof(unsigned short));
        pDest[n] = 0;
    }
    pTextInst->SetString(side == 0 ? mPlayerStrings[0] : mPlayerStrings[1]);
}
