#include "NL/nlMemory.h"
#include "NL/nlTask.h"

#include "Game/WorldManager.h"
#include "Game/BasicStadium.h"

World* WorldManager::s_World = NULL;

/**
 * Offset/Address/Size: 0x0 | 0x8019AC60 | size: 0x44
 */
void WorldManager::DestroyWorld()
{
    delete s_World;
    s_World = NULL;
}

/**
 * Offset/Address/Size: 0x44 | 0x8019ACA4 | size: 0x68
 */
bool WorldManager::LoadWorld(const char* szBaseName, bool forfe)
{
    s_World = new (nlMalloc(sizeof(BasicStadium), 8, FALSE)) BasicStadium(szBaseName);
    return s_World->Load(forfe);
}

/**
 * Offset/Address/Size: 0xAC | 0x8019AD0C | size: 0x90
 */
void WorldManager::UpdateWorld(float timeDelta)
{
    u32 taskState;
    World::m_uCurrentFrameCount += 1;
    if (0.f != timeDelta)
    {
        taskState = nlTaskManager::m_pInstance->m_CurrState;
        if ((taskState != 0x10U) && ((u32)(taskState + 0xFFFE0000) != 0U))
        {
            s_World->Update(timeDelta);
        }
        s_World->UpdateInReplay(timeDelta);
    }
}

/**
 * Offset/Address/Size: 0x13C | 0x8019AD9C | size: 0x30
 */
void WorldManager::RenderWorld()
{
    s_World->Render();
}
