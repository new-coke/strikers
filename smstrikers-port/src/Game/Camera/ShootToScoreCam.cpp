#include "Game/Camera/ShootToScoreCam.h"

#include "NL/gl/glMatrix.h"

/**
 * Offset/Address/Size: 0x90 | 0x801AA4F4 | size: 0x88
 */
cShootToScoreCamera::cShootToScoreCamera()
{
    nlVec3Set(m_v3Camera, 0.0f, 0.0f, 0.0f);
    nlVec3Set(m_v3Target, 1.0f, 0.0f, 0.0f);
    Update(0.0f);
}

/**
 * Offset/Address/Size: 0x34 | 0x801AA498 | size: 0x5C
 */
cShootToScoreCamera::~cShootToScoreCamera()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x801AA464 | size: 0x34
 */
void cShootToScoreCamera::Update(float fDeltaT)
{
    glMatrixLookAt(m_matView, m_v3Camera, m_v3Target, mUpVector);
}
