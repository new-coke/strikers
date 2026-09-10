#include "Game/AI/FielderDesires.h"

#include "Game/AI/AiUtil.h"
#include "Game/AI/FilteredRandom.h"
#include "Game/AI/Fuzzy.h"

#include "Game/AI/AvoidController.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/Scripts/CommonScript.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/AI/SpaceSearch.h"
#include "Game/AnimInventory.h"
#include "Game/FormationDefines.h"
#include "Game/GameTweaks.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Render/ShootToScoreMeter.h"

extern FuzzyVariant fvNotSet;
extern cTeam* g_pCurrentlyUpdatingTeam;
extern cFielder* g_pScriptCurrentFielder;
extern cCharacter* g_pCharacters[10];

enum eShotMeterState
{
    SHOT_METER_INACTIVE = 0,
    SHOT_METER_ACTIVE = 1,
    SHOT_METER_RELEASED = 2,
    SHOT_METER_STS_ACTIVE = 3,
    SHOT_METER_STS_TRANSISTION = 4,
    SHOT_METER_STS_RELEASED = 5,
};

class ShotMeter
{
public:
    eShotMeterState m_eShotMeterState;
    float m_fTime;
    float m_fScoreValue;
    float m_fSpeedValue;
    float m_fSTSValue;
    float mfSShotAimValue;
    void ShotReleased(cFielder* pFielder);
    float GetTotalDuration() const;
    void CalcOneTimerValue(cFielder* pFielder, bool bWasPerfectPass);
};

CommonDesireData g_vDesireCommonData[NUM_FIELDERDESIRES] = {
    CommonDesireData(FIELDERDESIRE_NEED_DESIRE),
    CommonDesireData(FIELDERDESIRE_CUT_AND_BREAK),
    CommonDesireData(FIELDERDESIRE_DEKE),
    CommonDesireData(FIELDERDESIRE_GET_IN_POSITION),
    CommonDesireData(FIELDERDESIRE_GET_OPEN),
    CommonDesireData(FIELDERDESIRE_HIT),
    CommonDesireData(FIELDERDESIRE_INTERCEPT_BALL),
    CommonDesireData(FIELDERDESIRE_MARK),
    CommonDesireData(FIELDERDESIRE_PROTECT_BALL),
    CommonDesireData(FIELDERDESIRE_RUN_TO_NET),
    CommonDesireData(FIELDERDESIRE_RUN_UPFIELD),
    CommonDesireData(FIELDERDESIRE_RUN_DOWNFIELD),
    CommonDesireData(FIELDERDESIRE_RUN_TO_LOCATION),
    CommonDesireData(FIELDERDESIRE_PASS),
    CommonDesireData(FIELDERDESIRE_SHOOT),
    CommonDesireData(FIELDERDESIRE_SLIDE_ATTACK),
    CommonDesireData(FIELDERDESIRE_SUPPORT_BALL_DEFENSIVE),
    CommonDesireData(FIELDERDESIRE_SUPPORT_BALL_OFFENSIVE),
    CommonDesireData(FIELDERDESIRE_USE_POWERUP),
    CommonDesireData(FIELDERDESIRE_WINDUP_PASS),
    CommonDesireData(FIELDERDESIRE_WINDUP_SHOT),
    CommonDesireData(FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP),
    CommonDesireData(FIELDERDESIRE_USER_CONTROLLED),
    CommonDesireData(FIELDERDESIRE_FINISH_ACTION),
    CommonDesireData(FIELDERDESIRE_ONETIMER),
    CommonDesireData(FIELDERDESIRE_POST_WHISTLE),
    CommonDesireData(FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE),
    CommonDesireData(FIELDERDESIRE_RECEIVE_PASS_FROM_RUN),
    CommonDesireData(FIELDERDESIRE_WAIT),
};

static const nlVector3 v3Zero = {
    0.0f,
    0.0f,
    0.0f,
};

static const nlVector2 g_vSupportBallDefensiveAILocations[6][2] = {
    { { 0.0f, 1.0f }, { 0.6f, 0.45f } },
    { { 0.0f, -1.0f }, { 0.6f, -0.45f } },
    { { 4.0f, 1.0f }, { 3.4f, 0.45f } },
    { { 4.0f, -1.0f }, { 3.4f, -0.45f } },
    { { 2.0f, 1.0f }, { 1.4f, 0.4f } },
    { { 2.0f, -1.0f }, { 1.4f, -0.4f } },
};

static const nlVector2 g_vSupportBallOffensiveAILocations[6][2] = {
    { { 0.0f, 1.0f }, { 0.6f, 0.45f } },
    { { 0.0f, -1.0f }, { 0.6f, -0.45f } },
    { { 4.0f, 1.0f }, { 3.4f, 0.45f } },
    { { 4.0f, -1.0f }, { 3.4f, -0.45f } },
    { { 2.0f, 1.0f }, { 2.6f, 0.4f } },
    { { 2.0f, -1.0f }, { 2.6f, -0.4f } },
};

static const nlVector2 g_vMarkingNetPassBalance = {
    0.0f,
    0.25f,
};

static const nlVector2 g_vMarkDistance = {
    7.0f,
    4.0f,
};

static const nlVector2 g_vMarkFormationBalance = {
    0.8f,
    1.0f,
};

static const nlVector2 g_vMarkBallOwner = {
    0.0f,
    0.5f,
};

static const nlVector2 g_vMarkImmediateThreatCoeff = {
    1.0f,
    0.5f,
};

static const nlVector2 g_vMarkFollowTimeDelay = {
    0.4f,
    0.1f,
};

static inline void CalcDeltaToTarget(nlVector3& outDelta, const nlVector3& target, const nlVector3& origin)
{
    outDelta.x = target.x - origin.x;
    outDelta.y = target.y - origin.y;
    outDelta.z = target.z - origin.z;
}

static inline const nlVector3& GetBallPosition(cBall* pBall)
{
    return pBall->m_v3Position;
}

static inline const nlVector3& GetBallVelocity(cBall* pBall)
{
    return pBall->m_v3Velocity;
}

/**
 * Offset/Address/Size: 0x7188 | 0x80037F0C | size: 0xCC
 */
CommonDesireData::CommonDesireData(eFielderDesireState desireType)
{
    m_DesireType = desireType;
    m_ConfidenceExtrema.x = 1.0f;
    m_ConfidenceExtrema.y = 0.0f;
}

/**
 * Offset/Address/Size: 0x668C | 0x80037410 | size: 0x3C
 */
float CommonDesireData::CalcFuzzyChance(float fChance)
{
    return FGREATER(fChance, m_RandomGenerator.genrand());
}

/**
 * Offset/Address/Size: 0x6668 | 0x800373EC | size: 0x24
 */
bool CommonDesireData::CalcBoolChance(float fChance)
{
    return m_RandomChanceGen.genrand(fChance);
}

/**
 * Offset/Address/Size: 0x660C | 0x80037390 | size: 0x5C
 */
float CommonDesireData::NormalizeConfidence(float fConfidence)
{
    if (m_ConfidenceExtrema.x > fConfidence)
    {
        m_ConfidenceExtrema.x = (0.5f * fConfidence) + (0.5f * m_ConfidenceExtrema.x);
    }
    if (m_ConfidenceExtrema.y < fConfidence)
    {
        m_ConfidenceExtrema.y = (0.5f * fConfidence) + (0.5f * m_ConfidenceExtrema.y);
    }
    return NormalizeVal(fConfidence, m_ConfidenceExtrema);
}

/**
 * Offset/Address/Size: 0x65F8 | 0x8003737C | size: 0x14
 */
CommonDesireData& GetCommonDesireData(eFielderDesireState desireType)
{
    return g_vDesireCommonData[desireType];
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x6484 | 0x80037208 | size: 0x174
 */
void cFielder::QueueDesire(eFielderDesireState eDesireType, float fDuration, FuzzyVariant opt1, FuzzyVariant opt2)
{
    ClearQueuedDesire();

    m_sQueuedDesireParams.eDesireType = eDesireType;
    m_sQueuedDesireParams.fDuration = fDuration;
    m_sQueuedDesireParams.opt1 = opt1;
    m_sQueuedDesireParams.opt2 = opt2;
}
#pragma dont_inline off

/**
 * Offset/Address/Size: 0x63C8 | 0x8003714C | size: 0xBC
 */
void cFielder::ClearQueuedDesire()
{
    m_sQueuedDesireParams.fDuration = 0.0f;
    m_sQueuedDesireParams.eDesireType = FIELDERDESIRE_NEED_DESIRE;
    m_sQueuedDesireParams.opt1 = fvNotSet;
    m_sQueuedDesireParams.opt2 = fvNotSet;
}

/**
 * Offset/Address/Size: 0x620C | 0x80036F90 | size: 0x1BC
 */
bool cFielder::InitDesire(const sDesireParams* pParams, float fConfidence)
{
    return InitDesire(pParams->eDesireType, fConfidence, pParams->fDuration, pParams->opt1, pParams->opt2);
}

/**
 * Offset/Address/Size: 0x54DC | 0x80036260 | size: 0xD30
 */
static const float g_fPostWhistleDelayToFinishAction = 2.0f;

/**
 * Offset/Address/Size: 0x620C | 0x80036F90 | size: 0x1BC
 */
bool cFielder::InitDesire(eFielderDesireState eDesireType, float fConfidence, float fDuration, FuzzyVariant opt1, FuzzyVariant opt2)
{
    bool bDesireInitSuccess;
    unsigned long uQueuedThoughtHash;
    if (GetGlobalPad() == NULL && m_pBall != NULL && m_eFielderDesireState == FIELDERDESIRE_WINDUP_SHOT
        && (u32)(eDesireType - FIELDERDESIRE_PASS) > 1 && eDesireType != FIELDERDESIRE_DEKE)
    {
        m_pShotMeter->ShotReleased(this);
        m_eFielderDesireState = FIELDERDESIRE_FINISH_ACTION;
        InitActionShot(false);
        return true;
    }
    uQueuedThoughtHash = 0;
    bDesireInitSuccess = true;
    m_DesireCommonVars.tAge.m_uPackedTime = 0;
    m_DesireCommonVars.tMiscTimer.m_uPackedTime = 0;
    m_DesireCommonVars.fMisc = 0.0f;
    m_DesireCommonVars.v3DesiredPosition = m_v3Position;
    m_DesireCommonVars.pBallOwner = g_pBall->m_pOwner;
    m_DesireCommonVars.pSBC = Fuzzy::GetStrategicBallCarrier(m_pTeam).mData.pPlayer;
    m_DesireCommonVars.turboRequest = TR_FAR_DISTANCE;
    if (fDuration > 0.0f)
    {
        SetDesireDuration(fDuration, true);
    }
    else
    {
        switch (eDesireType)
        {
        case FIELDERDESIRE_CUT_AND_BREAK:
            SetDesireDuration(3.0f, true);
            break;
        case FIELDERDESIRE_FINISH_ACTION:
            SetDesireDuration(99999.0f, true);
            break;
        case FIELDERDESIRE_USER_CONTROLLED:
            SetDesireDuration(99999.0f, true);
            break;
        case FIELDERDESIRE_WAIT:
            SetDesireDuration(0.5f, true);
            break;
        case FIELDERDESIRE_SLIDE_ATTACK:
            SetDesireDuration(1.0f, true);
            break;
        case FIELDERDESIRE_HIT:
            SetDesireDuration(1.0f, true);
            break;
        case FIELDERDESIRE_RUN_TO_NET:
            SetDesireDuration(1.0f, true);
            break;
        case FIELDERDESIRE_DEKE:
        case FIELDERDESIRE_GET_IN_POSITION:
        case FIELDERDESIRE_GET_OPEN:
        case FIELDERDESIRE_INTERCEPT_BALL:
        case FIELDERDESIRE_MARK:
        case FIELDERDESIRE_PROTECT_BALL:
        case FIELDERDESIRE_RUN_UPFIELD:
        case FIELDERDESIRE_RUN_DOWNFIELD:
        case FIELDERDESIRE_RUN_TO_LOCATION:
        case FIELDERDESIRE_PASS:
        case FIELDERDESIRE_SHOOT:
        case FIELDERDESIRE_SUPPORT_BALL_DEFENSIVE:
        case FIELDERDESIRE_SUPPORT_BALL_OFFENSIVE:
        case FIELDERDESIRE_USE_POWERUP:
        case FIELDERDESIRE_WINDUP_PASS:
        case FIELDERDESIRE_WINDUP_SHOT:
        case FIELDERDESIRE_POST_WHISTLE:
            SetDesireDuration(1.0f, true);
            break;
        case FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP:
            SetDesireDuration(0.0f, true);
            break;
        default:
            break;
        }
    }
    switch (eDesireType)
    {
    case FIELDERDESIRE_CUT_AND_BREAK:
    {
        if (g_pGame->IsThoughtAllowed(mThoughtHashInitCutAndBreak))
        {
            bDesireInitSuccess = InitDesireCutAndBreak();
        }
        else
        {
            uQueuedThoughtHash = mThoughtHashInitCutAndBreak;
        }
        break;
    }
    case FIELDERDESIRE_GET_OPEN:
        if (g_pGame->IsThoughtAllowed(mThoughtHashInitGetOpen))
        {
            bDesireInitSuccess = InitDesireGetOpen();
        }
        else
        {
            uQueuedThoughtHash = mThoughtHashInitGetOpen;
        }
        break;
    case FIELDERDESIRE_RUN_TO_NET:
        bDesireInitSuccess = InitDesireRunToNet();
        break;
    case FIELDERDESIRE_WINDUP_PASS:
    {
        if (g_pGame->IsThoughtAllowed(mThoughtHashInitWindupPass))
        {
            cPlayer* pTarget = (cPlayer*)opt1.mData.pPlayer;
            if (pTarget != NULL)
            {
                bDesireInitSuccess = InitDesireWindupPass(pTarget, opt2.mData.b);
            }
            else
            {
                bDesireInitSuccess = false;
                AbortPendingThoughts();
            }
        }
        else
        {
            uQueuedThoughtHash = mThoughtHashInitWindupPass;
        }
        break;
    }
    case FIELDERDESIRE_DEKE:
        InitDesireDeke();
        break;
    case FIELDERDESIRE_GET_IN_POSITION:
        InitDesireGetInPosition();
        break;
    case FIELDERDESIRE_INTERCEPT_BALL:
        InitDesireInterceptBall();
        break;
    case FIELDERDESIRE_MARK:
        InitDesireMark();
        break;
    case FIELDERDESIRE_POST_WHISTLE:
        InitDesirePostWhistle();
        break;
    case FIELDERDESIRE_PROTECT_BALL:
        InitDesireProtectBall();
        break;
    case FIELDERDESIRE_RUN_UPFIELD:
        InitDesireRunUpField();
        break;
    case FIELDERDESIRE_RUN_DOWNFIELD:
        InitDesireRunDownField();
        break;
    case FIELDERDESIRE_RUN_TO_LOCATION:
        InitDesireRunToLocation(opt1.mData.vector, opt2.mData.b);
        break;
    case FIELDERDESIRE_SHOOT:
        InitDesireShoot(opt1.mData.b, opt2.mData.b);
        break;
    case FIELDERDESIRE_SUPPORT_BALL_OFFENSIVE:
        InitDesireSupportBallOffensive();
        break;
    case FIELDERDESIRE_SUPPORT_BALL_DEFENSIVE:
        InitDesireSupportBallDefensive();
        break;
    case FIELDERDESIRE_WAIT:
        InitDesireWait();
        break;
    case FIELDERDESIRE_USER_CONTROLLED:
        InitDesireUserControlled();
        break;
    case FIELDERDESIRE_WINDUP_SHOT:
        InitDesireWindupShot();
        break;
    case FIELDERDESIRE_FINISH_ACTION:
        InitDesireFallThrough();
        break;
    case FIELDERDESIRE_PASS:
    {
        cPlayer* pTarget = (cPlayer*)opt1.mData.pPlayer;
        if (pTarget != NULL)
        {
            InitDesirePass(pTarget, opt2.mData.b);
        }
        else
        {
            bDesireInitSuccess = false;
            AbortPendingThoughts();
        }
        break;
    }
    case FIELDERDESIRE_SLIDE_ATTACK:
    {
        cFielder* pTarget = (cFielder*)opt1.mData.pPlayer;
        if (pTarget != NULL && pTarget->m_eClassType == FIELDER)
        {
            InitDesireSlideAttack(pTarget);
        }
        else
        {
            bDesireInitSuccess = false;
            AbortPendingThoughts();
        }
        break;
    }
    case FIELDERDESIRE_HIT:
    {
        cFielder* pTarget = (cFielder*)opt1.mData.pPlayer;
        if (pTarget != NULL && pTarget->m_eClassType == FIELDER)
        {
            InitDesireHit((cFielder*)opt1.mData.pPlayer);
        }
        else
        {
            bDesireInitSuccess = false;
            AbortPendingThoughts();
        }
        break;
    }
    case FIELDERDESIRE_USE_POWERUP:
    {
        cFielder* pTarget = (cFielder*)opt2.mData.pPlayer;
        if (pTarget == NULL || pTarget->m_eClassType == FIELDER)
        {
            InitDesireUsePowerup((ePowerUpType)opt1.mData.i, pTarget);
        }
        else
        {
            bDesireInitSuccess = false;
            AbortPendingThoughts();
        }
        break;
    }
    case FIELDERDESIRE_NEED_DESIRE:
    case FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP:
    case FIELDERDESIRE_ONETIMER:
    case FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE:
    case FIELDERDESIRE_RECEIVE_PASS_FROM_RUN:
    default:
        break;
    }
    if (uQueuedThoughtHash != 0)
    {
        QueueDesire(eDesireType, fDuration, opt1, opt2);
        InitDesire(FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP, 0.5f, -1.0f, fvNotSet, fvNotSet);
        bDesireInitSuccess = false;
    }
    else if (bDesireInitSuccess)
    {
        bDesireInitSuccess = SetDesire(eDesireType, fConfidence);
    }
    return bDesireInitSuccess;
}

/**
 * Offset/Address/Size: 0x4700 | 0x80035484 | size: 0xDDC
 */
void cFielder::UpdateDesireState(float fDeltaT)
{
    switch (m_eFielderDesireState)
    {
    case FIELDERDESIRE_CUT_AND_BREAK:
        DesireCutAndBreak(fDeltaT);
        break;

    case FIELDERDESIRE_DEKE:
        DesireDeke(fDeltaT);
        break;

    case FIELDERDESIRE_FINISH_ACTION:
        DesireFallThrough(fDeltaT);
        break;

    case FIELDERDESIRE_GET_IN_POSITION:
        DesireGetInPosition(fDeltaT);
        break;

    case FIELDERDESIRE_GET_OPEN:
        DesireGetOpen(fDeltaT);
        break;

    case FIELDERDESIRE_HIT:
        DesireHit(fDeltaT);
        break;

    case FIELDERDESIRE_INTERCEPT_BALL:
        DesireInterceptBall(fDeltaT);
        break;

    case FIELDERDESIRE_MARK:
        DesireMark(fDeltaT);
        break;

    case FIELDERDESIRE_ONETIMER:
        DesireOneTimer(fDeltaT);
        break;

    case FIELDERDESIRE_PROTECT_BALL:
        DesireProtectBall(fDeltaT);
        break;

    case FIELDERDESIRE_RUN_TO_NET:
        DesireRunToNet(fDeltaT);
        break;

    case FIELDERDESIRE_RUN_UPFIELD:
        DesireRunUpField(fDeltaT);
        break;

    case FIELDERDESIRE_RUN_DOWNFIELD:
        DesireRunDownField(fDeltaT);
        break;

    case FIELDERDESIRE_RUN_TO_LOCATION:
        DesireRunToLocation(fDeltaT);
        break;

    case FIELDERDESIRE_PASS:
        DesirePass(fDeltaT);
        break;

    case FIELDERDESIRE_SHOOT:
        DesireShoot(fDeltaT);
        break;

    case FIELDERDESIRE_POST_WHISTLE:
        DesirePostWhistle(fDeltaT);
        break;

    case FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE:
        DesireReceivePassFromIdle(fDeltaT);
        break;
    case FIELDERDESIRE_RECEIVE_PASS_FROM_RUN:
        DesireReceivePassFromRun(fDeltaT);
        break;
    case FIELDERDESIRE_SLIDE_ATTACK:
        DesireSlideAttack(fDeltaT);
        break;
    case FIELDERDESIRE_SUPPORT_BALL_DEFENSIVE:
        DesireSupportBall(fDeltaT, true);
        break;
    case FIELDERDESIRE_SUPPORT_BALL_OFFENSIVE:
        DesireSupportBall(fDeltaT, false);
        break;
    case FIELDERDESIRE_USE_POWERUP:
        DesireUsePowerup(fDeltaT);
        break;
    case FIELDERDESIRE_USER_CONTROLLED:
        DesireUserControlled(fDeltaT);
        break;
    case FIELDERDESIRE_WAIT:
        DesireWait(fDeltaT);
        break;
    case FIELDERDESIRE_WINDUP_PASS:
        DesireWindupPass(fDeltaT);
        break;
    case FIELDERDESIRE_WINDUP_SHOT:
        DesireWindupShot(fDeltaT);
        break;
    case FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP:
        if (!IsRunning())
        {
            StartRunning();
        }
        break;
    case FIELDERDESIRE_NEED_DESIRE:
    default:
        break;
    }

    if (m_eActionState == ACTION_NEED_ACTION)
    {
        StartRunning();
    }
}
/**
 * Offset/Address/Size: 0x469C | 0x80035420 | size: 0x64
 */
void cFielder::EndDesire(bool bCheckTimer)
{
    bool bShouldSetDuration = true;

    if (bCheckTimer)
    {
        bShouldSetDuration = m_DesireCommonVars.tAge.GetSeconds() > 0.5f;
    }

    if (bShouldSetDuration)
    {
        SetDesireDuration(0.0f, true);
    }
}

/**
 * Offset/Address/Size: 0x45C8 | 0x8003534C | size: 0xD4
 */
void cFielder::CleanUpDesire(eFielderDesireState eNewDesireState)
{
    switch (m_eFielderDesireState)
    {
    case FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP:
        AbortPendingThoughts();
        break;

    case FIELDERDESIRE_ONETIMER:
        CleanDesireOneTimer();
        break;

    case FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE:
        CleanDesireReceivePassFromIdle();
        break;

    case FIELDERDESIRE_RECEIVE_PASS_FROM_RUN:
        CleanDesireReceivePassFromRun();
        break;

    case FIELDERDESIRE_CUT_AND_BREAK:
    case FIELDERDESIRE_GET_OPEN:
    case FIELDERDESIRE_RUN_TO_NET:
    case FIELDERDESIRE_SHOOT:
        if (eNewDesireState != FIELDERDESIRE_RECEIVE_PASS_FROM_RUN && eNewDesireState != FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE)
        {
            SetSpaceSearch(nullptr);
        }
        break;

    case FIELDERDESIRE_WINDUP_PASS:
    case FIELDERDESIRE_WINDUP_SHOT:
        SetSpaceSearch(nullptr);
        break;

    default:
        break;
    }

    SetDesireDuration(0.0f, true);
    m_eFielderDesireState = FIELDERDESIRE_NEED_DESIRE;
}

static const float g_fLooseBallActionRethinkTime = 0.3f;

void cFielder::CleanDesireOneTimer()
{
    SetNoPickUpTime(0.0f);
}

void cFielder::CleanDesireReceivePassFromIdle()
{
    SetNoPickUpTime(0.0f);
    SetSpaceSearch(nullptr);
}

void cFielder::CleanDesireReceivePassFromRun()
{
    SetNoPickUpTime(0.0f);
    SetSpaceSearch(nullptr);
}

void cFielder::InitDesireInterceptBall()
{
    m_eDesireSubState = 0;
    m_DesireCommonVars.tMiscTimer.m_uPackedTime = 0;
    if (g_pBall->m_pPassTarget != NULL)
    {
        float fMultiplier = 0.5f + (nlRandomf(0.15f, &nlDefaultSeed) - 0.075f);
        float fPassTargetSeconds = g_pBall->m_tPassTargetTimer.GetSeconds();
        m_DesireCommonVars.tMiscTimer.SetSeconds(fPassTargetSeconds * fMultiplier);
    }
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

/**
 * Offset/Address/Size: 0x4204 | 0x80034F88 | size: 0x3C4
 */
void cFielder::DesireInterceptBall(float fDeltaT)
{
    bool bTrackBall;
    nlVector3 v3DesirePosition;
    nlVector3 v3FutureTargetPosition;
    float fTime;

    switch (m_eDesireSubState)
    {
    case 0:
    {
        bTrackBall = true;

        if (m_DesireCommonVars.tMiscTimer.m_uPackedTime == 0)
        {
            if (DoAILooseBallActionSelection())
            {
                const float& fActionRethinkTime = 99999.9f;
                m_DesireCommonVars.tMiscTimer.SetSeconds(fActionRethinkTime);
                bTrackBall = false;
            }
            else
            {
                const float& fActionRethinkTime = g_fLooseBallActionRethinkTime;
                m_DesireCommonVars.tMiscTimer.SetSeconds(fActionRethinkTime);
            }
        }

        if (bTrackBall)
        {
            cPlayer* pPassTarget = g_pBall->m_pPassTarget;
            if (pPassTarget != NULL && pPassTarget->m_eClassType == FIELDER)
            {
                float fVolley = ReceivingVolleyPass(pPassTarget);
                if (fVolley || High(g_pBall) >= 0.5f)
                {
                    ((cFielder*)pPassTarget)->CalcPointOnPerimeter(v3DesirePosition, m_v3Position, 0.25f);
                }
                else
                {
                    float fSeconds = g_pBall->m_tPassTargetTimer.GetSeconds();
                    float fz = pPassTarget->m_v3Position.z + fSeconds * pPassTarget->m_v3Velocity.z;
                    float fy = pPassTarget->m_v3Position.y + fSeconds * pPassTarget->m_v3Velocity.y;
                    float fx = pPassTarget->m_v3Position.x + fSeconds * pPassTarget->m_v3Velocity.x;
                    v3FutureTargetPosition.x = fx;
                    v3FutureTargetPosition.y = fy;
                    v3FutureTargetPosition.z = fz;

                    v3DesirePosition = GetClosestPointOnLineABFromPointC(g_pBall->m_v3Position, v3FutureTargetPosition, m_v3Position);
                }

                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                if (pSkillTweaks->Def_SlideAttackChance > 0.0f)
                {
                    if (CanISlideAttack(GetBallPosition(g_pBall), GetBallVelocity(g_pBall), &fTime))
                    {
                        InitActionSlideAttack(NULL, fTime);
                        m_eDesireSubState = 1;
                        bTrackBall = false;
                    }
                }
            }
            else
            {
                float fRawTime = m_pTeam->mfBallInterceptTimes[m_ID];
                float fInterceptTime = (3.0f <= fRawTime) ? 3.0f : fRawTime;

                float fz = g_pBall->m_v3Position.z + fInterceptTime * g_pBall->m_v3Velocity.z;
                float fy = g_pBall->m_v3Position.y + fInterceptTime * g_pBall->m_v3Velocity.y;
                float fx = g_pBall->m_v3Position.x + fInterceptTime * g_pBall->m_v3Velocity.x;
                v3DesirePosition.x = fx;
                v3DesirePosition.y = fy;
                v3DesirePosition.z = fz;
            }
        }

        if (bTrackBall)
        {
            eTurboRequest turboRequest = TR_MOVING_TARGET;
            float dx = v3DesirePosition.x - m_v3Position.x;
            float dy = v3DesirePosition.y - m_v3Position.y;
            if (nlSqrt(dx * dx + dy * dy, true) < 1.0f)
            {
                turboRequest = TR_FAR_DISTANCE;
            }

            SetDesiredSpeedAndDirectionToPosition(fDeltaT, v3DesirePosition, turboRequest, 0.3f, 0.6f);
            m_pAvoidance->UseMinimumAvoidance(NULL);
        }

        if (m_pBall == NULL)
        {
            cPlayer* pOwner = g_pBall->m_pOwner;
            if (pOwner == NULL || pOwner->m_eClassType != GOALIE)
            {
                break;
            }
        }
        SetDesireDuration(0.0f, true);
        break;
    }
    case 1:
    {
        SetDesireDuration(999999.9f, true);

        if (m_tSlideAttackTimer.m_uPackedTime != 0)
        {
            if (mActionSlideAttackVars.bAttackSucceeded == 0)
            {
                float fBallSpeed = nlSqrt(
                    g_pBall->m_v3Velocity.x * g_pBall->m_v3Velocity.x + g_pBall->m_v3Velocity.y * g_pBall->m_v3Velocity.y + g_pBall->m_v3Velocity.z * g_pBall->m_v3Velocity.z,
                    true);

                if (fBallSpeed > 0.05f)
                {
                    const nlVector3& v3BallVel = g_pBall->m_v3Velocity;
                    float fBallClosingSpeed = GetClosingSpeed2D(
                        GetJointPosition(m_nLeftFootJointIndex),
                        m_v3Velocity,
                        g_pBall->m_v3Position,
                        v3BallVel);

                    if (fBallClosingSpeed < 0.0f)
                    {
                        if (nlRandomf(1.0f, &nlDefaultSeed) > 0.5f)
                        {
                            m_tSlideAttackTimer.SetSeconds(0.0f);
                            m_eDesireSubState = 2;
                        }
                    }
                }
            }
        }
        else
        {
            m_eDesireSubState = 2;
        }
        break;
    }
    case 2:
    {
        if (m_eActionState == ACTION_NEED_ACTION)
        {
            SetDesireDuration(0.0f, true);
        }
        break;
    }
    }
}

void cFielder::DesireFallThrough(float fDeltaT)
{
    if (GetGlobalPad() != NULL)
    {
        if (GetGlobalPad()->JustPressed(PAD_USE, true))
        {
            if (m_eActionState != ACTION_ELECTROCUTION && m_eActionState != ACTION_SQUISH_REACT && m_eActionState != ACTION_STS_HIT_REACT
                && !IsPlayingPowerupAnim())
            {
                UseTeamPowerup(NULL);
            }
        }
        else if (GetGlobalPad()->JustPressed(PAD_TOGGLE_POWERUP, true))
        {
            m_pTeam->TogglePowerup(false);
        }
    }

    if (IsActionDone() || IsRunning())
    {
        SetDesireDuration(0.0f, true);
    }
}

void cFielder::InitDesireMark()
{
    m_pAvoidance->SetThingsToAvoid(0x1F);
    m_DesireCommonVars.tMiscTimer.m_uPackedTime = 0;
}

/**
 * Offset/Address/Size: 0x39EC | 0x80034770 | size: 0x818
 */
void cFielder::DesireMark(float fDeltaT)
{
    bool bBestBallInterceptor = (m_pTeam->m_pBallInterceptOrderedFielders[0] == this);
    float fTimeDelay;
    nlVector3 v3NetPosition;
    nlVector3 vAccumulated_v3;
    float fTotalWeight_v3;
    float fMarkingNetPassBalance;
    float fMarkingDistance;
    float fMarkFormationBalance;
    float fMarkBallOwnerBalance;
    float fMarkThreatCoeff;
    float fDistanceMultiplier;
    nlVector3 v3FormationPosition;
    nlVector3 v3DesiredPos;

    if (m_pMark == NULL || this == g_pBall->m_pOwner || (bBestBallInterceptor && m_pTeam->mpCurrentSituation == SITUATION_LOOSE))
    {
        SetDesireDuration(0.0f, true);
        return;
    }

    if (IsOnSameTeam(g_pBall->m_pOwner) || bBestBallInterceptor)
    {
        bool bStale = m_DesireCommonVars.tAge.GetSeconds() > 0.5f;
        if (bStale)
        {
            SetDesireDuration(0.0f, true);
        }
    }

    if (m_pMark->m_pBall != NULL)
    {
        SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);

        if (pSkillTweaks->Def_SlideAttackChance > 0.0f && m_pMark->m_tBallPossessionTimer.GetSeconds() > 5.0f)
        {
            InitDesire(FIELDERDESIRE_SLIDE_ATTACK, 0.5f, -1.0f, FuzzyVariant((cPlayer*)m_pMark), fvNotSet);
            return;
        }
    }

    if (m_DesireCommonVars.tMiscTimer.m_uPackedTime == 0)
    {
        fTimeDelay = Interpolate(
            g_vMarkFollowTimeDelay.x,
            g_vMarkFollowTimeDelay.y,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Def_Marking);

        float fTimeDelayRange = fTimeDelay;
        fTimeDelayRange *= 0.8f;
        m_DesireCommonVars.tMiscTimer.SetSeconds(
            fTimeDelay + (nlRandomf(fTimeDelayRange, &nlDefaultSeed) - (0.5f * fTimeDelayRange)));

        v3NetPosition = m_pTeam->m_pNet->m_v3NetLocation;

        nlVector3 v3MarkPosition;
        nlVec3ScaleAdd(v3MarkPosition, 0.1f, m_pMark->m_v3Velocity, m_pMark->m_v3Position);
        v3MarkPosition.z = 0.0f;

        nlVector3 v3Dir;
        nlVec3Set(v3Dir, v3NetPosition.x - v3MarkPosition.x, v3NetPosition.y - v3MarkPosition.y, v3NetPosition.z - v3MarkPosition.z);
        nlVec3Scale(v3Dir, nlRecipSqrt(nlVec3LengthSquared(v3Dir), true));

        fTotalWeight_v3 = 0.0f;

        vAccumulated_v3 = v3Zero;

        fMarkingNetPassBalance = Interpolate(
            g_vMarkingNetPassBalance.x,
            g_vMarkingNetPassBalance.y,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Def_Marking);
        fMarkingDistance = Interpolate(
            g_vMarkDistance.x,
            g_vMarkDistance.y,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Def_Marking);
        fMarkFormationBalance = Interpolate(
            g_vMarkFormationBalance.x,
            g_vMarkFormationBalance.y,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Def_Marking);
        fMarkBallOwnerBalance = Interpolate(
            g_vMarkBallOwner.x,
            g_vMarkBallOwner.y,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Def_Marking);
        fMarkThreatCoeff = Interpolate(
            g_vMarkImmediateThreatCoeff.x,
            g_vMarkImmediateThreatCoeff.y,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Def_Marking);

        fDistanceMultiplier = Interpolate(0.5f, 1.0f, FarToTheirNet(m_pMark));
        fMarkingDistance = fMarkingDistance * fDistanceMultiplier;

        if (UserControlledT(m_pTeam))
        {
            if (ReceivingPass(m_pMark) || WindingUpForShot(m_pMark))
            {
                fMarkingDistance = fMarkingDistance * fMarkThreatCoeff;
            }
        }

        if (m_pMark->m_pBall == NULL)
        {
            cPlayer* pSBC = Fuzzy::GetStrategicBallCarrier(m_pTeam->GetOtherTeam()).mData.pPlayer;

            if (pSBC != NULL && pSBC != m_pMark)
            {
                nlVector3 v3SBCPosition;
                nlVec3ScaleAdd(v3SBCPosition, 0.1f, pSBC->m_v3Velocity, pSBC->m_v3Position);

                nlVector3 v3SBCDir;
                nlVec3Set(v3SBCDir, v3SBCPosition.x - v3MarkPosition.x, v3SBCPosition.y - v3MarkPosition.y, v3SBCPosition.z - v3MarkPosition.z);

                nlVec3Scale(v3SBCDir, nlRecipSqrt(nlVec3LengthSquared(v3SBCDir), true));

                if (nlVec3DotProduct(v3SBCDir, v3Dir) >= 0.0f)
                {
                    float fToMarkNetPassBalance = 1.0f - fMarkingNetPassBalance;
                    nlVec3Set(v3Dir,
                        (fToMarkNetPassBalance * v3Dir.x) + (fMarkingNetPassBalance * v3SBCDir.x),
                        (fToMarkNetPassBalance * v3Dir.y) + (fMarkingNetPassBalance * v3SBCDir.y),
                        (fToMarkNetPassBalance * v3Dir.z) + (fMarkingNetPassBalance * v3SBCDir.z));
                }

                nlVector3 v3ToNet;
                nlVec3Set(v3ToNet, v3NetPosition.x - v3SBCPosition.x, v3NetPosition.y - v3SBCPosition.y, v3NetPosition.z - v3SBCPosition.z);
                float fToNetInvLength = nlRecipSqrt(nlVec3LengthSquared(v3ToNet), true);
                nlVec3Scale(v3ToNet, fToNetInvLength);

                nlVector3 vThreatTarget;
                nlVec3Set(vThreatTarget, (fMarkingDistance * v3ToNet.x) + v3SBCPosition.x, (fMarkingDistance * v3ToNet.y) + v3SBCPosition.y, (fMarkingDistance * v3ToNet.z) + v3SBCPosition.z);

                float fMarkBallOwner = Fuzzy::ShouldIMarkBallOwner(this).mData.f;
                if (fMarkBallOwner > 0.0f)
                {
                    float fWeight = fMarkBallOwner * fMarkBallOwnerBalance;
                    nlVec3ScaleAdd(vAccumulated_v3, fWeight, vThreatTarget, vAccumulated_v3);
                    fTotalWeight_v3 = fTotalWeight_v3 + fWeight;
                }
            }
        }

        nlVector3 v3MarkTarget;
        nlVec3Set(
            v3MarkTarget,
            (fMarkingDistance * v3Dir.x) + v3MarkPosition.x,
            (fMarkingDistance * v3Dir.y) + v3MarkPosition.y,
            (fMarkingDistance * v3Dir.z) + v3MarkPosition.z);
        nlVec3ScaleAdd(vAccumulated_v3, fMarkFormationBalance, v3MarkTarget, vAccumulated_v3);
        fTotalWeight_v3 = fTotalWeight_v3 + fMarkFormationBalance;

        m_DesireCommonVars.bInPosition = GetFormationPosition(v3FormationPosition, 0.0f);
        if (m_DesireCommonVars.bInPosition)
        {
            v3FormationPosition = m_v3Position;
        }

        float fFormationWeight = 1.0f - fMarkFormationBalance;
        fTotalWeight_v3 = fTotalWeight_v3 + fFormationWeight;
        nlVec3Set(vAccumulated_v3,
            (fFormationWeight * v3FormationPosition.x) + vAccumulated_v3.x,
            (fFormationWeight * v3FormationPosition.y) + vAccumulated_v3.y,
            (fFormationWeight * v3FormationPosition.z) + vAccumulated_v3.z);

        if (fTotalWeight_v3 > 0.0f)
        {
            float fInvTotalWeight = 1.0f / fTotalWeight_v3;
            nlVec3Set(v3DesiredPos,
                fInvTotalWeight * vAccumulated_v3.x,
                fInvTotalWeight * vAccumulated_v3.y,
                fInvTotalWeight * vAccumulated_v3.z);
        }
        else
        {
            v3DesiredPos = v3Zero;
        }

        m_DesireCommonVars.v3DesiredPosition = v3DesiredPos;
    }

    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_DesireCommonVars.v3DesiredPosition, TR_FAR_DISTANCE, 0.75f, 0.75f);
    ShouldIStrafe();
}

static inline float SupportClampLower(float x, float min)
{
    if (x >= min)
        return x;
    else
        return min;
}

static inline float SupportClampUpper(float x, float max)
{
    if (x <= max)
        return x;
    else
        return max;
}

void cFielder::InitDesireProtectBall()
{
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::DesireProtectBall(float fDeltaT)
{
    if (this != g_pBall->m_pOwner && (m_DesireCommonVars.tAge.GetSeconds() > 0.5f) != false)
    {
        SetDesireDuration(0.0f, true);
    }
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_v3Position, TR_FAR_DISTANCE, 1.0f, 1.0f);
    ShouldIStrafe();
}

/**
 * Offset/Address/Size: 0x35E4 | 0x80034368 | size: 0x408
 */
void cFielder::DesireSupportBall(float fDeltaT, bool bDefensive)
{
    if (g_pBall->m_pOwner == this)
    {
        SetDesireDuration(0.0f, true);
        return;
    }

    const nlVector2(*pAILocations)[2] = bDefensive ? g_vSupportBallDefensiveAILocations : g_vSupportBallOffensiveAILocations;
    int iNumRules = bDefensive ? 6 : 6;

    nlVector3 v3BallPosition;
    nlVector3 v3BallAILoc;

    cBall* pBall = g_pBall;
    v3BallPosition.x = pBall->m_v3Position.x + (0.2f * pBall->GetAIVelocity()->x);

    pBall = g_pBall;
    v3BallPosition.y = pBall->m_v3Position.y + (0.2f * pBall->GetAIVelocity()->y);
    v3BallPosition.z = 0.0f;

    FieldLocToAILoc(v3BallAILoc, v3BallPosition, (eTeamSide)m_pTeam->m_nSide);

    int best_rule_i[2] = { -1, -1 };
    float best_rule_distance[2] = { 1000000000.0f, 1000000000.0f };

    for (int i = 0; i < iNumRules; i++)
    {
        float dy = v3BallAILoc.y - pAILocations[i][0].y;
        float dx = v3BallAILoc.x - pAILocations[i][0].x;
        float dist = nlSqrt(dx * dx + dy * dy, true);

        if (dist < best_rule_distance[0])
        {
            best_rule_distance[1] = best_rule_distance[0];
            best_rule_i[1] = best_rule_i[0];
            best_rule_i[0] = i;
            best_rule_distance[0] = dist;
        }
        else if (dist < best_rule_distance[1])
        {
            best_rule_i[1] = i;
            best_rule_distance[1] = dist;
        }
    }

    nlVector2 v2OffsetFromBall[2];
    nlVector2 v2TargetPositions[2];

    for (int i_rule = 0; i_rule < 2; i_rule++)
    {
        const nlVector2* pLocation = pAILocations[best_rule_i[i_rule]];

        float offsetX = pLocation[1].x - pLocation[0].x;
        float offsetY = pLocation[1].y - pLocation[0].y;
        v2OffsetFromBall[i_rule].x = offsetX;
        v2OffsetFromBall[i_rule].y = offsetY;

        v2TargetPositions[i_rule].x = SupportClampUpper(SupportClampLower(v3BallAILoc.x + v2OffsetFromBall[i_rule].x, 0.0f), 4.0f);
        v2TargetPositions[i_rule].y = SupportClampUpper(SupportClampLower(v3BallAILoc.y + v2OffsetFromBall[i_rule].y, -1.0f), 1.0f);
    }

    nlVector3 v3SupportPosition = {
        0.0f,
        0.0f,
        0.0f,
    };

    float t = best_rule_distance[1] / (best_rule_distance[0] + best_rule_distance[1]);
    float oneMinusT = 1.0f - t;

    v3SupportPosition.x = ((1.0f - oneMinusT) * v2TargetPositions[0].x) + (oneMinusT * v2TargetPositions[1].x);
    v3SupportPosition.y = ((1.0f - oneMinusT) * v2TargetPositions[0].y) + (oneMinusT * v2TargetPositions[1].y);

    AILocToFieldLoc(v3SupportPosition, v3SupportPosition, (eTeamSide)m_pTeam->m_nSide);

    float fTotalWeight_v3 = 0.0f;
    float fAIBallLocationWeight = 0.7f;

    nlVector3 vAccumulated_v3 = v3Zero;
    fTotalWeight_v3 = fTotalWeight_v3 + fAIBallLocationWeight;

    float fWeightedX = (fAIBallLocationWeight * v3SupportPosition.x) + vAccumulated_v3.x;
    float fWeightedZ = (fAIBallLocationWeight * v3SupportPosition.z) + vAccumulated_v3.z;
    float fWeightedY = (fAIBallLocationWeight * v3SupportPosition.y) + vAccumulated_v3.y;
    vAccumulated_v3.x = fWeightedX;
    vAccumulated_v3.y = fWeightedY;
    vAccumulated_v3.z = fWeightedZ;

    float fFormationWeight = 0.3f;

    nlVector3 v3FormationPosition;
    m_DesireCommonVars.bInPosition = GetFormationPosition(v3FormationPosition, 0.0f);
    if (m_DesireCommonVars.bInPosition)
    {
        v3FormationPosition = m_v3Position;
    }

    fTotalWeight_v3 = fTotalWeight_v3 + fFormationWeight;

    float fFormationWeightedX = (fFormationWeight * v3FormationPosition.x) + vAccumulated_v3.x;
    float fFormationWeightedZ = (fFormationWeight * v3FormationPosition.z) + vAccumulated_v3.z;
    float fFormationWeightedY = (fFormationWeight * v3FormationPosition.y) + vAccumulated_v3.y;
    vAccumulated_v3.x = fFormationWeightedX;
    vAccumulated_v3.y = fFormationWeightedY;
    vAccumulated_v3.z = fFormationWeightedZ;

    nlVector3 v3DesiredPos;
    if (fTotalWeight_v3 > 0.0f)
    {
        float fInvWeight = 1.0f / fTotalWeight_v3;
        v3DesiredPos.x = fInvWeight * vAccumulated_v3.x;
        v3DesiredPos.y = fInvWeight * vAccumulated_v3.y;
        v3DesiredPos.z = fInvWeight * vAccumulated_v3.z;
    }
    else
    {
        v3DesiredPos = v3Zero;
    }

    SetDesiredSpeedAndDirectionToPosition(fDeltaT, v3DesiredPos, TR_FAR_DISTANCE, 1.0f, 1.0f);
    ShouldIStrafe();
}

void cFielder::InitDesireFallThrough()
{
    m_pAvoidance->SetThingsToAvoid(0);
}

void cFielder::InitDesireGetInPosition()
{
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::DesireGetInPosition(float fDeltaT)
{
    nlVector3 v3FormationPosition;
    m_DesireCommonVars.bInPosition = GetFormationPosition(v3FormationPosition, -1.0f);
    if (m_DesireCommonVars.bInPosition)
    {
        v3FormationPosition = m_v3Position;
    }

    SetDesiredSpeedAndDirectionToPosition(fDeltaT, v3FormationPosition, TR_FAR_DISTANCE, 1.0f, 1.0f);
    ShouldIStrafe();

    if (g_pBall->m_pOwner != m_DesireCommonVars.pBallOwner || m_pTeam->m_pBallInterceptOrderedFielders[0] == this)
    {
        if ((m_DesireCommonVars.tAge.GetSeconds() > 0.5f) != false)
        {
            SetDesireDuration(0.0f, true);
        }
    }

    ShouldIWave();
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x33A0 | 0x80034124 | size: 0x244
 */
bool cFielder::InitDesireGetOpen()
{
    if (m_DesireCommonVars.pSBC == this)
    {
        if (m_sQueuedDesireParams.eDesireType == FIELDERDESIRE_GET_OPEN)
        {
            m_sQueuedDesireParams.fDuration = 0.0f;
            m_sQueuedDesireParams.eDesireType = FIELDERDESIRE_NEED_DESIRE;
            m_sQueuedDesireParams.opt1 = fvNotSet;
            m_sQueuedDesireParams.opt2 = fvNotSet;
        }
        return false;
    }

    nlVector3 v3FormationPosition;
    m_DesireCommonVars.bInPosition = GetFormationPosition(v3FormationPosition, -1.0f);
    if (m_DesireCommonVars.bInPosition)
    {
        v3FormationPosition = m_v3Position;
    }

    nlVector3 v3BestPosition = v3FormationPosition;
    const nlVector3* pTargetPosition;
    if (m_DesireCommonVars.pSBC != NULL)
    {
        pTargetPosition = &m_DesireCommonVars.pSBC->m_v3Position;
    }
    else
    {
        pTargetPosition = &g_pBall->m_v3Position;
    }

    nlVector3 v3TargetPosition = *pTargetPosition;
    v3TargetPosition.z = 0.0f;

    SetSpaceSearch(new (nlMalloc(sizeof(SSearchBestPass), 8, false)) SSearchBestPass(m_DesireCommonVars.pSBC, this, false, false));
    m_pSpaceSearch->m_bDebugOn = false;
    m_pSpaceSearch->FindBestPosition(v3BestPosition, v3FormationPosition, DIR_TOWARD_TARGET, &v3TargetPosition, 4.0f, 0x8000);

    m_DesireCommonVars.v3DesiredPosition.x = (1.0f - 0.95f) * v3FormationPosition.x + 0.95f * v3BestPosition.x;
    m_DesireCommonVars.v3DesiredPosition.y = (1.0f - 0.95f) * v3FormationPosition.y + 0.95f * v3BestPosition.y;
    m_DesireCommonVars.v3DesiredPosition.z = (1.0f - 0.95f) * v3FormationPosition.z + 0.95f * v3BestPosition.z;

    m_pAvoidance->SetThingsToAvoid(0x1F);

    return true;
}
#pragma dont_inline off

void cFielder::DesireGetOpen(float fDeltaT)
{
    cPlayer* pSBC = Fuzzy::GetStrategicBallCarrier(m_pTeam).mData.pPlayer;

    if (m_pBall != NULL)
    {
        SetDesireDuration(0.0f, true);
        return;
    }

    if (pSBC == NULL || pSBC != m_DesireCommonVars.pSBC || m_pTeam->m_pBallInterceptOrderedFielders[0] == this)
    {
        if ((m_DesireCommonVars.tAge.GetSeconds() > 0.5f) != false)
        {
            SetDesireDuration(0.0f, true);
        }
    }

    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_DesireCommonVars.v3DesiredPosition, TR_FAR_DISTANCE, 0.85f, 0.85f);
    ShouldIStrafe();
    ShouldIWave();
}

void cFielder::InitDesirePostWhistle()
{
    if (m_pBall != NULL)
    {
        ReleaseBall();
        g_pBall->ShootRelease(m_v3Velocity, SPINTYPE_NONE);
    }
    const float& fDelayToFinishAction = g_fPostWhistleDelayToFinishAction;
    m_DesireCommonVars.tMiscTimer.SetSeconds(fDelayToFinishAction);
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::DesirePostWhistle(float fDeltaT)
{
    if (g_pGame->IsGameplayOrOvertime())
    {
        SetDesireDuration(0.0f, true);
        return;
    }
    if (m_DesireCommonVars.tMiscTimer.m_uPackedTime == 0 || m_eActionState == ACTION_NEED_ACTION)
    {
        InitActionPostWhistle();
    }
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_v3Position, TR_FAR_DISTANCE, 1.0f, 1.0f);
    ShouldIStrafe();
}

bool cFielder::InitDesireOneTimerFromIdle(bool bVolleyPass, bool bIsChipShot)
{
    float fBallContactTime;
    const LooseBallContactAnimInfo* pBestBallContactAnimInfo = GetOneTimerBallContactAnimInfo(
        m_aActualFacingDirection,
        m_v3Position,
        m_pTeam->GetOtherNet()->m_v3NetLocation,
        false,
        bVolleyPass);

    m_DesireOneTimerVars.nOneTimerAnim = pBestBallContactAnimInfo->nAnimID;
    const cSAnim* contactAnim = m_pAnimInventory->GetAnim(pBestBallContactAnimInfo->nAnimID);
    m_DesireOneTimerVars.fOneTimerAnimTime = pBestBallContactAnimInfo->fAnimContactFrame / (float)contactAnim->m_nNumKeys;

    if (!DoLooseBallContactFromIdle(
            m_DesireOneTimerVars.v3DesiredPosition,
            m_DesireOneTimerVars.fDesiredTime,
            m_DesireOneTimerVars.v3BallPosition,
            fBallContactTime,
            m_aActualFacingDirection,
            pBestBallContactAnimInfo))
    {
        return false;
    }

    m_DesireOneTimerVars.aDesiredFacingDirection = m_aActualFacingDirection;
    m_DesireOneTimerVars.bIsChipShot = bIsChipShot;
    m_DesireOneTimerVars.bVolleyPassReceive = bVolleyPass;

    if (m_DesireOneTimerVars.fDesiredTime < 0.0f)
    {
        const cSAnim* pOneTimerAnim = m_pAnimInventory->GetAnim(m_DesireOneTimerVars.nOneTimerAnim);
        float fAnimTimeInSecs = m_DesireOneTimerVars.fOneTimerAnimTime * ((float)pOneTimerAnim->m_nNumKeys / 30.0f);
        float fPlaybackScale = fAnimTimeInSecs / (fAnimTimeInSecs + m_DesireOneTimerVars.fDesiredTime);

        if (fPlaybackScale > 1.5f)
        {
            return false;
        }
    }

    SetDesire(FIELDERDESIRE_ONETIMER, 0.5f);
    SetDesireDuration(3.0f, false);
    InitActionWait();
    m_eDesireSubState = 0;
    SetNoPickUpTime(3.0f);
    g_pBall->SetPassTargetTimer(fBallContactTime);
    m_pAvoidance->SetThingsToAvoid(0);
    return true;
}

/**
 * Offset/Address/Size: 0x30B4 | 0x80033E38 | size: 0x2EC
 */
bool cFielder::InitDesireOneTimerFromRun(unsigned short aFutureFacingDirection, const nlVector3& v3FuturePosition, const nlVector3& v3PassIntercept, bool bVolleyPassReceive, bool bIsChipShot)
{
    extern float g_fSimulationTick;

    float fBallContactTime;

    const LooseBallContactAnimInfo* pBestBallContactAnimInfo = GetOneTimerBallContactAnimInfo(
        aFutureFacingDirection, v3FuturePosition, m_pTeam->GetOtherNet()->m_v3NetLocation, true, bVolleyPassReceive);

    m_DesireOneTimerVars.nOneTimerAnim = pBestBallContactAnimInfo->nAnimID;

    const cSAnim* contactAnim = m_pAnimInventory->GetAnim(pBestBallContactAnimInfo->nAnimID);
    m_DesireOneTimerVars.fOneTimerAnimTime = pBestBallContactAnimInfo->fAnimContactFrame / (float)contactAnim->m_nNumKeys;

    bool bFoundContact;
    if (bVolleyPassReceive)
    {
        bFoundContact = DoLooseBallContactFromRunVolley(
            m_DesireOneTimerVars.v3DesiredPosition,
            m_DesireOneTimerVars.fDesiredTime,
            m_DesireOneTimerVars.v3BallPosition,
            fBallContactTime,
            pBestBallContactAnimInfo,
            v3PassIntercept);
    }
    else
    {
        bFoundContact = DoLooseBallContactFromRun(
            m_DesireOneTimerVars.v3DesiredPosition,
            m_DesireOneTimerVars.fDesiredTime,
            m_DesireOneTimerVars.v3BallPosition,
            fBallContactTime,
            pBestBallContactAnimInfo,
            v3PassIntercept);
    }

    if (!bFoundContact)
    {
        return false;
    }

    m_DesireOneTimerVars.aDesiredFacingDirection = m_aActualFacingDirection;
    m_DesireOneTimerVars.bIsChipShot = bIsChipShot;
    m_DesireOneTimerVars.bVolleyPassReceive = bVolleyPassReceive;

    if (m_DesireOneTimerVars.fDesiredTime > (2.0f * g_fSimulationTick))
    {
        m_DesireOneTimerVars.fDesiredTime -= g_fSimulationTick;

        SetDesire(FIELDERDESIRE_ONETIMER, 0.5f);

        m_eDesireSubState = 0;
        InitActionRunning();

        nlVector3 v3Me2DesiredPosition;
        nlVec3Set(*(nlVector3*)&v3Me2DesiredPosition,
            m_DesireOneTimerVars.v3DesiredPosition.x - m_v3Position.x,
            m_DesireOneTimerVars.v3DesiredPosition.y - m_v3Position.y,
            m_DesireOneTimerVars.v3DesiredPosition.z - m_v3Position.z);

        unsigned short aDesiredAngle = (unsigned short)(int)(10430.378f * nlATan2f(v3Me2DesiredPosition.y, v3Me2DesiredPosition.x));

        s16 angleDiff = aDesiredAngle - m_aActualFacingDirection;
        int absDiff = angleDiff;
        if (angleDiff < 0)
            absDiff = -angleDiff;

        if ((u16)absDiff < 0x4000)
        {
            float fSpeed = nlSqrt(v3Me2DesiredPosition.x * v3Me2DesiredPosition.x + v3Me2DesiredPosition.y * v3Me2DesiredPosition.y, true) / m_DesireOneTimerVars.fDesiredTime;
            m_fDesiredSpeed = fSpeed;
            m_fActualSpeed = fSpeed;
            m_aDesiredFacingDirection = aDesiredAngle;
            m_aActualFacingDirection = aDesiredAngle;
            m_aDesiredMovementDirection = m_aDesiredFacingDirection;
        }
        else
        {
            m_fActualSpeed = 0.0f;
        }
    }
    else
    {
        const cSAnim* pOneTimerAnim = m_pAnimInventory->GetAnim(m_DesireOneTimerVars.nOneTimerAnim);
        float fAnimTimeInSecs = m_DesireOneTimerVars.fOneTimerAnimTime * ((float)pOneTimerAnim->m_nNumKeys / 30.0f);
        float fPlaybackScale = fAnimTimeInSecs / (fAnimTimeInSecs + m_DesireOneTimerVars.fDesiredTime);

        if (fPlaybackScale > 1.5f)
        {
            return false;
        }

        SetDesire(FIELDERDESIRE_ONETIMER, 0.5f);
        m_eDesireSubState = 1;

        SetFacingDirection(m_DesireOneTimerVars.aDesiredFacingDirection);

        InitActionOneTimer(
            m_DesireOneTimerVars.nOneTimerAnim,
            m_DesireOneTimerVars.v3DesiredPosition,
            m_DesireOneTimerVars.fOneTimerAnimTime,
            m_DesireOneTimerVars.bIsChipShot);

        m_pCurrentAnimController->m_fPlaybackSpeedScale = fPlaybackScale;
    }

    SetDesireDuration(3.0f, false);
    SetNoPickUpTime(3.0f);
    g_pBall->SetPassTargetTimer(fBallContactTime);
    m_pAvoidance->SetThingsToAvoid(0);

    return true;
}

/**
 * Offset/Address/Size: 0x2E60 | 0x80033BE4 | size: 0x254
 */
void cFielder::DesireOneTimer(float fDeltaT)
{
    cFielder* fp = this;

    nlVector3 vBallDir;
    vBallDir.Sub2D(fp->m_DesireOneTimerVars.v3BallPosition, g_pBall->m_v3Position);
    float invLen = nlRecipSqrt(vBallDir.GetLengthSq2D(), true);
    float targetDirY = invLen * vBallDir.y;
    vBallDir.y = invLen * vBallDir.x;

    cBall* pBall = g_pBall;
    invLen = nlRecipSqrt(pBall->m_v3Velocity.x * pBall->m_v3Velocity.x + pBall->m_v3Velocity.y * pBall->m_v3Velocity.y, true);

    float ballDirX;
    float ballDirY;
    ballDirY = invLen * pBall->m_v3Velocity.y;
    ballDirX = invLen * pBall->m_v3Velocity.x;

    if (fp->m_pBall == NULL && fp->m_eDesireSubState != 1)
    {
        invLen = vBallDir.y * ballDirX + targetDirY * ballDirY;
        if (invLen < 0.98f)
        {
            fp->ClearPassTargetIfAmThePassTarget();
            fp->SetDesireDuration(0.0f, true);
            return;
        }
    }

    fp->m_DesireOneTimerVars.fDesiredTime -= fDeltaT;

    switch (fp->m_eDesireSubState)
    {
    case 0:
    {
        if (fp->m_DesireOneTimerVars.fDesiredTime <= 0.0f)
        {
            float yToTarget = fp->m_v3Position.y - fp->m_DesireOneTimerVars.v3DesiredPosition.y;
            float xToTarget = fp->m_v3Position.x - fp->m_DesireOneTimerVars.v3DesiredPosition.x;

            if (xToTarget * xToTarget + yToTarget * yToTarget > 4.0f)
            {
                fp->ClearPassTargetIfAmThePassTarget();
                fp->SetDesireDuration(0.0f, true);
                return;
            }

            fp->SetFacingDirection(fp->m_DesireOneTimerVars.aDesiredFacingDirection);
            fp->InitActionOneTimer(
                fp->m_DesireOneTimerVars.nOneTimerAnim,
                fp->m_DesireOneTimerVars.v3DesiredPosition,
                fp->m_DesireOneTimerVars.fOneTimerAnimTime,
                fp->m_DesireOneTimerVars.bIsChipShot);
            fp->m_eDesireSubState = 1;

            cSAnim* pAnim = fp->m_pAnimInventory->GetAnim(fp->m_DesireOneTimerVars.nOneTimerAnim);
            float oneTimerTime = fp->m_DesireOneTimerVars.fOneTimerAnimTime * ((float)pAnim->m_nNumKeys / 30.0f);
            float totalTime = oneTimerTime + fp->m_DesireOneTimerVars.fDesiredTime;

            if (oneTimerTime > 0.0f && totalTime > 0.0f)
            {
                fp->m_pCurrentAnimController->m_fPlaybackSpeedScale = oneTimerTime / totalTime;
            }
        }
        break;
    }

    case 1:
    {
        if (fp->IsActionDone())
        {
            fp->SetDesireDuration(0.0f, true);
        }
        break;
    }

    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0x2D08 | 0x80033A8C | size: 0x158
 */
void cFielder::InitDesireReceivePassFromIdle(const LooseBallContactAnimInfo* pAnimInfo, unsigned short aAngle, bool bVolley)
{
    m_DesireReceivePassSharedVars.aDesiredFacingDirection = aAngle;
    m_DesireReceivePassSharedVars.nReceivePassAnim = pAnimInfo->nAnimID;

    cSAnim* pAnim = m_pAnimInventory->GetAnim(pAnimInfo->nAnimID);
    unsigned int nNumKeys = pAnim->m_nNumKeys;

    float fDesiredTime;

    m_DesireReceivePassSharedVars.fReceivePassAnimTime = pAnimInfo->fAnimContactFrame / (float)nNumKeys;
    m_DesireReceivePassSharedVars.iAttemptOneTouchShot = 0;
    m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = false;
    m_DesireReceivePassSharedVars.iAttemptOneTouchPass = 0;
    m_DesireReceivePassSharedVars.bVolleyPassReceive = bVolley;
    m_DesireReceivePassSharedVars.pOneTouchPassTarget = NULL;

    bool savedTiltForce = g_pBall->m_pPhysicsBall->m_bUseTiltForce;
    g_pBall->m_pPhysicsBall->m_bUseTiltForce = false;

    bool result = DoLooseBallContactFromIdle(
        m_DesireReceivePassSharedVars.v3DesiredPosition,
        m_DesireReceivePassSharedVars.fDesiredTime,
        m_DesireReceivePassSharedVars.v3BallPosition,
        fDesiredTime,
        aAngle,
        pAnimInfo);

    g_pBall->m_pPhysicsBall->m_bUseTiltForce = savedTiltForce;

    if (result)
    {
        SetDesire(FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE, 0.5f);
        SetDesireDuration(3.0f, false);
        InitActionIdleTurn(aAngle);
        m_eDesireSubState = 0;
        SetNoPickUpTime(3.0f);
        g_pBall->SetPassTargetTimer(fDesiredTime);
        g_pBall->SetPassTarget(this, m_DesireReceivePassSharedVars.v3BallPosition, bVolley);
        m_DesireCommonVars.fMisc = fDesiredTime;
        m_pAvoidance->SetThingsToAvoid(0);
    }
}

/**
 * Offset/Address/Size: 0x2080 | 0x80032E04 | size: 0xC88
 */
void cFielder::DesireReceivePassFromIdle(float fDeltaT)
{
    nlVector3 vBallDir;
    vBallDir.Sub2D(m_DesireReceivePassSharedVars.v3BallPosition, g_pBall->m_v3Position);
    float invDist = nlRecipSqrt(vBallDir.GetLengthSq2D(), true);
    float normY = invDist * vBallDir.y;
    vBallDir.y = invDist * vBallDir.x;

    cBall* pBall = g_pBall;
    invDist = nlRecipSqrt(pBall->m_v3Velocity.x * pBall->m_v3Velocity.x + pBall->m_v3Velocity.y * pBall->m_v3Velocity.y, true);

    float ballVelNormX;
    float ballVelNormY;
    ballVelNormY = invDist * pBall->m_v3Velocity.y;
    ballVelNormX = invDist * pBall->m_v3Velocity.x;

    if (m_pBall == NULL && m_eDesireSubState != 2)
    {
        invDist = vBallDir.y * ballVelNormX + normY * ballVelNormY;
        float fDot = invDist;
        if (fDot < 0.98f || g_pBall->m_pOwner != NULL)
        {
            ClearPassTargetIfAmThePassTarget();
            SetDesireDuration(0.0f, true);
            return;
        }
    }

    if (GetGlobalPad() != NULL)
    {
        if (GetGlobalPad()->JustPressed(PAD_USE, true))
        {
            if (!IsPlayingPowerupAnim())
            {
                UseTeamPowerup(NULL);
            }
        }
        else if (GetGlobalPad()->JustPressed(PAD_TOGGLE_POWERUP, true))
        {
            m_pTeam->TogglePowerup(false);
        }

        if (m_pBall != NULL)
        {
            if (m_pController != NULL && m_pController->IsTurboPressed())
            {
                SetDesiredSpeed(((FielderTweaks*)m_pTweaks)->fRunningWBSpeed, ((FielderTweaks*)m_pTweaks)->fRunningWBTurboSpeedLevel1);
            }
            else
            {
                SetDesiredSpeed(m_pTweaks->fJoggingSpeed, ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed);
            }

            if (GetGlobalPad()->JustPressed(PAD_SHOOT, true))
            {
                SetAttemptOneTouchShot();
                m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = true;
            }
            else if (GetGlobalPad()->JustPressed(PAD_PASS, true))
            {
                SetAttemptOneTouchPass();
            }
        }
        else
        {
            if (GetGlobalPad()->JustPressed(PAD_SHOOT, true))
            {
                SetAttemptOneTouchShot();
            }
            else if (GetGlobalPad()->JustPressed(PAD_HIT, true))
            {
                if (m_eDesireSubState != 1)
                {
                    InitActionHit(DoFindBestHitTarget());
                    return;
                }
            }
            else if (GetGlobalPad()->JustPressed(PAD_PASS, true))
            {
                SetAttemptOneTouchPass();
            }
        }
    }

    m_DesireReceivePassSharedVars.fDesiredTime -= fDeltaT;

    ShotMeter* pShotMeter;

    if (m_eDesireSubState == 0 || m_eDesireSubState == 1)
    {
        if (GetGlobalPad() == NULL && m_DesireCommonVars.tMiscTimer.m_uPackedTime == 0)
        {
            if (DoAIReceivePassActionSelection())
            {
                const float& fActionRethinkTime = 99999.9f;
                m_DesireCommonVars.tMiscTimer.SetSeconds(fActionRethinkTime);
            }
            else
            {
                float actionRethinkTime = (m_DesireCommonVars.fMisc / 3.0f) - 0.1f;
                m_DesireCommonVars.tMiscTimer.SetSeconds((0.1f >= actionRethinkTime) ? 0.1f : actionRethinkTime);
            }
        }

        if (m_DesireReceivePassSharedVars.iAttemptOneTouchShot != 0
            && !m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot)
        {
            bool bIsChipShot = m_DesireReceivePassSharedVars.iAttemptOneTouchShot == 2;

            if (InitDesireOneTimerFromIdle(m_DesireReceivePassSharedVars.bVolleyPassReceive, bIsChipShot))
            {
                return;
            }

            if (m_DesireReceivePassSharedVars.bVolleyPassReceive)
            {
                g_pGame->DoPerfectPassSlowDown();
            }

            m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = true;
        }
    }

    switch (m_eDesireSubState)
    {
    case 0:
    {
        if (IsActionDone() || m_DesireReceivePassSharedVars.fDesiredTime <= 0.0f)
        {
            m_eDesireSubState = 1;
            InitActionWait();
        }
        return;
    }

    case 1:
    {
        if (m_DesireReceivePassSharedVars.fDesiredTime <= 0.0f)
        {
            SetFacingDirection(m_DesireReceivePassSharedVars.aDesiredFacingDirection);
            InitActionReceivePass(
                m_DesireReceivePassSharedVars.nReceivePassAnim,
                m_DesireReceivePassSharedVars.v3DesiredPosition,
                m_DesireReceivePassSharedVars.fReceivePassAnimTime);
            m_eDesireSubState = 2;

            if (m_DesireReceivePassSharedVars.bVolleyPassReceive)
            {
                m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = false;
            }

            cSAnim* pAnim = m_pAnimInventory->GetAnim(m_DesireReceivePassSharedVars.nReceivePassAnim);
            float fAnimTimeInSecs = m_DesireReceivePassSharedVars.fReceivePassAnimTime * ((float)pAnim->m_nNumKeys / 30.0f);
            float fTimeToIntercept = fAnimTimeInSecs + m_DesireReceivePassSharedVars.fDesiredTime;

            if (fAnimTimeInSecs > 0.0f && fTimeToIntercept > 0.0f)
            {
                m_pCurrentAnimController->m_fPlaybackSpeedScale = fAnimTimeInSecs / fTimeToIntercept;
            }
        }
        return;
    }

    case 2:
    {
        if (m_pBall != NULL)
        {
            if (m_DesireReceivePassSharedVars.iAttemptOneTouchShot != 0)
            {
                if (m_DesireReceivePassSharedVars.bVolleyPassReceive && m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot)
                {
                    if ((float)fabs(m_DesireReceivePassSharedVars.fReceivePassAnimTime - m_pCurrentAnimController->m_fTime) <= 0.1f)
                    {
                        InitActionLateOneTimerFromVolley();
                        return;
                    }
                }

                if (!m_DesireReceivePassSharedVars.bVolleyPassReceive)
                {
                    if (m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot)
                    {
                        if (GetGlobalPad() != NULL && GetGlobalPad()->IsPressed(PAD_SHOOT, true))
                        {
                            if (!ShouldStartCrossBlend(0x1A))
                            {
                                return;
                            }

                            DoResetShotMeter(0.0f);
                            SetDesireDuration(0.0f, true);
                            return;
                        }

                        DoResetShotMeter(0.0f);
                        pShotMeter = m_pShotMeter;
                        pShotMeter->CalcOneTimerValue(this, UsePerfectPass());
                        InitDesire(FIELDERDESIRE_FINISH_ACTION, 0.5f, -1.0f, fvNotSet, fvNotSet);
                        InitActionShot(m_DesireReceivePassSharedVars.iAttemptOneTouchShot == 2);
                        return;
                    }

                    if (GetGlobalPad() == NULL)
                    {
                        return;
                    }

                    if (!GetGlobalPad()->IsPressed(PAD_SHOOT, true))
                    {
                        return;
                    }

                    if (!ShouldStartCrossBlend(0x1A))
                    {
                        return;
                    }

                    DoResetShotMeter(0.0f);
                    SetDesireDuration(0.0f, true);
                    return;
                }
                else if (ShouldStartCrossBlend(0x1A))
                {
                    if (GetGlobalPad() != NULL && GetGlobalPad()->IsPressed(PAD_SHOOT, true))
                    {
                        DoResetShotMeter(0.0f);
                        SetDesireDuration(0.0f, true);
                    }
                    else
                    {
                        DoResetShotMeter(0.0f);
                        pShotMeter = m_pShotMeter;
                        pShotMeter->CalcOneTimerValue(this, UsePerfectPass());
                        InitDesire(FIELDERDESIRE_FINISH_ACTION, 0.5f, -1.0f, fvNotSet, fvNotSet);
                        InitActionShot(m_DesireReceivePassSharedVars.iAttemptOneTouchShot == 2);
                    }
                }
            }
            else if (m_DesireReceivePassSharedVars.iAttemptOneTouchPass != 0)
            {
                bool bStrongOneTouchPass = m_DesireReceivePassSharedVars.iAttemptOneTouchPass == 2;

                if (m_DesireReceivePassSharedVars.bVolleyPassReceive)
                {
                    if ((float)fabs(m_DesireReceivePassSharedVars.fReceivePassAnimTime - m_pCurrentAnimController->m_fTime) <= 0.1f)
                    {
                        InitActionOneTouchPassFromVolley(m_DesireReceivePassSharedVars.pOneTouchPassTarget);
                        return;
                    }
                }

                if (!m_DesireReceivePassSharedVars.bVolleyPassReceive || ShouldStartCrossBlend(0x1A))
                {
                    InitActionPass(m_DesireReceivePassSharedVars.pOneTouchPassTarget, bStrongOneTouchPass, false);
                    return;
                }
            }
            else
            {
                if (!m_DesireReceivePassSharedVars.bVolleyPassReceive || ShouldStartCrossBlend(0x1A))
                {
                    SetDesireDuration(0.0f, true);
                    InitActionRunningWB(true);
                    return;
                }
            }
        }
        else
        {
            if (m_DesireReceivePassSharedVars.iAttemptOneTouchShot != 0)
            {
                m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = true;
            }
        }
        if (ShouldStartCrossBlend(0x1A))
        {
            SetDesireDuration(0.0f, true);
        }
        break;
    }

    default:
        return;
    }
}

/**
 * Offset/Address/Size: 0x1DE8 | 0x80032B6C | size: 0x298
 */
void cFielder::InitDesireReceivePassFromRun(const LooseBallContactAnimInfo* pAnimInfo, const nlVector3& rv3Velocity, bool bVolley, const nlVector3& v3PassIntercept)
{
    extern float g_fSimulationTick;

    float fDesiredTime;

    SetVelocity(rv3Velocity);
    SetFacingDirection((unsigned short)(int)(10430.378f * nlATan2f(rv3Velocity.y, rv3Velocity.x)));

    m_DesireReceivePassSharedVars.aDesiredFacingDirection = m_aActualFacingDirection;
    m_DesireReceivePassSharedVars.nReceivePassAnim = pAnimInfo->nAnimID;

    cSAnim* pAnim = m_pAnimInventory->GetAnim(pAnimInfo->nAnimID);
    unsigned int nNumKeys = pAnim->m_nNumKeys;

    m_DesireReceivePassSharedVars.fReceivePassAnimTime = pAnimInfo->fAnimContactFrame / (float)nNumKeys;
    m_DesireReceivePassSharedVars.iAttemptOneTouchShot = 0;
    m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = false;
    m_DesireReceivePassSharedVars.iAttemptOneTouchPass = 0;
    m_DesireReceivePassSharedVars.bVolleyPassReceive = bVolley;
    m_DesireReceivePassSharedVars.pOneTouchPassTarget = NULL;

    bool savedTiltForce = g_pBall->m_pPhysicsBall->m_bUseTiltForce;
    g_pBall->m_pPhysicsBall->m_bUseTiltForce = false;

    bool result = DoLooseBallContactFromRun(
        m_DesireReceivePassSharedVars.v3DesiredPosition,
        m_DesireReceivePassSharedVars.fDesiredTime,
        m_DesireReceivePassSharedVars.v3BallPosition,
        fDesiredTime,
        pAnimInfo,
        v3PassIntercept);

    g_pBall->m_pPhysicsBall->m_bUseTiltForce = savedTiltForce;

    if (result)
    {
        SetDesire(FIELDERDESIRE_RECEIVE_PASS_FROM_RUN, 0.5f);
        SetDesireDuration(3.0f, false);

        if (m_DesireReceivePassSharedVars.fDesiredTime > (2.0f * g_fSimulationTick))
        {
            nlVector3 v3DesiredDelta;

            m_DesireReceivePassSharedVars.fDesiredTime -= g_fSimulationTick;
            m_eDesireSubState = 0;

            InitActionRunning();
            SetRunningAnimState(0.1f);

            nlVec3Set(*(nlVector3*)&v3DesiredDelta,
                m_DesireReceivePassSharedVars.v3DesiredPosition.x - m_v3Position.x,
                m_DesireReceivePassSharedVars.v3DesiredPosition.y - m_v3Position.y,
                m_DesireReceivePassSharedVars.v3DesiredPosition.z - m_v3Position.z);
            float fSpeed = nlGetLength2D(v3DesiredDelta.x, v3DesiredDelta.y) / m_DesireReceivePassSharedVars.fDesiredTime;

            m_fDesiredSpeed = fSpeed;
            m_fActualSpeed = fSpeed;

            unsigned short aDesiredAngle = (unsigned short)(int)(10430.378f * nlATan2f(v3DesiredDelta.y, v3DesiredDelta.x));
            m_aDesiredFacingDirection = aDesiredAngle;
            m_aActualFacingDirection = aDesiredAngle;
            m_aDesiredMovementDirection = m_aDesiredFacingDirection;
        }
        else
        {
            InitActionReceivePass(
                m_DesireReceivePassSharedVars.nReceivePassAnim,
                m_DesireReceivePassSharedVars.v3DesiredPosition,
                m_DesireReceivePassSharedVars.fReceivePassAnimTime);

            m_eDesireSubState = 1;

            cSAnim* pReceivePassAnim = m_pAnimInventory->GetAnim(m_DesireReceivePassSharedVars.nReceivePassAnim);
            m_pCurrentAnimController->m_fPlaybackSpeedScale = (m_DesireReceivePassSharedVars.fReceivePassAnimTime * ((float)pReceivePassAnim->m_nNumKeys / 30.0f)) / fDesiredTime;
        }

        SetNoPickUpTime(3.0f);
        g_pBall->SetPassTargetTimer(fDesiredTime);
        g_pBall->SetPassTarget(this, m_DesireReceivePassSharedVars.v3BallPosition, bVolley);

        m_DesireCommonVars.tMiscTimer.m_uPackedTime = 0;
        m_DesireCommonVars.fMisc = fDesiredTime;

        m_pAvoidance->SetThingsToAvoid(0);
    }
}

/**
 * Offset/Address/Size: 0x130C | 0x80032090 | size: 0xADC
 */
void cFielder::DesireReceivePassFromRun(float fDeltaT)
{
    nlVector3 vBallDir;
    vBallDir.Sub2D(m_DesireReceivePassSharedVars.v3BallPosition, g_pBall->m_v3Position);
    float invDist = nlRecipSqrt(vBallDir.GetLengthSq2D(), true);
    float normY = invDist * vBallDir.y;
    vBallDir.y = invDist * vBallDir.x;

    cBall* pBall = g_pBall;
    invDist = nlRecipSqrt(pBall->m_v3Velocity.x * pBall->m_v3Velocity.x + pBall->m_v3Velocity.y * pBall->m_v3Velocity.y, true);

    float ballVelNormX;
    float ballVelNormY;
    ballVelNormY = invDist * pBall->m_v3Velocity.y;
    ballVelNormX = invDist * pBall->m_v3Velocity.x;

    if (m_pBall == NULL && m_eDesireSubState != 1)
    {
        invDist = vBallDir.y * ballVelNormX + normY * ballVelNormY;
        float fDot = invDist;
        if (fDot < 0.98f || g_pBall->m_pOwner != NULL)
        {
            ClearPassTargetIfAmThePassTarget();
            SetDesireDuration(0.0f, true);
            return;
        }
    }

    if (GetGlobalPad() != NULL)
    {
        if (GetGlobalPad()->JustPressed(PAD_USE, true))
        {
            if (!IsPlayingPowerupAnim())
            {
                UseTeamPowerup(NULL);
            }
        }
        else if (GetGlobalPad()->JustPressed(PAD_TOGGLE_POWERUP, true))
        {
            m_pTeam->TogglePowerup(false);
        }

        if (m_pBall != NULL)
        {
            if (m_pController != NULL && m_pController->IsTurboPressed())
            {
                SetDesiredSpeed(((FielderTweaks*)m_pTweaks)->fRunningWBSpeed, ((FielderTweaks*)m_pTweaks)->fRunningWBTurboSpeedLevel1);
            }
            else
            {
                SetDesiredSpeed(m_pTweaks->fJoggingSpeed, ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed);
            }

            if (GetGlobalPad()->JustPressed(PAD_SHOOT, true))
            {
                SetAttemptOneTouchShot();
                m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = true;
            }
            else if (GetGlobalPad()->JustPressed(PAD_PASS, true))
            {
                SetAttemptOneTouchPass();
            }
        }
        else
        {
            if (GetGlobalPad()->JustPressed(PAD_SHOOT, true))
            {
                SetAttemptOneTouchShot();
            }
            else if (GetGlobalPad()->JustPressed(PAD_HIT, true))
            {
                if (m_eDesireSubState != 1)
                {
                    InitActionHit(DoFindBestHitTarget());
                    return;
                }
            }
            else if (GetGlobalPad()->JustPressed(PAD_PASS, true))
            {
                SetAttemptOneTouchPass();
            }
        }
    }

    m_DesireReceivePassSharedVars.fDesiredTime -= fDeltaT;

    switch (m_eDesireSubState)
    {
    case 0:
    {
        if (GetGlobalPad() == NULL && m_DesireCommonVars.tMiscTimer.m_uPackedTime == 0)
        {
            if (DoAIReceivePassActionSelection())
            {
                const float& fActionRethinkTime = 99999.9f;
                m_DesireCommonVars.tMiscTimer.SetSeconds(fActionRethinkTime);
            }
            else
            {
                float actionRethinkTime = (m_DesireCommonVars.fMisc / 3.0f) - 0.1f;
                m_DesireCommonVars.tMiscTimer.SetSeconds((0.1f >= actionRethinkTime) ? 0.1f : actionRethinkTime);
            }
        }

        if (m_DesireReceivePassSharedVars.iAttemptOneTouchShot != 0
            && !m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot)
        {
            bool bStrongOneTouch = m_DesireReceivePassSharedVars.iAttemptOneTouchShot == 2;

            bool bSuccess = InitDesireOneTimerFromRun(
                m_aActualFacingDirection,
                m_DesireReceivePassSharedVars.v3DesiredPosition,
                m_DesireReceivePassSharedVars.v3BallPosition,
                m_DesireReceivePassSharedVars.bVolleyPassReceive,
                bStrongOneTouch);

            if (bSuccess)
            {
                return;
            }

            if (m_DesireReceivePassSharedVars.bVolleyPassReceive)
            {
                g_pGame->DoPerfectPassSlowDown();
            }

            m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = true;
        }

        if (m_DesireReceivePassSharedVars.fDesiredTime <= 0.0f)
        {
            float yToTarget = m_v3Position.y - m_DesireReceivePassSharedVars.v3DesiredPosition.y;
            float xToTarget = m_v3Position.x - m_DesireReceivePassSharedVars.v3DesiredPosition.x;

            if ((xToTarget * xToTarget) + (yToTarget * yToTarget) > 4.0f)
            {
                ClearPassTargetIfAmThePassTarget();
                SetDesireDuration(0.0f, true);
                return;
            }

            SetFacingDirection(m_DesireReceivePassSharedVars.aDesiredFacingDirection);
            InitActionReceivePass(
                m_DesireReceivePassSharedVars.nReceivePassAnim,
                m_DesireReceivePassSharedVars.v3DesiredPosition,
                m_DesireReceivePassSharedVars.fReceivePassAnimTime);
            m_eDesireSubState = 1;

            if (m_DesireReceivePassSharedVars.bVolleyPassReceive)
            {
                m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = false;
            }

            cSAnim* pAnim = m_pAnimInventory->GetAnim(m_DesireReceivePassSharedVars.nReceivePassAnim);
            float fAnimTimeInSecs = m_DesireReceivePassSharedVars.fReceivePassAnimTime * ((float)pAnim->m_nNumKeys / 30.0f);
            float fTimeToIntercept = fAnimTimeInSecs + m_DesireReceivePassSharedVars.fDesiredTime;

            if (fAnimTimeInSecs > 0.0f && fTimeToIntercept > 0.0f)
            {
                m_pCurrentAnimController->m_fPlaybackSpeedScale = fAnimTimeInSecs / fTimeToIntercept;
            }
        }
        return;
    }

    case 1:
    {
        if (m_pBall != NULL)
        {
            if (m_DesireReceivePassSharedVars.iAttemptOneTouchShot != 0)
            {
                if (m_DesireReceivePassSharedVars.bVolleyPassReceive && m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot)
                {
                    if ((float)fabs(m_DesireReceivePassSharedVars.fReceivePassAnimTime - m_pCurrentAnimController->m_fTime) <= 0.1f)
                    {
                        InitActionLateOneTimerFromVolley();
                        return;
                    }
                }

                if (!m_DesireReceivePassSharedVars.bVolleyPassReceive)
                {
                    if (m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot)
                    {
                        if (GetGlobalPad() != NULL && GetGlobalPad()->IsPressed(PAD_SHOOT, true))
                        {
                            if (!ShouldStartCrossBlend(0x1A))
                            {
                                return;
                            }

                            DoResetShotMeter(0.0f);
                            SetDesireDuration(0.0f, true);
                            return;
                        }

                        DoResetShotMeter(0.0f);
                        ShotMeter* pShotMeter = m_pShotMeter;
                        pShotMeter->CalcOneTimerValue(this, UsePerfectPass());
                        InitDesire(FIELDERDESIRE_FINISH_ACTION, 0.5f, -1.0f, fvNotSet, fvNotSet);
                        InitActionShot(m_DesireReceivePassSharedVars.iAttemptOneTouchShot == 2);
                        return;
                    }

                    if (GetGlobalPad() == NULL)
                    {
                        return;
                    }

                    if (!GetGlobalPad()->IsPressed(PAD_SHOOT, true))
                    {
                        return;
                    }

                    if (!ShouldStartCrossBlend(0x1A))
                    {
                        return;
                    }

                    DoResetShotMeter(0.0f);
                    SetDesireDuration(0.0f, true);
                    return;
                }
                else if (ShouldStartCrossBlend(0x1A))
                {
                    if (GetGlobalPad() != NULL && GetGlobalPad()->IsPressed(PAD_SHOOT, true))
                    {
                        DoResetShotMeter(0.0f);
                        SetDesireDuration(0.0f, true);
                    }
                    else
                    {
                        DoResetShotMeter(0.0f);
                        ShotMeter* pShotMeter = m_pShotMeter;
                        pShotMeter->CalcOneTimerValue(this, UsePerfectPass());
                        InitDesire(FIELDERDESIRE_FINISH_ACTION, 0.5f, -1.0f, fvNotSet, fvNotSet);
                        InitActionShot(m_DesireReceivePassSharedVars.iAttemptOneTouchShot == 2);
                    }
                }
            }
            else
            {
                if (m_DesireReceivePassSharedVars.iAttemptOneTouchPass != 0)
                {
                    bool bStrongOneTouchPass = m_DesireReceivePassSharedVars.iAttemptOneTouchPass == 2;

                    if (m_DesireReceivePassSharedVars.bVolleyPassReceive)
                    {
                        if ((float)fabs(m_DesireReceivePassSharedVars.fReceivePassAnimTime - m_pCurrentAnimController->m_fTime) <= 0.1f)
                        {
                            InitActionOneTouchPassFromVolley(m_DesireReceivePassSharedVars.pOneTouchPassTarget);
                            return;
                        }
                    }

                    if (!m_DesireReceivePassSharedVars.bVolleyPassReceive || ShouldStartCrossBlend(0x1A))
                    {
                        InitActionPass(m_DesireReceivePassSharedVars.pOneTouchPassTarget, bStrongOneTouchPass, false);
                        return;
                    }
                }
            }
        }
        else
        {
            if (m_DesireReceivePassSharedVars.iAttemptOneTouchShot != 0)
            {
                m_DesireReceivePassSharedVars.bFailedToInitOneTouchShot = true;
            }
        }
        if (ShouldStartCrossBlend(0x1A))
        {
            SetDesireDuration(0.0f, true);
        }
        break;
    }

    default:
        return;
    }
}

/**
 * Offset/Address/Size: 0xEE4 | 0x80031C68 | size: 0x428
 */
bool cFielder::InitDesireRunToNet()
{
    if (m_pBall == NULL)
    {
        if (m_sQueuedDesireParams.eDesireType == FIELDERDESIRE_RUN_TO_NET)
        {
            m_sQueuedDesireParams.fDuration = 0.0f;
            m_sQueuedDesireParams.eDesireType = FIELDERDESIRE_NEED_DESIRE;
            m_sQueuedDesireParams.opt1 = fvNotSet;
            m_sQueuedDesireParams.opt2 = fvNotSet;
        }
        return 0;
    }

    SpaceSearch* pSpaceSearch = new (nlMalloc(sizeof(SSearchRunToNet), 8, false)) SSearchRunToNet(this);
    SetSpaceSearch(pSpaceSearch);

    m_pSpaceSearch->m_bDebugOn = false;

    nlVector3 v3BestPosition;
    m_pSpaceSearch->FindBestPosition(v3BestPosition, m_v3Position, DIR_NONE, NULL, 4.0f, 0x8000);

    nlVector3 v3DesiredVelDirection;
    nlVec3Sub(v3DesiredVelDirection, v3BestPosition, m_v3Position);

    float fInvDistance = nlRecipSqrt(
        (v3DesiredVelDirection.x * v3DesiredVelDirection.x) + (v3DesiredVelDirection.y * v3DesiredVelDirection.y) + (v3DesiredVelDirection.z * v3DesiredVelDirection.z), true);

    nlVec3Scale(v3DesiredVelDirection, fInvDistance);

    float fVelocityLengthSq = m_v3Velocity.GetLengthSq3D();
    float fInvVelocity = nlRecipSqrt(fVelocityLengthSq, true);

    nlVector3 v3NormVelocity;
    nlVec3Scale(v3NormVelocity, m_v3Velocity, fInvVelocity);
    float fNormVelY = v3NormVelocity.y;
    float fNormVelX = v3NormVelocity.x;
    float fNormVelZ = v3NormVelocity.z;

    float fDot = (fNormVelX * v3DesiredVelDirection.x) + (fNormVelY * v3DesiredVelDirection.y) + (fNormVelZ * v3DesiredVelDirection.z);

    m_DesireCommonVars.v3DesiredPosition = v3DesiredVelDirection;
    m_DesireCommonVars.turboRequest = TR_FAR_DISTANCE;

    float fReaction[2];

    if (fDot >= 0.8f)
    {
        nlVector3 v3ToPosition;
        nlVec3ScaleAdd(v3ToPosition, 8.0f, v3DesiredVelDirection, m_v3Position);

        float fConfidence[3];
        float bTurboChance;
        bTurboChance = (float)g_vDesireCommonData[m_eFielderDesireState].m_RandomChanceGen.genrand(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Off_TurboChance);

        fConfidence[0] = OpenToPosition(m_v3Position, v3ToPosition, m_pTeam->GetOtherTeam(), this, NULL, false);
        fConfidence[1] = Open(g_pScriptCurrentFielder);
        fConfidence[2] = OnBreakaway(g_pScriptCurrentFielder);
        float fInvincible = Invincible(g_pScriptCurrentFielder);

        fConfidence[1] = (fConfidence[1] >= fConfidence[0]) ? fConfidence[1] : fConfidence[0];
        fConfidence[2] = (fConfidence[2] >= fConfidence[1]) ? fConfidence[2] : fConfidence[1];

        if (fInvincible >= fConfidence[2])
        {
            fConfidence[2] = fInvincible;
        }

        float fFarToGoalie = FarToTheirGoalie(g_pScriptCurrentFielder);

        u8 bForceTurbo = 0;
        if (bTurboChance)
        {
            fFarToGoalie = (fFarToGoalie <= fConfidence[2]) ? fFarToGoalie : fConfidence[2];

            if (fFarToGoalie >= 0.7f)
            {
                bForceTurbo = 1;
            }
        }

        m_DesireCommonVars.turboRequest = (bForceTurbo != 0) ? TR_FORCED_ON : TR_FAR_DISTANCE;
    }

    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    fReaction[0] = 0.7f * (0.3f * (1.0f - pSkillTweaks->Off_Reaction));
    fReaction[1] = nlRandomf(fReaction[0], &nlDefaultSeed) - (0.5f * fReaction[0]);
    m_DesireCommonVars.fMisc = 0.7f + fReaction[1];

    m_pAvoidance->SetThingsToAvoid(0x1F);
    return 1;
}

void cFielder::DesireRunToNet(float fDeltaT)
{
    float fNearGoalie = NearToTheirGoalie(g_pScriptCurrentFielder);
    float fCloseToSideline;
    float fFacingSideline;
    if (fNearGoalie > 0.0f
        || (fCloseToSideline = CloseToSideline(g_pScriptCurrentFielder),
            fFacingSideline = FacingSideline(g_pScriptCurrentFielder),
            fFacingSideline = (fFacingSideline <= fCloseToSideline) ? fFacingSideline : fCloseToSideline,
            fFacingSideline > 0.3f))
    {
        m_DesireCommonVars.turboRequest = TR_FORCED_OFF;
    }

    nlVector3 v3DesiredPosition;
    nlVec3ScaleAdd(v3DesiredPosition, 4.0f, m_DesireCommonVars.v3DesiredPosition, m_v3Position);

    SetDesiredSpeedAndDirectionToPosition(fDeltaT, v3DesiredPosition, m_DesireCommonVars.turboRequest, 0.9f, 0.9f);

    if (GetDistanceToDesiredPos() <= 1.5f || NearToTheirGoalie(g_pScriptCurrentFielder) >= 0.5f)
    {
        if ((m_DesireCommonVars.tAge.GetSeconds() > 0.5f) != false)
        {
            SetDesireDuration(0.0f, true);
        }
    }
    float fMax;
    float fStuck = StuckOnSidelines(g_pScriptCurrentFielder);
    float fAttacked = Attacked(g_pScriptCurrentFielder);
    float fPressured = Pressured(g_pScriptCurrentFielder);

    if (fAttacked >= fStuck)
    {
        fMax = fAttacked;
    }
    else
    {
        fMax = fStuck;
    }
    fPressured = (fPressured >= fMax) ? fPressured : fMax;

    if (m_pBall == NULL || fPressured >= m_DesireCommonVars.fMisc)
    {
        SetDesireDuration(0.0f, true);
    }
}

void cFielder::InitDesirePass(cPlayer* pTarget, bool bHighPass)
{
    mActionPassingVars.pPassTarget = pTarget;
    mActionPassingVars.bVolleyPass = bHighPass;
    m_fDesiredSpeed = (m_fActualSpeed <= ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed) ? m_fActualSpeed : ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed;
    m_pAvoidance->SetThingsToAvoid(0);
}

void cFielder::DesirePass(float fDeltaT)
{
    if (m_pBall != NULL)
    {
        if (!IsBallAwayFromCarrier())
        {
            InitActionPass(mActionPassingVars.pPassTarget, mActionPassingVars.bVolleyPass, false);
        }
    }
    else if (IsActionDone())
    {
        SetDesireDuration(0.0f, true);
    }
}

void cFielder::InitDesireShoot(bool bShootToScore, bool bChipShot)
{
    if (m_pBall != NULL)
    {
        if (bShootToScore)
        {
            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            bool bDidHit = GenerateFilteredRandom() < pSkillTweaks->Shoot_CaptainS2SFirstButtonChance;
            if (bDidHit)
            {
                m_DesireCommonVars.fMisc = g_pGame->m_pGameTweaks->fShootToScorePerfectFirstButtonTime;
            }
            else
            {
                float fRandomError = (float)(0.6f * GenerateFilteredRandom() - 0.30000001192092896);
                m_DesireCommonVars.fMisc = fRandomError + g_pGame->m_pGameTweaks->fShootToScorePerfectFirstButtonTime;
            }
            SetDesireDuration(100000000.0f, true);
            mActionShotVars.bIsShootToScore = true;
        }
        else
        {
            mActionShotVars.bIsChipShot = bChipShot;
            mActionShotVars.bIsShootToScore = false;
        }
        m_pAvoidance->SetThingsToAvoid(0);
    }
}

void cFielder::DesireShoot(float fDeltaT)
{
    if (m_pBall != NULL)
    {
        if (m_eActionState == ACTION_SHOOT_TO_SCORE)
        {
            if (mActionShootToScoreVars.bShootWasPressed)
            {
                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                float fChance = pSkillTweaks->Shoot_CaptainS2SSecondButtonChance - ShootToScoreMeter::instance.mfRumbleAmount;
                bool bDidHit = GenerateFilteredRandom() < fChance;
                if (bDidHit)
                {
                    m_DesireCommonVars.fMisc = -g_pGame->m_pGameTweaks->fShootToScorePerfectSecondButtonTime;
                }
                else
                {
                    float fRandomError = (float)(0.6f * GenerateFilteredRandom() - 0.30000001192092896);
                    m_DesireCommonVars.fMisc = -(fRandomError + g_pGame->m_pGameTweaks->fShootToScorePerfectSecondButtonTime);
                }
                mActionShootToScoreVars.bShootWasPressed = false;
            }
        }
        else if (!IsBallAwayFromCarrier())
        {
            if (mActionShotVars.bIsShootToScore)
            {
                mActionShotVars.bIsShootToScore = false;
                InitActionShootToScore();
            }
            else
            {
                if (ShouldIClearBall())
                {
                    m_pShotMeter->m_fTime = 0.1f + (float)nlRandom((unsigned int)(g_pGame->m_pGameTweaks->fShotWindupTime - 0.2f), &nlDefaultSeed);
                }
                m_pShotMeter->ShotReleased(this);
                InitActionShot(mActionShotVars.bIsChipShot);
            }
        }
    }
    else if (IsActionDone())
    {
        SetDesireDuration(0.0f, true);
    }
}

void cFielder::InitDesireDeke()
{
    m_pAvoidance->SetThingsToAvoid(0);
}

void cFielder::DesireDeke(float fDeltaT)
{
    bool bInitDekeAction = (m_eActionState != ACTION_DEKE);
    if (m_pBall != NULL && IsBallAwayFromCarrier())
    {
        bInitDekeAction = false;
    }

    if (bInitDekeAction)
    {
        InitActionDeke(PAD_DEKE);
    }

    if (IsActionDone())
    {
        SetDesireDuration(0.0f, true);
    }
}

static const nlVector2 g_vUpFieldRange = {
    0.5f,
    3.5f,
};

static const nlVector2 g_vUpFieldMaxDistance = {
    4.0f,
    1.0f,
};

void cFielder::InitDesireRunUpField()
{
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::DesireRunUpField(float fDeltaT)
{
    nlVector3 v3DesiredPosition;
    if (GetFormationPosition(v3DesiredPosition, -1.0f) && m_DesireCommonVars.bInPosition && g_pBall->GetOwnerGoalie() == NULL)
    {
        v3DesiredPosition = m_v3Position;
    }
    else
    {
        float fUpFieldDistance = InterpolateRangeClamped(g_vUpFieldMaxDistance.x, g_vUpFieldMaxDistance.y, g_vUpFieldRange.x, g_vUpFieldRange.y, m_v3AIPosition.x);
        float fSign = AIsgn(m_pTeam->GetOtherNet()->m_v3NetLocation.x);
        v3DesiredPosition.x = v3DesiredPosition.x + (fUpFieldDistance * fSign);
        float dx = v3DesiredPosition.x - m_v3Position.x;
        float dy = v3DesiredPosition.y - m_v3Position.y;
        m_DesireCommonVars.bInPosition = ((dx * dx + dy * dy) <= 1.0f);
    }
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, v3DesiredPosition, TR_FAR_DISTANCE, 0.8f, 0.8f);
    if (GetDistanceToDesiredPos() < 1.5f)
    {
        ShouldIStrafe();
    }
    ShouldIWave();
    if (m_pBall != NULL || m_pTeam->m_pBallInterceptOrderedFielders[0] == this)
    {
        if ((m_DesireCommonVars.tAge.GetSeconds() > 0.5f) != false)
        {
            SetDesireDuration(0.0f, true);
        }
    }
}

void cFielder::InitDesireRunDownField()
{
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::DesireRunDownField(float fDeltaT)
{
    nlVector3 v3DesiredPosition;
    if (GetFormationPosition(v3DesiredPosition, -1.0f) && m_DesireCommonVars.bInPosition && g_pBall->GetOwnerGoalie() == NULL)
    {
        v3DesiredPosition = m_v3Position;
    }
    else
    {
        float fUpFieldDistance = InterpolateRangeClamped(g_vUpFieldMaxDistance.y, g_vUpFieldMaxDistance.x, g_vUpFieldRange.x, g_vUpFieldRange.y, m_v3AIPosition.x);
        if (g_pBall->GetOwnerGoalie() != NULL)
        {
            fUpFieldDistance *= 2.0f;
        }
        float fSign = AIsgn(m_pTeam->m_pNet->m_v3NetLocation.x);
        v3DesiredPosition.x = v3DesiredPosition.x + (fUpFieldDistance * fSign);
        float dx = v3DesiredPosition.x - m_v3Position.x;
        float dy = v3DesiredPosition.y - m_v3Position.y;
        m_DesireCommonVars.bInPosition = ((dx * dx + dy * dy) <= 1.5f);
    }
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, v3DesiredPosition, TR_FAR_DISTANCE, 0.8f, 0.8f);
    if (GetDistanceToDesiredPos() < 1.5f)
    {
        ShouldIStrafe();
    }
    if (m_pBall != NULL || m_pTeam->m_pBallInterceptOrderedFielders[0] == this)
    {
        if ((m_DesireCommonVars.tAge.GetSeconds() > 0.5f) != false)
        {
            SetDesireDuration(0.0f, true);
        }
    }
}

void cFielder::InitDesireRunToLocation(const nlVector3& v3DesiredLocation, bool bTurbo)
{
    if (m_pBall != NULL)
    {
        ReleaseBall();
    }
    m_DesireCommonVars.v3DesiredPosition = v3DesiredLocation;
    m_DesireCommonVars.turboRequest = bTurbo ? TR_FORCED_ON : TR_FAR_DISTANCE;
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::DesireRunToLocation(float fDeltaT)
{
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_DesireCommonVars.v3DesiredPosition, m_DesireCommonVars.turboRequest, 0.7f, 0.7f);
    if (GetDistanceToDesiredPos() < 1.0f)
    {
        SetDesireDuration(0.0f, true);
        if ((g_pGame->m_eGameState == GS_POST_GOAL || g_pGame->m_eGameState == GS_END_GAME) && this == g_pCharacters[0])
        {
            g_pEventManager->CreateValidEvent(8, 20);
        }
    }
}

void cFielder::InitDesireSlideAttack(cFielder* pTarget)
{
    m_DesireSlideAttackVars.m_pSlideAttackTarget = pTarget;
    m_eDesireSubState = 0;
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

/**
 * Offset/Address/Size: 0xCA8 | 0x80031A2C | size: 0x23C
 */
void cFielder::DesireSlideAttack(float fDeltaT)
{
    float fTime;
    nlVector3 v3VictimPosition;
    float fBallClosingSpeed;

    switch (m_eDesireSubState)
    {
    case 0:
    {
        if (m_pMark == NULL || m_DesireSlideAttackVars.m_pSlideAttackTarget == NULL || m_DesireSlideAttackVars.m_pSlideAttackTarget != g_pBall->m_pOwner)
        {
            SetDesireDuration(0.0f, true);
            break;
        }

        if (CanISlideAttack(m_DesireSlideAttackVars.m_pSlideAttackTarget->m_v3Position,
                m_DesireSlideAttackVars.m_pSlideAttackTarget->m_v3Velocity,
                &fTime))
        {
            InitActionSlideAttack(m_DesireSlideAttackVars.m_pSlideAttackTarget, fTime);
            m_eDesireSubState = 1;
            break;
        }

        cFielder* pTarget = m_DesireSlideAttackVars.m_pSlideAttackTarget;
        v3VictimPosition.x = pTarget->m_v3Position.x + 0.25f * pTarget->m_v3Velocity.x;
        v3VictimPosition.y = pTarget->m_v3Position.y + 0.25f * pTarget->m_v3Velocity.y;
        v3VictimPosition.z = 0.0f;

        u8 turbo = ShouldITurboWithoutBall();
        SetDesiredSpeedAndDirectionToPosition(fDeltaT, v3VictimPosition, (eTurboRequest)(turbo != 0), 1.0f, 1.0f);

        m_pAvoidance->UseMinimumAvoidance(m_DesireSlideAttackVars.m_pSlideAttackTarget);
        ShouldIStrafe();
        break;
    }
    case 1:
    {
        SetDesireDuration(999999.9f, true);

        if (m_tSlideAttackTimer.m_uPackedTime != 0)
        {
            if (mActionSlideAttackVars.bAttackSucceeded == 0)
            {
                float fBallSpeed = nlSqrt(
                    g_pBall->m_v3Velocity.x * g_pBall->m_v3Velocity.x + g_pBall->m_v3Velocity.y * g_pBall->m_v3Velocity.y + g_pBall->m_v3Velocity.z * g_pBall->m_v3Velocity.z,
                    true);

                if (fBallSpeed > 0.05f)
                {
                    const nlVector3& ballVelocity = GetBallVelocity(g_pBall);
                    fBallClosingSpeed = GetClosingSpeed2D(GetJointPosition(m_nLeftFootJointIndex), m_v3Velocity, GetBallPosition(g_pBall), ballVelocity);
                    if (fBallClosingSpeed < 0.0f)
                    {
                        if (nlRandomf(1.0f, &nlDefaultSeed) > 0.5f)
                        {
                            m_tSlideAttackTimer.SetSeconds(0.0f);
                            m_eDesireSubState = 2;
                        }
                    }
                }
            }
        }
        else
        {
            m_eDesireSubState = 2;
        }
        break;
    }
    case 2:
    {
        if (m_eActionState == ACTION_NEED_ACTION)
        {
            SetDesireDuration(0.0f, true);
        }
        break;
    }
    }
}

void cFielder::InitDesireSupportBallDefensive()
{
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::InitDesireSupportBallOffensive()
{
    m_pAvoidance->SetThingsToAvoid(0x1F);
}

void cFielder::InitDesireUserControlled()
{
    StartRunning();
    m_pAvoidance->SetThingsToAvoid(8);
}

/**
 * Offset/Address/Size: 0x794 | 0x80031518 | size: 0x514
 */
void cFielder::DesireUserControlled(float fDeltaT)
{
    bool bWasActionTaken;
    nlPolar p;
    nlVector3 v3Velocity;

    if (GetGlobalPad() == NULL)
    {
        SetDesireDuration(0.0f, true);
        return;
    }

    if (g_pGame->m_eGameState == GS_KICKOFF)
    {
        if (mbCanKickoff && m_pBall != NULL)
        {
            bWasActionTaken = false;

            if (GetGlobalPad()->JustPressed(PAD_PASS, true))
            {
                InitActionPass(DoFindBestPassTarget(GetGlobalPad()->JustPressed(PAD_AIM, true), false), (bWasActionTaken = GetGlobalPad()->JustPressed(PAD_AIM, true)), true);
                bWasActionTaken = true;
            }
            else if (GetGlobalPad()->JustPressed(PAD_DEKE, true) || m_pController->GetCStickMovementStickMagnitude() > 0.0f)
            {
                InitActionDeke(PAD_DEKE);
                bWasActionTaken = true;
            }
            else if (GetGlobalPad()->JustPressed(PAD_SHOOT, true))
            {
                DoResetShotMeter(0.0f);
                ShotMeter* pShotMeter = m_pShotMeter;
                pShotMeter->CalcOneTimerValue(this, UsePerfectPass());
                InitActionShot(GetGlobalPad()->JustPressed(PAD_AIM, true));
                bWasActionTaken = true;
            }
            else if (GetGlobalPad()->JustPressed(PAD_USE, true))
            {
                if (!IsPlayingPowerupAnim())
                {
                    UseTeamPowerup(NULL);
                }
                StartRunning();
                bWasActionTaken = true;
            }
            else if (m_pController->GetMovementStickMagnitude() > 0.001f)
            {
                if (mtKickOffWaitTimer.GetSeconds() > 0.15f)
                {
                    mtKickOffWaitTimer.SetSeconds(0.15f);
                }
                else if (mtKickOffWaitTimer.GetSeconds() < 0.05f)
                {
                    StartRunning();
                    bWasActionTaken = true;
                }
            }

            if (bWasActionTaken)
            {
                g_pEventManager->CreateValidEvent(0xb, 0x14);
                mtKickOffWaitTimer.SetSeconds(0.0f);
                mbCanKickoff = false;
            }
        }

        m_fDesiredSpeed = 0.0f;
        m_aDesiredFacingDirection = m_aActualFacingDirection;
        m_aDesiredMovementDirection = m_aActualFacingDirection;

        if (GetGlobalPad()->JustPressed(PAD_TOGGLE_POWERUP, true))
        {
            m_pTeam->TogglePowerup(false);
        }
    }
    else
    {
        if (!g_pGame->mbCaptainShotToScoreOn && GetGlobalPad()->JustPressed(PAD_USE, true) && !IsPlayingPowerupAnim())
        {
            UseTeamPowerup(NULL);
        }

        if (GetGlobalPad()->JustPressed(PAD_TOGGLE_POWERUP, true))
        {
            m_pTeam->TogglePowerup(false);
        }

        if (m_eActionState == ACTION_SHOOT_TO_SCORE)
        {
            return;
        }

        if (m_eActionState == ACTION_NEED_ACTION)
        {
            StartRunning();
        }

        SetDesiredFacingDirection();

        if (m_eActionState == ACTION_RUNNING)
        {
            TestButtonsRunning();
            if (m_pController->IsTurboPressed())
                SetDesiredSpeed(m_pTweaks->fRunningSpeed, ((FielderTweaks*)m_pTweaks)->fRunningTurboSpeed);
            else
                SetDesiredSpeed(m_pTweaks->fJoggingSpeed, m_pTweaks->fRunningSpeed);
            if (g_pBall->m_pOwner == NULL)
                DoPositioningInterceptBall();
        }
        else if (m_eActionState == ACTION_RUNNING_WB)
        {
            TestButtonsRunningWB(fDeltaT);
            if (m_pController->IsTurboPressed())
                SetDesiredSpeed(((FielderTweaks*)m_pTweaks)->fRunningWBSpeed, ((FielderTweaks*)m_pTweaks)->fRunningWBTurboSpeedLevel1);
            else
                SetDesiredSpeed(m_pTweaks->fJoggingSpeed, ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed);
        }
        else if (m_eActionState == ACTION_RUNNING_WB_TURBO)
        {
            if (IsBallAwayFromCarrier())
            {
                TestButtonsToQueueActions(fDeltaT);
            }
            else if (!TestQueuedActions())
            {
                TestButtonsRunningWB(fDeltaT);
                u8 bIsShotActive = false;
                eShotMeterState state = m_pShotMeter->m_eShotMeterState;
                if (state == SHOT_METER_ACTIVE || state == SHOT_METER_STS_ACTIVE || state == SHOT_METER_STS_TRANSISTION)
                    bIsShotActive = true;
                if (bIsShotActive)
                {
                    m_fActualSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed;
                    InitActionRunningWB(false);
                }
            }
        }

        p.a = m_aDesiredMovementDirection;
        p.r = m_fDesiredSpeed;
        nlPolarToCartesian(v3Velocity, p);
        float fScale = 0.25f;
        v3Velocity.z = 0.0f;
        float fZero = v3Velocity.z;
        float fDesiredX = fScale * v3Velocity.x + m_v3Position.x;
        float fDesiredZ = fScale * fZero + m_v3Position.z;
        float fDesiredY = fScale * v3Velocity.y + m_v3Position.y;
        m_v3DesiredPosition.x = fDesiredX;
        m_v3DesiredPosition.y = fDesiredY;
        m_v3DesiredPosition.z = fDesiredZ;

        if (m_pTeam->mpCurrentSituation != SITUATION_LOOSE)
            ShouldIStrafe();
        ShouldIWave();
    }
}

void cFielder::InitDesireWait()
{
    m_fDesiredSpeed = 0.0f;
}

void cFielder::DesireWait(float fDeltaT)
{
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_v3Position, TR_FAR_DISTANCE, 1.0f, 1.0f);
}

void cFielder::InitDesireHit(cFielder* pTarget)
{
    if (pTarget == NULL)
    {
        pTarget = DoFindBestHitTarget();
    }
    InitActionHit(pTarget);
    m_pAvoidance->SetThingsToAvoid(0);
}

void cFielder::DesireHit(float fDeltaT)
{
    if (IsActionDone())
    {
        SetDesireDuration(0.0f, true);
    }
}

void cFielder::InitDesireUsePowerup(ePowerUpType ePowerup, cFielder* pTarget)
{
    if (ePowerup == m_pTeam->GetCurrentPowerUp().eType && !IsPlayingPowerupAnim())
    {
        UseTeamPowerup(pTarget);
    }
}

/**
 * Offset/Address/Size: 0x41C | 0x800311A0 | size: 0x378
 */
void cFielder::DesireUsePowerup(float fDeltaT)
{
    extern float Offensive(cTeam*);

    if (Offensive(this != NULL ? m_pTeam : NULL))
    {
        SetDesireDuration(0.0f, true);
        return;
    }

    switch (m_ePrevFielderDesireState)
    {
    case FIELDERDESIRE_INTERCEPT_BALL:
        InitDesire(FIELDERDESIRE_INTERCEPT_BALL, 0.5f, -1.0f, fvNotSet, fvNotSet);
        break;

    default:
        InitDesire(FIELDERDESIRE_MARK, 0.5f, -1.0f, fvNotSet, fvNotSet);
        break;
    }
}

bool cFielder::InitDesireWindupPass(cPlayer* pTarget, bool bHighPass)
{
    if (m_pBall == NULL)
    {
        if (m_sQueuedDesireParams.eDesireType == FIELDERDESIRE_WINDUP_PASS)
        {
            m_sQueuedDesireParams.fDuration = 0.0f;
            m_sQueuedDesireParams.eDesireType = FIELDERDESIRE_NEED_DESIRE;
            m_sQueuedDesireParams.opt1 = fvNotSet;
            m_sQueuedDesireParams.opt2 = fvNotSet;
        }
        return false;
    }

    mActionPassingVars.pPassTarget = pTarget;
    mActionPassingVars.bVolleyPass = bHighPass;
    SetDesireDuration(3.2f, true);
    if (bHighPass)
    {
        m_DesireCommonVars.tMiscTimer.m_uPackedTime = 0;
    }
    else
    {
        m_DesireCommonVars.tMiscTimer.SetSeconds(3.0f);
    }
    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    float fReactionTimeRange = 0.85f * (0.3f * (1.0f - pSkillTweaks->Off_Reaction));
    m_DesireCommonVars.fMisc = 0.85f + (nlRandomf(fReactionTimeRange, &nlDefaultSeed) - (0.5f * fReactionTimeRange));
    SetSpaceSearch(new (nlMalloc(sizeof(SSearchOpenLane), 8, false)) SSearchOpenLane(this, pTarget));
    m_pSpaceSearch->m_bDebugOn = false;
    m_pSpaceSearch->FindBestPosition(m_DesireCommonVars.v3DesiredPosition, m_v3Position, DIR_UPFIELD, &pTarget->m_v3Position, 4.5f, 0x8000);
    m_pAvoidance->SetThingsToAvoid(0x1F);
    return true;
}

void cFielder::DesireWindupPass(float fDeltaT)
{
    if (m_pBall == NULL || Incapacitated(mActionPassingVars.pPassTarget))
    {
        SetDesireDuration(0.0f, true);
        return;
    }
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_DesireCommonVars.v3DesiredPosition, TR_FORCED_OFF, 1.0f, 1.0f);
    bool bSwitchToPassDesire = false;
    if (m_DesireCommonVars.tMiscTimer.m_uPackedTime != 0)
    {
        float fInDanger = Fuzzy::InDangerDelayed(g_pScriptCurrentFielder).mData.f;
        float fNotFarToTheirGoalie = FLESS(FarToTheirGoalie(g_pScriptCurrentFielder), 0.3f);
        float fDistanceToDesiredPos = GetDistanceToDesiredPos();
        float fClosingSpeedToDesiredPos = GetClosingSpeed2D(m_DesireCommonVars.v3DesiredPosition, v3Zero, m_v3Position, m_v3Velocity);
        if (fClosingSpeedToDesiredPos < 0.0f || fDistanceToDesiredPos <= 1.0f
            || ((fInDanger = (fInDanger >= fNotFarToTheirGoalie) ? fInDanger : fNotFarToTheirGoalie) >= m_DesireCommonVars.fMisc))
        {
            bSwitchToPassDesire = true;
        }
    }
    else
    {
        bSwitchToPassDesire = true;
    }
    if (bSwitchToPassDesire && !IsBallAwayFromCarrier())
    {
        SetDesireDuration(0.0f, true);
        InitDesire(FIELDERDESIRE_PASS, m_fDesireConfidence, -1.0f, FuzzyVariant(mActionPassingVars.pPassTarget), FuzzyVariant(mActionPassingVars.bVolleyPass));
    }
}

void cFielder::InitDesireWindupShot()
{
    if (m_pBall != NULL)
    {
        if (!IsBallAwayFromCarrier())
        {
            DoResetShotMeter(0.0f);
            m_DesireWindupForShotVars.bIsBallAwayFromCarrier = false;
            SetDesireDuration(m_pShotMeter->GetTotalDuration(), false);
        }
        else
        {
            SetDesireDuration(999999.9f, true);
            m_DesireWindupForShotVars.bIsBallAwayFromCarrier = true;
        }
        m_pAvoidance->SetThingsToAvoid(0);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80030D84 | size: 0x41C
 */
void cFielder::DesireWindupShot(float fDeltaT)
{
    if (m_pBall == NULL)
    {
        SetDesireDuration(0.0f, true);
        return;
    }

    float fDesiredSpeed = m_fActualSpeed;
    float fMaxSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed;
    fDesiredSpeed = (fDesiredSpeed <= fMaxSpeed) ? fDesiredSpeed : fMaxSpeed;
    m_fDesiredSpeed = fDesiredSpeed;

    if (m_DesireWindupForShotVars.bIsBallAwayFromCarrier)
    {
        if (!IsBallAwayFromCarrier())
        {
            DoResetShotMeter(0.0f);
            SetDesireDuration(m_pShotMeter->GetTotalDuration(), false);

            m_DesireWindupForShotVars.bIsBallAwayFromCarrier = false;

            m_DesireCommonVars.tMiscTimer.SetSeconds(g_pGame->m_pGameTweaks->fShotWindupTime / 3.0f);
        }
        else
        {
            return;
        }
    }

    bool bShootToScore = false;
    unsigned char bSwitchToShootDesire = 0;

    if (m_DesireCommonVars.tMiscTimer.m_uPackedTime == 0)
    {
        if (DoAIWindupActionSelection())
        {
            m_DesireCommonVars.tMiscTimer.SetSeconds(10000000.0f);
        }
        else
        {
            float fTimer = (g_pGame->m_pGameTweaks->fShotWindupTime / 3.0f) - 0.05f;
            float fMinTimer = 0.1f;
            fMinTimer = (fMinTimer >= fTimer) ? fMinTimer : fTimer;
            m_DesireCommonVars.tMiscTimer.SetSeconds(fMinTimer);
        }
    }

    bool bMeterWindupState = false;
    eShotMeterState shotMeterState;
    ShotMeter* pShotMeter = m_pShotMeter;

    shotMeterState = pShotMeter->m_eShotMeterState;
    if (shotMeterState == SHOT_METER_ACTIVE || shotMeterState == SHOT_METER_STS_ACTIVE || shotMeterState == SHOT_METER_STS_TRANSISTION)
    {
        bMeterWindupState = true;
    }

    if (bMeterWindupState)
    {
        if (pShotMeter->m_eShotMeterState == SHOT_METER_STS_TRANSISTION)
        {
            static FilteredRandomChance s2sChanceGen;
            bool bS2SChance = s2sChanceGen.genrand(SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->Off_CaptainS2SChance);

            float fInvincible = Invincible(g_pScriptCurrentFielder);
            float fOpen = Open(g_pScriptCurrentFielder);
            fOpen = (fOpen >= fInvincible) ? fOpen : fInvincible;

            if (fOpen >= 0.8f && NearToTheirGoalie(g_pScriptCurrentFielder) <= 0.65f && bS2SChance)
            {
                KillWindup(this, "ball_shot_windup", true);
                EmitWindupAtCharacter(this, "ball_sts_windup");
            }
            else
            {
                bSwitchToShootDesire = 1;
            }
        }
    }
    else
    {
        if (pShotMeter->m_eShotMeterState == SHOT_METER_RELEASED)
        {
            bSwitchToShootDesire = 1;
        }
        else if (pShotMeter->m_eShotMeterState == SHOT_METER_STS_RELEASED)
        {
            bShootToScore = true;
            bSwitchToShootDesire = 1;
        }
    }

    if (m_tDesireDuration.m_uPackedTime == 0)
    {
        bSwitchToShootDesire = 1;
    }

    if (bSwitchToShootDesire)
    {
        SetDesireDuration(0.0f, true);
        InitDesire(FIELDERDESIRE_SHOOT, m_fDesireConfidence, -1.0f, FuzzyVariant(bShootToScore), fvNotSet);
    }
}

bool cFielder::InitDesireCutAndBreak()
{
    if (m_pBall != NULL)
    {
        if (m_sQueuedDesireParams.eDesireType == FIELDERDESIRE_CUT_AND_BREAK)
        {
            m_sQueuedDesireParams.fDuration = 0.0f;
            m_sQueuedDesireParams.eDesireType = FIELDERDESIRE_NEED_DESIRE;
            m_sQueuedDesireParams.opt1 = fvNotSet;
            m_sQueuedDesireParams.opt2 = fvNotSet;
        }
        return false;
    }

    SetSpaceSearch(new (nlMalloc(sizeof(SSearchCutAndBreak), 8, false)) SSearchCutAndBreak(this));
    m_pSpaceSearch->m_bDebugOn = false;
    m_pSpaceSearch->FindBestPosition(m_DesireCommonVars.v3DesiredPosition, m_v3Position, DIR_NONE, NULL, 4.0f, 0x8000);
    m_pAvoidance->SetThingsToAvoid(0x1F);
    return true;
}

void cFielder::DesireCutAndBreak(float fDeltaT)
{
    SetDesiredSpeedAndDirectionToPosition(fDeltaT, m_DesireCommonVars.v3DesiredPosition, TR_FAR_DISTANCE, 0.5f, 0.5f);
    if (GetDistanceToDesiredPos() < 0.5f || m_pTeam->m_pBallInterceptOrderedFielders[0] == this)
    {
        if ((m_DesireCommonVars.tAge.GetSeconds() > 0.5f) != false)
        {
            SetDesireDuration(0.0f, true);
        }
    }
    else if (m_pBall != NULL)
    {
        SetDesireDuration(0.0f, true);
    }
}
