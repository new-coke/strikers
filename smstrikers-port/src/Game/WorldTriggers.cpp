#include "Game/WorldTriggers.h"
#include "NL/nlMath.h"

#include "Game/Effects/EmissionManager.h"
#include "Game/Effects/EffectsGroup.h"

/**
 * Offset/Address/Size: 0x0 | 0x801A3FF4 | size: 0x84
 */
void EmitCameraFlash(const nlVector3& position)
{
    EmissionController* cntrl = EmissionManager::Create(fxGetGroup("camera_flashes"), 0);
    nlVector3 velocity = { 0.0f, 0.0f, 0.0f };
    cntrl->SetVelocity(velocity);
    cntrl->m_fGround = 0.02f;
    cntrl->SetPosition(position);
}
