#include "Game/Camera/DebugCam.h"

#include "NL/nlMath.h"
#include "NL/gl/glMatrix.h"

#include "NL/globalpad.h"
#include "NL/nlTask.h"

static float sfDebugCamFOV = 60.0f;
static float sfControlSpeedScale = 50.f;

/**
 * Offset/Address/Size: 0x5A4 | 0x801A8C60 | size: 0xB8
 */
cDebugCamera::cDebugCamera()
{
    m_Fov = 27.0f;
    m_fRadius = 10.0f;
    m_fAzimuth = 215.0f;
    m_fTheta = 25.0f;
    m_fHeight = 0.0f;
    m_bEnableControls = true;
    nlVec3Set(m_vecTarget, 0.0f, 0.0f, 0.0f);
    m_matView.SetIdentity();
    Update(0.0f);
}

/**
 * Offset/Address/Size: 0x548 | 0x801A8C04 | size: 0x5C
 */
cDebugCamera::~cDebugCamera()
{
}

/**
 * Offset/Address/Size: 0x14C | 0x801A8808 | size: 0x3FC
 */
void cDebugCamera::UpdateCameraControls(float dt)
{
    float xPressure = cPadManager::GetPad(0)->GetPressure(PAD_BUTTON_X, false);
    float yPressure = cPadManager::GetPad(0)->GetPressure(PAD_BUTTON_Y, false);

    // Toggle controls with Z + Y buttons
    if ((xPressure > 0.0f) && (cPadManager::GetPad(0)->JustPressed(PAD_BUTTON_Y, false))
        || (yPressure > 0.0f) && (cPadManager::GetPad(0)->JustPressed(PAD_BUTTON_X, false)))
    {
        m_bEnableControls = !m_bEnableControls;
    }

    m_fAzimuth += dt * (sfControlSpeedScale * (2.0f * cPadManager::GetPad(0)->AnalogRightX()));
    m_fTheta += dt * (sfControlSpeedScale * (2.0f * cPadManager::GetPad(0)->AnalogRightY()));

    if (m_fTheta > 89.0f)
    {
        m_fTheta = 89.0f;
    }
    if (m_fTheta < -89.0f)
    {
        m_fTheta = -89.0f;
    }

    float temp_f27, temp_f28;
    float t = 0.f;
    float temp_f5;
    temp_f27 = (0.2f * m_matView.m21);
    temp_f28 = (0.2f * m_matView.m11);
    temp_f5 = dt * (sfControlSpeedScale * cPadManager::GetPad(0)->AnalogLeftX());
    float x0 = temp_f5 * temp_f28;
    float y0 = temp_f5 * temp_f27;
    float z0 = m_vecTarget.z + t;
    nlVec3Set(m_vecTarget,
        m_vecTarget.x + x0,
        m_vecTarget.y + y0,
        z0);

    temp_f27 = (0.2f * m_matView.m23);
    temp_f28 = (0.2f * m_matView.m13);
    temp_f5 = dt * (sfControlSpeedScale * -cPadManager::GetPad(0)->AnalogLeftY());
    float x1 = temp_f5 * temp_f28;
    float y1 = temp_f5 * temp_f27;
    float z1 = m_vecTarget.z + t;
    nlVec3Set(m_vecTarget,
        m_vecTarget.x + x1,
        m_vecTarget.y + y1,
        z1);

    if (nlTaskManager::m_pInstance->m_CurrState == 0x100)
    {
        m_fHeight = -(cPadManager::GetPad(0)->GetPressure(4, true) * (dt * (0.1f * sfControlSpeedScale)) - m_fHeight);
        m_fHeight += dt * (0.1f * sfControlSpeedScale) * cPadManager::GetPad(0)->GetPressure(8, true);
    }
    else
    {
        m_fHeight = -(cPadManager::GetPad(0)->GetPressure(1, true) * (dt * (sfControlSpeedScale * 0.1f)) - m_fHeight);
        m_fHeight += dt * (sfControlSpeedScale * 0.1f) * cPadManager::GetPad(0)->GetPressure(0, true);
    }

    if (m_fHeight < 0.0f)
    {
        m_fHeight = 0.0f;
    }
    m_fRadius += (dt * (xPressure * sfControlSpeedScale)) / 5.0f;
    m_fRadius -= (dt * (yPressure * sfControlSpeedScale)) / 5.0f;
    if (m_fRadius < 0.001)
    {
        m_fRadius = 0.001f;
    }

    // Set FOV from global variable
    m_Fov = sfDebugCamFOV;
}

/**
 * Offset/Address/Size: 0x0 | 0x801A86BC | size: 0x14C
 */
void cDebugCamera::Update(float dt)
{
    nlVector3 vecUp; // r1+0x10
    float sn;        // r1+0xC
    float cs;        // r1+0x8
    float z;         // f31
    float d;         // f30
    float y;         // f1

    UpdateCameraControls(dt);
    nlVec3Set(vecUp, 0.0f, 0.0f, 1.0f); // r1+0x10

    nlSinCos(&sn, &cs, (s16)(10430.378f * ((3.1415927f * m_fTheta) / 180.0f)));
    z = m_fRadius * sn;
    d = m_fRadius * cs;
    nlSinCos(&sn, &cs, (s16)(10430.378f * ((3.1415927f * m_fAzimuth) / 180.0f)));

    nlVec3Set(m_vecCamera, d * cs, d * sn, z);

    m_vecTarget.z = m_fHeight;

    nlVec3Add(m_vecCamera, m_vecCamera, m_vecTarget);

    glMatrixLookAt(m_matView, m_vecCamera, m_vecTarget, vecUp);
}
