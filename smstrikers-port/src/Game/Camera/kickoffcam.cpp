#include "Game/Camera/kickoffcam.h"

#include "NL/gl/glMatrix.h"

nlVector3 vecCameraDefault = { 0.0f, -18.0f, 8.0f };
nlVector3 vecTargetDefault = { 0.0f, 0.0f, 0.0f };

/**
 * Offset/Address/Size: 0x90 | 0x801A6184 | size: 0x78
 */
cKickOffCamera::cKickOffCamera()
{
    m_v3Camera = vecCameraDefault;
    m_v3Target = vecTargetDefault;
}

/**
 * Offset/Address/Size: 0x34 | 0x801A6128 | size: 0x5C
 */
cKickOffCamera::~cKickOffCamera()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x801A60F4 | size: 0x34
 */
void cKickOffCamera::Update(float dt)
{
    glMatrixLookAt(m_matView, m_v3Camera, m_v3Target, mUpVector);
}
