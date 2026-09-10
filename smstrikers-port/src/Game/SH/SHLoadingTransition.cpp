#include "Game/SH/SHLoadingTransition.h"
#include "Game/GameSceneManager.h"
#include "Game/GameInfo.h"
#include "Game/FE/feInput.h"
#include "Game/SH/SHLoading.h"
#include "Game/TrophyTextures.h"

extern bool g_e3_Build;
extern FEInput* g_pFEInput;

/**
 * Offset/Address/Size: 0x158 | 0x800DA214 | size: 0x70
 */
LoadingTransitionScene::LoadingTransitionScene()
{
    g_pFEInput->PushExclusiveInputLock(this, SCENE_LOADING_TRANSITION);
}

/**
 * Offset/Address/Size: 0xE0 | 0x800DA19C | size: 0x78
 */
LoadingTransitionScene::~LoadingTransitionScene()
{
    g_pFEInput->PopExclusiveInputLock(this);
}

/**
 * Offset/Address/Size: 0xCC | 0x800DA188 | size: 0x14
 */
void LoadingTransitionScene::SceneCreated()
{
    m_pFEPresentation->m_currentSlide->m_uPlayMode = TLPM_STOP_AT_END;
}

/**
 * Offset/Address/Size: 0x0 | 0x800DA0BC | size: 0xCC
 */
void LoadingTransitionScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);

    TLSlide* slide = m_pFEPresentation->m_currentSlide;

    if (slide->m_time >= slide->m_start + slide->m_duration)
    {
        GameSceneManager::GetInstance()->Pop();

        if (!g_e3_Build)
        {
            if (!GameInfoManager::GetInstance()->IsInCupOrTournamentMode())
            {
                if (!GameInfoManager::GetInstance()->mIsInStrikers101Mode)
                {
                    GameSceneManager::GetInstance()->Pop();
                }
            }
        }

        SuperLoadingScene* scene = (SuperLoadingScene*)GameSceneManager::GetInstance()->Push(SCENE_SUPER_LOADING, SCREEN_NOTHING, false);
        scene->mType = SuperLoadingScene::TT_IN;
    }
}
