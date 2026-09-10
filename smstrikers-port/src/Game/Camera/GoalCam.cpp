#include "Game/Camera/GoalCam.h"

#include "NL/gl/glMatrix.h"
#include "NL/globalpad.h"

#include "Game/Ball.h"
#include "Game/Field.h"
#include "Game/MathHelpers.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Player.h"

static s32 gnCamType;
static bool gbCamFreeze;

static f32 gfDistance = 10.0f;
static f32 gfHeight = 3.0f;
static f32 gfSideMult = 2.0f;
static f32 gfSideBias = 0.4f;
static f32 gfCamDir = 3.1415927f;
static f32 gfCamTilt = 0.1f;
static f32 gfCamDist = 14.0f;

/**
 * Offset/Address/Size: 0x6CC | 0x801AAC68 | size: 0x7C
 */
GoalCamera::GoalCamera()
{
    m_vecTarget.x = 0.0f;
    m_vecTarget.y = 0.0f;
    m_vecTarget.z = 0.0f;
    Update(0.0f);
}

/**
 * Offset/Address/Size: 0x670 | 0x801AAC0C | size: 0x5C
 */
GoalCamera::~GoalCamera()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x801AA59C | size: 0x670
 *
 */
void GoalCamera::Update(float dt)
{
    nlVector3 vecUp;
    nlVector3 ballpos;
    nlVector3 lastpos;
    nlVector3 lasttarg;
    nlVector3 dirvec;
    float fDirSin;
    float fDirCos;
    float fTiltSin;
    float fTiltCos;
    nlVector3 midvec;

    vecUp.x = 0.0f;
    vecUp.y = 0.0f;
    vecUp.z = 1.0f;

    if (gnCamType == 0)
    {
        if (g_pBall->m_tShotTimer.m_uPackedTime != 0 || g_pBall->GetOwnerGoalie() != NULL)
        {
            return;
        }
    }

    if (gbCamFreeze != 0)
    {
        return;
    }

    static f32 fDir = 1.0f;

    if (g_pBall->m_pOwner == NULL || g_pBall->m_pOwner->m_eClassType == GOALIE)
    {
        ballpos = g_pBall->m_v3Position;
    }
    else
    {
        ballpos = g_pBall->m_pOwner->m_v3Position;
    }

    lastpos = m_vecCamera;
    lasttarg = m_vecTarget;

    if (gnCamType == 0)
    {
        m_vecTarget.x = fDir * cField::GetGoalLineX((unsigned int)1);
        m_vecTarget.y = 0.0f;
        m_vecTarget.z = 0.0f;

        nlVec3Sub(m_vecCamera, ballpos, m_vecTarget);

        f32 invLen = nlRecipSqrt((m_vecCamera.x * m_vecCamera.x) + (m_vecCamera.y * m_vecCamera.y) + (m_vecCamera.z * m_vecCamera.z), 1);
        nlVec3Scale(m_vecCamera, invLen);

        nlVec3ScaleAdd(m_vecCamera, gfDistance, m_vecCamera, ballpos);
        m_vecCamera.z = gfHeight;

        m_vecCamera.x = 0.9f * lastpos.x + 0.1f * m_vecCamera.x;
        m_vecCamera.y = 0.9f * lastpos.y + 0.1f * m_vecCamera.y;
        m_vecCamera.z = 0.9f * lastpos.z + 0.1f * m_vecCamera.z;
    }
    else if (gnCamType == 1)
    {
        m_vecTarget = ballpos;

        g_pBall->m_pPhysicsBall->GetLinearVelocity(&dirvec);

        dirvec.z = 0.0f;
        dirvec.x = 1.0f;
        dirvec.y = 0.0f;
        dirvec.z = 0.3f;

        {
            f32 invLen = nlRecipSqrt((dirvec.x * dirvec.x) + (dirvec.y * dirvec.y) + (dirvec.z * dirvec.z), 1);
            nlVec3Scale(dirvec, invLen);
            dirvec.x *= -1.0f;
            dirvec.y *= -1.0f;
        }

        nlVec3ScaleAdd(m_vecCamera, gfDistance, dirvec, m_vecTarget);
        m_vecCamera.z = m_vecTarget.z + gfHeight;

        m_vecCamera.x = 0.8f * lastpos.x + 0.2f * m_vecCamera.x;
        m_vecCamera.y = 0.8f * lastpos.y + 0.2f * m_vecCamera.y;
        m_vecCamera.z = 0.8f * lastpos.z + 0.2f * m_vecCamera.z;

        m_vecTarget.x = 0.8f * lasttarg.x + 0.2f * m_vecTarget.x;
        m_vecTarget.y = 0.8f * lasttarg.y + 0.2f * m_vecTarget.y;
        m_vecTarget.z = 0.8f * lasttarg.z + 0.2f * m_vecTarget.z;
    }
    else
    {
        m_vecTarget.x = fDir * cField::GetGoalLineX((unsigned int)1);
        m_vecTarget.y = 0.0f;
        m_vecTarget.z = gfHeight;

        nlSinCos(&fDirSin, &fDirCos, (u16)(s32)(10430.378f * gfCamDir));
        nlSinCos(&fTiltSin, &fTiltCos, (u16)(s32)(10430.378f * gfCamTilt));

        m_vecCamera.x = fTiltCos * (gfCamDist * fDirCos);
        m_vecCamera.y = fTiltCos * (gfCamDist * fDirSin);
        m_vecCamera.z = gfCamDist * fTiltSin;

        nlVec3Add(m_vecCamera, m_vecCamera, m_vecTarget);

        m_vecCamera.x = 0.9f * lastpos.x + 0.1f * m_vecCamera.x;
        m_vecCamera.y = 0.9f * lastpos.y + 0.1f * m_vecCamera.y;
        m_vecCamera.z = 0.9f * lastpos.z + 0.1f * m_vecCamera.z;

        ballpos = m_vecCamera;
        ballpos.z = gfHeight;
    }

    if (cPadManager::GetPad(0)->IsPressed(0x800, 0))
    {
        if (gnCamType != 1)
        {
            nlVec3Sub(dirvec, m_vecTarget, ballpos);

            nlVec3WeightedSum(midvec, 1.0f - gfSideBias, m_vecTarget, gfSideBias, ballpos);

            dirvec.z = dirvec.x;
            dirvec.x = dirvec.y;
            dirvec.y = -dirvec.z;

            nlVec3ScaleAdd(dirvec, gfSideMult, dirvec, midvec);
            dirvec.z = gfHeight;
        }
        else if (gnCamType == 1)
        {
            float y = dirvec.y;
            float x = dirvec.x;
            float nx = -y;

            dirvec.z = x;
            float x2 = dirvec.z;
            dirvec.x = nx;
            dirvec.y = x2;

            nlVec3ScaleAdd(dirvec, gfDistance, dirvec, m_vecTarget);
            dirvec.z = m_vecTarget.z + gfHeight;

            midvec = m_vecTarget;
        }
        glMatrixLookAt(m_matView, dirvec, midvec, vecUp);
    }
    else
    {
        glMatrixLookAt(m_matView, m_vecCamera, m_vecTarget, vecUp);
    }
}
