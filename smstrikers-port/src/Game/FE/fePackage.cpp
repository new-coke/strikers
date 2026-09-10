#include "Game/FE/fePackage.h"

/**
 * Offset/Address/Size: 0x24 | 0x80209D6C | size: 0x8
 */
FEPresentation* FEPackage::GetPresentation() const
{
    return m_pFEPresentation;
}

/**
 * Offset/Address/Size: 0x0 | 0x80209D48 | size: 0x24
 */
void FEPackage::Update(float deltaTime)
{
    m_pFEPresentation->Update(deltaTime);
}
