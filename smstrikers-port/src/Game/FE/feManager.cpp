#include "Game/FE/feManager.h"

#include "Game/Camera/CameraMan.h"
#include "Game/Camera/animcam.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feNSNMessenger.h"
#include "Game/FE/feSceneManager.h"
#include "Game/Game.h"
#include "Game/Team.h"
#include <dolphin/os.h>
#include "Game/GameInfo.h"
#include "Game/OverlayManager.h"
#include "Game/Render/Presentation.h"
#include "Game/RumbleActions.h"
#include "Game/SH/SHLessonSelect.h"
#include "Game/SH/SHPause.h"
#include "Game/Sys/eventman.h"
#include "NL/glx/glxSwap.h"
#include "NL/nlTask.h"

extern float g_AllActorsHidden;
extern bool g_e3_Build;

cAnimCamera* FrontEnd::m_pPauseMenuCamera = nullptr;
bool FrontEnd::m_bGameOver = false;
bool FrontEnd::m_bInPauseMenuState = false;
float FrontEnd::m_fDemoTimeElapsed = 0.0f;
unsigned char FrontEnd::m_ctrlConnectedState[4];
float FrontEnd::m_pauseDelay = 0.0f;

// Global variables
static unsigned char AlreadyStartedStrikers101Menu;
static unsigned char DontCheckForControllerRemovalHack;
unsigned char g_JaapAndJacksNastyHackBecauseWeDoNotKnowDifferenceBetweenPausePauseAndPostGamePause;

eFEState FrontEnd::m_feStateCurrent = eFE_INVALID;
eFEState FrontEnd::m_feStatePending = eFE_INVALID;
eFEState FrontEnd::m_feStatePrevious = eFE_INVALID;
unsigned int FrontEnd::m_lastTaskState = -1;
eFEINPUT_PAD FrontEnd::m_hitStartPad = FE_ALL_PADS;
FrontEnd::MenuEnterType FrontEnd::m_menuType = MET_INVALID;

/**
 * Offset/Address/Size: 0x0 | 0x80094C84 | size: 0x44
 */
void FrontEnd::ReturnToFE()
{
    OSReport("[fe] return-to-fe\n");
    glxSwapSetBlack(true);
    if (nlTaskManager::m_pInstance->m_CurrState == 1)
    {
        nlTaskManager::m_pInstance->m_Locked = false;
    }
    nlTaskManager::SetNextState(4);
}

/**
 * Offset/Address/Size: 0x44 | 0x80094CC8 | size: 0x1A0
 */
void FrontEnd::UpdateForGame(float fDeltaT)
{
    if (m_bGameOver)
        return;

    if (!m_bInPauseMenuState && m_pauseDelay <= 0.0f)
    {
        nlTaskManager* taskManager = nlTaskManager::m_pInstance;
        if (taskManager->m_CurrState != 1 && taskManager->m_PendingState == taskManager->m_CurrState)
        {
            for (int i = 0; i < 4; i++)
            {
                if (g_pFEInput->JustPressed((eFEINPUT_PAD)i, 0x1000, false, NULL))
                {
                    m_hitStartPad = (eFEINPUT_PAD)i;
                    EnterMenuState(MET_PAUSE);
                }
                if (m_bInPauseMenuState)
                    break;
            }
        }
    }

    if (Presentation::Instance().DuringEndOfGamePresentation())
    {
        DontCheckForControllerRemovalHack = 1;
    }

    if (DontCheckForControllerRemovalHack)
        return;

    if (m_bInPauseMenuState)
        return;

    if (!(m_pauseDelay <= 0.0f))
        return;

    if (nlTaskManager::m_pInstance->m_CurrState == 1)
        return;

    if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode && !AlreadyStartedStrikers101Menu)
        return;

    for (int i = 0; i < 4; i++)
    {
        bool curConnected = g_pFEInput->IsConnected((eFEINPUT_PAD)i);
        if (m_ctrlConnectedState[i] == 1 && !curConnected)
        {
            if (nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((unsigned short)i) != -1)
            {
                EnterMenuState(MET_CHOOSESIDES);
            }
        }
        m_ctrlConnectedState[i] = curConnected;
        if (m_bInPauseMenuState)
            break;
    }
}

/**
 * Offset/Address/Size: 0x1E4 | 0x80094E68 | size: 0x33C
 */
void FrontEnd::Update(float fTimeDelta)
{
    m_pauseDelay -= fTimeDelta;
    if (m_pauseDelay < 0.0f)
    {
        m_pauseDelay = 0.0f;
    }

    if (nlSingleton<GameInfoManager>::Instance()->IsInDemoMode())
    {
        m_fDemoTimeElapsed += fTimeDelta;
        if (m_fDemoTimeElapsed < 3.0f)
        {
        }
        else
        {
            nlSingleton<OverlayManager>::Instance()->ShowDemoSlide();
            if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x1F00, false, NULL))
            {
                glxSwapSetBlack(true);
                if (nlTaskManager::m_pInstance->m_CurrState == 1)
                {
                    nlTaskManager::m_pInstance->m_Locked = false;
                }
                nlTaskManager::SetNextState(4);
                m_fDemoTimeElapsed = 0.0f;
            }
            else if (!g_e3_Build)
            {
                static float maxBackendDemoTime;
                static signed char init;

                if (!init)
                {
                    maxBackendDemoTime = GetConfigFloat(Config::Global(), "be_demo_mode_time_out", 60.0f);
                    init = 1;
                }

                if (m_fDemoTimeElapsed >= maxBackendDemoTime)
                {
                    glxSwapSetBlack(true);
                    if (nlTaskManager::m_pInstance->m_CurrState == 1)
                    {
                        nlTaskManager::m_pInstance->m_Locked = false;
                    }
                    nlTaskManager::SetNextState(4);
                    m_fDemoTimeElapsed = 0.0f;
                }
            }
        }
    }
    else
    {
        UpdateForGame(fTimeDelta);
    }

    nlSingleton<OverlayManager>::Instance()->Update(fTimeDelta);
    m_feStateCurrent = m_feStatePending;

    switch (m_feStateCurrent)
    {
    case 6:
    case 4:
        break;

    case 3:
    {
        BaseSceneHandler* scene;

        g_pBall->SetVisible(true);
        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode && !AlreadyStartedStrikers101Menu)
        {
            if (m_pauseDelay <= 0.0f)
            {
                EnterMenuState(MET_PAUSE);
                scene = nlSingleton<OverlayManager>::Instance()->GetScene(OVERLAY_LESSON_TICKER);
                if (scene != NULL)
                {
                    scene = (BaseSceneHandler*)((char*)scene - 4);
                }
                ((NSNMessengerScene*)(void*)scene)->EnableScrolling(true);
                SetTickerLesson(-1);
                m_lastTaskState = 2;
                m_feStatePrevious = eFE_INGAME;
                AlreadyStartedStrikers101Menu = 1;
            }
        }
        else
        {
            m_feStatePending = eFE_INGAME;
        }

        m_bGameOver = false;
        break;
    }

    case 5:
    {
        nlTaskManager::SetNextState(1);
        g_pBall->SetVisible(false);

        m_pPauseMenuCamera = new (nlMalloc(sizeof(cAnimCamera), 8, false)) cAnimCamera();
        ((cAnimCamera*)m_pPauseMenuCamera)->SelectCameraAnimation("pause");
        cCameraManager::PushCamera((cBaseCamera*)m_pPauseMenuCamera);
        ((cAnimCamera*)m_pPauseMenuCamera)->m_fAnimationSpeed = 0.3f;
        m_feStatePending = eFE_WAIT_USER_END_GAME_INPUT;
        break;
    }

    case 8:
        if (nlTaskManager::m_pInstance->m_CurrState == 1)
        {
            nlTaskManager::m_pInstance->m_Locked = true;
        }
        break;
    }
}

/**
 * Offset/Address/Size: 0x520 | 0x800951A4 | size: 0x90
 */
void FrontEnd::ExitMenuState()
{
    if (!FESceneManager::Instance()->AreAllScenesValid())
        return;

    m_bInPauseMenuState = false;
    m_menuType = MET_INVALID;
    m_feStatePending = m_feStatePrevious;
    nlTaskManager::m_pInstance->m_Locked = false;
    nlTaskManager::SetNextState(m_lastTaskState);
    OverlayManager::Instance()->Pop();
    g_pEventManager->CreateValidEvent(1, 0x14);
    g_pFEInput->EnableAnalogToDPadMapping(FE_ALL_PADS, false);
    m_pauseDelay = 0.25f;
}

/**
 * Offset/Address/Size: 0x5B0 | 0x80095234 | size: 0x188
 */
void FrontEnd::EnterMenuState(FrontEnd::MenuEnterType menuType)
{
    int i;
    cGlobalPad* globalPad;
    nlTaskManager* taskManager;

    m_bInPauseMenuState = true;
    m_menuType = menuType;
    m_feStatePrevious = m_feStateCurrent;
    taskManager = nlTaskManager::m_pInstance;
    if (taskManager->m_CurrState != taskManager->m_PendingState)
    {
        m_lastTaskState = taskManager->m_PendingState;
    }
    else
    {
        m_lastTaskState = taskManager->m_CurrState;
    }
    for (i = 0; i < 4; i++)
    {
        globalPad = g_pFEInput->GetGlobalPad((eFEINPUT_PAD)i);
        if (globalPad != NULL)
        {
            StopRumbleAction(globalPad);
        }
    }
    nlTaskManager::SetNextState(1);
    if (nlSingleton<OverlayManager>::Instance()->IsOnStack(SCENE_SUPER_LOADING))
    {
        nlSingleton<OverlayManager>::Instance()->Pop();
        nlSingleton<FESceneManager>::Instance()->ForceImmediateStackProcessing();
    }
    switch (m_menuType)
    {
    case MET_PAUSE:
        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_STRIKERS_101_PAUSE, SCREEN_NOTHING, false);
        }
        else
        {
            nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_PAUSE, SCREEN_NOTHING, false);
        }
        PauseMenuScene::mControllingInput = FE_ALL_PADS;
        break;
    case MET_CHOOSESIDES:
        nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_CHOOSE_SIDES, SCREEN_NOTHING, false);
        PauseMenuScene::mControllingInput = FE_ALL_PADS;
        break;
    case MET_END:
    default:
        break;
    }
    m_feStatePending = eFE_PROCESS_MENU_INPUT;
    g_pEventManager->CreateValidEvent(0, 0x14);
    g_pFEInput->EnableAnalogToDPadMapping(FE_ALL_PADS, true);
}

/**
 * Offset/Address/Size: 0x738 | 0x800953BC | size: 0x70
 */
void FrontEnd::ExitWinnerScreen()
{
    cCameraManager::PopCameraWithTransition(1.0f, eCT_EASE_IN, 0);
    delete m_pPauseMenuCamera;
    m_pPauseMenuCamera = 0;
    g_AllActorsHidden = 0.5f;
    g_JaapAndJacksNastyHackBecauseWeDoNotKnowDifferenceBetweenPausePauseAndPostGamePause = 1;
    DontCheckForControllerRemovalHack = 0;
    nlTaskManager::SetNextState(2);
}

/**
 * Offset/Address/Size: 0x7A8 | 0x8009542C | size: 0x48
 */
void FrontEnd::EnterStartScreen(bool bStraightToKickoff)
{
    bool isInStrikers101 = false;
    if (GameInfoManager::Instance()->mIsInStrikers101Mode)
    {
        isInStrikers101 = true;
    }
    g_pGame->BeginGame(false, isInStrikers101);
    m_feStatePending = eFE_WAIT_FOR_LOAD;
}

/**
 * Offset/Address/Size: 0x7F0 | 0x80095474 | size: 0x54
 */
void FrontEnd::SetControllerState()
{
    for (int i = 0; i < 4; i++)
    {
        m_ctrlConnectedState[i] = g_pFEInput->IsConnected((eFEINPUT_PAD)i);
    }
}

/**
 * Offset/Address/Size: 0x844 | 0x800954C8 | size: 0x10
 */
void FrontEnd::Destroy()
{
    m_feStateCurrent = eFE_INVALID;
    m_feStatePending = eFE_INVALID;
}

/**
 * Offset/Address/Size: 0x854 | 0x800954D8 | size: 0x48
 */
bool FrontEnd::Initialize()
{
    m_feStateCurrent = eFE_INVALID;
    m_feStatePending = eFE_INVALID;
    m_pPauseMenuCamera = 0;
    m_hitStartPad = FE_ALL_PADS;
    m_menuType = MET_INVALID;
    m_bGameOver = 0;
    m_bInPauseMenuState = 0;
    m_fDemoTimeElapsed = 0.0f;
    m_pauseDelay = 1.5f;
    AlreadyStartedStrikers101Menu = 0;
    DontCheckForControllerRemovalHack = 0;
    return true;
}

/**
 * Offset/Address/Size: 0x89C | 0x80095520 | size: 0x9C
 */
void FrontEnd::FEEventHandler(Event* pEvent, void* pParam)
{
    switch (pEvent->m_uEventID)
    {
    case 3:
        OSReport("[match] over score=%d-%d\n",
                 (g_pTeams[0] != NULL) ? g_pTeams[0]->m_nScore : -1,
                 (g_pTeams[1] != NULL) ? g_pTeams[1]->m_nScore : -1);
        m_bGameOver = true;
        m_feStatePending = eFE_END_GAME;
        break;
    case 9:
        m_feStatePending = eFE_PRE_GAME_START;
        break;
    case 0x1C:
        if (nlSingleton<OverlayManager>::s_pInstance != NULL)
        {
            if (nlSingleton<OverlayManager>::Instance()->IsOnStack(SCENE_SUPER_LOADING))
            {
                nlSingleton<OverlayManager>::Instance()->Pop();
                nlSingleton<FESceneManager>::Instance()->ForceImmediateStackProcessing();
            }
        }
        break;
    }
}
