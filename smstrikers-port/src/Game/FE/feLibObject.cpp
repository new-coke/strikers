#include "Game/FE/feLibObject.h"

/**
 * Offset/Address/Size: 0x0 | 0x80209D28 | size: 0x8
 */
feVector3& FELibObject::GetScale() const
{
    return const_cast<feVector3&>(m_attributes.v3Scale);
}

/**
 * Offset/Address/Size: 0x8 | 0x80209D30 | size: 0x8
 */
feVector3& FELibObject::GetRotation() const
{
    return const_cast<feVector3&>(m_attributes.v3Rotation);
}

/**
 * Offset/Address/Size: 0x10 | 0x80209D38 | size: 0x8
 */
feVector3& FELibObject::GetPosition() const
{
    return const_cast<feVector3&>(m_attributes.v3Position);
}

/**
 * Offset/Address/Size: 0x18 | 0x80209D40 | size: 0x8
 */
nlColour& FELibObject::GetColour() const
{
    return const_cast<nlColour&>(m_attributes.colour);
}
