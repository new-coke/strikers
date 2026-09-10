#include "Game/Net.h"
#include "Game/Field.h"

float cNet::m_fNetHeight = 1.0f;
float cNet::m_fNetWidth = 1.0f;
float cNet::m_fNetDepth = 2.5f;
float cNet::m_fNetPostRadius = 0.01f;
float cNet::m_fNetPostOffsetFromGoalLine = 0.0f;

/**
 * Offset/Address/Size: 0xE4 | 0x80057518 | size: 0x38
 */
cNet::cNet(int nIndex)
{
    m_nIndex = nIndex;

    m_v3NetLocation.y = 0.0f;
    m_v3NetLocation.z = 0.0f;
    m_v3NetLocation.x = 0.0f;

    if (m_nIndex == 0)
    {
        m_fDirection = -1.0f;
    }
    else
    {
        m_fDirection = +1.0f;
    }
}

/**
 * Offset/Address/Size: 0xA8 | 0x800574DC | size: 0x3C
 */
cNet::~cNet()
{
}

/**
 * Offset/Address/Size: 0x84 | 0x800574B8 | size: 0x24
 */
float cNet::GetGoalLineX() const
{
    return cField::GetGoalLineX(m_fDirection);
}

/**
 * Offset/Address/Size: 0x14 | 0x80057448 | size: 0x70
 */
void cNet::GetPostLocation(nlVector3& v3PostPosition, unsigned int uPostNum, float fYAdjust) const
{
    f32 fAdjust;

    v3PostPosition = m_v3NetLocation;
    v3PostPosition.x = -((m_fNetPostRadius * m_fDirection) - v3PostPosition.x);
    v3PostPosition.x += m_fNetPostOffsetFromGoalLine * m_fDirection;
    fAdjust = (0.5f * m_fNetWidth) + fYAdjust;

    if (uPostNum == 0)
    {
        v3PostPosition.y -= fAdjust;
        return;
    }

    v3PostPosition.y += fAdjust;
}

/**
 * Offset/Address/Size: 0x0 | 0x80057434 | size: 0x14
 */
void cNet::SetNetDimensions(float fWidth, float fHeight, float fPostRadius, float fPostOffsetFromGoalLine)
{
    m_fNetWidth = fWidth;
    m_fNetHeight = fHeight;
    m_fNetPostRadius = fPostRadius;
    m_fNetPostOffsetFromGoalLine = fPostOffsetFromGoalLine;
}

void cNet::ShowNets()
{
}
