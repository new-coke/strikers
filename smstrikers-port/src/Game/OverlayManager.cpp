#include "Game/OverlayManager.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/FE/feInGameMessengerManager.h"
#include "Game/FE/feSceneManager.h"
#include "Game/GameInfo.h"
#include "Game/OverlayHandlerHUD.h"
#include "NL/nlSingleton.h"
#include "NL/nlTask.h"
#include "NL/nlBasicString.h"
#include "Game/OverlayHandlerInGameText.h"
#include "Game/FE/feNSNMessenger.h"
#include "Game/OverlayHandlerGoal.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Render/Presentation.h"
#include "Game/ResetTask.h"
#include "Game/FE/Overlay/OverlayHandlerSummary.h"
#include "Game/Goalie.h"
#include "NL/nlConfig.h"
#include "Game/main.h"

template <>
OverlayManager* nlSingleton<OverlayManager>::s_pInstance = 0;

static bool isSlowMotionOn;
static bool isGoalScored;

/**
 * Offset/Address/Size: 0xB48 | 0x800C8974 | size: 0x60
 */
OverlayManager::OverlayManager()
{
    this->mInGameTextOverlay = nullptr;
    this->mIsHUDSlideIn = false;
    this->mDoHUDSlideIn = false;
    this->mIsInHighlights = false;
    this->mIsDemoSlideVisible = false;
    this->mHUDDelay = 0.0f;
    this->mIGMessengerManager = nullptr;
}

/**
 * Offset/Address/Size: 0xAE8 | 0x800C8914 | size: 0x60
 */
OverlayManager::~OverlayManager()
{
}

/**
 * Offset/Address/Size: 0xA9C | 0x800C88C8 | size: 0x4C
 */
BaseSceneHandler* OverlayManager::Push(SceneList newOverlay, ScreenMovement movement, bool popFirst)
{
    BaseSceneHandler* newscene = BaseGameSceneManager::Push(newOverlay, movement, popFirst);
    if (newscene != NULL && newOverlay == OVERLAY_TEXT)
    {
        this->mInGameTextOverlay = (InGameTextOverlay*)newscene;
    }
    return newscene;
}

/**
 * Offset/Address/Size: 0xA7C | 0x800C88A8 | size: 0x20
 */
void OverlayManager::Pop()
{
    BaseGameSceneManager::Pop();
}

/**
 * Offset/Address/Size: 0xA50 | 0x800C887C | size: 0x2C
 */
void OverlayManager::SetCurrentTextOverlaySlide(OverlaySlideName slideName)
{
    if (this->mInGameTextOverlay != NULL)
    {
        mInGameTextOverlay->SetSlide(slideName);
    }
}

/**
 * Offset/Address/Size: 0x968 | 0x800C8794 | size: 0xE8
 */
void OverlayManager::Update(float fDeltaT)
{
    if (this->mIGMessengerManager != nullptr)
    {
        this->mIGMessengerManager->Update(fDeltaT);
    }
    if (this->mHUDDelay > 0.0f && nlTaskManager::m_pInstance->m_CurrState == 2)
    {
        this->mHUDDelay -= fDeltaT;
        if (this->mHUDDelay <= 0.0f)
        {
            this->mHUDDelay = 0.0f;
            if (mDoHUDSlideIn)
            {
                this->mHUDDelay = 0.0f;
                if (!mIsHUDSlideIn)
                {
                    HUDOverlay* hudSlideIn = (HUDOverlay*)GetScene(OVERLAY_HUD);
                    hudSlideIn->SetSlideIn();
                    mIsHUDSlideIn = true;
                }
            }
            else
            {
                this->mHUDDelay = 0.0f;
                if (mIsHUDSlideIn == 1)
                { // For some reason doing (mIsHudSlideIn) doesn't match?
                    HUDOverlay* hudSlideOut = (HUDOverlay*)GetScene(OVERLAY_HUD);
                    hudSlideOut->SetSlideOut();
                    mIsHUDSlideIn = false;
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x330 | 0x800C815C | size: 0x638
 */
void OverlayManager::FEEventHandler(Event* pEvent, void* pParam)
{
    if (nlTaskManager::m_pInstance->m_CurrState == 4)
        return;

    switch (pEvent->m_uEventID)
    {
    case 3:
    {
        if (g_e3_Build && nlSingleton<GameInfoManager>::Instance()->IsInDemoMode())
        {
            bool notimeout = GetConfigBool(Config::Global(), "notimeout", false);
            if (!notimeout)
            {
                RESET_STATE state = ResetTask::s_ResetState;
                if (state == RS_RUNNING)
                    state = RS_STARTRESET;
                ResetTask::s_ResetState = state;
                return;
            }
        }
        AudioLoader::StartFEStream("FE_Game_Over", true, "Music");
        SummaryOverlay* summaryOverlay = (SummaryOverlay*)nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_SUMMARY, SCREEN_NOTHING, false);
        summaryOverlay->mButtonState = ButtonComponent::BS_A_ONLY;
        OverlayManager* inst = nlSingleton<OverlayManager>::s_pInstance;
        inst->mHUDDelay = 0.0f;
        if (inst->mIsHUDSlideIn == 1)
        {
            HUDOverlay* HUD = (HUDOverlay*)inst->GetScene(OVERLAY_HUD);
            HUD->SetSlideOut();
            inst->mIsHUDSlideIn = false;
        }
        Presentation::Instance().StopOverlay();
        HUDOverlay* HUD = (HUDOverlay*)nlSingleton<OverlayManager>::Instance()->GetScene(OVERLAY_HUD);
        HUD->ResetScores();
        break;
    }
    case 5:
    {
        isGoalScored = true;
        GoalScoredData* pGoalData;
        if ((s32)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pGoalData = NULL;
        }
        else if ((s32)port_event_data_id(&pEvent->m_data) != (s32)GoalScoredData::ID)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pGoalData = NULL;
        }
        else
        {
            pGoalData = (GoalScoredData*)&pEvent->m_data;
        }
        u32 goalType = pGoalData->uGoalType;
        if (goalType != 2 && goalType != 6)
        {
            OverlayManager* inst = nlSingleton<OverlayManager>::s_pInstance;
            inst->mHUDDelay = 0.0f;
            if (inst->mIsHUDSlideIn == 1)
            {
                HUDOverlay* HUD = (HUDOverlay*)inst->GetScene(OVERLAY_HUD);
                HUD->SetSlideOut();
                inst->mIsHUDSlideIn = false;
            }
        }
        HUDOverlay* HUD = (HUDOverlay*)nlSingleton<OverlayManager>::Instance()->GetScene(OVERLAY_HUD);
        HUD->UpdateScore();
        break;
    }
    case 9:
    {
        nlSingleton<OverlayManager>::Instance()->SetVisible(OVERLAY_HUD, true, true);
        OverlayManager* inst = nlSingleton<OverlayManager>::s_pInstance;
        inst->mHUDDelay = 0.25f;
        inst->mDoHUDSlideIn = true;
        isGoalScored = false;
        HUDOverlay* HUD = (HUDOverlay*)nlSingleton<OverlayManager>::Instance()->GetScene(OVERLAY_HUD);
        HUD->DisplayNewScore();
        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            NSNMessengerScene* messenger = (NSNMessengerScene*)nlSingleton<OverlayManager>::Instance()->GetScene(OVERLAY_LESSON_TICKER);
            messenger->ForceMessengerVisibleNow();
        }
        break;
    }
    case 10:
    {
        nlSingleton<OverlayManager>::Instance()->SetVisible(OVERLAY_TEXT, false, false);
        break;
    }
    case 0x46:
    {
        isSlowMotionOn = true;
        OverlayManager* inst = nlSingleton<OverlayManager>::s_pInstance;
        inst->mHUDDelay = 0.0f;
        if (inst->mIsHUDSlideIn == 1)
        {
            HUDOverlay* HUD = (HUDOverlay*)inst->GetScene(OVERLAY_HUD);
            HUD->SetSlideOut();
            inst->mIsHUDSlideIn = false;
        }
        break;
    }
    case 0x3f:
    {
        OverlayManager* inst = nlSingleton<OverlayManager>::s_pInstance;
        inst->mHUDDelay = 0.0f;
        if (inst->mIsHUDSlideIn == 1)
        {
            HUDOverlay* HUD = (HUDOverlay*)inst->GetScene(OVERLAY_HUD);
            HUD->SetSlideOut();
            inst->mIsHUDSlideIn = false;
        }
        break;
    }
    case 0x47:
    {
        if (isSlowMotionOn == true)
        {
            OverlayManager* inst = nlSingleton<OverlayManager>::s_pInstance;
            inst->mHUDDelay = 1.0f;
            inst->mDoHUDSlideIn = true;
        }
        isSlowMotionOn = false;
        break;
    }
    case 0x41:
    {
        if (isGoalScored)
            break;
        OverlayManager* inst = nlSingleton<OverlayManager>::s_pInstance;
        inst->mHUDDelay = 0.0f;
        if (!inst->mIsHUDSlideIn)
        {
            HUDOverlay* HUD = (HUDOverlay*)inst->GetScene(OVERLAY_HUD);
            HUD->SetSlideIn();
            inst->mIsHUDSlideIn = true;
        }
        break;
    }
    }
}

/**
 * Offset/Address/Size: 0x1A8 | 0x800C7FD4 | size: 0x188
 */
void OverlayManager::SetVisible(SceneList scene, bool visibility, bool overrideStateSettings)
{
    if (nlSingleton<GameInfoManager>::Instance()->mCurrentMode == GameInfoManager::GM_DEMO && scene != OVERLAY_HUD)
    {
        return;
    }

    BasicString<char, Detail::TempStringAllocator> fileName = GetFileName(scene);
    u32 uHashID = nlStringLowerHash(fileName.c_str());
    BaseOverlayHandler* sceneHandler = (BaseOverlayHandler*)nlSingleton<FESceneManager>::Instance()->GetSceneHandler(uHashID);
    u32 state = nlTaskManager::m_pInstance->m_CurrState;

    if (overrideStateSettings || (sceneHandler->mVisibilityMask & state))
    {
        sceneHandler->SetVisible(visibility);
    }
}

/**
 * Offset/Address/Size: 0xCC | 0x800C7EF8 | size: 0xDC
 */
void OverlayManager::HandleStateTransition(u32 to, u32 param_2)
{
    for (u32 i = 0; i < mCurrentStackDepth; i++)
    {
        SceneList sceneType = m_sceneStack[i];
        if (sceneType <= SCENE_LAST)
        {
            continue;
        }

        BaseSceneHandler* handler = mBaseSceneHandlerStack[i];
        if (handler == nullptr)
        {
            continue;
        }

        if (sceneType == OVERLAY_TEXT && mIsInHighlights)
        {
            continue;
        }

        BaseOverlayHandler* overlayHandler = static_cast<BaseOverlayHandler*>(handler);
        if ((overlayHandler->mVisibilityMask & param_2) != 0)
        {
            if (overlayHandler->mWasLastVisible == false)
            {
                continue;
            }
            overlayHandler->SetVisible(true);
        }
        else
        {
            overlayHandler->mWasLastVisible = overlayHandler->m_bVisible;
            overlayHandler->SetVisible(false);
        }
    }
}

/**
 * Offset/Address/Size: 0x88 | 0x800C7EB4 | size: 0x44
 */
void OverlayManager::DestroyMessengerManager()
{
    if (this->mIGMessengerManager != NULL)
    {
        delete this->mIGMessengerManager;
        this->mIGMessengerManager = 0;
    }
}

/**
 * Offset/Address/Size: 0x28 | 0x800C7E54 | size: 0x60
 */
void OverlayManager::ShowDemoSlide()
{
    class BaseSceneHandler* scene;
    if (!this->mIsDemoSlideVisible)
    {
        scene = BaseGameSceneManager::GetScene(OVERLAY_DEMO);
        if (scene != nullptr)
        {
            scene->SetVisible(true);
            this->mIsDemoSlideVisible = true;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800C7E2C | size: 0x28
 */
void OverlayManager::RestartGoalOverlay()
{
    GoalOverlay* goalOverlay = (GoalOverlay*)GetScene(OVERLAY_GOAL);
    goalOverlay->Restart();
}
