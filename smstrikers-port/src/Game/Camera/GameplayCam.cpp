#include "Game/Camera/GameplayCam.h"
#include "port/aspect.h"

#include "Game/Ball.h"
#include "Game/GameInfo.h"
#include "Game/ReplayManager.h"
#include "Game/Team.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/AiUtil.h"
#include "NL/nlTask.h"
#include "NL/gl/glMatrix.h"
#include "math.h"

static f32 CANT_COLLIDE = *(f32*)__float_max;

static const float nearZoomPositiveTargetKnotsY[3] = { -5.2f, 0.0f, 4.0f };
static const float nearZoomPositiveFieldKnotsY[3] = { -5.6f, 0.0f, 11.65f };
static const float farZoomPositiveTargetKnotsY[3] = { -2.6f, -2.6f, 2.0f };
static const float farZoomPositiveFieldKnotsY[3] = { 0.0f, 0.0f, 11.65f };

static const float nearZoomPositiveTargetKnotsX[2] = { -16.8f, 16.8f };
static const float nearZoomPositiveFieldKnotsX[2] = { -17.0f, 14.5f };
static const float nearZoomNegativeFieldKnotsX[2] = { -14.5f, 17.0f };
static const float nearZoomNeutralFieldKnotsX[2] = { -15.75f, 15.75f };
static const float farZoomPositiveTargetKnotsX[2] = { -13.3f, 13.3f };
static const float farZoomPositiveFieldKnotsX[2] = { -13.0f, 8.0f };
static const float farZoomNegativeFieldKnotsX[2] = { -8.0f, 13.0f };
static const float farZoomNeutralFieldKnotsX[2] = { -10.5f, 10.5f };
static const float nearZoomWidePositiveTargetKnotsX[2] = { -15.5f, 15.5f };
static const float farZoomWidePositiveTargetKnotsX[2] = { -10.1f, 10.1f };

static const CameraData gCameraData[4] = {
    {
        2,
        3,
        { nearZoomPositiveTargetKnotsX, nearZoomPositiveTargetKnotsX, nearZoomPositiveTargetKnotsX },
        { nearZoomNeutralFieldKnotsX, nearZoomPositiveFieldKnotsX, nearZoomNegativeFieldKnotsX },
        { nearZoomPositiveTargetKnotsY, nearZoomPositiveTargetKnotsY, nearZoomPositiveTargetKnotsY },
        { nearZoomPositiveFieldKnotsY, nearZoomPositiveFieldKnotsY, nearZoomPositiveFieldKnotsY },
        33.4f,
        20.0f,
        25.0f,
        270.0f,
    },
    {
        2,
        3,
        { farZoomPositiveTargetKnotsX, farZoomPositiveTargetKnotsX, farZoomPositiveTargetKnotsX },
        { farZoomNeutralFieldKnotsX, farZoomPositiveFieldKnotsX, farZoomNegativeFieldKnotsX },
        { farZoomPositiveTargetKnotsY, farZoomPositiveTargetKnotsY, farZoomPositiveTargetKnotsY },
        { farZoomPositiveFieldKnotsY, farZoomPositiveFieldKnotsY, farZoomPositiveFieldKnotsY },
        33.4f,
        35.0f,
        38.0f,
        270.0f,
    },
    {
        2,
        3,
        { nearZoomWidePositiveTargetKnotsX, nearZoomWidePositiveTargetKnotsX, nearZoomWidePositiveTargetKnotsX },
        { nearZoomNeutralFieldKnotsX, nearZoomPositiveFieldKnotsX, nearZoomNegativeFieldKnotsX },
        { nearZoomPositiveTargetKnotsY, nearZoomPositiveTargetKnotsY, nearZoomPositiveTargetKnotsY },
        { nearZoomPositiveFieldKnotsY, nearZoomPositiveFieldKnotsY, nearZoomPositiveFieldKnotsY },
        44.3f,
        20.0f,
        25.0f,
        270.0f,
    },
    {
        2,
        3,
        { farZoomWidePositiveTargetKnotsX, farZoomWidePositiveTargetKnotsX, farZoomWidePositiveTargetKnotsX },
        { farZoomNeutralFieldKnotsX, farZoomPositiveFieldKnotsX, farZoomNegativeFieldKnotsX },
        { farZoomPositiveTargetKnotsY, farZoomPositiveTargetKnotsY, farZoomPositiveTargetKnotsY },
        { farZoomPositiveFieldKnotsY, farZoomPositiveFieldKnotsY, farZoomPositiveFieldKnotsY },
        44.3f,
        35.0f,
        38.0f,
        270.0f,
    },
};

bool gGameplayCameraInReplay = false;

/**
 * Offset/Address/Size: 0xCC8 | 0x801AA308 | size: 0x110
 */
GameplayCamera::GameplayCamera()
{
    m_bDynamicZoom = true;
    m_fZoom = 0.0f;
    m_fDesiredZoom = 0.0f;
    m_fZoomSeekSpeed = 0.0f;
    m_ForceNeutralAndNearZoom = false;
    m_matView.SetIdentity();
}

/**
 * Offset/Address/Size: 0xB24 | 0x801AA164 | size: 0x1A4
 */
void GameplayCamera::CalcDynamicZoom()
{
    m_fDesiredZoom = 0.0f;

    for (int i = 0; i < 2; i++)
    {
        int numFieldersInInnerZone = 0;
        int numFieldersInMidZone = 0;

        for (int j = 0; j < 4; j++)
        {
            cFielder* fielder = g_pTeams[i]->GetFielder(j);
            nlVector3 screenPosition = fielder->m_v3ScreenPosition;

            screenPosition.x = fabsf(screenPosition.x);
            screenPosition.y = fabsf(screenPosition.y);

            if (screenPosition.x <= 0.75f && screenPosition.y <= 0.75f)
            {
                numFieldersInInnerZone++;
            }

            if (screenPosition.x <= 0.85f && screenPosition.y <= 0.85f)
            {
                numFieldersInMidZone++;
            }
        }

        int numAssignedControllers = g_pTeams[i]->GetNumAssignedControllers();
        int teamDesiredNumFieldersOnScreen = 0;
        if (numAssignedControllers != 0)
        {
            teamDesiredNumFieldersOnScreen = numAssignedControllers >= 1 ? numAssignedControllers : 1;
        }

        Goalie* goalie = g_pTeams[i]->GetGoalie();
        if (goalie->m_pBall != NULL)
        {
            teamDesiredNumFieldersOnScreen = teamDesiredNumFieldersOnScreen >= 4 ? teamDesiredNumFieldersOnScreen : 4;
        }

        float currentZoom;
        float teamDesiredZoom = 0.0f;
        if (teamDesiredNumFieldersOnScreen > 0)
        {
            if (numFieldersInInnerZone >= teamDesiredNumFieldersOnScreen)
            {
                teamDesiredZoom = 0.0f;
            }
            else if (numFieldersInMidZone < teamDesiredNumFieldersOnScreen)
            {
                teamDesiredZoom = 1.0f;
            }
            else
            {
                teamDesiredZoom = m_fZoom;
            }
        }

        currentZoom = m_fDesiredZoom;
        currentZoom = (currentZoom >= teamDesiredZoom) ? currentZoom : teamDesiredZoom;
        m_fDesiredZoom = currentZoom;
    }
}

// PORT: the two shipped camera tables blended by display aspect; [0]/[1] are 4:3 near and far, [2]/[3] their widescreen twins.
static const CameraData* PortGameplayCameraData(int farZoom)
{
    static CameraData sBlended[2];
    static float sKnots[2][2];
    static int sReady = 0;
    static unsigned int sGeneration = 0;

    // Rebuilt when the display aspect moves, which in STRIKERS_ASPECT=auto is whenever the window changes shape.
    if (sReady && PortAspectGeneration() != sGeneration)
        sReady = 0;

    if (!sReady)
    {
        const float t = PortCameraAspectBlend();
        int which;

        sGeneration = PortAspectGeneration();

        for (which = 0; which < 2; ++which)
        {
            const CameraData* narrow = &gCameraData[which];
            const CameraData* wide = &gCameraData[which + 2];
            const float* a = narrow->targetKnotsX[0];
            const float* b = wide->targetKnotsX[0];
            int k;

            sBlended[which] = *narrow;
            for (k = 0; k < 2; ++k)
                sKnots[which][k] = a[k] + t * (b[k] - a[k]);
            for (k = 0; k < 3; ++k)
                sBlended[which].targetKnotsX[k] = sKnots[which];
        }
        sReady = 1;
    }

    return &sBlended[farZoom ? 1 : 0];
}

/**
 * Offset/Address/Size: 0x8CC | 0x801A9F0C | size: 0x258
 */
void GameplayCamera::Update(float deltaTime)
{
    m_bDynamicZoom = nlSingleton<GameInfoManager>::Instance()->mUserInfo.mVisualOptions.mIsAutoZoomCamera;
    m_fDesiredZoom = 1.0f - nlSingleton<GameInfoManager>::Instance()->mUserInfo.mVisualOptions.mCameraZoomLevel;

    // PORT: blended by display aspect rather than chosen by the game's widescreen option.
    m_nearZoom.m_CameraData = PortGameplayCameraData(0);
    m_farZoom.m_CameraData = PortGameplayCameraData(1);

    m_nearZoom.Update(deltaTime, m_ForceNeutralAndNearZoom);
    m_farZoom.Update(deltaTime, m_ForceNeutralAndNearZoom);

    if (m_ForceNeutralAndNearZoom)
    {
        m_fZoom = 0.0f;
    }
    else
    {
        bool gamePaused = (nlTaskManager::m_pInstance->m_CurrState == 1);

        if (m_bDynamicZoom && !gamePaused && !gGameplayCameraInReplay)
        {
            CalcDynamicZoom();
        }

        float clampedDesiredZoom = Interpolate(0.2f, 0.7f, m_fDesiredZoom);
        float smoothTime;

        if (gamePaused)
        {
            smoothTime = 0.1f;
        }
        else
        {
            smoothTime = 0.75f;
        }

        float change;
        float x;
        float omega = 2.0f / smoothTime;
        x = omega * deltaTime;
        float exp = 1.0f / (((0.48f * x * x) + (1.0f + x)) + (x * (0.235f * x * x)));
        change = m_fZoom - clampedDesiredZoom;
        float currentVelocity = m_fZoomSeekSpeed;

        m_fZoomSeekSpeed = exp * (currentVelocity - (omega * (deltaTime * ((omega * change) + currentVelocity))));
        m_fZoom = (exp * (change + (deltaTime * ((omega * change) + currentVelocity)))) + clampedDesiredZoom;
    }

    float inverseZoom;
    float zoom = m_fZoom;
    inverseZoom = 1.0f - zoom;

    m_v3Target.x = (inverseZoom * m_nearZoom.m_v3Target.x) + (zoom * m_farZoom.m_v3Target.x);
    m_v3Target.y = (inverseZoom * m_nearZoom.m_v3Target.y) + (zoom * m_farZoom.m_v3Target.y);
    m_v3Target.z = (inverseZoom * m_nearZoom.m_v3Target.z) + (zoom * m_farZoom.m_v3Target.z);

    zoom = m_fZoom;
    inverseZoom = 1.0f - zoom;

    m_v3Camera.x = (inverseZoom * m_nearZoom.m_v3Camera.x) + (zoom * m_farZoom.m_v3Camera.x);
    m_v3Camera.y = (inverseZoom * m_nearZoom.m_v3Camera.y) + (zoom * m_farZoom.m_v3Camera.y);
    m_v3Camera.z = (inverseZoom * m_nearZoom.m_v3Camera.z) + (zoom * m_farZoom.m_v3Camera.z);

    m_fFOV = Interpolate(m_nearZoom.m_CameraData->fov, m_farZoom.m_CameraData->fov, m_fZoom);

    glMatrixLookAt(m_matView, m_v3Camera, m_v3Target, mUpVector);
}

/**
 * Offset/Address/Size: 0x874 | 0x801A9EB4 | size: 0x58
 */
void GameplayCamera::Reactivate()
{
    m_nearZoom.CalcDesiredTarget();
    m_nearZoom.m_fDampenedTargetX = m_nearZoom.m_fDesiredTargetX;
    m_nearZoom.m_fDampenedTargetY = m_nearZoom.m_fDesiredTargetY;

    m_farZoom.CalcDesiredTarget();
    m_farZoom.m_fDampenedTargetX = m_farZoom.m_fDesiredTargetX;
    m_farZoom.m_fDampenedTargetY = m_farZoom.m_fDesiredTargetY;
}

GameplayCameraZoomLevel::GameplayCameraZoomLevel()
    : m_CameraData(NULL)
    , m_fDesiredTargetX(0.0f)
    , m_fDesiredTargetY(0.0f)
    , m_fDampenedTargetX(0.0f)
    , m_fDampenedTargetY(0.0f)
    , m_fTargetSeekSpeedX(0.0f)
    , m_fTargetSeekSpeedY(0.0f)
    , m_fTargetSeekTime(0.15f)
{
}

static inline float MapFromFieldPosToTargetPos(float fPos, const float* pFieldKnots, const float* pTargetKnots, int nNumKnots)
{
    float fMin = pFieldKnots[0];
    float fMax = pFieldKnots[nNumKnots - 1] - 0.001f;

    fPos = (fPos >= fMin) ? fPos : fMin;
    fPos = (fPos <= fMax) ? fPos : fMax;

    int nKnot;
    for (nKnot = 0; nKnot < nNumKnots - 1; nKnot++)
    {
        if (fPos < pFieldKnots[nKnot + 1])
        {
            break;
        }
    }

    float fKnotPercent;
    if (pFieldKnots[nKnot] == pFieldKnots[nKnot + 1])
    {
        fKnotPercent = 0.0f;
    }
    else
    {
        fKnotPercent = (fPos - pFieldKnots[nKnot]) / (pFieldKnots[nKnot + 1] - pFieldKnots[nKnot]);
    }

    return Interpolate(pTargetKnots[nKnot], pTargetKnots[nKnot + 1], fKnotPercent);
}

/**
 * Offset/Address/Size: 0x470 | 0x801A9AB0 | size: 0x404
 */
void GameplayCameraZoomLevel::CalcDesiredTarget()
{
    nlVector3 v3OOIPos = { 0.0f, 0.0f, 0.0f };
    float fKnotTableBlendWeights[3];
    float fCurrWeight;
    float fDampenedBlendRiser;
    float fAccumulatedWeight;
    float fBlendPercent;
    float* pKnotTableBlendWeights;
    int i;

    if (gGameplayCameraInReplay == false)
    {
        cBall* pBall = g_pBall;
        if (pBall != NULL)
        {
            cPlayer* pBallOwner = pBall->m_pOwner;
            if (pBallOwner != NULL)
            {
                v3OOIPos = pBallOwner->m_v3Position;
            }
            else
            {
                v3OOIPos = pBall->m_v3Position;
            }
        }
    }
    else
    {
        ReplayManager* pReplayManager = ReplayManager::Instance();
        if (pReplayManager->mRender != NULL)
        {
            v3OOIPos = ReplayManager::Instance()->mRender->mBall.mPosition;
        }
    }

    pKnotTableBlendWeights = fKnotTableBlendWeights;

    pKnotTableBlendWeights[0] = 0.0f;
    pKnotTableBlendWeights[1] = 0.0f;
    pKnotTableBlendWeights[2] = 0.0f;

    fCurrWeight = 1.0f;
    for (int j = 0; j < 5; j++)
    {
        fDampenedBlendRiser = m_KnotTableBlendQueue[j].fBlendRiser;
        pKnotTableBlendWeights[m_KnotTableBlendQueue[j].nKnotTable] += fCurrWeight * fDampenedBlendRiser;
        if (1.0f == fDampenedBlendRiser)
        {
            break;
        }
        fCurrWeight *= 1.0f - fDampenedBlendRiser;
    }

    fAccumulatedWeight = 0.0f;
    m_fDesiredTargetX = 0.0f;
    m_fDesiredTargetY = 0.0f;

    i = 0;

    for (; i < 3; pKnotTableBlendWeights++, i++)
    {
        if (*pKnotTableBlendWeights > 0.0f)
        {
            fAccumulatedWeight += *pKnotTableBlendWeights;

            float fMappedX = MapFromFieldPosToTargetPos(v3OOIPos.x, m_CameraData->fieldKnotsX[i], m_CameraData->targetKnotsX[i], m_CameraData->numKnotsX);
            float fMappedY = MapFromFieldPosToTargetPos(v3OOIPos.y, m_CameraData->fieldKnotsY[i], m_CameraData->targetKnotsY[i], m_CameraData->numKnotsY);
            fBlendPercent = *pKnotTableBlendWeights / fAccumulatedWeight;

            m_fDesiredTargetX = Interpolate(m_fDesiredTargetX, fMappedX, fBlendPercent);
            m_fDesiredTargetY = Interpolate(m_fDesiredTargetY, fMappedY, fBlendPercent);
        }
    }
}

static inline void CalcCurrentKnotTable(GameplayCameraZoomLevel* self, bool forceNeutral)
{
    cPlayer* pBallOwner;
    int nNewKnotTable;
    int i;

    if (g_pBall != NULL)
    {
        pBallOwner = g_pBall->m_pOwner;
    }
    else
    {
        pBallOwner = NULL;
    }

    if (pBallOwner == NULL)
    {
        pBallOwner = g_pBall->m_pPassTarget;
    }

    if (pBallOwner != NULL && !forceNeutral)
    {
        if (pBallOwner->m_pTeam->GetOtherNet()->m_v3NetLocation.x > 0.0f)
        {
            nNewKnotTable = 1;
        }
        else
        {
            nNewKnotTable = 2;
        }
    }
    else
    {
        nNewKnotTable = 0;
    }

    if (nNewKnotTable != self->m_KnotTableBlendQueue[0].nKnotTable)
    {
        for (i = 4; i > 0; i--)
        {
            self->m_KnotTableBlendQueue[i] = self->m_KnotTableBlendQueue[i - 1];
        }

        self->m_KnotTableBlendQueue[0].nKnotTable = nNewKnotTable;
        self->m_KnotTableBlendQueue[0].fBlendRiser = 0.0f;
    }
}

/**
 * Offset/Address/Size: 0x5C | 0x801A969C | size: 0x414
 */
void GameplayCameraZoomLevel::Update(float fDeltaT, bool forceNeutral)
{
    float fSin;
    float fCos;
    float fOrientSin;
    float fOrientCos;
    float fXYDist;
    int i;

    if (gGameplayCameraInReplay)
    {
        forceNeutral = true;
    }

    CalcCurrentKnotTable(this, forceNeutral);

    if (forceNeutral)
    {
        m_KnotTableBlendQueue[0].fBlendRiser = 1.0f;
    }

    float t = fDeltaT / 0.75f;
    for (i = 0; i < 5; i++)
    {
        m_KnotTableBlendQueue[i].fBlendRiser += t;
        if (m_KnotTableBlendQueue[i].fBlendRiser >= 1.0f)
        {
            m_KnotTableBlendQueue[i].fBlendRiser = 1.0f;
            break;
        }
    }

    CalcDesiredTarget();

    {
        float change;
        float x;
        float omega = 2.0f / m_fTargetSeekTime;
        x = omega * fDeltaT;
        float exp = 1.0f / (((0.48f * x * x) + (1.0f + x)) + (x * (0.235f * x * x)));
        change = m_fDampenedTargetX - m_fDesiredTargetX;
        float currentVelocity = m_fTargetSeekSpeedX;

        m_fTargetSeekSpeedX = exp * (currentVelocity - (omega * (fDeltaT * ((omega * change) + currentVelocity))));
        m_fDampenedTargetX = (exp * (change + (fDeltaT * ((omega * change) + currentVelocity)))) + m_fDesiredTargetX;
    }

    {
        float change;
        float x;
        float omega = 2.0f / m_fTargetSeekTime;
        x = omega * fDeltaT;
        float exp = 1.0f / (((0.48f * x * x) + (1.0f + x)) + (x * (0.235f * x * x)));
        change = m_fDampenedTargetY - m_fDesiredTargetY;
        float currentVelocity = m_fTargetSeekSpeedY;

        m_fTargetSeekSpeedY = exp * (currentVelocity - (omega * (fDeltaT * ((omega * change) + currentVelocity))));
        m_fDampenedTargetY = (exp * (change + (fDeltaT * ((omega * change) + currentVelocity)))) + m_fDesiredTargetY;
    }

    nlSinCos(&fSin, &fCos, ((s32)(65536.0f * m_CameraData->pitch)) / 360);
    nlSinCos(&fOrientSin, &fOrientCos, ((s32)(65536.0f * m_CameraData->orientation)) / 360);

    fXYDist = fCos * m_CameraData->distance;

    m_v3Camera.x = (fOrientCos * fXYDist) + m_fDampenedTargetX;
    m_v3Camera.y = (fOrientSin * fXYDist) + m_fDampenedTargetY;
    m_v3Camera.z = fSin * m_CameraData->distance;

    float dampenedY = m_fDampenedTargetY;
    float dampenedX = m_fDampenedTargetX;
    m_v3Target.x = dampenedX;
    m_v3Target.y = dampenedY;
    m_v3Target.z = 0.0f;
}
