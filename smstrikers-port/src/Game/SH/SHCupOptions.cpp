#include "Game/SH/SHCupOptions.h"

#include "Game/FE/FEAudio.h"
#include "Game/FE/feInput.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "Game/SH/SHSaveLoad.h"
#include "Game/SH/SHTournSetParams.h"
#include "NL/nlMemory.h"

/**
 * Offset/Address/Size: 0x374 | 0x800E9D1C | size: 0x40
 */
CupOptionsScene::CupOptionsScene(SceneList forwardScene, SceneList backScene)
{
    m_backScene = backScene;
    m_forwardScene = forwardScene;
}

/**
 * Offset/Address/Size: 0x2E8 | 0x800E9C90 | size: 0x8C
 */
CupOptionsScene::~CupOptionsScene()
{
    delete m_gameplayMenu;
}

/**
 * Offset/Address/Size: 0x190 | 0x800E9B38 | size: 0x158
 */
void CupOptionsScene::SceneCreated()
{
    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();
    OptionsGameplayMenuV2* menu;

    if (GameInfoManager::Instance()->IsInRegularCupMode())
    {
        bool legendUnlocked = GameInfoManager::Instance()->IsLegendSkillUnlocked();
        menu = (OptionsGameplayMenuV2*)nlMalloc(sizeof(OptionsGameplayMenuV2), 8, false);
        // if (menu != NULL)
        {
            menu = new (menu) OptionsGameplayMenuV2(
                pres,
                ButtonComponent::BS_A_AND_B,
                GameInfoManager::Instance()->mCurrentCup->mCupSettings,
                !legendUnlocked ? 4 : -1);
        }
        m_gameplayMenu = menu;
    }
    else if (GameInfoManager::Instance()->IsInSuperCupMode())
    {
        menu = (OptionsGameplayMenuV2*)nlMalloc(sizeof(OptionsGameplayMenuV2), 8, false);
        // if (menu != NULL)
        {
            menu = new (menu) OptionsGameplayMenuV2(
                pres,
                ButtonComponent::BS_A_AND_B,
                GameInfoManager::Instance()->mCurrentCup->mCupSettings,
                1);
        }
        m_gameplayMenu = menu;
    }
    else
    {
        bool legendUnlocked = GameInfoManager::Instance()->IsLegendSkillUnlocked();
        menu = (OptionsGameplayMenuV2*)nlMalloc(sizeof(OptionsGameplayMenuV2), 8, false);
        // if (menu != NULL)
        {
            menu = new (menu) OptionsGameplayMenuV2(
                pres,
                ButtonComponent::BS_A_AND_B,
                GameInfoManager::Instance()->mCurrentCup->mCupSettings,
                !legendUnlocked ? 4 : -1);
        }
        m_gameplayMenu = menu;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800E99A8 | size: 0x190
 */
void CupOptionsScene::Update(float dt)
{
    BaseSceneHandler::Update(dt);

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        // A button - confirm and go forward
        m_gameplayMenu->Save();

        int sceneType = GameSceneManager::Instance()->GetSceneType(this);
        GameSceneManager::Instance()->Push(m_forwardScene, SCREEN_FORWARD, true);

        // If not in cup options scene and IO is enabled, push save scene
        if (sceneType != SCENE_CUP_OPTIONS_INITIAL_CUP && sceneType != SCENE_CUP_OPTIONS_INITIAL_SUPER && sceneType != SCENE_CUP_OPTIONS_INITIAL_TOURN)
        {
            if (SaveLoadScene::IsIOEnabled())
            {
                GameSceneManager::Instance()->Push(SCENE_SAVE, SCREEN_NOTHING, false);
            }
        }

        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
    {
        // B button - go back
        TournSetParamsScene* scene = (TournSetParamsScene*)GameSceneManager::Instance()->Push(m_backScene, SCREEN_BACK, true);

        if (m_backScene == SCENE_TOURN_SETPARAMS)
        {
            GameInfoManager* gim = GameInfoManager::Instance();

            u8 numTeams = gim->mCustomTournamentInfo.m_numTeams;
            eTournamentMode tournMode = gim->mCustomTournamentInfo.m_tournMode;
            u8 numGamesPerTeam = gim->mCustomTournamentInfo.m_numGamesPerTeam;

            scene->SetInitialParams(tournMode == 0, numTeams, numGamesPerTeam);
        }

        FEAudio::PlayAnimAudioEvent("sfx_back", false);
    }
    else
    {
        m_gameplayMenu->Update(dt);
    }
}
