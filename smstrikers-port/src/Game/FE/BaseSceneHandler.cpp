#include "Game/BaseSceneHandler.h"

/**
 * Offset/Address/Size: 0x68 | 0x80212920 | size: 0x24
 */
void BaseSceneHandler::Update(float dt)
{
    m_pFEScene->Update(dt);
}

/**
 * Offset/Address/Size: 0x3C | 0x802128F4 | size: 0x2C
 */
void BaseSceneHandler::AddScreenHandler(BaseScreenHandler* handler)
{
    handler->m_pFEScene = m_pFEScene;
    nlDLRingAddEnd<BaseScreenHandler>(&m_pScreenHandlerList, handler);
}

/**
 * Offset/Address/Size: 0x38 | 0x802128F0 | size: 0x4
 */
void BaseSceneHandler::RemoveScreenHandler(BaseScreenHandler* pScreenHandler)
{
}

/**
 * Offset/Address/Size: 0x0 | 0x802128B8 | size: 0x38
 */
void BaseSceneHandler::OnActivate()
{
    if (m_pActiveScreenHandler != nullptr)
    {
        m_pActiveScreenHandler->fnc3();
    }
}
