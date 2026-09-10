#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/FormationDefines.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/AvoidController.h"
#include "Game/AI/ShotMeter.h"
#include "Game/AI/Fuzzy.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/GameTweaks.h"
#include "Game/Goalie.h"
#include "Game/CharacterTweaks.h"
#include "Game/MathHelpers.h"
#include "types.h"
extern cTeam* g_pCurrentlyUpdatingTeam;
extern cBall* g_pScriptBall;
extern cBall* g_pBall;
extern cTeam* g_pScriptOtherTeam;
extern cTeam* g_pScriptCurrentTeam;
extern cFielder* g_pScriptBallOwner;

static const nlVector2 g_vOpenToAdjust = { 0.0f, 0.8f };
static const nlVector2 g_vPassCloseToDoneConfidence = { 0.0f, 0.5f };
static const nlVector2 g_vStallingConfidenceTime = { 1.0f, 8.0f };
static const nlVector2 v2Zero = { 0.0f, 0.0f };

static float CloseToGoaliePosition(const nlVector3& v3FromPos, const nlVector3& v3GoaliePos);
static float FarToGoaliePosition(const nlVector3& v3FromPos, const nlVector3& v3GoaliePos);

static inline float IsPassInPlay(cBall* pBall)
{
    if (pBall->m_fTotalPassTime > 0.0f)
    {
        float fElapsedTime = pBall->m_tPassTargetTimer.GetSeconds() / pBall->m_fTotalPassTime;
        return (1.0f - fElapsedTime);
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5E44 | 0x800848CC | size: 0x30
 */
float CalcSelectChance(float fDifficultyChance, float fPlayerAttribute)
{
    float fScore = 0.0f;

    if (fDifficultyChance > 0.0f)
    {
        float fDifficultyWeight = 1.0f;
        float weight = g_pGame->m_pGameTweaks->fFielderAttributeWeight;
        fDifficultyWeight = fDifficultyWeight - weight;
        fScore = (fDifficultyChance * fDifficultyWeight) + fPlayerAttribute * weight;
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x5E18 | 0x800848A0 | size: 0x2C
 */
float BallOwner(cPlayer* player)
{
    if (player == NULL)
        return 0.0f;

    return player->HasBall() ? 1.0f : 0.0f;
}

/**
 * Offset/Address/Size: 0x5DCC | 0x80084854 | size: 0x4C
 */
float BallOwnerT(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    u8 isOwnerOnTeam = 0;
    cPlayer* pOwner = g_pBall->m_pOwner;
    if (pOwner != NULL && pOwner->m_pTeam == team)
    {
        isOwnerOnTeam = 1;
    }

    return isOwnerOnTeam ? 1.0f : 0.0f;
}

/**
 * Offset/Address/Size: 0x5D9C | 0x80084824 | size: 0x30
 */
float LastBallOwner(cPlayer* player)
{
    if (player == NULL)
    {
        return 0.0f;
    }

    if (g_pScriptBall->m_pPrevOwner == player)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5D58 | 0x800847E0 | size: 0x44
 */
float Striker(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (fielder->IsStriker())
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5D14 | 0x8008479C | size: 0x44
 */
float Winger(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (fielder->IsWinger())
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5CD0 | 0x80084758 | size: 0x44
 */
float Midfield(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (fielder->IsMidField())
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5C8C | 0x80084714 | size: 0x44
 */
float Defence(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (fielder->IsDefense())
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5C48 | 0x800846D0 | size: 0x44
 */
float Captain(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (fielder->IsCaptain())
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5C1C | 0x800846A4 | size: 0x2C
 */
float GoalieType(cPlayer* player)
{
    if (player == NULL)
    {
        return 0.0f;
    }

    if (player->m_eClassType == GOALIE)
    {
        return 1.0f;
    }

    return 0.0f;
}

static float FielderType(cPlayer* player)
{
    if (player == NULL)
    {
        return 0.0f;
    }

    if (player->m_eClassType == FIELDER)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5BE0 | 0x80084668 | size: 0x3C
 */
float Marking(cFielder* pMarking, cPlayer* pMarked)
{
    if (pMarking == NULL)
    {
        return 0.0f;
    }

    if (pMarked == NULL)
    {
        return 0.0f;
    }

    if (pMarking->m_pMark == pMarked)
    {
        return 1.0f;
    }

    return 0.0f;
}

static float OnMyTeam(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (g_pScriptCurrentTeam == fielder->m_pTeam)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5BB0 | 0x80084638 | size: 0x30
 */
float OnTheirTeam(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (g_pScriptOtherTeam == fielder->m_pTeam)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5B20 | 0x800845A8 | size: 0x90
 */
float OnScreen(cPlayer* player)
{
    u8 onScreenFlags[2];

    if (player == NULL)
    {
        return 0.0f;
    }

    onScreenFlags[1] = 0;
    onScreenFlags[0] = onScreenFlags[1];

    if ((float)fabs(player->m_v3ScreenPosition.x) <= 1.0f)
    {
        if ((float)fabs(player->m_v3ScreenPosition.y) <= 1.0f)
        {
            onScreenFlags[0] = 1;
        }
    }

    if (onScreenFlags[0] != 0)
    {
        if ((float)fabs(player->m_v3ScreenPosition.z) <= 1.0f)
        {
            onScreenFlags[1] = 1;
        }
    }

    if (onScreenFlags[1] != 0)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x5AD0 | 0x80084558 | size: 0x50
 */
float OnTheGround(cPlayer* player)
{
    if (player == nullptr)
    {
        return 0.0f;
    }

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
    int jointIndex = player->m_nBip01JointIndex_0xA4;
    const nlVector3& jointPos = player->GetJointPosition(jointIndex);

    return NormalizeVal(jointPos.z, pFuzzyTweaks->vOnGroundConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x59EC | 0x80084474 | size: 0xE4
 */
float StrategicBallOwner(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    if (pFielder == g_pBall->m_pOwner)
    {
        return 1.0f;
    }

    float fWantsToReceivePass;
    if (pFielder == NULL)
    {
        fWantsToReceivePass = 0.0f;
    }
    else
    {
        eFielderDesireState desireState = pFielder->m_eFielderDesireState;
        fWantsToReceivePass = 0.0f;
        if (((s32)desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE) || ((s32)desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_RUN) || ((s32)desireState == FIELDERDESIRE_ONETIMER))
        {
            fWantsToReceivePass = 1.0f;
        }
    }

    float fWantsToReceivePassCheck = 0.0f;
    if (fWantsToReceivePass != fWantsToReceivePassCheck)
    {
        return 0.8f;
    }

    eFielderDesireState desireState = pFielder->m_eFielderDesireState;
    if (desireState == FIELDERDESIRE_INTERCEPT_BALL)
    {
        return AbleToInterceptBall(pFielder);
    }

    if ((g_pBall->m_tShotTimer.m_uPackedTime != 0) && (pFielder == g_pBall->m_pPrevOwner))
    {
        return 0.4f;
    }

    cTeam* pTeam = pFielder->m_pTeam;
    if (pFielder->m_pTeam->mpCurrentSituation == SITUATION_LOOSE && desireState == FIELDERDESIRE_USER_CONTROLLED)
    {
        return AbleToInterceptBall(pFielder);
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x59A0 | 0x80084428 | size: 0x4C
 */
float UserControlled(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    bool bHasGlobalPad = fielder->GetGlobalPad() != NULL;
    if (bHasGlobalPad)
    {
        return 1.0f;
    }

    return 0.0f;
}

static float InPassingLane(cFielder* pFielder, cPlayer* pPassTarget, float fPotentialBallSpeed)
{
    nlPolar pBallSpeedPolar;
    nlCartesianToPolar(pBallSpeedPolar, g_pScriptBall->m_v3Velocity.x, g_pScriptBall->m_v3Velocity.y);
    if (fPotentialBallSpeed <= 0.0f)
    {
        fPotentialBallSpeed = pBallSpeedPolar.r;
    }
    else
    {
        fPotentialBallSpeed = FMAX(fPotentialBallSpeed, pBallSpeedPolar.r);
    }

    nlVector3 v3BetweenIntercept = GetClosestPointOnLineABFromPointC(g_pScriptBall->m_v3Position, pPassTarget->m_v3Position, pFielder->m_v3Position);
    float fPossibleFielderDistance = nlSqrt(g_pScriptBall->m_v3Position.CalculateDistanceSquared2D(v3BetweenIntercept), true) / fPotentialBallSpeed;
    fPossibleFielderDistance = pFielder->GetRunningDistance(fPossibleFielderDistance);

    return NormalizeVal(nlSqrt(pFielder->m_v3Position.CalculateDistanceSquared2D(v3BetweenIntercept), true), fPossibleFielderDistance + g_pGame->m_pFuzzyTweaks->fPassLaneDistance, fPossibleFielderDistance);
}

/**
 * Offset/Address/Size: 0x5838 | 0x800842C0 | size: 0x168
 */
float InPassingLane(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    cPlayer* pPassTarget = g_pScriptBall->m_pPassTarget;
    if (pPassTarget == NULL)
    {
        return 0.0f;
    }

    if (pFielder->IsOnSameTeam(pPassTarget))
    {
        return 0.0f;
    }

    cPlayer* pPassingTarget = g_pScriptBall->m_pPassTarget;
    if (g_pScriptBall->m_pPrevOwner == NULL || pPassingTarget == NULL)
    {
        return 0.0f;
    }

    return InPassingLane(pFielder, pPassingTarget, 0.0f);
}

static float IsShotInPlay();

static float InShotLane(cFielder* pFielder)
{
    if (pFielder == NULL
        || (1.0f - NormalizeVal(nlSqrt(g_pScriptBall->m_v3Position.CalculateDistanceSquared2D(g_pScriptBall->m_v3ShotTarget), true), 0.0f, g_pGame->m_pFuzzyTweaks->fShotInPlayFullConfidenceDistance))
               == 0.0f)
    {
        return 0.0f;
    }

    nlVector3 v3BetweenIntercept = GetClosestPointOnLineABFromPointC(g_pScriptBall->m_v3Position, g_pScriptBall->m_v3ShotTarget, pFielder->m_v3Position);
    nlPolar pBallSpeedPolar;
    nlCartesianToPolar(pBallSpeedPolar, g_pScriptBall->m_v3Velocity);

    float fPossibleFielderDistance;
    fPossibleFielderDistance = nlSqrt(g_pScriptBall->m_v3Position.CalculateDistanceSquared2D(v3BetweenIntercept), true) / pBallSpeedPolar.r;
    fPossibleFielderDistance = pFielder->GetRunningDistance(fPossibleFielderDistance);

    return NormalizeVal(nlSqrt(pFielder->m_v3Position.CalculateDistanceSquared2D(v3BetweenIntercept), true), fPossibleFielderDistance + g_pGame->m_pFuzzyTweaks->fShotLaneDistance, fPossibleFielderDistance);
}

/**
 * Offset/Address/Size: 0x581C | 0x800842A4 | size: 0x1C
 */
float Aggressive(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    FielderTweaks* pTweaks = (FielderTweaks*)fielder->m_pTweaks;
    return pTweaks->fAggression;
}

/**
 * Offset/Address/Size: 0x5800 | 0x80084288 | size: 0x1C
 */
float Shooter(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    FielderTweaks* pTweaks = (FielderTweaks*)fielder->m_pTweaks;
    return pTweaks->fShooting;
}

/**
 * Offset/Address/Size: 0x57E4 | 0x8008426C | size: 0x1C
 */
float Passer(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    FielderTweaks* pTweaks = (FielderTweaks*)fielder->m_pTweaks;
    return pTweaks->fPassing;
}

/**
 * Offset/Address/Size: 0x57C8 | 0x80084250 | size: 0x1C
 */
float Deker(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    FielderTweaks* pTweaks = (FielderTweaks*)fielder->m_pTweaks;
    return pTweaks->fDekeing;
}

/**
 * Offset/Address/Size: 0x579C | 0x80084224 | size: 0x2C
 */
float RepeatingLastDesire(cFielder* fielder, eScriptFielderDesire eDesire)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if ((eFielderDesireState)eDesire == fielder->m_ePrevFielderDesireState)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x54B4 | 0x80083F3C | size: 0x2E8
 */
float AbleToInterceptBall(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    float fScore = 0.0f;

    if (!Incapacitated(pPlayer))
    {
        if (pPlayer->HasBall())
        {
            fScore = 1.0f;
        }
        else
        {
            if (pPlayer->m_eClassType == FIELDER)
            {
                cFielder* pFielder = (cFielder*)pPlayer;
                float fInterceptScore = NormalizeVal(pFielder->m_pTeam->mfBallInterceptTimes[pFielder->m_ID], g_pGame->m_pFuzzyTweaks->vInterceptBallConfidenceTime);
                float fPlayerCoord = pFielder->m_v3Position.x;
                float dx = g_pBall->m_v3Position.x - fPlayerCoord;
                fPlayerCoord = pFielder->m_v3Position.y;
                float dy = g_pBall->m_v3Position.y - fPlayerCoord;
                float distance = nlSqrt((dx * dx) + (dy * dy), true);
                float fClosenessScore = NormalizeVal(distance, g_pGame->m_pFuzzyTweaks->vInterceptBallConfidenceDistance);
                float weight = g_pGame->m_pFuzzyTweaks->fInterceptBallScoreWeight;
                fScore = (fInterceptScore * weight) + (fClosenessScore * (1.0f - weight));
            }
            else if (pPlayer->m_eClassType == GOALIE)
            {
                Goalie* pGoalie = (Goalie*)pPlayer;
                if (pGoalie->IsBusy())
                {
                    fScore = CloseToBall(pPlayer);
                }
            }
        }
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x52B0 | 0x80083D38 | size: 0x204
 */
float AbleToInterceptBallForSwapController(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    float fScore = 0.0f;

    if (!Incapacitated(pFielder))
    {
        if (pFielder->HasBall())
        {
            fScore = 1.0f;
        }
        else
        {
            float fInterceptScore = NormalizeVal(pFielder->m_pTeam->mfBallInterceptTimes[pFielder->m_ID], g_pGame->m_pFuzzyTweaks->vInterceptBallConfidenceTime);
            float fFielderCoord = pFielder->m_v3Position.x;
            float dx = g_pBall->m_v3Position.x - fFielderCoord;
            fFielderCoord = pFielder->m_v3Position.y;
            float dy = g_pBall->m_v3Position.y - fFielderCoord;
            float distance = nlSqrt((dx * dx) + (dy * dy), true);
            float fClosenessScore = NormalizeVal(distance, g_pGame->m_pFuzzyTweaks->vInterceptBallConfidenceDistance);
            float weight = g_pGame->m_pFuzzyTweaks->fInterceptBallSwapControlerScoreWeight;
            fScore = (fInterceptScore * weight) + (fClosenessScore * (1.0f - weight));
        }
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x5228 | 0x80083CB0 | size: 0x88
 */
float AbleToUsePowerup(cFielder* pFielder, int ePowerup)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return (!pFielder->IsPlayingPowerupAnim() && ePowerup == pFielder->m_pTeam->GetCurrentPowerUp().eType) ? 1.0f : 0.0f;
}

/**
 * Offset/Address/Size: 0x50E8 | 0x80083B70 | size: 0x140
 */
float LikelyToUsePowerup(cFielder* pFielder, int ePowerup)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    float fRandomChance = pFielder->GetPowerupUsageRandomChance(ePowerup);
    float fPowerupMatch = AbleToUsePowerup(pFielder, ePowerup);
    float fNormalizedZ = OnTheGround(pFielder);

    fPowerupMatch = FMIN(fPowerupMatch, fRandomChance);
    if (fNormalizedZ <= fPowerupMatch)
    {
        return fNormalizedZ;
    }

    return fPowerupMatch;
}

static float ReallyCloseToBall(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    float dx = g_pScriptBall->m_v3Position.x - pPlayer->m_v3Position.x;
    float dy = g_pScriptBall->m_v3Position.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);
    return NormalizeVal(distance, g_pGame->m_pFuzzyTweaks->vReallyCloseToBallDistanceConfidence);
}

/**
 * Offset/Address/Size: 0x5080 | 0x80083B08 | size: 0x68
 */
float CloseToBall(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    return NormalizeVal(
        nlSqrt(g_pScriptBall->m_v3Position.CalculateDistanceSquared2D(pPlayer->m_v3Position), true),
        g_pGame->m_pFuzzyTweaks->vCloseBallConfidenceDistance);
}

static float ClosestToBall(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    cTeam* pOtherTeam = pFielder->GetTeam()->GetOtherTeam();
    int index = 0;
    {
        int i = 0;
        do
        {
            if (pFielder->GetTeam()->GetBallInterceptFielder(i) == pFielder)
            {
                index = i;
                break;
            }
            i++;
        } while (i < 4);
    }

    for (int i = 0; i < 4; i++)
    {
        if (pFielder->GetTeam()->mfBallInterceptTimes[pFielder->m_ID] > pOtherTeam->mfBallInterceptTimes[pOtherTeam->GetFielder(i)->m_ID])
        {
            index++;
        }
    }

    return (float)(7 - index) / 7.0f;
}

static float ClosestToBallAmongstMyTeam(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    int index = -1;
    cTeam* pTeam = pFielder->GetTeam();
    for (int i = 0; i < 4; i++)
    {
        if (pTeam->GetBallInterceptFielder(i) == pFielder)
        {
            index = i;
            break;
        }
    }

    return (float)(3 - index) / 3.0f;
}

/**
 * Offset/Address/Size: 0x5018 | 0x80083AA0 | size: 0x68
 */
float NearToBall(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    return NormalizeVal(
        nlSqrt(g_pScriptBall->m_v3Position.CalculateDistanceSquared2D(pPlayer->m_v3Position), true),
        g_pGame->m_pFuzzyTweaks->vNearBallConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4FB0 | 0x80083A38 | size: 0x68
 */
float FarToBall(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    float dx = g_pScriptBall->m_v3Position.x - pPlayer->m_v3Position.x;
    float dy = g_pScriptBall->m_v3Position.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(distance, pFuzzyTweaks->vFarBallConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4F34 | 0x800839BC | size: 0x7C
 */
float CloseToMyNet(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = pPlayer->GetAIDefNetLocation(NULL);

    float dx = netLocation.x - pPlayer->m_v3Position.x;
    float dy = netLocation.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(distance, pFuzzyTweaks->vCloseNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4EB8 | 0x80083940 | size: 0x7C
 */
float NearToMyNet(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = pPlayer->GetAIDefNetLocation(NULL);

    float dx = netLocation.x - pPlayer->m_v3Position.x;
    float dy = netLocation.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(distance, pFuzzyTweaks->vNearNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4E3C | 0x800838C4 | size: 0x7C
 */
float FarToMyNet(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = pPlayer->GetAIDefNetLocation(NULL);

    float dx = netLocation.x - pPlayer->m_v3Position.x;
    float dy = netLocation.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(distance, pFuzzyTweaks->vFarNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4DC0 | 0x80083848 | size: 0x7C
 */
float CloseToTheirNet(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = pPlayer->GetAIOffNetLocation(NULL);

    float dx = netLocation.x - pPlayer->m_v3Position.x;
    float dy = netLocation.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(distance, pFuzzyTweaks->vCloseNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4D44 | 0x800837CC | size: 0x7C
 */
float NearToTheirNet(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = pPlayer->GetAIOffNetLocation(NULL);

    float dx = netLocation.x - pPlayer->m_v3Position.x;
    float dy = netLocation.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(distance, pFuzzyTweaks->vNearNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4CC8 | 0x80083750 | size: 0x7C
 */
float FarToTheirNet(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = pPlayer->GetAIOffNetLocation(NULL);

    float dx = netLocation.x - pPlayer->m_v3Position.x;
    float dy = netLocation.y - pPlayer->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(distance, pFuzzyTweaks->vFarNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x4934 | 0x800833BC | size: 0x394
 */
float Pressured(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    float fScore = 0.0f;
    int i = 0;
    while (i < 4)
    {
        cFielder* pOpponent = pFielder->m_pTeam->GetOtherTeam()->GetFielder(i);
        float fNearScore = NearTo(pFielder, pOpponent);
        if (!Incapacitated(pOpponent) && fNearScore >= 0.4f)
        {
            float fClosingWeight = 1.0f - g_pGame->m_pFuzzyTweaks->fPressuredNearWeight;
            float fClosingScore = ClosingTo(pFielder, pOpponent);
            fScore += (fNearScore * g_pGame->m_pFuzzyTweaks->fPressuredNearWeight) + (fClosingScore * fClosingWeight);
        }

        i++;
    }

    fScore *= 0.5f;
    float fResult = FMAX(fScore, 0.0f);
    return FMIN(fResult, 1.0f);
}

/**
 * Offset/Address/Size: 0x4490 | 0x80082F18 | size: 0x4A4
 */
float Attacked(cFielder* pFielder)
{
    if (!pFielder)
    {
        return 0.0f;
    }

    float fScore = 0.0f;

    for (int i = 0; i < 5; i++)
    {
        cPlayer* pOpponent = pFielder->m_pTeam->GetOtherTeam()->GetPlayer(i);

        unsigned char bAttackState = 0;
        if (pOpponent->m_eClassType == GOALIE)
        {
            bAttackState = ((Goalie*)pOpponent)->IsAttacking();
        }
        else if (pOpponent->m_eClassType == FIELDER)
        {
            bAttackState = ((cFielder*)pOpponent)->IsAttacking();
        }

        if (!bAttackState)
        {
            continue;
        }

        float fClosingScore = ClosingTo(pFielder, pOpponent);
        fClosingScore = FMIN(NearTo(pFielder, pOpponent), fClosingScore);
        fClosingScore = FMAX(CloseTo(pFielder, pOpponent), fClosingScore);
        float fAngleScore = Facing(pOpponent, pFielder);

        float fAngleWeight = 0.2f;
        float fClosingWeight = 0.8f;
        fScore += fAngleScore * fAngleWeight + fClosingScore * fClosingWeight;
    }

    float fResult = FMAX(fScore, 0.0f);
    if (fResult <= 1.0f)
    {
        return fResult;
    }

    return 1.0f;
}

static float AvoidingBowser(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3Repulsion = pFielder->m_pAvoidance->GetLastRepulsionVector(AVOID_BOWSER);
    float fScore = nlSqrt(v3Repulsion.x * v3Repulsion.x + v3Repulsion.y * v3Repulsion.y + v3Repulsion.z * v3Repulsion.z, true);
    return NormalizeVal(fScore, g_pGame->m_pFuzzyTweaks->vAvoidBowserRepulsionConfidence);
}

static float AvoidingGoalie(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3Repulsion = pFielder->m_pAvoidance->GetLastRepulsionVector(AVOID_GOALIES);
    float fScore = nlSqrt(v3Repulsion.x * v3Repulsion.x + v3Repulsion.y * v3Repulsion.y + v3Repulsion.z * v3Repulsion.z, true);
    return NormalizeVal(fScore, g_pGame->m_pFuzzyTweaks->vAvoidGoalieRepulsionConfidence);
}

static float AvoidingFielders(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3Repulsion = pFielder->m_pAvoidance->GetLastRepulsionVector(AVOID_FIELDERS);
    float fScore = nlSqrt(v3Repulsion.x * v3Repulsion.x + v3Repulsion.y * v3Repulsion.y + v3Repulsion.z * v3Repulsion.z, true);
    return NormalizeVal(fScore, g_pGame->m_pFuzzyTweaks->vAvoidFieldersRepulsionConfidence);
}

/**
 * Offset/Address/Size: 0x440C | 0x80082E94 | size: 0x84
 */
float AvoidingPowerups(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    nlVector3 repulsionVec = pFielder->m_pAvoidance->GetLastRepulsionVector(AVOID_POWERUPS);

    float magnitude = nlSqrt(repulsionVec.x * repulsionVec.x + repulsionVec.y * repulsionVec.y + repulsionVec.z * repulsionVec.z, true);

    return NormalizeVal(magnitude, g_pGame->m_pFuzzyTweaks->vAvoidPowerupsRepulsionConfidence);
}

static float AvoidingSidelines(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    if (pFielder->m_pAvoidance->m_CurrentlyAvoiding & AVOID_SIDELINES)
    {
        return 1.0f;
    }

    return 0.0f;
}

static float AvoidingStuff(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return FMAX(FMAX(FMAX(FMAX(AvoidingBowser(pFielder), AvoidingGoalie(pFielder)), AvoidingFielders(pFielder)), AvoidingPowerups(pFielder)), AvoidingSidelines(pFielder));
}

/**
 * Offset/Address/Size: 0x43C8 | 0x80082E50 | size: 0x44
 */
float Invincible(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    if (pFielder->IsInvincible())
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x4294 | 0x80082D1C | size: 0x134
 */
float Incapacitated(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    float fScore = 0.0f;

    if (pPlayer->m_eClassType == GOALIE)
    {
        Goalie* pGoalie = (Goalie*)pPlayer;
        fScore = (pGoalie->IsRecovering() || !pGoalie->IsBusy()) ? 1.0f : 0.0f;
    }
    else if (pPlayer->m_eClassType == FIELDER)
    {
        cFielder* pFielder = (cFielder*)pPlayer;
        fScore = pFielder->IsFrozen() || pFielder->IsFallenDown(25.0f) ? 1.0f : 0.0f;
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x4250 | 0x80082CD8 | size: 0x44
 */
float Frozen(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    if (pFielder->IsFrozen())
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x4208 | 0x80082C90 | size: 0x48
 */
float FallenDown(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    if (pFielder->IsFallenDown(0.0f))
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x3FA0 | 0x80082A28 | size: 0x268
 */
float LikelyToScoreFromPosition(const nlVector3& v3Position, const nlVector3& v3GoaliePosition, const cNet* pNet, bool bIsChipShot)
{
    float fNetHalfWidth;
    float fGoalLine;
    unsigned short aNetAngle;
    unsigned short aLowerPostAngle;
    unsigned short aUpperPostAngle;
    unsigned short aNetOpenAngle;
    unsigned short aVisibleGapAngle;
    float fNetOpenScore;
    float fGoalieDeltaY;
    float fGoalieDeltaX;

    float fSideSign = pNet->m_fDirection;
    float fOpenAngle = 65536.0f;
    fOpenAngle *= g_pGame->m_pGameTweaks->fShotMeterNetOpenAngle;
    fNetHalfWidth = 0.5f * cNet::m_fNetWidth;
    aNetAngle = ((s32)fOpenAngle) / 360;

    fGoalLine = pNet->GetGoalLineX();

    float fGoalX = fGoalLine - v3Position.x;
    float fGoalDeltaX = fGoalX * fSideSign;
    float fPositionY = v3Position.y;

    float fUpperAngle = nlATan2f(fNetHalfWidth - fPositionY, fGoalDeltaX);
    float fLowerAngle = nlATan2f(-fNetHalfWidth - fPositionY, fGoalDeltaX);

    aLowerPostAngle = (u16)(s32)(10430.378f * fLowerAngle);
    aUpperPostAngle = (u16)(s32)(10430.378f * fUpperAngle);
    aNetOpenAngle = aUpperPostAngle - aLowerPostAngle;

    fGoalieDeltaX = v3GoaliePosition.x - v3Position.x;
    fGoalieDeltaX *= fSideSign;
    fGoalieDeltaY = v3GoaliePosition.y - v3Position.y;

    float fGoalieCenterAngle = nlATan2f(fGoalieDeltaY, fGoalieDeltaX);
    float fGoalieHalfAngle = nlATan2f(0.5f, nlSqrt(fGoalieDeltaX * fGoalieDeltaX + fGoalieDeltaY * fGoalieDeltaY, true));

    u16 aGoalieHalfOpen = (u16)(s32)(10430.378f * fGoalieHalfAngle);
    u16 aGoalieCenter = (u16)(s32)(10430.378f * fGoalieCenterAngle);

    u16 aMinOpen = (aGoalieCenter - aGoalieHalfOpen) - aLowerPostAngle;
    u16 aMaxOpen = (aGoalieCenter + aGoalieHalfOpen) - aLowerPostAngle;

    if (aMinOpen < aNetOpenAngle && aMaxOpen < aNetOpenAngle)
    {
        u16 aGap = aNetOpenAngle - aMaxOpen;
        aVisibleGapAngle = aGap;
        if (aMinOpen >= aGap)
        {
            aVisibleGapAngle = aMinOpen;
        }
    }
    else if (aMinOpen < aNetOpenAngle)
    {
        aVisibleGapAngle = aMinOpen;
    }
    else if (aMaxOpen < aNetOpenAngle)
    {
        aVisibleGapAngle = aNetOpenAngle - aMaxOpen;
    }
    else if (aMinOpen > aMaxOpen && aMaxOpen > aNetOpenAngle)
    {
        aVisibleGapAngle = 0;
    }
    else
    {
        aVisibleGapAngle = aNetOpenAngle;
    }

    fNetOpenScore = InterpolateRangeClamped(0.0f, 1.0f, 0.0f, (float)aNetAngle, (float)aVisibleGapAngle);
    return fNetOpenScore;
}

/**
 * Offset/Address/Size: 0x3F28 | 0x800829B0 | size: 0x78
 */
float PlayerShotDistance(cFielder* pFielder)
{
    cNet* pOtherNet = pFielder->m_pTeam->GetOtherNet();

    nlVector3 v;
    nlVec3Set(v, pFielder->m_v3Position.x - pOtherNet->m_v3NetLocation.x, pFielder->m_v3Position.y - pOtherNet->m_v3NetLocation.y, pFielder->m_v3Position.z - pOtherNet->m_v3NetLocation.z);

    return NormalizeVal(nlSqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z), true), g_pGame->m_pFuzzyTweaks->vPlayerShotDistance);
}

/**
 * Offset/Address/Size: 0x3E9C | 0x80082924 | size: 0x8C
 */
float LikelyToScore(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    cTeam* pOtherTeam = pFielder->m_pTeam->GetOtherTeam();

    bool bIsChipShot = false;
    if ((pFielder->mActionShotVars.bIsChipShot != 0) || (pFielder->mActionLooseBallShotVars.bIsChipShot != 0))
    {
        bIsChipShot = true;
    }

    cNet* pNet = pOtherTeam->m_pNet;
    cPlayer* pGoalie = pOtherTeam->GetGoalie();

    return LikelyToScoreFromPosition(pFielder->m_v3Position, pGoalie->m_v3Position, pNet, bIsChipShot);
}

/**
 * Offset/Address/Size: 0x3D40 | 0x800827C8 | size: 0x15C
 */
float GoalieOutOfPosition(cFielder* pFielder)
{
    nlVector3 goalieNetPos;
    cPlayer* pGoalie;

    if (pFielder == NULL)
    {
        return 0.0f;
    }

    pGoalie = pFielder->m_pTeam->GetOtherTeam()->GetGoalie();
    float halfNetWidth = 0.5f * cNet::m_fNetWidth;
    goalieNetPos = pGoalie->m_v3Position;
    goalieNetPos.x = pGoalie->m_pTeam->m_pNet->m_v3NetLocation.x;

    float goalieY = goalieNetPos.y;
    if (goalieY < -halfNetWidth)
    {
        goalieNetPos.y = -halfNetWidth;
    }
    else if (goalieY > halfNetWidth)
    {
        goalieNetPos.y = halfNetWidth;
    }

    const nlVector3& offNetLocation = pFielder->GetAIOffNetLocation(NULL);

    float dx1 = pFielder->m_v3Position.x - offNetLocation.x;
    float dy1 = pFielder->m_v3Position.y - offNetLocation.y;
    float fielderDistance = nlSqrt(dx1 * dx1 + dy1 * dy1, true);

    float dx2 = pGoalie->m_v3Position.x - goalieNetPos.x;
    float dy2 = pGoalie->m_v3Position.y - goalieNetPos.y;
    float goalieDistance = nlSqrt(dx2 * dx2 + dy2 * dy2, true);

    if (!((double)fielderDistance > 0.0))
    {
        fielderDistance = 0.1f;
    }

    if (!((double)goalieDistance > 0.0))
    {
        goalieDistance = 0.1f;
    }

    return NormalizeVal(goalieDistance / fielderDistance, g_pGame->m_pFuzzyTweaks->vGoalieOutOfPositionDistance);
}

/**
 * Offset/Address/Size: 0x3C18 | 0x800826A0 | size: 0x128
 */
float PositionIsInFrontOfNet(const nlVector3& v3Position, const cNet* pNet)
{
    float sideSign;
    nlVector3 diff;
    nlVec3Set(diff,
        v3Position.x - pNet->m_v3NetLocation.x,
        v3Position.y - pNet->m_v3NetLocation.y,
        v3Position.z - pNet->m_v3NetLocation.z);
    sideSign = pNet->m_fDirection;
    nlVec3Scale(diff, diff, sideSign);

    nlPolar polar;
    nlCartesianToPolar(polar, diff);

    float angleRad = 0.005493164f * (u16)(polar.a - 0x8000);
    if (angleRad > 180.0f)
    {
        FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
        float complementaryMidAngle = 360.0f - pFuzzyTweaks->fFrontOfNetMidAngle;
        if (angleRad < complementaryMidAngle)
        {
            return InterpolateRangeClamped(0.0f, pFuzzyTweaks->fFrontOfNetMidScore, 360.0f - pFuzzyTweaks->fFrontOfNetMaxAngle, complementaryMidAngle, angleRad);
        }
        return InterpolateRangeClamped(1.0f, pFuzzyTweaks->fFrontOfNetMidScore, 360.0f, complementaryMidAngle, angleRad);
    }

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
    float midAngle = pFuzzyTweaks->fFrontOfNetMidAngle;
    if (angleRad > midAngle)
    {
        return InterpolateRangeClamped(0.0f, pFuzzyTweaks->fFrontOfNetMidScore, pFuzzyTweaks->fFrontOfNetMaxAngle, midAngle, angleRad);
    }
    return InterpolateRangeClamped(1.0f, pFuzzyTweaks->fFrontOfNetMidScore, 0.0f, midAngle, angleRad);
}

/**
 * Offset/Address/Size: 0x3ACC | 0x80082554 | size: 0x14C
 */
float InFrontOfTheirNet(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    cTeam* pOtherTeam = pFielder->m_pTeam->GetOtherTeam();
    cNet* pNet = pOtherTeam->m_pNet;

    nlVector3 diff;
    nlVec3Set(diff,
        pFielder->m_v3Position.x - pNet->m_v3NetLocation.x,
        pFielder->m_v3Position.y - pNet->m_v3NetLocation.y,
        pFielder->m_v3Position.z - pNet->m_v3NetLocation.z);

    float sideSign = pNet->m_fDirection;
    nlVec3Scale(diff, diff, sideSign);

    nlPolar polar;
    nlCartesianToPolar(polar, diff);

    float angleRad = 0.005493164f * (u16)(polar.a - 0x8000);

    if (angleRad > 180.0f)
    {
        FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
        float complementaryMidAngle = 360.0f - pFuzzyTweaks->fFrontOfNetMidAngle;
        if (angleRad < complementaryMidAngle)
        {
            return InterpolateRangeClamped(0.0f, pFuzzyTweaks->fFrontOfNetMidScore, 360.0f - pFuzzyTweaks->fFrontOfNetMaxAngle, complementaryMidAngle, angleRad);
        }
        return InterpolateRangeClamped(1.0f, pFuzzyTweaks->fFrontOfNetMidScore, 360.0f, complementaryMidAngle, angleRad);
    }

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
    float midAngle = pFuzzyTweaks->fFrontOfNetMidAngle;
    if (angleRad > midAngle)
    {
        return InterpolateRangeClamped(0.0f, pFuzzyTweaks->fFrontOfNetMidScore, pFuzzyTweaks->fFrontOfNetMaxAngle, midAngle, angleRad);
    }
    return InterpolateRangeClamped(1.0f, pFuzzyTweaks->fFrontOfNetMidScore, 0.0f, midAngle, angleRad);
}

static float InFrontOfMyNet(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    cNet* pNet = pFielder->m_pTeam->m_pNet;
    nlVector3 diff;
    nlVec3Set(diff,
        pFielder->m_v3Position.x - pNet->m_v3NetLocation.x,
        pFielder->m_v3Position.y - pNet->m_v3NetLocation.y,
        pFielder->m_v3Position.z - pNet->m_v3NetLocation.z);

    float sideSign = pNet->m_fDirection;
    nlVec3Scale(diff, diff, sideSign);

    nlPolar polar;
    nlCartesianToPolar(polar, diff);

    float angleRad = 0.005493164f * (u16)(polar.a - 0x8000);
    if (angleRad > 180.0f)
    {
        FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
        float complementaryMidAngle = 360.0f - pFuzzyTweaks->fFrontOfNetMidAngle;
        if (angleRad < complementaryMidAngle)
        {
            return InterpolateRangeClamped(0.0f, pFuzzyTweaks->fFrontOfNetMidScore, 360.0f - pFuzzyTweaks->fFrontOfNetMaxAngle, complementaryMidAngle, angleRad);
        }
        return InterpolateRangeClamped(1.0f, pFuzzyTweaks->fFrontOfNetMidScore, 360.0f, complementaryMidAngle, angleRad);
    }

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
    float midAngle = pFuzzyTweaks->fFrontOfNetMidAngle;
    if (angleRad > midAngle)
    {
        return InterpolateRangeClamped(0.0f, pFuzzyTweaks->fFrontOfNetMidScore, pFuzzyTweaks->fFrontOfNetMaxAngle, midAngle, angleRad);
    }
    return InterpolateRangeClamped(1.0f, pFuzzyTweaks->fFrontOfNetMidScore, 0.0f, midAngle, angleRad);
}

static float CloseToFormationPosition(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3FormationPos;
    pFielder->GetFormationPosition(v3FormationPos, -1.0f);
    return NormalizeVal(nlSqrt(pFielder->m_v3Position.CalculateDistanceSquared2D(v3FormationPos), true), g_pGame->m_pFuzzyTweaks->vCloseToFormationPositionDistance);
}

static float NearToFormationPosition(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3FormationPos;
    pFielder->GetFormationPosition(v3FormationPos, -1.0f);
    return NormalizeVal(nlSqrt(pFielder->m_v3Position.CalculateDistanceSquared2D(v3FormationPos), true), g_pGame->m_pFuzzyTweaks->vNearToFormationPositionDistance);
}

static float FarToFormationPosition(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3FormationPos;
    pFielder->GetFormationPosition(v3FormationPos, -1.0f);
    return NormalizeVal(nlSqrt(pFielder->m_v3Position.CalculateDistanceSquared2D(v3FormationPos), true), g_pGame->m_pFuzzyTweaks->vFarToFormationPositionDistance);
}

static float InBetween(const nlVector3& v3InBetweenPos, const nlVector3& v3A, const nlVector3& v3B);
static float InBetweenTheirNetAnd(cFielder* pFielder, cFielder* pOtherFielder);

/**
 * Offset/Address/Size: 0x36FC | 0x80082184 | size: 0x3D0
 */
float OnBreakaway(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }
    float fScore = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        cFielder* pOpponent = pFielder->m_pTeam->GetOtherTeam()->GetFielder(i);
        float fWeight = 1.0f;
        float fUpfieldScore = UpfieldFrom(pFielder, pOpponent);
        float fInterceptScore = InBetweenTheirNetAnd(pFielder, pOpponent);
        float fProximityScore = NearTo(pFielder, pOpponent);
        float fMaxScore = FMAX(fProximityScore, fInterceptScore);
        fScore += fWeight * (fWeight - fMaxScore) + fUpfieldScore * fMaxScore;
        fScore -= fInterceptScore;
    }

    return NormalizeVal(fScore, 0.0f, 4.0f);
}

/**
 * Offset/Address/Size: 0x32DC | 0x80081D64 | size: 0x420
 */
float OpenToPosition(const nlVector3& v3FromPos, const nlVector3& v3ToPos, const cTeam* pBlockingTeam, const cPlayer* pIgnorePlayer1, const cPlayer* pIgnorePlayer2, bool bIgnoreGoalies)
{
    float sqrDist = nlGetLengthSquared3D(v3ToPos.x - v3FromPos.x, v3ToPos.y - v3FromPos.y, v3ToPos.z - v3FromPos.z);
    float fClosedScore = 0.0f;
    float isZeroDist = (float)(fabsf(sqrDist - fClosedScore) <= 0.0001f);
    if (isZeroDist != fClosedScore)
    {
        return 1.0f;
    }
    int i_team = 0;
    do
    {
        cTeam* pTeam = g_pTeams[i_team];
        int iStartIndex = 0;
        int iEndIndex = 4;
        if (pBlockingTeam != NULL)
        {
            if (pBlockingTeam != pTeam)
            {
                iStartIndex = 4;
            }
        }
        if (bIgnoreGoalies)
        {
            iEndIndex = 3;
        }
        for (int i_player = iStartIndex; i_player <= iEndIndex; i_player++)
        {
            cPlayer* pBlocker = pTeam->GetPlayer(i_player);
            if (pBlocker == pIgnorePlayer1)
                continue;
            if (pBlocker == pIgnorePlayer2)
                continue;
            nlVector3 v3BetweenIntercept = GetClosestPointOnLineABFromPointC(v3FromPos, v3ToPos, pBlocker->m_v3Position);
            float pdy = pBlocker->m_v3Position.y - v3BetweenIntercept.y;
            float pdx = pBlocker->m_v3Position.x - v3BetweenIntercept.x;
            float fLaneDistance = nlSqrt(pdx * pdx + pdy * pdy, true);
            float fDist = fLaneDistance;
            if (fLaneDistance <= g_pGame->m_pFuzzyTweaks->vGetOpenPassLaneOffset.y)
            {
                bool isClosestFrom = (v3FromPos.x == v3BetweenIntercept.x) && (v3FromPos.y == v3BetweenIntercept.y) && (v3FromPos.z == v3BetweenIntercept.z);
                if (!isClosestFrom)
                {
                    bool isClosestTo = (v3ToPos.x == v3BetweenIntercept.x) && (v3ToPos.y == v3BetweenIntercept.y) && (v3ToPos.z == v3BetweenIntercept.z);
                    if (!isClosestTo)
                    {
                        float fOpenDist = g_pGame->m_pFuzzyTweaks->vGetOpenPassLaneDist.x;
                        if (pIgnorePlayer1 != NULL)
                        {
                            if (g_pScriptBallOwner == pIgnorePlayer1)
                            {
                                float fBallHeight = NormalizeVal(pIgnorePlayer1->GetJointPosition(pIgnorePlayer1->m_nBip01JointIndex_0xA4).z, 0.0f, 2.0f);
                                fOpenDist = Interpolate(fOpenDist, 3.5f, fBallHeight);
                            }
                        }
                        float ldx = v3FromPos.x - v3BetweenIntercept.x;
                        float ldy = v3FromPos.y - v3BetweenIntercept.y;
                        float fFromDist = nlSqrt(ldx * ldx + ldy * ldy, true);
                        fOpenDist = InterpolateRangeClamped(
                            g_pGame->m_pFuzzyTweaks->vGetOpenPassLaneOffset.x,
                            g_pGame->m_pFuzzyTweaks->vGetOpenPassLaneOffset.y,
                            fOpenDist,
                            g_pGame->m_pFuzzyTweaks->vGetOpenPassLaneDist.y,
                            fFromDist);
                        float sign = AIsgn(pBlocker->GetAIDefNetLocation(NULL).x);
                        float fDepthDelta = pBlocker->m_v3Position.x - v3ToPos.x;
                        float fDepth = fDepthDelta * sign;
                        if (fDepth < 0.0f)
                        {
                            float fDoDepthScale;
                            if (pBlocker->m_pTeam == NULL)
                            {
                                fDoDepthScale = 0.0f;
                            }
                            else if (pBlocker->m_pTeam->mpCurrentSituation == SITUATION_DEFENSE)
                            {
                                fDoDepthScale = 1.0f;
                            }
                            else
                            {
                                fDoDepthScale = 0.0f;
                            }
                            float fZero = 0.0f;
                            if (fDoDepthScale != fZero)
                            {
                                fDepth = NormalizeVal(-fDepth, 0.0f, 8.0f);
                                fDist *= fDepth;
                            }
                        }
                        float fNorm = FMIN(FMAX(fDist / fOpenDist, 0.0f), 1.0f);
                        float fOpen = 1.0f - fNorm;
                        if (pIgnorePlayer1 != NULL)
                        {
                            if (pIgnorePlayer1->m_pTeam == pBlocker->m_pTeam)
                                fOpen *= 0.5f;
                        }
                        if (fOpen > 0.0f)
                            fClosedScore += fOpen * fOpen;
                    }
                }
            }
            if (fClosedScore >= 1.0f)
                break;
        }
        if (fClosedScore >= 1.0f)
            break;
        i_team++;
    } while (i_team < 2);
    fClosedScore = FMAX(fClosedScore, 0.0f);
    fClosedScore = FMIN(fClosedScore, 1.0f);
    return 1.0f - fClosedScore;
}

/**
 * Offset/Address/Size: 0x3030 | 0x80081AB8 | size: 0x2AC
 */
float OpenPosition(const nlVector3& v3Position, cTeam* pOpponentTeam, cPlayer* pCurrentPlayer, const nlVector2* vOpenRadius)
{
    f32 fTotalScore;
    f32 fWeight;
    cTeam* pMyTeam;
    int i;

    fTotalScore = 0.0f;
    fWeight = 1.0f;

    if (vOpenRadius == NULL)
    {
        vOpenRadius = &g_pGame->m_pFuzzyTweaks->vOpenRadius;
    }

    pMyTeam = pOpponentTeam->GetOtherTeam();

    for (i = 0; i < 5; i++)
    {
        cPlayer* pPlayers[2] = {
            pOpponentTeam->GetPlayer(i),
            (pCurrentPlayer != NULL && i != pCurrentPlayer->m_ID) ? pMyTeam->GetPlayer(i) : NULL
        };

        for (int i_player = 0; i_player < 2; i_player++)
        {
            if (pPlayers[i_player] != NULL && !Incapacitated(pPlayers[i_player]))
            {
                float fScore = NormalizeVal(
                    nlSqrt(v3Position.CalculateDistanceSquared2D(pPlayers[i_player]->m_v3Position), true),
                    *vOpenRadius);
                if (fScore > 0.0f)
                {
                    fTotalScore += fWeight * fScore;
                    fWeight *= 0.5f;
                }
            }
        }
    }

    return FMIN(FMAX(1.0f - fTotalScore, 0.0f), 1.0f);
}

/**
 * Offset/Address/Size: 0x2D84 | 0x8008180C | size: 0x2AC
 */
float WideOpenPosition(const nlVector3& v3Position, cTeam* pOpponentTeam, cPlayer* pCurrentPlayer)
{
    return OpenPosition(v3Position, pOpponentTeam, pCurrentPlayer, &g_pGame->m_pFuzzyTweaks->vWideOpenRadius);
}

/**
 * Offset/Address/Size: 0x2B10 | 0x80081598 | size: 0x274
 */
float Open(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return OpenPosition(
        pFielder->m_v3Position,
        pFielder->m_pTeam->GetOtherTeam(),
        NULL,
        NULL);
}

/**
 * Offset/Address/Size: 0x2854 | 0x800812DC | size: 0x2BC
 */
float WideOpen(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return WideOpenPosition(
        pFielder->m_v3Position,
        pFielder->m_pTeam->GetOtherTeam(),
        pFielder);
}

/**
 * Offset/Address/Size: 0x27E0 | 0x80081268 | size: 0x74
 */
float OpenToTheirNet(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    cTeam* pOtherTeam = pFielder->m_pTeam->GetOtherTeam();

    return OpenToPosition(pFielder->m_v3Position, pFielder->GetAIOffNetLocation(NULL), pOtherTeam, pFielder, NULL, true);
}

/**
 * Offset/Address/Size: 0x2780 | 0x80081208 | size: 0x60
 */
float OpenToMyNet(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return OpenToPosition(pFielder->m_v3Position, pFielder->GetAIDefNetLocation(NULL), NULL, pFielder, NULL, true);
}

static float InBetween(const nlVector3& v3InBetweenPos, const nlVector3& v3A, const nlVector3& v3B)
{
    nlVector3 v3Intercept = GetClosestPointOnLineABFromPointC(v3A, v3B, v3InBetweenPos);

    bool bAtA = v3A.x == v3Intercept.x && v3A.y == v3Intercept.y && v3A.z == v3Intercept.z;
    bool bAtB;
    if (bAtA || (bAtB = v3B.x == v3Intercept.x && v3B.y == v3Intercept.y && v3B.z == v3Intercept.z))
    {
        return 0.0f;
    }

    float dx1 = v3A.x - v3Intercept.x;
    float dy1 = v3A.y - v3Intercept.y;
    float distA = nlSqrt(dx1 * dx1 + dy1 * dy1, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
    float fMaxConeWidth = InterpolateRangeClamped(pFuzzyTweaks->vInBetweenConeWidth, pFuzzyTweaks->vInBetweenInterceptRange, distA);

    float dx2 = v3Intercept.x - v3InBetweenPos.x;
    float dy2 = v3Intercept.y - v3InBetweenPos.y;

    return InterpolateRangeClamped(1.0f, 0.0f, 0.0f, fMaxConeWidth, nlSqrt(dx2 * dx2 + dy2 * dy2, true));
}

/**
 * Offset/Address/Size: 0x25DC | 0x80081064 | size: 0x1A4
 */
float InBetweenMyNetAnd(cFielder* pFielder, cFielder* pOtherFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }
    if (pOtherFielder == NULL)
    {
        return 0.0f;
    }

    return InBetween(pFielder->GetPosition(), pOtherFielder->GetPosition(), pFielder->m_pTeam->m_pNet->m_v3NetLocation);
}

static float InBetweenTheirNetAnd(cFielder* pFielder, cFielder* pOtherFielder)
{
    if (pOtherFielder == NULL)
    {
        return 0.0f;
    }
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return InBetween(
        pOtherFielder->GetPosition(),
        pFielder->GetPosition(),
        pOtherFielder->m_pTeam->m_pNet->m_v3NetLocation);
}

/**
 * Offset/Address/Size: 0x2438 | 0x80080EC0 | size: 0x1A4
 */
float InBetweenMyNetAnd(cFielder* pFielder, cBall* pBall)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }
    if (pBall == NULL)
    {
        return 0.0f;
    }

    return InBetween(
        pFielder->GetPosition(),
        pBall->GetPosition(),
        pFielder->m_pTeam->m_pNet->m_v3NetLocation);
}

static float InBetweenTheirNetAnd(cFielder* pFielder, cBall* pBall)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }
    if (pBall == NULL)
    {
        return 0.0f;
    }

    return InBetween(
        pFielder->m_pTeam->GetOtherNet()->m_v3NetLocation,
        pBall->GetPosition(),
        pFielder->GetPosition());
}

/**
 * Offset/Address/Size: 0x23D8 | 0x80080E60 | size: 0x60
 */
float OpenTo(cPlayer* pFromFielder, cPlayer* pToFielder)
{
    if (pFromFielder == NULL)
    {
        return 0.0f;
    }

    if (pToFielder == NULL)
    {
        return 0.0f;
    }

    float fResult = OpenToPosition(pFromFielder->m_v3Position, pToFielder->m_v3Position, g_pScriptOtherTeam, pFromFielder, pToFielder, false);
    return NormalizeVal(fResult, g_vOpenToAdjust);
}

/**
 * Offset/Address/Size: 0x2298 | 0x80080D20 | size: 0x140
 */
float CloseTo(cPlayer* pPlayer1, cPlayer* pPlayer2)
{
    if (pPlayer1 == NULL)
    {
        return 0.0f;
    }

    if (pPlayer2 == NULL)
    {
        return 0.0f;
    }

    float fScore;
    if (pPlayer1->m_eClassType == GOALIE)
    {
        fScore = CloseToGoaliePosition(pPlayer2->GetPosition(), pPlayer1->GetPosition());
    }
    else if (pPlayer2->m_eClassType == GOALIE)
    {
        fScore = CloseToGoaliePosition(pPlayer1->GetPosition(), pPlayer2->GetPosition());
    }
    else
    {
        const nlVector2& vDistanceConfidence = pPlayer1->IsOnSameTeam(pPlayer2) ? g_pGame->m_pFuzzyTweaks->vCloseTeammateConfidenceDistance : g_pGame->m_pFuzzyTweaks->vCloseOpponentConfidenceDistance;
        fScore = nlSqrt(pPlayer1->GetPosition().CalculateDistanceSquared2D(pPlayer2->GetPosition()), true);
        fScore = NormalizeVal(fScore, vDistanceConfidence);
    }
    return fScore;
}

/**
 * Offset/Address/Size: 0x2158 | 0x80080BE0 | size: 0x140
 */
float NearTo(cPlayer* pPlayer1, cPlayer* pPlayer2)
{
    if (pPlayer1 == NULL)
    {
        return 0.0f;
    }

    if (pPlayer2 == NULL)
    {
        return 0.0f;
    }

    float fScore;
    if (pPlayer1->m_eClassType == GOALIE)
    {
        fScore = NearToGoaliePosition(pPlayer2->GetPosition(), pPlayer1->GetPosition());
    }
    else if (pPlayer2->m_eClassType == GOALIE)
    {
        fScore = NearToGoaliePosition(pPlayer1->GetPosition(), pPlayer2->GetPosition());
    }
    else
    {
        const nlVector2& vDistanceConfidence = pPlayer1->IsOnSameTeam(pPlayer2) ? g_pGame->m_pFuzzyTweaks->vNearTeammateConfidenceDistance : g_pGame->m_pFuzzyTweaks->vNearOpponentConfidenceDistance;
        fScore = nlSqrt(pPlayer1->GetPosition().CalculateDistanceSquared2D(pPlayer2->GetPosition()), true);
        fScore = NormalizeVal(fScore, vDistanceConfidence);
    }
    return fScore;
}

/**
 * Offset/Address/Size: 0x2018 | 0x80080AA0 | size: 0x140
 */
float FarTo(cPlayer* pPlayer1, cPlayer* pPlayer2)
{
    if (pPlayer1 == NULL)
    {
        return 0.0f;
    }

    if (pPlayer2 == NULL)
    {
        return 0.0f;
    }

    float fScore;
    if (pPlayer1->m_eClassType == GOALIE)
    {
        fScore = FarToGoaliePosition(pPlayer2->GetPosition(), pPlayer1->GetPosition());
    }
    else if (pPlayer2->m_eClassType == GOALIE)
    {
        fScore = FarToGoaliePosition(pPlayer1->GetPosition(), pPlayer2->GetPosition());
    }
    else
    {
        const nlVector2& vDistanceConfidence = pPlayer1->IsOnSameTeam(pPlayer2) ? g_pGame->m_pFuzzyTweaks->vFarTeammateConfidenceDistance : g_pGame->m_pFuzzyTweaks->vFarOpponentConfidenceDistance;
        fScore = nlSqrt(pPlayer1->GetPosition().CalculateDistanceSquared2D(pPlayer2->GetPosition()), true);
        fScore = NormalizeVal(fScore, vDistanceConfidence);
    }
    return fScore;
}

static float CloseToGoaliePosition(const nlVector3& v3FromPos, const nlVector3& v3GoaliePos)
{
    float dx = v3FromPos.x - v3GoaliePos.x;
    float dy = v3FromPos.y - v3GoaliePos.y;

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
    float fScore = NormalizeVal(nlSqrt(dx * dx + dy * dy, true), pFuzzyTweaks->vCloseGoalieConfidenceDistance);
    return fScore;
}

/**
 * Offset/Address/Size: 0x1FBC | 0x80080A44 | size: 0x5C
 */
float NearToGoaliePosition(const nlVector3& v3FromPos, const nlVector3& v3GoaliePos)
{
    float dx = v3FromPos.x - v3GoaliePos.x;
    float dy = v3FromPos.y - v3GoaliePos.y;

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), pFuzzyTweaks->vNearGoalieConfidenceDistance);
}

static float FarToGoaliePosition(const nlVector3& v3FromPos, const nlVector3& v3GoaliePos)
{
    float dx = v3FromPos.x - v3GoaliePos.x;
    float dy = v3FromPos.y - v3GoaliePos.y;

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
    float fScore = NormalizeVal(nlSqrt(dx * dx + dy * dy, true), pFuzzyTweaks->vFarGoalieConfidenceDistance);
    return fScore;
}

static float CloseToMyGoalie(cPlayer* pPlayer)
{
    return CloseToGoaliePosition(pPlayer->m_v3Position, pPlayer->m_pTeam->GetGoalie()->m_v3Position);
}

static float NearToMyGoalie(cPlayer* pPlayer)
{
    return NearToGoaliePosition(pPlayer->m_v3Position, pPlayer->m_pTeam->GetGoalie()->m_v3Position);
}

static float FarToMyGoalie(cPlayer* pPlayer)
{
    return FarToGoaliePosition(pPlayer->m_v3Position, pPlayer->m_pTeam->GetGoalie()->m_v3Position);
}

/**
 * Offset/Address/Size: 0x1F50 | 0x800809D8 | size: 0x6C
 */
float CloseToTheirGoalie(cPlayer* pPlayer)
{
    cTeam* pOtherTeam = pPlayer->m_pTeam->GetOtherTeam();
    cPlayer* pGoalie = pOtherTeam->GetGoalie();

    float dx = pPlayer->m_v3Position.x - pGoalie->m_v3Position.x;
    float dy = pPlayer->m_v3Position.y - pGoalie->m_v3Position.y;

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), pFuzzyTweaks->vCloseGoalieConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x1EE4 | 0x8008096C | size: 0x6C
 */
float NearToTheirGoalie(cPlayer* pPlayer)
{
    cTeam* pOtherTeam = pPlayer->m_pTeam->GetOtherTeam();
    cPlayer* pGoalie = pOtherTeam->GetGoalie();

    float dx = pPlayer->m_v3Position.x - pGoalie->m_v3Position.x;
    float dy = pPlayer->m_v3Position.y - pGoalie->m_v3Position.y;

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), pFuzzyTweaks->vNearGoalieConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x1E78 | 0x80080900 | size: 0x6C
 */
float FarToTheirGoalie(cPlayer* pPlayer)
{
    cTeam* pOtherTeam = pPlayer->m_pTeam->GetOtherTeam();
    cPlayer* pGoalie = pOtherTeam->GetGoalie();

    float dx = pPlayer->m_v3Position.x - pGoalie->m_v3Position.x;
    float dy = pPlayer->m_v3Position.y - pGoalie->m_v3Position.y;

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), pFuzzyTweaks->vFarGoalieConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x1D08 | 0x80080790 | size: 0x170
 */
float CloseToSideline(const nlVector3& v3Position, const nlVector2* vDistanceConfidence, bool bInvert)
{
    if (vDistanceConfidence == NULL)
    {
        vDistanceConfidence = &g_pGame->m_pFuzzyTweaks->vCloseToSidelineDistanceConfidence;
    }

    float fScore;
    if (bInvert)
    {
        fScore = 1.0f;
    }
    else
    {
        fScore = 0.0f;
    }

    int i = 0;
    f32 fZero = 0.0f;

    for (; i < 4; i++)
    {
        const sSideLinePlane& sideline = cField::GetSideline(i);
        nlVector3 v3SidelinePos = v3Position;
        v3SidelinePos.z = fZero;

        if (fZero == sideline.vNormal.x)
        {
            v3SidelinePos.y = sideline.fDistance * sideline.vNormal.y;
        }
        else
        {
            v3SidelinePos.x = sideline.fDistance * sideline.vNormal.x;
        }

        float fDistance = nlSqrt(v3SidelinePos.CalculateDistanceSquared2D(v3Position), true);

        if (bInvert)
        {
            float fNormalized = NormalizeVal(fDistance, *vDistanceConfidence);
            fScore = (fScore <= fNormalized) ? fScore : fNormalized;
        }
        else
        {
            float fNormalized = NormalizeVal(fDistance, *vDistanceConfidence);
            fScore = (fScore >= fNormalized) ? fScore : fNormalized;
        }
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x1BD4 | 0x8008065C | size: 0x134
 */
float NearToSideline(const nlVector3& v3Position)
{
    return CloseToSideline(v3Position, &g_pGame->m_pFuzzyTweaks->vNearToSidelineDistanceConfidence, false);
}

static float FarFromSideline(const nlVector3& v3Position)
{
    return CloseToSideline(v3Position, &g_pGame->m_pFuzzyTweaks->vFarFromSidelineDistanceConfidence, false);
}

/**
 * Offset/Address/Size: 0x1AB8 | 0x80080540 | size: 0x11C
 */
float CloseToSideline(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return CloseToSideline(pFielder->m_v3Position, NULL, false);
}

static float NearToSideline(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return CloseToSideline(pFielder->m_v3Position, &g_pGame->m_pFuzzyTweaks->vNearToSidelineDistanceConfidence, false);
}

static float FarFromSideline(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return CloseToSideline(pFielder->m_v3Position, &g_pGame->m_pFuzzyTweaks->vFarFromSidelineDistanceConfidence, false);
}

/**
 * Offset/Address/Size: 0x1A18 | 0x800804A0 | size: 0xA0
 */
float FacingSideline(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    AvoidController* pAvoidance = pFielder->m_pAvoidance;
    float fScore = 0.0f;

    nlVector2* pSidelineNormal;
    if (pAvoidance->m_VeryCloseToSideline)
    {
        pSidelineNormal = &pAvoidance->m_SidelineNormal;
    }
    else
    {
        pSidelineNormal = NULL;
    }

    if (pSidelineNormal != NULL)
    {
        nlVector2 v2FacingDirection;
        nlSinCos(&v2FacingDirection.y, &v2FacingDirection.x, pFielder->m_aActualFacingDirection);

        float dp = (pSidelineNormal->x * v2FacingDirection.x) + (pSidelineNormal->y * v2FacingDirection.y);
        fScore = -dp;

        if (0.0f >= fScore)
        {
            fScore = 0.0f;
        }
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x187C | 0x80080304 | size: 0x19C
 */
float StuckOnSidelines(cFielder* pFielder)
{
    if (!pFielder)
    {
        return 0.0f;
    }

    bool sidelineUnavoidable = pFielder->m_pAvoidance->m_SidelineUnavoidable;
    float sidelineRepulsion = AvoidingFielders(pFielder);
    float powerupRepulsion = AvoidingPowerups(pFielder);
    float flagVal = sidelineUnavoidable ? 1.0f : 0.0f;

    powerupRepulsion = FMAX(powerupRepulsion, sidelineRepulsion);
    flagVal = FMAX(flagVal, powerupRepulsion);

    return FMIN(AvoidingSidelines(pFielder), flagVal);
}

/**
 * Offset/Address/Size: 0x17E8 | 0x80080270 | size: 0x94
 */
float AtIdealDistanceForTackling(cPlayer* pFielder1, cPlayer* pFielder2)
{
    if (pFielder1 == NULL)
    {
        return 0.0f;
    }

    if (pFielder2 == NULL)
    {
        return 0.0f;
    }

    float dx = pFielder1->m_v3Position.x - pFielder2->m_v3Position.x;
    float dy = pFielder1->m_v3Position.y - pFielder2->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    float idealDistance = pFuzzyTweaks->vIdealTacklingDistance.x;
    if (distance < idealDistance)
    {
        return NormalizeVal(distance, idealDistance - pFuzzyTweaks->vIdealTacklingDistance.y, idealDistance);
    }
    return NormalizeVal(distance, idealDistance + pFuzzyTweaks->vIdealTacklingDistance.y, idealDistance);
}

/**
 * Offset/Address/Size: 0x1774 | 0x800801FC | size: 0x74
 */
float PositionIsAtIdealDistanceForShooting(const nlVector3& vPosition, const nlVector3& vOffNetPosition)
{
    float dx = vPosition.x - vOffNetPosition.x;
    float dy = vPosition.y - vOffNetPosition.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

    float idealDistance = pFuzzyTweaks->vIdealDistanceForShooting.x;
    if (distance < idealDistance)
    {
        return NormalizeVal(distance, idealDistance - pFuzzyTweaks->vIdealDistanceForShooting.y, idealDistance);
    }
    return NormalizeVal(distance, idealDistance + pFuzzyTweaks->vIdealDistanceForShooting.y, idealDistance);
}

static float AtIdealDistanceForShooting(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    return PositionIsAtIdealDistanceForShooting(pFielder->m_v3Position, pFielder->GetAIOffNetLocation(NULL));
}

static float Facing(unsigned short facingAngle, const nlVector3& direction)
{
    nlPolar p;
    nlCartesianToPolar(p, direction);

    s16 nFacingDelta = (s16)(facingAngle - p.a);
    nFacingDelta = (u16)abs_s16(nFacingDelta);

    if (nFacingDelta < g_pGame->m_pFuzzyTweaks->nFacingFullConfidenceAngle)
    {
        return 1.0f;
    }

    if (nFacingDelta > g_pGame->m_pFuzzyTweaks->nFacingNoConfidenceAngle)
    {
        return 0.0f;
    }

    return 1.0f - ((float)(nFacingDelta - g_pGame->m_pFuzzyTweaks->nFacingFullConfidenceAngle) / (float)(g_pGame->m_pFuzzyTweaks->nFacingNoConfidenceAngle - g_pGame->m_pFuzzyTweaks->nFacingFullConfidenceAngle));
}

/**
 * Offset/Address/Size: 0x1660 | 0x800800E8 | size: 0x114
 */
float Facing(cPlayer* pCandidateFielder, cPlayer* pTargetFielder)
{
    nlVector3 v3Direction;

    if (pCandidateFielder == NULL)
    {
        return 0.0f;
    }

    if (pTargetFielder == NULL)
    {
        return 0.0f;
    }

    nlVec3Sub(v3Direction, pTargetFielder->GetPosition(), pCandidateFielder->GetPosition());
    return Facing(pCandidateFielder->GetActualFacing(), v3Direction);
}

static float FaceToFace(cPlayer* pCandidateFielder, cPlayer* pTargetFielder)
{
    if (pCandidateFielder == NULL)
    {
        return 0.0f;
    }

    if (pTargetFielder == NULL)
    {
        return 0.0f;
    }

    float fScore = Facing(pCandidateFielder, pTargetFielder);
    fScore += Facing(pTargetFielder, pCandidateFielder);
    fScore *= 0.5f;
    return fScore;
}

/**
 * Offset/Address/Size: 0x15C0 | 0x80080048 | size: 0xA0
 */
float UpfieldFrom(cPlayer* pUpfieldPlayer, cPlayer* pFromPlayer)
{
    if (pUpfieldPlayer == NULL)
    {
        return 0.0f;
    }

    if (pFromPlayer == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3UpfieldPos = pUpfieldPlayer->m_v3Position;
    nlVector3 v3FromPos = pFromPlayer->m_v3Position;
    float fDelta = (v3UpfieldPos.x - v3FromPos.x) * AIsgn(pUpfieldPlayer->m_pTeam->GetOtherNet()->m_v3NetLocation.x);
    float fScore = NormalizeVal(fDelta, 0.0f, g_pGame->m_pFuzzyTweaks->fUpfieldMaxDistance);
    return fScore;
}

/**
 * Offset/Address/Size: 0x1520 | 0x8007FFA8 | size: 0xA0
 */
float DownfieldFrom(cPlayer* pDownfieldPlayer, cPlayer* pFromPlayer)
{
    if (pDownfieldPlayer == NULL)
    {
        return 0.0f;
    }

    if (pFromPlayer == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3FromPos = pDownfieldPlayer->m_v3Position;
    nlVector3 v3DownfieldPos = pFromPlayer->m_v3Position;
    float delta = v3DownfieldPos.x - v3FromPos.x;
    float score = delta * AIsgn(pDownfieldPlayer->m_pTeam->GetOtherNet()->m_v3NetLocation.x);

    return NormalizeVal(score, 0.0f, g_pGame->m_pFuzzyTweaks->fDownfieldMaxDistance);
}

/**
 * Offset/Address/Size: 0x14B8 | 0x8007FF40 | size: 0x68
 */
float ClosingTo(cPlayer* pFielder1, cPlayer* pFielder2)
{
    float fScore = 0.0f;
    if (pFielder1 == NULL)
    {
        return fScore;
    }

    if (pFielder2 == NULL)
    {
        return fScore;
    }

    float fClosingSpeed = GetClosingSpeed2D(
        pFielder1->GetPosition(),
        pFielder1->GetVelocity(),
        pFielder2->GetPosition(),
        pFielder2->GetVelocity());
    fScore = NormalizeVal(fClosingSpeed, 0.0f, g_pGame->m_pFuzzyTweaks->fClosingSpeedMax);

    return fScore;
}

/**
 * Offset/Address/Size: 0x1450 | 0x8007FED8 | size: 0x68
 */
float ClosingTo(cPlayer* pPlayer, cBall* pBall)
{
    float fScore = 0.0f;
    if (pPlayer == NULL)
    {
        return fScore;
    }

    if (pBall == NULL)
    {
        return fScore;
    }

    float fClosingSpeed = GetClosingSpeed2D(
        pPlayer->GetPosition(),
        pPlayer->GetVelocity(),
        pBall->m_v3Position,
        pBall->m_v3Velocity);
    fScore = NormalizeVal(fClosingSpeed, 0.0f, g_pGame->m_pFuzzyTweaks->fClosingSpeedMax);

    return fScore;
}

/**
 * Offset/Address/Size: 0x13E4 | 0x8007FE6C | size: 0x6C
 */
float SeparatingFrom(cPlayer* pFielder1, cPlayer* pFielder2)
{
    if (pFielder1 == NULL)
    {
        return 0.0f;
    }

    if (pFielder2 == NULL)
    {
        return 0.0f;
    }

    float closingSpeed = GetClosingSpeed2D(
        pFielder1->m_v3Position,
        pFielder1->m_v3Velocity,
        pFielder2->m_v3Position,
        pFielder2->m_v3Velocity);

    float separatingSpeedMax = g_pGame->m_pFuzzyTweaks->fSeparatingSpeedMax;

    return NormalizeVal(closingSpeed, 0.0f, -separatingSpeedMax);
}

static float SeparatingFrom(cPlayer* pPlayer, cBall* pBall)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    if (pBall == NULL)
    {
        return 0.0f;
    }

    float fClosingSpeed = GetClosingSpeed2D(
        pPlayer->m_v3Position,
        pPlayer->m_v3Velocity,
        pBall->m_v3Position,
        pBall->m_v3Velocity);

    float fScore = NormalizeVal(fClosingSpeed, 0.0f, -g_pGame->m_pFuzzyTweaks->fSeparatingSpeedMax);
    return fScore;
}

/**
 * Offset/Address/Size: 0x1350 | 0x8007FDD8 | size: 0x94
 */
float OutOfNet(Goalie* pGoalie)
{
    if (pGoalie == NULL)
    {
        return 0.0f;
    }

    nlVector3 netLocation = pGoalie->GetAIDefNetLocation(NULL);

    float dx = netLocation.x - pGoalie->m_v3Position.x;
    float dy = netLocation.y - pGoalie->m_v3Position.y;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), g_pGame->m_pFuzzyTweaks->vOutOfNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x1324 | 0x8007FDAC | size: 0x2C
 */
float Stunned(Goalie* pGoalie)
{
    if (pGoalie == NULL)
    {
        return 0.0f;
    }

    if (pGoalie->mGoalieActionState == GOALIEACTION_STS_RECOVER)
    {
        return 1.0f;
    }

    return 0.0f;
}

float CloseToTheirNetB(cBall* ball)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = g_pScriptCurrentFielder->GetAIOffNetLocation(&ball->m_v3Position);

    float dx = ball->m_v3Position.x - netLocation.x;
    float dy = ball->m_v3Position.y - netLocation.y;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), g_pGame->m_pFuzzyTweaks->vCloseBallNetConfidenceDistance);
}

float NearToTheirNetB(cBall* ball)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = g_pScriptCurrentFielder->GetAIOffNetLocation(&ball->m_v3Position);

    float dx = ball->m_v3Position.x - netLocation.x;
    float dy = ball->m_v3Position.y - netLocation.y;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), g_pGame->m_pFuzzyTweaks->vNearBallNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x12A8 | 0x8007FD30 | size: 0x7C
 */
float FarToTheirNetB(cBall* ball)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = g_pScriptCurrentFielder->GetAIOffNetLocation(&ball->m_v3Position);

    float dx = ball->m_v3Position.x - netLocation.x;
    float dy = ball->m_v3Position.y - netLocation.y;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), g_pGame->m_pFuzzyTweaks->vFarBallNetConfidenceDistance);
}

float CloseToPlayersNet(cBall* ball, cPlayer* player)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    if (player == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = player->GetAIDefNetLocation(&ball->m_v3Position);

    float dx = ball->m_v3Position.x - netLocation.x;
    float dy = ball->m_v3Position.y - netLocation.y;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), g_pGame->m_pFuzzyTweaks->vCloseBallNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x1220 | 0x8007FCA8 | size: 0x88
 */
float NearToPlayersNet(cBall* ball, cPlayer* player)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    if (player == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = player->GetAIDefNetLocation(&ball->m_v3Position);

    float dx = ball->m_v3Position.x - netLocation.x;
    float dy = ball->m_v3Position.y - netLocation.y;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), g_pGame->m_pFuzzyTweaks->vNearBallNetConfidenceDistance);
}

float FarToPlayersNet(cBall* ball, cPlayer* player)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    if (player == NULL)
    {
        return 0.0f;
    }

    const nlVector3& netLocation = player->GetAIDefNetLocation(&ball->m_v3Position);

    float dx = ball->m_v3Position.x - netLocation.x;
    float dy = ball->m_v3Position.y - netLocation.y;

    return NormalizeVal(nlSqrt(dx * dx + dy * dy, true), g_pGame->m_pFuzzyTweaks->vFarBallNetConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x11A4 | 0x8007FC2C | size: 0x7C
 */
float InControlOfBall(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    if (fielder != g_pScriptBall->m_pOwner)
    {
        return 0.0f;
    }

    float dx = g_pScriptBall->m_v3Position.x - fielder->m_v3Position.x;
    float dy = g_pScriptBall->m_v3Position.y - fielder->m_v3Position.y;
    float distance = nlSqrt(dx * dx + dy * dy, true);

    return NormalizeVal(distance, g_pGame->m_pFuzzyTweaks->vControlConfidenceDistance);
}

/**
 * Offset/Address/Size: 0x1118 | 0x8007FBA0 | size: 0x8C
 */
float WindingUpForShot(cFielder* fielder)
{
    if (fielder == NULL)
    {
        return 0.0f;
    }

    eShotMeterState shotMeterState = fielder->m_pShotMeter->m_eShotMeterState;
    bool bShotMeterActive = shotMeterState == SHOT_METER_ACTIVE || shotMeterState == SHOT_METER_STS_ACTIVE || shotMeterState == SHOT_METER_STS_TRANSISTION;
    if (bShotMeterActive || fielder->IsPreparingForOneTimer() || fielder->m_eActionState == ACTION_SHOOT_TO_SCORE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x10FC | 0x8007FB84 | size: 0x1C
 */
float OnMushrooms(cFielder* fielder)
{
    if (fielder->m_ePowerup == POWER_UP_MUSHROOM)
    {
        return 1.0f;
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0xFF4 | 0x8007FA7C | size: 0x108
 */
float ChasingBall(cPlayer* pPlayer)
{
    float fScore;

    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    fScore = 0.0f;
    cFielder* pFielder = (cFielder*)pPlayer;

    if (pPlayer->m_eClassType != GOALIE && pPlayer->m_eClassType == FIELDER)
    {
        if (pFielder->m_eFielderDesireState == FIELDERDESIRE_INTERCEPT_BALL)
        {
            return 1.0f;
        }
        else if (pFielder->m_eFielderDesireState == FIELDERDESIRE_USER_CONTROLLED)
        {
            fScore = ClosestToBallAmongstMyTeam(pFielder);
        }
        else if (pFielder->m_eFielderDesireState == FIELDERDESIRE_MARK)
        {
            if (pFielder->GetMark() != NULL && pFielder->GetMark()->HasBall())
            {
                fScore = 0.5f;
            }
        }
    }

    return fScore;
}

static float DoingS2S(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    if (pFielder->m_eActionState == ACTION_SHOOT_TO_SCORE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0xFBC | 0x8007FA44 | size: 0x38
 */
float ReceivingPass(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    eFielderDesireState desireState = pFielder->m_eFielderDesireState;
    float fScore = 0.0f;
    if (((s32)desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE || (desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_RUN) || desireState == FIELDERDESIRE_ONETIMER))
    {
        fScore = 1.0f;
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0xF5C | 0x8007F9E4 | size: 0x60
 */
float ReceivingVolleyPass(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    float fScore = 0.0f;
    if (pPlayer->m_eClassType == FIELDER)
    {
        if (((cFielder*)pPlayer)->IsReceivingVolleyPass())
        {
            fScore = 1.0f;
        }
    }

    return fScore;
}

static float ReceivingLowPass(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    float fScore = 0.0f;
    eFielderDesireState desireState = pFielder->m_eFielderDesireState;
    if (desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE || desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_RUN || desireState == FIELDERDESIRE_ONETIMER)
    {
        fScore = 1.0f;
        if (pFielder->IsReceivingVolleyPass())
        {
            fScore = 0.0f;
        }
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0xE80 | 0x8007F908 | size: 0xDC
 */
float ReceivingPassDelayed(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    eFielderDesireState desireState = pFielder->m_eFielderDesireState;
    float fScore = 0.0f;
    if (((s32)desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE || (desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_RUN) || desireState == FIELDERDESIRE_ONETIMER))
    {
        FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;
        SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);

        float fDeadZone = pFuzzyTweaks->fPassDeadZone * (1.0f - pSkillTweaks->Off_Reaction);
        float fMaxValue = (0.1f >= fDeadZone) ? 0.1f : fDeadZone;

        float fProgress = IsPassInPlay(g_pBall);

        fScore = NormalizeVal(fProgress, 0.0f, fMaxValue);
    }
    return fScore;
}

/**
 * Offset/Address/Size: 0xDF8 | 0x8007F880 | size: 0x88
 */
float PassReceiveCloseToDone(cFielder* pFielder)
{
    float fScore;
    if (pFielder == NULL)
    {
        return 0.0f;
    }

    eFielderDesireState desireState = pFielder->m_eFielderDesireState;
    fScore = 0.0f;

    if (((s32)desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE || (desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_RUN) || desireState == FIELDERDESIRE_ONETIMER))
    {
        float fProgress = IsPassInPlay(g_pBall);
        fScore = NormalizeVal(fProgress, g_vPassCloseToDoneConfidence);
    }
    return fScore;
}

/**
 * Offset/Address/Size: 0xD18 | 0x8007F7A0 | size: 0xE0
 */
float ReceivingVolleyPassDelayed(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    float fScore = 0.0f;
    if (pPlayer->m_eClassType == FIELDER && ((cFielder*)pPlayer)->IsReceivingVolleyPass())
    {
        FuzzyTweaks* pFuzzyTweaks = g_pGame->m_pFuzzyTweaks;

        float fMaxValue;

        SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
        float fDeadZone = pFuzzyTweaks->fPassDeadZone * (1.0f - pSkillTweaks->Off_Reaction);

        fMaxValue = (0.1f >= fDeadZone) ? 0.1f : fDeadZone;

        float fProgress = IsPassInPlay(g_pBall);

        fScore = NormalizeVal(fProgress, 0.0f, fMaxValue);
    }
    return fScore;
}

static float ReceivingLowPassDelayed(cFielder* pFielder)
{
    float fScore = ReceivingLowPass(pFielder);
    if (fScore != 0.0f)
    {
        fScore = NormalizeVal(IsPassInPlay(g_pBall), 0.0f, g_pGame->m_pFuzzyTweaks->fPassDeadZone * (1.0f - SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Off_Reaction));
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0xCD4 | 0x8007F75C | size: 0x44
 */
float High(cBall* ball)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    return NormalizeVal(g_pScriptBall->m_v3Position.z, g_pGame->m_pFuzzyTweaks->vHighBallConfidenceDistance);
}

static float ReallyHigh(cBall* ball)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    return NormalizeVal(g_pScriptBall->m_v3Position.z, g_pGame->m_pFuzzyTweaks->vReallyHighBallConfidenceDistance);
}

/**
 * Offset/Address/Size: 0xCA8 | 0x8007F730 | size: 0x2C
 */
float Ownerless(cBall* ball)
{
    if (ball == NULL)
    {
        return 0.0f;
    }

    if (ball->m_pOwner == NULL)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0xC7C | 0x8007F704 | size: 0x2C
 */
float AggressiveT(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    if (team->meCurrentTeamStyle == TEAM_STYLE_AGGRESSIVE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0xC50 | 0x8007F6D8 | size: 0x2C
 */
float Moderate(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    if (team->meCurrentTeamStyle == TEAM_STYLE_MODERATE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0xC24 | 0x8007F6AC | size: 0x2C
 */
float Passive(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    if (team->meCurrentTeamStyle == TEAM_STYLE_PASSIVE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0xBD8 | 0x8007F660 | size: 0x4C
 */
float UserControlledT(cTeam* team)
{
    if (!team)
    {
        return 0.0f;
    }

    if (team->GetNumAssignedControllers() > 0)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x744 | 0x8007F1CC | size: 0x494
 */
float GonnaGetBall(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    cFielder* players[2];
    players[0] = team->m_pBallInterceptOrderedFielders[0];
    players[1] = team->GetOtherTeam()->m_pBallInterceptOrderedFielders[0];
    float score[2] = {
        FMAX(BallOwner(players[0]), FMIN(ChasingBall(players[0]), (NearToBall(players[0]) + (AbleToInterceptBall(players[0]) + ClosingTo(players[0], g_pBall))) / 3.0f)),
        FMAX(BallOwner(players[1]), FMIN(ChasingBall(players[1]), (NearToBall(players[1]) + (AbleToInterceptBall(players[1]) + ClosingTo(players[1], g_pBall))) / 3.0f))
    };
    float fScore = 0.0f;
    float total_score;

    total_score = score[0] + score[1];
    if (total_score > 0.0f)
    {
        fScore = score[0] / total_score;
    }

    return fScore;
}

/**
 * Offset/Address/Size: 0x6CC | 0x8007F154 | size: 0x78
 */
float Losing(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    cTeam* pOtherTeam = team->GetOtherTeam();
    int scoreDiff = team->m_nScore - pOtherTeam->m_nScore;

    return NormalizeVal((float)scoreDiff, 0.0f, -g_pGame->m_pFuzzyTweaks->fLosingScoreDelta);
}

/**
 * Offset/Address/Size: 0x64C | 0x8007F0D4 | size: 0x80
 */
float Tied(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    cTeam* pOtherTeam = team->GetOtherTeam();
    int scoreDiff = team->m_nScore - pOtherTeam->m_nScore;
    int absScoreDiff = (scoreDiff < 0) ? -scoreDiff : scoreDiff;

    return NormalizeVal((float)absScoreDiff, g_pGame->m_pFuzzyTweaks->fTiedScoreDelta, 0.0f);
}

/**
 * Offset/Address/Size: 0x5D8 | 0x8007F060 | size: 0x74
 */
float Winning(cTeam* team)
{
    if (team == NULL)
    {
        return 0.0f;
    }

    cTeam* pOtherTeam = team->GetOtherTeam();
    int scoreDiff = team->m_nScore - pOtherTeam->m_nScore;

    return NormalizeVal((float)scoreDiff, 0.0f, g_pGame->m_pFuzzyTweaks->fWinningScoreDelta);
}

/**
 * Offset/Address/Size: 0x5AC | 0x8007F034 | size: 0x2C
 */
float Offensive(cTeam* pTeam)
{
    if (pTeam == NULL)
    {
        return 0.0f;
    }

    if (pTeam->mpCurrentSituation == SITUATION_OFFENSE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x580 | 0x8007F008 | size: 0x2C
 */
float Defensive(cTeam* pTeam)
{
    if (pTeam == NULL)
    {
        return 0.0f;
    }

    if (pTeam->mpCurrentSituation == SITUATION_DEFENSE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x554 | 0x8007EFDC | size: 0x2C
 */
float Loose(cTeam* pTeam)
{
    if (pTeam == NULL)
    {
        return 0.0f;
    }

    if (pTeam->mpCurrentSituation == SITUATION_LOOSE)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x518 | 0x8007EFA0 | size: 0x3C
 */
float Stalling(cTeam* pTeam)
{
    if (pTeam == NULL)
    {
        return 0.0f;
    }

    float seconds = pTeam->mtDefensiveZoneTimer.GetSeconds();
    return NormalizeVal(seconds, g_vStallingConfidenceTime);
}

static float IsPassInPlay()
{
    if (g_pBall->m_pPassTarget != NULL)
    {
        return 1.0f;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x46C | 0x8007EEF4 | size: 0xAC
 */
float IsPassInPlayDelayed()
{
    float fScore;
    if (g_pBall->m_pPassTarget == NULL)
    {
        return 0.0f;
    }

    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);

    float fReaction = pSkillTweaks->Off_Reaction;
    float fPassDeadZone = g_pGame->m_pFuzzyTweaks->fPassDeadZone;
    float fMaxValue = fPassDeadZone * (1.0f - fReaction);

    fScore = IsPassInPlay(g_pBall);

    return NormalizeVal(fScore, 0.0f, fMaxValue);
}

static float IsShotInPlay()
{
    if (g_pScriptBall->m_tShotTimer.m_uPackedTime != 0)
    {
        return FMIN(1.0f, FMAX(0.0f, g_pScriptBall->m_tShotTimer.GetSeconds() / 1.5f));
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x3E0 | 0x8007EE68 | size: 0x8C
 */
float IsPerfectPassInPlay()
{
    float fInitialScore;
    if (g_pBall->m_pPassTarget != NULL)
    {
        fInitialScore = 1.0f;
    }
    else
    {
        fInitialScore = 0.0f;
    }

    float fScore = fInitialScore;
    if (fInitialScore > 0.0f)
    {
        if (g_pScriptBall->m_pPrevOwner != NULL && g_pScriptBall->m_pPrevOwner->m_eClassType == FIELDER)
        {
            if (g_pScriptBall->mbHyperSTS == false)
            {
                fScore = 0.0f;
            }
            fScore *= 2.0f;
        }
    }

    float fClampedScore = FMAX(fScore, 0.0);
    fClampedScore = FMIN(fClampedScore, 1.0);

    return fClampedScore;
}

/**
 * Offset/Address/Size: 0x35C | 0x8007EDE4 | size: 0x84
 */
float PerfectPassCandidateFrom(cFielder* pReceiver, cFielder* pBallOwner)
{
    bool canDoPerfectPass;

    if (pReceiver == NULL)
    {
        return 0.0f;
    }

    if (pBallOwner == NULL)
    {
        return 0.0f;
    }

    canDoPerfectPass = false;
    if ((pBallOwner->m_pBall != 0) && (pBallOwner->DoCalcCanDoPerfectPass(pReceiver, pReceiver->m_v3Position) != 0))
    {
        canDoPerfectPass = true;
    }

    if (canDoPerfectPass)
    {
        return 1.0f;
    }
    return 0.0f;
}

/**
 * Offset/Address/Size: 0x310 | 0x8007ED98 | size: 0x4C
 */
float TimeCloseToOver(cGame* pGame)
{
    if (!pGame)
    {
        return 0.0f;
    }

    FuzzyTweaks* pTweaks = g_pGame->m_pFuzzyTweaks;
    float gameTime = pGame->GetNormalizedGameTime();
    return NormalizeVal(gameTime, pTweaks->fGameTimeCloseToOver, 1.0f);
}

/**
 * Offset/Address/Size: 0x2C4 | 0x8007ED4C | size: 0x4C
 */
float TimeNearlyOver(cGame* pGame)
{
    if (!pGame)
    {
        return 0.0f;
    }

    FuzzyTweaks* pTweaks = g_pGame->m_pFuzzyTweaks;
    float gameTime = pGame->GetNormalizedGameTime();
    return NormalizeVal(gameTime, pTweaks->fGameTimeNearlyOver, 1.0f);
}

/**
 * Offset/Address/Size: 0x278 | 0x8007ED00 | size: 0x4C
 */
float TimeFarFromOver(cGame* pGame)
{
    if (!pGame)
    {
        return 0.0f;
    }

    FuzzyTweaks* pTweaks = g_pGame->m_pFuzzyTweaks;
    float gameTime = pGame->GetNormalizedGameTime();
    return NormalizeVal(gameTime, 1.0f, pTweaks->fGameTimeFarFromOver);
}

/**
 * Offset/Address/Size: 0x1E0 | 0x8007EC68 | size: 0x98
 */
float Difficult(cTeam* pTeam)
{
    eDifficultyID diff;
    f32 fScore;

    if (pTeam == NULL)
    {
        return 0.0f;
    }

    diff = nlSingleton<GameInfoManager>::Instance()->mCurrentDifficulty[(s16)pTeam->m_nSide];
    if (diff == DIFF_HUMAN)
    {
        return 0.5f;
    }

    float fMul = 0.25f;
    fScore = FMAX((float)diff * fMul, 0.0f);
    fScore = (fScore <= 1.0f) ? fScore : 1.0f;
    return fScore;
}

static float InDefensiveZone(const nlVector3& v3Position, eTeamSide teamside)
{
    nlVector3 aiLoc;
    FieldLocToAILoc(aiLoc, v3Position, teamside);
    return NormalizeVal(aiLoc.x, g_pGame->m_pFuzzyTweaks->vDefensiveConfidenceDistances);
}

/**
 * Offset/Address/Size: 0x1A0 | 0x8007EC28 | size: 0x40
 */
float InOffensiveZone(const nlVector3& v3Position, eTeamSide teamside)
{
    nlVector3 aiLoc;
    FieldLocToAILoc(aiLoc, v3Position, teamside);

    return NormalizeVal(aiLoc.x, g_pGame->m_pFuzzyTweaks->vOffensiveConfidenceDistances);
}

/**
 * Offset/Address/Size: 0x13C | 0x8007EBC4 | size: 0x64
 */
float InDefensiveZone(cPlayer* pPlayer)
{
    nlVector3 aiLoc;
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    eTeamSide teamSide = (eTeamSide)(pPlayer->m_pTeam->m_nSide);
    nlVector3& playerPos = PositionOf<cPlayer*>(pPlayer);
    FieldLocToAILoc(aiLoc, playerPos, teamSide);

    return NormalizeVal(aiLoc.x, g_pGame->m_pFuzzyTweaks->vDefensiveConfidenceDistances);
}

/**
 * Offset/Address/Size: 0xD8 | 0x8007EB60 | size: 0x64
 */
float InOffensiveZone(cPlayer* pPlayer)
{
    nlVector3 aiLoc;
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    eTeamSide teamSide = (eTeamSide)(pPlayer->m_pTeam->m_nSide);
    nlVector3& playerPos = PositionOf<cPlayer*>(pPlayer);
    FieldLocToAILoc(aiLoc, playerPos, teamSide);

    return NormalizeVal(aiLoc.x, g_pGame->m_pFuzzyTweaks->vOffensiveConfidenceDistances);
}

static float InNeutralZone(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return 0.0f;
    }

    return FMIN(
        1.0f - InOffensiveZone(pPlayer->m_v3Position, (eTeamSide)pPlayer->m_pTeam->m_nSide),
        1.0f - InDefensiveZone(pPlayer->m_v3Position, (eTeamSide)pPlayer->m_pTeam->m_nSide));
}

/**
 * Offset/Address/Size: 0x6C | 0x8007EAF4 | size: 0x6C
 */
float InDefensiveZoneOfPlayer(cBall* pBall, cPlayer* pPlayer)
{
    nlVector3 aiLoc;
    if ((pBall == NULL) && (pPlayer != NULL))
    {
        return 0.0f;
    }

    eTeamSide teamSide = (eTeamSide)(pPlayer->m_pTeam->m_nSide);
    nlVector3& ballPos = PositionOf<cBall*>(pBall);
    FieldLocToAILoc(aiLoc, ballPos, teamSide);

    return NormalizeVal(aiLoc.x, g_pGame->m_pFuzzyTweaks->vDefensiveConfidenceDistances);
}

static float InNeutralZoneOfPlayer(cBall* pBall, cPlayer* pPlayer)
{
    if (pBall == NULL)
    {
        return 0.0f;
    }
    eTeamSide teamSide = (eTeamSide)pPlayer->m_pTeam->m_nSide;
    const nlVector3& ballPos = pBall->m_v3Position;
    return FMIN(1.0f - InOffensiveZoneOfPlayer(pBall, pPlayer), 1.0f - InDefensiveZone(ballPos, teamSide));
}

/**
 * Offset/Address/Size: 0x0 | 0x8007EA88 | size: 0x6C
 */
float InOffensiveZoneOfPlayer(cBall* pBall, cPlayer* pPlayer)
{
    nlVector3 aiLoc;
    if ((pBall == NULL) && (pPlayer != NULL))
    {
        return 0.0f;
    }

    eTeamSide teamSide = (eTeamSide)(pPlayer->m_pTeam->m_nSide);
    nlVector3& ballPos = PositionOf<cBall*>(pBall);
    FieldLocToAILoc(aiLoc, ballPos, teamSide);

    return NormalizeVal(aiLoc.x, g_pGame->m_pFuzzyTweaks->vOffensiveConfidenceDistances);
}
