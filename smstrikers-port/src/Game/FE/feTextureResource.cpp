#include "Game/FE/feTextureResource.h"

/**
 * Offset/Address/Size: 0x0 | 0x8020D628 | size: 0x1C
 */
FETextureResource::FETextureResource()
{
    m_next = 0;
    m_prev = 0;
    m_bValid = 0;
    m_glTextureHandle = 0;
    m_type = FERT_TEXTURE;
}
