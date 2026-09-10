#include "Game/SH/SHSuperTeam.h"

#include "Game/FE/feFinder.h"
#include "Game/FE/feInput.h"
#include "Game/GameSceneManager.h"
#include "Game/SH/SHCupHub.h"
#include "Game/Audio/WorldAudio.h"

/**
 * Offset/Address/Size: 0x15C | 0x800F6050 | size: 0x78
 */
SuperTeamScene::~SuperTeamScene()
{
}

/**
 * Offset/Address/Size: 0xC0 | 0x800F5FB4 | size: 0x9C
 */
void SuperTeamScene::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();

    TLSlide* slide;
    FEPresentation* pres;

    pres = m_pFEPresentation;
    slide = pres->m_currentSlide;

    if (pres->m_fadeDuration < slide->m_start + slide->m_duration)
    {
        return;
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        CupHubScene* scene = (CupHubScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CUP_STANDINGS, SCREEN_NOTHING, true);
        scene->mDoAutoSave = true;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800F5EF4 | size: 0xC0
 */
void SuperTeamScene::SceneCreated()
{

    mButtons.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtons.SetState(ButtonComponent::BS_A_ONLY);

    Audio::gWorldSFX.Play((Audio::eWorldSFX)27, 100.0f, -1.0f, true, 100.0f);
}
