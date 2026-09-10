#include "Game/SH/SHPauseOptions.h"
#include "Game/SH/SHPause.h"
#include "Game/FE/FEAudio.h"
#include "Game/GameInfo.h"
#include "Game/OverlayManager.h"
#include "NL/nlMemory.h"

/**
 * Offset/Address/Size: 0x230 | 0x800B0424 | size: 0x64
 */
PauseOptionsScene::PauseOptionsScene(PauseOptionsScene::Mode mode)
    : BaseOverlayHandler(1, POSITION_ALL)
{
    m_desiredMode = mode;
    m_pauseMenu = NULL;
    m_controllingInput = FE_ALL_PADS;
}

/**
 * Offset/Address/Size: 0x194 | 0x800B0388 | size: 0x9C
 */
PauseOptionsScene::~PauseOptionsScene()
{
    delete m_pauseMenu;
}

/**
 * Offset/Address/Size: 0xCC | 0x800B02C0 | size: 0xC8
 */
void PauseOptionsScene::SceneCreated()
{
    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();
    OptionsSubMenu* menu;

    switch (m_desiredMode)
    {
    case MODE_AUDIO:
        menu = (OptionsSubMenu*)nlMalloc(sizeof(OptionsAudioMenuV2), 8, false);
        menu = new (menu) OptionsAudioMenuV2(pres, ButtonComponent::BS_B_ONLY, nlSingleton<GameInfoManager>::Instance()->mUserInfo.mAudioOptions);
        m_pauseMenu = menu;
        break;
    case MODE_VISUAL:
        menu = (OptionsSubMenu*)nlMalloc(sizeof(OptionsVisualMenuV2), 8, false);
        menu = new (menu) OptionsVisualMenuV2(pres, ButtonComponent::BS_B_ONLY, nlSingleton<GameInfoManager>::Instance()->mUserInfo.mVisualOptions);
        m_pauseMenu = menu;
        break;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800B01F4 | size: 0xCC
 */
void PauseOptionsScene::Update(float dt)
{
    BaseSceneHandler::Update(dt);

    if (g_pFEInput->JustPressed(m_controllingInput, 0x200, false, NULL))
    {
        m_pauseMenu->Save();

        PauseMenuScene* pauseScene = (PauseMenuScene*)nlSingleton<OverlayManager>::Instance()->Push(IGSCENE_PAUSE, SCREEN_BACK, true);

        PauseMenuScene::mControllingInput = m_controllingInput;

        pauseScene->mStartAnimAtEnd = true;

        FEAudio::PlayAnimAudioEvent("sfx_back", false);
    }
    else
    {
        m_pauseMenu->Update(dt);
    }
}
