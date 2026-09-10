#include "Game/AI/AvoidController.h"
#include "Game/AI/AiUtil.h"
#include "Game/MathHelpers.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/BasicStadium.h"
#include "Game/Team.h"

extern cTeam* g_pCurrentlyUpdatingTeam;
extern PowerupBase* g_pPowerups[];

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };
static const nlVector2 v2Zero = { 0.0f, 0.0f };

static inline float fMin(float a, float b)
{
    if (a <= b)
    {
        return a;
    }
    return b;
}

/**
 * Offset/Address/Size: 0x22E8 | 0x8000993C | size: 0x98
 */
AvoidController::AvoidController(cFielder* fielder)
{
    m_pFielder = fielder;
    m_pFTweaks = (const FielderTweaks*)fielder->m_pTweaks;
    m_ThingsToAvoid = 0;
    m_CurrentlyAvoiding = 0;
    m_VeryCloseToSideline = false;
    m_UseMinimumAvoidance = false;
    m_pIgnoreThisPlayer = 0;

    m_SidelineNormal.x = 0.0f;
    m_SidelineNormal.y = 0.0f;
    m_SidelineDirection.x = 0.0f;
    m_SidelineDirection.y = 0.0f;

    m_LastRepulsionVector[0] = v3Zero;
    m_LastRepulsionVector[1] = v3Zero;
    m_LastRepulsionVector[2] = v3Zero;
    m_LastRepulsionVector[3] = v3Zero;
    m_LastRepulsionVector[4] = v3Zero;
    m_LastRepulsionVector[5] = v3Zero;
}

/**
 * Offset/Address/Size: 0x2250 | 0x800098A4 | size: 0x98
 */
void AvoidController::SetThingsToAvoid(int thingsToAvoid)
{
    m_ThingsToAvoid = thingsToAvoid;
    if (m_ThingsToAvoid != 0)
    {
        return;
    }

    m_CurrentlyAvoiding = 0;
    m_VeryCloseToSideline = false;
    m_SidelineUnavoidable = false;

    m_SidelineNormal = v2Zero;
    m_SidelineDirection = v2Zero;

    m_LastRepulsionVector[0] = v3Zero;
    m_LastRepulsionVector[1] = v3Zero;
    m_LastRepulsionVector[2] = v3Zero;
    m_LastRepulsionVector[3] = v3Zero;
    m_LastRepulsionVector[4] = v3Zero;
    m_LastRepulsionVector[5] = v3Zero;
}

/**
 * Offset/Address/Size: 0x2240 | 0x80009894 | size: 0x10
 */
void AvoidController::UseMinimumAvoidance(cPlayer* player)
{
    m_UseMinimumAvoidance = true;
    m_pIgnoreThisPlayer = player;
}

static inline int AvoidableEnumToIndex(eAvoidableThings avoidable)
{
    if (avoidable == AVOID_EVERYTHING)
    {
        return 5;
    }
    for (int i = 0; i < NUM_AVOIDABLES; i++)
    {
        if ((1 << i) & (int)avoidable)
        {
            return i;
        }
    }
    return -1;
}

/**
 * Offset/Address/Size: 0x2188 | 0x800097DC | size: 0xB8
 */
nlVector3& AvoidController::GetLastRepulsionVector(eAvoidableThings things)
{
    return m_LastRepulsionVector[AvoidableEnumToIndex(things)];
}

/**
 * Offset/Address/Size: 0x12BC | 0x80008910 | size: 0xECC
 */
#pragma opt_lifetimes off
void AvoidController::Update(float fDeltaT)
{
    nlVector3 vAccumulated_v3 = v3Zero;
    float fTotalWeight_v3 = 0.0f;

    bool bCanAvoid;
    m_VeryCloseToSideline = m_SidelineUnavoidable = bCanAvoid = false;

    if (m_ThingsToAvoid & AVOID_SIDELINES)
    {
        if (!Incapacitated(m_pFielder))
        {
            bCanAvoid = true;
        }
    }
    if (bCanAvoid)
    {
        AvoidSidelines();
    }

    bool bAverageWithLastRepulsion = (m_CurrentlyAvoiding & AVOID_FIELDERS) || (m_CurrentlyAvoiding & AVOID_GOALIES) || (m_CurrentlyAvoiding & AVOID_BOWSER);

    bCanAvoid = false;
    if (m_ThingsToAvoid & AVOID_FIELDERS)
    {
        if (!Invincible(m_pFielder))
        {
            if (!Incapacitated(m_pFielder))
            {
                bCanAvoid = true;
            }
        }
    }
    if (bCanAvoid)
    {
        nlVector3 v3Repulsion = v3Zero;
        bool bAvoidedSomething = CalcFielderRepulsionVector(v3Repulsion);

        if (bAvoidedSomething)
        {
            float fWeight = 1.0f;
            nlVec3ScaleAdd(vAccumulated_v3, fWeight, v3Repulsion, vAccumulated_v3);
            fTotalWeight_v3 += fWeight;

            m_CurrentlyAvoiding |= AVOID_FIELDERS;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_FIELDERS)] = v3Repulsion;
        }
        else
        {
            m_CurrentlyAvoiding &= ~AVOID_FIELDERS;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_FIELDERS)] = v3Zero;
        }
    }

    bCanAvoid = false;
    if (m_ThingsToAvoid & AVOID_POWERUPS)
    {
        if (!Incapacitated(m_pFielder))
        {
            bCanAvoid = true;
        }
    }
    if (bCanAvoid)
    {
        nlVector3 v3Repulsion = v3Zero;
        bool bAvoidedSomething = CalcPowerupRepulsionVector(v3Repulsion);

        if (bAvoidedSomething)
        {
            float fWeight = 1.0f;
            nlVec3ScaleAdd(vAccumulated_v3, fWeight, v3Repulsion, vAccumulated_v3);
            fTotalWeight_v3 += fWeight;

            m_CurrentlyAvoiding |= AVOID_POWERUPS;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_POWERUPS)] = v3Repulsion;
        }
        else
        {
            m_CurrentlyAvoiding &= ~AVOID_POWERUPS;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_POWERUPS)] = v3Zero;
        }
    }

    bCanAvoid = false;
    if (m_ThingsToAvoid & AVOID_GOALIES)
    {
        if (!Incapacitated(m_pFielder))
        {
            bCanAvoid = true;
        }
    }
    if (bCanAvoid)
    {
        nlVector3 v3Repulsion = v3Zero;
        bool bAvoidedSomething = CalcGoalieRepulsionVector(v3Repulsion);

        if (bAvoidedSomething)
        {
            float fWeight = 1.0f;
            nlVec3ScaleAdd(vAccumulated_v3, fWeight, v3Repulsion, vAccumulated_v3);
            fTotalWeight_v3 += fWeight;

            m_CurrentlyAvoiding |= AVOID_GOALIES;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_GOALIES)] = v3Repulsion;
        }
        else
        {
            m_CurrentlyAvoiding &= ~AVOID_GOALIES;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_GOALIES)] = v3Zero;
        }
    }

    bCanAvoid = false;
    if (m_ThingsToAvoid & AVOID_BOWSER)
    {
        if (!Incapacitated(m_pFielder))
        {
            bCanAvoid = true;
        }
    }
    if (bCanAvoid)
    {
        nlVector3 v3Repulsion = v3Zero;
        bool bAvoidedSomething = CalcBowserRepulsionVector(v3Repulsion);

        if (bAvoidedSomething)
        {
            float fWeight = 1.0f;
            nlVec3ScaleAdd(vAccumulated_v3, fWeight, v3Repulsion, vAccumulated_v3);
            fTotalWeight_v3 += fWeight;

            m_CurrentlyAvoiding |= AVOID_BOWSER;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_BOWSER)] = v3Repulsion;
        }
        else
        {
            m_CurrentlyAvoiding &= ~AVOID_BOWSER;
            m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_BOWSER)] = v3Zero;
        }
    }

    nlVector3 v3FinalRepulsion = v3Zero;
    nlVector3 v3SmoothedRepulsion;

    if (m_CurrentlyAvoiding != 0)
    {
        if (fTotalWeight_v3 > 0.0f)
        {
            float fInvTotalWeight = 1.0f / fTotalWeight_v3;
            v3FinalRepulsion.x = fInvTotalWeight * vAccumulated_v3.x;
            v3FinalRepulsion.y = fInvTotalWeight * vAccumulated_v3.y;
            v3FinalRepulsion.z = fInvTotalWeight * vAccumulated_v3.z;
        }
        else
        {
            v3FinalRepulsion = v3Zero;
        }

        if (bAverageWithLastRepulsion)
        {
            float fLastRepulsionWeight = 0.5f;

            nlVec3Scale(v3SmoothedRepulsion, v3FinalRepulsion, fLastRepulsionWeight);
            nlVec3ScaleAdd(v3SmoothedRepulsion, fLastRepulsionWeight, m_LastRepulsionVector[5], v3SmoothedRepulsion);
        }
        else
        {
            v3SmoothedRepulsion = v3FinalRepulsion;
        }

        ApplyRepulsionVector(v3SmoothedRepulsion);
    }
    else
    {
        float fLastRepulsionWeight = 0.95f;
        v3FinalRepulsion.x = fLastRepulsionWeight * m_LastRepulsionVector[5].x;
        v3FinalRepulsion.y = fLastRepulsionWeight * m_LastRepulsionVector[5].y;
        v3FinalRepulsion.z = fLastRepulsionWeight * m_LastRepulsionVector[5].z;

        if ((m_ThingsToAvoid != 0) && (nlVec3LengthSquared(v3FinalRepulsion) > 0.1f))
        {
            ApplyRepulsionVector(v3FinalRepulsion);
        }
        else
        {
            v3FinalRepulsion = v3Zero;
        }
    }

    m_UseMinimumAvoidance = false;
    m_pIgnoreThisPlayer = 0;
    m_LastRepulsionVector[5] = v3FinalRepulsion;
}
#pragma opt_lifetimes reset

/**
 * Offset/Address/Size: 0x1060 | 0x800086B4 | size: 0x25C
 */
bool AvoidController::CalcFielderRepulsionVector(nlVector3& v3OutRepulsion)
{
    bool bAvoidedSomething = false;
    float fDeltaX, fDeltaY, fDeltaZ;
    float fDistance, fMagnitude, fClosingSpeed;

    v3OutRepulsion.x = 0.0f;
    v3OutRepulsion.y = 0.0f;
    v3OutRepulsion.z = 0.0f;

    for (int i_team = 0; i_team < 2; i_team++)
    {
        for (int i_fielder = 0; i_fielder < 4; i_fielder++)
        {
            cFielder* pFielder = g_pTeams[i_team]->GetFielder(i_fielder);
            if (pFielder == m_pFielder)
            {
                continue;
            }

            fDeltaZ = m_pFielder->m_v3Position.y - pFielder->m_v3Position.y;
            fDeltaX = m_pFielder->m_v3Position.x - pFielder->m_v3Position.x;
            fDeltaY = m_pFielder->m_v3Position.z - pFielder->m_v3Position.z;

            float fDistanceSq = fDeltaZ * fDeltaZ;
            fDistanceSq += fDeltaX * fDeltaX;
            fDistanceSq += fDeltaY * fDeltaY;
            if (fDistanceSq > 6.25f)
            {
                continue;
            }

            fDistance = nlSqrt(fDistanceSq, true);

            float fInvDistance = 1.0f / fDistance;
            fDeltaY = fInvDistance * fDeltaY;
            fDeltaZ = fInvDistance * fDeltaZ;
            fDeltaX = fInvDistance * fDeltaX;

            fDistance -= m_pFTweaks->fPhysCapsuleRadius + pFielder->m_pTweaks->fPhysCapsuleRadius;
            fClosingSpeed = GetClosingSpeed2D(m_pFielder->GetPosition(), m_pFielder->GetVelocity(), pFielder->GetPosition(), pFielder->GetVelocity());

            fMagnitude = 6.5f * NormalizeVal(fDistance, 2.5f, 0.5f);
            fMagnitude += 3.0f * NormalizeVal(fClosingSpeed, 0.0f, 3.0f);

            if (m_UseMinimumAvoidance)
            {
                fMagnitude *= 0.3f;
            }

            m_pFielder->IsOnSameTeam(pFielder);
            if (m_pFielder->m_pMark == pFielder)
            {
                fMagnitude *= 0.4f;
            }

            if (pFielder == m_pIgnoreThisPlayer)
            {
                fMagnitude *= 0.3f;
            }

            if (m_pFielder->m_pBall != NULL)
            {
                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                fMagnitude *= 2.0f * pSkillTweaks->Off_Avoidance;
            }

            if (fMagnitude <= 0.0f)
            {
                continue;
            }

            float fContribution = 10.0f;
            if (fMagnitude <= fContribution)
            {
                fContribution = fMagnitude;
            }

            nlVec3Add(v3OutRepulsion, fContribution * fDeltaX, fContribution * fDeltaZ, fContribution * fDeltaY);
            bAvoidedSomething = true;
        }
    }

    return bAvoidedSomething;
}

bool AvoidController::CalcGoalieRepulsionVector(nlVector3& v3OutRepulsion)
{
    bool bAvoidedSomething = false;
    nlVec3Set(v3OutRepulsion, 0.0f, 0.0f, 0.0f);

    for (int i_team = 0; i_team < 2; i_team++)
    {
        Goalie* pGoalie = g_pTeams[i_team]->GetGoalie();

        const float fDeltaY = m_pFielder->m_v3Position.y - pGoalie->m_v3Position.y;
        const float fDeltaX = m_pFielder->m_v3Position.x - pGoalie->m_v3Position.x;
        nlVector3 v3Delta;
        nlVec3Set(
            v3Delta,
            fDeltaX,
            fDeltaY,
            m_pFielder->m_v3Position.z - pGoalie->m_v3Position.z);

        float fDistanceSq = nlVec3LengthSquared(v3Delta);
        if (fDistanceSq > 16.0f)
        {
            continue;
        }

        float fDistance = nlSqrt(fDistanceSq, true);
        nlVec3Scale(v3Delta, v3Delta, 1.0f / fDistance);

        fDistance -= m_pFTweaks->fPhysCapsuleRadius + pGoalie->m_pTweaks->fPhysCapsuleRadius;

        float fMagnitude;
        float fClosingSpeed = GetClosingSpeed2D(
            m_pFielder->GetPosition(),
            m_pFielder->GetVelocity(),
            pGoalie->GetPosition(),
            pGoalie->GetVelocity());

        fMagnitude = 10.0f * NormalizeVal(fDistance, 4.0f, 0.5f);
        fMagnitude += 3.0f * NormalizeVal(fClosingSpeed, 0.0f, 3.0f);

        if (!m_pFielder->IsOnSameTeam(pGoalie))
        {
            fMagnitude *= 1.5f;
        }

        if (m_pFielder->m_pBall != NULL)
        {
            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            fMagnitude *= 1.5f * pSkillTweaks->Off_Avoidance;
        }

        if (m_UseMinimumAvoidance)
        {
            fMagnitude *= 0.3f;
        }

        if (!(fMagnitude <= 0.0f))
        {
            nlVec3ScaleAdd(
                v3OutRepulsion,
                fMin(fMagnitude, 10.0f),
                v3Delta,
                v3OutRepulsion);
            bAvoidedSomething = true;
        }
    }

    return bAvoidedSomething;
}

bool AvoidController::CalcBowserRepulsionVector(nlVector3& v3OutRepulsion)
{
    Bowser* pBowser;
    bool bAvoidedSomething = false;
    float fDeltaZ;
    float fDeltaY;
    float fDeltaX;
    float fDistance, fMagnitude, fClosingSpeed;

    pBowser = BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser;
    if ((pBowser != NULL) && (pBowser->meBowserState != BOWSER_STATE_HIDDEN))
    {
        nlVector3 v3Delta;
        nlVec3Sub(v3Delta, m_pFielder->m_v3Position, pBowser->mv3Position);
        fDeltaY = v3Delta.y;
        fDeltaX = v3Delta.x;
        fDeltaZ = v3Delta.z;

        float fDistanceSq = fDeltaY * fDeltaY;
        fDistanceSq += fDeltaX * fDeltaX;
        fDistanceSq += fDeltaZ * fDeltaZ;

        if (fDistanceSq <= 49.0f)
        {
            fDistance = nlSqrt(fDistanceSq, true);
            float fInvDistance = 1.0f / fDistance;
            nlVec3Scale(v3Delta, fInvDistance);

            fDistance -= m_pFTweaks->fPhysCapsuleRadius + pBowser->mpPhysObj->GetRadius();
            fClosingSpeed = GetClosingSpeed2D(
                m_pFielder->GetPosition(),
                m_pFielder->GetVelocity(),
                pBowser->mv3Position,
                pBowser->mv3Velocity);

            fMagnitude = 10.0f * NormalizeVal(fDistance, 7.0f, 2.0f);
            fMagnitude += 3.0f * NormalizeVal(fClosingSpeed, 0.0f, 3.0f);

            if (m_pFielder->m_pBall != NULL)
            {
                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                fMagnitude *= 3.0f * pSkillTweaks->Off_Avoidance;
            }

            if (fMagnitude > 0.0f)
            {
                fMagnitude = (fMagnitude <= 10.0f) ? fMagnitude : 10.0f;

                nlVec3ScaleAdd(v3OutRepulsion, fMagnitude, v3Delta, v3OutRepulsion);

                bAvoidedSomething = true;
            }
        }
    }

    return bAvoidedSomething;
}

/**
 * Offset/Address/Size: 0xE08 | 0x8000845C | size: 0x258
 */
bool AvoidController::CalcPowerupRepulsionVector(nlVector3& v3OutRepulsion)
{
    bool bAvoidedSomething = false;
    float fDeltaX, fDeltaY, fDeltaZ;
    float fDistance;
    float fMagnitude;
    float fClosingSpeed;

    v3OutRepulsion.x = 0.0f;
    v3OutRepulsion.y = 0.0f;
    v3OutRepulsion.z = 0.0f;

    if (m_pFielder->IsInvincible())
    {
        return false;
    }

    for (int i = 0; i < 25; i++)
    {
        PowerupBase* pPowerup = g_pPowerups[i];

        if (pPowerup != NULL)
        {
            nlVector3 delta;
            nlVec3Sub(delta, m_pFielder->m_v3Position, pPowerup->m_v3Position);
            fDeltaX = delta.x;
            fDeltaY = delta.y;
            fDeltaZ = delta.z;
            float fDistanceSq = nlVec3DotProduct(delta, delta);

            if (fDistanceSq > 25.0f)
            {
                continue;
            }

            fDistance = nlSqrt(fDistanceSq, true);

            float fInvDistance = 1.0f / fDistance;
            fDeltaZ = fInvDistance * fDeltaZ;
            fDeltaY = fInvDistance * fDeltaY;
            fDeltaX = fInvDistance * fDeltaX;

            fDistance -= m_pFTweaks->fPhysCapsuleRadius + pPowerup->GetRadius();

            fClosingSpeed = GetClosingSpeed2D(m_pFielder->GetPosition(), m_pFielder->GetVelocity(), pPowerup->m_v3Position, pPowerup->m_v3Velocity);

            float fPowerupSpeedSq = pPowerup->m_v3Velocity.x * pPowerup->m_v3Velocity.x
                                  + pPowerup->m_v3Velocity.y * pPowerup->m_v3Velocity.y
                                  + pPowerup->m_v3Velocity.z * pPowerup->m_v3Velocity.z;

            if (fPowerupSpeedSq <= 2.0f)
            {
                fMagnitude = 10.0f * NormalizeVal(fDistance, 3.0f, 1.0f);
            }
            else
            {
                fMagnitude = 10.0f * NormalizeVal(fDistance, 5.0f, 2.0f);
                fMagnitude += 15.0f * NormalizeVal(fClosingSpeed, 0.0f, 4.0f);

                if (m_pFielder->m_pBall != NULL)
                {
                    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                    fMagnitude *= 2.0f * pSkillTweaks->Off_Avoidance;
                }
            }

            if (fMagnitude <= 0.0f)
            {
                continue;
            }

            float fContribution = 30.0f;
            if (fMagnitude <= fContribution)
            {
                fContribution = fMagnitude;
            }

            nlVec3Add(v3OutRepulsion, fContribution * fDeltaX, fContribution * fDeltaY, fContribution * fDeltaZ);
            bAvoidedSomething = true;
        }
    }

    return bAvoidedSomething;
}

/**
 * Offset/Address/Size: 0xC30 | 0x80008284 | size: 0x1D8
 */
bool AvoidController::CalcDesiredVelocityToAvoidSideline(
    nlVector2& vNewDesiredVelDir,
    const nlVector2& vCurrentDesiredVelDir,
    const nlVector2& vCurrentVelDir,
    const nlVector2& vSidelinePos,
    const nlVector2& vSidelineNormal)
{
    bool bHitSideline = false;

    float fDotNormalVel = vSidelineNormal.x * vCurrentDesiredVelDir.x + vSidelineNormal.y * vCurrentDesiredVelDir.y;

    nlVector2* pPos;
    if (m_pFielder->m_pBall != NULL)
    {
        pPos = (nlVector2*)&m_pFielder->m_pBall->m_v3Position;
    }
    else
    {
        pPos = (nlVector2*)&m_pFielder->m_v3Position;
    }

    float dy = pPos->y - vSidelinePos.y;
    float dx = pPos->x - vSidelinePos.x;
    float fDistance = nlSqrt(dx * dx + dy * dy, true);

    float fMaxDistance = 0.5f;
    fDistance -= m_pFTweaks->fPhysCapsuleRadius;
    if (m_CurrentlyAvoiding & AVOID_SIDELINES)
    {
        fMaxDistance = 1.0f;
    }

    float fMinDistance = 2.0f;
    m_SidelineUnavoidable = false;
    m_VeryCloseToSideline = false;

    if (fDistance <= fMinDistance)
    {
        m_VeryCloseToSideline = true;
        m_SidelineNormal = vSidelineNormal;
        m_SidelineDirection = vNewDesiredVelDir;
    }

    if (fDistance <= fMaxDistance)
    {
        if (fDotNormalVel <= 0.0f)
        {
            if (fDotNormalVel > -0.99f)
            {
                float fCos, fSin;
                nlSinCos(&fSin, &fCos, 0x4000);

                nlVector2 vParallelVelDir;
                vParallelVelDir.x = vSidelineNormal.x * fCos - vSidelineNormal.y * fSin;
                vParallelVelDir.y = vSidelineNormal.y * fCos + vSidelineNormal.x * fSin;

                if (vParallelVelDir.x * vCurrentDesiredVelDir.x + vParallelVelDir.y * vCurrentDesiredVelDir.y < 0.0f)
                {
                    vParallelVelDir.y = v3Zero.y - vParallelVelDir.y;
                    vParallelVelDir.x = v3Zero.x - vParallelVelDir.x;
                }

                vNewDesiredVelDir = vParallelVelDir;
            }
            else
            {
                vNewDesiredVelDir = v2Zero;
                m_SidelineUnavoidable = true;
            }
            bHitSideline = true;
        }
    }

    return bHitSideline;
}

/**
 * Offset/Address/Size: 0x8C8 | 0x80007F1C | size: 0x368
 */
bool AvoidController::CalcDesiredVelocityToAvoidCorner(
    nlVector2& vNewDesiredVelDir,
    const sCornerSegment& corner,
    const nlVector2& vCurrentDesiredVelDir,
    const nlVector2& vCurrentVelDir)
{
    bool bHitSideline = false;
    nlVector2 vSidelinePos;
    nlVector2 vSidelineNormal;
    nlVector2 vPosition = *(nlVector2*)&m_pFielder->m_v3Position;
    nlVector2 vBallPosition;

    if (m_pFielder->m_pBall != NULL)
    {
        vBallPosition = *(nlVector2*)&m_pFielder->m_pBall->m_v3Position;
    }
    else
    {
        vBallPosition = vPosition;
    }

    float fDistX;
    float fDistY;
    fDistY = corner.vCenter.y - vBallPosition.y;
    fDistX = corner.vCenter.x - vBallPosition.x;
    if (nlSqrt(fDistX * fDistX + fDistY * fDistY, true) <= corner.fRadius)
    {
        float fDeltaX;
        float fDeltaY;
        fDeltaY = vBallPosition.y - corner.vCenter.y;
        fDeltaX = vBallPosition.x - corner.vCenter.x;

        f32 fAngle = 10430.378f * nlATan2f(fDeltaY, fDeltaX);
        u16 aCornerToPos = (u16)(s32)fAngle;

        u16 absStart = (u16)abs_s16((s16)(aCornerToPos - corner.thetaStart));

        u16 absEnd = (u16)abs_s16((s16)(aCornerToPos - corner.thetaEnd));

        if (absStart >= absEnd)
            absEnd = absStart;
        if ((s16)absEnd <= 0x4000)
        {
            float fFielderX;
            float fFielderY;
            fFielderY = vPosition.y - corner.vCenter.y;
            fFielderX = vPosition.x - corner.vCenter.x;

            f32 fAngle2 = 10430.378f * nlATan2f(fFielderY, fFielderX);
            u16 aCornerToFielder = (u16)(s32)fAngle2;

            u16 absStart2 = (u16)abs_s16((s16)(aCornerToFielder - corner.thetaStart));

            u16 absEnd2 = (u16)abs_s16((s16)(aCornerToFielder - corner.thetaEnd));

            if (absStart2 >= absEnd2)
                absEnd2 = absStart2;
            if ((s16)absEnd2 <= 0x4000)
            {
                float fInvDistance = nlRecipSqrt(fFielderX * fFielderX + fFielderY * fFielderY, true);
                float nx;
                float ny;
                ny = fInvDistance * fFielderY;
                nx = fInvDistance * fFielderX;
                nlVec2Set(vSidelineNormal, v2Zero.x - nx, v2Zero.y - ny);
                nlVec2Set(vSidelinePos, corner.fRadius * nx + corner.vCenter.x, corner.fRadius * ny + corner.vCenter.y);
                bHitSideline = CalcDesiredVelocityToAvoidSideline(
                    vNewDesiredVelDir, vCurrentDesiredVelDir, vCurrentVelDir, vSidelinePos, vSidelineNormal);
            }
            else if (m_pFielder->IsTurboing())
            {
                float fInvDistance = nlRecipSqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY, true);
                float ny = fInvDistance * fDeltaY;
                float sidelineNormalY = v2Zero.y - ny;
                float nx = fInvDistance * fDeltaX;
                float sidelinePosX;
                float sidelinePosY;
                float sidelineNormalX;
                float fCornerDistX;
                float fCornerDistY;
                float fDotNormalVelY;
                float fDotNormalVel;
                sidelinePosY = corner.fRadius * ny + corner.vCenter.y;
                sidelinePosX = corner.fRadius * nx + corner.vCenter.x;
                fCornerDistY = vBallPosition.y - sidelinePosY;
                sidelineNormalX = v2Zero.x - nx;
                fDotNormalVelY = sidelineNormalY * vCurrentDesiredVelDir.y;
                fDotNormalVel = sidelineNormalX * vCurrentDesiredVelDir.x + fDotNormalVelY;
                fCornerDistX = vBallPosition.x - sidelinePosX;
                vSidelineNormal.y = sidelineNormalY;
                vSidelineNormal.x = sidelineNormalX;
                vSidelinePos.x = sidelinePosX;
                vSidelinePos.y = sidelinePosY;
                float fDistance = nlSqrt(fCornerDistX * fCornerDistX + fCornerDistY * fCornerDistY, true);
                float fMaxDistance = 0.5f;
                fDistance -= m_pFTweaks->fPhysCapsuleRadius;
                if (m_CurrentlyAvoiding & AVOID_SIDELINES)
                    fMaxDistance = 1.0f;
                if (fDistance <= fMaxDistance)
                {
                    if (fDotNormalVel <= 0.0f)
                    {
                        vNewDesiredVelDir = v2Zero;
                        bHitSideline = true;
                    }
                }
            }
        }
    }
    return bHitSideline;
}

static inline f32 ClampRunningWBSpeed(f32 speed, f32 maxSpeed)
{
    if (speed <= maxSpeed)
        return speed;
    else
        return maxSpeed;
}

/**
 * Offset/Address/Size: 0x41C | 0x80007A70 | size: 0x4AC
 */
bool AvoidController::AvoidSidelines()
{
    bool bHitSideline;
    bool bTurboAllowed;
    nlVector2 vCurrentVelDir;
    nlVector2 vCurrentDesiredVelDir;
    nlVector2 vNewDesiredVelDir;
    sCornerSegment corner;

    if (m_pFielder->GetDistanceToDesiredPos() <= 0.25f)
    {
        m_CurrentlyAvoiding &= ~AVOID_SIDELINES;
        m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_SIDELINES)] = v3Zero;
        return false;
    }

    bTurboAllowed = true;

    nlSinCos(&vCurrentVelDir.y, &vCurrentVelDir.x, m_pFielder->m_aActualMovementDirection);
    nlSinCos(&vCurrentDesiredVelDir.y, &vCurrentDesiredVelDir.x, m_pFielder->m_aDesiredMovementDirection);

    vNewDesiredVelDir = vCurrentDesiredVelDir;

    {
        u8* pBase = (u8*)cField::mCorners;
        int i = 0;
        for (; i < 4; i++)
        {
            u32* pSrc = (u32*)(pBase + i * sizeof(sCornerSegment));
            ((u32*)&corner)[0] = pSrc[0];
            ((u32*)&corner)[1] = pSrc[1];
            ((u32*)&corner)[2] = pSrc[2];
            ((u32*)&corner)[3] = pSrc[3];
            bHitSideline = CalcDesiredVelocityToAvoidCorner(vNewDesiredVelDir, corner, vCurrentDesiredVelDir, vCurrentVelDir);
            if (bHitSideline)
                break;
        }
    }

    if (!bHitSideline)
    {
        u8* pBase = (u8*)cField::mSidelines;
        int i = 0;
        for (; i < 4; i++)
        {
            sSideLinePlane* pSide = (sSideLinePlane*)(pBase + i * sizeof(sSideLinePlane));
            nlVector2 vSidelineNormal;
            nlVector2 vSidelinePos = *(nlVector2*)&m_pFielder->m_v3Position;
            float normY = v2Zero.y - pSide->vNormal.y;
            vSidelineNormal.x = v2Zero.x - pSide->vNormal.x;
            vSidelineNormal.y = normY;

            if (vSidelineNormal.x == 0.0f)
            {
                vSidelinePos.y = pSide->fDistance * pSide->vNormal.y;
            }
            else
            {
                vSidelinePos.x = pSide->fDistance * pSide->vNormal.x;
            }

            bHitSideline = CalcDesiredVelocityToAvoidSideline(
                vNewDesiredVelDir, vCurrentDesiredVelDir, vCurrentVelDir, vSidelinePos, vSidelineNormal);
            if (bHitSideline)
                break;
        }
    }

    if (bHitSideline)
    {
        bool isZero = false;
        if (v2Zero.x == vNewDesiredVelDir.x)
            if (v2Zero.y == vNewDesiredVelDir.y)
                isZero = true;
        if (isZero)
            bTurboAllowed = false;
        else
        {
            float fDot = vNewDesiredVelDir.x * vCurrentVelDir.x + vNewDesiredVelDir.y * vCurrentVelDir.y;
            if (fDot < 0.99f)
                bTurboAllowed = false;
            m_pFielder->m_aDesiredMovementDirection = (u16)(s32)(10430.378f * nlATan2f(vNewDesiredVelDir.y, vNewDesiredVelDir.x));
            m_pFielder->m_aDesiredFacingDirection = m_pFielder->m_aDesiredMovementDirection;
        }
    }

    if (!bTurboAllowed)
    {
        if (m_pFielder->IsRunning())
        {
            if (m_pFielder->m_pBall != NULL)
            {
                f32 fDesiredSpeed = ClampRunningWBSpeed(m_pFielder->m_fDesiredSpeed, m_pFTweaks->fRunningWBSpeed);
                u16 aDesiredMovementDir = m_pFielder->m_aDesiredMovementDirection;
                if (m_pFielder->IsTurboing())
                {
                    m_pFielder->InitActionRunningWB(false);
                    m_pFielder->InitMovementRunning(
                        m_pFTweaks->fRunningWBDirectionSeekSpeed,
                        m_pFTweaks->fRunningWBDirectionSeekFalloff,
                        m_pFTweaks->fRunningWBAccel,
                        m_pFTweaks->fRunningWBDecel);
                }
                m_pFielder->m_fDesiredSpeed = fDesiredSpeed;
                m_pFielder->m_aDesiredMovementDirection = aDesiredMovementDir;
                m_pFielder->m_aDesiredFacingDirection = aDesiredMovementDir;
            }
        }
    }

    int currentlyAvoiding;
    if (bHitSideline)
        currentlyAvoiding = m_CurrentlyAvoiding | AVOID_SIDELINES;
    else
        currentlyAvoiding = m_CurrentlyAvoiding & ~AVOID_SIDELINES;
    m_CurrentlyAvoiding = currentlyAvoiding;
    m_LastRepulsionVector[AvoidableEnumToIndex(AVOID_SIDELINES)] = v3Zero;
    return bHitSideline;
}

void AvoidController::CalcDesiredAndCurrentVelocities(
    nlVector2* vOutDesiredVel, nlVector2* vOutCurrentVel, bool bNormalized)
{
    if (vOutDesiredVel != NULL)
    {
        nlSinCos(&vOutDesiredVel->y, &vOutDesiredVel->x, m_pFielder->m_aDesiredMovementDirection);
        if (!bNormalized)
        {
            vOutDesiredVel->x *= m_pFielder->m_fDesiredSpeed;
            vOutDesiredVel->y *= m_pFielder->m_fDesiredSpeed;
        }
    }

    if (vOutCurrentVel != NULL)
    {
        nlSinCos(&vOutCurrentVel->y, &vOutCurrentVel->x, m_pFielder->m_aActualMovementDirection);
        if (!bNormalized)
        {
            vOutCurrentVel->x *= m_pFielder->m_fActualSpeed;
            vOutCurrentVel->y *= m_pFielder->m_fActualSpeed;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80007654 | size: 0x41C
 */
void AvoidController::ApplyRepulsionVector(nlVector3 v3Repulsion)
{
    if (m_pFielder->GetDistanceToDesiredPos() <= 0.5f)
    {
        return;
    }

    f32 fRepulsionMag = nlSqrt(
        v3Repulsion.x * v3Repulsion.x + v3Repulsion.y * v3Repulsion.y + v3Repulsion.z * v3Repulsion.z,
        true);
    if (fRepulsionMag <= 0.0f)
    {
        return;
    }

    nlVector3 v3DesiredVelDir = { 0.0f, 0.0f, 0.0f };
    nlVector3 v3RepulsionDir;
    nlVector3& rRepulsionDir = v3RepulsionDir;
    f32 fDesiredSpeed;

    CalcDesiredAndCurrentVelocities((nlVector2*)&v3DesiredVelDir, NULL, true);

    v3DesiredVelDir.z = 0.0f;

    f32 fInvRepulsionMag = nlRecipSqrt(
        v3Repulsion.x * v3Repulsion.x + v3Repulsion.y * v3Repulsion.y + v3Repulsion.z * v3Repulsion.z,
        true);

    f32 fRepulsionX = fInvRepulsionMag * v3Repulsion.x;
    f32 fRepulsionZ = fInvRepulsionMag * v3Repulsion.z;
    f32 fRepulsionY = fInvRepulsionMag * v3Repulsion.y;

    rRepulsionDir.x = fRepulsionX;
    rRepulsionDir.y = fRepulsionY;
    rRepulsionDir.z = fRepulsionZ;

    f32 fDot = nlVec3DotProduct(v3DesiredVelDir, rRepulsionDir);

    if (fDot <= -0.75f || fDot >= 0.7f)
    {
        f32 fCos;
        f32 fSin;

        f32 fRepulsionAngle = nlATan2f(fRepulsionY, fRepulsionX);
        f32 fDesiredAngle = nlATan2f(v3DesiredVelDir.y, v3DesiredVelDir.x);

        u16 aDesired = (u16)(s32)(10430.378f * fDesiredAngle);
        u16 aRepulsion = (u16)(s32)(10430.378f * fRepulsionAngle);
        s16 aDelta = (s16)(aRepulsion - aDesired);

        s16 aAdjust;
        if ((f32)aDelta > 0.0f)
        {
            aAdjust = 0x4000;
        }
        else
        {
            aAdjust = -0x4000;
        }

        nlSinCos(&fSin, &fCos, (u16)aAdjust);

        rRepulsionDir.x = v3DesiredVelDir.x * fCos - v3DesiredVelDir.y * fSin;
        rRepulsionDir.y = v3DesiredVelDir.y * fCos + v3DesiredVelDir.x * fSin;

        nlVec3Scale(v3Repulsion, rRepulsionDir, fRepulsionMag);
    }

    if (m_VeryCloseToSideline)
    {
        f32 fDotNormalVel = rRepulsionDir.x * m_SidelineNormal.x
                          + rRepulsionDir.y * m_SidelineNormal.y;

        if (fDotNormalVel < -0.1f)
        {
            f32 fSidelineDirXSq = m_SidelineDirection.x * m_SidelineDirection.x;
            f32 fSidelineDirYSq = m_SidelineDirection.y * m_SidelineDirection.y;

            if (fSidelineDirXSq + fSidelineDirYSq > 0.0f)
            {
                f32 fRepulsionDirZ = rRepulsionDir.z;
                f32 fSidelineDirZ = 0.0f;
                f32 fMinusOne = -1.0f;

                v3Repulsion.z = fRepulsionMag * fSidelineDirZ;

                f32 fScale = (fRepulsionDirZ * fSidelineDirZ
                                 + (rRepulsionDir.x * m_SidelineDirection.x + rRepulsionDir.y * m_SidelineDirection.y))
                           / nlGetLengthSquared3D(m_SidelineDirection.x, m_SidelineDirection.y, fSidelineDirZ);
                f32 fParallelY = fScale * m_SidelineDirection.y;
                f32 fParallelX = fScale * m_SidelineDirection.x;
                f32 fParallelZ = fScale * fSidelineDirZ;

                rRepulsionDir.x = fMinusOne * (rRepulsionDir.x - fParallelX) + fParallelX;
                rRepulsionDir.y = fMinusOne * (rRepulsionDir.y - fParallelY) + fParallelY;
                rRepulsionDir.z = fMinusOne * (fRepulsionDirZ - fParallelZ) + fParallelZ;
                rRepulsionDir.z = fSidelineDirZ;

                f32 fScaledY = fRepulsionMag * rRepulsionDir.y;
                f32 fScaledX = fRepulsionMag * rRepulsionDir.x;
                v3Repulsion.x = fScaledX;
                v3Repulsion.y = fScaledY;
            }
            else
            {
                nlVec2Set(*(nlVector2*)&v3Repulsion,
                    fRepulsionMag * m_SidelineNormal.x,
                    fRepulsionMag * m_SidelineNormal.y);
            }
        }
    }

    nlVector3 v3Resultant;
    nlVec3ScaleAdd(v3Resultant, m_pFielder->m_fDesiredSpeed, v3DesiredVelDir, v3Repulsion);
    f32 fResultantX = v3Resultant.x;
    f32 fResultantY = v3Resultant.y;
    f32 fResultantZ = v3Resultant.z;

    f32 fResultantMag = nlSqrt(fResultantX * fResultantX + fResultantY * fResultantY + fResultantZ * fResultantZ, true);

    m_pFielder->m_aDesiredMovementDirection = (u16)(s32)(10430.378f * nlATan2f(fResultantY, fResultantX));
    if (m_pFielder->m_eMovementState != MOVEMENT_STRAFING)
    {
        m_pFielder->m_aDesiredFacingDirection = m_pFielder->m_aDesiredMovementDirection;
    }

    if (m_pFielder->m_pBall != NULL)
    {
        fDesiredSpeed = m_pFTweaks->fRunningWBSpeed;
    }
    else
    {
        if ((float)fabs(fResultantMag - m_pFTweaks->fJoggingSpeed)
            < (float)fabs(fResultantMag - m_pFTweaks->fRunningSpeed))
        {
            fDesiredSpeed = m_pFTweaks->fJoggingSpeed;
        }
        else
        {
            fDesiredSpeed = m_pFTweaks->fRunningSpeed;
        }
    }

    m_pFielder->m_fDesiredSpeed = (m_pFielder->m_fDesiredSpeed >= fDesiredSpeed) ? m_pFielder->m_fDesiredSpeed : fDesiredSpeed;
}
