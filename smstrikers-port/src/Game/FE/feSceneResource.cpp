#include "Game/FE/feSceneResource.h"

/**
 * Offset/Address/Size: 0x0 | 0x8020D608 | size: 0x20
 */
FESceneResource::FESceneResource()
{
    m_next = 0;
    m_prev = 0;
    m_bValid = false;
    m_pFESceneContext = 0;
    m_type = FERT_SCENE;
}
