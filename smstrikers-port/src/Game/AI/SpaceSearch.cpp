#include "Game/AI/SpaceSearch.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/Fuzzy.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/AI/AiUtil.h"
#include "Game/Debug/ShapeRender.h"
#include "Game/Field.h"

static const nlVector2 g_vRunToNetDistanceConfidence = { 50.0f, 1.5f };
static const nlVector2 g_vRunToNetFormationDistConfidence = { 10.0f, 1.5f };

/**
 * Offset/Address/Size: 0x1874 | 0x800641C4 | size: 0x48
 */
SpaceSearch::~SpaceSearch()
{
}

/**
 * Offset/Address/Size: 0x1284 | 0x80063BD4 | size: 0x5F0
 *
 */
float SpaceSearch::FindBestPosition(nlVector3& v3Dest, const nlVector3& v3CenterPos, eFieldDirection eSearchDir, const nlVector3* pv3TargetOrDirection, float fMaxRadius, unsigned short aSearchCone)
{
    int numAngleSteps;
    int numRadiusSteps;
    unsigned short aDirection;
    nlVector3 v3BestOpenPosition;
    float fOriginalPositionScore;
    float fBestPositionScore;
    int aFromAngle;
    int aToAngle;
    long aDelta;
    float fRadiusDelta;
    nlVector3 v3LastPos;
    int numIterations;
    int i_radius;
    unsigned short aAngleDelta;
    int i_angle;
    nlVector3 v3TestPosition;
    float fScore;
    nlColour colour;

    aDirection = 0;

    switch (eSearchDir)
    {
    case DIR_NONE:
        aSearchCone = 0xFFFF;
        break;
    case DIR_UPFIELD:
        aDirection = (m_fNetDirection > 0.0f) ? 0x8000 : 0;
        break;
    case DIR_DOWNFIELD:
        aDirection = (m_fNetDirection > 0.0f) ? 0 : 0x8000;
        break;
    case DIR_TOWARD_TARGET:
        aDirection = (unsigned short)(s32)(10430.378f * nlATan2f(pv3TargetOrDirection->y - v3CenterPos.y, pv3TargetOrDirection->x - v3CenterPos.x));
        break;
    case DIR_AWAYFROM_TARGET:
        aDirection = (unsigned short)(s32)(10430.378f * nlATan2f(v3CenterPos.y - pv3TargetOrDirection->y, v3CenterPos.x - pv3TargetOrDirection->x));
        break;
    case DIR_CUSTOM:
        aDirection = (unsigned short)(s32)(10430.378f * nlATan2f(pv3TargetOrDirection->y, pv3TargetOrDirection->x));
        break;
    }

    v3BestOpenPosition = v3CenterPos;
    fOriginalPositionScore = EvaluatePosition(v3BestOpenPosition, v3CenterPos, eSearchDir, aDirection);
    fBestPositionScore = fOriginalPositionScore;

    nlPolar pLocation = { 0, 0.0f };
    float fMinRadius = 0.0f;

    if (fMaxRadius <= 0.0f)
    {
        fMaxRadius = 4.0f;
    }

    m_fMaxRadius = fMaxRadius;

    aFromAngle = (int)((float)aDirection - 0.5f * (float)aSearchCone);
    aToAngle = (int)((float)aDirection + 0.5f * (float)aSearchCone);
    aDelta = (short)(aToAngle - aFromAngle);

    numRadiusSteps = 5;
    if ((int)(0.5f + (fMaxRadius - fMinRadius) / 1.5f) < 5)
    {
        numRadiusSteps = (int)(0.5f + (fMaxRadius - fMinRadius) / 1.5f);
    }

    fRadiusDelta = (fMaxRadius - fMinRadius) / (float)numRadiusSteps;

    v3LastPos = v3CenterPos;
    numIterations = 0;

    static int maxIterations;
    static signed char init;
    if (!init)
    {
        maxIterations = 0;
        init = 1;
    }

    for (i_radius = 0; i_radius < numRadiusSteps; i_radius++)
    {
        pLocation.a = aFromAngle;
        pLocation.r += fRadiusDelta;
        if (i_radius == numRadiusSteps - 1)
        {
            pLocation.r = fMaxRadius;
        }

        aAngleDelta = (unsigned short)(int)(65536.0f * (1.5f / (6.2831855f * pLocation.r)));
        numAngleSteps = ((int)(0.5f + (float)(unsigned short)aDelta / (float)aAngleDelta) < 10) ? (int)(0.5f + (float)(unsigned short)aDelta / (float)aAngleDelta) : 10;

        aAngleDelta = (unsigned short)((unsigned short)aDelta / numAngleSteps);

        for (i_angle = 0; i_angle < numAngleSteps; i_angle++)
        {
            numIterations++;
            pLocation.a += aAngleDelta;
            if (i_angle == numAngleSteps - 1)
            {
                pLocation.a = aToAngle;
            }

            nlPolarToCartesian(v3TestPosition, pLocation);
            nlVec3Add(v3TestPosition, v3TestPosition, v3CenterPos);
            v3TestPosition.z = 0.0f;

            cField::FixOutOfBoundsPosition(v3TestPosition, 0.2f);

            fScore = EvaluatePosition(v3TestPosition, v3CenterPos, eSearchDir, aDirection);

            if (m_bDrawSearchSpace)
            {
                colour.c[0] = 0x99;
                colour.c[1] = 0;
                colour.c[2] = 0x99;
                colour.c[3] = 0xFF;
                g_ShapeRenderer.DrawLine3D(v3LastPos, v3TestPosition, colour, false);
                v3LastPos = v3TestPosition;
            }

            if (fScore > fBestPositionScore)
            {
                fBestPositionScore = fScore;
                v3BestOpenPosition = v3TestPosition;
                if (fScore > 1.0f && !m_bDrawSearchSpace)
                {
                    break;
                }
            }
        }

        if (fBestPositionScore > 1.0f && !m_bDebugOn)
        {
            break;
        }
    }

    if (numIterations > maxIterations)
    {
        maxIterations = numIterations;
    }

    v3Dest = v3BestOpenPosition;
    return fBestPositionScore;
}

/**
 * Offset/Address/Size: 0x117C | 0x80063ACC | size: 0x108
 */
SSearchOpenLane::SSearchOpenLane(cPlayer* pPlayer1, cPlayer* pPlayer2)
    : SpaceSearch(pPlayer1 != NULL ? pPlayer1->m_pTeam->m_pNet->m_fDirection : pPlayer2->m_pTeam->m_pNet->m_fDirection)
{
    if (pPlayer2 != NULL)
    {
        m_pBallOwner = pPlayer1;
        m_pPassTarget = pPlayer2;
        m_bOtherPosIsTarget = false;

        if (m_pBallOwner != NULL)
        {
            m_v3OtherPos = pPlayer1->m_v3Position;
        }
        else
        {
            m_v3OtherPos = g_pBall->m_v3Position;
        }
    }
    else
    {
        m_pBallOwner = pPlayer1;
        m_pPassTarget = NULL;
        m_v3OtherPos = pPlayer1->GetAIOffNetLocation(NULL);
        m_bOtherPosIsTarget = true;
    }
}

/**
 * Offset/Address/Size: 0x1060 | 0x800639B0 | size: 0x11C
 */
float SSearchOpenLane::EvaluatePosition(const nlVector3& position, const nlVector3& v3CenterPos, eFieldDirection eSearchDir, unsigned short aDirection)
{
    float fWeightedSum = 0.0f;
    float fTotalWeight = 0.0f;

    float fOpenToPosition;
    if (m_bOtherPosIsTarget)
    {
        fOpenToPosition = OpenToPosition(position, m_v3OtherPos, NULL, m_pBallOwner, m_pPassTarget, false);
    }
    else
    {
        fOpenToPosition = OpenToPosition(m_v3OtherPos, position, NULL, m_pBallOwner, m_pPassTarget, false);
    }

    fOpenToPosition = 0.0f + NormalizeVal(fOpenToPosition, 0.0f, 0.8f);
    fWeightedSum += fOpenToPosition;
    fTotalWeight += 1.0f;

    if (m_pPassTarget != NULL)
    {
        float fWideOpen = WideOpenPosition(position, m_pPassTarget->m_pTeam->GetOtherTeam(), NULL);
        fWeightedSum += 0.55f * fWideOpen;
        fTotalWeight += 0.55f;
    }

    float fNearToSideline = NearToSideline(position);
    float fAwayFromSideline = 1.0f - fNearToSideline;
    fWeightedSum += 0.2f * fAwayFromSideline;
    fTotalWeight += 0.2f;

    if (fTotalWeight > 0.0f)
    {
        return fWeightedSum / fTotalWeight;
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0xF18 | 0x80063868 | size: 0x148
 */
float SSearchIdealShot::EvaluatePosition(const nlVector3& position, const nlVector3& v3CenterPos, eFieldDirection eSearchDir, unsigned short aDirection)
{
    float fWeightedSum = 0.0f;
    float fTotalWeight = 0.0f;

    float fOpenToPosition = OpenToPosition(position, m_SSearchOpenLane.m_pBallOwner->GetAIOffNetLocation(NULL), NULL, m_SSearchOpenLane.m_pBallOwner, NULL, false);
    fWeightedSum += 0.5f * fOpenToPosition + 0.0f;
    fTotalWeight += 0.5f;

    float fOtherOpenToPosition = OpenPosition(position, m_SSearchOpenLane.m_pBallOwner->m_pTeam->GetOtherTeam(), m_SSearchOpenLane.m_pBallOwner, NULL);
    fWeightedSum += 0.3f * fOtherOpenToPosition;
    fTotalWeight += 0.3f;

    const cNet* pNet = m_pGoalie->m_pTeam->m_pNet;

    float fLikelyToScore = LikelyToScoreFromPosition(position, m_pGoalie->m_v3Position, pNet, false);
    fWeightedSum += 1.5f * fLikelyToScore;
    fTotalWeight += 1.5f;

    float fInFrontOfNet = PositionIsInFrontOfNet(position, pNet);
    fWeightedSum += 0.5f * fInFrontOfNet;
    fTotalWeight += 0.5f;

    float fNearToGoalie = NearToGoaliePosition(position, m_pGoalie->m_v3Position);
    fWeightedSum += 1.0f - fNearToGoalie;
    fTotalWeight += 1.0f;

    if (fTotalWeight > 0.0f)
    {
        return fWeightedSum / fTotalWeight;
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0xCEC | 0x8006363C | size: 0x22C
 */
SSearchBestPass::SSearchBestPass(cPlayer* pBallOwner, cPlayer* pPassTarget, bool bAllowLeadPass, bool bIsPerfectPass)
    : SpaceSearch(pBallOwner != NULL ? pBallOwner->m_pTeam->m_pNet->m_fDirection : pPassTarget->m_pTeam->m_pNet->m_fDirection)
    , m_SSearchIdealShot(pPassTarget)
    , m_SSearchOpenLane(pBallOwner, pPassTarget)
{
    m_bAllowLeadPass = bAllowLeadPass;
    m_bIsPerfectPass = bIsPerfectPass;
}

/**
 * Offset/Address/Size: 0xA10 | 0x80063360 | size: 0x2DC
 */
static float CalcIdealShootingPositionScore(const nlVector3& v3TestPosition, const nlVector3& v3OtherPosition, const nlVector2& v2OffNetPosition, float fMaxDistance)
{
    float fMaxDistanceLocal;
    nlVector2 vIdealDistance;
    float fScore;
    nlVector2 vDelta;
    nlVector2 vCandidate;

    fMaxDistanceLocal = fMaxDistance;
    vDelta.x = v2OffNetPosition.x - v3TestPosition.x;
    vDelta.y = v2OffNetPosition.y - v3TestPosition.y;
    fScore = 0.0f;

    vIdealDistance.x = g_pGame->m_pFuzzyTweaks->vIdealDistanceForShooting.x;

    {
        float fDistSq = vDelta.y * vDelta.y;
        fDistSq += vDelta.x * vDelta.x;

        if (fDistSq <= vIdealDistance.x * vIdealDistance.x)
        {
            float fInvDist = nlRecipSqrt(fDistSq, true);
            float fNegIdealDistance = -vIdealDistance.x;
            vCandidate.x = v2OffNetPosition.x + fNegIdealDistance * (fInvDist * vDelta.x);
            vCandidate.y = v2OffNetPosition.y + fNegIdealDistance * (fInvDist * vDelta.y);
        }
        else
        {
            float fDist = nlSqrt(fDistSq, true);
            float fScale = (fDist - vIdealDistance.x) / fDist;
            vCandidate.x = v3TestPosition.x + fScale * vDelta.x;
            vCandidate.y = v3TestPosition.y + fScale * vDelta.y;
        }
    }

    {
        float fDeltaY = vCandidate.y - v3TestPosition.y;
        float fDeltaX = vCandidate.x - v3TestPosition.x;
        vIdealDistance.y = g_pGame->m_pFuzzyTweaks->vIdealDistanceForShooting.y;

        if ((fDeltaX * fDeltaX + fDeltaY * fDeltaY) < (vIdealDistance.y * vIdealDistance.y))
        {
            nlVector3 v3OffNetPos = { 0.0f, 0.0f, 0.0f };
            v3OffNetPos.x = v2OffNetPosition.x;
            v3OffNetPos.y = v2OffNetPosition.y;
            fScore = PositionIsAtIdealDistanceForShooting(v3TestPosition, v3OffNetPos);
        }
        else
        {
            float fDx2 = v2OffNetPosition.x - v3OtherPosition.x;
            float fDy2 = v2OffNetPosition.y - v3OtherPosition.y;
            float fCandidateX2 = g_pGame->m_pFuzzyTweaks->vIdealDistanceForShooting.x;
            float fCandidateY2;
            float fDistSq2 = fDy2 * fDy2;
            fDistSq2 += fDx2 * fDx2;

            if (fDistSq2 <= fCandidateX2 * fCandidateX2)
            {
                float fInvDist2 = nlRecipSqrt(fDistSq2, true);
                float fNegIdealDistance2 = -fCandidateX2;
                fCandidateX2 = v2OffNetPosition.x + fNegIdealDistance2 * (fInvDist2 * fDx2);
                fCandidateY2 = v2OffNetPosition.y + fNegIdealDistance2 * (fInvDist2 * fDy2);
            }
            else
            {
                float fDist2 = nlSqrt(fDistSq2, true);
                float fScale2 = (fDist2 - fCandidateX2) / fDist2;
                fCandidateX2 = v3OtherPosition.x + fScale2 * fDx2;
                fCandidateY2 = v3OtherPosition.y + fScale2 * fDy2;
            }

            {
                nlVector2 vDelta3;
                float fDeltaLenSq;
                float fCandidateLenSq = fCandidateX2 * fCandidateX2 + fCandidateY2 * fCandidateY2;
                if (fCandidateLenSq > 0.1f)
                {
                    nlVec2Set(
                        vDelta3,
                        v3TestPosition.x - v3OtherPosition.x,
                        v3TestPosition.y - v3OtherPosition.y);
                    fDeltaLenSq = vDelta3.x * vDelta3.x + vDelta3.y * vDelta3.y;

                    if (fDeltaLenSq > 0.1f)
                    {
                        float fInvDeltaLen;
                        float fInvCandidateLen;
                        float fDot;

                        fScore = NormalizeVal(nlSqrt(fDeltaLenSq, true), 0.0f, fMaxDistanceLocal);

                        fInvDeltaLen = nlRecipSqrt(fDeltaLenSq, true);
                        fInvCandidateLen = nlRecipSqrt(fCandidateLenSq, true);

                        fDot = (fInvDeltaLen * vDelta3.x) * (fInvCandidateLen * fCandidateX2)
                             + (fInvDeltaLen * vDelta3.y) * (fInvCandidateLen * fCandidateY2);
                        fScore = fScore * NormalizeVal(fDot, 0.0f, 0.5f);
                    }
                }
            }
        }
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x594 | 0x80062EE4 | size: 0x47C
 */
float SSearchBestPass::EvaluatePosition(const nlVector3& position, const nlVector3& v3OtherPosition, eFieldDirection eSearchDir, unsigned short aDirection)
{
    float fWeightedSum = 0.0f;
    float fTotalWeight = 0.0f;

    float fOpenLane = m_SSearchOpenLane.EvaluatePosition(position, v3OtherPosition, eSearchDir, aDirection);
    fWeightedSum += 0.7f * fOpenLane;
    fTotalWeight += 0.7f;

    float fSidelineDist = fabsf(fabsf(position.y) - cField::GetSidelineY(1));
    float fSidelineNorm = NormalizeVal(fSidelineDist, 1.0f, 5.0f);
    float fSidelineInv = 1.0f - fSidelineNorm;
    fWeightedSum += 0.2f * (fSidelineNorm * fSidelineInv);
    fTotalWeight += 0.2f * fSidelineInv;

    float fInOffensiveZone = InOffensiveZone(position, m_fNetDirection < 0.0f ? (eTeamSide)0 : (eTeamSide)1);

    float fIdealShot = m_SSearchIdealShot.EvaluatePosition(position, v3OtherPosition, eSearchDir, aDirection);
    fWeightedSum += fInOffensiveZone * (0.15f * fIdealShot);
    fTotalWeight += 0.15f * fInOffensiveZone;

    nlVector2 v2GoalLine = { 0.0f, 0.0f };
    v2GoalLine.x = cField::GetGoalLineX(-m_fNetDirection);

    float fIdealPositionScore = CalcIdealShootingPositionScore(position, v3OtherPosition, v2GoalLine, m_fMaxRadius);
    float fIdealPositionWeight = InterpolateRangeClamped(1.0f, 1.5f, 0.0f, 1.0f, 1.0f - fInOffensiveZone);
    fWeightedSum += 0.2f * (fIdealPositionScore * fIdealPositionWeight);
    fTotalWeight += 0.2f * fIdealPositionWeight;

    if (m_SSearchOpenLane.m_pPassTarget != NULL)
    {
        nlVector3 v3PassTargetVelocity = m_SSearchOpenLane.m_pPassTarget->m_v3Velocity;
        float fPassTargetVelocitySq = v3PassTargetVelocity.GetLengthSq3D();

        if (fPassTargetVelocitySq >= 12.25f)
        {
            float fInvPassTargetVelocity = nlRecipSqrt(fPassTargetVelocitySq, true);

            nlVec3Scale(v3PassTargetVelocity, fInvPassTargetVelocity);

            nlVector3 v3VelDir;
            nlVec3Sub(v3VelDir, position, v3OtherPosition);
            float fDistSq = v3VelDir.GetLengthSq3D();

            if (fDistSq > 0.2f)
            {
                float fInvDist = nlRecipSqrt(fDistSq, true);
                nlVec3Scale(v3VelDir, fInvDist);
                float fDot = nlVec3DotProduct(v3VelDir, v3PassTargetVelocity);

                fWeightedSum += 0.15f * (0.0f >= fDot ? 0.0f : fDot);
                fTotalWeight += 0.15f;
            }
        }

        if (m_SSearchOpenLane.m_pBallOwner != NULL)
        {
            float fDy2 = position.x - m_SSearchOpenLane.m_pBallOwner->m_v3Position.x;
            float fDx2 = position.y - m_SSearchOpenLane.m_pBallOwner->m_v3Position.y;
            float fDistToOwner = nlSqrt(fDy2 * fDy2 + fDx2 * fDx2, true);

            if (m_bIsPerfectPass)
            {
                float fPerfectPassDist = NormalizeVal(fDistToOwner, g_pGame->m_pFuzzyTweaks->vFarTeammateConfidenceDistance);
                fWeightedSum += 0.5f * fPerfectPassDist;
                fTotalWeight += 0.5f;
            }
            else
            {
                float fNearPassDist = NormalizeVal(fDistToOwner, g_pGame->m_pFuzzyTweaks->vNearTeammateConfidenceDistance);
                fWeightedSum += 0.15f * (1.0f - fNearPassDist);
                fTotalWeight += 0.15f;
            }

            float fDy4;
            float fDx4;
            float fDistToTarget;
            float fPassSpeed;
            if (m_bAllowLeadPass)
            {
                fPassSpeed = m_SSearchOpenLane.m_pBallOwner->m_pTweaks->fPassVolleySpeedMax;
            }
            else
            {
                fPassSpeed = m_SSearchOpenLane.m_pBallOwner->m_pTweaks->fPassGroundSpeedMax;
            }

            float fDy3 = m_SSearchOpenLane.m_pBallOwner->m_v3Position.x - position.x;
            float fDx3 = m_SSearchOpenLane.m_pBallOwner->m_v3Position.y - position.y;
            float fOwnerDistNorm = nlSqrt(fDy3 * fDy3 + fDx3 * fDx3, true) / fPassSpeed;

            fDy4 = m_SSearchOpenLane.m_pPassTarget->m_v3Position.x - position.x;
            fDx4 = m_SSearchOpenLane.m_pPassTarget->m_v3Position.y - position.y;
            fDistToTarget = nlSqrt(fDy4 * fDy4 + fDx4 * fDx4, true);

            float fZero = 0.0f;
            float fNearZero = (float)(fabsf(fOwnerDistNorm - fZero) <= 0.0001f);
            if (!fNearZero)
            {
                float fLeadPass = NormalizeVal(fDistToTarget / fOwnerDistNorm, 0.0f, m_SSearchOpenLane.m_pPassTarget->m_pTweaks->fJoggingSpeed);
                fLeadPass *= (float)m_bIsPerfectPass;

                fWeightedSum += fLeadPass ? 0.2f : 0.1f;
                fTotalWeight += m_bIsPerfectPass ? 0.2f : 0.1f;
            }
        }
    }

    if (fTotalWeight > 0.0f)
    {
        return fWeightedSum / fTotalWeight;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x380 | 0x80062CD0 | size: 0x130
 */
SSearchRunToNet::SSearchRunToNet(cPlayer* pPlayer)
    : SpaceSearch(pPlayer->m_pTeam->m_pNet->m_fDirection)
    , m_SSearchIdealShot(pPlayer)
{
}

/**
 * Offset/Address/Size: 0x194 | 0x80062AE4 | size: 0x1EC
 */
float SSearchRunToNet::EvaluatePosition(const nlVector3& v3TestPosition, const nlVector3& v3CenterPos, eFieldDirection eSearchDir, unsigned short aDirection)
{
    float fTotalSum = 0.0f;
    float fTotalWeight = 0.0f;
    cFielder* pBallOwner = (cFielder*)m_SSearchIdealShot.m_SSearchOpenLane.m_pBallOwner;

    nlVector3 v3NetPosition = pBallOwner->GetAIOffNetLocation(NULL);
    float fNetX = v3NetPosition.x;
    float fNetY = v3NetPosition.y;

    float fDx = pBallOwner->m_v3Position.x - fNetX;
    float fDy = pBallOwner->m_v3Position.y - fNetY;
    if (fDx * fDx + fDy * fDy < 100.0f)
    {
        fTotalSum += m_SSearchIdealShot.EvaluatePosition(v3TestPosition, v3CenterPos, eSearchDir, aDirection);
        fTotalWeight += 1.0f;
    }
    else
    {
        float fDxNet = fNetX - v3TestPosition.x;
        float fDyNet = fNetY - v3TestPosition.y;
        float fDistToNet = nlSqrt(fDxNet * fDxNet + fDyNet * fDyNet, true);
        float fNormDist = NormalizeVal(fDistToNet, g_vRunToNetDistanceConfidence);
        fTotalSum += 5.0f * fNormDist;
        fTotalWeight += 5.0f;

        float fOpenPos = OpenPosition(v3TestPosition, pBallOwner->m_pTeam->GetOtherTeam(), NULL, NULL);
        fTotalSum += 0.5f * fOpenPos;
        fTotalWeight += 0.5f;

        OpenToPosition(v3TestPosition, v3NetPosition, pBallOwner->m_pTeam->GetOtherTeam(), pBallOwner, NULL, true);

        nlVector3 v3FormationPos;
        pBallOwner->GetFormationPosition(v3FormationPos, -1.0f);
        float fFormDx = v3FormationPos.x - v3TestPosition.x;
        float fFormDy = v3FormationPos.y - v3TestPosition.y;
        float fFormDist = nlSqrt(fFormDx * fFormDx + fFormDy * fFormDy, true);
        float fNormFormDist = NormalizeVal(fFormDist, g_vRunToNetFormationDistConfidence);
        fTotalSum += 0.4f * fNormFormDist;
        fTotalWeight += 0.4f;

        float fCloseToSideline = CloseToSideline(v3TestPosition, NULL, false);
        fTotalSum += 0.6f * (1.0f - fCloseToSideline);
        fTotalWeight += 0.6f;
    }

    if (fTotalWeight > 0.0f)
    {
        return fTotalSum / fTotalWeight;
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0x158 | 0x80062AA8 | size: 0x3C
 */
SSearchCutAndBreak::SSearchCutAndBreak(cPlayer* pPlayer)
    : SpaceSearch(pPlayer->m_pTeam->m_pNet->m_fDirection)
{
    m_pPlayer = pPlayer;
}

/**
 * Offset/Address/Size: 0x5C | 0x800629AC | size: 0xFC
 */
float SSearchCutAndBreak::EvaluatePosition(const nlVector3& v3TestPosition, const nlVector3& v3CenterPos, eFieldDirection eSearchDir, unsigned short aDirection)
{
    float fWeightedSum = 0.0f;
    float fTotalWeight = 0.0f;

    float fInFrontOfNet = PositionIsInFrontOfNet(v3TestPosition, m_pPlayer->m_pTeam->GetOtherTeam()->m_pNet);
    fWeightedSum += 0.5f * FGREATER(fInFrontOfNet, 0.5f) + 0.0f;
    fTotalWeight += 0.5f;

    float fWideOpen = WideOpenPosition(v3TestPosition, m_pPlayer->m_pTeam->GetOtherTeam(), m_pPlayer);
    fWeightedSum += 0.4f * fWideOpen;
    fTotalWeight += 0.4f;

    float fIdealDistance = PositionIsAtIdealDistanceForShooting(v3TestPosition, m_pPlayer->GetAIOffNetLocation(NULL));
    fWeightedSum += 0.3f * fIdealDistance;
    fTotalWeight += 0.3f;

    if (fTotalWeight > 0.0f)
    {
        return fWeightedSum / fTotalWeight;
    }
    return 0.0f;
}
