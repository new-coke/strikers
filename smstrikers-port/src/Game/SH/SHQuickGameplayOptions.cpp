#include "Game/SH/SHQuickGameplayOptions.h"

#include "Game/BaseGameSceneManager.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feOptionsSubMenus.h"
#include "Game/FE/fePackage.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "NL/nlMemory.h"

extern FEInput* g_pFEInput;

/**
 * Offset/Address/Size: 0x278 | 0x8010D0BC | size: 0x74
 */
QuickGameplayOptionsScene::QuickGameplayOptionsScene()
    : BaseSceneHandler()
{
    m_pOptionsMenu = NULL;
    g_pFEInput->PushExclusiveInputLock(this, SCENE_QUICK_GAMEPLAY_OPTIONS);
}

/**
 * Offset/Address/Size: 0x1DC | 0x8010D020 | size: 0x9C
 */
QuickGameplayOptionsScene::~QuickGameplayOptionsScene()
{
    if (m_pOptionsMenu != NULL)
    {
        delete m_pOptionsMenu;
    }
    g_pFEInput->PopExclusiveInputLock(this);
}

/**
 * Offset/Address/Size: 0x100 | 0x8010CF44 | size: 0xDC
 */
void QuickGameplayOptionsScene::SceneCreated()
{
    GameInfoManager* pGameInfo = GameInfoManager::GetInstance();

    if (!pGameInfo->mUseCurGameSettings)
    {
        // Restore gameplay options from user's saved settings
        pGameInfo->mCurGameGameplayOptions.SkillLevel = pGameInfo->mUserInfo.mGameplayOptions.SkillLevel;
        pGameInfo->mCurGameGameplayOptions.GameTime = pGameInfo->mUserInfo.mGameplayOptions.GameTime;
        pGameInfo->mCurGameGameplayOptions.PowerUps = pGameInfo->mUserInfo.mGameplayOptions.PowerUps;
        pGameInfo->mCurGameGameplayOptions.Shoot2Score = pGameInfo->mUserInfo.mGameplayOptions.Shoot2Score;
        pGameInfo->mCurGameGameplayOptions.BowserAttackEnabled = pGameInfo->mUserInfo.mGameplayOptions.BowserAttackEnabled;
        pGameInfo->mCurGameGameplayOptions.RumbleEnabled = pGameInfo->mUserInfo.mGameplayOptions.RumbleEnabled;
    }

    FEPresentation* pPresentation = m_pFEScene->m_pFEPackage->GetPresentation();

    int maxSkillLevel = GameInfoManager::GetInstance()->IsLegendSkillUnlocked() ? -1 : 4;

    OptionsGameplayMenuV2* pMem = (OptionsGameplayMenuV2*)nlMalloc(sizeof(OptionsGameplayMenuV2), 8, false);
    pMem = new (pMem) OptionsGameplayMenuV2(pPresentation, ButtonComponent::BS_B_ONLY, GameInfoManager::GetInstance()->mCurGameGameplayOptions, maxSkillLevel);
    m_pOptionsMenu = pMem;
}

/**
 * Offset/Address/Size: 0x0 | 0x8010CE44 | size: 0x100
 */
void QuickGameplayOptionsScene::Update(float dt)
{
    BaseSceneHandler::Update(dt);

    if (!g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, NULL))
        {
            m_pOptionsMenu->Save();

            GameInfoManager* pGameInfo = GameInfoManager::GetInstance();
            pGameInfo->mUserInfo.mGameplayOptions.SkillLevel = pGameInfo->mCurGameGameplayOptions.SkillLevel;
            pGameInfo->mUserInfo.mGameplayOptions.GameTime = pGameInfo->mCurGameGameplayOptions.GameTime;
            pGameInfo->mUserInfo.mGameplayOptions.PowerUps = pGameInfo->mCurGameGameplayOptions.PowerUps;
            pGameInfo->mUserInfo.mGameplayOptions.Shoot2Score = pGameInfo->mCurGameGameplayOptions.Shoot2Score;
            pGameInfo->mUserInfo.mGameplayOptions.BowserAttackEnabled = pGameInfo->mCurGameGameplayOptions.BowserAttackEnabled;
            pGameInfo->mUserInfo.mGameplayOptions.RumbleEnabled = pGameInfo->mCurGameGameplayOptions.RumbleEnabled;

            GameSceneManager::GetInstance()->Pop();
            FEAudio::PlayAnimAudioEvent("sfx_screen_back", false);
            return;
        }
    }

    m_pOptionsMenu->Update(dt);
}
