#include "Game/ParticleUpdateTask.h"
#include "Game/GameRenderTask.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/main.h"

float ParticleUpdateTask::sfTimeScale = 1.0f;

/**
 * Offset/Address/Size: 0x64 | 0x80170B4C | size: 0x8
 */
void ParticleUpdateTask::SetTimeScale(float timeScale)
{
    sfTimeScale = timeScale;
}

/**
 * Offset/Address/Size: 0x0 | 0x80170AE8 | size: 0x64
 */
void ParticleUpdateTask::Run(float dt)
{
    EmissionManager* var_r3;

    if (g_bRenderWorld)
    {
        if (g_e3_Build)
        {
            EmissionManager::Update(dt * sfTimeScale);
        }
        else
        {
            if (nlTaskManager::m_pInstance->m_CurrState != 1)
            {
                EmissionManager::Update(dt * sfTimeScale);
            }
            // var_r3 = m_pInstance__13nlTaskManager.unk0;
            // if (var_r3->unk8 != 1)
            // {
            //     var_r3 = Update__15EmissionManagerFf(var_r3, arg0 * sfTimeScale__18ParticleUpdateTask.unk0);
            // }
        }
        EmissionManager::Render();
    }
}
