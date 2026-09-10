#include "Game/WorldUpdateTask.h"
#include "Game/WorldManager.h"
#include "Game/FE/feManager.h"

/**
 * Offset/Address/Size: 0x0 | 0x8016E644 | size: 0x48
 */
void WorldUpdateTask::Run(float dt)
{
    if (nlTaskManager::m_pInstance->m_CurrState == 1 && FrontEnd::m_feStateCurrent != 0)
    {
        WorldManager::UpdateWorld(0.f);
        return;
    }
    WorldManager::UpdateWorld(dt);
}
