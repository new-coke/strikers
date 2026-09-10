
#include "Game/Camera/FollowCam.h"
#include "Game/Ball.h"
#include "Game/Team.h"
#include "Game/ReplayManager.h"

#include "NL/gl/glMatrix.h"
#include "Game/main.h"

extern cTeam* g_pTeams[2];

static f32 g_fDistanceSeek = 0.5f;
static f32 g_fMaxDistance = 40.0f;
static f32 g_fMinDistance = 2.0f;
static f32 g_fFollowCamOOISeek = 0.1f;
static f32 g_fFollowCamOOIZSeek = 0.025f;
static f32 g_fFollowCamMaxRotPerFrame = 400.0f;
static u16 g_aFollowCamMaxPitch = 0x3555;
static u16 g_aFollowCamMinPitch = 0x4B9;
static f32 g_fFollowCamMaxZOffset = 3.0f;
static f32 g_fFollowCamMinZOffset = 0.6f;

/**
 * Offset/Address/Size: 0x668 | 0x801A9580 | size: 0x90
 */
cFollowCamera::cFollowCamera(FollowTarget followTarget)
{
    m_FollowTarget = followTarget;
    m_bOOISet = false;
    m_aFacingDirection = 0;
    m_aPitch = 0x1000;
    m_fOOIDistance = 5.0f;
    m_bPitchLimits = true;
    m_bControlsLocked = false;
    m_matView.SetIdentity();
}

/**
 * Offset/Address/Size: 0x0 | 0x801A8F18 | size: 0x668
 */
void cFollowCamera::Update(float fDeltaT)
{
    int i;
    RenderSnapshot* snap;
    cGlobalPad* pController;
    cCharacter* pCharacter;
    float xPressure;
    float yPressure;
    float fScalar;
    nlMatrix4 m4Orient;

    pController = cPadManager::GetPad(0);
    if (!pController->IsConnected())
    {
        m_matView.SetIdentity();
        return;
    }

    if (m_FollowTarget == FOLLOW_CHARACTER)
    {
        pCharacter = NULL;
        for (i = 0; i < 2; i++)
        {
            pCharacter = g_pTeams[i]->GetControlledPlayer(pController);
            if (pCharacter != NULL)
            {
                break;
            }
        }
        if (!pCharacter)
        {
            m_matView.SetIdentity();
            return;
        }

        m_v3OOI = pCharacter->m_v3Position;
    }
    else if (m_FollowTarget == FOLLOW_BALL)
    {
        m_v3OOI = *g_pBall->GetDrawablePosition();
    }
    else if (m_FollowTarget == FOLLOW_ANIM_VIEWER_CHARACTER)
    {
        // EMPTY
    }
    else if (m_FollowTarget == FOLLOW_SELECTABLE)
    {
        snap = ReplayManager::Instance()->mRender;
        static s32 currentlySelectedTarget = 0;

        const int numAvailableObjs = snap->NumDrawableObjects();

        if (!g_bTweaking && fDeltaT > 0.0f)
        {
            if (cPadManager::GetPad(0)->JustPressed(1, false) && !g_bTweaking)
            {
                currentlySelectedTarget = (currentlySelectedTarget - 1 + numAvailableObjs) % numAvailableObjs;
            }
            if (cPadManager::GetPad(0)->JustPressed(2, false) && !g_bTweaking)
            {
                currentlySelectedTarget = (currentlySelectedTarget + 1) % numAvailableObjs;
            }
        }

        m_v3OOI = snap->GetPositionForDrawableObject(currentlySelectedTarget);
    }
    else
    {
        m_matView.SetIdentity();
        return;
    }

    m_v3OOI.z += g_fFollowCamMinZOffset + (g_fFollowCamMaxZOffset - g_fFollowCamMinZOffset) * ((m_fOOIDistance - g_fMinDistance) / (g_fMaxDistance - g_fMinDistance));

    m_v3OOIDampenedPrev = m_v3OOIDampened;

    m_v3OOIDampened.x = (1.0f - g_fFollowCamOOISeek) * m_v3OOIDampened.x + g_fFollowCamOOISeek * m_v3OOI.x;
    m_v3OOIDampened.y = (1.0f - g_fFollowCamOOISeek) * m_v3OOIDampened.y + g_fFollowCamOOISeek * m_v3OOI.y;
    m_v3OOIDampened.z = (1.0f - g_fFollowCamOOIZSeek) * m_v3OOIDampened.z + g_fFollowCamOOIZSeek * m_v3OOI.z;

    xPressure = cPadManager::GetPad(0)->GetPressure(0x400, false);
    yPressure = cPadManager::GetPad(0)->GetPressure(0x800, false);
    if ((xPressure > 0.0f && cPadManager::GetPad(0)->JustPressed(0x800, false))
        || (yPressure > 0.0f && cPadManager::GetPad(0)->JustPressed(0x400, false)))
    {
        m_bControlsLocked = !m_bControlsLocked;
    }

    if (!g_bTweaking && !g_bProfiling && !m_bControlsLocked)
    {
        m_fOOIDistance -= g_fDistanceSeek * pController->AnalogLeftY();
    }

    if (m_fOOIDistance > g_fMaxDistance)
        m_fOOIDistance = g_fMaxDistance;
    else if (m_fOOIDistance < g_fMinDistance)
        m_fOOIDistance = g_fMinDistance;

    m_aFacingDirection = m_aFacingDirection + (int)(g_fFollowCamMaxRotPerFrame * pController->AnalogRightX());
    m_aPitch = m_aPitch + (int)(g_fFollowCamMaxRotPerFrame * pController->AnalogRightY());

    if (m_bPitchLimits)
    {
        if (m_aPitch > g_aFollowCamMaxPitch)
            m_aPitch = g_aFollowCamMaxPitch;
        else if (m_aPitch < g_aFollowCamMinPitch)
            m_aPitch = g_aFollowCamMinPitch;
    }

    const float vx = -m_matView.e2[0][2];
    const float vy = -m_matView.e2[1][2];

    const float dx = (m_v3OOIDampened.x - m_v3OOIDampenedPrev.x);
    const float dy = (m_v3OOIDampened.y - m_v3OOIDampenedPrev.y);

    fScalar = 0.0f;
    const float denom = nlSqrt(fScalar + (vx * vx + vy * vy), true);
    const float t = (fScalar + (vx * dx + vy * dy)) / (denom * denom);

    float rx = dx - t * vx;
    float ry = dy - t * vy;
    const float len = nlSqrt(fScalar + (rx * rx + ry * ry), true);

    const float invDist = len / m_fOOIDistance;
    const float angleShortF = 10430.378f * invDist;
    const u16 angleShort = (u16)(int)angleShortF;

    const float signCheck = (rx * vy - ry * vx);
    if (signCheck >= 0.0f)
        m_aFacingDirection = m_aFacingDirection - angleShort;
    else
        m_aFacingDirection = m_aFacingDirection + angleShort;

    nlVec3Set(m_v3CameraPosition, m_fOOIDistance, 0.0f, 0.0f);

    nlMakeRotationMatrixY(m4Orient, (u16)(-m_aPitch) * 0.0000958738f);
    nlMultPosVectorMatrix(m_v3CameraPosition, m_v3CameraPosition, m4Orient);

    nlMakeRotationMatrixZ(m4Orient, m_aFacingDirection * 0.0000958738f);
    nlMultPosVectorMatrix(m_v3CameraPosition, m_v3CameraPosition, m4Orient);

    m_v3CameraPosition.x += m_v3OOIDampened.x;
    m_v3CameraPosition.y += m_v3OOIDampened.y;
    m_v3CameraPosition.z += m_v3OOIDampened.z;

    glMatrixLookAt(m_matView, m_v3CameraPosition, m_v3OOIDampened, mUpVector);
}
