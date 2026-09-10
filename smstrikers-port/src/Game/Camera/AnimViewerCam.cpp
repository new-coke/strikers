#include "Game/Camera/AnimViewerCam.h"
#include "Game/Camera/FollowCam.h"
#include "types.h"

/**
 * Offset/Address/Size: 0x108 | 0x801ACC8C | size: 0x8
 */
eCameraType cAnimViewerCamera::GetType()
{
    return eCameraType_AnimViewer;
}

/**
 * Offset/Address/Size: 0x94 | 0x801ACC18 | size: 0x74
 */
cAnimViewerCamera::~cAnimViewerCamera()
{
    m_pCurrentPlayer = nullptr;
}

/**
 * Offset/Address/Size: 0x44 | 0x801ACBC8 | size: 0x50
 */
cAnimViewerCamera::cAnimViewerCamera()
    : cFollowCamera(FOLLOW_ANIM_VIEWER_CHARACTER)
{
    m_pCurrentPlayer = nullptr;
    m_bPitchLimits = false;
    m_aPitch = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x801ACB84 | size: 0x44
 */
void cAnimViewerCamera::Update(float fDeltaT)
{
    if (m_pCurrentPlayer)
    {
        m_v3OOI = m_pCurrentPlayer->m_v3Position;
    }
    cFollowCamera::Update(fDeltaT);
}
