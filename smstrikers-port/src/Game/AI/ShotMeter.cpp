#include "Game/AI/ShotMeter.h"
#include "Game/AI/AIPad.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/Ball.h"
#include "Game/CharacterTweaks.h"
#include "Game/Game.h"
#include "Game/GameTweaks.h"
#include "Game/CharacterTriggers.h"

extern cTeam* g_pCurrentlyUpdatingTeam;
static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };

static inline void CalcShotAim(cFielder* pFielder, ShotMeter* pMeter);
static inline void CalcScoreValue(cFielder* pFielder, ShotMeter* pMeter);

/**
 * Offset/Address/Size: 0x6C4 | 0x800627E4 | size: 0x16C
 */
void ShotMeter::Update(float fDeltaT, bool bHoldTime)
{
    if (!bHoldTime)
    {
        m_fTime += fDeltaT;
    }

    switch (m_eShotMeterState)
    {
    case SHOT_METER_ACTIVE:
    {
        if (m_fTime >= g_pGame->m_pGameTweaks->fShotWindupTime)
        {
            float fNetDirection = -1.0f;
            cPlayer* pPrevOwner = g_pBall->m_pOwner;
            if (pPrevOwner != NULL)
            {
                cNet* pOtherNet = pPrevOwner->m_pTeam->GetOtherNet();
                fNetDirection = g_pBall->m_pOwner->m_v3Position.x * pOtherNet->m_v3NetLocation.x;
            }
            if (fNetDirection > 0.0f)
            {
                if (g_pBall->GetOwnerFielder() != NULL)
                {
                    if (g_pBall->GetOwnerFielder()->CanDoCaptainShootToScore())
                    {
                        m_eShotMeterState = SHOT_METER_STS_TRANSISTION;
                        break;
                    }
                }
            }
            if (!bHoldTime)
            {
                m_eShotMeterState = SHOT_METER_RELEASED;
            }
        }
        break;
    }
    case SHOT_METER_STS_TRANSISTION:
        m_eShotMeterState = SHOT_METER_STS_ACTIVE;
        break;
    case SHOT_METER_STS_ACTIVE:
    {
        SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
        float fSTSWindupTime = pSkillTweaks->fSTSWindupTime;
        float fShotWindupTime = g_pGame->m_pGameTweaks->fShotWindupTime;
        if (m_fTime >= fShotWindupTime + fSTSWindupTime)
        {
            if (!bHoldTime)
            {
                m_eShotMeterState = SHOT_METER_STS_RELEASED;
            }
        }
        break;
    }
    case SHOT_METER_RELEASED:
    case SHOT_METER_STS_RELEASED:
        m_eShotMeterState = SHOT_METER_INACTIVE;
        break;
    case SHOT_METER_INACTIVE:
    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0x684 | 0x800627A4 | size: 0x40
 */
void ShotMeter::Abort(cFielder* pFielder)
{
    KillWindups(pFielder);
    m_eShotMeterState = SHOT_METER_INACTIVE;
    m_fTime = 0.0f;
}

/**
 * Offset/Address/Size: 0x258 | 0x80062378 | size: 0x42C
 */
void ShotMeter::CalcOneTimerValue(cFielder* pFielder, bool bWasPerfectPass)
{
    m_eShotMeterState = SHOT_METER_INACTIVE;

    if (!bWasPerfectPass)
    {
        nlVector3 v3BallDirection;
        float fBallDirectionLengthSq;

        nlVec3Sub(v3BallDirection, g_pBall->m_v3Position, g_pBall->m_v3PrevPosition);
        fBallDirectionLengthSq = v3BallDirection.GetLengthSq3D();

        if (nlSqrt(fBallDirectionLengthSq, true) > 0.0001f)
        {
            float fBallDirectionInvLength = nlRecipSqrt(fBallDirectionLengthSq, true);
            nlVec3Scale(v3BallDirection, fBallDirectionInvLength);
        }
        else
        {
            v3BallDirection = v3Zero;
        }

        nlVector3 v3FielderToNet;
        float fFielderToNetLengthSq;
        const nlVector3& v3OffNetLocation = pFielder->GetAIOffNetLocation(NULL);

        nlVec3Sub(v3FielderToNet, v3OffNetLocation, pFielder->m_v3Position);
        fFielderToNetLengthSq = v3FielderToNet.GetLengthSq3D();

        if (nlSqrt(fFielderToNetLengthSq, true) > 0.0001f)
        {
            float fFielderToNetInvLength = nlRecipSqrt(fFielderToNetLengthSq, true);
            nlVec3Scale(v3FielderToNet, fFielderToNetInvLength);
        }
        else
        {
            v3FielderToNet = v3Zero;
        }

        const nlVector3& v3OffNetLocation2 = pFielder->GetAIOffNetLocation(NULL);
        float fDistX;
        float fDistY;
        float fDistZ;
        fDistY = g_pBall->m_v3Position.y - v3OffNetLocation2.y;
        fDistZ = g_pBall->m_v3Position.z - v3OffNetLocation2.z;
        fDistX = g_pBall->m_v3Position.x - v3OffNetLocation2.x;
        float fDistanceValue = InterpolateRangeClamped(0.0f, 1.0f, 20.0f, 7.5f, nlSqrt((fDistX * fDistX) + (fDistY * fDistY) + (fDistZ * fDistZ), true));
        float fDot = (v3FielderToNet.x * v3BallDirection.x) + (v3FielderToNet.y * v3BallDirection.y) + (v3FielderToNet.z * v3BallDirection.z);
        float fDirectionValue = InterpolateRangeClamped(0.0f, 1.0f, 1.0f, 0.0f, fDot);

        m_fSpeedValue = nlRandomf(0.2f * g_pGame->m_pGameTweaks->fShotMeterOneTimerMaxSpeed, &nlDefaultSeed);

        float fSumValue = fDirectionValue + fDistanceValue;
        float fHalfValue = 0.5f;
        float fCombinedValue = fSumValue * fHalfValue;
        m_fSpeedValue += InterpolateRangeClamped(0.25f, 0.8f * g_pGame->m_pGameTweaks->fShotMeterOneTimerMaxSpeed, 0.0f, 1.0f, fCombinedValue);
    }
    else
    {
        m_fSpeedValue = 1.0f;
    }

    CalcScoreValue(pFielder, this);
    CalcShotAim(pFielder, this);
}

/**
 * Offset/Address/Size: 0x21C | 0x8006233C | size: 0x3C
 */
float ShotMeter::GetTotalDuration() const
{
    float fShotWindupTime;
    float fSTSWindupTime;
    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    fSTSWindupTime = pSkillTweaks->fSTSWindupTime;
    fShotWindupTime = g_pGame->m_pGameTweaks->fShotWindupTime;
    return fShotWindupTime + fSTSWindupTime;
}

/**
 * Offset/Address/Size: 0x1FC | 0x8006231C | size: 0x20
 */
void ShotMeter::Reset()
{
    m_eShotMeterState = SHOT_METER_ACTIVE;
    m_fTime = 0.0f;
    m_fScoreValue = 0.0f;
    m_fSpeedValue = 0.0f;
    m_fSTSValue = 0.0f;
}

static inline void CalcShotAim(cFielder* pFielder, ShotMeter* pMeter)
{
    cAIPad* pPad = pFielder->m_pController;
    float fAimValue = 0.0f;
    if (pPad != NULL)
    {
        if (pPad->GetMovementStickMagnitude() > 0.0001f)
        {
            s16 dir = pPad->GetMovementStickDirection();
            if ((s16)(dir + 0x8000) >= 0)
            {
                fAimValue = -1.0f;
            }
            else
            {
                fAimValue = 1.0f;
            }
        }
    }
    pMeter->mfSShotAimValue = fAimValue;
}

static inline void CalcScoreValue(cFielder* pFielder, ShotMeter* pMeter)
{
    float fNetOpeness;
    float fPlayerDistance;
    float fChargedValue;
    float fRatingsValue;

    fRatingsValue = LikelyToScore(pFielder);
    fPlayerDistance = PlayerShotDistance(pFielder);
    fChargedValue = pMeter->m_fSpeedValue;

    float fShooting = ((FielderTweaks*)pFielder->m_pTweaks)->fShooting;
    GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;
    float fPositionWeighting = pGameTweaks->fShotMeterRatingsWeight;
    bool bIsChipShot = pFielder->mActionShotVars.bIsChipShot || pFielder->mActionLooseBallShotVars.bIsChipShot;

    fNetOpeness = fRatingsValue;

    if (!bIsChipShot)
    {
        float fPlayerWeighting = pGameTweaks->fShotMeterPlayerDistanceWeight;
        fShooting *= fPositionWeighting;
        float fNetWeighting = pGameTweaks->fShotMeterNetOpenWeight;
        fNetOpeness *= fNetWeighting;
        fPlayerDistance *= fPlayerWeighting;
        float fSumWeighting = fPlayerWeighting + fNetWeighting;
        float fTotalWeighting = fPositionWeighting + fSumWeighting;
        float fScore = fNetOpeness + fPlayerDistance;
        float fRemainder = 1.0f - fTotalWeighting;
        fChargedValue *= fRemainder;
        pMeter->m_fScoreValue = fShooting + (fChargedValue + fScore);
    }
    else
    {
        float fChipWeight = pGameTweaks->fShotChipMeterGPositionWeight;
        float fGoalieOut = GoalieOutOfPosition(pFielder);
        pGameTweaks = g_pGame->m_pGameTweaks;
        float fGoalieVal;
        float fSum;
        float fSumWeights;
        float fRemainder;
        fGoalieVal = fGoalieOut;
        fGoalieVal *= fChipWeight;
        fShooting *= fPositionWeighting;
        float fChipOpenWeight = pGameTweaks->fShotChipMeterNetOpenWeight;
        fNetOpeness *= fChipOpenWeight;
        fSum = fChipWeight + fChipOpenWeight;
        fSumWeights = fPositionWeighting + fSum;
        fRemainder = 1.0f - fSumWeights;
        fChargedValue *= fRemainder;
        pMeter->m_fScoreValue = fShooting + (fChargedValue + (fGoalieVal + fNetOpeness));
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80062120 | size: 0x1FC
 */
void ShotMeter::ShotReleased(cFielder* pFielder)
{
    KillWindups(pFielder);
    m_eShotMeterState = SHOT_METER_RELEASED;

    GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;
    if (m_fTime > pGameTweaks->fShotWindupTime)
    {
        m_fSpeedValue = 1.0f;
    }
    else
    {
        m_fSpeedValue = Interpolate(0.25f, 1.0f, m_fTime / pGameTweaks->fShotWindupTime);
    }

    CalcScoreValue(pFielder, this);
    CalcShotAim(pFielder, this);
    m_fTime = 0.0f;
}
