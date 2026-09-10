#include "port/region.h"  // PORT: one binary, three discs
#include "Game/AI/Fielder.h"
#include "Game/MathHelpers.h"
#include "Game/AI/FielderActions.h"
#include "Game/AI/AIPlay.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/AvoidController.h"
#include "Game/AI/FielderDesires.h"
#include "Game/AI/Fuzzy.h"
#include "Game/AI/ShotMeter.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/AnimInventory.h"
#include "Game/Ball.h"
#include "Game/BasicStadium.h"
#include "Game/CharacterTriggers.h"
#include "Game/FixedUpdateTask.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/Render/Bowser.h"
#include "Game/Render/ChainChomp.h"
#include "Game/Render/ShootToScoreArrow.h"
#include "Game/Render/ShootToScoreMeter.h"
#include "Game/Drawable/DrawableCharacter.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Camera/animcam.h"
#include "Game/ParticleUpdateTask.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/DB/StatsTracker.h"
#include "Game/RumbleActions.h"
#include "Game/SAnim/pnBlender.h"
#include "Game/SAnim/pnSingleAxisBlender.h"
#include "Game/SAnim/pnSAnimController.h"
#include "Game/SAnim.h"
#include "Game/Sys/eventman.h"
#include "Game/Net.h"
#include "Game/World.h"
#include "NL/nlSlotPool.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsColumn.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "math.h"

static inline s16 GetAngleDifference(u32 a, u32 b)
{
    return (s16)(a - b);
}

static f32 CANT_COLLIDE = *(f32*)__float_max;

static bool gbDebugShooting;
static bool gbDoCaptainShootToScore;

enum DebugShootingMode
{
    DEBUG_SHOOT_AT_NET = 0,
    DEBUG_SHOOT_IN_ANALOG_STICK_DIRECTION = 1,
    NUM_DEBUG_SHOOTING_MODES = 2,
};

static DebugShootingMode sDebugShootingMode;
static float sfDebugShotVelocity = 20.0f;
static float sfDebugShotHeight = 2.0f;
static float sfDebugShotXOffset;
static float sfDebugShotYOffset;

namespace Audio
{
enum eWorldSFX
{
    WORLDSFX_DUMMY = 0,
};

class cWorldSFX : public cGameSFX
{
public:
    uintptr_t Play(Audio::SoundAttributes&);
    void Stop(eWorldSFX, cGameSFX::StopFlag);
};

extern cWorldSFX gCrowdSFX;
extern cWorldSFX gStadGenSFX;
} // namespace Audio

extern bool g_e3_Build;
void FireCameraRumbleFilter(float, float);
class FakeBallWorld
{
public:
    static void ResetBallIterator();
    static void GetNextBallPosition(nlVector3&);
};
extern float g_fSimulationTick;

namespace Fuzzy
{
FuzzyVariant ShouldIStrafeBall(cFielder*);
FuzzyVariant ShouldIStrafeMark(cFielder*);
FuzzyVariant GetBestHitTarget(cFielder*);
FuzzyVariant GetBestLooseBallAction(cFielder*);
FuzzyVariant GetBestPassReceiveAction(cFielder*);
FuzzyVariant GetBestWindupShotAction(cFielder*);
} // namespace Fuzzy

const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };

static const LooseBallContactAnimInfo gOneTimerIdleGroundContactAnims[4] = {
    { 0x40, 7.0f, 0xE000, 0x2000 },
    { 0x41, 7.0f, 0xA000, 0xE000 },
    { 0x43, 7.0f, 0x6000, 0xA000 },
    { 0x42, 7.0f, 0x2000, 0x6000 },
};
static const LooseBallContactAnimInfo gOneTimerIdleVolleyContactAnims[4] = {
    { 0x48, 10.0f, 0xE000, 0x2000 },
    { 0x49, 9.0f, 0xA000, 0xE000 },
    { 0x4B, 9.0f, 0x6000, 0xA000 },
    { 0x4A, 9.0f, 0x2000, 0x6000 },
};
static const LooseBallContactAnimInfo gOneTimerLeadGroundContactAnims[4] = {
    { 0x44, 9.0f, 0xE000, 0x2000 },
    { 0x45, 9.0f, 0xA000, 0xE000 },
    { 0x47, 9.0f, 0x6000, 0xA000 },
    { 0x46, 9.0f, 0x2000, 0x6000 },
};
static const LooseBallContactAnimInfo gOneTimerLeadVolleyContactAnims[4] = {
    { 0x4C, 10.0f, 0xE000, 0x2000 },
    { 0x4D, 10.0f, 0xA000, 0xE000 },
    { 0x4F, 10.0f, 0x6000, 0xA000 },
    { 0x4E, 9.5f, 0x2000, 0x6000 },
};

/**
 * Offset/Address/Size: 0xD2E8 | 0x80026624 | size: 0x474
 */
cFielder::cFielder(int nPlayerID, int nTeamID, eCharacterClass cc, const int* nModelID,
    cSHierarchy* pHierarchy, cAnimInventory* pAnimInventory,
    const CharacterPhysicsData* pCharacterPhysicsData, FielderTweaks* pCharTweaks,
    AnimRetargetList* pAnimRetargetList)
    : cPlayer(nPlayerID, cc, nModelID, pHierarchy, pAnimInventory, pCharacterPhysicsData,
          pCharTweaks, pAnimRetargetList, FIELDER)
    , m_eActionState(ACTION_NEED_ACTION)
    , m_tFrozenTimer(0.0f)
    , m_eFielderDesireState((eFielderDesireState)0)
    , m_ePrevFielderDesireState((eFielderDesireState)0)
    , m_tDesireDuration(0.0f)
    , m_ePowerup((ePowerUpType)-1)
    , mnNumPowerups(0)
    , m_pPowerupTarget(NULL)
    , m_nPowerupAnimID(-1)
    , mtBombImpactTime(0.0f)
{
    m_ePenaltyCardStatus = PENALTY_CARD_NONE;
    m_pMark = NULL;
    m_pMarker = NULL;
    m_eRole = (eRole)0;
    m_pCurrentPlay = NULL;
    mbCanKickoff = false;
    mbCaptShootToScoreEffectOn = false;
    m_fDistanceToDesiredPosition = -9999.9f;
    m_v3DesiredPosition.x = 0.0f;
    m_v3DesiredPosition.y = 0.0f;
    m_v3DesiredPosition.z = 0.0f;

    AIPlay* pAIPlayMem = (AIPlay*)nlMalloc(sizeof(AIPlay), 8, false);
    pAIPlayMem = new (pAIPlayMem) AIPlay(this, AIPLAY_NULL, -1.0f);
    m_pCurrentPlay = pAIPlayMem;

    ShotMeter* pShotMeter = (ShotMeter*)nlMalloc(sizeof(ShotMeter), 8, false);
    if (pShotMeter != NULL)
    {
        pShotMeter->m_eShotMeterState = SHOT_METER_INACTIVE;
        pShotMeter->m_fTime = 0.0f;
        pShotMeter->m_fScoreValue = 0.0f;
        pShotMeter->m_fSpeedValue = 0.0f;
        pShotMeter->m_fSTSValue = 0.0f;
        pShotMeter->mfSShotAimValue = 0.0f;
    }
    m_pShotMeter = pShotMeter;

    AvoidController* pAvoidMem = (AvoidController*)nlMalloc(sizeof(AvoidController), 8, false);
    pAvoidMem = new (pAvoidMem) AvoidController(this);
    m_pAvoidance = pAvoidMem;

    m_DesireCommonVars.tAge.m_uPackedTime = 0;
    m_DesireCommonVars.tMiscTimer.m_uPackedTime = 0;
    m_DesireCommonVars.bInPosition = false;
    m_DesireCommonVars.pBallOwner = NULL;
    m_DesireCommonVars.pSBC = NULL;
    mActionShotVars.bIsChipShot = false;
    mActionLooseBallShotVars.bIsChipShot = false;
    mActionShootToScoreVars.isCurrentlyInvincible = false;
    mActionShootToScoreVars.isInUnbreakablePart = false;

    char buff[32];
    nlSNPrintf(buff, 31, "CalcNewDesire%d", GetUniqueID(nTeamID));
    mThoughtHashCalcDesire = nlStringHash(buff);
    nlSNPrintf(buff, 31, "InitRunToNet%d", GetUniqueID(nTeamID));
    mThoughtHashInitRunToNet = nlStringHash(buff);
    nlSNPrintf(buff, 31, "InitGetOpen%d", GetUniqueID(nTeamID));
    mThoughtHashInitGetOpen = nlStringHash(buff);
    nlSNPrintf(buff, 31, "InitWindupPass%d", GetUniqueID(nTeamID));
    mThoughtHashInitWindupPass = nlStringHash(buff);
    nlSNPrintf(buff, 31, "InitWindupCutAndBreak%d", GetUniqueID(nTeamID));
    mThoughtHashInitCutAndBreak = nlStringHash(buff);
}

/**
 * Offset/Address/Size: 0xD264 | 0x800265A0 | size: 0x84
 */
cFielder::~cFielder()
{
    CleanUpAction();
    delete m_pAvoidance;
    delete m_pShotMeter;
    delete m_pCurrentPlay;
}

/**
 * Offset/Address/Size: 0xD240 | 0x8002657C | size: 0x24
 */
void cFielder::AbortPlay()
{
    m_pCurrentPlay->ClearPlay();
}

/**
 * Offset/Address/Size: 0xD1D4 | 0x80026510 | size: 0x6C
 */
void cFielder::AbortPendingThoughts()
{
    g_pGame->AbortPendingThought(mThoughtHashCalcDesire);
    g_pGame->AbortPendingThought(mThoughtHashInitRunToNet);
    g_pGame->AbortPendingThought(mThoughtHashInitGetOpen);
    g_pGame->AbortPendingThought(mThoughtHashInitWindupPass);
    g_pGame->AbortPendingThought(mThoughtHashInitCutAndBreak);
    ClearQueuedDesire();
}

static inline bool IsGameplayOrOvertime(eGameState state);

bool cFielder::HasNoDesire() const
{
    bool bNoDesire;
    if (m_eFielderDesireState == FIELDERDESIRE_NEED_DESIRE || m_tDesireDuration.m_uPackedTime == 0)
    {
        bNoDesire = true;
    }
    else
    {
        bNoDesire = false;
    }
    return bNoDesire;
}

/**
 * Offset/Address/Size: 0xC890 | 0x80025BCC | size: 0x944
 */
void cFielder::CalculateNewDesire()
{
    if (m_sQueuedDesireParams.eDesireType)
    {
        if (InitDesire(&m_sQueuedDesireParams, 0.5f))
        {
            m_sQueuedDesireParams.fDuration = 0.0f;
            m_sQueuedDesireParams.eDesireType = FIELDERDESIRE_NEED_DESIRE;
            m_sQueuedDesireParams.opt1 = fvNotSet;
            m_sQueuedDesireParams.opt2 = fvNotSet;
        }
    }
    else if (g_pGame->m_eGameState == GS_KICKOFF)
    {
        if (GetGlobalPad())
            InitDesire(FIELDERDESIRE_USER_CONTROLLED, 0.5f, -1.0f, fvNotSet, fvNotSet);
        else
            InitDesire(FIELDERDESIRE_WAIT, 0.5f, -1.0f, fvNotSet, fvNotSet);
    }
    else
    {
        bool bGameplayOrOvertime = IsGameplayOrOvertime(g_pGame->GetGameState());
        if (bGameplayOrOvertime)
        {
            if (GetGlobalPad())
                InitDesire(FIELDERDESIRE_USER_CONTROLLED, 0.5f, -1.0f, fvNotSet, fvNotSet);
            else if (g_pGame->IsThoughtAllowed(mThoughtHashCalcDesire))
                m_pCurrentPlay->CalculateNewDesire();
            else
                InitDesire(FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP, 0.5f, -1.0f, fvNotSet, fvNotSet);
        }
    }

    const cFielder* pFielder = this;
    if (pFielder->HasNoDesire())
    {
        if (m_eFielderDesireState != FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP)
        {
            nlPrintf("Fielder::CalculateNewDesire - no desire was calculated, falling back to Wait.\n");
            InitDesire(FIELDERDESIRE_WAIT, 0.5f, -1.0f, fvNotSet, fvNotSet);
        }
    }
}

float cFielder::CalcJogRunBlendWeight() const
{
    const FielderTweaks* pTweaks = (const FielderTweaks*)m_pTweaks;
    float fTopSpeed;
    if (m_pBall != NULL)
    {
        fTopSpeed = pTweaks->fRunningWBSpeed;
    }
    else
    {
        fTopSpeed = pTweaks->fRunningSpeed;
    }
    return InterpolateRangeClamped(0.0f, 1.0f, pTweaks->fJoggingSpeed, fTopSpeed, __fabsf(m_fActualSpeed));
}

float cFielder::CalcRunTurboBlendWeight() const
{
    const FielderTweaks* pTweaks = (const FielderTweaks*)m_pTweaks;
    float fTopSpeed = pTweaks->fRunningTurboSpeed;
    return InterpolateRangeClamped(0.0f, 1.0f, pTweaks->fRunningSpeed, fTopSpeed, __fabsf(m_fActualSpeed));
}

bool cFielder::CanGetElectrocuted(const CollisionPlayerWallData* eventData)
{
    eFielderActionState state = m_eActionState;
    if (state == ACTION_HIT_REACT || state == ACTION_BOMB_REACT || state == ACTION_STS_HIT_REACT)
    {
        float minYElectrocutionPosition = m_pTeam->m_pNet->GetNetWidth() / 2.0f;
        float netHeight = m_pTeam->m_pNet->GetNetHeight();
        nlVector3 jointPos = GetJointPosition(m_nBip01JointIndex_0xA4);
        if ((float)fabs(eventData->contactPoint.y) > minYElectrocutionPosition
            || (float)fabs(jointPos.z) > netHeight)
        {
            return true;
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0xC814 | 0x80025B50 | size: 0x7C
 */
bool cFielder::CanDoCaptainShootToScore()
{
    if (nlSingleton<GameInfoManager>::Instance()->GetGameplayOptions().Shoot2Score
        && (IsCaptain() || nlSingleton<GameInfoManager>::Instance()->GetTeam(g_pBall->m_pOwner->m_pTeam->m_nSide) == TEAM_MYSTERY))
    {
        return true;
    }

    return false;
}

static inline float GetNormalizedContactTime(const cSAnim* anim, float contactFrame)
{
    return contactFrame / (float)anim->m_nNumKeys;
}

bool cFielder::CanPassTargetAttemptOneTouch(cFielder* pPassTarget)
{
    return pPassTarget != NULL
        && pPassTarget != this
        && pPassTarget->m_pTeam == m_pTeam
        && pPassTarget->GetGlobalPad() == NULL
        && (pPassTarget->m_eFielderDesireState == FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE
            || pPassTarget->m_eFielderDesireState == FIELDERDESIRE_RECEIVE_PASS_FROM_RUN)
        && g_pBall->m_pPrevOwner == this;
}

/**
 * Offset/Address/Size: 0xC648 | 0x80025984 | size: 0x1CC
 */
bool cFielder::CanLooseBallShoot()
{
    if ((g_pBall->m_pOwner == NULL)
        && (g_pBall->m_pPassTarget == NULL)
        && (g_pBall->m_tShotTimer.m_uPackedTime == 0)
        && (g_pBall->m_unk_0xA6 == 0))
    {
        const cSAnim* guessContactAnim = m_pAnimInventory->GetAnim(gOneTimerLeadGroundContactAnims[0].nAnimID);
        float ratio = GetNormalizedContactTime(guessContactAnim, gOneTimerLeadGroundContactAnims[0].fAnimContactFrame);
        float frames = (float)guessContactAnim->m_nNumKeys / 30.0f;
        float contactTime = ratio * frames;

        nlVector3 v3PredictedPos;
        nlVec3Set(v3PredictedPos,
            (contactTime * g_pBall->m_v3Velocity.x) + g_pBall->m_v3Position.x,
            (contactTime * g_pBall->m_v3Velocity.y) + g_pBall->m_v3Position.y,
            (contactTime * g_pBall->m_v3Velocity.z) + g_pBall->m_v3Position.z);

        s16 facingDelta = GetFacingDeltaToPosition(v3PredictedPos);
        u16 uFacingDelta = (facingDelta < 0) ? -facingDelta : facingDelta;

        float fInterpolatedValue = InterpolateRangeClamped(1.75f, 2.75f, 32768.0f, 16384.0f, uFacingDelta);

        if (v3PredictedPos.z < 1.0f)
        {
            nlVector3 v3Delta;
            nlVec3Sub2D(v3Delta, v3PredictedPos, m_v3Position);
            float fDistanceSquared = v3Delta.GetLengthSq2D();

            if (fDistanceSquared < fInterpolatedValue * fInterpolatedValue)
            {
                float fMyInterceptAbility = AbleToInterceptBall(this);
                float fOtherInterceptAbility = AbleToInterceptBall(m_pTeam->GetOtherTeam()->GetStriker());

                if (((float)fabs(fOtherInterceptAbility - fMyInterceptAbility) > 0.03f) && (fMyInterceptAbility > fOtherInterceptAbility))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0xC468 | 0x800257A4 | size: 0x1E0
 */
bool cFielder::CanLooseBallPass()
{
    if ((g_pBall->m_pOwner == NULL)
        && (g_pBall->m_pPassTarget == NULL)
        && (g_pBall->m_tShotTimer.m_uPackedTime == 0)
        && (g_pBall->m_unk_0xA6 == 0))
    {
        const cSAnim* guessContactAnim = m_pAnimInventory->GetAnim(gOneTimerLeadGroundContactAnims[0].nAnimID);
        float ratio = GetNormalizedContactTime(guessContactAnim, gOneTimerLeadGroundContactAnims[0].fAnimContactFrame);
        float frames = (float)guessContactAnim->m_nNumKeys / 30.0f;
        float contactTime = ratio * frames;

        nlVector3 v3PredictedPos;
        nlVec3Set(v3PredictedPos,
            (contactTime * g_pBall->m_v3Velocity.x) + g_pBall->m_v3Position.x,
            (contactTime * g_pBall->m_v3Velocity.y) + g_pBall->m_v3Position.y,
            (contactTime * g_pBall->m_v3Velocity.z) + g_pBall->m_v3Position.z);

        s16 facingDelta = GetFacingDeltaToPosition(v3PredictedPos);
        u16 uFacingDelta = (facingDelta < 0) ? -facingDelta : facingDelta;

        float fLooseBallRadius = InterpolateRangeClamped(1.75f, 2.75f, 32768.0f, 16384.0f, uFacingDelta);

        if (g_pBall->m_v3Velocity.z < 0.05f)
        {
            if (v3PredictedPos.z < 1.0f)
            {
                nlVector3 v3Delta;
                nlVec3Sub2D(v3Delta, v3PredictedPos, m_v3Position);
                float fDistanceSquared = v3Delta.GetLengthSq2D();

                if (fDistanceSquared < fLooseBallRadius * fLooseBallRadius)
                {
                    float fMyScore = AbleToInterceptBall(this);
                    float fHisScore = AbleToInterceptBall(m_pTeam->GetOtherTeam()->GetStriker());

                    if (((float)fabs(fHisScore - fMyScore) > 0.03f) && (fMyScore > fHisScore))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0xC410 | 0x8002574C | size: 0x58
 */
bool cFielder::CanReceivePass()
{
    bool bCanReceivePass = false;
    if (!IsFallenDown(0.0f) && (m_tFrozenTimer.m_uPackedTime == 0))
    {
        bCanReceivePass = true;
    }

    return bCanReceivePass;
}

/**
 * Offset/Address/Size: 0xC3D8 | 0x80025714 | size: 0x38
 */
void cFielder::SetMark(cFielder* pMark)
{
    if (m_pMark != nullptr && m_pMark->m_pMarker == this)
    {
        m_pMark->m_pMarker = nullptr;
    }

    m_pMark = pMark;

    if (m_pMark != nullptr)
    {
        pMark->m_pMarker = this;
    }
}

float CalcPenaltyWorth(ePenaltyType eType)
{
    float fMinAmount = 0.0f;
    float fMaxAmount = 0.0f;

    switch (eType)
    {
    case PEN_TYPE_HIT_WITH_BALL:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMinAmount = pTweaks->fPowerupHitWithBallMinAmount;
        fMaxAmount = pTweaks->fPowerupHitWithBallMaxAmount;
        break;
    }
    case PEN_TYPE_HIT_NO_BALL:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMinAmount = pTweaks->fPowerupHitNoBallMinAmount;
        fMaxAmount = pTweaks->fPowerupHitNoBallMaxAmount;
        break;
    }
    case PEN_TYPE_SLIDE_WITH_BALL:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMinAmount = pTweaks->fPowerupSlideWithBallMinAmount;
        fMaxAmount = pTweaks->fPowerupSlideWithBallMaxAmount;
        break;
    }
    case PEN_TYPE_SLIDE_NO_BALL:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMinAmount = pTweaks->fPowerupSlideNoBallMinAmount;
        fMaxAmount = pTweaks->fPowerupSlideNoBallMaxAmount;
        break;
    }
    }

    return InterpolateRangeClamped(fMinAmount, fMaxAmount, 0.0f, 1.0f, nlRandomf(1.0f, &nlDefaultSeed));
}

/**
 * Offset/Address/Size: 0xAEBC | 0x800241F8 | size: 0x151C
 */
void cFielder::CollideWithCharacterCallback(CollisionPlayerPlayerData* pData)
{
    cPlayer* pPlayerCollidedWith = pData->player2;

    if (pPlayerCollidedWith->m_eClassType != FIELDER)
        return;

    cFielder* pFielderCollidedWith = (cFielder*)pPlayerCollidedWith;
    TestCollisionForInvicibility(pFielderCollidedWith);

    if (pFielderCollidedWith->m_pTeam == m_pTeam)
        return;

    if (IsFallenDown(0.0f))
        return;

    u8 gotHit = pFielderCollidedWith->IsHitting();

    if (gotHit)
    {
        u8 hitteeIsHitter = 1;
        u8 bAlsoHitting = IsHitting();

        if (bAlsoHitting)
        {
            float fMyHitTime = fabsf(m_pCurrentAnimController->m_fTime - 0.35f);
            float fOtherHitTime = fabsf(pFielderCollidedWith->m_pCurrentAnimController->m_fTime - 0.35f);
            if (fMyHitTime <= fOtherHitTime)
                hitteeIsHitter = 0;
        }

        if (!hitteeIsHitter)
            return;

        float thisRadius, otherRadius;
        m_pPhysicsCharacter->m_pPlayerPlayerColumn->GetRadius(&thisRadius);
        pFielderCollidedWith->m_pPhysicsCharacter->m_pPlayerPlayerColumn->GetRadius(&otherRadius);
        float combinedRadius = thisRadius + otherRadius;

        float sinVal, cosVal;
        nlSinCos(&sinVal, &cosVal, pFielderCollidedWith->m_aActualFacingDirection);

        nlVector3 adjustedPosition;
        adjustedPosition = pFielderCollidedWith->m_v3Position;
        adjustedPosition.x += cosVal * combinedRadius;
        adjustedPosition.y += sinVal * combinedRadius;

        float closingSpeed = GetClosingSpeed(adjustedPosition, pData->velocity1, pFielderCollidedWith->m_v3Position, v3Zero);
        float attackIntensity = NormalizeVal(closingSpeed, -m_pTweaks->fRunningSpeed, m_pTweaks->fRunningSpeed);

        u8 canPickup = 0;
        if (m_pBall != 0 && attackIntensity > 0.4f)
            canPickup = 1;

        InitActionHitReact(pFielderCollidedWith, pFielderCollidedWith->m_aActualFacingDirection, canPickup != 0);
        BeginRumbleAction(RUMBLE_SOLID_CONTACT, pFielderCollidedWith->GetGlobalPad());

        Event* pEvent = g_pEventManager->CreateValidEvent(0x17, 0x28);
        PlayerAttackData* pAttackData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) PlayerAttackData();
        pAttackData->pAttacker = pFielderCollidedWith;
        u8 bHasGlobalPad = pFielderCollidedWith->GetGlobalPad() != 0;
        pAttackData->nAttackerPadID = bHasGlobalPad ? pFielderCollidedWith->GetGlobalPad()->m_padIndex : -1;
        pAttackData->pTarget = this;
        pAttackData->fAttackIntensity = attackIntensity;
    }
    else
    {
        u8 isOpponentSlideAttacking;
        if (pFielderCollidedWith->m_tFrozenTimer.m_uPackedTime == 0 && pFielderCollidedWith->m_eActionState == ACTION_SLIDE_ATTACK && (pFielderCollidedWith->mActionSlideAttackVars.eSlideAttackState == SLIDE_ATTACK_DOWN || pFielderCollidedWith->mActionSlideAttackVars.eSlideAttackState == SLIDE_ATTACK_DECELERATE))
        {
            isOpponentSlideAttacking = 1;
        }
        else
        {
            isOpponentSlideAttacking = 0;
        }

        if (!isOpponentSlideAttacking)
            return;

        if (m_eActionState == ACTION_HIT)
            return;

        s16 nHitteeToHitterFacingDelta = pFielderCollidedWith->GetFacingDeltaToPosition(m_v3Position);
        s16 nHitterToHitteeFacingDelta = GetFacingDeltaToPosition(pFielderCollidedWith->m_v3Position);

        u16 uAbsFacingDelta = (nHitteeToHitterFacingDelta < 0) ? -nHitteeToHitterFacingDelta : nHitteeToHitterFacingDelta;
        if (uAbsFacingDelta >= 0x4000)
            return;

        u8 isThisSlideAttacking;
        if (m_tFrozenTimer.m_uPackedTime == 0 && m_eActionState == ACTION_SLIDE_ATTACK && (mActionSlideAttackVars.eSlideAttackState == SLIDE_ATTACK_DOWN || mActionSlideAttackVars.eSlideAttackState == SLIDE_ATTACK_DECELERATE))
        {
            isThisSlideAttacking = 1;
        }
        else
        {
            isThisSlideAttacking = 0;
        }

        if (isThisSlideAttacking)
        {
            u16 uAbsHitterDelta = (nHitterToHitteeFacingDelta < 0) ? -nHitterToHitteeFacingDelta : nHitterToHitteeFacingDelta;

            if (uAbsHitterDelta < 0x4000)
            {
                if (m_fActualSpeed < pFielderCollidedWith->m_fActualSpeed)
                {
                    if (m_pBall != 0)
                        pFielderCollidedWith->DoPenaltyCardBooking(this, PEN_TYPE_SLIDE_WITH_BALL);
                    else
                        pFielderCollidedWith->DoPenaltyCardBooking(this, PEN_TYPE_SLIDE_NO_BALL);

                    InitActionSlideAttackReact(pFielderCollidedWith, false);
                    pFielderCollidedWith->mActionSlideAttackVars.bAttackSucceeded = true;
                    BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, pFielderCollidedWith->GetGlobalPad());

                    u16 uAbsDelta = (nHitterToHitteeFacingDelta < 0) ? -nHitterToHitteeFacingDelta : nHitterToHitteeFacingDelta;
                    if (uAbsDelta >= 0x4000)
                    {
                        if (pFielderCollidedWith->m_pBall == 0)
                            pFielderCollidedWith->InitActionSlideAttackFailReact();
                    }
                    else
                    {
                        if (pFielderCollidedWith->CanPickupBall(g_pBall))
                        {
                            pFielderCollidedWith->PickupBall(g_pBall);
                            pFielderCollidedWith->DoSlideAttackStats();
                        }
                    }
                }
                else
                {
                    if (pFielderCollidedWith->m_pBall != 0)
                        DoPenaltyCardBooking(pFielderCollidedWith, PEN_TYPE_SLIDE_WITH_BALL);
                    else
                        DoPenaltyCardBooking(pFielderCollidedWith, PEN_TYPE_SLIDE_NO_BALL);

                    pFielderCollidedWith->InitActionSlideAttackReact(this, false);
                    mActionSlideAttackVars.bAttackSucceeded = true;
                    BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, GetGlobalPad());

                    u16 uAbsDelta = (nHitterToHitteeFacingDelta < 0) ? -nHitterToHitteeFacingDelta : nHitterToHitteeFacingDelta;
                    if (uAbsDelta >= 0x4000)
                    {
                        if (m_pBall == 0)
                            InitActionSlideAttackFailReact();
                    }
                    else
                    {
                        if (CanPickupBall(g_pBall))
                        {
                            PickupBall(g_pBall);
                            DoSlideAttackStats();
                        }
                    }
                }
            }
            else
            {
                if (m_pBall != 0)
                    pFielderCollidedWith->DoPenaltyCardBooking(this, PEN_TYPE_SLIDE_WITH_BALL);
                else
                    pFielderCollidedWith->DoPenaltyCardBooking(this, PEN_TYPE_SLIDE_NO_BALL);

                InitActionSlideAttackReact(pFielderCollidedWith, false);
                pFielderCollidedWith->mActionSlideAttackVars.bAttackSucceeded = true;
                BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, pFielderCollidedWith->GetGlobalPad());

                u16 uAbsDelta = (nHitterToHitteeFacingDelta < 0) ? -nHitterToHitteeFacingDelta : nHitterToHitteeFacingDelta;
                if (uAbsDelta >= 0x4000)
                {
                    if (pFielderCollidedWith->m_pBall == 0)
                        pFielderCollidedWith->InitActionSlideAttackFailReact();
                }
                else
                {
                    if (pFielderCollidedWith->CanPickupBall(g_pBall))
                    {
                        pFielderCollidedWith->PickupBall(g_pBall);
                        pFielderCollidedWith->DoSlideAttackStats();
                    }
                }
            }
        }
        else
        {
            if (m_pBall != 0)
                pFielderCollidedWith->DoPenaltyCardBooking(this, PEN_TYPE_SLIDE_WITH_BALL);
            else
                pFielderCollidedWith->DoPenaltyCardBooking(this, PEN_TYPE_SLIDE_NO_BALL);

            InitActionSlideAttackReact(pFielderCollidedWith, false);
            pFielderCollidedWith->mActionSlideAttackVars.bAttackSucceeded = true;
            BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, pFielderCollidedWith->GetGlobalPad());

            u16 uAbsDelta = (nHitterToHitteeFacingDelta < 0) ? -nHitterToHitteeFacingDelta : nHitterToHitteeFacingDelta;
            if (uAbsDelta >= 0x4000)
            {
                if (pFielderCollidedWith->m_pBall == 0)
                    pFielderCollidedWith->InitActionSlideAttackFailReact();
            }
            else
            {
                if (pFielderCollidedWith->CanPickupBall(g_pBall))
                {
                    pFielderCollidedWith->PickupBall(g_pBall);
                    pFielderCollidedWith->DoSlideAttackStats();
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xAD6C | 0x800240A8 | size: 0x150
 */
bool cFielder::CollideWithShellCallback(ePowerupSize eSize, bool bUnknown, const nlVector3& rv3Pos1, const nlVector3& rv3Pos2)
{
    if (!IsFallenDown(0.0f) && (m_eActionState != ACTION_POST_WHISTLE))
    {
        bool bShouldSkip = false;
        if ((m_ePowerup == POWER_UP_STAR && m_tPowerupEffectTime.m_uPackedTime != 0) || (mActionShootToScoreVars.isCurrentlyInvincible != 0))
        {
            bShouldSkip = true;
        }

        if (!bShouldSkip)
        {
            if (eSize == POWERUPSIZE_LARGE)
            {
                InitActionSquishReact(rv3Pos2);
            }
            else
            {
                InitActionShellReact(rv3Pos1, rv3Pos2);

                ePowerupSize eSoundSize = eSize;
                if (bUnknown)
                {
                    eSoundSize = POWERUPSIZE_LARGE;
                }

                switch (eSoundSize)
                {
                case POWERUPSIZE_SMALL:
                    PlayAttackReactionSounds(g_pGame->m_pGameTweaks->fSmallShellHitReactionVolume);
                    break;
                case POWERUPSIZE_MEDIUM:
                    PlayAttackReactionSounds(g_pGame->m_pGameTweaks->fMediumShellHitReactionVolume);
                    break;
                case POWERUPSIZE_LARGE:
                    PlayAttackReactionSounds(g_pGame->m_pGameTweaks->fBombHitReactionVolume);
                    break;
                }
            }

            if (g_pBall->m_pPassTarget != nullptr && g_pBall->m_pPassTarget == this)
            {
                g_pBall->ClearPassTarget();
            }

            return true;
        }
    }
    return false;
}

void cFielder::CollideWithSidelineFragmentCallback(const nlVector3& v3CollisionLocation, const nlVector3& v3CollisionVelocity)
{
    if (!IsFallenDown(0.0f) && m_eActionState != ACTION_POST_WHISTLE)
    {
        bool bShouldSkip = false;
        if ((m_ePowerup == POWER_UP_STAR && m_tPowerupEffectTime.m_uPackedTime != 0) || mActionShootToScoreVars.isCurrentlyInvincible != 0)
        {
            bShouldSkip = true;
        }

        if (!bShouldSkip)
        {
            ClearPassTargetIfAmThePassTarget();
            InitActionShellReact(v3CollisionLocation, v3CollisionVelocity);
        }
    }
}

/**
 * Offset/Address/Size: 0xAB90 | 0x80023ECC | size: 0x1DC
 */
bool cFielder::CollideWithFreezeCallback()
{
    bool bShouldSkip = false;

    if ((m_ePowerup == POWER_UP_STAR && m_tPowerupEffectTime.m_uPackedTime != 0) || mActionShootToScoreVars.isCurrentlyInvincible != 0)
    {
        bShouldSkip = true;
    }

    if ((!bShouldSkip) && (m_tFrozenTimer.m_uPackedTime == 0))
    {
        if (m_eAnimID != 0x74 && m_eAnimID != 0x75)
        {

            if (m_pBall != nullptr)
            {
                ReleaseBall();
                ShootBallDueToContact(m_v3Velocity);

                switch (m_eActionState)
                {
                case ACTION_SHOT:
                    break;
                case ACTION_RUNNING_WB:
                case ACTION_RUNNING_WB_TURBO:
                case ACTION_RUNNING_WB_TURBO_TURN:
                case ACTION_SHOOT_TO_SCORE:
                    InitActionRunning();
                    break;
                }

                m_pShotMeter->Abort(this);
            }

            float fFreezeDuration = g_pGame->m_pGameTweaks->fFreezeShellFrozenTime;
            m_tFrozenTimer.SetSeconds(fFreezeDuration);

            m_fDesiredSpeed = 0.0f;
            m_fActualSpeed = m_fDesiredSpeed;
            SetVelocity(v3Zero);

            if ((fFreezeDuration > 0.0f) && (m_eFielderDesireState != FIELDERDESIRE_FINISH_ACTION))
            {
                g_pGame->AbortPendingThought(mThoughtHashCalcDesire);
                g_pGame->AbortPendingThought(mThoughtHashInitRunToNet);
                g_pGame->AbortPendingThought(mThoughtHashInitGetOpen);
                g_pGame->AbortPendingThought(mThoughtHashInitWindupPass);
                g_pGame->AbortPendingThought(mThoughtHashInitCutAndBreak);
                ClearQueuedDesire();
                EndDesire(false);
            }

            SetVelocity(v3Zero);
            m_fDesiredSpeed = 0.0f;
            m_fActualSpeed = m_fDesiredSpeed;

            KillDaze(this);
            EmitFreeze(this);

            if (g_pBall->m_pPassTarget != nullptr && g_pBall->m_pPassTarget == this)
            {
                g_pBall->ClearPassTarget();
            }
            return true;
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0xAACC | 0x80023E08 | size: 0xC4
 */
bool cFielder::CollideWithBananaCallback(const nlVector3& rv3BananaPosition)
{
    if (!IsFallenDown(0.0f) && (m_eActionState != ACTION_POST_WHISTLE))
    {
        bool bShouldSkip = false;
        if ((m_ePowerup == POWER_UP_STAR && m_tPowerupEffectTime.m_uPackedTime != 0) || mActionShootToScoreVars.isCurrentlyInvincible != 0)
        {
            bShouldSkip = true;
        }

        if ((!bShouldSkip) && (m_tFrozenTimer.m_uPackedTime == 0))
        {
            if ((g_pBall->m_pPassTarget != nullptr) && (g_pBall->m_pPassTarget == this))
            {
                g_pBall->ClearPassTarget();
            }
            InitActionBananaReact(rv3BananaPosition);
            return true;
        }
    }
    return false;
}

void cFielder::CollideWithBobombCallback(const nlVector3& v3CollisionLocation, float fBombRadius)
{
    bool bShouldSkip = false;
    if ((m_ePowerup == POWER_UP_STAR && m_tPowerupEffectTime.m_uPackedTime != 0) || mActionShootToScoreVars.isCurrentlyInvincible != 0)
    {
        bShouldSkip = true;
    }

    if (!bShouldSkip && m_tFrozenTimer.m_uPackedTime == 0)
    {
        if (g_pBall->m_pPassTarget != NULL && g_pBall->m_pPassTarget == this)
        {
            g_pBall->ClearPassTarget();
        }
        mbWasHitByPowerupThisFrame = true;
        InitActionBombReact(v3CollisionLocation, fBombRadius);
    }
}

static inline bool IsGameplayOrOvertime(eGameState state)
{
    bool result = false;
    if (state == GS_GAMEPLAY || state == GS_OVERTIME)
    {
        result = true;
    }
    return result;
}

/**
 * Offset/Address/Size: 0xA910 | 0x80023C4C | size: 0x1BC
 */
void cFielder::CollideWithChainCallback(ChainChomp* pChainChomp)
{
    if (mActionShootToScoreVars.isInUnbreakablePart == 0)
    {
        if (g_pBall->m_pPassTarget != nullptr && g_pBall->m_pPassTarget == this)
        {
            g_pBall->ClearPassTarget();
        }

        if (!IsFallenDown(0.0f))
        {
            PowerupBase::PlayPowerupSound(POWER_UP_CHAIN_CHOMP, PowerupBase::PWRUP_SOUND_HIT, pChainChomp->mpPhysObj, 100.0f);
        }

        float offsetZ = 0.0f;

        float rightFootZ;
        float leftFootZ;
        float ballJointZ;

        leftFootZ = offsetZ + GetJointPosition(m_nLeftFootJointIndex).z;
        rightFootZ = offsetZ + GetJointPosition(m_nRightFootJointIndex).z;
        ballJointZ = offsetZ + GetJointPosition(m_nHeadJointIndex).z;

        bool bInAir;
        if (leftFootZ > 1.0f && rightFootZ > 1.0f && ballJointZ > 1.0f)
        {
            bInAir = true;
        }
        else
        {
            bInAir = false;
        }

        if (bInAir)
        {
            InitActionBombReact(m_v3Position, 0.0f);
            EmitTackleImpact(this);
        }
        else
        {
            InitActionSquishReact(pChainChomp->mv3Velocity);
            EmitChainBite(this);
        }

        if (pChainChomp->mpThrower != nullptr)
        {
            if (g_pGame->IsGameplayOrOvertime() && !IsOnSameTeam(pChainChomp->mpThrower))
            {
                cFielder* pThrower = pChainChomp->mpThrower;
                nlSingleton<StatsTracker>::Instance()->TrackStat(
                    STATS_POWERUPS_HIT,
                    pThrower->m_pTeam->m_nSide,
                    pThrower->m_ID,
                    pChainChomp->mnThrowerPadID,
                    0,
                    0,
                    0);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xA804 | 0x80023B40 | size: 0x10C
 */
void cFielder::CollideWithBowserCallback(Bowser* pBowser)
{
    if (mActionShootToScoreVars.isInUnbreakablePart == 0)
    {
        if (g_pBall->m_pPassTarget != nullptr && g_pBall->m_pPassTarget == this)
        {
            g_pBall->ClearPassTarget();
        }

        if (g_pBall->m_pOwner == this)
        {
            ReleaseBall();

            nlVector3 v3Direction;
            nlVec3Set(v3Direction,
                g_pBall->m_v3Position.x - pBowser->mv3Position.x,
                g_pBall->m_v3Position.y - pBowser->mv3Position.y,
                g_pBall->m_v3Position.z - pBowser->mv3Position.z);
            nlVec3Scale(v3Direction, v3Direction, 2.0f);

            v3Direction.z = 3.0f + nlRandomf(4.0f, &nlDefaultSeed);

            g_pBall->ShootRelease(v3Direction, SPINTYPE_NONE);
        }

        if (!IsFallenDown(0.0f))
        {
            InitActionBombHitReact(pBowser->mv3Position);
        }
    }
}

/**
 * Offset/Address/Size: 0xA70C | 0x80023A48 | size: 0xF8
 */
void cFielder::CollideWithWallCallback(const CollisionPlayerWallData* eventData)
{
    cPlayer::CollideWithWallCallback(eventData);

    if (CanGetElectrocuted(eventData))
    {
        InitActionElectrocution(eventData->contactPoint, eventData->wallNormal);
    }
}

/**
 * Offset/Address/Size: 0xA6D0 | 0x80023A0C | size: 0x3C
 */
void cFielder::ClearPassTargetIfAmThePassTarget()
{
    cBall* pBall = g_pBall;
    if (pBall->m_pPassTarget != nullptr)
    {
        if (pBall->m_pPassTarget == this)
        {
            pBall->ClearPassTarget();
        }
    }
}

/**
 * Offset/Address/Size: 0xA6B8 | 0x800239F4 | size: 0x18
 */
bool cFielder::UsePerfectPass()
{
    return g_pBall->mbHyperSTS != 0;
}

/**
 * Offset/Address/Size: 0xA6A8 | 0x800239E4 | size: 0x10
 */
bool cFielder::IsPlayingPowerupAnim()
{
    return m_nPowerupAnimID >= 0;
}

/**
 * Offset/Address/Size: 0xA5DC | 0x80023918 | size: 0xCC
 */
bool cFielder::IsCharacterInAir(bool bUseOffset) const
{
    f32 offsetZ = 0.0f;
    if (bUseOffset)
    {
        offsetZ = m_v3Position.z;
    }

    f32 leftFootZ = offsetZ + GetJointPosition(m_nLeftFootJointIndex).z;
    f32 rightFootZ = offsetZ + GetJointPosition(m_nRightFootJointIndex).z;
    f32 headZ = offsetZ + GetJointPosition(m_nHeadJointIndex).z;

    if (leftFootZ > 1.0f && rightFootZ > 1.0f && headZ > 1.0f)
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0xA548 | 0x80023884 | size: 0x94
 */
bool cFielder::IsTurboing()
{
    if (m_pBall != nullptr)
    {
        bool bTurboing = true;
        if (!IsTurboingAnimID())
        {
            bTurboing = false;
        }
        return bTurboing;
    }
    else
    {
        bool bTurboing = true;
        if (!IsTurboingWithoutBallAnimID())
        {
            bTurboing = false;
        }
        return bTurboing;
    }
}

/**
 * Offset/Address/Size: 0xA50C | 0x80023848 | size: 0x3C
 */
bool cFielder::IsRunning() const
{
    bool bRunning = false;
    if (m_eActionState == ACTION_RUNNING || IsRunningWithBall())
    {
        bRunning = true;
    }
    return bRunning;
}

bool cFielder::IsRunningWithBall() const
{
    bool bRunningWithBall = false;
    if (m_eActionState == ACTION_RUNNING_WB || m_eActionState == ACTION_RUNNING_WB_TURBO || m_eActionState == ACTION_RUNNING_WB_TURBO_TURN)
    {
        bRunningWithBall = true;
    }
    return bRunningWithBall;
}

/**
 * Offset/Address/Size: 0xA4D8 | 0x80023814 | size: 0x34
 */
bool cFielder::IsInvincible() const
{
    bool result = false;

    if (((m_ePowerup == POWER_UP_STAR) && (m_tPowerupEffectTime.m_uPackedTime != 0)) || (mActionShootToScoreVars.isCurrentlyInvincible != 0))
    {
        result = true;
    }

    return result;
}

/**
 * Offset/Address/Size: 0xA49C | 0x800237D8 | size: 0x3C
 */
bool cFielder::IsBallAwayFromCarrier() const
{
    if (m_eActionState == ACTION_RUNNING_WB_TURBO)
    {
        if ((m_pCurrentAnimController->m_fTime > 0.2f) && (m_pCurrentAnimController->m_fTime < 0.975f))
        {
            return true;
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0xA468 | 0x800237A4 | size: 0x34
 */
bool cFielder::IsReceivingVolleyPass() const
{
    eFielderDesireState desireState = m_eFielderDesireState;
    bool result = false;

    if (desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE || desireState == FIELDERDESIRE_RECEIVE_PASS_FROM_RUN)
    {
        result = m_DesireReceivePassSharedVars.bVolleyPassReceive;
    }
    else if (desireState == FIELDERDESIRE_ONETIMER)
    {
        result = m_DesireOneTimerVars.bVolleyPassReceive;
    }

    return result;
}

/**
 * Offset/Address/Size: 0xA444 | 0x80023780 | size: 0x24
 */
bool cFielder::IsPreparingForOneTimer() const
{
    bool result = false;
    if (m_eFielderDesireState == FIELDERDESIRE_ONETIMER)
    {
        result = m_eDesireSubState == 0;
    }
    return result;
}

/**
 * Offset/Address/Size: 0xA2D0 | 0x8002360C | size: 0x174
 */
void cFielder::CleanUpAction()
{
    switch (m_eActionState)
    {
    case ACTION_HIT:
        Audio::gCrowdSFX.Stop((Audio::eWorldSFX)0x9F, cGameSFX::SFX_STOP_FIRST);
        break;

    case ACTION_DEKE:
        m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.0f;
        break;

    case ACTION_ELECTROCUTION:
        m_v3Position.z = 0.0f;
        m_v3Velocity.z = 0.0f;
        break;

    case ACTION_LOOSE_BALL_PASS:
        m_pPhysicsCharacter->m_CanCollideWithWall = true;
        SetNoPickUpTime(0.0f);
        mActionLooseBallPassVars.bVolleyPass = false;
        break;

    case ACTION_LOOSE_BALL_SHOT:
        m_pPhysicsCharacter->m_CanCollideWithWall = true;
        SetNoPickUpTime(0.0f);
        mActionLooseBallShotVars.bIsChipShot = false;
        break;

    case ACTION_ONETIMER:
        EndBlur();
        mActionShotVars.bIsChipShot = false;
        break;

    case ACTION_PASS:
        mActionPassingVars.pPassTarget = NULL;
        mActionPassingVars.bVolleyPass = false;
        break;

    case ACTION_RUNNING:
        mActionRunningVars.eLastStrafeDirection = STRAFE_IDLE;
        break;

    case ACTION_RUNNING_WB:
        m_eLastPadAction = PAD_NONE;
        break;

    case ACTION_RUNNING_WB_TURBO:
        m_eLastPadAction = PAD_NONE;
        if (m_ePowerup != POWER_UP_MUSHROOM)
        {
            EndBlur();
        }
        break;

    case ACTION_SHOOT_TO_SCORE:
        CleanActionShootToScore();
        break;

    case ACTION_SHOT:
        mActionShotVars.bIsChipShot = false;
        break;

    case ACTION_SLIDE_ATTACK:
        Audio::gCrowdSFX.Stop((Audio::eWorldSFX)0x9F, cGameSFX::SFX_STOP_FIRST);
        KillSlideTackleTrail(this);
        StopSFX(Audio::CHARSFX_SLIDE);
        break;

    case ACTION_SQUISH_REACT:
        KillDaze(this);
        break;

    case ACTION_LATE_ONETIMER_FROM_VOLLEY:
    case ACTION_SLIDE_ATTACK_REACT:
    case ACTION_BOMB_REACT:
    case ACTION_SHELL_REACT:
    case ACTION_BANANA_REACT:
    case ACTION_STS_HIT_REACT:
    default:
        break;
    }

    m_eActionState = ACTION_NEED_ACTION;
}

/**
 * Offset/Address/Size: 0xA258 | 0x80023594 | size: 0x78
 */
void cFielder::CleanUpPowerupEffect()
{
    switch (m_ePowerup)
    {
    case POWER_UP_STAR:
        KillStar(this);
        m_ePowerup = POWER_UP_NONE;
        mnNumPowerups = 0;
        m_tPowerupEffectTime.m_uPackedTime = 0;
        break;

    case POWER_UP_MUSHROOM:
        KillMushroom(this);
        m_ePowerup = POWER_UP_NONE;
        mnNumPowerups = 0;
        m_tPowerupEffectTime.m_uPackedTime = 0;
        break;

    default:
        break;
    }
}

float cFielder::GetShotProbability(float fValue, Goalie* pGoalie)
{
    extern cTeam* g_pCurrentlyUpdatingTeam;

    float fProb;
    if (fValue < SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue1)
    {
        fProb = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance0,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance1,
            0.0f,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue1,
            fValue);
    }
    else if (fValue < SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue2)
    {
        fProb = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance1,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance2,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue1,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue2,
            fValue);
    }
    else if (fValue < SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue3)
    {
        fProb = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance2,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance3,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue2,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue3,
            fValue);
    }
    else
    {
        fProb = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance3,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance4,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue3,
            1.0f,
            fValue);
    }
    return fProb;
}

/**
 * Offset/Address/Size: 0x9DFC | 0x80023138 | size: 0x45C
 */
void cFielder::CalcRegularShot(nlVector3& rv3Vel, nlVector3& rv3Target)
{
    u32 uSaveType = 0x10000 - 4;
    float fShotValue = m_pShotMeter->m_fScoreValue;
    Goalie* pGoalie = m_pTeam->GetOtherTeam()->GetGoalie();
    float fGoalieEnergy = pGoalie->mFatigue.mfEnergyLevel;
    GoalieTweaks* pGoalieTweaks = (GoalieTweaks*)pGoalie->m_pTweaks;

    bool bIsChipShot = mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot;

    if (bIsChipShot
        || ((fGoalieEnergy >= pGoalieTweaks->fFatigueCatchThreshold)
            && (m_pShotMeter->m_fSpeedValue < pGoalieTweaks->fCatchSaveMaxSpeed)))
    {
        uSaveType = 0x10000 - 1;
    }

    pGoalie->muSaveType = uSaveType;

    cBall* pBall = g_pBall;
    float fShotTime;

    DoFindBestShotTarget(rv3Target, fShotTime, false);

    float fDesiredTime = 1.0f / fShotTime;
    float fDeltaX = pBall->m_v3Position.x - rv3Target.x;
    float fDeltaY = pBall->m_v3Position.y - rv3Target.y;
    float fDistance = nlSqrt((fDeltaX * fDeltaX) + (fDeltaY * fDeltaY), true);
    fDesiredTime = fDistance * fDesiredTime;

    bool bIsChipShot2 = false;
    if (mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot)
    {
        bIsChipShot2 = true;
    }

    float fAccuracyScale;
    if (bIsChipShot2)
    {
        fAccuracyScale = (1.1f - fShotValue) * (1.5f * fDistance);
    }
    else
    {
        fAccuracyScale = fShotTime * (1.1f - fShotValue);
    }

    GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;
    float fYAccuracy = fAccuracyScale * pGameTweaks->fShotWidthVariance;
    float fZAccuracy = fAccuracyScale * pGameTweaks->fShotHeightVariance;

    static FilteredRandomReal randgenYAccuracy;
    static FilteredRandomReal randgenZAccuracy;

    float fYRand = fYAccuracy * randgenYAccuracy.genrand();
    rv3Target.y += 0.5f * fYAccuracy - fYRand;
    float fZOffset = fZAccuracy * randgenZAccuracy.genrand();
    rv3Target.z += fZOffset;

    g_pBall->ShootAtFast(rv3Vel, rv3Target, fDesiredTime);

    extern cTeam* g_pCurrentlyUpdatingTeam;

    if (fShotValue < SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue1)
    {
        fShotValue = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance0,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance1,
            0.0f,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue1,
            fShotValue);
    }
    else if (fShotValue < SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue2)
    {
        fShotValue = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance1,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance2,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue1,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue2,
            fShotValue);
    }
    else if (fShotValue < SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue3)
    {
        fShotValue = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance2,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance3,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue2,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue3,
            fShotValue);
    }
    else
    {
        fShotValue = InterpolateRangeClamped(
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance3,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotChance4,
            SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fShotValue3,
            1.0f,
            fShotValue);
    }

    static FilteredRandomReal randgenMiss;

    if ((100.0f * randgenMiss.genrand()) < fShotValue)
    {
        pGoalie->mbShouldMiss = true;
    }
    else
    {
        pGoalie->mbShouldMiss = false;
    }
}

/**
 * Offset/Address/Size: 0x95C8 | 0x80022904 | size: 0x834
 */
void cFielder::CalcShootToScoreShot(nlVector3& v3BallVelocity, nlVector3& v3BallTarget)
{
    float fShotSpeed;
    float fDist2Goalie;
    eShootToScoreResult result;
    float fSTSValue;
    float fInvSpeed;
    float fDesiredTime;
    float fTime2Goalie;
    nlVector3 v3InterceptPos;
    Goalie* pGoalie;
    nlVector3* goaliePos;
    nlVector3* ballPos = &g_pBall->m_v3Position;
    DoFindBestShotTarget(v3BallTarget, fShotSpeed, true);
    pGoalie = m_pTeam->GetOtherTeam()->GetGoalie();
    result = meS2SResult;
    fSTSValue = 1.0f;
    goaliePos = &pGoalie->m_v3Position;
    if (result == S2S_SAVED_YELLOW)
    {
        fInvSpeed = m_pShotMeter->m_fSTSValue;
        fSTSValue = fInvSpeed;
        {
            static FilteredRandomReal randgenSTS;
            if (0.95f * randgenSTS.genrand() < fInvSpeed)
            {
                result = S2S_SAVED;
            }
        }
    }
    if (result == S2S_SAVED)
    {
        fInvSpeed = 1.2f / (1.25f * fShotSpeed);
    }
    else
    {
        fInvSpeed = 1.0f / (1.25f * fShotSpeed);
    }
    {
        float dx = ballPos->x - v3BallTarget.x;
        float dy = ballPos->y - v3BallTarget.y;
        float fDist = nlSqrt(dx * dx + dy * dy, true);
        fDesiredTime = fDist * fInvSpeed;
    }
    g_pBall->ShootAtFast(v3BallVelocity, v3BallTarget, fDesiredTime);
    {
        float dx = ballPos->x - goaliePos->x;
        float dy = ballPos->y - goaliePos->y;
        fDist2Goalie = nlSqrt(dx * dx + dy * dy, true);
    }
    ReleaseBall();
    g_pBall->ShootRelease(v3BallVelocity, (eSpinType)0);
    fTime2Goalie = pGoalie->CalcTimeToPlane();
    v3InterceptPos = pGoalie->mv3TargetPosition;
    switch (result)
    {
    case S2S_SAVED:
    {
        static FilteredRandomReal randgenSwat;
        if (fDist2Goalie > 7.0f && 100.0f * randgenSwat.genrand() < 66.0f * fSTSValue)
        {
            int nSide = -1;
            if (nlAbs(ballPos->x) > 4.0f)
            {
                if (ballPos->y * ballPos->x < 0.0f)
                {
                    nSide = 0;
                }
                else
                {
                    nSide = 1;
                }
            }
            pGoalie->ChooseSwatAnim(nSide);
            GetWorldPoint(v3BallTarget, *(nlVector3*)pGoalie->mpLooseBallInfo, *goaliePos, pGoalie->m_aActualFacingDirection);
            v3BallTarget.z += InterpolateRangeClamped(0.0f, 0.8f, 6.0f, 20.0f, fDist2Goalie);
            if (fTime2Goalie >= pGoalie->mpLooseBallInfo->mfPickupTime * pGoalie->mpLooseBallInfo->mfAnimDuration)
            {
                fDesiredTime = fTime2Goalie;
            }
            else
            {
                fDesiredTime = pGoalie->mpLooseBallInfo->mfPickupTime * pGoalie->mpLooseBallInfo->mfAnimDuration;
            }
            g_pBall->m_unk_0xA6 = true;
        }
        else
        {
            pGoalie->mbShouldMiss = false;
            unsigned int uSaveType;
            if (nlRandomf(1.0f, &nlDefaultSeed) < 0.75f)
            {
                uSaveType = 0xFFFF;
            }
            else
            {
                uSaveType = 0xFFFC;
            }
            fTime2Goalie = 0.1f + fTime2Goalie;
            unsigned short aSaveAngle = pGoalie->CalcBestSave(fTime2Goalie, *ballPos, v3InterceptPos, uSaveType, true);
            if (pGoalie->mpSaveData != NULL)
            {
                nlVector3 v3WorldSavePos;
                GetWorldPoint(v3WorldSavePos, pGoalie->mBlendInfo.mv3BlendedSavePos, *goaliePos, aSaveAngle);
                float sdx;
                float sdy;
                float sdz;
                sdy = v3WorldSavePos.y - v3InterceptPos.y;
                sdx = v3WorldSavePos.x - v3InterceptPos.x;
                sdz = v3WorldSavePos.z - v3InterceptPos.z;
                if (sdx * sdx + sdy * sdy + sdz * sdz > 1.0f)
                {
                    pGoalie->mpSaveData = NULL;
                }
                else
                {
                    v3BallTarget = v3WorldSavePos;
                }
            }
            if (pGoalie->mpSaveData == NULL)
            {
                cNet* pNet = pGoalie->m_pTeam->m_pNet;
                v3BallTarget.x = pNet->m_v3NetLocation.x;
                float fNetY = 0.5f * cNet::m_fNetWidth + 0.1f;
                if (v3BallTarget.y > 0.0f)
                {
                    v3BallTarget.y = fNetY;
                }
                else
                {
                    v3BallTarget.y = -fNetY;
                }
                float dx = ballPos->x - v3BallTarget.x;
                float dy = ballPos->y - v3BallTarget.y;
                float dist = nlSqrt(dx * dx + dy * dy, true);
                float fPercent = fDist2Goalie / dist;
                v3InterceptPos.x = (1.0f - fPercent) * ballPos->x + fPercent * v3BallTarget.x;
                v3InterceptPos.y = (1.0f - fPercent) * ballPos->y + fPercent * v3BallTarget.y;
                v3InterceptPos.z = (1.0f - fPercent) * ballPos->z + fPercent * v3BallTarget.z;
                pGoalie->CalcBestSave(0.6f, *ballPos, v3InterceptPos, 0xFFFC, true);
            }
            else
            {
                fDesiredTime = fTime2Goalie;
            }
        }
        g_pBall->ShootAtFast(v3BallVelocity, v3BallTarget, fDesiredTime);
        break;
    }
    case S2S_SAVED_YELLOW:
    {
        pGoalie->FindSTSStunData();
        GetWorldPoint(v3BallTarget, pGoalie->mpSaveData->mv3SavePos, *goaliePos, pGoalie->m_aActualFacingDirection);
        if (fTime2Goalie < 0.03f || CalculateDistanceSquared(v3InterceptPos, v3BallTarget) > 4.0f)
        {
            cNet* pNet = pGoalie->m_pTeam->m_pNet;
            v3BallTarget.x = pNet->m_v3NetLocation.x;
            float fNetY = 0.5f * cNet::m_fNetWidth + 0.1f;
            if (v3BallTarget.y > 0.0f)
            {
                v3BallTarget.y = fNetY;
            }
            else
            {
                v3BallTarget.y = -fNetY;
            }
            float dx = ballPos->x - v3BallTarget.x;
            float dy = ballPos->y - v3BallTarget.y;
            float dist = nlSqrt(dx * dx + dy * dy, true);
            float fPercent = fDist2Goalie / dist;
            v3InterceptPos.x = (1.0f - fPercent) * ballPos->x + fPercent * v3BallTarget.x;
            v3InterceptPos.y = (1.0f - fPercent) * ballPos->y + fPercent * v3BallTarget.y;
            v3InterceptPos.z = (1.0f - fPercent) * ballPos->z + fPercent * v3BallTarget.z;
            pGoalie->CalcBestSave(0.5f, *ballPos, v3InterceptPos, 0xFFFC, true);
        }
        else
        {
            fDesiredTime = fTime2Goalie;
        }
        pGoalie->mbShouldMiss = false;
        g_pBall->ShootAtFast(v3BallVelocity, v3BallTarget, fDesiredTime);
        break;
    }
    case S2S_SCORE:
    {
        pGoalie->mbShouldMiss = true;
        pGoalie->CalcSaveParameters(
            ((0.05f + fTime2Goalie) >= 0.2f) ? (0.05f + fTime2Goalie) : 0.2f, 0xFFFC, false, true);
        break;
    }
    case S2S_SUPER_SHOT:
    {
        nlVector3 v3MagicPos = m_v3Position;
        if ((f32)fabs(pGoalie->m_v3Position.y) > (0.5f * cNet::m_fNetWidth - 0.5f))
        {
            cNet* pNet = pGoalie->m_pTeam->m_pNet;
            v3MagicPos.x = pNet->m_v3NetLocation.x;
        }
        bool bSharpAngle = pGoalie->FindSTSMissData(v3MagicPos);
        unsigned short aAngle = pGoalie->m_aActualFacingDirection;
        if (goaliePos->x > 0.0f)
        {
            aAngle = (unsigned short)(aAngle + 0x8000);
        }
        short sAngle = (short)aAngle;
        sAngle = (sAngle < 0) ? -sAngle : sAngle;
        if ((unsigned short)sAngle > 0x271a)
        {
            unsigned short uAngle = aAngle;
            short clampedAngle = -0x271a;
            if (uAngle < 0x8000)
            {
                clampedAngle = 0x271a;
            }
            aAngle = (unsigned short)clampedAngle;
        }
        if (goaliePos->x > 0.0f)
        {
            aAngle = (unsigned short)(aAngle + 0x8000);
        }
        nlVector3 v3BlastPos;
        GetWorldPoint(v3BlastPos, pGoalie->mpSaveData->mv3SavePos, *goaliePos, aAngle);
        if (!bSharpAngle && (fTime2Goalie < 0.1f || CalculateDistanceSquared(v3InterceptPos, v3BlastPos) > 9.0f))
        {
            bool bDoSpin;
            if (pGoalie->mv3LocalContactPosition.y > 0.0f)
            {
                bDoSpin = true;
            }
            else
            {
                bDoSpin = false;
            }
            pGoalie->FindSTSSpinData(bDoSpin);
        }
        else
        {
            v3BallTarget = v3BlastPos;
            if (fTime2Goalie >= 0.1f)
            {
                fDesiredTime = fTime2Goalie;
            }
            else
            {
                fDesiredTime = 0.1f;
            }
        }
        g_pBall->ShootAtFast(v3BallVelocity, v3BallTarget, fDesiredTime);
        break;
    }
    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0x9594 | 0x800228D0 | size: 0x34
 */
void cFielder::SetSlideAttackSuccessFlag()
{
    mActionSlideAttackVars.bAttackSucceeded = true;
    BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, GetGlobalPad());
}

/**
 * Offset/Address/Size: 0x9564 | 0x800228A0 | size: 0x30
 */
void cFielder::SetKickOffWaitTime()
{
    mbCanKickoff = true;
    mtKickOffWaitTimer.SetSeconds(2.0f);
}

/**
 * Offset/Address/Size: 0x94B8 | 0x800227F4 | size: 0xAC
 */
void cFielder::SetBombImpactTime(const nlVector3& v3BombImpactLocation, float fBombImpactRadius)
{
    float fDeltaX = m_v3Position.x - v3BombImpactLocation.x;
    float fDeltaY = m_v3Position.y - v3BombImpactLocation.y;
    float fDeltaZ = m_v3Position.z - v3BombImpactLocation.z;

    float fDistanceSquared = fDeltaX * fDeltaX + fDeltaY * fDeltaY + fDeltaZ * fDeltaZ;
    float fTime = nlSqrt(fDistanceSquared, true) / 25.f;

    mtBombImpactTime.SetSeconds(fTime);
    mv3BombImpactLocation = v3BombImpactLocation;
    mfBombImpactRadius = fBombImpactRadius;
}

/**
 * Offset/Address/Size: 0x93F4 | 0x80022730 | size: 0xC4
 */
void cFielder::SetDesireDuration(float fNewDuration, bool bRandomVariation)
{
    if (fNewDuration > 0.0f && bRandomVariation)
    {
        static FilteredRandomReal randgen;

        float rand = randgen.genrand();
        float scaledRandom = 0.6f * rand;
        float variation = scaledRandom - 0.3f;
        fNewDuration += variation;
        if (0.0f >= fNewDuration)
        {
            fNewDuration = 0.0f;
        }
    }

    m_tDesireDuration.SetSeconds(fNewDuration);
}

/**
 * Offset/Address/Size: 0x921C | 0x80022558 | size: 0x1D8
 */
void cFielder::ShootBallDueToContact(const nlVector3& v3IncomingVelocity)
{
    bool bBallAwayFromCarrier;
    if ((m_eActionState == ACTION_RUNNING_WB_TURBO) && ((m_pCurrentAnimController->m_fTime > 0.2f) && (m_pCurrentAnimController->m_fTime < 0.975f)))
    {
        bBallAwayFromCarrier = true;
    }
    else
    {
        bBallAwayFromCarrier = false;
    }
    if (bBallAwayFromCarrier)
    {
        nlVector3 v3Direction;
        nlPolarToCartesian(v3Direction.x, v3Direction.y, m_aActualFacingDirection, m_fActualSpeed);
        v3Direction.z = 0.5f;
        g_pBall->ShootRelease(v3Direction, SPINTYPE_NONE);
        return;
    }
    if (m_eActionState == ACTION_SHOOT_TO_SCORE)
    {
        g_pBall->ShootRelease(v3Zero, SPINTYPE_NONE);
        return;
    }
    nlVector3 v3ReleaseVelocity;
    float fZ = v3IncomingVelocity.z + m_v3Velocity.z;
    float fY = v3IncomingVelocity.y + m_v3Velocity.y;
    float fX = v3IncomingVelocity.x + m_v3Velocity.x;
    nlVec3Set(v3ReleaseVelocity, fX, fY, fZ);
    if ((v3IncomingVelocity.x * v3IncomingVelocity.x + v3IncomingVelocity.y * v3IncomingVelocity.y + v3IncomingVelocity.z * v3IncomingVelocity.z < 0.001f * 0.001f) || (m_v3Velocity.x * m_v3Velocity.x + m_v3Velocity.y * m_v3Velocity.y + m_v3Velocity.z * m_v3Velocity.z < 0.001f * 0.001f) || (v3ReleaseVelocity.x * v3ReleaseVelocity.x + v3ReleaseVelocity.y * v3ReleaseVelocity.y + v3ReleaseVelocity.z * v3ReleaseVelocity.z < 0.001f * 0.001f))
    {
        nlVector3 v3FallbackDirection;
        nlPolarToCartesian(v3FallbackDirection.x, v3FallbackDirection.y, m_aActualFacingDirection, 2.0f);
        v3FallbackDirection.z = 0.5f;
        g_pBall->ShootRelease(v3FallbackDirection, SPINTYPE_NONE);
        return;
    }
    float fRecip = nlRecipSqrt(v3ReleaseVelocity.x * v3ReleaseVelocity.x + v3ReleaseVelocity.y * v3ReleaseVelocity.y + v3ReleaseVelocity.z * v3ReleaseVelocity.z, true);
    nlVec3Scale(v3ReleaseVelocity, v3ReleaseVelocity, fRecip);

    float fSpeed = 2.0f + m_fActualSpeed;
    nlVec3Set(v3ReleaseVelocity, fSpeed * v3ReleaseVelocity.x, fSpeed * v3ReleaseVelocity.y, fSpeed * v3ReleaseVelocity.z);
    v3ReleaseVelocity.z = 0.5f;
    g_pBall->ShootRelease(v3ReleaseVelocity, SPINTYPE_NONE);
}

/**
 * Offset/Address/Size: 0x918C | 0x800224C8 | size: 0x90
 */
void cFielder::ShootBallDueToContact(unsigned short aShootDirection)
{
    bool bBallAwayFromCarrier;

    if ((m_eActionState == ACTION_RUNNING_WB_TURBO) && ((m_pCurrentAnimController->m_fTime > 0.2f) && (m_pCurrentAnimController->m_fTime < 0.975f)))
    {
        bBallAwayFromCarrier = true;
    }
    else
    {
        bBallAwayFromCarrier = false;
    }

    if (!bBallAwayFromCarrier)
    {
        nlVector3 v3Direction;
        nlPolarToCartesian(v3Direction.x, v3Direction.y, aShootDirection, 2.0f + m_fActualSpeed);
        v3Direction.z = 0.5f;

        g_pBall->ShootRelease(v3Direction, SPINTYPE_NONE);
    }
}

/**
 * Offset/Address/Size: 0x8F5C | 0x80022298 | size: 0x230
 */
void cFielder::DoClearBall()
{
    nlPolar pClearingTopAngle;
    nlPolar pClearingBottomAngle;
    u16 aClearingAngle;

    float fRandomDistance = nlRandomf(3.0f, &nlDefaultSeed);
    float fGoalline = cField::GetGoalLineX(m_pTeam->GetOtherNet()->m_fDirection);

    float fTopY = cField::GetSidelineY(1) - fRandomDistance;
    nlCartesianToPolar(pClearingTopAngle, fGoalline - m_v3Position.x, fTopY - m_v3Position.y);

    float fBottomY = fRandomDistance + cField::GetSidelineY(0);
    nlCartesianToPolar(pClearingBottomAngle, fGoalline - m_v3Position.x, fBottomY - m_v3Position.y);

    ShotMeter* pShotMeter = m_pShotMeter;
    u32 aFacing = m_aActualFacingDirection;
    u32 aBottom = pClearingBottomAngle.a;
    u32 aTop = pClearingTopAngle.a;
    s16 nBottomDelta = GetAngleDifference(aFacing, aBottom);
    s16 nTopDelta = GetAngleDifference(aFacing, aTop);

    if (pShotMeter->mfSShotAimValue > 0.001f)
    {
        aClearingAngle = pClearingTopAngle.a;
    }
    else if (m_pShotMeter->mfSShotAimValue < -0.0001f)
    {
        aClearingAngle = pClearingBottomAngle.a;
    }
    else
    {
        u16 nAbsTopDelta = (u16)abs_s16(nTopDelta);
        u16 nAbsBottomDelta = (u16)abs_s16(nBottomDelta);

        if (nAbsBottomDelta < nAbsTopDelta)
        {
            aClearingAngle = pClearingBottomAngle.a;
        }
        else
        {
            aClearingAngle = pClearingTopAngle.a;
        }
    }

    float fClearSpeed = InterpolateRangeClamped(
        g_pGame->m_pGameTweaks->fClearBallGroundMinSpeed, g_pGame->m_pGameTweaks->fClearBallGroundMaxSpeed, 0.0f, 1.0f, pShotMeter->m_fSpeedValue);

    GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
    float fZSpeed = pTweaks->fClearBallMinZSpeed;
    bool bIsChipShot = mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot;
    if (bIsChipShot)
    {
        fZSpeed = pTweaks->fClearBallMaxZSpeed;
    }

    nlVector3 v3ClearBallVelocity;
    nlPolarToCartesian(v3ClearBallVelocity.x, v3ClearBallVelocity.y, aClearingAngle, fClearSpeed);
    v3ClearBallVelocity.z = fZSpeed;

    if (m_pBall != NULL)
    {
        ReleaseBall();
    }

    g_pBall->ShootRelease(v3ClearBallVelocity,
        nlRandom(2, &nlDefaultSeed) != 0 ? SPINTYPE_FORWARD : SPINTYPE_BACK);
    SetNoPickUpTime(0.2f);

    Audio::SoundAttributes sndAtr;
    sndAtr.Init();
    sndAtr.SetSoundType(0xB3, true);
    sndAtr.UseStationaryPosVector(m_v3Position);
    Audio::gStadGenSFX.Play(sndAtr);
    EmitBallImpact(this, true);
}

static inline bool IsShotMeterActive(eShotMeterState state)
{
    return (state == SHOT_METER_ACTIVE || state == SHOT_METER_STS_ACTIVE || state == SHOT_METER_STS_TRANSISTION);
}

/**
 * Offset/Address/Size: 0x8CA8 | 0x80021FE4 | size: 0x2B4
 */
void cFielder::DoHandleActiveShotMeter()
{
    if (GetGlobalPad() == NULL)
    {
        return;
    }

    eFielderActionState actionState = m_eActionState;

    switch (actionState)
    {
    case ACTION_ELECTROCUTION:
    case ACTION_HIT_REACT:
    case ACTION_PASS:
    case ACTION_SLIDE_ATTACK_REACT:
    case ACTION_BOMB_REACT:
    case ACTION_SHELL_REACT:
    case ACTION_BANANA_REACT:
    case ACTION_STS_HIT_REACT:
    case ACTION_SQUISH_REACT:
    case ACTION_SLIDE_FAIL_REACT:
        m_pShotMeter->Abort(this);
        break;
    case ACTION_SHOOT_TO_SCORE:
        break;
    default:
    {

        if (m_pBall == NULL)
        {
            m_pShotMeter->Abort(this);
            return;
        }

        bool bReturn;
        if (actionState == ACTION_RUNNING_WB_TURBO && m_pCurrentAnimController->m_fTime > 0.2f && m_pCurrentAnimController->m_fTime < 0.975f)
        {
            bReturn = true;
        }
        else
        {
            bReturn = false;
        }

        if (bReturn || actionState == ACTION_DEKE)
        {
            return;
        }

        ShotMeter* pShotMeter = m_pShotMeter;
        bool bIsActive = IsShotMeterActive(pShotMeter->m_eShotMeterState);
        if (bIsActive)
        {
            if (pShotMeter->m_eShotMeterState == SHOT_METER_STS_TRANSISTION)
            {
                KillWindup(this, "ball_shot_windup", false);
                EmitWindupAtCharacter(this, "ball_sts_windup");
            }
        }

        bool bIsChipShot = false;
        if (GetGlobalPad() != NULL)
        {
            GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
            if (GetGlobalPad()->GetPressure(PAD_AIM, true) > pTweaks->fLeftTriggerDownPressure)
            {
                bIsChipShot = true;
            }
        }

        pShotMeter = m_pShotMeter;
        bIsActive = IsShotMeterActive(pShotMeter->m_eShotMeterState);
        if (bIsActive)
        {
            bool bCanShootToScore = false;
            if (pShotMeter->m_eShotMeterState == SHOT_METER_STS_TRANSISTION)
            {
                cNet* pOtherNet = m_pTeam->GetOtherNet();
                if ((m_v3Position.x * pOtherNet->m_fDirection) <= 0.0f)
                {
                    bCanShootToScore = true;
                }
            }

            if (GetGlobalPad()->IsPressed(PAD_SHOOT, true))
            {
                if (!bCanShootToScore)
                {
                    return;
                }
            }

            mActionShotVars.bIsChipShot = bIsChipShot;
            m_pShotMeter->ShotReleased(this);
            InitActionShot(mActionShotVars.bIsChipShot);
        }
        else
        {
            if (m_eActionState != ACTION_SHOT && pShotMeter->m_eShotMeterState == SHOT_METER_RELEASED)
            {
                mActionShotVars.bIsChipShot = bIsChipShot;
                m_pShotMeter->ShotReleased(this);

                if (GetGlobalPad() != NULL)
                {
                    InitActionShot(mActionShotVars.bIsChipShot);
                }
                else
                {
                    InitActionShot(false);
                }
            }
            else if (m_eActionState != ACTION_SHOT && pShotMeter->m_eShotMeterState == SHOT_METER_STS_RELEASED)
            {
                InitActionShootToScore();
            }
        }
        break;
    }
    }
}

/**
 * Offset/Address/Size: 0x8A18 | 0x80021D54 | size: 0x290
 */
bool cFielder::DoLooseBallContactFromIdle(nlVector3& v3AnimStartPosition, float& fAnimStartTime, nlVector3& v3BallContactPosition, float& fBallContactTime,
    unsigned short aFutureFacingDirection, const LooseBallContactAnimInfo* pBestBallContactAnimInfo)
{
    const cSAnim* pGuessContactAnim = m_pAnimInventory->GetAnim(pBestBallContactAnimInfo->nAnimID);
    float fAnimTimeToContact = pBestBallContactAnimInfo->fAnimContactFrame / (float)pGuessContactAnim->m_nNumKeys;

    nlVector3 v3ContactOffsetLocal;
    GetJointPositionFuture(&v3ContactOffsetLocal, pBestBallContactAnimInfo->nAnimID, m_nBallJointIndex, fAnimTimeToContact, true, true, false);

    float fCos;
    float fSin;
    nlSinCos(&fSin, &fCos, aFutureFacingDirection);

    nlVector3 v3ContactOffsetWorld;
    nlVector3* pContactOffsetWorld = &v3ContactOffsetWorld;
    float ySin = v3ContactOffsetLocal.y * fSin;
    float xSin = v3ContactOffsetLocal.x * fSin;

    pContactOffsetWorld->x = (v3ContactOffsetLocal.x * fCos) - ySin;
    pContactOffsetWorld->y = (v3ContactOffsetLocal.y * fCos) + xSin;
    pContactOffsetWorld->z = v3ContactOffsetLocal.z;

    float fPrevBestDistToContactSquared;
    float fBallContactTargetX;
    float fBallContactTargetY;
    float fBallContactTargetZ;
    float fBestDistToContactSquared;
    float fSimulatedTime;

    fBallContactTargetZ = m_v3Position.z + pContactOffsetWorld->z;
    fBallContactTargetY = m_v3Position.y + pContactOffsetWorld->y;
    fBallContactTargetX = m_v3Position.x + pContactOffsetWorld->x;

    FakeBallWorld::ResetBallIterator();

    fBestDistToContactSquared = 0.0f;
    bool bDone = false;
    fSimulatedTime = fBestDistToContactSquared;
    fPrevBestDistToContactSquared = fBestDistToContactSquared;

    while (!bDone || fSimulatedTime > 5.0f)
    {
        nlVector3 v3SimulatedBallPos;
        FakeBallWorld::GetNextBallPosition(v3SimulatedBallPos);

        float distSq;
        float deltaX;
        float deltaY;
        float deltaZ;

        deltaY = fBallContactTargetY - v3SimulatedBallPos.y;
        deltaZ = fBallContactTargetZ - v3SimulatedBallPos.z;
        deltaX = fBallContactTargetX - v3SimulatedBallPos.x;
        distSq = deltaY * deltaY;
        distSq += deltaX * deltaX;
        distSq += deltaZ * deltaZ;

        if (fSimulatedTime > fPrevBestDistToContactSquared)
        {
            if (distSq > fBestDistToContactSquared)
            {
                bDone = true;
            }
        }

        if (!bDone)
        {
            v3BallContactPosition = v3SimulatedBallPos;
            fBestDistToContactSquared = distSq;
            fSimulatedTime += FixedUpdateTask::GetPhysicsUpdateTick();
            if (fSimulatedTime > 5.0f)
            {
                return false;
            }
        }
    }

    float startX;
    float startY;
    float startZ;

    startX = v3BallContactPosition.x - pContactOffsetWorld->x;
    startZ = v3BallContactPosition.z - pContactOffsetWorld->z;
    startY = v3BallContactPosition.y - pContactOffsetWorld->y;
    v3AnimStartPosition.x = startX;
    v3AnimStartPosition.y = startY;
    v3AnimStartPosition.z = startZ;

    unsigned int nNumKeys = pGuessContactAnim->m_nNumKeys;
    float fContactTimeNorm = pBestBallContactAnimInfo->fAnimContactFrame / (float)nNumKeys;
    float fAnimLength = (float)nNumKeys / 30.0f;
    fAnimStartTime = fSimulatedTime - (fContactTimeNorm * fAnimLength);
    fBallContactTime = fSimulatedTime;

    if (!m_bHasBeenUpdated)
    {
        fAnimStartTime += g_fSimulationTick;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x87A8 | 0x80021AE4 | size: 0x270
 */
bool cFielder::DoLooseBallContactFromRun(nlVector3& v3AnimStartPosition, float& fAnimStartTime, nlVector3& v3BallContactPosition, float& fBallContactTime,
    const LooseBallContactAnimInfo* pBestBallContactAnimInfo, const nlVector3& v3PassIntercept)
{
    float fMaxSimulatedTime;
    float passInterceptY;
    float passInterceptX;
    float distSq;
    float fSimulatedTime;
    nlVector3 bestIntercept;
    float bestDistToPassInterceptSquared;
    nlVector3 v3SimulatedBallPos;
    const cSAnim* guessContactAnim;
    nlVector3 v3ContactOffsetWorld;
    nlVector3 v3ContactOffsetLocal;
    float fAnimTimeToContact;
    float fCos;
    float fSin;

    FakeBallWorld::ResetBallIterator();

    fSimulatedTime = 0.0f;
    passInterceptX = v3PassIntercept.x;
    bestDistToPassInterceptSquared = 0.0f;
    passInterceptY = v3PassIntercept.y;
    fMaxSimulatedTime = 5.0f;
    float bestTime;

    while (fSimulatedTime < fMaxSimulatedTime)
    {
        FakeBallWorld::GetNextBallPosition(v3SimulatedBallPos);
        fSimulatedTime += FixedUpdateTask::GetPhysicsUpdateTick();

        float deltaX = v3SimulatedBallPos.x - passInterceptX;
        float deltaY = v3SimulatedBallPos.y - passInterceptY;
        distSq = deltaX * deltaX + deltaY * deltaY;

        if (distSq < bestDistToPassInterceptSquared)
        {
        }
        else if (fSimulatedTime != FixedUpdateTask::GetPhysicsUpdateTick())
        {
            break;
        }

        bestDistToPassInterceptSquared = distSq;
        bestTime = fSimulatedTime;
        bestIntercept = v3SimulatedBallPos;
    }

    if (fSimulatedTime >= fMaxSimulatedTime)
    {
        return false;
    }

    guessContactAnim = m_pAnimInventory->GetAnim(pBestBallContactAnimInfo->nAnimID);
    u32 nNumKeys = guessContactAnim->m_nNumKeys;
    fAnimTimeToContact = pBestBallContactAnimInfo->fAnimContactFrame / (float)nNumKeys;
    GetJointPositionFuture(&v3ContactOffsetLocal, pBestBallContactAnimInfo->nAnimID, m_nBallJointIndex, fAnimTimeToContact, true, true, false);

    nlSinCos(&fSin, &fCos, m_aActualFacingDirection);

    nlVector3* pContactOffsetWorld = &v3ContactOffsetWorld;
    float ySin = v3ContactOffsetLocal.y * fSin;
    float xSin = v3ContactOffsetLocal.x * fSin;
    pContactOffsetWorld->x = (v3ContactOffsetLocal.x * fCos) - ySin;
    pContactOffsetWorld->y = (v3ContactOffsetLocal.e[1] * fCos) + xSin;
    pContactOffsetWorld->z = v3ContactOffsetLocal.z;

    float fDesiredSpeedToAnimStart = (float)nNumKeys / 30.0f;
    float fBestSpeedToAnimStartDelta = fAnimTimeToContact * fDesiredSpeedToAnimStart;

    fMaxSimulatedTime = bestIntercept.z - pContactOffsetWorld->z;
    nlVec3Set(v3AnimStartPosition,
        bestIntercept.x - pContactOffsetWorld->x,
        bestIntercept.y - pContactOffsetWorld->y,
        fMaxSimulatedTime);
    v3AnimStartPosition.z = 0.0f;

    fAnimStartTime = bestTime - fBestSpeedToAnimStartDelta;

    v3BallContactPosition = bestIntercept;
    fBallContactTime = bestTime;

    if (!m_bHasBeenUpdated)
    {
        fAnimStartTime += g_fSimulationTick;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x84AC | 0x800217E8 | size: 0x2FC
 */
bool cFielder::DoLooseBallContactFromRunVolley(nlVector3& v3AnimStartPosition, float& fAnimStartTime, nlVector3& v3BallContactPosition, float& fBallContactTime,
    const LooseBallContactAnimInfo* pBestBallContactAnimInfo, const nlVector3& v3PassIntercept)
{
    const cSAnim* pGuessContactAnim;
    nlVector3 v3ContactOffsetLocal;

    pGuessContactAnim = m_pAnimInventory->GetAnim(pBestBallContactAnimInfo->nAnimID);
    GetJointPositionFuture(&v3ContactOffsetLocal, pBestBallContactAnimInfo->nAnimID, m_nBallJointIndex, pBestBallContactAnimInfo->fAnimContactFrame / (float)pGuessContactAnim->m_nNumKeys, true, true, false);

    float fCos;
    float fSin;
    nlSinCos(&fSin, &fCos, m_aActualFacingDirection);

    nlVector3 v3ContactOffsetWorld;
    nlVector3* pContactOffsetWorld = &v3ContactOffsetWorld;
    float ySin = v3ContactOffsetLocal.y * fSin;
    float xSin = v3ContactOffsetLocal.x * fSin;
    pContactOffsetWorld->x = (v3ContactOffsetLocal.x * fCos) - ySin;
    pContactOffsetWorld->y = (v3ContactOffsetLocal.e[1] * fCos) + xSin;
    pContactOffsetWorld->z = v3ContactOffsetLocal.z;

    float fContactZ;
    float currDistZ;
    float prevDistZ;
    float fPrevBallZ;
    float fMaxSimulatedTime;
    nlVector3 bestIntercept;
    nlVector3 v3SimulatedBallPos;

    FakeBallWorld::ResetBallIterator();

    float fSimulatedTime = 0.0f;
    fContactZ = pContactOffsetWorld->z;
    fPrevBallZ = fSimulatedTime;
    fMaxSimulatedTime = 5.0f;
    float bestTime;

    while (fSimulatedTime < fMaxSimulatedTime)
    {
        FakeBallWorld::GetNextBallPosition(v3SimulatedBallPos);
        fSimulatedTime += FixedUpdateTask::GetPhysicsUpdateTick();

        currDistZ = (float)fabs(v3SimulatedBallPos.z - fContactZ);
        prevDistZ = (float)fabs(fPrevBallZ - fContactZ);

        if (fSimulatedTime > FixedUpdateTask::GetPhysicsUpdateTick())
        {
            if (((v3SimulatedBallPos.z < v3ContactOffsetWorld.z) || (fPrevBallZ < v3ContactOffsetWorld.z)) && ((v3SimulatedBallPos.z >= v3ContactOffsetWorld.z) || (fPrevBallZ >= v3ContactOffsetWorld.z)) || (currDistZ >= prevDistZ))
            {
                float deltaY = v3SimulatedBallPos.y - v3PassIntercept.y;
                float deltaX = v3SimulatedBallPos.x - v3PassIntercept.x;
                float distSq = deltaX * deltaX + deltaY * deltaY;
                if (distSq < 1.0f)
                {
                    bestTime = fSimulatedTime;
                    bestIntercept = v3SimulatedBallPos;
                    break;
                }
            }
        }

        fPrevBallZ = v3SimulatedBallPos.z;
    }

    if (fSimulatedTime >= fMaxSimulatedTime)
    {
        return false;
    }

    u32 nNumKeys = pGuessContactAnim->m_nNumKeys;
    float fContactTimeNorm = pBestBallContactAnimInfo->fAnimContactFrame / (float)nNumKeys;
    float fAnimLength = (float)nNumKeys / 30.0f;

    nlVec3Sub(v3AnimStartPosition, bestIntercept, *pContactOffsetWorld);
    v3AnimStartPosition.z = 0.0f;

    float fBestSpeedToAnimStartDelta = fContactTimeNorm * fAnimLength;
    fAnimStartTime = bestTime - fBestSpeedToAnimStartDelta;

    v3BallContactPosition = bestIntercept;
    fBallContactTime = bestTime;

    if (!m_bHasBeenUpdated)
    {
        fAnimStartTime += g_fSimulationTick;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x82B0 | 0x800215EC | size: 0x1FC
 */
void cFielder::DoPenaltyCardBooking(cFielder* pFoulee, ePenaltyType eType)
{
    if (pFoulee->m_tBallUnPossessionTimer.GetSeconds() < 0.5f)
    {
        if (eType == PEN_TYPE_HIT_NO_BALL)
        {
            eType = PEN_TYPE_HIT_WITH_BALL;
        }
        else if (eType == PEN_TYPE_SLIDE_NO_BALL)
        {
            eType = PEN_TYPE_SLIDE_WITH_BALL;
        }
    }

    switch (m_ePenaltyCardStatus)
    {
    case PENALTY_CARD_NONE:
        m_ePenaltyCardStatus = PENALTY_CARD_YELLOW_1;
        break;
    case PENALTY_CARD_YELLOW_1:
        m_ePenaltyCardStatus = PENALTY_CARD_YELLOW_2;
        break;
    case PENALTY_CARD_RED:
        break;
    case PENALTY_CARD_YELLOW_2:
        m_ePenaltyCardStatus = PENALTY_CARD_RED;
        break;
    }

    PenaltyData* pEventData;
    if (eType == PEN_TYPE_HIT_NO_BALL)
    {
        pEventData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&g_pEventManager->CreateValidEvent(0x3D, 0x24)->m_data) PenaltyData();
    }
    else
    {
        pEventData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&g_pEventManager->CreateValidEvent(0x3C, 0x24)->m_data) PenaltyData();
    }

    pEventData->fPenaltyWorth = CalcPenaltyWorth(eType);
    pEventData->pFouler = this;
    pEventData->pFoulee = pFoulee;
}

/**
 * Offset/Address/Size: 0x8184 | 0x800214C0 | size: 0x12C
 */
void cFielder::DoPositioningInterceptBall()
{
    int nInterceptResult;
    float fInterceptTimes[2];
    nlVector3* pPos = &m_v3Position;

    if (nlGetLengthSquared2D(g_pBall->m_v3Position.x - pPos->x, g_pBall->m_v3Position.y - pPos->y) <= 4.0f)
    {
        CalcInterceptXY(*pPos, m_pTweaks->fRunningSpeed, 0.f, g_pBall->m_v3Position, g_pBall->m_v3Velocity, nInterceptResult, fInterceptTimes);

        if (nInterceptResult != 0)
        {
            float fTime;

            if (nInterceptResult == 2)
            {
                fTime = (fInterceptTimes[0] < fInterceptTimes[1]) ? fInterceptTimes[0] : fInterceptTimes[1];
            }
            else
            {
                fTime = fInterceptTimes[0];
            }

            float fInterceptX = g_pBall->m_v3Position.x + fTime * g_pBall->m_v3Velocity.x;
            float fInterceptY = g_pBall->m_v3Position.y + fTime * g_pBall->m_v3Velocity.y;

            float dyToIntercept = fInterceptY - m_v3Position.y;
            float dxToIntercept = fInterceptX - m_v3Position.x;
            float angleRad = nlATan2f(dyToIntercept, dxToIntercept);

            float angle16 = 10430.378f * angleRad;
            u16 targetAngle = (u16)(s32)angle16;

            s16 angleDelta = (s16)(targetAngle - m_aDesiredFacingDirection);
            angleDelta = (angleDelta < 0) ? -angleDelta : angleDelta;

            if ((u16)angleDelta <= 0x3000)
            {
                m_aDesiredFacingDirection = targetAngle;
                s16 desiredFacingDirection = m_aDesiredFacingDirection;
                m_aDesiredMovementDirection = desiredFacingDirection;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x8024 | 0x80021360 | size: 0x160
 */
void cFielder::DoAwardPowerupStuff(eAwardPowerupType eType, float fAmountOfAward)
{
    Event* pEvent = g_pEventManager->CreateValidEvent(0x3E, 0x20);
    PowerupData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) PowerupData();

    float fMinAmount = 0.0f;
    float fMaxAmount = 0.0f;

    switch (eType)
    {
    case AWARD_POWERUP_POWER_SHOT:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMaxAmount = InterpolateRangeClamped(pTweaks->fPowerupPowerShotMinAmount, pTweaks->fPowerupPowerShotMaxAmount, 0.0f, 0.9f, fAmountOfAward);
        fMinAmount = fMaxAmount;
        break;
    }
    case AWARD_POWERUP_INTERCEPT_PASS:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMinAmount = pTweaks->fPowerupInterceptPassMinAmount;
        fMaxAmount = pTweaks->fPowerupInterceptPassMaxAmount;
        break;
    }
    case AWARD_POWERUP_PERFECT_PASS:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMinAmount = pTweaks->fPowerupPerfectPassMinAmount;
        fMaxAmount = pTweaks->fPowerupPerfectPassMaxAmount;
        break;
    }
    case AWARD_POWERUP_CONTEXT_DEKE:
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        fMinAmount = pTweaks->fPowerupContextDekeMinAmount;
        fMaxAmount = pTweaks->fPowerupContextDekeMaxAmount;
        break;
    }
    }

    pData->fAwardWorth = InterpolateRangeClamped(fMinAmount, fMaxAmount, 0.0f, 1.0f, nlRandomf(1.0f, &nlDefaultSeed));
    pData->pFielder = this;
}

/**
 * Offset/Address/Size: 0x7E78 | 0x800211B4 | size: 0x1AC
 */
void cFielder::DoCalcShootToScoreResult(float fPerfectJumpTime, float fPerfectReleaseTime, float fActualJumpTime, float fActualReleaseTime, float fGreenWidth)
{
    eShootToScoreResult result = S2S_SAVED;
    float fAbsDiff = (float)fabs(fPerfectReleaseTime - fActualReleaseTime);

    if (fPerfectJumpTime == fActualJumpTime && fAbsDiff < fGreenWidth)
    {
        result = S2S_SUPER_SHOT;

        Event* pEvent = g_pEventManager->CreateValidEvent(0x43, 0x1C);
        ShotAtGoalData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) ShotAtGoalData();
        pData->pShooter = this;

        if (g_e3_Build)
        {
            result = S2S_SCORE;
        }
    }
    else
    {
        g_pEventManager->CreateValidEvent(0x44, 0x14);

        GameInfoManager* pGameInfoManager = nlSingleton<GameInfoManager>::Instance();
        if (pGameInfoManager->IsPerfectStrikesOn())
        {
            result = S2S_SUPER_SHOT;

            Event* pEvent = g_pEventManager->CreateValidEvent(0x43, 0x1C);
            ShotAtGoalData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) ShotAtGoalData();
            pData->pShooter = this;
        }
        else if (fAbsDiff < mActionShootToScoreVars.fCaptainYellowWidth)
        {
            if (fAbsDiff < fGreenWidth && fGreenWidth > 0.0f)
            {
                result = S2S_SCORE;
            }
            else
            {
                float fValue = (float)fabs(fAbsDiff - fGreenWidth) / (mActionShootToScoreVars.fCaptainYellowWidth - fGreenWidth);

                if (fPerfectJumpTime == fActualJumpTime)
                {
                    m_pShotMeter->m_fSTSValue = 0.01f;
                }
                else
                {
                    m_pShotMeter->m_fSTSValue = fValue;
                }

                result = S2S_SAVED_YELLOW;
            }
        }
    }

    meS2SResult = result;
}

/**
 * Offset/Address/Size: 0x7C34 | 0x80020F70 | size: 0x244
 */
cFielder* cFielder::DoFindBestHitTarget()
{
    if (GetGlobalPad() == NULL)
    {
        FuzzyVariant vBestTarget = Fuzzy::GetBestHitTarget(this);
        return (cFielder*)vBestTarget.mData.pPlayer;
    }

    float fBestScore = 99999.9f;
    cFielder* pBestCandidate = NULL;
    cTeam* pTeam = m_pTeam->GetOtherTeam();
    u16 aDirection = m_aActualFacingDirection;

    if (m_pController->GetMovementStickMagnitude())
    {
        aDirection = m_pController->GetMovementStickDirection();
    }

    for (int i = 0; i < 4; i++)
    {
        float fTempScore = 99999.9f;
        cFielder* pCandidate = pTeam->GetFielder(i);

        if (!pCandidate->IsFallenDown(0.0f) && pCandidate->m_tFrozenTimer.m_uPackedTime == 0)
        {
            bool bInvalidTarget = ((pCandidate->m_ePowerup == POWER_UP_STAR)
                                      && (pCandidate->m_tPowerupEffectTime.m_uPackedTime != 0))
                               || pCandidate->mActionShootToScoreVars.isCurrentlyInvincible;

            if (!bInvalidTarget)
            {
                fTempScore = DoFlashLight(
                    pCandidate->m_v3Position,
                    aDirection,
                    g_pGame->m_pGameTweaks->fAngleWeighting,
                    0.0f,
                    9999.0f);
            }
        }

        if ((g_pBall->GetOwnerFielder() == pCandidate) || (g_pBall->GetPassTargetFielder() == pCandidate))
        {
            fTempScore *= g_pGame->m_pGameTweaks->fHitCarrierWeighting;
        }

        if (fTempScore < fBestScore)
        {
            pBestCandidate = pCandidate;
            fBestScore = fTempScore;
        }
    }

    if (pBestCandidate == NULL)
    {
        pBestCandidate = GetClosestOpponentFielder(NULL);
    }

    return pBestCandidate;
}

/**
 * Offset/Address/Size: 0x73B8 | 0x800206F4 | size: 0x87C
 */
void cFielder::DoFindBestShotTarget(nlVector3& v3PositionOut, float& fShotSpeed, bool bIsSTS)
{
    cBall* pBall = g_pBall;
    Goalie* pGoalie = m_pTeam->GetOtherTeam()->GetGoalie();

    float fShotDist;
    float kBallAllowance = 0.18f + cNet::m_fNetPostRadius;
    bool bIsChipShot = false;
    if (mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot)
    {
        bIsChipShot = true;
    }
    if (bIsChipShot)
    {
        kBallAllowance += g_pGame->m_pGameTweaks->fChipShotPostOffset;
    }
    else
    {
        kBallAllowance += g_pGame->m_pGameTweaks->fShotPostOffset;
    }

    float fDist2NetSide = 0.5f * cNet::m_fNetWidth - kBallAllowance;
    cNet* pNet = m_pTeam->GetOtherNet();
    float fNetBaseX = pNet->m_v3NetLocation.x;
    float fBallY = pBall->m_v3Position.y;

    float fBallYClamped = nlMinEquals(nlMaxEquals(fBallY, -fDist2NetSide), fDist2NetSide);

    float fBallZClamped = bIsSTS
                            ? nlMinEquals(nlMaxEquals(pBall->m_v3Position.z, 0.18f), kBallAllowance)
                            : nlMinEquals(nlMaxEquals(pBall->m_v3Position.z, 0.18f), cNet::m_fNetHeight - kBallAllowance);

    fBallY -= fBallYClamped;
    float fDX = pBall->m_v3Position.x - fNetBaseX;
    fShotDist = nlSqrt((fDX * fDX) + (fBallY * fBallY), true);

    bool bIsChipShot2 = false;
    if (mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot)
    {
        bIsChipShot2 = true;
    }

    float speedFactor;
    float fShotMinSpeed;
    float fShotMaxSpeed;
    if (bIsChipShot2)
    {
        speedFactor = fShotDist;
        fShotMinSpeed = ((FielderTweaks*)m_pTweaks)->fShotChipMinSpeed;
        fShotMaxSpeed = ((FielderTweaks*)m_pTweaks)->fShotChipMaxSpeed;
    }
    else
    {
        speedFactor = fShotDist * (0.3f + m_pShotMeter->m_fSpeedValue);
        fShotMinSpeed = ((FielderTweaks*)m_pTweaks)->fShotMinSpeed;
        fShotMaxSpeed = ((FielderTweaks*)m_pTweaks)->fShotMaxSpeed;
    }

    fShotSpeed = InterpolateRangeClamped(fShotMinSpeed, fShotMaxSpeed, 3.0f, 18.0f, speedFactor);

    float fAngPost1;
    float fAbsBallX = fabsf(pBall->m_v3Position.x);
    float fAimValue = m_pShotMeter->GetShotAimValue();
    float fAbsAimValue = fabsf(fAimValue);
    float fAbsBallY = fabsf(pBall->m_v3Position.y);

    if (fAbsBallY < 1.5f + fDist2NetSide
        && (fAbsBallX > fabsf(pGoalie->m_v3Position.x)
            || fAbsBallX > cField::GetGoalLineX(1u) - 1.5f))
    {
        v3PositionOut.x = 1.005f * pNet->m_v3NetLocation.x;
        v3PositionOut.y = 0.9f * fBallYClamped;
        v3PositionOut.z = fBallZClamped + nlRandomf(0.2f, &nlDefaultSeed);
        if (fShotDist < 2.0f)
        {
            fShotSpeed = 10.0f;
        }
    }
    else
    {
        float fNetBaseY = pNet->m_v3NetLocation.y;
        nlVector3 v3Post1 = pNet->m_v3NetLocation;
        nlVector3 v3Post2 = pNet->m_v3NetLocation;
        v3Post1.y = fNetBaseY - fDist2NetSide;
        v3Post2.y = fNetBaseY + fDist2NetSide;

        nlVector3 v3Post1Delta;
        nlVector3 v3Post2Delta;
        nlVec3Sub(v3Post1Delta, v3Post1, pBall->m_v3Position);
        nlVec3Sub(v3Post2Delta, v3Post2, pBall->m_v3Position);
        nlVector3 v3GoalieDelta;
        nlVec3Sub(v3GoalieDelta, pGoalie->m_v3Position, pBall->m_v3Position);
        float fPost1DY = v3Post1Delta.y;
        float fPost2DY = v3Post2Delta.y;
        float fPost1DX = v3Post1Delta.x;
        float fPost2DX = v3Post2Delta.x;
        float fPost1DZ = v3Post1Delta.z;
        float fPost2DZ = v3Post2Delta.z;
        float fGoalieDY = v3GoalieDelta.y;
        float fGoalieDX = v3GoalieDelta.x;

        fAngPost1 = nlATan2f(fPost1DY, fPost1DX);
        u16 aAngPost2 = nlVector3ToAngle(v3Post2Delta);
        float fAngGoalie = nlATan2f(fGoalieDY, fGoalieDX);

        u16 uAbsP1G = (u16)abs_s16(
            GetAngleDifference((u16)(s32)(10430.378f * fAngPost1), (u16)(s32)(10430.378f * fAngGoalie)));
        u16 uAbsP2G = (u16)abs_s16(
            GetAngleDifference(aAngPost2, (u16)(s32)(10430.378f * fAngGoalie)));
        u16 uAbsP1P2 = (u16)abs_s16(
            GetAngleDifference((u16)(s32)(10430.378f * fAngPost1), aAngPost2));

        v3PositionOut.x = 1.005f * pNet->m_v3NetLocation.x;

        float fProbability;
        if (fAbsAimValue > 0.01f)
        {
            fProbability = 0.5f - 0.5f * fAimValue;
        }
        else if (uAbsP1G >= uAbsP1P2)
        {
            fProbability = 1.0f;
        }
        else if (uAbsP2G >= uAbsP1P2)
        {
            fProbability = 0.0f;
        }
        else
        {
            float fAngToNet = nlATan2f(pNet->m_v3NetLocation.y - pBall->m_v3Position.y,
                pNet->m_v3NetLocation.x - pBall->m_v3Position.x);
            u16 angle2Net = (u16)(s32)(10430.378f * fAngToNet);
            if (pNet->m_v3NetLocation.x < 0.0f)
            {
                angle2Net += 0x8000;
            }
            s16 sAng2Net = (s16)angle2Net;
            s32 nAbsAng2Net = sAng2Net;
            if (sAng2Net < 0)
            {
                nAbsAng2Net = -sAng2Net;
            }

            if ((u16)nAbsAng2Net > 0x2000)
            {
                nlVector3 v3GD1;
                nlVector3 v3GD2;
                nlVec3Sub(v3GD1, pGoalie->m_v3Position, v3Post1);
                nlVec3Sub(v3GD2, pGoalie->m_v3Position, v3Post2);
                float fGD1Sq = v3GD1.GetLengthSq2D();
                float fGD2Sq = v3GD2.GetLengthSq2D();
                fProbability = nlMinEquals(
                    nlMaxEquals(fGD1Sq / (fGD1Sq + fGD2Sq), 0.03f), 0.97f);
            }
            else if (3 * uAbsP1G < uAbsP2G || 3 * uAbsP2G < uAbsP1G)
            {
                if (uAbsP1G < uAbsP2G)
                {
                    fProbability = nlMaxEquals(0.05f, 0.5f * (int)(3 * uAbsP1G - uAbsP2G) / (int)(uAbsP1G + uAbsP2G));
                }
                else
                {
                    fProbability = nlMinEquals(0.95f, 1.0f - 0.5f * (int)(3 * uAbsP2G - uAbsP1G) / (int)(uAbsP1G + uAbsP2G));
                }
            }
            else
            {
                float fAngleLimit = 8192.0f;
                fProbability = InterpolateRangeClamped(0.15f, 0.85f, -fAngleLimit, fAngleLimit, -(float)(s32)sAng2Net);
            }
        }

        static FilteredRandomChance randgenPost;

        if (randgenPost.genrand(fProbability))
        {
            v3PositionOut.y = pBall->m_v3Position.y + fPost1DY;
            float fDistPost1Sq = (fPost1DX * fPost1DX) + (fPost1DY * fPost1DY) + (fPost1DZ * fPost1DZ);
            float fDistPost2Sq = (fPost2DX * fPost2DX) + (fPost2DY * fPost2DY) + (fPost2DZ * fPost2DZ);
            if (fDistPost1Sq < fDistPost2Sq)
            {
                v3PositionOut.x = 0.985f * pNet->m_v3NetLocation.x;
            }
        }
        else
        {
            v3PositionOut.y = pBall->m_v3Position.y + fPost2DY;
            float fDistPost2Sq = (fPost2DX * fPost2DX) + (fPost2DY * fPost2DY) + (fPost2DZ * fPost2DZ);
            float fDistPost1Sq = (fPost1DX * fPost1DX) + (fPost1DY * fPost1DY) + (fPost1DZ * fPost1DZ);
            if (fDistPost2Sq < fDistPost1Sq)
            {
                v3PositionOut.x = 0.985f * pNet->m_v3NetLocation.x;
            }
        }

        if (bIsSTS)
        {
            v3PositionOut.z = kBallAllowance + nlRandomf(0.5f, &nlDefaultSeed);
        }
        else if (fShotDist > g_pGame->m_pGameTweaks->fShotHighDistance)
        {
            v3PositionOut.z = cNet::m_fNetHeight - kBallAllowance;
        }
        else
        {
            bool bIsChip = false;
            if (mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot)
            {
                bIsChip = true;
            }
            if (bIsChip)
            {
                float fAllowableHeight = cNet::m_fNetHeight - kBallAllowance;
                v3PositionOut.z = 0.9f * fAllowableHeight + nlRandomf(0.100000024f * fAllowableHeight, &nlDefaultSeed);
            }
            else
            {
                v3PositionOut.z = kBallAllowance + nlRandomf(cNet::m_fNetHeight - 2.0f * kBallAllowance, &nlDefaultSeed);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x6E28 | 0x80020164 | size: 0x590
 */
void cFielder::DoRegularShooting()
{
    nlVector3 v3BallVelocity;
    nlVector3 v3Target;

    cBall* pBall = g_pBall;
    pBall->m_unk_0xA6 = 0;
    pBall->mpDamageTarget = NULL;

    bool bIsSTS = (m_eActionState == ACTION_SHOOT_TO_SCORE);

    if (bIsSTS)
    {
        CalcShootToScoreShot(v3BallVelocity, v3Target);
        g_pBall->m_unk_0xA5 = (meS2SResult == S2S_SUPER_SHOT);
        if (IsCaptain() || ((GameInfoManager*)nlSingleton<GameInfoManager>::s_pInstance)->GetTeam((short)m_pTeam->m_nSide) == 8)
            g_pBall->m_uGoalType = 6;
        else
            g_pBall->m_uGoalType = 2;
    }
    else
    {
        CalcRegularShot(v3BallVelocity, v3Target);
        g_pBall->m_unk_0xA5 = 0;

        if (m_eActionState == ACTION_ONETIMER || m_eActionState == ACTION_LATE_ONETIMER_FROM_VOLLEY || (m_eActionState == ACTION_SHOT && m_tBallPossessionTimer.GetSeconds() < 0.1f))
        {
            g_pBall->m_uGoalType = 1;
            g_pGame->SetPotentialScorer(this);
            DoAwardPowerupStuff(AWARD_POWERUP_POWER_SHOT, g_pGame->m_pGameTweaks->fPowerupPowerShotMinAmount);
        }
        else
        {
            g_pBall->m_uGoalType = 0;

            if (m_eActionState != ACTION_LOOSE_BALL_SHOT)
            {
                DoAwardPowerupStuff(AWARD_POWERUP_POWER_SHOT, m_pShotMeter->m_fSpeedValue);
            }
            else
            {
                DoAwardPowerupStuff(AWARD_POWERUP_POWER_SHOT, g_pGame->m_pGameTweaks->fPowerupPowerShotMinAmount);
            }
        }
    }

    if (m_pBall != NULL)
        ReleaseBall();

    g_pBall->m_v3ShotTarget = v3Target;

    bool bIsChipShot = mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot;
    eSpinType spinType;
    nlVector3 v3AngVel;

    if (bIsChipShot)
    {
        spinType = SPINTYPE_BACK;
        v3AngVel = v3Zero;
    }
    else
    {
        spinType = SPINTYPE_PARAMETER;

        v3AngVel.x = 7.5f - nlRandomf(15.0f, &nlDefaultSeed);
        v3AngVel.y = 15.0f - nlRandomf(30.0f, &nlDefaultSeed);
        v3AngVel.z = 10.0f + nlRandomf(15.0f, &nlDefaultSeed);

        nlVector3 v3Delta;
        v3Delta.Sub2D(v3Target, g_pBall->m_v3Position);
        bool bNegZSpin = false;

        if (fabsf(v3Delta.x) < fabsf(v3Delta.y))
        {
            if (v3Delta.x * v3Delta.y > 0.0f)
                bNegZSpin = true;
        }
        else
        {
            if (v3Target.x * v3Target.y > 0.0f)
                bNegZSpin = true;
        }

        if (bNegZSpin)
            v3AngVel.z *= -1.0f;

        RotateVectorZAxis(v3AngVel, v3AngVel, m_aActualFacingDirection);

        if (m_eActionState == ACTION_ONETIMER)
        {
            nlVec3Scale(v3AngVel, 0.4f);
        }

        if (m_pShotMeter->m_fSpeedValue >= 0.99f && !bIsSTS)
        {
            g_pBall->m_pPhysicsBall->m_bUseMagnusEffect = true;
            g_pBall->m_unk_0xA6 = true;
        }
    }

    g_pBall->Shoot(v3BallVelocity, v3AngVel, spinType, bIsSTS, m_pShotMeter->m_fSpeedValue >= 0.99f, mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot);

    g_pBall->m_pShooter = this;
    SetNoPickUpTime(0.2f);

    if (g_pGame->IsGameplayOrOvertime())
    {
        Event* pEvent = g_pEventManager->CreateValidEvent(0x14, 0x1C);
        ShotAtGoalData* pShotData = (ShotAtGoalData*)&pEvent->m_data;
        new (pShotData) ShotAtGoalData();
        pShotData->pShooter = this;
    }
}

void cFielder::DoDebugShooting()
{
    nlVector3 v3Target = v3Zero;
    nlVector3 v3BallVelocity;

    if (sDebugShootingMode == DEBUG_SHOOT_AT_NET)
    {
        cNet* pNet = m_pTeam->GetOtherNet();
        v3Target.x = pNet->m_v3NetLocation.x + sfDebugShotXOffset;
        v3Target.y = sfDebugShotYOffset;
        v3Target.z = sfDebugShotHeight * cNet::m_fNetHeight;
        nlVec3Sub(v3BallVelocity, v3Target, g_pBall->m_v3Position);
        nlVec3Scale(v3BallVelocity, nlRecipSqrt(v3BallVelocity.GetLengthSq3D(), true));
        nlVec3Scale(v3BallVelocity, sfDebugShotVelocity);
    }
    else if (sDebugShootingMode == DEBUG_SHOOT_IN_ANALOG_STICK_DIRECTION)
    {
        float x = cPadManager::GetPad(0)->AnalogLeftX();
        float y = cPadManager::GetPad(0)->AnalogLeftY();
        if (x == 0.0f && y == 0.0f)
        {
            nlVec3Set(v3BallVelocity, 0.0f, 0.0f, sfDebugShotVelocity);
            v3Target = m_v3Position;
        }
        else
        {
            nlVec3Set(v3BallVelocity, x, y, 0.0f);
            nlVec3Scale(v3BallVelocity, nlRecipSqrt(v3BallVelocity.GetLengthSq3D(), true));
            nlVec3Scale(v3BallVelocity, sfDebugShotVelocity);
        }
        nlVec3Add(v3Target, v3Target, v3BallVelocity);
    }

    g_pBall->mpDamageTarget = NULL;
    if (m_pBall != NULL)
    {
        ReleaseBall();
    }

    g_pBall->m_v3ShotTarget = v3Target;
    g_pBall->Shoot(v3BallVelocity, v3Zero, SPINTYPE_BACK, m_eActionState == ACTION_SHOOT_TO_SCORE, m_pShotMeter->m_fSpeedValue >= 0.99f, false);
    g_pBall->m_pShooter = this;
    SetNoPickUpTime(0.2f);

    Event* pEvent = g_pEventManager->CreateValidEvent(0x14, 0x1C);
    ShotAtGoalData* pShotData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) ShotAtGoalData();
    pShotData->pShooter = this;
}

/**
 * Offset/Address/Size: 0x6DE4 | 0x80020120 | size: 0x44
 */
void cFielder::DoResetShotMeter(float fTime)
{
    m_pShotMeter->Reset();
    m_pShotMeter->m_fTime = fTime;
}

/**
 * Offset/Address/Size: 0x6DD0 | 0x8002010C | size: 0x14
 */
bool cFielder::IsActionDone() const
{
    return (u8)(m_eActionState == ACTION_NEED_ACTION);
}

/**
 * Offset/Address/Size: 0x6D94 | 0x800200D0 | size: 0x3C
 */
void cFielder::SetAction(eFielderActionState actionState)
{
    CleanUpAction();
    m_eActionState = actionState;
}

/**
 * Offset/Address/Size: 0x6D58 | 0x80020094 | size: 0x3C
 */
bool cFielder::GetFormationPosition(nlVector3& v3DestPosition, float fBallPosFormationWeight)
{
    if (fBallPosFormationWeight < 0.0f)
    {
        fBallPosFormationWeight = 1.0f;
    }

    return m_pTeam->CalculateFormationPosition(v3DestPosition, this, m_DesireCommonVars.bInPosition, fBallPosFormationWeight);
}

const LooseBallContactAnimInfo* GetOneTimerIdleGroundContactAnims()
{
    return gOneTimerIdleGroundContactAnims;
}

int GetNumOneTimerIdleGroundContactAnims()
{
    return sizeof(gOneTimerIdleGroundContactAnims) / sizeof(gOneTimerIdleGroundContactAnims[0]);
}

const LooseBallContactAnimInfo* GetOneTimerIdleVolleyContactAnims()
{
    return gOneTimerIdleVolleyContactAnims;
}

int GetNumOneTimerIdleVolleyContactAnims()
{
    return sizeof(gOneTimerIdleVolleyContactAnims) / sizeof(gOneTimerIdleVolleyContactAnims[0]);
}

/**
 * Offset/Address/Size: 0x6D4C | 0x80020088 | size: 0xC
 */
const LooseBallContactAnimInfo* GetOneTimerLeadGroundContactAnims()
{
    return gOneTimerLeadGroundContactAnims;
}

int GetNumOneTimerLeadGroundContactAnims()
{
    return sizeof(gOneTimerLeadGroundContactAnims) / sizeof(gOneTimerLeadGroundContactAnims[0]);
}

const LooseBallContactAnimInfo* GetOneTimerLeadVolleyContactAnims()
{
    return gOneTimerLeadVolleyContactAnims;
}

int GetNumOneTimerLeadVolleyContactAnims()
{
    return sizeof(gOneTimerLeadVolleyContactAnims) / sizeof(gOneTimerLeadVolleyContactAnims[0]);
}

/**
 * Offset/Address/Size: 0x6C1C | 0x8001FF58 | size: 0x130
 */
LooseBallContactAnimInfo* cFielder::GetOneTimerBallContactAnimInfo(unsigned short aFutureFacingDirection, const nlVector3& v3FuturePosition, const nlVector3& v3OneTimerTarget, bool bLeadPass, bool bVolleyPass)
{
    const LooseBallContactAnimInfo* pBallContactAnimInfo;
    int nNumContactAnims;

    if (bLeadPass)
    {
        if (bVolleyPass)
        {
            nNumContactAnims = 4;
            pBallContactAnimInfo = gOneTimerLeadVolleyContactAnims;
        }
        else
        {
            nNumContactAnims = 4;
            pBallContactAnimInfo = gOneTimerLeadGroundContactAnims;
        }
    }
    else
    {
        if (bVolleyPass)
        {
            nNumContactAnims = 4;
            pBallContactAnimInfo = gOneTimerIdleVolleyContactAnims;
        }
        else
        {
            nNumContactAnims = 4;
            pBallContactAnimInfo = gOneTimerIdleGroundContactAnims;
        }
    }

    u16 aNetAngle = (u16)(s32)(10430.378f * nlATan2f(v3OneTimerTarget.y - v3FuturePosition.y, v3OneTimerTarget.x - v3FuturePosition.x)) - aFutureFacingDirection;

    LooseBallContactAnimInfo* pBestBallContactAnimInfo = NULL;

    for (int i = 0; i < nNumContactAnims; i++)
    {
        if (pBallContactAnimInfo[i].aIncomingAngleMin < pBallContactAnimInfo[i].aIncomingAngleMax)
        {
            if ((aNetAngle >= pBallContactAnimInfo[i].aIncomingAngleMin) && (aNetAngle <= pBallContactAnimInfo[i].aIncomingAngleMax))
            {
                pBestBallContactAnimInfo = (LooseBallContactAnimInfo*)&pBallContactAnimInfo[i];
            }
        }
        else if ((aNetAngle >= pBallContactAnimInfo[i].aIncomingAngleMin) || (aNetAngle <= pBallContactAnimInfo[i].aIncomingAngleMax))
        {
            pBestBallContactAnimInfo = (LooseBallContactAnimInfo*)&pBallContactAnimInfo[i];
        }
    }
    return pBestBallContactAnimInfo;
}

/**
 * Offset/Address/Size: 0x6AEC | 0x8001FE28 | size: 0x130
 */
const LooseBallContactAnimInfo* cFielder::GetReceivePassBallContactAnimInfo(cBall* pBall, const nlVector3& rv3Pos, unsigned short aAngle, bool bLeadPass, bool bVolleyPass)
{
    static LooseBallContactAnimInfo IdleGroundContactAnims[1] = {
        { 0x37, 3.0f, 0x0000, 0xFFFF }
    };
    static LooseBallContactAnimInfo IdleVolleyContactAnims[1] = {
        { 0x38, 9.0f, 0x0000, 0xFFFF }
    };
    static LooseBallContactAnimInfo LeadGroundContactAnims[4] = {
        { 0x39, 7.0f, 0xE000, 0x2000 },
        { 0x3A, 7.0f, 0xA000, 0xE000 },
        { 0x3C, 7.0f, 0x6000, 0xA000 },
        { 0x3B, 7.0f, 0x2000, 0x6000 }
    };
    static LooseBallContactAnimInfo LeadVolleyContactAnims[3] = {
        { 0x3D, 10.0f, 0xE000, 0x2000 },
        { 0x3F, 9.0f, 0x2000, 0x8000 },
        { 0x3E, 9.0f, 0x8000, 0xE000 }
    };

    const LooseBallContactAnimInfo* pBallContactAnimInfo;
    int nNumContactAnims;

    if (bLeadPass)
    {
        if (bVolleyPass)
        {
            nNumContactAnims = 3;
            pBallContactAnimInfo = LeadVolleyContactAnims;
        }
        else
        {
            nNumContactAnims = 4;
            pBallContactAnimInfo = LeadGroundContactAnims;
        }
    }
    else
    {
        if (bVolleyPass)
        {
            nNumContactAnims = 1;
            pBallContactAnimInfo = IdleVolleyContactAnims;
        }
        else
        {
            nNumContactAnims = 1;
            pBallContactAnimInfo = IdleGroundContactAnims;
        }
    }

    u16 aNetAngle = (u16)(s32)(10430.378f * nlATan2f(pBall->m_v3Position.y - rv3Pos.y, pBall->m_v3Position.x - rv3Pos.x)) - aAngle;
    const LooseBallContactAnimInfo* pBestBallContactAnimInfo = NULL;
    for (int i = 0; i < nNumContactAnims; i++)
    {
        if (pBallContactAnimInfo[i].aIncomingAngleMin < pBallContactAnimInfo[i].aIncomingAngleMax)
        {
            if ((aNetAngle >= pBallContactAnimInfo[i].aIncomingAngleMin) && (aNetAngle <= pBallContactAnimInfo[i].aIncomingAngleMax))
            {
                pBestBallContactAnimInfo = &pBallContactAnimInfo[i];
            }
        }
        else if ((aNetAngle >= pBallContactAnimInfo[i].aIncomingAngleMin) || (aNetAngle <= pBallContactAnimInfo[i].aIncomingAngleMax))
        {
            pBestBallContactAnimInfo = &pBallContactAnimInfo[i];
        }
    }
    return pBestBallContactAnimInfo;
}

/**
 * Offset/Address/Size: 0x6A00 | 0x8001FD3C | size: 0xEC
 */
void cFielder::GetReceivePassBallContactOffset(nlVector3& v3Offset, unsigned short aFacingDirection, const LooseBallContactAnimInfo* pBestBallContactAnimInfo)
{
    nlVector3 v3ContactOffsetLocal;
    const cSAnim* guessContactAnim = m_pAnimInventory->GetAnim(pBestBallContactAnimInfo->nAnimID);

    GetJointPositionFuture(&v3ContactOffsetLocal, pBestBallContactAnimInfo->nAnimID, m_nBallJointIndex, pBestBallContactAnimInfo->fAnimContactFrame / (float)guessContactAnim->m_nNumKeys, false, true, false);

    float cos, sin;
    nlSinCos(&sin, &cos, aFacingDirection);

    v3Offset.x = v3ContactOffsetLocal.x * cos - v3ContactOffsetLocal.y * sin;
    v3Offset.y = v3ContactOffsetLocal.y * cos + v3ContactOffsetLocal.x * sin;
    v3Offset.z = v3ContactOffsetLocal.z;
}

/**
 * Offset/Address/Size: 0x68F0 | 0x8001FC2C | size: 0x110
 */
bool cFielder::IsFallenDown(float fThreshold) const
{
    s32 nAnimID = m_eAnimID;
    u32 nIndex = (u32)(nAnimID - 0x5c);
    float fThresholdValue = -1.0f;

    switch (nIndex)
    {
    case 16:
        fThresholdValue = 67.0f;
        break;
    case 17:
        fThresholdValue = 64.0f;
        break;
    case 19:
        fThresholdValue = 34.0f;
        break;
    case 20:
    case 22:
        fThresholdValue = 37.0f;
        break;
    case 21:
        fThresholdValue = 32.0f;
        break;
    case 10:
        fThresholdValue = 42.0f;
        break;
    case 12:
        fThresholdValue = 46.0f;
        break;
    case 11:
    case 13:
        fThresholdValue = 43.0f;
        break;
    case 9:
        fThresholdValue = 20.0f;
        if (fThresholdValue < fThreshold)
        {
            return false;
        }
        break;
    case 14:
        fThresholdValue = 46.0f;
        break;
    case 15:
        fThresholdValue = 42.0f;
        break;
    case 23:
        fThresholdValue = 61.0f;
        break;
    case 0:
        fThresholdValue = 108.0f;
        break;
    case 24:
    case 25:
    {
        fThresholdValue = (float)(m_pCurrentAnimController->m_pSAnim->m_nNumKeys);
        break;
    }
    case 26:
        fThresholdValue = 29.0f;
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 18:
        break;
    }

    return m_pCurrentAnimController->m_fTime < (fThresholdValue / m_pCurrentAnimController->m_pSAnim->m_nNumKeys);
}

/**
 * Offset/Address/Size: 0x6868 | 0x8001FBA4 | size: 0x88
 */
bool cFielder::IsHitting() const
{
    const cPN_SAnimController* pAnimController = m_pCurrentAnimController;
    const float fAnimTime = 30.0f * pAnimController->m_fTime * (pAnimController->m_pSAnim->m_nNumKeys / 30.0f);
    bool isHitting = ((m_tFrozenTimer.m_uPackedTime == 0) && (m_eActionState == ACTION_HIT) && (fAnimTime >= 4.0f) && (fAnimTime <= 14.0f));

    return isHitting;
}

/**
 * Offset/Address/Size: 0x682C | 0x8001FB68 | size: 0x3C
 */
bool cFielder::IsSlideTackling() const
{
    if ((m_tFrozenTimer.m_uPackedTime == 0) && (m_eActionState == ACTION_SLIDE_ATTACK) && ((mActionSlideAttackVars.eSlideAttackState == SLIDE_ATTACK_DOWN || mActionSlideAttackVars.eSlideAttackState == SLIDE_ATTACK_DECELERATE)))
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x681C | 0x8001FB58 | size: 0x10
 */
bool cFielder::IsStriker() const
{
    return m_eRole == ROLE_STRIKER;
}

/**
 * Offset/Address/Size: 0x6808 | 0x8001FB44 | size: 0x14
 */
bool cFielder::IsWinger() const
{
    return m_eRole == ROLE_WINGER;
}

/**
 * Offset/Address/Size: 0x67F4 | 0x8001FB30 | size: 0x14
 */
bool cFielder::IsMidField() const
{
    return m_eRole == ROLE_MIDFIELD;
}

/**
 * Offset/Address/Size: 0x67E0 | 0x8001FB1C | size: 0x14
 */
bool cFielder::IsDefense() const
{
    return m_eRole == ROLE_DEFENCE;
}

/**
 * Offset/Address/Size: 0x67CC | 0x8001FB08 | size: 0x14
 */
bool cFielder::IsFrozen() const
{
    return m_tFrozenTimer.m_uPackedTime != 0;
}

/**
 * Offset/Address/Size: 0x6700 | 0x8001FA3C | size: 0xCC
 */
void cFielder::SetFrozen(float seconds)
{
    m_tFrozenTimer.SetSeconds(seconds);
    m_fDesiredSpeed = 0.0f;
    m_fActualSpeed = m_fDesiredSpeed;
    SetVelocity(v3Zero);

    if (seconds > 0.0f)
    {
        if (m_eFielderDesireState != FIELDERDESIRE_FINISH_ACTION)
        {
            g_pGame->AbortPendingThought(mThoughtHashCalcDesire);
            g_pGame->AbortPendingThought(mThoughtHashInitRunToNet);
            g_pGame->AbortPendingThought(mThoughtHashInitGetOpen);
            g_pGame->AbortPendingThought(mThoughtHashInitWindupPass);
            g_pGame->AbortPendingThought(mThoughtHashInitCutAndBreak);
            ClearQueuedDesire();
            EndDesire(false);
        }
    }
}

/**
 * Offset/Address/Size: 0x6310 | 0x8001F64C | size: 0x3F0
 */
float cFielder::DoFindBestSlideAttackTarget(nlVector3& v3PositionOut, nlVector3& v3VelocityOut)
{
    int nNumSolutions;
    float pSolutions[2];
    nlVector3 v3BallVelocity;
    nlVector3 v3BallPosition;
    nlVector3 v3LandingSpot;

    v3BallVelocity = g_pBall->m_v3Velocity;
    v3BallPosition = g_pBall->m_v3Position;

    if (g_pBall->GetOwnerFielder() != NULL && g_pBall->GetOwnerFielder()->m_eActionState == ACTION_DEKE)
    {
        v3BallVelocity = v3Zero;
    }
    else if (g_pBall->GetOwnerFielder() != NULL && g_pBall->GetOwnerFielder()->m_eActionState == ACTION_SHOOT_TO_SCORE)
    {
        v3BallVelocity = v3Zero;
        v3BallPosition = g_pBall->GetOwnerFielder()->m_v3Position;
    }

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    const nlVector3& myPos = m_v3Position;
    float fSpeed = 1.0f;
    float fSlideSpeed = pTweaks->fRunningWBTurboSpeedLevel2;

    if (fSlideSpeed >= 0.0f)
    {
        switch (m_ePowerup)
        {
        case POWER_UP_MUSHROOM:
            fSpeed *= g_pGame->m_pGameTweaks->fMushroomSpeed;
            break;
        case POWER_UP_STAR:
            fSpeed *= g_pGame->m_pGameTweaks->fStarSpeed;
            break;
        }
    }

    fSpeed *= fSlideSpeed;

    switch (m_ePowerup)
    {
    case POWER_UP_MUSHROOM:
    case POWER_UP_STAR:
        fSpeed *= 1.4f;
        break;
    }

    CalcInterceptXY(myPos, fSpeed, pTweaks->fPhysCapsuleRadius, v3BallPosition, v3BallVelocity, nNumSolutions, pSolutions);

    float t;

    if (nNumSolutions != 0)
    {
        if (nNumSolutions == 2)
        {
            t = (pSolutions[0] < pSolutions[1]) ? pSolutions[0] : pSolutions[1];
        }
        else
        {
            t = pSolutions[0];
        }

        if (g_pBall->m_pPassTarget != NULL)
        {
            if (g_pBall->m_pPrevOwner->m_tBallUnPossessionTimer.GetSeconds() > 0.25f)
            {
                float dx1 = g_pBall->m_v3Position.x - g_pBall->m_v3PassIntercept.x;
                float dy1 = g_pBall->m_v3Position.y - g_pBall->m_v3PassIntercept.y;
                float fDistToPassIntercept = nlSqrt(dx1 * dx1 + dy1 * dy1, true);

                float predX = g_pBall->m_v3Position.x + t * g_pBall->m_v3Velocity.x;
                float predY = g_pBall->m_v3Position.y + t * g_pBall->m_v3Velocity.y;
                float dx2 = g_pBall->m_v3Position.x - predX;
                float dy2 = g_pBall->m_v3Position.y - predY;
                float fDistToPredicted = nlSqrt(dx2 * dx2 + dy2 * dy2, true);

                if (fDistToPassIntercept < fDistToPredicted || g_pBall->m_v3Position.z > 0.75f)
                {
                    v3PositionOut = g_pBall->m_v3PassIntercept;
                    v3VelocityOut = v3Zero;
                    return 0.0f;
                }
            }
        }

        if (v3BallPosition.z > 0.75f)
        {
            float fLandingTime = g_pBall->PredictLandingSpotAndTime(v3LandingSpot);
            v3PositionOut = v3LandingSpot;
            v3VelocityOut = v3Zero;
            return fLandingTime;
        }
    }
    else
    {
        GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;
        cPlayer* pPassTarget = g_pBall->m_pPassTarget;
        t = pGameTweaks->fSlideAttackTimeToSlide + pGameTweaks->fSlideAttackTimeToDecelrate;

        if (pPassTarget != NULL)
        {
            v3PositionOut = pPassTarget->m_v3Position;
            v3VelocityOut = g_pBall->m_pPassTarget->m_v3Velocity;
            return t;
        }
    }

    v3PositionOut = v3BallPosition;
    v3VelocityOut = v3BallVelocity;
    return t;
}

/**
 * Offset/Address/Size: 0x62DC | 0x8001F618 | size: 0x34
 */
bool cFielder::CanPickupBall(cBall* pBall)
{
    if (m_tFrozenTimer.m_uPackedTime != 0)
    {
        return false;
    }
    return cPlayer::CanPickupBall(pBall);
}

/**
 * Offset/Address/Size: 0x6284 | 0x8001F5C0 | size: 0x58
 */
bool cFielder::CanBeBlownUp()
{
    switch (m_eActionState)
    {
    case ACTION_ELECTROCUTION:
        if (m_eAnimID == 0x76 || m_eAnimID == 0x75)
        {
            return true;
        }
        return false;

    case ACTION_STS_HIT_REACT:
    case ACTION_SQUISH_REACT:
        return false;

    default:
        return true;
    }
}

/**
 * Offset/Address/Size: 0x6230 | 0x8001F56C | size: 0x54
 */
void cFielder::CanBreakOutOfSlideTackle()
{
    if (m_eActionState == ACTION_SLIDE_ATTACK)
    {
        if (mActionSlideAttackVars.bAttackSucceeded != 0)
        {
            m_tSlideAttackTimer.SetSeconds(0.0f);
            mActionSlideAttackVars.eSlideAttackState = SLIDE_ATTACK_DECELERATE;
        }
    }
}

/**
 * Offset/Address/Size: 0x5F54 | 0x8001F290 | size: 0x2DC
 */
eStrafeDirection cFielder::CalculateStrafeDirection(unsigned short aDesiredFacingDirection, unsigned short aDesiredMovementDirection)
{
    s16 nMovementFacingDelta = (s16)(aDesiredMovementDirection - aDesiredFacingDirection);
    float fTransitionToForwardDelta;
    float fTransitionToBackWardsDelta;

    switch (mActionRunningVars.eLastStrafeDirection)
    {
    case STRAFE_RIGHT:
    case STRAFE_LEFT:
        fTransitionToForwardDelta = (float)g_pGame->m_pGameTweaks->nStrafeToRunOutDirectionDelta;
        fTransitionToBackWardsDelta = (float)g_pGame->m_pGameTweaks->nBackwardsToStrafeRunOutDirectionDelta;
        break;

    case STRAFE_IDLE:
    case STRAFE_FORWARD:
    case STRAFE_BACK:
    default:
        fTransitionToForwardDelta = (float)g_pGame->m_pGameTweaks->nStrafeToRunInDirectionDelta;
        fTransitionToBackWardsDelta = (float)g_pGame->m_pGameTweaks->nBackwardsToStrafeRunInDirectionDelta;
        break;
    }

    float zero = 0.1f;
    float desiredSpeed = m_fDesiredSpeed;

    if (!(desiredSpeed > zero))
    {
        return STRAFE_IDLE;
    }

    float runningSpeed = zero + m_pTweaks->fRunningSpeed;
    if (desiredSpeed > runningSpeed)
    {
        float swapFacingSeconds = m_tSwapFacingTimer.GetSeconds();
        if (!swapFacingSeconds)
        {
            return STRAFE_FORWARD;
        }

        if ((float)(u16)((nMovementFacingDelta < 0) ? -nMovementFacingDelta : nMovementFacingDelta) < fTransitionToForwardDelta)
        {
            return STRAFE_FORWARD;
        }

        if ((float)nMovementFacingDelta > -fTransitionToBackWardsDelta)
        {
            if ((float)nMovementFacingDelta <= fTransitionToForwardDelta)
            {
                return STRAFE_RIGHT;
            }
        }

        if ((float)nMovementFacingDelta < fTransitionToBackWardsDelta)
        {
            if ((float)nMovementFacingDelta >= fTransitionToForwardDelta)
            {
                return STRAFE_LEFT;
            }
        }

        return STRAFE_BACK;
    }

    if ((float)(u16)((nMovementFacingDelta < 0) ? -nMovementFacingDelta : nMovementFacingDelta) < fTransitionToForwardDelta)
    {
        return STRAFE_FORWARD;
    }

    if ((float)nMovementFacingDelta > -fTransitionToBackWardsDelta)
    {
        if ((float)nMovementFacingDelta <= fTransitionToForwardDelta)
        {
            return STRAFE_RIGHT;
        }
    }

    if ((float)nMovementFacingDelta < fTransitionToBackWardsDelta)
    {
        if ((float)nMovementFacingDelta >= fTransitionToForwardDelta)
        {
            return STRAFE_LEFT;
        }
    }

    return STRAFE_BACK;
}

/**
 * Offset/Address/Size: 0x5DF0 | 0x8001F12C | size: 0x164
 */
void cFielder::CalcPointOnPerimeter(nlVector3& dest, const nlVector3& fromPoint, float fFutureTimeDelta)
{
    float fMinDistance = 1.0f + m_pTweaks->fPhysCapsuleRadius;
    nlVector3 v3Position;

    if (fFutureTimeDelta > 0.0f)
    {
        v3Position.x = m_v3Position.x + fFutureTimeDelta * m_v3Velocity.x;
        v3Position.y = m_v3Position.y + fFutureTimeDelta * m_v3Velocity.y;
        v3Position.z = m_v3Position.z + fFutureTimeDelta * m_v3Velocity.z;
    }
    else
    {
        v3Position = m_v3Position;
    }

    float dx = fromPoint.x - v3Position.x;
    float dy = fromPoint.y - v3Position.y;
    float dz = fromPoint.z - v3Position.z;
    dest.x = dx;
    dest.y = dy;
    dest.z = dz;

    float fInvLen = nlRecipSqrt(dest.x * dest.x + dest.y * dest.y + dest.z * dest.z, true);

    float nx = fInvLen * dest.x;
    float nz = fInvLen * dest.z;
    float ny = fInvLen * dest.y;
    dest.x = nx;
    dest.y = ny;
    dest.z = nz;

    float rx = v3Position.x + fMinDistance * dest.x;
    float rz = v3Position.z + fMinDistance * dest.z;
    float ry = v3Position.y + fMinDistance * dest.y;
    dest.x = rx;
    dest.y = ry;
    dest.z = rz;
}

void cFielder::CleanActionHit()
{
    Audio::gCrowdSFX.Stop((Audio::eWorldSFX)0x9F, cGameSFX::SFX_STOP_FIRST);
}

void cFielder::CleanActionDeke()
{
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.0f;
}

void cFielder::CleanActionElectrocution()
{
    m_v3Position.z = 0.0f;
    m_v3Velocity.z = 0.0f;
}

void cFielder::CleanActionLooseBallPass()
{
    m_pPhysicsCharacter->m_CanCollideWithWall = true;
    SetNoPickUpTime(0.0f);
    mActionLooseBallPassVars.bVolleyPass = false;
}

void cFielder::CleanActionLooseBallShot()
{
    m_pPhysicsCharacter->m_CanCollideWithWall = true;
    SetNoPickUpTime(0.0f);
    mActionLooseBallShotVars.bIsChipShot = false;
}

void cFielder::CleanActionOnetimer()
{
    EndBlur();
    mActionShotVars.bIsChipShot = false;
}

void cFielder::CleanActionPass()
{
    mActionPassingVars.pPassTarget = NULL;
    mActionPassingVars.bVolleyPass = false;
}

/**
 * Offset/Address/Size: 0x5DA4 | 0x8001F0E0 | size: 0x4C
 */
void cFielder::ClearTimers()
{
    m_tPowerupEffectTime.SetSeconds(0.0f);
    mtBombImpactTime.SetSeconds(0.0f);
    m_tFrozenTimer.SetSeconds(0.0f);
}

/**
 * Offset/Address/Size: 0x5DA0 | 0x8001F0DC | size: 0x4
 */
void cFielder::ClearVolleyPass()
{
}

void cFielder::CleanActionPostWhistle()
{
}

void cFielder::CleanActionRunning()
{
    mActionRunningVars.eLastStrafeDirection = STRAFE_IDLE;
}

void cFielder::CleanActionRunningWB()
{
    m_eLastPadAction = PAD_NONE;
}

void cFielder::CleanActionRunningWBTurbo()
{
    m_eLastPadAction = PAD_NONE;
    if (m_ePowerup != POWER_UP_MUSHROOM)
    {
        EndBlur();
    }
}

/**
 * Offset/Address/Size: 0x5B34 | 0x8001EE70 | size: 0x26C
 */
void cFielder::CleanActionShootToScore()
{
    Audio::FadeFilterFromCurrentToZero();
    FixedUpdateTask::mTimeScale = 1.0f;
    ParticleUpdateTask::SetTimeScale(1.0f);
    g_pEventManager->CreateValidEvent(0x41, 0x14);

    this->mActionShootToScoreVars.isCurrentlyInvincible = false;
    this->mActionShootToScoreVars.isInUnbreakablePart = false;

    WorldDarkening::Instance().Fade(100.0f, 0.0f);
    ShootToScoreMeter::instance.m_bMeterVisible = false;
    DrawableCharacter::RenderAllCharacters();
    DrawableCharacter::sSTSLighting = false;
    g_pGame->mbCaptainShotToScoreOn = false;

    g_pBall->m_pDrawableBall->m_uObjectFlags &= ~0x40;

    BasicString<char, Detail::TempStringAllocator> effectName(
        GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((s16)this->m_pTeam->m_nSide)));
    effectName.AppendInPlace("_captain_sts_effect");
    EffectsGroup* pGroup = fxGetGroup(effectName.c_str());
    KillEffect(pGroup);
    KillEffect(fxGetGroup("shoot_to_score_hyper"));

    if (this->mActionShootToScoreVars.captainStsCamera != NULL)
    {
        cCameraManager::Remove(*this->mActionShootToScoreVars.captainStsCamera);
        delete this->mActionShootToScoreVars.captainStsCamera;
        this->mActionShootToScoreVars.captainStsCamera = NULL;
    }

    ParticleUpdateTask::SetTimeScale(1.0f);
    World::sbIsHyperShootToScoreRenderingEnabled = false;
}

void cFielder::CleanActionShot()
{
    mActionShotVars.bIsChipShot = false;
}

void cFielder::CleanActionSlideAttack()
{
    Audio::gCrowdSFX.Stop((Audio::eWorldSFX)0x9F, cGameSFX::SFX_STOP_FIRST);
    KillSlideTackleTrail(this);
    StopSFX(Audio::CHARSFX_SLIDE);
}

void cFielder::CleanActionSquishReact()
{
    KillDaze(this);
}

/**
 * Offset/Address/Size: 0x5A64 | 0x8001EDA0 | size: 0xD0
 */
void cFielder::SetAttemptOneTouchPass()
{
    bool shouldAttempt = false;

    cGlobalPad* pad = GetGlobalPad();
    if (pad != NULL)
    {
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        float pressure = GetGlobalPad()->GetPressure(0x15, true);
        if (pressure > tweaks->fLeftTriggerDownPressure)
        {
            shouldAttempt = true;
        }
    }

    m_DesireReceivePassSharedVars.iAttemptOneTouchPass = shouldAttempt ? 2 : 1;

    m_DesireReceivePassSharedVars.pOneTouchPassTarget = DoFindBestPassTarget(ReceivingVolleyPass(this), shouldAttempt);
    m_DesireReceivePassSharedVars.iAttemptOneTouchShot = 0;
}

/**
 * Offset/Address/Size: 0x59C0 | 0x8001ECFC | size: 0xA4
 */
void cFielder::SetAttemptOneTouchShot()
{
    bool shouldAttempt = false;

    cGlobalPad* pad = GetGlobalPad();
    if (pad != NULL)
    {
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        float pressure = GetGlobalPad()->GetPressure(0x15, true);
        if (pressure > tweaks->fLeftTriggerDownPressure)
        {
            shouldAttempt = true;
        }
    }

    m_DesireReceivePassSharedVars.iAttemptOneTouchShot = shouldAttempt ? 2 : 1;
    m_DesireReceivePassSharedVars.iAttemptOneTouchPass = 0;
    m_DesireReceivePassSharedVars.pOneTouchPassTarget = NULL;
}

/**
 * Offset/Address/Size: 0x595C | 0x8001EC98 | size: 0x64
 */
s16 cFielder::GetOneTouchShotDesire()
{
    float fZero = 0.f;
    float fResult = ReceivingPass(this);
    if (fResult != fZero)
    {
        return ((m_DesireReceivePassSharedVars.iAttemptOneTouchShot != 0) || (m_DesireReceivePassSharedVars.iAttemptOneTouchPass != 0));
    }

    return 0;
}

/**
 * Offset/Address/Size: 0x573C | 0x8001EA78 | size: 0x220
 */
void cFielder::SetStartAnimState(int animState)
{
    static int RunStartAnims[4] = { 5, 5, 6, 4 };

    if (animState != -1)
    {
        SetAnimState(RunStartAnims[animState], true, 0.2f, false, false);

        s16 turnAdjust = CalcAnimTurnAdjust(m_aActualFacingDirection, m_aDesiredFacingDirection, m_eAnimID);
        InitMovementFromAnim(turnAdjust, v3Zero, 1.0f, false);

        m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.2f;
    }
    else
    {
        int nAnimState = ((m_aDesiredFacingDirection - m_aActualFacingDirection + 0x2000) >> 14) & 3;

        if (nAnimState != 0)
        {
            SetAnimState(RunStartAnims[nAnimState], true, 0.2f, false, false);

            s16 turnAdjust = CalcAnimTurnAdjust(m_aActualFacingDirection, m_aDesiredFacingDirection, m_eAnimID);
            InitMovementFromAnim(turnAdjust, v3Zero, 1.0f, false);

            m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.2f;
        }
        else
        {
            SetRunningAnimState(0.1f);
        }
    }
}

/**
 * Offset/Address/Size: 0x5618 | 0x8001E954 | size: 0x124
 */
void cFielder::SetWindupWBAnimState()
{
    cNet* pOtherNet = m_pTeam->GetOtherNet();
    s16 facingDelta = GetFacingDeltaToPosition(pOtherNet->m_v3NetLocation);

    if (facingDelta < 0)
    {
        SetAnimState(0x57, true, 0.2f, false, false);
    }
    else
    {
        SetAnimState(0x56, true, 0.2f, false, false);
    }

    m_fDesiredSpeed = 0.0f;
    if (m_fActualSpeed > ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed)
    {
        m_fActualSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed;
    }

    InitMovementRunningNoTurn(0.0f, ((FielderTweaks*)m_pTweaks)->fShotWindupDecel);

    Event* pEvent = g_pEventManager->CreateValidEvent(0x15, 0x1c);
    ShotAtGoalData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) ShotAtGoalData();
    pData->pShooter = this;

    Play3DSFX(Audio::CHARSFX_SHOT_WINDUP, PHYSOBJ, 100.0f);
    Play3DSFX(Audio::CHARSFX_EFFORTS_HEAD_SHAKE, PHYSOBJ, 100.0f);

    EmitWindupAtBall(this, "ball_shot_windup");
}

/**
 * Offset/Address/Size: 0x5464 | 0x8001E7A0 | size: 0x1B4
 */
void cFielder::SetStartWBAnimState()
{
    static int RunWBStartAnims[4] = { 0x17, 0x17, 0x18, 0x16 };

    int nAnimState = ((m_aDesiredFacingDirection - m_aActualFacingDirection + 0x2000) >> 14) & 3;

    if (nAnimState != 0)
    {
        SetAnimState(RunWBStartAnims[nAnimState], true, 0.2f, false, false);

        s16 turnAdjust = CalcAnimTurnAdjust(m_aActualFacingDirection, m_aDesiredFacingDirection, m_eAnimID);
        InitMovementFromAnim(turnAdjust, v3Zero, 1.0f, false);

        m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.2f;
    }
    else
    {
        SetRunningWBAnimState(0.1f);
    }
}

/**
 * Offset/Address/Size: 0x53E0 | 0x8001E71C | size: 0x84
 */
void cFielder::SetRunTurboAnimState(int animID, bool bForceMirrorSwap)
{
    if (animID == 0x1D)
    {
        EmitTurbo(this, "footy_burst");
    }

    SetAnimState(animID, false, 0.067f, false, bForceMirrorSwap);

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    InitMovementRunningNoTurn(pTweaks->fRunningWBTurboAccel, pTweaks->fRunningWBTurboDecel);
}

/**
 * Offset/Address/Size: 0x5350 | 0x8001E68C | size: 0x90
 */
void cFielder::SetHardStopAnimState()
{
    if (m_pBall != nullptr)
    {
        SetAnimState(0x24, true, 0.2f, false, false);
    }
    else
    {
        SetAnimState(0x12, true, 0.2f, false, false);
    }
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.4f;
}

/**
 * Offset/Address/Size: 0x52C0 | 0x8001E5FC | size: 0x90
 */
void cFielder::SetHardStopRecoverAnimState()
{
    if (m_pBall != nullptr)
    {
        SetAnimState(0x26, false, 0.03f, false, false);
    }
    else
    {
        SetAnimState(0x14, false, 0.03f, false, false);
    }
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.4f;
}

/**
 * Offset/Address/Size: 0x5230 | 0x8001E56C | size: 0x90
 */
void cFielder::SetHardStopTurnAnimState()
{
    if (m_pBall != nullptr)
    {
        SetAnimState(0x25, false, 0.03f, false, false);
    }
    else
    {
        SetAnimState(0x13, false, 0.03f, false, false);
    }
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.4f;
}

/**
 * Offset/Address/Size: 0x51CC | 0x8001E508 | size: 0x64
 */
void cFielder::SetRunBackwardsAnimState()
{
    SetAnimState(0x27, true, 0.067f, true, false);

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    InitMovementStrafing(pTweaks->fRunningStrafeDirectionSeekSpeed, pTweaks->fRunningStrafeDirectionSeekFalloff, pTweaks->fRunningStrafeAccel, pTweaks->fRunningStrafeDecel);

    m_aActualMovementDirection = m_aDesiredMovementDirection;
}

/**
 * Offset/Address/Size: 0x515C | 0x8001E498 | size: 0x70
 */
void cFielder::SetRunToBackRunningAnimState()
{
    SetAnimState(0x2d, true, 0.067f, true, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_aActualMovementDirection = m_aDesiredMovementDirection;
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.25f;
}

/**
 * Offset/Address/Size: 0x50EC | 0x8001E428 | size: 0x70
 */
void cFielder::SetBackRunningToRunAnimState()
{
    SetAnimState(0x2e, true, 0.067f, true, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_aActualMovementDirection = m_aDesiredMovementDirection;
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.25f;
}

/**
 * Offset/Address/Size: 0x507C | 0x8001E3B8 | size: 0x70
 */
void cFielder::SetBackRunningStopAnimState()
{
    SetAnimState(0x2a, true, 0.067f, true, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_aActualMovementDirection = m_aDesiredMovementDirection;
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.25f;
}

/**
 * Offset/Address/Size: 0x500C | 0x8001E348 | size: 0x70
 */
void cFielder::SetBackRunningStopStartAnimState()
{
    SetAnimState(0x2b, true, 0.067f, true, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_aActualMovementDirection = m_aDesiredMovementDirection;
    m_pCurrentAnimController->m_fPlaybackSpeedScale = 1.25f;
}

/**
 * Offset/Address/Size: 0x4FA8 | 0x8001E2E4 | size: 0x64
 */
void cFielder::SetBackRunningStopRecoverAnimState()
{
    SetAnimState(0x2c, true, 0.067f, true, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_aActualMovementDirection = m_aDesiredMovementDirection;
}

/**
 * Offset/Address/Size: 0x4F44 | 0x8001E280 | size: 0x64
 */
void cFielder::SetStopAnimState()
{
    SetAnimState(0xb, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_fDesiredSpeed = 0.0f;
}

/**
 * Offset/Address/Size: 0x4EE8 | 0x8001E224 | size: 0x5C
 */
void cFielder::SetStopWBAnimState()
{
    SetAnimState(0x23, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
}

/**
 * Offset/Address/Size: 0x4E84 | 0x8001E1C0 | size: 0x64
 */
void cFielder::SetStrafeLeftAnimState()
{
    SetAnimState(0x28, true, 0.067f, true, false);

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    InitMovementStrafing(pTweaks->fRunningStrafeDirectionSeekSpeed, pTweaks->fRunningStrafeDirectionSeekFalloff, pTweaks->fRunningStrafeAccel, pTweaks->fRunningStrafeDecel);

    m_aActualMovementDirection = m_aDesiredMovementDirection;
}

/**
 * Offset/Address/Size: 0x4E20 | 0x8001E15C | size: 0x64
 */
void cFielder::SetStrafeRightAnimState()
{
    SetAnimState(0x29, true, 0.067f, true, false);

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    InitMovementStrafing(pTweaks->fRunningStrafeDirectionSeekSpeed, pTweaks->fRunningStrafeDirectionSeekFalloff, pTweaks->fRunningStrafeAccel, pTweaks->fRunningStrafeDecel);

    m_aActualMovementDirection = m_aDesiredMovementDirection;
}

/**
 * Offset/Address/Size: 0x4DB4 | 0x8001E0F0 | size: 0x6C
 */
void cFielder::SetIdleStrafeAnimState()
{
    SetAnimState(0, true, 0.2f, false, false);

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    InitMovementRunning(pTweaks->fRunningDirectionSeekSpeed, pTweaks->fRunningDirectionSeekFalloff, pTweaks->fRunningAccel, pTweaks->fRunningDecel);

    m_fActualSpeed = 0.0f;
    m_aActualMovementDirection = m_aDesiredMovementDirection;
}

/**
 * Offset/Address/Size: 0x4D58 | 0x8001E094 | size: 0x5C
 */
void cFielder::SetIdleAnimState()
{
    SetAnimState(0, true, 0.2f, false, false);

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    InitMovementRunning(pTweaks->fRunningDirectionSeekSpeed, pTweaks->fRunningDirectionSeekFalloff, pTweaks->fRunningAccel, pTweaks->fRunningDecel);
}

/**
 * Offset/Address/Size: 0x4CFC | 0x8001E038 | size: 0x5C
 */
void cFielder::SetIdleWBAnimState()
{
    SetAnimState(0x15, true, 0.2f, false, false);

    FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
    InitMovementRunning(pTweaks->fRunningWBDirectionSeekSpeed, pTweaks->fRunningWBDirectionSeekFalloff, pTweaks->fRunningWBAccel, pTweaks->fRunningWBDecel);
}

void cFielder::JogRunSynchronizedWeightCallback(uintptr_t nParam, cPN_SAnimController* pController)
{
    cFielder* pChar = (cFielder*)nParam;
    if (pChar->m_eAnimID == 0x07 || pChar->m_eAnimID == 0x1A)
    {
        pController->m_fSynchronizedWeight = pChar->CalcJogRunBlendWeight();
    }
    else
    {
        pController->m_fSynchronizedWeight = pChar->CalcRunTurboBlendWeight();
    }
}

void cFielder::JogRunSABcallback(uintptr_t nParam1, cPN_SingleAxisBlender* pSAB)
{
    cFielder* pChar = (cFielder*)nParam1;
    if (pChar->m_eAnimID == 0x07)
    {
        pSAB->m_fDesiredWeight = pChar->CalcJogRunBlendWeight();
    }
    else if (pChar->m_eAnimID == 0x0F)
    {
        pSAB->m_fDesiredWeight = pChar->CalcRunTurboBlendWeight();
    }
    else
    {
        pSAB->m_fDesiredWeight = pSAB->m_fSmoothedWeight;
    }
}

/**
 * Offset/Address/Size: 0x4CA8 | 0x8001DFE4 | size: 0x54
 */
void cFielder::RunningSABcallback(uintptr_t nParam1, cPN_SingleAxisBlender* pSAB)
{
    cFielder* pThis = (cFielder*)nParam1;

    if (pThis->m_eAnimID == ACTION_LOOSE_BALL_SHOT || pThis->m_eAnimID == 0x1A || pThis->m_eAnimID == ACTION_RUNNING_WB_TURBO)
    {
        float fWeight = (0.5f * pThis->m_fLeanAmount) + 0.5f;
        if (pThis->m_pCurrentAnimController->m_bMirror != 0)
        {
            fWeight = 1.0f - fWeight;
        }

        pSAB->m_fDesiredWeight = fWeight;
        return;
    }
    pSAB->m_fDesiredWeight = pSAB->m_fSmoothedWeight;
}

void cFielder::SetRunLeanSAB(const int* pSABAnims, int nNumSABAnims, int nPrimaryAnim)
{
    cPN_SingleAxisBlender* pSAB;
    cPN_SAnimController* synchingController;
    int i;
    cPN_SAnimController* nextController;

    pSAB = CreateSingleAxisBlender(pSABAnims, nNumSABAnims, nPrimaryAnim, RunningSABcallback, 0.1f, nullptr);

    synchingController = (cPN_SAnimController*)pSAB->GetChild(nPrimaryAnim);
    synchingController->m_fSynchronizedWeight = 0.0f;

    for (i = 0; i < nNumSABAnims; ++i)
    {
        if (i != nPrimaryAnim)
        {
            nextController = (cPN_SAnimController*)pSAB->GetChild(i);
            nextController->m_bIsSynchronized = true;
            synchingController->m_pSynchronizedController = nextController;
            synchingController = nextController;
        }
    }

    *m_pAILayer = ::new (AllocateBlender()) cPN_Blender(*m_pAILayer, pSAB, 0.1f);
}

void cFielder::SetJogRunLeanSAB(
    const int* pRunningAnims, int nNumRunningAnims, int nPrimaryRunningAnim, int nJogAnim, float fBlendTime, float fWeightSeek)
{
    cPN_SAnimController* pJoggingController = NewAnimController(nJogAnim, false, false, NULL, 0);
    pJoggingController->m_funcSychronizedWeightCallback = JogRunSynchronizedWeightCallback;
    pJoggingController->m_nSynchronizedWeightCallbackParam = (uintptr_t)this;
    pJoggingController->m_fSynchronizedWeight = 0.0f;

    cPN_SingleAxisBlender* pRunningSAB = ::new (AllocateSingleAxisBlender()) cPN_SingleAxisBlender(2, JogRunSABcallback, (uintptr_t)this, 0.1f);
    pRunningSAB->SetChild(0, pJoggingController);
    pRunningSAB->SetChild(1,
        CreateSingleAxisBlender(pRunningAnims, nNumRunningAnims, nPrimaryRunningAnim, RunningSABcallback, 0.1f, pJoggingController));

    *m_pAILayer = ::new (AllocateBlender()) cPN_Blender(*m_pAILayer, pRunningSAB, fBlendTime);
}

/**
 * Offset/Address/Size: 0x4B70 | 0x8001DEAC | size: 0x138
 */
void cFielder::SetRunningAnimState(float fBlendTime)
{
    int RunningAnims[3] = {
        0x0000000D,
        0x00000007,
        0x0000000E,
    };

    SetRunLeanSAB(RunningAnims, 3, 1);

    FielderTweaks* pTweaks = ((FielderTweaks*)m_pTweaks);
    InitMovementRunning(pTweaks->fRunningDirectionSeekSpeed, pTweaks->fRunningDirectionSeekFalloff, pTweaks->fRunningAccel, pTweaks->fRunningDecel);
}

/**
 * Offset/Address/Size: 0x4A30 | 0x8001DD6C | size: 0x140
 */
void cFielder::SetRunningTurboAnimState()
{
    int RunningTurboAnims[3] = {
        0x00000010,
        0x0000000F,
        0x00000011,
    };

    SetRunLeanSAB(RunningTurboAnims, 3, 1);

    FielderTweaks* pTweaks = ((FielderTweaks*)m_pTweaks);
    InitMovementRunning(pTweaks->fRunningTurboDirectionSeekSpeed, pTweaks->fRunningTurboDirectionSeekFalloff, pTweaks->fRunningTurboAccel, pTweaks->fRunningTurboDecel);

    mActionRunningVars.bFirstCycleOfTurbo = true;
}

/**
 * Offset/Address/Size: 0x48F8 | 0x8001DC34 | size: 0x138
 */
void cFielder::SetRunningWBAnimState(float fBlendTime)
{
    int RunningWBAnims[3] = {
        0x0000001B,
        0x0000001A,
        0x0000001C,
    };

    SetRunLeanSAB(RunningWBAnims, 3, 1);

    FielderTweaks* pTweaks = ((FielderTweaks*)m_pTweaks);
    InitMovementRunning(pTweaks->fRunningWBDirectionSeekSpeed, pTweaks->fRunningWBDirectionSeekFalloff, pTweaks->fRunningWBAccel, pTweaks->fRunningWBDecel);
}

/**
 * Offset/Address/Size: 0x48A8 | 0x8001DBE4 | size: 0x50
 */
bool cFielder::ShouldIClearBall()
{
    cNet* pOtherNet = m_pTeam->GetOtherNet();
    float fPositionX = m_v3Position.x;
    float fSideSign = pOtherNet->m_fDirection;
    float fResult = fPositionX * fSideSign;

    return fResult <= 0.0f;
}

/**
 * Offset/Address/Size: 0x4894 | 0x8001DBD0 | size: 0x14
 */
bool cFielder::ShouldILeadPass()
{
    return m_eActionState == ACTION_RUNNING;
}

/**
 * Offset/Address/Size: 0x4564 | 0x8001D8A0 | size: 0x330
 */
bool cFielder::CanISlideAttack(const nlVector3& v3Position, const nlVector3& v3Velocity, float* fTime)
{
    float t;
    int nNumSolutions;
    float pSolutions[2];
    float fMaxT;
    nlPolar polarDesiredVelocity;

    if (m_eActionState == ACTION_SLIDE_ATTACK)
    {
        return false;
    }

    if (v3Position.z > 0.5f)
    {
        return false;
    }

    float fYDiff = m_v3Position.y - v3Position.y;
    float fXDiff = m_v3Position.x - v3Position.x;

    t = -1.0f;

    GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;

    if (nlSqrt(fXDiff * fXDiff + fYDiff * fYDiff, true) < pGameTweaks->fSlideAttackRadius)
    {
        FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
        const nlVector3& myPos = m_v3Position;
        float fSpeed = 1.0f;
        float fSlideSpeed = pTweaks->fRunningWBTurboSpeedLevel2;

        if (fSlideSpeed >= 0.0f)
        {
            switch (m_ePowerup)
            {
            case POWER_UP_MUSHROOM:
                fSpeed *= g_pGame->m_pGameTweaks->fMushroomSpeed;
                break;
            case POWER_UP_STAR:
                fSpeed *= g_pGame->m_pGameTweaks->fStarSpeed;
                break;
            }
        }

        fSpeed *= fSlideSpeed;

        switch (m_ePowerup)
        {
        case POWER_UP_MUSHROOM:
        case POWER_UP_STAR:
            fSpeed *= 1.4f;
            break;
        }

        CalcInterceptXY(myPos, fSpeed, pTweaks->fPhysCapsuleRadius, v3Position, v3Velocity, nNumSolutions, pSolutions);

        if (nNumSolutions != 0)
        {
            if (nNumSolutions == 2)
            {
                t = pSolutions[0] < pSolutions[1] ? pSolutions[0] : pSolutions[1];
            }
            else
            {
                t = pSolutions[0];
            }
        }
    }

    float fZero = 0.0f;
    fMaxT = g_pGame->m_pGameTweaks->fSlideAttackTimeToSlide + g_pGame->m_pGameTweaks->fSlideAttackTimeToDecelrate;

    if (t > fZero && t <= fMaxT)
    {
        if (fTime != nullptr)
        {
            float fInterceptX = v3Velocity.x * t + v3Position.x;
            float fInterceptY = v3Velocity.y * t + v3Position.y;
            float fToInterceptX = fInterceptX - m_v3Position.x;
            float fToInterceptY = fInterceptY - m_v3Position.y;

            float fLenSq = fToInterceptX * fToInterceptX + fToInterceptY * fToInterceptY;
            float fInvLen = nlRecipSqrt(fZero + fLenSq, true);
            FielderTweaks* pTweaks = (FielderTweaks*)m_pTweaks;
            float fSpeedX = 1.0f;
            float fSlideSpeed = pTweaks->fRunningWBTurboSpeedLevel2;
            float fDesiredVelX = fInvLen * fToInterceptX;
            float fDesiredVelY = fInvLen * fToInterceptY;
            if (fSlideSpeed >= 0.0f)
            {
                switch (m_ePowerup)
                {
                case POWER_UP_MUSHROOM:
                    fSpeedX *= g_pGame->m_pGameTweaks->fMushroomSpeed;
                    break;
                case POWER_UP_STAR:
                    fSpeedX *= g_pGame->m_pGameTweaks->fStarSpeed;
                    break;
                }
            }

            fSpeedX *= fSlideSpeed;

            switch (m_ePowerup)
            {
            case POWER_UP_MUSHROOM:
            case POWER_UP_STAR:
                fSpeedX *= 1.4f;
                break;
            }

            fDesiredVelX *= fSpeedX;

            float fSpeedY = 1.0f;
            if (fSlideSpeed >= 0.0f)
            {
                switch (m_ePowerup)
                {
                case POWER_UP_MUSHROOM:
                    fSpeedY *= g_pGame->m_pGameTweaks->fMushroomSpeed;
                    break;
                case POWER_UP_STAR:
                    fSpeedY *= g_pGame->m_pGameTweaks->fStarSpeed;
                    break;
                }
            }

            fSpeedY *= fSlideSpeed;

            switch (m_ePowerup)
            {
            case POWER_UP_MUSHROOM:
            case POWER_UP_STAR:
                fSpeedY *= 1.4f;
                break;
            }

            fDesiredVelY *= fSpeedY;

            nlCartesianToPolar(polarDesiredVelocity, fDesiredVelX, fDesiredVelY);

            *fTime = t;
        }

        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x4518 | 0x8001D854 | size: 0x4C
 */
void cFielder::SetPosition(const nlVector3& v3Position)
{
    m_v3DesiredPosition = v3Position;
    cCharacter::SetPosition(v3Position);
    m_fDistanceToDesiredPosition = 0.0f;
}

static inline bool IsNearlyZero(float value, float zero)
{
    float bNearlyZero = (float)(fabsf(value - zero) <= 0.0001f);
    return bNearlyZero == zero;
}

/**
 * Offset/Address/Size: 0x3F70 | 0x8001D2AC | size: 0x5A8
 */
void cFielder::SetDesiredSpeedAndDirectionToPosition(float fDeltaT, const nlVector3& v3Pos, eTurboRequest turboRequest, float fInRadiusMult, float fOutRadiusMult)
{
    nlVector3 v3FixedPos = v3Pos;
    cField::FixOutOfBoundsPosition(v3FixedPos, 0.2f);

    nlVector3 v3Delta;
    v3Delta.Sub2D(v3FixedPos, m_v3Position);
    float fDeltaY = v3Delta.y;
    float fDeltaYFromDesired = v3FixedPos.y - m_v3DesiredPosition.y;
    float fDeltaX = v3FixedPos.x - m_v3Position.x;
    float fDeltaXFromDesired = v3FixedPos.x - m_v3DesiredPosition.x;
    float fDistSq = fDeltaX * fDeltaX + fDeltaY * fDeltaY;
    float fDeltaZFromDesired = v3FixedPos.z - m_v3DesiredPosition.z;

    float fDesiredPositionRateOfChange = 0.0f;
    float fZero = fDesiredPositionRateOfChange;
    float bAtTarget = (float)(fabsf(fDistSq - fZero) <= 0.0001f);
    if (bAtTarget != fZero)
    {
    }
    else
    {
        fDesiredPositionRateOfChange = nlSqrt(fDeltaXFromDesired * fDeltaXFromDesired + fDeltaYFromDesired * fDeltaYFromDesired + fDeltaZFromDesired * fDeltaZFromDesired, true) / fDeltaT;
        float fAngle = nlATan2f(v3FixedPos.y - m_v3Position.y, v3FixedPos.x - m_v3Position.x);
        m_aDesiredFacingDirection = (u16)(fAngle * 10430.378f);
        m_aDesiredMovementDirection = m_aDesiredFacingDirection;
    }

    float fSpeedPercent = 0.0f;
    switch (m_ePositionSeekState)
    {
    case PSS_ARRIVED:
    {
        float fOutRad = fOutRadiusMult * g_pGame->m_pGameTweaks->fArrivalOutRadius;
        fSpeedPercent = NormalizeVal(fDistSq, 0.0f, fOutRad * fOutRad);
        float fNearSeekOut = fOutRadiusMult * g_pGame->m_pGameTweaks->fNearSeekOutRadius;
        if (fDistSq >= fNearSeekOut * fNearSeekOut)
            m_ePositionSeekState = PSS_FAR_SEEKING;
        else
        {
            float fArrivalOut = fOutRadiusMult * g_pGame->m_pGameTweaks->fArrivalOutRadius;
            if (fDistSq >= fArrivalOut * fArrivalOut)
                m_ePositionSeekState = PSS_NEAR_SEEKING;
        }
        break;
    }
    case PSS_NEAR_SEEKING:
    {
        float fOutRad = fOutRadiusMult * g_pGame->m_pGameTweaks->fNearSeekOutRadius;
        float fInRad = fInRadiusMult * g_pGame->m_pGameTweaks->fArrivalInRadius;
        fSpeedPercent = NormalizeVal(fDistSq, fInRad * fInRad, fOutRad * fOutRad);
        float fNearSeekOut = fOutRadiusMult * g_pGame->m_pGameTweaks->fNearSeekOutRadius;
        if (fDistSq >= fNearSeekOut * fNearSeekOut)
            m_ePositionSeekState = PSS_FAR_SEEKING;
        else
        {
            float fArrivalIn = fInRadiusMult * g_pGame->m_pGameTweaks->fArrivalInRadius;
            if (fDistSq <= fArrivalIn * fArrivalIn)
                m_ePositionSeekState = PSS_ARRIVED;
        }
        break;
    }
    case PSS_FAR_SEEKING:
    {
        float fOutRad = fOutRadiusMult * g_pGame->m_pGameTweaks->fNearSeekOutRadius;
        float fInRad = fInRadiusMult * g_pGame->m_pGameTweaks->fNearSeekInRadius;
        fSpeedPercent = NormalizeVal(fDistSq, fInRad * fInRad, fOutRad * fOutRad);
        float fNearSeekIn = fInRadiusMult * g_pGame->m_pGameTweaks->fNearSeekInRadius;
        if (fDistSq < fNearSeekIn * fNearSeekIn)
            m_ePositionSeekState = PSS_NEAR_SEEKING;
        else
        {
            float fArrivalIn = fInRadiusMult * g_pGame->m_pGameTweaks->fArrivalInRadius;
            if (fDistSq < fArrivalIn * fArrivalIn)
                m_ePositionSeekState = PSS_ARRIVED;
        }
        break;
    }
    }

    float fMaxSpeed = 0.0f;
    float fMinSpeed = fMaxSpeed;
    if (m_pBall == NULL)
    {
        switch (m_ePositionSeekState)
        {
        case PSS_ARRIVED:
            fMinSpeed = 0.0f;
            fMaxSpeed = 0.0f;
            break;
        case PSS_NEAR_SEEKING:
            fMinSpeed = m_pTweaks->fJoggingSpeed;
            fMaxSpeed = m_pTweaks->fRunningSpeed;
            break;
        case PSS_FAR_SEEKING:
            fMinSpeed = m_pTweaks->fRunningSpeed;
            fMaxSpeed = ((FielderTweaks*)m_pTweaks)->fRunningTurboSpeed;
            break;
        default:
            break;
        }
        if (turboRequest == TR_FORCED_OFF)
        {
            fMaxSpeed = nlMinEquals(fMaxSpeed, m_pTweaks->fRunningSpeed);
        }
        else if (turboRequest == TR_FORCED_ON || (turboRequest == TR_MOVING_TARGET && IsNearlyZero(fDesiredPositionRateOfChange, fZero)))
        {
            fMinSpeed = ((FielderTweaks*)m_pTweaks)->fRunningTurboSpeed;
        }
    }
    else
    {
        switch (m_ePositionSeekState)
        {
        case PSS_ARRIVED:
            fMinSpeed = 0.0f;
            fMaxSpeed = 0.0f;
            break;
        case PSS_NEAR_SEEKING:
            fMinSpeed = m_pTweaks->fJoggingSpeed;
            fMaxSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed;
            break;
        case PSS_FAR_SEEKING:
            fMinSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed;
            fMaxSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBTurboSpeedLevel1;
            break;
        default:
            break;
        }
        if (turboRequest == TR_FORCED_OFF)
        {
            fMaxSpeed = nlMinEquals(fMaxSpeed, ((FielderTweaks*)m_pTweaks)->fRunningWBSpeed);
        }
        else if (turboRequest == TR_FORCED_ON || (turboRequest == TR_MOVING_TARGET && IsNearlyZero(fDesiredPositionRateOfChange, fZero)))
        {
            fMinSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBTurboSpeedLevel1;
        }
    }

    float bDistZero = (float)(fabsf(fDistSq - fZero) <= 0.0001f);
    if (bDistZero != fZero)
    {
        fMinSpeed = 0.0f;
        fMaxSpeed = 0.0f;
    }

    if (!IsRunning())
    {
        if (m_pBall != NULL)
            InitActionRunningWB(false);
        else
            InitActionRunning();
    }

    m_fDesiredSpeed = Interpolate(fMinSpeed, fMaxSpeed, fSpeedPercent);
    m_v3DesiredPosition = v3FixedPos;
    m_fDistanceToDesiredPosition = -9999.9f;
}

/**
 * Offset/Address/Size: 0x3EE8 | 0x8001D224 | size: 0x88
 */
void cFielder::SetDesiredSpeed(float fMinSpeed, float fMaxSpeed)
{
    if (m_pController != nullptr)
    {
        float fMagnitude = m_pController->GetMovementStickMagnitude();
        if (fMagnitude > 0.0f)
        {
            m_fDesiredSpeed = (fMaxSpeed - fMinSpeed) * m_pController->GetMovementStickMagnitude() + fMinSpeed;
        }
        else
        {
            m_fDesiredSpeed = 0.0f;
        }
    }
}

/**
 * Offset/Address/Size: 0x3E8C | 0x8001D1C8 | size: 0x5C
 */
float cFielder::GetSpeedPowerupAdjusted(float fSpeed)
{
    float fMultiplier = 1.0f;

    if (fSpeed >= 0.0f)
    {
        switch (m_ePowerup)
        {
        case POWER_UP_MUSHROOM:
            fMultiplier = fMultiplier * g_pGame->m_pGameTweaks->fMushroomSpeed;
            break;
        case POWER_UP_STAR:
            fMultiplier = fMultiplier * g_pGame->m_pGameTweaks->fStarSpeed;
            break;
        }
    }

    return fMultiplier * fSpeed;
}

/**
 * Offset/Address/Size: 0x3E0C | 0x8001D148 | size: 0x80
 */
float cFielder::GetSlideAttackSpeed()
{
    float fSpeed = ((FielderTweaks*)m_pTweaks)->fRunningWBTurboSpeedLevel2;
    float fResult = 1.0f;

    if (fSpeed >= 0.0f)
    {
        switch (m_ePowerup)
        {
        case POWER_UP_MUSHROOM:
            fResult *= g_pGame->m_pGameTweaks->fMushroomSpeed;
            break;
        case POWER_UP_STAR:
            fResult *= g_pGame->m_pGameTweaks->fStarSpeed;
            break;
        }
    }

    fResult *= fSpeed;

    if (m_ePowerup >= 9)
    {
        return fResult;
    }

    if (m_ePowerup < POWER_UP_MUSHROOM)
    {
        return fResult;
    }

    float fMultiplier = 1.4f;
    return fResult * fMultiplier;
}

/**
 * Offset/Address/Size: 0x3D5C | 0x8001D098 | size: 0xB0
 */
bool cFielder::SetDesire(eFielderDesireState eNewDesire, float fConfidence)
{
    GetCommonDesireData(eNewDesire).NormalizeConfidence(fConfidence);
    m_fDesireConfidence = fConfidence;

    eFielderDesireState currentDesire = m_eFielderDesireState;
    if (currentDesire != eNewDesire)
    {
        if (currentDesire != 0x17 && currentDesire != 0x0 && currentDesire != 0x15)
        {
            m_ePrevFielderDesireState = currentDesire;
        }

        float fDuration = m_tDesireDuration.GetSeconds();
        CleanUpDesire(eNewDesire);
        m_tDesireDuration.SetSeconds(fDuration);
        m_eFielderDesireState = eNewDesire;

        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x3B44 | 0x8001CE80 | size: 0x218
 */
u8 cFielder::ShouldIStrafe()
{
    u16 aDesiredFacingDir = m_aDesiredMovementDirection;
    bool shouldStrafe;

    if (m_pBall != NULL)
    {
        shouldStrafe = true;
        bool isTurboing = false;
        s32 animID = m_eAnimID;
        if (!(animID != 0x1D && animID != 0x1E && animID != 0x1F && animID != 0x20 && animID != 0x21 && animID != 0x22))
        {
            isTurboing = true;
        }
        if (!isTurboing)
        {
            shouldStrafe = false;
        }
    }
    else
    {
        shouldStrafe = true;
        bool isTurboing = false;
        s32 animID = m_eAnimID;
        if (!(animID != 0x10 && animID != 0x0F && animID != 0x11))
        {
            isTurboing = true;
        }
        if (!isTurboing)
        {
            shouldStrafe = false;
        }
    }

    if (!shouldStrafe)
    {
        float fStrafeBall = Fuzzy::ShouldIStrafeBall(this).mData.f;
        float strafeMarkValue = Fuzzy::ShouldIStrafeMark(this).mData.f;
        float fStrafeMark = strafeMarkValue;
        float fTotalScore = fStrafeBall + fStrafeMark;

        if (GetGlobalPad() != NULL)
        {
            fStrafeBall = fTotalScore;
            fStrafeMark = 0.0f;
        }

        bool isStrafing = false;
        s32 strafeDir = mActionRunningVars.eLastStrafeDirection;
        if (!(strafeDir != STRAFE_LEFT && strafeDir != STRAFE_RIGHT && strafeDir != STRAFE_BACK))
        {
            isStrafing = true;
        }

        float threshold;
        if (isStrafing)
        {
            threshold = 0.5f;
        }
        else
        {
            threshold = 0.75f;
        }

        if (fTotalScore > threshold)
        {
            float ballWeight = fStrafeBall / fTotalScore;
            float targetY = ballWeight * g_pBall->m_v3Position.y;
            float targetX = ballWeight * g_pBall->m_v3Position.x;

            if (m_pMark != NULL)
            {
                float markWeight = fStrafeMark / fTotalScore;
                targetX += markWeight * m_pMark->m_v3Position.x;
                targetY += markWeight * m_pMark->m_v3Position.y;
            }

            float angle = nlATan2f(targetY - m_v3Position.y, targetX - m_v3Position.x);
            aDesiredFacingDir = (u16)(s32)(10430.378f * angle);
        }

        eStrafeDirection strafeResult = CalculateStrafeDirection(aDesiredFacingDir, m_aDesiredMovementDirection);
        if (strafeResult == STRAFE_FORWARD)
        {
            aDesiredFacingDir = m_aDesiredMovementDirection;
        }
    }

    m_aDesiredFacingDirection = aDesiredFacingDir;
    return m_aDesiredFacingDirection != m_aDesiredMovementDirection;
}

bool cFielder::ShouldITurboWithBall()
{
    return true;
}

/**
 * Offset/Address/Size: 0x39F8 | 0x8001CD34 | size: 0x14C
 */
bool cFielder::ShouldITurboWithoutBall()
{
    if (m_pMark != NULL)
    {
        if (m_pTeam->mpCurrentSituation != SITUATION_OFFENSE)
        {
            if (m_pMark->m_fDesiredSpeed > m_pTweaks->fRunningSpeed)
            {
                return true;
            }

            const nlVector3& offNet = m_pMark->GetAIOffNetLocation(NULL);

            float dx = m_pMark->m_v3Position.x - offNet.x;
            float dy = m_pMark->m_v3Position.y - offNet.y;
            float distOff = nlSqrt(dx * dx + dy * dy, true);

            const nlVector3& defNet = GetAIDefNetLocation(NULL);

            float dx2 = m_v3Position.x - defNet.x;
            float dy2 = m_v3Position.y - defNet.y;
            float distDef = nlSqrt(dx2 * dx2 + dy2 * dy2, true);

            if (distOff < distDef)
            {
                return true;
            }
        }
    }

    if (-9999.9f == m_fDistanceToDesiredPosition)
    {
        float dx = m_v3Position.x - m_v3DesiredPosition.x;
        float dy = m_v3Position.y - m_v3DesiredPosition.y;
        m_fDistanceToDesiredPosition = nlSqrt(dx * dx + dy * dy, true);
    }

    if (m_fDistanceToDesiredPosition > 10.0f)
    {
        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x3890 | 0x8001CBCC | size: 0x168
 */
void cFielder::ShouldIWave()
{
    cFielder* pOwner = g_pBall->GetOwnerFielder();
    if (pOwner == NULL)
    {
        return;
    }

    if (g_pBall->GetOwnerFielder() == this)
    {
        return;
    }

    if (!IsOnSameTeam(g_pBall->GetOwnerFielder()))
    {
        return;
    }

    if (m_nPowerupAnimID < 0 && ((cPoseNode*)m_pPowerupLayer)->GetChild(1) == NULL)
    {
        if (g_pBall->GetOwnerFielder()->DoCalcCanDoPerfectPass(this, m_v3Position))
        {
            SetPowerupAnimState(99);

            static float fTimer;
            static s8 init;

            if (!init)
            {
                fTimer = 0.0f;
                init = true;
            }

            if (FixedUpdateTask::mSimulationTime < fTimer)
            {
                fTimer = 0.0f;
            }

            if (fTimer != 0.0f)
            {
                if (FixedUpdateTask::mSimulationTime - fTimer < 3.0f)
                {
                    return;
                }
            }

            fTimer = FixedUpdateTask::mSimulationTime;
            PlayRandomCharDialogue(CHAR_DIALOGUE_WAVE, VECTORS, 100.0f, -1.0f);
        }
    }
    else
    {
        if (((cPoseNode*)m_pPowerupLayer)->GetChild(1) != NULL)
        {
            if (m_nPowerupAnimID < 0)
            {
                if (!g_pBall->GetOwnerFielder()->DoCalcCanDoPerfectPass(this, m_v3Position))
                {
                    ClearPowerupAnimState(false);
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x3668 | 0x8001C9A4 | size: 0x228
 */
void cFielder::TestCollisionForInvicibility(cFielder* pOpponent)
{
    cFielder* pReactee = NULL;
    cFielder* pAttacker = NULL;
    static ePowerUpType currPowerup = POWER_UP_STAR;
    ePowerUpType thisPowerup = m_ePowerup;

    if (thisPowerup == POWER_UP_STAR)
    {
        currPowerup = thisPowerup;
    }

    if (IsOnSameTeam(pOpponent))
    {
        return;
    }

    do
    {
        if (IsInvincible())
        {
            if (!pOpponent->IsInvincible())
            {
                pReactee = pOpponent;
                pAttacker = this;
                break;
            }
        }

        if (pOpponent->IsInvincible())
        {
            if (!IsInvincible())
            {
                pReactee = this;
                pAttacker = pOpponent;
            }
        }
    } while (false);

    if (pReactee == NULL)
        return;

    if (pReactee->IsFallenDown(0.0f))
        return;

    pReactee->InitActionSlideAttackReact(this, true);

    PowerupBase::PlayPowerupSound(currPowerup, PowerupBase::PWRUP_SOUND_HIT, m_pPhysicsCharacter, 100.0f);

    if (pAttacker->CanPickupBall(g_pBall))
    {
        pAttacker->PickupBall(g_pBall);
    }

    if (g_pGame->IsGameplayOrOvertime())
    {
        nlSingleton<StatsTracker>::Instance()->TrackStat(STATS_POWERUPS_HIT,
            pReactee->m_pTeam->m_nSide,
            pReactee->m_ID,
            0,
            0,
            0,
            0);
    }
}

/**
 * Offset/Address/Size: 0x355C | 0x8001C898 | size: 0x10C
 */
void cFielder::TestButtonsToQueueActions(float fTime)
{
    if (GetGlobalPad() == nullptr)
    {
        return;
    }
    if (m_pBall == nullptr)
    {
        return;
    }

    if (GetGlobalPad()->JustPressed(PAD_PASS, true))
    {
        m_eLastPadAction = PAD_PASS;
    }
    else if (GetGlobalPad()->JustPressed(PAD_SHOOT, true))
    {
        m_pShotMeter->Reset();
        m_pShotMeter->m_fTime = 0.0f;
        m_eLastPadAction = PAD_SHOOT;
    }
    else if (GetGlobalPad()->JustPressed(PAD_DEKE, true))
    {
        m_eLastPadAction = PAD_DEKE;
    }
    else if (m_pController->GetCStickMovementStickMagnitude() > 0.0f)
    {
        u16 direction = m_pController->GetCStickMovementStickDirection();
        s16 diff = (s16)(direction - m_aActualFacingDirection);
        if (diff < 0)
        {
            m_eLastPadAction = PAD_DEKE_RIGHT;
        }
        else
        {
            m_eLastPadAction = PAD_DEKE_LEFT;
        }
    }
}

/**
 * Offset/Address/Size: 0x346C | 0x8001C7A8 | size: 0xF0
 */
bool cFielder::TestQueuedActions()
{
    bool bResult = false;
    int eAction = m_eLastPadAction;

    if (eAction == PAD_SHOOT)
    {
        InitActionRunningWB(false);
        bResult = true;
    }
    else if (eAction == PAD_PASS)
    {
        bool bAllowLeadPass = false;
        if (GetGlobalPad() != nullptr)
        {
            GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
            if (GetGlobalPad()->GetPressure(PAD_AIM, true) > pTweaks->fLeftTriggerDownPressure)
            {
                bAllowLeadPass = true;
            }
        }

        InitActionPass(DoFindBestPassTarget(bAllowLeadPass, false), bAllowLeadPass, false);
        bResult = true;
    }
    else if (eAction >= PAD_DEKE && eAction <= PAD_DEKE_RIGHT)
    {
        InitActionDeke(m_eLastPadAction);
        bResult = true;
    }

    return bResult;
}

/**
 * Offset/Address/Size: 0x2BB0 | 0x8001BEEC | size: 0x8BC
 */
void cFielder::TestButtonsRunning()
{
    if (!m_bCanTestController)
    {
        return;
    }

    if (GetGlobalPad()->JustPressed(PAD_PASS, true))
    {
        cFielder* pPassTarget = (cFielder*)g_pBall->GetPassTargetFielder();
        bool bIsVolleyPassToMe = CanPassTargetAttemptOneTouch(pPassTarget);

        if (bIsVolleyPassToMe)
        {
            bool shouldAttempt;
            GameTweaks* tweaks;
            cFielder* pOneTouchTarget = (cFielder*)g_pBall->GetPassTargetFielder();
            shouldAttempt = false;

            cGlobalPad* pad = pOneTouchTarget->GetGlobalPad();
            if (pad != NULL)
            {
                tweaks = g_pGame->m_pGameTweaks;
                float pressure = pOneTouchTarget->GetGlobalPad()->GetPressure(PAD_AIM, true);
                if (pressure > tweaks->fLeftTriggerDownPressure)
                {
                    shouldAttempt = true;
                }
            }

            pOneTouchTarget->m_DesireReceivePassSharedVars.iAttemptOneTouchPass = shouldAttempt ? 2 : 1;
            pOneTouchTarget->m_DesireReceivePassSharedVars.pOneTouchPassTarget = pOneTouchTarget->DoFindBestPassTarget(ReceivingVolleyPass(pOneTouchTarget), shouldAttempt);
            pOneTouchTarget->m_DesireReceivePassSharedVars.iAttemptOneTouchShot = 0;
        }
        else if (CanLooseBallPass())
        {
            GameTweaks* tweaks = g_pGame->m_pGameTweaks;
            bool bAllowLeadPass = GetGlobalPad()->GetPressure(PAD_AIM, true) > tweaks->fLeftTriggerDownPressure;
            InitActionLooseBallPass(NULL, bAllowLeadPass);
        }
    }
    else if (GetGlobalPad()->JustPressed(PAD_HIT, true))
    {
        InitActionHit(DoFindBestHitTarget());
    }
    else if (GetGlobalPad()->JustPressed(PAD_SLIDE_ATTACK, true) || GetGlobalPad()->JustPressed(PAD_SHOOT, true))
    {
        cFielder* pPassTarget = (cFielder*)g_pBall->GetPassTargetFielder();
        bool bIsVolleyShotToMe = CanPassTargetAttemptOneTouch(pPassTarget);

        if (bIsVolleyShotToMe)
        {
            cFielder* pOneTouchTarget = (cFielder*)g_pBall->GetPassTargetFielder();
            bool shouldAttempt = false;

            cGlobalPad* pad = pOneTouchTarget->GetGlobalPad();
            if (pad != NULL)
            {
                GameTweaks* tweaks = g_pGame->m_pGameTweaks;
                float pressure = pOneTouchTarget->GetGlobalPad()->GetPressure(PAD_AIM, true);
                if (pressure > tweaks->fLeftTriggerDownPressure)
                {
                    shouldAttempt = true;
                }
            }

            pOneTouchTarget->m_DesireReceivePassSharedVars.iAttemptOneTouchShot = shouldAttempt ? 2 : 1;
            pOneTouchTarget->m_DesireReceivePassSharedVars.iAttemptOneTouchPass = 0;
            pOneTouchTarget->m_DesireReceivePassSharedVars.pOneTouchPassTarget = NULL;
        }
        else if (CanLooseBallShoot())
        {
            GameTweaks* tweaks = g_pGame->m_pGameTweaks;
            bool bChipShot = GetGlobalPad()->GetPressure(PAD_AIM, true) > tweaks->fLeftTriggerDownPressure;
            InitActionLooseBallShot(bChipShot);
        }
        else if (!IsOnSameTeam(g_pBall->m_pOwner))
        {
            InitActionSlideAttack(NULL, -1.0f);
        }
    }
}

/**
 * Offset/Address/Size: 0x2A6C | 0x8001BDA8 | size: 0x144
 */
void cFielder::TestButtonsRunningWB(float fTime)
{
    bool shouldAttempt = false;

    cGlobalPad* pad = GetGlobalPad();
    if (pad != NULL)
    {
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        float pressure = GetGlobalPad()->GetPressure(PAD_AIM, true);
        if (pressure > tweaks->fLeftTriggerDownPressure)
        {
            shouldAttempt = true;
        }
    }

    if (GetGlobalPad()->JustPressed(PAD_PASS, true))
    {
        InitActionPass(DoFindBestPassTarget(shouldAttempt, false), shouldAttempt, false);
    }
    else if (GetGlobalPad()->JustPressed(PAD_SHOOT, true))
    {
        m_pShotMeter->Reset();
        m_pShotMeter->m_fTime = 0.0f;
    }
    else if (GetGlobalPad()->JustPressed(PAD_DEKE, true))
    {
        InitActionDeke(PAD_DEKE);
    }
    else if (m_pController->GetCStickMovementStickMagnitude() > 0.0f)
    {
        InitActionDeke(PAD_DEKE);
    }
}

void cFielder::TestOneTimerBallContact()
{
    cBall* pBall = g_pBall;

    if (m_eActionState == ACTION_ONETIMER || m_eActionState == ACTION_LOOSE_BALL_SHOT)
    {
        if (m_pCurrentAnimController->TestTrigger(mActionOneTimerVars.fOneTimerAnimTime))
        {
            if (pBall->m_pOwner == NULL)
            {
                nlVector3 ballPos;
                nlVector3 ballPhysicsPos;
                pBall->m_pPhysicsBall->GetPosition(&ballPhysicsPos);
                ballPos = pBall->m_v3Position;

                int jointIndex = m_nBallJointIndex;
                if (TestCollision(0.18f, GetPrevJointPosition(jointIndex), GetJointPosition(jointIndex), 0.18f, ballPos, ballPhysicsPos))
                {
                    pBall->SetPosition(GetJointPosition(m_nBallJointIndex));

                    m_pShotMeter->Reset();
                    m_pShotMeter->m_fTime = 0.0f;

                    u8 wasPerfectPass = pBall->mbHyperSTS;
                    m_pShotMeter->CalcOneTimerValue(this, wasPerfectPass);

                    pBall->ClearPassTarget();

                    cNet* pOtherNet = m_pTeam->GetOtherNet();
                    float posX = m_v3Position.x;
                    float sideSign = pOtherNet->m_fDirection;
                    if (!(posX * sideSign <= 0.0f))
                    {
                        pBall->SetOwner(this);
                        pBall->ClearOwner();
                        DoRegularShooting();

                        if (mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot)
                        {
                            EmitBallShot(this, BALL_EFFECT_CHIP_SHOT, NULL, false);
                        }
                        else
                        {
                            bool bIsOneTimerShot = false;
                            if (pBall->m_tShotTimer.m_uPackedTime != 0 && pBall->m_unk_0xA4 != 0)
                            {
                                bIsOneTimerShot = true;
                            }

                            if (bIsOneTimerShot)
                            {
                                EmitBallShot(this, BALL_EFFECT_PERFECT_SHOT, NULL, false);
                                FireCameraRumbleFilter(0.0f, 0.2f);
                                Play3DSFX(Audio::eCharSFX(0x3D), VECTORS, 100.0f);
                            }
                            else
                            {
                                EmitBallShot(this, BALL_EFFECT_ONETIMER_SHOT, NULL, false);
                            }
                        }
                    }
                    else
                    {
                        DoClearBall();
                    }

                    if (FixedUpdateTask::mTimeScale < 1.0f)
                    {
                        Audio::FadeFilterFromCurrentToZero();
                    }
                }
            }
        }
    }
}

bool cFielder::ShouldHoldShotMeter()
{
    eFielderActionState eAction = m_eActionState;
    bool bHoldTime = false;
    ShotMeter* pMeter = m_pShotMeter;

    bool bCheckState;
    if (eAction == ACTION_RUNNING_WB_TURBO && m_pCurrentAnimController->m_fTime > 0.2f && m_pCurrentAnimController->m_fTime < 0.975f)
    {
        bCheckState = true;
    }
    else
    {
        bCheckState = false;
    }

    if (bCheckState || eAction == ACTION_DEKE || eAction == ACTION_SLIDE_ATTACK)
    {
        int shotState = pMeter->m_eShotMeterState;
        if (shotState == 1 || shotState == 3)
        {
            bHoldTime = true;
        }
    }

    return bHoldTime;
}

void cFielder::UpdateTimers(float fDeltaT)
{
    if (mtKickOffWaitTimer.m_uPackedTime != 0)
    {
        if (mtKickOffWaitTimer.Countdown(fDeltaT, 0.0f))
        {
            InitActionPass(DoFindBestPassTarget(false, false), false, true);
            g_pEventManager->CreateValidEvent(0xB, 0x14);
            mbCanKickoff = false;
        }
    }

    bool bIsGameplay = g_pGame->IsGameplayOrOvertime();

    if (bIsGameplay)
    {
        m_DesireCommonVars.tAge.Countup(fDeltaT, 10.0f);
        m_DesireCommonVars.tMiscTimer.Countdown(fDeltaT, 0.0f);
        m_tDesireDuration.Countdown(fDeltaT, 0.0f);

        if (m_tFrozenTimer.m_uPackedTime != 0)
        {
            if (m_tFrozenTimer.Countdown(fDeltaT, 0.0f))
            {
                EmitUnFreeze(this);
            }
        }

        if (m_tPowerupEffectTime.m_uPackedTime != 0)
        {
            if (m_tPowerupEffectTime.Countdown(fDeltaT, 0.0f))
            {
                switch (m_ePowerup)
                {
                case POWER_UP_STAR:
                    KillStar(this);
                    m_ePowerup = POWER_UP_NONE;
                    mnNumPowerups = 0;
                    m_tPowerupEffectTime.m_uPackedTime = 0;
                    break;
                case POWER_UP_MUSHROOM:
                    KillMushroom(this);
                    m_ePowerup = POWER_UP_NONE;
                    mnNumPowerups = 0;
                    m_tPowerupEffectTime.m_uPackedTime = 0;
                    break;
                default:
                    break;
                }
            }
        }

        if (mtBombImpactTime.m_uPackedTime != 0)
        {
            if (mtBombImpactTime.Countdown(fDeltaT, 0.0f))
            {
                float fBombRadius = mfBombImpactRadius;
                if (m_eActionState != ACTION_POST_WHISTLE)
                {
                    bool bSkip = (m_ePowerup == POWER_UP_STAR && m_tPowerupEffectTime.m_uPackedTime != 0) || mActionShootToScoreVars.isCurrentlyInvincible;
                    if (!bSkip)
                    {
                        AddRandomDirt();
                        if (g_pBall->m_pPassTarget != NULL && g_pBall->m_pPassTarget == this)
                        {
                            g_pBall->ClearPassTarget();
                        }
                        InitActionBombReact(mv3BombImpactLocation, fBombRadius);
                    }
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x2A34 | 0x8001BD70 | size: 0x38
 */
void cFielder::PreUpdate(float fTime)
{
    cPlayer::PreUpdate(fTime);
    m_bHasBeenUpdated = false;
    mbWasHitByPowerupThisFrame = false;
}

/**
 * Offset/Address/Size: 0x27EC | 0x8001BB28 | size: 0x248
 */
void cFielder::PostPhysicsUpdate()
{
    cPlayer::PostPhysicsUpdate();

    if (port_region() == PORT_REGION_JAPAN && m_tFrozenTimer.m_uPackedTime != 0)
    {
        return;
    }

    if (m_eActionState == ACTION_ONETIMER || m_eActionState == ACTION_LOOSE_BALL_SHOT)
    {
        if (m_pCurrentAnimController->TestTrigger(mActionOneTimerVars.fOneTimerAnimTime))
        {
            if (g_pBall->m_pOwner == NULL)
            {
                nlVector3 ballPos;
                nlVector3 ballPhysicsPos;
                g_pBall->m_pPhysicsBall->GetPosition(&ballPhysicsPos);
                ballPos = g_pBall->m_v3Position;

                nlVector3* pJointPos;
                int jointIndex;
                jointIndex = m_nBallJointIndex;
                if (TestCollision(0.18f, GetPrevJointPosition(jointIndex), *(pJointPos = &GetJointPosition(jointIndex)), 0.18f, ballPos, ballPhysicsPos))
                {
                    g_pBall->SetPosition(GetJointPosition(m_nBallJointIndex));

                    m_pShotMeter->Reset();
                    m_pShotMeter->m_fTime = 0.0f;

                    u8 wasPerfectPass = g_pBall->mbHyperSTS;
                    m_pShotMeter->CalcOneTimerValue(this, wasPerfectPass);

                    g_pBall->ClearPassTarget();

                    cNet* pOtherNet = m_pTeam->GetOtherNet();
                    float posX = m_v3Position.x;
                    float sideSign = pOtherNet->m_fDirection;
                    if (!(posX * sideSign <= 0.0f))
                    {
                        g_pBall->SetOwner(this);
                        g_pBall->ClearOwner();
                        DoRegularShooting();

                        bool bIsChipShot = false;
                        if (mActionShotVars.bIsChipShot || mActionLooseBallShotVars.bIsChipShot)
                        {
                            bIsChipShot = true;
                        }

                        if (bIsChipShot)
                        {
                            EmitBallShot(this, BALL_EFFECT_CHIP_SHOT, NULL, false);
                        }
                        else
                        {
                            bool bIsOneTimerShot = false;
                            if (g_pBall->m_tShotTimer.m_uPackedTime != 0 && g_pBall->m_unk_0xA4 != 0)
                            {
                                bIsOneTimerShot = true;
                            }

                            if (bIsOneTimerShot)
                            {
                                EmitBallShot(this, BALL_EFFECT_PERFECT_SHOT, NULL, false);
                                FireCameraRumbleFilter(0.0f, 0.2f);
                                Play3DSFX(Audio::eCharSFX(0x3D), VECTORS, 100.0f);
                            }
                            else
                            {
                                EmitBallShot(this, BALL_EFFECT_ONETIMER_SHOT, NULL, false);
                            }
                        }
                    }
                    else
                    {
                        DoClearBall();
                    }

                    if (FixedUpdateTask::mTimeScale < 1.0f)
                    {
                        Audio::FadeFilterFromCurrentToZero();
                    }
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x234C | 0x8001B688 | size: 0x4A0
 */
void cFielder::Update(float fDeltaT)
{
    if (mtKickOffWaitTimer.m_uPackedTime != 0)
    {
        if (mtKickOffWaitTimer.Countdown(fDeltaT, 0.0f))
        {
            InitActionPass(DoFindBestPassTarget(false, false), false, true);
            g_pEventManager->CreateValidEvent(0xB, 0x14);
            mbCanKickoff = false;
        }
    }

    bool bIsGameplay = g_pGame->IsGameplayOrOvertime();

    if (bIsGameplay)
    {
        m_DesireCommonVars.tAge.Countup(fDeltaT, 10.0f);
        m_DesireCommonVars.tMiscTimer.Countdown(fDeltaT, 0.0f);
        m_tDesireDuration.Countdown(fDeltaT, 0.0f);

        if (m_tFrozenTimer.m_uPackedTime != 0)
        {
            if (m_tFrozenTimer.Countdown(fDeltaT, 0.0f))
            {
                EmitUnFreeze(this);
            }
        }

        if (m_tPowerupEffectTime.m_uPackedTime != 0)
        {
            if (m_tPowerupEffectTime.Countdown(fDeltaT, 0.0f))
            {
                switch (m_ePowerup)
                {
                case POWER_UP_STAR:
                    KillStar(this);
                    m_ePowerup = POWER_UP_NONE;
                    mnNumPowerups = 0;
                    m_tPowerupEffectTime.m_uPackedTime = 0;
                    break;
                case POWER_UP_MUSHROOM:
                    KillMushroom(this);
                    m_ePowerup = POWER_UP_NONE;
                    mnNumPowerups = 0;
                    m_tPowerupEffectTime.m_uPackedTime = 0;
                    break;
                default:
                    break;
                }
            }
        }

        if (mtBombImpactTime.m_uPackedTime != 0)
        {
            if (mtBombImpactTime.Countdown(fDeltaT, 0.0f))
            {
                float fBombRadius = mfBombImpactRadius;
                if (m_eActionState != ACTION_POST_WHISTLE)
                {
                    bool bSkip = (m_ePowerup == POWER_UP_STAR && m_tPowerupEffectTime.m_uPackedTime != 0) || mActionShootToScoreVars.isCurrentlyInvincible;
                    if (!bSkip)
                    {
                        AddRandomDirt();
                        if (g_pBall->m_pPassTarget != NULL && g_pBall->m_pPassTarget == this)
                        {
                            g_pBall->ClearPassTarget();
                        }
                        InitActionBombReact(mv3BombImpactLocation, fBombRadius);
                    }
                }
            }
        }
    }

    cPlayer::Update(fDeltaT);

    m_fDistanceToDesiredPosition = -9999.9f;
    m_v3AccumDesiredPos.x = 0.0f;
    m_v3AccumDesiredPos.y = 0.0f;
    m_v3AccumDesiredPos.z = 0.0f;
    m_fAccumDesiredPosWeight = 0.0f;

    if (m_tFrozenTimer.m_uPackedTime == 0)
    {
        eFielderActionState eAction = m_eActionState;
        bool bHoldTime = false;
        ShotMeter* pMeter = m_pShotMeter;

        bool bCheckState;
        if (eAction == ACTION_RUNNING_WB_TURBO && m_pCurrentAnimController->m_fTime > 0.2f && m_pCurrentAnimController->m_fTime < 0.975f)
        {
            bCheckState = true;
        }
        else
        {
            bCheckState = false;
        }

        if (bCheckState || eAction == ACTION_DEKE || eAction == ACTION_SLIDE_ATTACK)
        {
            int shotState = pMeter->m_eShotMeterState;
            if (shotState == 1 || shotState == 3)
            {
                bHoldTime = true;
            }
        }

        pMeter->Update(fDeltaT, bHoldTime);
    }

    if (m_tFrozenTimer.m_uPackedTime == 0)
    {
        bool bNeedDesire;
        if (m_eFielderDesireState == FIELDERDESIRE_NEED_DESIRE || m_tDesireDuration.m_uPackedTime == 0)
        {
            bNeedDesire = true;
        }
        else
        {
            bNeedDesire = false;
        }
        if (bNeedDesire)
        {
            CalculateNewDesire();
        }

        UpdateDesireState(fDeltaT);
        m_pAvoidance->Update(fDeltaT);
        UpdateActionState(fDeltaT);
        UpdateHeadTracking(fDeltaT);
        cCharacter::Update(fDeltaT);
    }

    UpdateMovementLoopSFX();

    UpdateController(fDeltaT);
    m_bHasBeenUpdated = true;
    RandomChance(0.0f);
}

/**
 * Offset/Address/Size: 0x21D8 | 0x8001B514 | size: 0x174
 */
void cFielder::ThrowPowerup()
{
    cFielder* pTarget;

    if (!g_pGame->mbCaptainShotToScoreOn)
    {
        switch (m_ePowerup)
        {
        case POWER_UP_STAR:
            m_tPowerupEffectTime.SetSeconds(g_pGame->m_pGameTweaks->fStarEffectTime);
            EmitStar(this);
            break;
        case POWER_UP_MUSHROOM:
        {
            f32 fMushroomTime = g_pGame->m_pGameTweaks->fMushroomEffectTime;
            switch (m_eCharacterClass)
            {
            case LUIGI:
            case MARIO:
                fMushroomTime *= 1.33f;
                break;
            default:
                break;
            }
            m_tPowerupEffectTime.SetSeconds(fMushroomTime);
            EmitMushroom(this);
            InitBlur(0);
            break;
        }
        case POWER_UP_GREEN_SHELL:
        case POWER_UP_RED_SHELL:
        case POWER_UP_SPINY_SHELL:
        case POWER_UP_FREEZE_SHELL:
        case POWER_UP_BANANA:
        case POWER_UP_BOBOMB:
            m_pPowerupTarget = FindPowerupTarget(this, NULL);
            PowerupCreateAndThrow(this, m_ePowerup, mnNumPowerups, NULL);
            break;
        case POWER_UP_CHAIN_CHOMP:
        {
            pTarget = m_pPowerupTarget;
            BasicStadium* pStadium = BasicStadium::GetCurrentStadium();
            pStadium->mpNPCManager->mpChainChomp->Fall(this, pTarget);
            break;
        }
        case POWER_UP_NONE:
            return;
        }

        if (g_pGame->IsGameplayOrOvertime())
        {
            nlSingleton<StatsTracker>::Instance()->TrackStat(STATS_POWERUPS_USED, m_pTeam->m_nSide, m_ID, 0, 0, 0, 0);
        }
        m_pPowerupTarget = NULL;
    }
}

/**
 * Offset/Address/Size: 0x1D18 | 0x8001B054 | size: 0x4C0
 */
void cFielder::SetPowerup(ePowerUpType eNewPowerup, int nnumOfPowerups, cFielder* pTarget)
{
    switch (m_ePowerup)
    {
    case POWER_UP_STAR:
        KillStar(this);
        m_ePowerup = POWER_UP_NONE;
        mnNumPowerups = 0;
        m_tPowerupEffectTime.m_uPackedTime = 0;
        break;
    case POWER_UP_MUSHROOM:
        KillMushroom(this);
        m_ePowerup = POWER_UP_NONE;
        mnNumPowerups = 0;
        m_tPowerupEffectTime.m_uPackedTime = 0;
        break;
    }

    switch (eNewPowerup)
    {
    case POWER_UP_GREEN_SHELL:
    case POWER_UP_RED_SHELL:
    case POWER_UP_SPINY_SHELL:
    case POWER_UP_FREEZE_SHELL:
    case POWER_UP_BANANA:
    case POWER_UP_BOBOMB:
    case POWER_UP_CHAIN_CHOMP:
        if (pTarget == NULL)
        {
            pTarget = FindPowerupTarget(this, NULL);
        }
        break;
    case POWER_UP_MUSHROOM:
    case POWER_UP_STAR:
    case POWER_UP_NONE:
    default:
        pTarget = NULL;
        break;
    }

    m_ePowerup = eNewPowerup;
    mnNumPowerups = nnumOfPowerups;
    m_pPowerupTarget = pTarget;

    if (m_ePowerup == POWER_UP_NONE)
        return;

    int dir = 0;
    if (m_pPowerupTarget != NULL)
    {
        dir = (GetFacingDeltaToPosition(m_pPowerupTarget->m_v3Position) >> 14) & 3;
    }

    static int ThrowAnims[] = { 95, 98, 97, 96 };
    SetPowerupAnimState(ThrowAnims[dir]);
    m_nPowerupAnimID = ThrowAnims[dir];

    if (m_eActionState == ACTION_SLIDE_ATTACK)
    {
        mActionSlideAttackVars.bWasStarMushroomUsedDuring = true;
    }

    if (m_tFrozenTimer.m_uPackedTime != 0)
        return;

    switch (GetPowerupType())
    {
    case POWER_UP_CHAIN_CHOMP:
        ThrowPowerup();
        if (g_pGame->IsGameplayOrOvertime())
        {
            nlSingleton<StatsTracker>::Instance()->TrackStat(STATS_POWERUPS_USED, m_pTeam->m_nSide, m_ID, 0, 0, 0, 0);
        }
        m_pPowerupTarget = NULL;
        ClearPowerupAnimState(false);
        m_ePowerup = POWER_UP_NONE;
        return;
    case POWER_UP_MUSHROOM:
    case POWER_UP_STAR:
        ThrowPowerup();
        if (g_pGame->IsGameplayOrOvertime())
        {
            nlSingleton<StatsTracker>::Instance()->TrackStat(STATS_POWERUPS_USED, m_pTeam->m_nSide, m_ID, 0, 0, 0, 0);
        }
        m_pPowerupTarget = NULL;
        ClearPowerupAnimState(false);
        break;
    }
}

/**
 * Offset/Address/Size: 0x1BE4 | 0x8001AF20 | size: 0x134
 */
void cFielder::UseTeamPowerup(cFielder* pTarget)
{
    if ((m_ePowerup == POWER_UP_STAR) || (m_tFrozenTimer.m_uPackedTime != 0))
    {
        return;
    }

    if (IsFallenDown(0.0f))
    {
        if (GetTeam()->IsCurrentStar())
        {
            return;
        }
        if (GetTeam()->IsCurrentMushroom())
        {
            return;
        }
    }

    if (!GetTeam()->IsCurrentNoPowerup())
    {
        SetPowerup(
            GetTeam()->GetCurrentPowerUp().eType,
            GetTeam()->GetCurrentPowerUp().nnumOfPowerups,
            pTarget);
        GetTeam()->ClearCurrentPowerUp();
    }
    else
    {
        if (GetTeam()->GetPowerUpByIndex(1).eType == POWER_UP_NONE)
        {
            return;
        }
        GetTeam()->TogglePowerup(true);
        SetPowerup(
            GetTeam()->GetCurrentPowerUp().eType,
            GetTeam()->GetCurrentPowerUp().nnumOfPowerups,
            pTarget);
        GetTeam()->ClearCurrentPowerUp();
    }
}

/**
 * Offset/Address/Size: 0x1AB4 | 0x8001ADF0 | size: 0x130
 */
void cFielder::UpdateActionState(float dt)
{
    switch (m_eActionState)
    {
    case ACTION_DEKE:
        ActionDeke(dt);
        break;
    case ACTION_ELECTROCUTION:
        ActionElectrocution(dt);
        break;
    case ACTION_HIT:
        ActionHit(dt);
        break;
    case ACTION_HIT_REACT:
        ActionHitReact(dt);
        break;
    case ACTION_LATE_ONETIMER_FROM_VOLLEY:
        ActionLateOneTimerFromVolley(dt);
        break;
    case ACTION_IDLE_TURN:
        ActionIdleTurn(dt);
        break;
    case ACTION_LOOSE_BALL_PASS:
        ActionLooseBallPass(dt);
        break;
    case ACTION_LOOSE_BALL_SHOT:
        ActionLooseBallShot(dt);
        break;
    case ACTION_ONETIMER:
        ActionOneTimer(dt);
        break;
    case ACTION_ONETOUCH_PASS_FROM_VOLLEY:
        ActionOneTouchPassFromVolley(dt);
        break;
    case ACTION_PASS:
        ActionPass(dt);
        break;
    case ACTION_POST_WHISTLE:
        ActionPostWhistle(dt);
        break;
    case ACTION_RECEIVE_PASS:
        ActionReceivePass(dt);
        break;
    case ACTION_RUNNING:
        ActionRunning(dt);
        break;
    case ACTION_RUNNING_WB:
        ActionRunningWB(dt);
        break;
    case ACTION_RUNNING_WB_TURBO:
        ActionRunningWBTurbo(dt);
        break;
    case ACTION_RUNNING_WB_TURBO_TURN:
        ActionRunningWBTurboTurn(dt);
        break;
    case ACTION_SHOT:
        ActionShot(dt);
        break;
    case ACTION_SHOOT_TO_SCORE:
        ActionShootToScore(dt);
        break;
    case ACTION_SLIDE_ATTACK:
        ActionSlideAttack(dt);
        break;
    case ACTION_SLIDE_ATTACK_REACT:
        ActionSlideAttackReact(dt);
        break;
    case ACTION_BOMB_REACT:
        ActionBombReact(dt);
        break;
    case ACTION_BANANA_REACT:
        ActionBananaReact(dt);
        break;
    case ACTION_SHELL_REACT:
        ActionShellReact(dt);
        break;
    case ACTION_STS_HIT_REACT:
        ActionSTSHitReact(dt);
        break;
    case ACTION_SQUISH_REACT:
        ActionSquishReact(dt);
        break;
    case ACTION_SLIDE_FAIL_REACT:
        ActionSlideAttackFailReact(dt);
        break;
    case ACTION_WAIT:
        ActionWait(dt);
        break;
    }
    DoHandleActiveShotMeter();
}

/**
 * Offset/Address/Size: 0x17CC | 0x8001AB08 | size: 0x2E8
 */
void cFielder::UpdateHeadTracking(float fDeltaT)
{
    switch (m_eActionState)
    {
    case ACTION_NEED_ACTION:
    case ACTION_DEKE:
    case ACTION_ELECTROCUTION:
    case ACTION_HIT:
    case ACTION_HIT_REACT:
    case ACTION_IDLE_TURN:
    case ACTION_LATE_ONETIMER_FROM_VOLLEY:
    case ACTION_ONETOUCH_PASS_FROM_VOLLEY:
    case ACTION_RUNNING_WB_TURBO:
    case ACTION_RUNNING_WB_TURBO_TURN:
    case ACTION_SHOT:
    case ACTION_SHOOT_TO_SCORE:
    case ACTION_SLIDE_ATTACK_REACT:
    case ACTION_BOMB_REACT:
    case ACTION_SHELL_REACT:
    case ACTION_BANANA_REACT:
    case ACTION_STS_HIT_REACT:
    case ACTION_SQUISH_REACT:
    case ACTION_SLIDE_FAIL_REACT:
        m_pHeadTrack->m_bTrackOOI = false;
        break;

    case ACTION_ONETIMER:
        switch (m_eAnimID)
        {
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F:
            m_pHeadTrack->m_bTrackOOI = false;
            return;
        default:
            break;
        }

        if (m_pCurrentAnimController->m_fTime > mActionOneTimerVars.fOneTimerAnimTime)
        {
            m_pHeadTrack->m_bTrackOOI = false;
        }
        else
        {
            m_pHeadTrack->m_v3OOI = g_pBall->m_v3Position;
            m_pHeadTrack->m_bTrackOOI = true;
        }
        break;

    case ACTION_LOOSE_BALL_PASS:
    case ACTION_LOOSE_BALL_SHOT:
        if (m_pCurrentAnimController->m_fTime > mActionOneTimerVars.fOneTimerAnimTime)
        {
            m_pHeadTrack->m_bTrackOOI = false;
        }
        else
        {
            m_pHeadTrack->m_v3OOI = g_pBall->m_v3Position;
            m_pHeadTrack->m_bTrackOOI = true;
        }
        break;

    case ACTION_PASS:
    case ACTION_RECEIVE_PASS:
    case ACTION_SLIDE_ATTACK:
        if (m_pBall == nullptr)
        {
            if (m_eAnimID != 0x35)
            {
                m_pHeadTrack->m_v3OOI = g_pBall->m_v3Position;
                m_pHeadTrack->m_bTrackOOI = true;
                break;
            }
        }

        m_pHeadTrack->m_bTrackOOI = false;
        break;

    case ACTION_RUNNING_WB:
        if (!BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->IsHidden())
        {
            m_pHeadTrack->m_v3OOI = BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->mv3Position;
            m_pHeadTrack->m_bTrackOOI = true;
        }
        else
        {
            m_pHeadTrack->m_bTrackOOI = false;
        }
        break;

    case ACTION_RUNNING:
        if (!BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->IsHidden())
        {
            m_pHeadTrack->m_v3OOI = BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->mv3Position;
        }
        else
        {
            m_pHeadTrack->m_v3OOI = g_pBall->m_v3Position;
        }
        m_pHeadTrack->m_bTrackOOI = true;
        break;

    case ACTION_POST_WHISTLE:
    {
        cPlayer* pScorer = g_pGame->m_pScorer;
        if (pScorer != nullptr)
        {
            m_pHeadTrack->m_v3OOI = pScorer->m_v3Position;
        }
        else
        {
            m_pHeadTrack->m_v3OOI = g_pBall->m_v3Position;
        }
        m_pHeadTrack->m_bTrackOOI = true;
        break;
    }

    case ACTION_WAIT:
        m_pHeadTrack->m_v3OOI = g_pBall->m_v3Position;
        m_pHeadTrack->m_bTrackOOI = true;
        break;
    }
}

/**
 * Offset/Address/Size: 0x13C4 | 0x8001A700 | size: 0x408
 */
void cFielder::UpdateController(float fDeltaT)
{
    extern cTeam* g_pCurrentlyUpdatingTeam;

    int nNumUsers;
    int i;
    float yPercentage;
    float xPercentage;

    switch (m_eActionState)
    {
    case ACTION_NEED_ACTION:
    case ACTION_PASS:
    case ACTION_POST_WHISTLE:
        if (m_tFrozenTimer.m_uPackedTime != 0)
        {
            if (GetGlobalPad() != NULL)
            {
                if (GetGlobalPad()->JustPressed(PAD_SWITCH, true))
                {
                    if (m_pBall == NULL)
                    {
                        SwapController();
                    }
                }
            }
        }
        break;

    case ACTION_RUNNING_WB:
    case ACTION_RUNNING_WB_TURBO:
    case ACTION_RUNNING_WB_TURBO_TURN:
    case ACTION_SHOOT_TO_SCORE:
        if (GetGlobalPad() != NULL)
        {
            if (GetGlobalPad()->JustPressed(PAD_SWITCH, true))
            {
                if (m_tFrozenTimer.m_uPackedTime != 0)
                {
                    if (m_pBall == NULL)
                    {
                        SwapController();
                        break;
                    }
                }

                if (m_pBall == NULL)
                {
                    SwapController();
                }
            }
        }
        break;

    case ACTION_DEKE:
    case ACTION_ELECTROCUTION:
    case ACTION_HIT:
    case ACTION_HIT_REACT:
    case ACTION_IDLE_TURN:
    case ACTION_LATE_ONETIMER_FROM_VOLLEY:
    case ACTION_LOOSE_BALL_PASS:
    case ACTION_LOOSE_BALL_SHOT:
    case ACTION_ONETIMER:
    case ACTION_ONETOUCH_PASS_FROM_VOLLEY:
    case ACTION_RECEIVE_PASS:
    case ACTION_RUNNING:
    case ACTION_SHOT:
    case ACTION_SLIDE_ATTACK:
    case ACTION_SLIDE_ATTACK_REACT:
    case ACTION_BOMB_REACT:
    case ACTION_SHELL_REACT:
    case ACTION_BANANA_REACT:
    case ACTION_STS_HIT_REACT:
    case ACTION_SQUISH_REACT:
    case ACTION_SLIDE_FAIL_REACT:
    case ACTION_WAIT:
        if (GetGlobalPad() != NULL)
        {
            if (GetGlobalPad()->JustPressed(PAD_SWITCH, true))
            {
                if (m_pBall == NULL)
                {
                    SwapController();
                }
            }
        }
        break;
    }

    if ((g_pBall->GetOwnerFielder() != NULL)
        && !IsOnSameTeam(g_pBall->GetOwnerFielder())
        && (g_pBall->GetOwnerFielder()->m_eActionState == ACTION_SHOOT_TO_SCORE))
    {
        if (ShootToScoreMeter::instance.m_bMeterVisible != 0)
        {
            nNumUsers = 0;
            i = 0;
            while (i < 4)
            {
                if (m_pTeam->GetFielder(i)->GetGlobalPad() != NULL)
                {
                    nNumUsers++;
                }
                i++;
            }

            if (GetGlobalPad() == NULL)
            {
                const GameplaySettings& gameplayOptions = nlSingleton<GameInfoManager>::Instance()->GetGameplayOptions();
                switch (gameplayOptions.SkillLevel)
                {
                case GameplaySettings::TRAINING:
                case GameplaySettings::ROOKIE:
                case GameplaySettings::PROFESSIONAL:
                    break;
                case GameplaySettings::SUPERSTAR:
                case GameplaySettings::LEGEND:
                    if (nNumUsers == 0)
                    {
                        SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                        if (nlRandomf(1.0f, &nlDefaultSeed) < pSkillTweaks->Shoot_CaptainS2SSecondButtonChance)
                        {
                            EmitSolidRumble(g_pBall->GetOwnerFielder());

                            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                            yPercentage = (float)nlRandom(2, &nlDefaultSeed) - 1.0f;
                            ShootToScoreMeter::instance.RumbleMeter(
                                pSkillTweaks->Shoot_CaptainS2SSecondButtonChance,
                                (float)nlRandom(2, &nlDefaultSeed) - 1.0f,
                                yPercentage);
                        }
                    }
                    break;
                }
            }
            else if (m_pController->GetCStickMovementStickMagnitude() > 0.0f)
            {
                mActionRumbleVars.fRumbleDirection = (float)(u16)m_pController->GetCStickMovementStickDirection();

                EmitSolidRumble(g_pBall->GetOwnerFielder());
                EmitSolidRumble(this);

                if (mActionRumbleVars.fRumbleDirection < 32768.0f)
                {
                    yPercentage = InterpolateRangeClamped(-1.0f, 1.0f, 32768.0f, 0.0f, mActionRumbleVars.fRumbleDirection);
                }
                else
                {
                    yPercentage = InterpolateRangeClamped(-1.0f, 1.0f, 32768.0f, 65000.0f, mActionRumbleVars.fRumbleDirection);
                }

                if ((mActionRumbleVars.fRumbleDirection > 16384.0f)
                    && (mActionRumbleVars.fRumbleDirection < 49152.0f))
                {
                    xPercentage = InterpolateRangeClamped(-1.0f, 1.0f, 49152.0f, 16384.0f, mActionRumbleVars.fRumbleDirection);
                }
                else if (mActionRumbleVars.fRumbleDirection <= 16384.0f)
                {
                    xPercentage = InterpolateRangeClamped(1.0f, 0.0f, 16384.0f, 0.0f, mActionRumbleVars.fRumbleDirection);
                }
                else
                {
                    xPercentage = InterpolateRangeClamped(-1.0f, 0.0f, 49152.0f, 65000.0f, mActionRumbleVars.fRumbleDirection);
                }

                ShootToScoreMeter::instance.RumbleMeter(
                    m_pController->GetCStickMovementStickMagnitude(),
                    yPercentage,
                    xPercentage);
            }
        }
    }
    else
    {
        mActionRumbleVars.fRumbleDirection = 0.0f;
    }
}

void cFielder::UpdateMovementLoopSFX()
{
    eFielderActionState eAction = m_eActionState;
    Audio::cCharacterSFX* pSFX = m_pCharacterSFX;
    bool bIsActive = false;

    if (((int)eAction == ACTION_RUNNING_WB) || ((int)eAction == ACTION_RUNNING_WB_TURBO) || ((int)eAction == ACTION_RUNNING_WB_TURBO_TURN))
    {
        bIsActive = true;
    }

    if (bIsActive || eAction == ACTION_DEKE || (eAction == ACTION_NEED_ACTION && GetGlobalPad() != NULL))
    {
        if (m_fActualSpeed >= 4.5f && m_fDesiredSpeed > 0.0f && !g_pGame->mbCaptainShotToScoreOn)
        {
            if (!pSFX->IsMovementLoopStarted() || (pSFX->IsMovementLoopStarted() && !pSFX->IsMovementLoopPlaying()))
            {
                pSFX->StartMovementLoop();
            }
        }
        else
        {
            if (pSFX->IsMovementLoopStarted() && m_fDesiredSpeed < 0.001f && m_fActualSpeed < 0.5f && m_eActionState != ACTION_RUNNING_WB_TURBO_TURN)
            {
                pSFX->StopMovementLoop();
            }
        }
    }
    else
    {
        pSFX->StopMovementLoop();
    }
}

/**
 * Offset/Address/Size: 0x13A0 | 0x8001A6DC | size: 0x24
 */
void cFielder::UpdatePlay(float fTime)
{
    m_pCurrentPlay->Update(fTime);
}

nlVector3 cFielder::GetAIDesiredPosition()
{
    nlVector3 v3DesiredAIPos;
    nlVec3Scale(v3DesiredAIPos, m_v3AccumDesiredPos, 1.0f / m_fAccumDesiredPosWeight);
    return v3DesiredAIPos;
}

/**
 * Offset/Address/Size: 0x1338 | 0x8001A674 | size: 0x68
 */
float cFielder::GetDistanceToDesiredPos()
{
    if (m_fDistanceToDesiredPosition == -9999.9f)
    {
        float dx = m_v3Position.x - m_v3DesiredPosition.x;
        float dy = m_v3Position.y - m_v3DesiredPosition.y;
        float distanceSquared = dx * dx + dy * dy;
        m_fDistanceToDesiredPosition = nlSqrt(distanceSquared, true);
    }
    return m_fDistanceToDesiredPosition;
}

/**
 * Offset/Address/Size: 0x1224 | 0x8001A560 | size: 0x114
 */
bool cFielder::S2SShootWasPressed()
{
    bool result;
    bool padResult;

    result = false;

    if (m_eActionState == ACTION_SHOOT_TO_SCORE)
    {

        if (GetGlobalPad() != NULL)
        {
            padResult = false;
            if (GetGlobalPad()->JustPressed(PAD_SHOOT, true) || GetGlobalPad()->JustPressed(PAD_PASS, true))
            {
                padResult = true;
            }
            result = padResult;
        }
        else
        {
            if (mActionShootToScoreVars.fFrameButtonDownTime1 < 0.0f)
            {
                result = (mActionShootToScoreVars.fMeterFractionTime >= (m_DesireCommonVars.fMisc - 0.009f));
            }
            else
            {
                result = (mActionShootToScoreVars.fMeterFractionTime <= -(m_DesireCommonVars.fMisc - 0.009f));
            }
            mActionShootToScoreVars.bShootWasPressed = result;
        }
    }

    if (mActionShootToScoreVars.fFrameButtonDownTime2 >= 0.0f)
    {
        mActionShootToScoreVars.bShootWasPressed = false;
        result = false;
    }

    return result;
}

/**
 * Offset/Address/Size: 0x11AC | 0x8001A4E8 | size: 0x78
 */
void cFielder::StartRunning()
{
    if (!IsRunning())
    {
        if (m_pBall != 0)
        {
            InitActionRunningWB(false);
            return;
        }
        InitActionRunning();
    }
}

/**
 * Offset/Address/Size: 0xC14 | 0x80019F50 | size: 0x598
 */
bool cFielder::DoAILooseBallActionSelection()
{
    extern cFielder* g_pScriptCurrentFielder;
    extern cTeam* g_pCurrentlyUpdatingTeam;
    extern cBall* g_pBall;

    eFielderDesireState action;
    cPlayer* pTarget;
    bool bDidSomething = false;

    FuzzyVariant looseBallAction = Fuzzy::GetBestLooseBallAction(this);

    action = (eFielderDesireState)looseBallAction.mData.i;

    static FilteredRandomChance randchancegen;

    bool bSelectChance = randchancegen.genrand(looseBallAction.SelectionChance);
    float fActionScore = looseBallAction.Confidence;
    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    float fPerturbPercent = 0.5f * (1.0f - pSkillTweaks->Off_Reaction);

    if (looseBallAction.Confidence > 0.0f && bSelectChance)
    {
        switch (action)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 16:
        case 17:
            break;
        case FIELDERDESIRE_SLIDE_ATTACK:
        {
            float fReactionRandom = 0.5f * fPerturbPercent;
            if (!(fActionScore >= 0.5f + (nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom)))
                break;
            float fTime;
            if (!CanISlideAttack(g_pBall->GetPosition(), g_pBall->GetVelocity(), &fTime))
                break;
            InitActionSlideAttack(NULL, fTime);
            m_eDesireSubState = 1;
            bDidSomething = true;
            break;
        }
        case FIELDERDESIRE_SHOOT:
        {
            float fReactionRandom = 0.5f * fPerturbPercent;
            if (!(fActionScore >= 0.5f + (nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom)))
                break;
            InitActionLooseBallShot(looseBallAction.ExtraData.mData.b);
            bDidSomething = true;
            break;
        }
        case FIELDERDESIRE_PASS:
        {
            float fReactionRandom = 0.5f * fPerturbPercent;
            if (!(fActionScore >= 0.5f + (nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom)))
                break;
            cFielder* pTarget = (cFielder*)looseBallAction.ExtraData.mData.pPlayer;
            InitActionLooseBallPass(pTarget, OpenTo(g_pScriptCurrentFielder, pTarget) < 0.5f);
            bDidSomething = true;
            break;
        }
        case FIELDERDESIRE_HIT:
        {
            float fReactionRandom = 0.59f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore > 0.59f + fReactionOffset))
                break;
            pTarget = looseBallAction.ExtraData.mData.pPlayer;
            InitDesire(FIELDERDESIRE_HIT, looseBallAction.Confidence, -1.0f, FuzzyVariant(pTarget), fvNotSet);
            bDidSomething = true;
            break;
        }
        case FIELDERDESIRE_USE_POWERUP:
        {
            ePowerUpType powerupType = (ePowerUpType)looseBallAction.ExtraData.mData.i;
            float fReactionRandom = 0.6f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.6f + fReactionOffset))
                break;
            if (powerupType != m_pTeam->GetCurrentPowerUp().eType)
                break;
            if (m_nPowerupAnimID >= 0)
                break;
            UseTeamPowerup(NULL);
            break;
        }
        default:
            break;
        }
    }

    return bDidSomething;
}

/**
 * Offset/Address/Size: 0x6BC | 0x800199F8 | size: 0x558
 */
bool cFielder::DoAIReceivePassActionSelection()
{
    extern cFielder* g_pScriptCurrentFielder;
    extern cTeam* g_pCurrentlyUpdatingTeam;

    eFielderDesireState action;
    bool bDidSomething = false;

    FuzzyVariant looseBallAction = Fuzzy::GetBestPassReceiveAction(this);

    action = (eFielderDesireState)looseBallAction.mData.i;

    static FilteredRandomChance randchancegen;

    bool bSelectChance = randchancegen.genrand(looseBallAction.SelectionChance);
    float fActionScore = looseBallAction.Confidence;
    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    float fPerturbPercent = 0.5f * (1.0f - pSkillTweaks->Off_Reaction);

    if (looseBallAction.Confidence > 0.0f && bSelectChance)
    {
        switch (action)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 14:
        case 15:
        case 16:
        case 17:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
            break;
        case FIELDERDESIRE_ONETIMER:
        {
            float fReactionRandom = 0.5f * fPerturbPercent;
            if (!(fActionScore >= 0.5f + (nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom)))
                break;
            m_DesireReceivePassSharedVars.iAttemptOneTouchShot = looseBallAction.ExtraData.mData.b ? 2 : 1;
            bDidSomething = true;
            break;
        }
        case FIELDERDESIRE_PASS:
        {
            cPlayer* pTarget = looseBallAction.ExtraData.mData.pPlayer;
            float fReactionRandom = 0.6f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.6f + fReactionOffset))
                break;
            m_DesireReceivePassSharedVars.iAttemptOneTouchPass = (OpenTo(g_pScriptCurrentFielder, pTarget) < 0.5f) ? 2 : 1;
            bDidSomething = true;
            m_DesireReceivePassSharedVars.pOneTouchPassTarget = pTarget;
            break;
        }
        case FIELDERDESIRE_USE_POWERUP:
        {
            ePowerUpType powerupType = (ePowerUpType)looseBallAction.ExtraData.mData.i;
            float fReactionRandom = 0.45f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.45f + fReactionOffset))
                break;
            if (powerupType != m_pTeam->GetCurrentPowerUp().eType)
                break;
            if (m_nPowerupAnimID >= 0)
                break;
            UseTeamPowerup(NULL);
            break;
        }
        case FIELDERDESIRE_HIT:
        {
            cPlayer* pTarget = looseBallAction.ExtraData.mData.pPlayer;
            float fReactionRandom = 0.59f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.59f + fReactionOffset))
                break;
            EndDesire(false);
            InitDesire(FIELDERDESIRE_HIT, 0.5f, -1.0f, FuzzyVariant(pTarget), fvNotSet);
            bDidSomething = true;
            break;
        }
        default:
            break;
        }
    }

    return bDidSomething;
}

/**
 * Offset/Address/Size: 0x3C | 0x80019378 | size: 0x680
 */
bool cFielder::DoAIWindupActionSelection()
{
    extern cFielder* g_pScriptCurrentFielder;
    extern cTeam* g_pCurrentlyUpdatingTeam;
    static FilteredRandomChance randchancegen;

    FuzzyVariant looseBallAction = Fuzzy::GetBestWindupShotAction(this);

    bool bSelectChance = randchancegen.genrand(looseBallAction.SelectionChance);
    bool bDidSomething = false;
    float fActionScore = looseBallAction.Confidence;
    eFielderDesireState action = (eFielderDesireState)looseBallAction.mData.i;
    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    float fPerturbPercent = 0.5f * (1.0f - pSkillTweaks->Off_Reaction);

    if (bSelectChance)
    {
        switch (action)
        {
        case 0:
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 15:
        case 16:
        case 17:
            break;
        case FIELDERDESIRE_PASS:
        {
            float fReactionRandom = 0.6f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.6f + fReactionOffset))
                break;
            cPlayer* pTarget = looseBallAction.ExtraData.mData.pPlayer;
            EndDesire(false);
            InitDesire(FIELDERDESIRE_PASS, looseBallAction.Confidence, -1.0f, FuzzyVariant(pTarget), FuzzyVariant(OpenTo(g_pScriptCurrentFielder, pTarget) < 0.5f));
            bDidSomething = true;
            break;
        }
        case FIELDERDESIRE_DEKE:
        {
            float fReactionRandom = 0.4f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.4f + fReactionOffset))
                break;
            EndDesire(false);
            InitDesire(FIELDERDESIRE_DEKE, 0.5f, -1.0f, fvNotSet, fvNotSet);
            bDidSomething = true;
            break;
        }
        case FIELDERDESIRE_USE_POWERUP:
        {
            ePowerUpType powerupType = (ePowerUpType)looseBallAction.ExtraData.mData.i;
            float fReactionRandom = 0.4f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.4f + fReactionOffset))
                break;
            if (powerupType != m_pTeam->GetCurrentPowerUp().eType)
                break;
            if (m_nPowerupAnimID >= 0)
                break;
            cFielder* pPowerupTarget = NULL;
            UseTeamPowerup(pPowerupTarget);
            break;
        }
        case FIELDERDESIRE_SHOOT:
        {
            float fReactionRandom = 0.6f * fPerturbPercent;
            float fReactionOffset = nlRandomf(fReactionRandom, &nlDefaultSeed) - 0.5f * fReactionRandom;
            if (!(fActionScore >= 0.6f + fReactionOffset))
                break;
            m_pShotMeter->ShotReleased(this);
            bDidSomething = true;
            break;
        }
        default:
            break;
        }
    }

    return bDidSomething;
}

/**
 * Offset/Address/Size: 0x0 | 0x8001933C | size: 0x3C
 */
void cFielder::DoSpeedBoost()
{
    eShotMeterState meterState = m_pShotMeter->m_eShotMeterState;
    bool bReturn = (meterState == SHOT_METER_ACTIVE || meterState == SHOT_METER_STS_ACTIVE || meterState == SHOT_METER_STS_TRANSISTION);

    if (bReturn)
        return;

    m_fActualSpeed = 12.0f;
}
