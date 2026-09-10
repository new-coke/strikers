#include "Game/SH/SHStadiumSelect.h"

#include "Game/FE/FEAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feInput.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "Game/SH/SHChooseCaptains.h"
#include "NL/gl/glStruct.h"
#include "NL/nlPrint.h"
#include "types.h"

static const eStadiumID STADIUM_ORDER[7] = {
    STAD_MARIO_STADIUM,
    STAD_PEACH_TOAD_STADIUM,
    STAD_WARIO_STADIUM,
    STAD_DK_DAISY,
    STAD_YOSHI_STADIUM,
    STAD_FORBIDDEN_DOME,
    STAD_SUPER_STADIUM,
};

static const StadiumEntry StadiumEntries[8] = {
    { STAD_PEACH_TOAD_STADIUM, "fe/mainv2/targas/stadium_peach" },
    { STAD_MARIO_STADIUM, "fe/mainv2/targas/stadium_mario" },
    { STAD_WARIO_STADIUM, "fe/mainv2/targas/stadium_wario" },
    { STAD_DK_DAISY, "fe/mainv2/targas/stadium_dk" },
    { STAD_YOSHI_STADIUM, "fe/mainv2/targas/stadium_yoshi" },
    { STAD_FORBIDDEN_DOME, "fe/mainv2/targas/stadium_dome" },
    { STAD_SUPER_STADIUM, "fe/mainv2/targas/stadium_super" },
    { (eStadiumID)7, "fe/mainv2/targas/stadium_LOCKED" },
};

static unsigned long StadiumDescriptions[7] = {
    0xE2FCB05C,
    0x065E30EC,
    0xEF23A6A3,
    0xEF484926,
    0xF045B5A2,
    0x618EAFC3,
    0xEFDC5FC5,
};

namespace
{
char* GetStadiumEntry(int index)
{
    return (char*)StadiumEntries[index].mFilename;
}
} // namespace

static inline int WrapStadiumIndex(int index)
{
    if (index < 0)
    {
        index += 7;
    }
    return index % 7;
}

/**
 * Offset/Address/Size: 0x1400 | 0x800D9980 | size: 0xF4
 */
StadiumSelectSceneV2::StadiumSelectSceneV2()
    : mTempTextureBuffer(NULL)
    , mTempTextureBufferSize(0)
    , mStadiumIndex(0)
    , mCurrentMenuList(NULL)
    , mLastDirection(DIR_LEFT)
{
}

/**
 * Offset/Address/Size: 0x1230 | 0x800D97B0 | size: 0x1D0
 */
StadiumSelectSceneV2::~StadiumSelectSceneV2()
{
    for (int i = 0; i < NUM_IMAGES; i++)
    {
        delete mImages[i];
    }

    if (mTempTextureBuffer != NULL)
    {
        delete[] mTempTextureBuffer;
    }

    delete m_pTicker;

    m_pTicker = NULL;
}

/**
 * Offset/Address/Size: 0x7D4 | 0x800D8D54 | size: 0xA5C
 */
void StadiumSelectSceneV2::SceneCreated()
{
    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();

    for (int i = 0; i < NUM_IMAGES; i++)
    {

        char buf[64];
        nlSNPrintf(buf, 64, "stadium_%c", i + 'A');

        TLImageInstance* img = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            pres->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash(buf)));
        AsyncImage* asyncImg = new (nlMalloc(sizeof(AsyncImage), 0x20, true)) AsyncImage("art/fe/StadiumsUI.res", NULL);
        mImages[i] = asyncImg;
        mImages[i]->mImageInstance = img;
    }

    mStadiumIndex = 0;

    {

        TLTextInstance* tickerText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            pres->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("TickerText")));

        gl_ScreenInfo* screenInfo = glGetScreenInfo();
        FEScrollText* ticker = new (nlMalloc(sizeof(FEScrollText), 0x8, false)) FEScrollText(tickerText, 0, screenInfo->ScreenWidth + 50);
        m_pTicker = ticker;
    }

    m_pTicker->SetDisplayMessage(StadiumDescriptions[mStadiumIndex]);

    eStadiumID sid;
    int loadIdx;

    {
        int idx;
        if ((idx = mStadiumIndex - 4) < 0)
        {
            idx += 7;
        }
        idx = idx % 7;
        sid = STADIUM_ORDER[idx];
        loadIdx = IsStadiumUnlocked(sid) ? idx : 7;
        mImages[6]->QueueLoad(StadiumEntries[loadIdx].mFilename, true);
    }

    {
        int idx;
        if ((idx = mStadiumIndex - 3) < 0)
        {
            idx += 7;
        }
        idx = idx % 7;
        sid = STADIUM_ORDER[idx];
        loadIdx = IsStadiumUnlocked(sid) ? idx : 7;
        mImages[3]->QueueLoad(StadiumEntries[loadIdx].mFilename, true);
    }

    {
        int idx;
        if ((idx = mStadiumIndex - 2) < 0)
        {
            idx += 7;
        }
        idx = idx % 7;
        sid = STADIUM_ORDER[idx];
        loadIdx = IsStadiumUnlocked(sid) ? idx : 7;
        mImages[2]->QueueLoad(StadiumEntries[loadIdx].mFilename, true);
    }

    {
        int idx;
        if ((idx = mStadiumIndex - 1) < 0)
        {
            idx += 7;
        }
        idx = idx % 7;
        sid = STADIUM_ORDER[idx];
        loadIdx = IsStadiumUnlocked(sid) ? idx : 7;
        mImages[0]->QueueLoad(StadiumEntries[loadIdx].mFilename, true);
    }

    {
        int idx;
        if ((idx = mStadiumIndex) < 0)
        {
            idx += 7;
        }
        idx = idx % 7;
        sid = STADIUM_ORDER[idx];
        loadIdx = IsStadiumUnlocked(sid) ? idx : 7;
        mImages[1]->QueueLoad(StadiumEntries[loadIdx].mFilename, true);
    }

    {
        int idx;
        if ((idx = mStadiumIndex + 1) < 0)
        {
            idx += 7;
        }
        idx = idx % 7;
        sid = STADIUM_ORDER[idx];
        loadIdx = IsStadiumUnlocked(sid) ? idx : 7;
        mImages[4]->QueueLoad(StadiumEntries[loadIdx].mFilename, true);
    }

    {
        int idx;
        if ((idx = mStadiumIndex + 2) < 0)
        {
            idx += 7;
        }
        idx = idx % 7;
        sid = STADIUM_ORDER[idx];
        loadIdx = IsStadiumUnlocked(sid) ? idx : 7;
        mImages[5]->QueueLoad(StadiumEntries[loadIdx].mFilename, true);
    }

    mLastDirection = DIR_LEFT;
    mTempTextureBuffer = NULL;
    mTempTextureBufferSize = -1;

    {

        struct SlideHolder
        {
            TLSlide* p;
        } holder;
        holder.p = m_pFEPresentation->m_currentSlide;

        TLTextInstance* nameText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            holder.p,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("stadiumname")));

        eStadiumID currentStadium = StadiumEntries[mStadiumIndex].mStadiumID;
        bool isUnlocked = true;
        switch (currentStadium)
        {
        case STAD_DK_DAISY:
            isUnlocked = GameInfoManager::Instance()->IsKongaUnlocked();
            break;
        case STAD_YOSHI_STADIUM:
            isUnlocked = GameInfoManager::Instance()->IsYoshiUnlocked();
            break;
        case STAD_FORBIDDEN_DOME:
            isUnlocked = GameInfoManager::Instance()->IsForbiddenUnlocked();
            break;
        case STAD_SUPER_STADIUM:
            isUnlocked = GameInfoManager::Instance()->IsSuperStadiumUnlocked();
            break;
        }

        if (isUnlocked)
        {
            nameText->m_LocStrId = GetStadiumStringID(StadiumEntries[mStadiumIndex].mStadiumID);
            nameText->m_OverloadFlags |= 0x8;
        }
        else
        {
            nameText->m_LocStrId = 0x2A68AC55;
            nameText->m_OverloadFlags |= 0x8;
        }
    }

    {

        TLComponentInstance* dayNightComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pres->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("day night")));
        dayNightComp->m_bVisible = false;
        pres->SetActiveSlide("RIGHT");
    }

    {

        TLComponentInstance* dayNightComp2 = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            pres->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("day night")));
        dayNightComp2->m_bVisible = false;
        pres->SetActiveSlide("LEFT");
    }

    {

        TLComponentInstance* buttonsComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));
        mButtons.mButtonInstance = buttonsComp;
        mButtons.SetState(ButtonComponent::BS_A_AND_B);
    }
}

/**
 * Offset/Address/Size: 0x210 | 0x800D8790 | size: 0x5C4
 */
void StadiumSelectSceneV2::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    if (m_pFEPresentation->m_currentSlide->m_time >= m_pFEPresentation->m_currentSlide->m_start + m_pFEPresentation->m_currentSlide->m_duration)
    {
        m_pTicker->Update(fDeltaT);
        bool rightPressed = g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xC, true, NULL);
        bool leftPressed = g_pFEInput->IsAutoPressed(FE_ALL_PADS, 0xB, true, NULL);

        if (rightPressed || leftPressed)
        {
            int newIndex;
            if (rightPressed)
            {
                if ((newIndex = mStadiumIndex + 1) < 0)
                {
                    newIndex += 7;
                }
                newIndex %= 7;
            }
            else
            {
                if ((newIndex = mStadiumIndex - 1) < 0)
                {
                    newIndex += 7;
                }
                newIndex %= 7;
            }
            mStadiumIndex = newIndex;

            m_pFEPresentation->SetActiveSlide(rightPressed ? "LEFT" : "RIGHT");

            {
                TLComponentInstance* buttonsComp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
                    m_pFEPresentation->m_currentSlide,
                    InlineHasher(nlStringLowerHash("Layer")),
                    InlineHasher(nlStringLowerHash("buttons")));
                mButtons.mButtonInstance = buttonsComp;
                mButtons.SetState(ButtonComponent::BS_A_AND_B);
            }

            if (rightPressed)
            {
                if (mLastDirection != DIR_LEFT)
                {
                    ResetFromRight();
                }
                else
                {
                    ResetFromLeft();
                }
            }
            else
            {
                if (mLastDirection != DIR_RIGHT)
                {
                    ResetFromLeft();
                }
                else
                {
                    ResetFromRight();
                }
            }

            mLastDirection = (Direction)(rightPressed == 0 ? 1 : 0);
            FEAudio::PlayAnimAudioEvent("sfx_toggle_stadium", false);

            eStadiumID sid = GetSelectedStadium();

            TLTextInstance* nameText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                m_pFEPresentation->m_currentSlide,
                InlineHasher(nlStringLowerHash("Layer")),
                InlineHasher(nlStringLowerHash("stadiumname")));

            {
                bool isUnlocked = true;
                switch (sid)
                {
                case STAD_DK_DAISY:
                    isUnlocked = GameInfoManager::Instance()->IsKongaUnlocked();
                    break;
                case STAD_YOSHI_STADIUM:
                    isUnlocked = GameInfoManager::Instance()->IsYoshiUnlocked();
                    break;
                case STAD_FORBIDDEN_DOME:
                    isUnlocked = GameInfoManager::Instance()->IsForbiddenUnlocked();
                    break;
                case STAD_SUPER_STADIUM:
                    isUnlocked = GameInfoManager::Instance()->IsSuperStadiumUnlocked();
                    break;
                }

                if (isUnlocked)
                {
                    nameText->m_LocStrId = GetStadiumStringID(sid);
                    nameText->m_OverloadFlags |= 0x8;
                }
                else
                {
                    nameText->m_LocStrId = 0x2A68AC55;
                    nameText->m_OverloadFlags |= 0x8;
                }
            }

            {
                TLTextInstance* tickerText = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
                    m_pFEPresentation->m_currentSlide,
                    InlineHasher(nlStringLowerHash("Layer")),
                    InlineHasher(nlStringLowerHash("TickerText")));

                m_pTicker->ApplyNewTextInstancePointer(tickerText, 8000.0f, 100.0f);
            }

            {
                bool isUnlocked = true;
                switch (sid)
                {
                case STAD_DK_DAISY:
                    isUnlocked = GameInfoManager::Instance()->IsKongaUnlocked();
                    break;
                case STAD_YOSHI_STADIUM:
                    isUnlocked = GameInfoManager::Instance()->IsYoshiUnlocked();
                    break;
                case STAD_FORBIDDEN_DOME:
                    isUnlocked = GameInfoManager::Instance()->IsForbiddenUnlocked();
                    break;
                case STAD_SUPER_STADIUM:
                    isUnlocked = GameInfoManager::Instance()->IsSuperStadiumUnlocked();
                    break;
                }

                if (isUnlocked)
                {
                    m_pTicker->SetDisplayMessage(StadiumDescriptions[mStadiumIndex]);
                }
                else
                {
                    m_pTicker->SetDisplayMessage((unsigned long)0xDAA0A048);
                }
            }
        }
        else
        {
            if (g_pFEInput->JustPressed(FE_ALL_PADS, PAD_BUTTON_A, false, NULL))
            {
                OnSelectStadium();
                return;
            }

            if (g_pFEInput->JustPressed(FE_ALL_PADS, PAD_BUTTON_B, false, NULL))
            {
                ChooseCaptainsSceneV2* scene = (ChooseCaptainsSceneV2*)GameSceneManager::Instance()->Push(
                    SCENE_CHOOSE_CAPTAINS, SCREEN_BACK, true);
                scene->mDesiredSceneType = ChooseCaptainsSceneV2::ST_CHOOSE_SIDES;
                FEAudio::PlayAnimAudioEvent("sfx_back", false);
                return;
            }
        }
    }

    for (int i = 0; i < NUM_IMAGES; i++)
    {
        mImages[i]->Update(true);
    }
}

void StadiumSelectSceneV2::Copy(char source, char dest)
{
    if (source == dest)
    {
        return;
    }

    AsyncImage* sourceImage = GetImage(source);
    GetImage(dest)->CopyFrom(sourceImage);
    GetImage(dest)->SwapTextures();
}

/**
 * Offset/Address/Size: 0x108 | 0x800D8688 | size: 0x108
 */
void StadiumSelectSceneV2::ResetFromRight()
{
    PrepareTempTextureBuffer();
    CopyToTempTextureBuffer(mImages[5]->m_loadBuffer);

    Copy(4, 5);
    Copy(1, 4);
    Copy(0, 1);
    Copy(2, 0);
    Copy(3, 2);
    Copy(6, 3);

    mImages[6]->CopyFrom(mTempTextureBuffer, mTempTextureBufferSize);

    mImages[0]->SwapTextures();
    mImages[1]->SwapTextures();
    mImages[2]->SwapTextures();
}

/**
 * Offset/Address/Size: 0x0 | 0x800D8580 | size: 0x108
 */
void StadiumSelectSceneV2::ResetFromLeft()
{
    PrepareTempTextureBuffer();
    CopyToTempTextureBuffer(mImages[6]->m_loadBuffer);

    Copy(3, 6);
    Copy(2, 3);
    Copy(0, 2);
    Copy(1, 0);
    Copy(4, 1);
    Copy(5, 4);

    mImages[5]->CopyFrom(mTempTextureBuffer, mTempTextureBufferSize);

    mImages[0]->SwapTextures();
    mImages[1]->SwapTextures();
    mImages[2]->SwapTextures();
}

void StadiumSelectSceneV2::PrepareTempTextureBuffer()
{
    if (mTempTextureBuffer == NULL)
    {
        mTempTextureBufferSize = mImages[0]->mTextureSize;
        mTempTextureBuffer = nlMalloc(mTempTextureBufferSize, 0x20, true);
    }
}

void StadiumSelectSceneV2::CopyToTempTextureBuffer(void* source)
{
    memcpy(mTempTextureBuffer, source, mTempTextureBufferSize);
}

eStadiumID StadiumSelectSceneV2::GetSelectedStadium()
{
    return StadiumEntries[mStadiumIndex].mStadiumID;
}

void StadiumSelectSceneV2::OnSelectStadium()
{
    eStadiumID stadium = StadiumEntries[mStadiumIndex].mStadiumID;
    bool isUnlocked = true;
    switch (stadium)
    {
    case STAD_DK_DAISY:
        isUnlocked = GameInfoManager::Instance()->IsKongaUnlocked();
        break;
    case STAD_YOSHI_STADIUM:
        isUnlocked = GameInfoManager::Instance()->IsYoshiUnlocked();
        break;
    case STAD_FORBIDDEN_DOME:
        isUnlocked = GameInfoManager::Instance()->IsForbiddenUnlocked();
        break;
    case STAD_SUPER_STADIUM:
        isUnlocked = GameInfoManager::Instance()->IsSuperStadiumUnlocked();
        break;
    }

    if (isUnlocked)
    {
        GameSceneManager::Instance()->PushLoadingScene(true);
        GameInfoManager::Instance()->SetStadium(stadium);
        FEAudio::PlayAnimAudioEvent("sfx_accept_stadium", false);
    }
    else
    {
        FEAudio::PlayAnimAudioEvent("sfx_deny", false);
    }
}

void StadiumSelectSceneV2::OnMenuBack()
{
    GameSceneManager::Instance()->PopEntireStack();
    GameSceneManager::Instance()->Push(SCENE_CHOOSE_CAPTAINS, SCREEN_BACK, true);
}

bool StadiumSelectSceneV2::CanProceed(eStadiumID stadium)
{
    bool canProceed = true;
    switch (stadium)
    {
    case STAD_DK_DAISY:
        canProceed = GameInfoManager::Instance()->IsKongaUnlocked();
        break;
    case STAD_YOSHI_STADIUM:
        canProceed = GameInfoManager::Instance()->IsYoshiUnlocked();
        break;
    case STAD_SUPER_STADIUM:
        canProceed = GameInfoManager::Instance()->IsSuperStadiumUnlocked();
        break;
    }
    return canProceed;
}

bool StadiumSelectSceneV2::IsStadiumUnlocked(eStadiumID stadium)
{
    bool returnValue;
    switch (stadium)
    {
    case STAD_MARIO_STADIUM:
        returnValue = true;
        break;
    case STAD_PEACH_TOAD_STADIUM:
        returnValue = true;
        break;
    case STAD_DK_DAISY:
        returnValue = GameInfoManager::Instance()->IsKongaUnlocked();
        break;
    case STAD_WARIO_STADIUM:
        returnValue = true;
        break;
    case STAD_YOSHI_STADIUM:
        returnValue = GameInfoManager::Instance()->IsYoshiUnlocked();
        break;
    case STAD_SUPER_STADIUM:
        returnValue = GameInfoManager::Instance()->IsSuperStadiumUnlocked();
        break;
    case STAD_FORBIDDEN_DOME:
        returnValue = GameInfoManager::Instance()->IsForbiddenUnlocked();
        break;
    default:
        returnValue = true;
        break;
    }
    return returnValue;
}
