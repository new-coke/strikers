#include "Game/FE/feFontResource.h"

/**
 * Offset/Address/Size: 0x0 | 0x8020D644 | size: 0x8
 */
void FEFontResource::SetFontReference(nlFont* pFontReference)
{
    m_pFontReference = pFontReference;
}
