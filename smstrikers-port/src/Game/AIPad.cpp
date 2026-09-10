#include "Game/AI/AIPad.h"
#include "Game/Camera/CameraMan.h"

static f32 g_fMovementDeadZone = 0.3f;
static f32 g_fCStickDeadZone = 0.5f;

cAIPad AIPadManager::mAIPads[4];

/**
 * Offset/Address/Size: 0xFC | 0x8000582C | size: 0x20
 */
f32 cAIPad::GetMovementStickMagnitude()
{
    f32 mag = m_pGlobalPad->m_polarAnalogLeft.r;
    f32 dz = g_fMovementDeadZone;
    return (mag - dz) / (1.0f - dz);
}

/**
 * Offset/Address/Size: 0xDC | 0x8000580C | size: 0x20
 */
u16 cAIPad::GetMovementStickDirection()
{
    return (u16)(cCameraManager::m_aJoystickRemap - 0x4000) + m_pGlobalPad->m_polarAnalogLeft.a;
}

/**
 * Offset/Address/Size: 0xBC | 0x800057EC | size: 0x20
 */
f32 cAIPad::GetCStickMovementStickMagnitude()
{
    f32 mag = m_pGlobalPad->m_polarAnalogRight.r;
    f32 dz = g_fCStickDeadZone;
    return (mag - dz) / (1.0f - dz);
}

/**
 * Offset/Address/Size: 0x9C | 0x800057CC | size: 0x20
 */
u16 cAIPad::GetCStickMovementStickDirection()
{
    return (u16)(cCameraManager::m_aJoystickRemap - 0x4000) + m_pGlobalPad->m_polarAnalogRight.a;
}

u16 cAIPad::GetMovementStickDirectionNoRemap()
{
    return m_pGlobalPad->m_polarAnalogLeft.a;
}

/**
 * Offset/Address/Size: 0x54 | 0x80005784 | size: 0x48
 */
bool cAIPad::IsTurboPressed()
{
    return (m_pGlobalPad->GetPressure(0x14, true) > 0.2f);
}

/**
 * Offset/Address/Size: 0x0 | 0x80005730 | size: 0x54
 */
void AIPadManager::Startup()
{
    int i;
    for (i = 0; i < 4; ++i)
    {
        mAIPads[i].m_pGlobalPad = cPadManager::GetPad(i);
    }
}
