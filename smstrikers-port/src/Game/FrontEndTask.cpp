#include "Game/FrontEndTask.h"

#include "Game/FE/feInput.h"
#include "Game/FE/feManager.h"
#include "Game/FE/feResourceManager.h"
#include "Game/FE/feSceneManager.h"
#include "Game/FE/feAnimModelManager.h"
#include "Game/GameInfo.h"
#include "Game/DB/StatsTracker.h"
#include "Game/GameSceneManager.h"
#include "Game/ResetTask.h"
#include "NL/globalpad.h"
#include "NL/nlTask.h"
#include "NL/nlConfig.h"
#include "NL/nlLexicalCast.h"
#include "NL/gl/gl.h"
#include "dolphin/pad.h"
#include <math.h>
#include "Game/main.h"

float g_fUpTime;
float g_fIdleGameTime;

extern FrontEnd* g_pFrontEnd;
extern FEInput* g_pFEInput;

namespace TakeGameMemSnapshot
{
void Update(float);
}

/**
 * Offset/Address/Size: 0x484 | 0x8017071C | size: 0x2A8
 */
void FrontEndTask::Run(float dt)
{
    int buttonPressCount;
    cGlobalPad* pad;

    g_fUpTime += dt;
    g_pFEInput->Update(dt);
    HandleE3IdleReset(dt);

    if (nlTaskManager::m_pInstance->m_CurrState == 2)
    {
        for (int padIdx = 0; padIdx < 4; padIdx++)
        {
            short playingSide = nlSingleton<GameInfoManager>::Instance()->GetPlayingSide(padIdx);
            if (playingSide == -1)
            {
                continue;
            }

            pad = cPadManager::GetPad(padIdx);
            if (!pad->IsConnected())
            {
                continue;
            }

            buttonPressCount = 0;

            if (pad->JustPressed(PAD_BUTTON_A, false))
            {
                buttonPressCount++;
            }
            if (pad->JustPressed(PAD_BUTTON_B, false))
            {
                buttonPressCount++;
            }
            if (pad->JustPressed(PAD_BUTTON_X, false))
            {
                buttonPressCount++;
            }
            if (pad->JustPressed(PAD_BUTTON_Y, false))
            {
                buttonPressCount++;
            }

            if (buttonPressCount != 0)
            {
                int side = (short)(GameInfoManager::Instance()->GetPlayingSide(padIdx));
                StatsTracker::Instance()->AddStat(STATS_BUTTON_PRESSES, side, -1, buttonPressCount);
                StatsTracker::Instance()->AddUserStatByPad(STATS_BUTTON_PRESSES, padIdx, buttonPressCount);
            }
        }
    }

    if (nlTaskManager::m_pInstance->m_CurrState != 4)
    {
        FrontEnd::Update(dt);
    }

    if (FEResourceManager::Instance() != nullptr)
    {
        FEResourceManager::Instance()->Run(dt);
        FESceneManager::Instance()->Update(dt);
        FESceneManager::Instance()->RenderActiveScenes();
        if (!FESceneManager::Instance()->AreAllScenesValid())
        {
            glDiscardFrame(1);
        }
    }

    FEAnimModelManager* animModelManager = FEAnimModelManager::Instance();
    if (animModelManager != nullptr)
    {
        animModelManager->Update(dt);
    }

    if (nlTaskManager::m_pInstance->m_CurrState != 4)
    {
        Config& config = Config::Global();
        if (GetConfigBool(config, "domemsnapshot", false))
        {
            TakeGameMemSnapshot::Update(dt);
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80170298 | size: 0x484
 */
void FrontEndTask::HandleE3IdleReset(float dt)
{
    if (!g_e3_Build)
    {
        return;
    }

    if (nlSingleton<GameInfoManager>::Instance()->IsInDemoMode())
    {
        if (nlTaskManager::m_pInstance->m_CurrState & 0x2)
        {
            g_fIdleGameTime = 0.0f;
            return;
        }
    }

    Config& config = Config::Global();
    if (GetConfigBool(config, "notimeout", false))
    {
        return;
    }

    bool allIdle = true;

    for (int padIdx = 0; padIdx < 4; padIdx++)
    {
        cGlobalPad* pad = cPadManager::GetPad(padIdx);
        if (!pad->IsConnected())
        {
            continue;
        }

        if (pad->IsPressed(PAD_BUTTON_START, false)
            || pad->IsPressed(PAD_BUTTON_A, false)
            || pad->IsPressed(PAD_BUTTON_B, false)
            || pad->IsPressed(PAD_BUTTON_X, false)
            || pad->IsPressed(PAD_BUTTON_X, false) // this code/check duplication seems to be in the original code! :)
            || pad->IsPressed(PAD_BUTTON_Y, false)
            || pad->IsPressed(PAD_TRIGGER_R, false)
            || pad->IsPressed(PAD_TRIGGER_L, false))
        {
            allIdle = false;
            break;
        }

        if (((float)fabs(pad->AnalogLeftX()) >= 0.5f)
            || ((float)fabs(pad->AnalogLeftY()) >= 0.5f)
            || ((float)fabs(pad->AnalogRightX()) >= 0.5f)
            || ((float)fabs(pad->AnalogRightY()) >= 0.5f))
        {
            allIdle = false;
            break;
        }

        if (pad->IsPressed(PAD_BUTTON_UP, false)
            || pad->IsPressed(PAD_BUTTON_DOWN, false)
            || pad->IsPressed(PAD_BUTTON_LEFT, false)
            || pad->IsPressed(PAD_BUTTON_RIGHT, false))
        {
            allIdle = false;
            break;
        }
    }

    if (allIdle)
    {
        if ((nlTaskManager::m_pInstance->m_CurrState & 0x7) != 0)
        {
            g_fIdleGameTime += dt;

            if (g_fIdleGameTime >= 60.0f)
            {
                if (nlTaskManager::m_pInstance->m_CurrState == 4)
                {
                    if (!GameSceneManager::Instance()->IsOnStack(SCENE_TITLE))
                    {
                        GameSceneManager::Instance()->PopEntireStack();
                        FESceneManager::Instance()->ForceImmediateStackProcessing();
                        GameSceneManager::Instance()->Push(SCENE_TITLE, SCREEN_NOTHING, false);
                    }
                }
                else
                {
                    ResetTask::s_ResetMode = 0;
                    ResetTask::s_ResetState = (ResetTask::s_ResetState == RS_RUNNING) ? RS_STARTRESET : ResetTask::s_ResetState;
                }

                g_fIdleGameTime = 0.0f;
            }
        }
    }
    else
    {
        g_fIdleGameTime = 0.0f;
    }
}
