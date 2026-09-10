#include "Game/Player.h"
#include "Game/Character.h"
#include "Game/Ball.h"
#include "Game/AnimInventory.h"
#include "Game/CharacterAudio.h"
#include "Game/Net.h"

#include "Game/AI/Fielder.h"
#include "Game/AI/ShotMeter.h"
#include "Game/AI/SpaceSearch.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Scripts/CommonScript.h"
#include "Game/AI/Scripts/ScriptQuestions.h"

#include "Game/CharacterTemplate.h"
#include "Game/SAnim/pnFeather.h"
#include "Game/SAnim/pnSingleAxisBlender.h"
#include "NL/nlMath.h"
#include "NL/nlString.h"

#include "Game/DB/StatsTracker.h"
#include "Game/Effects/EffectsGroup.h"
#include "Game/CharacterTriggers.h"
#include "Game/Game.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsColumn.h"
#include "Game/FixedUpdateTask.h"
#include "Game/FormationDefines.h"

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 sbNoBallPickups;

extern float g_fFixedUpdateTick;

static u16 g_aOOIConstraint = (u16)DegreesToAngle(180.0f);
static int g_nHeadTiltMax = DegreesToAngle(15.0f);
static int g_nHeadSpinMax = DegreesToAngle(85.0f);
static int g_nWaluigiHeadSpinMax = DegreesToAngle(70.0f);
static int g_nWarioHeadSpinMax = DegreesToAngle(50.0f);
static int g_nWarioHeadTiltMax = DegreesToAngle(-10.0f);

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };
static const float g_fPassInterceptNoPickupTimer = 0.5f;

inline float max_float(float a, float b)
{
    return (a >= b) ? a : b;
}

inline float min_float(float a, float b)
{
    return (a <= b) ? a : b;
}

static inline float DoClampPassSpeed(float fNewPassGroundSpeed, cPlayer* pPlayer)
{
    float fMaxPassSpeed = pPlayer->m_pTweaks->fPassGroundSpeedMax;
    float fMinPassSpeed = pPlayer->m_pTweaks->fPassGroundSpeedMin;
    fNewPassGroundSpeed = (fNewPassGroundSpeed >= fMinPassSpeed) ? fNewPassGroundSpeed : fMinPassSpeed;
    return (fNewPassGroundSpeed <= fMaxPassSpeed) ? fNewPassGroundSpeed : fMaxPassSpeed;
}

static inline float DoCalculatePassSpeed(
    const nlVector3& teammatePosition,
    const nlVector3& teammateVelocity,
    float passSpeedMin,
    float passSpeedMax,
    float passDistMin,
    float passDistMax)
{
    const nlVector3& ballPos = g_pBall->m_v3Position;
    float dx = teammatePosition.x - ballPos.x;
    float dy = teammatePosition.y - ballPos.y;
    float distToTarget = nlSqrt(dx * dx + dy * dy, true);
    float passGroundSpeed = InterpolateRangeClamped(passSpeedMin, passSpeedMax, passDistMin, passDistMax, distToTarget);
    float closingSpeed = GetClosingSpeed2D(teammatePosition, teammateVelocity, g_pBall->m_v3Position, v3Zero);
    return passGroundSpeed - 0.5f * closingSpeed;
}

static inline cFielder* GetAIOrderedFielder(cTeam* pTeam, s32 i)
{
    return pTeam->m_pAIOrderedFielders[i];
}

static inline cPlayer* GetClosestPlayerOnTeam(cPlayer* pSelf, cTeam* pTeam, int nNumPlayers, nlVector3* pPosition)
{
    cPlayer* pClosestPlayer = NULL;
    f32 refX;
    f32 refY;
    f32 fClosestDistSquared = HUGE_VALF;

    nlVector3 v3RefPos;
    if (pPosition == NULL)
    {
        v3RefPos = pSelf->m_v3Position;
    }
    else
    {
        v3RefPos = *pPosition;
    }

    refX = v3RefPos.x;
    refY = v3RefPos.y;

    for (int i = 0; i < nNumPlayers; i++)
    {
        cPlayer* pPlayer = pTeam->GetPlayer(i);
        if (pPlayer == pSelf)
        {
            continue;
        }
        f32 dx = refX - pPlayer->m_v3Position.x;
        f32 dy = refY - pPlayer->m_v3Position.y;
        f32 dist = dx * dx + dy * dy;
        if (dist < fClosestDistSquared)
        {
            pClosestPlayer = pPlayer;
            fClosestDistSquared = dist;
        }
    }

    return pClosestPlayer;
}

static inline void UpdateTimers(cPlayer* self, float fDeltaT)
{
    if (self->m_pBall != NULL)
    {
        self->m_tBallPossessionTimer.Countup(fDeltaT, 10.0f);
    }
    else
    {
        self->m_tBallUnPossessionTimer.Countup(fDeltaT, 10.0f);
    }

    self->m_tSlideAttackTimer.Countdown(fDeltaT, 0.0f);
    self->m_tSwapFacingTimer.Countdown(fDeltaT, 0.0f);
    self->m_tNoPickupPassInterceptTimer.Countdown(fDeltaT, 0.0f);

    if (self->m_tNoPickupTimer.m_uPackedTime != 0)
    {
        bool bCanCollideWithBall = self->m_tNoPickupTimer.Countdown(fDeltaT, 0.0f);
        self->m_pPhysicsCharacter->m_CanCollideWithBall = bCanCollideWithBall;
    }
    else
    {
        self->m_pPhysicsCharacter->m_CanCollideWithBall = true;
    }

    float fScaledDt = fDeltaT * (1.0f / FixedUpdateTask::mTimeScale);
    for (int iPadID = 0; iPadID < 4; iPadID++)
    {
        self->m_tSwapControllerTimer[iPadID].Countdown(fScaledDt, 0.0f);
    }

    if (self->m_fActualSpeed < 2.5f)
    {
        self->m_tInactivityTimer.Countup(fDeltaT, 10.0f);
    }
    else
    {
        self->m_tInactivityTimer.m_uPackedTime = 0;
    }
}

/**
 * Offset/Address/Size: 0x2FAC | 0x8005A4FC | size: 0x380
 */
cPlayer::cPlayer(
    int nPlayerID,
    eCharacterClass characterClass,
    const int* nModelID,
    cSHierarchy* hierarchy,
    cAnimInventory* animInventory,
    const CharacterPhysicsData* physData,
    PlayerTweaks* playerTweaks,
    AnimRetargetList* animRetargetList,
    eClassTypes classType)
    : cCharacter(
          characterClass,
          nModelID,
          hierarchy,
          animInventory,
          physData,
          playerTweaks->fPhysCapsuleHeight,
          playerTweaks->fPhysCapsuleRadius,
          animRetargetList,
          classType)
    , m_ID(nPlayerID)
    , m_bIsContactingWall(false)
    , m_ePositionSeekState(PSS_ARRIVED)
    , m_eBallRotationMode(BRM_MATCH_VELOCITY)
    , m_ResetBaseBallOrientation(false)
    , m_tBallPossessionTimer(0.0f)
    , m_tBallUnPossessionTimer(0.0f)
    , m_tNoPickupTimer(0.0f)
    , m_tNoPickupPassInterceptTimer(0.0f)
    , m_fShotStrengthTime(0.0f)
    , m_tSlideAttackTimer(0.0f)
    , m_tLooseBallPassTimer(0.0f)
    , m_tInactivityTimer(0.0f)
    , m_bCanTestController(true)
    , m_eLastPadAction(PAD_NONE)
    , m_aSwapFacingDirection(0)
    , m_tSwapFacingTimer(0.0f)
    , m_UserControlledTime(0.0f)
    , m_pController(NULL)
    , m_pTweaks(playerTweaks)
    , m_pBall(NULL)
    , m_pTeam(NULL)
{
    cSHierarchy* baseHierarchy;

    baseHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nBallJointIndex = baseHierarchy->GetNodeIndexByID(nlStringLowerHash("ball"));

    baseHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nRightFootJointIndex = baseHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 r foot"));

    baseHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nLeftFootJointIndex = baseHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 l foot"));

    baseHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nLeftHandJointIndex = baseHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 l hand"));

    baseHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nRightHandJointIndex = baseHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 r hand"));

    cPN_Feather* receivePassLayer = ::new (AllocateFeather()) cPN_Feather(m_pPoseAccumulator->m_BaseSHierarchy, NULL, 0);
    m_pReceivePassLayer = receivePassLayer;

    baseHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
    int nodeIndex = baseHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 spine1"));
    m_pReceivePassLayer->SetNodeWeight(nodeIndex, 1.0f, 0.2f);
    m_pReceivePassLayer->SetNodeWeight(m_nBallJointIndex, 1.0f);
    m_pAILayer = m_pReceivePassLayer->GetChildPtr(0);

    cPN_Feather* powerupLayer = ::new (AllocateFeather()) cPN_Feather(m_pPoseAccumulator->m_BaseSHierarchy, NULL, 0);
    m_pPowerupLayer = powerupLayer;
    m_pPowerupLayer->SetChild(0, m_pReceivePassLayer);
    m_pPoseTree = m_pPowerupLayer;
    m_pSpaceSearch = NULL;

    SetAnimState(0, true, 0.2f, false, false);
    PoseLocalSpace();
    AdjustPoseMatrices();

    m_tSwapFacingTimer.SetSeconds(0.0f);

    m_v3AIPosition.x = 0.0f;
    m_v3AIPosition.y = 0.0f;
    m_v3AIPosition.z = 0.0f;
}

/**
 * Offset/Address/Size: 0x2E4C | 0x8005A39C | size: 0x160
 */
cPlayer::~cPlayer()
{
    EndBlur();
    delete m_pTweaks;

    if (m_pBall != NULL)
    {
        m_pPhysicsCharacter->ReleaseObject();
        m_pBall->ClearOwner();
        m_pBall = NULL;

        f32 possessionTime = m_tBallPossessionTimer.GetSeconds();
        StatsTracker::Instance()->TrackStat(STATS_POSSESION_TIME, m_pTeam->m_nSide, m_ID, 100.0f * possessionTime, 0, 0, 0);

        if (IsPlayingEffect(fxGetGroup("ball_sts_windup")))
        {
            StopSFX((Audio::eCharSFX)0x14);
            StopSFX((Audio::eCharSFX)0x39);
        }

        KillWindups(this);
        StopSFX((Audio::eCharSFX)0x12);
        m_pCharacterSFX->StopMovementLoop();
    }

    delete m_pSpaceSearch;
}

/**
 * Offset/Address/Size: 0x2DF0 | 0x8005A340 | size: 0x5C
 */
void cPlayer::SetSpaceSearch(SpaceSearch* pSpaceSearch)
{
    if (m_pSpaceSearch != NULL)
    {
        delete m_pSpaceSearch;
    }
    m_pSpaceSearch = pSpaceSearch;
}

/**
 * Offset/Address/Size: 0x2DE0 | 0x8005A330 | size: 0x10
 */
void cPlayer::ResetDesiredDirections(unsigned short direction)
{
    m_aDesiredFacingDirection = direction;
    m_aDesiredMovementDirection = m_aDesiredFacingDirection;
}

/**
 * Offset/Address/Size: 0x2CCC | 0x8005A21C | size: 0x114
 */
void cPlayer::SetDesiredFacingDirection()
{
    if (m_pController != NULL)
    {
        if (m_pController->GetMovementStickMagnitude() > 0.0f)
        {
            if (m_tSwapFacingTimer.m_uPackedTime != 0)
            {
                GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;
                f32 t = m_tSwapFacingTimer.GetSeconds() / pGameTweaks->fSwapFacingTime;
                u16 aStickDirection = m_pController->GetMovementStickDirection();
                s16 diff = m_aSwapFacingDirection - aStickDirection;
                m_aDesiredFacingDirection = (u16)((f32)aStickDirection + (f32)diff * t);
            }
            else
            {
                m_aDesiredFacingDirection = m_pController->GetMovementStickDirection();
            }
        }
        else
        {
            m_aDesiredFacingDirection = m_aActualFacingDirection;
        }
    }

    if (m_eAnimID >= 0x5C || m_eAnimID < 0x56)
    {
        m_aDesiredMovementDirection = m_aDesiredFacingDirection;
    }
}

/**
 * Offset/Address/Size: 0x2A50 | 0x80059FA0 | size: 0x27C
 */
u8 cPlayer::SwapController()
{
    cPlayer* pSwapPlayer;
    s32 i;
    s32 iPadID;
    cPlayer* pPotentialSwapPlayer;

    if (m_bCanTestController != 0)
    {
        if (g_pBall->m_pPassTarget == NULL || g_pBall->m_pPassTarget != this)
        {
            pSwapPlayer = NULL;
            iPadID = ((m_pController != NULL) ? m_pController->m_pGlobalPad : NULL)->m_padIndex;

            for (i = 0; i < 4; i++)
            {
                pPotentialSwapPlayer = m_pTeam->GetPlayer(i);
                if (pPotentialSwapPlayer != this)
                {
                    cGlobalPad* pPotentialSwapPad = (pPotentialSwapPlayer->m_pController != NULL)
                                                      ? pPotentialSwapPlayer->m_pController->m_pGlobalPad
                                                      : NULL;
                    if (pPotentialSwapPad == NULL)
                    {
                        if (pSwapPlayer == NULL
                            || (pPotentialSwapPlayer->m_tSwapControllerTimer[iPadID].GetSeconds() == 0.0f
                                && pSwapPlayer->m_tSwapControllerTimer[iPadID].GetSeconds() > 0.0f))
                        {
                            pSwapPlayer = pPotentialSwapPlayer;
                            continue;
                        }

                        float fSwapPlayerTime = pSwapPlayer->m_tSwapControllerTimer[iPadID].GetSeconds();
                        if (pPotentialSwapPlayer->m_tSwapControllerTimer[iPadID].GetSeconds() <= fSwapPlayerTime)
                        {
                            float fPotentialScore = Fuzzy::GetSwapControllerScore(pPotentialSwapPlayer).mData.f;
                            float fSwapScore = Fuzzy::GetSwapControllerScore(pSwapPlayer).mData.f;
                            if (fPotentialScore > fSwapScore)
                            {
                                pSwapPlayer = pPotentialSwapPlayer;
                            }
                        }
                    }
                }
            }

            if (pSwapPlayer == NULL)
            {
                float fBestDist = 999999.9f;
                s32 iFielder;
                cFielder* pCandidate;

                for (iFielder = 0; iFielder < 4; iFielder++)
                {
                    pCandidate = m_pTeam->GetFielder(iFielder);

                    float dx, dy, dz;
                    dy = pCandidate->m_v3Position.y - g_pBall->m_v3Position.y;
                    dx = pCandidate->m_v3Position.x - g_pBall->m_v3Position.x;
                    dz = pCandidate->m_v3Position.z - g_pBall->m_v3Position.z;
                    float fDist = nlSqrt(dx * dx + dy * dy + dz * dz, true);

                    if (pCandidate != this)
                    {
                        cGlobalPad* pCandidatePad = (pCandidate->m_pController != NULL) ? pCandidate->m_pController->m_pGlobalPad : NULL;
                        if (pCandidatePad == NULL && fDist < fBestDist)
                        {
                            pSwapPlayer = pCandidate;
                            fBestDist = fDist;
                        }
                    }
                }

                if (pSwapPlayer == NULL)
                {
                    return 0;
                }
            }

            if (m_fActualSpeed > 1.0f)
            {
                pSwapPlayer->m_tSwapFacingTimer.SetSeconds(g_pGame->m_pGameTweaks->fSwapFacingTime);
                pSwapPlayer->m_aSwapFacingDirection = pSwapPlayer->m_aActualMovementDirection;
            }

            m_tSwapControllerTimer[iPadID].SetSeconds(g_pGame->m_pGameTweaks->fSwapControllerTime);
            pSwapPlayer->SetAIPad(m_pController);
            pSwapPlayer->m_bCanTestController = false;
            SetAIPad(NULL);

            return 1;
        }
    }

    return 0;
}

/**
 * Offset/Address/Size: 0x288C | 0x80059DDC | size: 0x1C4
 */
void cPlayer::Update(float fDeltaT)
{
    if (m_pController != NULL)
    {
        m_UserControlledTime += fDeltaT;
    }
    else
    {
        m_UserControlledTime = 0.0f;
    }

    if (g_pGame->IsGameplayOrOvertime())
    {
        UpdateTimers(this, fDeltaT);
    }

    FieldLocToAILoc(m_v3AIPosition, m_v3Position, (eTeamSide)m_pTeam->m_nSide);
}

/**
 * Offset/Address/Size: 0x26FC | 0x80059C4C | size: 0x190
 */
void cPlayer::GetAnimatedBallOrientation(nlQuaternion& qRetval)
{
    cSAnim* pSAnim = m_pCurrentAnimController->m_pSAnim;
    float time = m_pCurrentAnimController->m_fTime;
    int sanimBallNode = m_pCurrentAnimController->RemapNode(m_nBallJointIndex);

    RotAccum* rot = &m_pPoseAccumulator->m_rot.mData[m_nBallJointIndex];
    rot->q.x = 0.0f;
    rot->q.y = 0.0f;
    rot->q.z = 0.0f;
    rot->q.w = 1.0f;
    rot->quatAccumulatedWeight = 0.0f;
    rot->rotAroundZ = 0;
    rot->rotAroundZAccumulatedWeight = 0.0f;
    rot->bIdentity = true;

    pSAnim->BlendRot(m_nBallJointIndex, sanimBallNode, time, 1.0f, m_pPoseAccumulator, m_pCurrentAnimController->m_bMirror);

    nlQuaternion qAnimated = rot->q;
    nlQuaternion qFacing;
    nlVector3 v3Up = { 0.0f, 0.0f, 1.0f };
    nlQuaternion qOrient;
    nlQuaternion startInverse;

    nlMakeQuat(qFacing, v3Up, 0.0000958738f * (float)m_aActualFacingDirection);
    nlMultQuat(qOrient, qFacing, qAnimated);

    if (m_ResetBaseBallOrientation)
    {
        nlQuatInverse(startInverse, qOrient);
        nlMultQuat(m_BaseBallOrientation, startInverse, g_pBall->m_qOrientation);
        m_ResetBaseBallOrientation = false;
    }

    nlMultQuat(qRetval, qOrient, m_BaseBallOrientation);
}

/**
 * Offset/Address/Size: 0x26A0 | 0x80059BF0 | size: 0x5C
 */
void cPlayer::SetAnimID(int animID)
{
    cCharacter::SetAnimID(animID);
    m_eBallRotationMode = (eBallRotationMode)m_pAnimInventory->GetBallRotationMode(animID);
    if (m_pBall != nullptr)
    {
        m_ResetBaseBallOrientation = true;
    }
}

/**
 * Offset/Address/Size: 0x256C | 0x80059ABC | size: 0x134
 */
float cPlayer::DoFlashLight(
    const nlVector3& Position1,
    const nlVector3& Position2,
    unsigned short aDirection,
    float fAngleWeighting,
    float fIgnoreObjectCloserThanThis,
    float fIgnoreObjectFartherThanThis)
{
    float dx;
    float dy = Position2.y - Position1.y;
    dx = Position2.x - Position1.x;
    float fDistBetween;
    float dist = nlSqrt(dx * dx + dy * dy, true);
    fDistBetween = dist;
    if (dist < fIgnoreObjectCloserThanThis || dist > fIgnoreObjectFartherThanThis)
    {
        fDistBetween = 9999.9f;
    }

    u16 targetAngle = (u16)(s32)(10430.378f * nlATan2f(dy, dx));
    float fDistWeight = 1.0f - fAngleWeighting;
    s16 angleDiff = aDirection - targetAngle;
    u16 absAngle = (angleDiff < 0) ? -angleDiff : angleDiff;

    return fDistBetween * fDistWeight + fAngleWeighting * absAngle;
}

/**
 * Offset/Address/Size: 0x2438 | 0x80059988 | size: 0x134
 */
float cPlayer::DoFlashLight(
    const nlVector3& Position,
    unsigned short aDirection,
    float fAngleWeighting,
    float fIgnoreObjectCloserThanThis,
    float fIgnoreObjectFartherThanThis)
{
    float fDistBetween;
    float dx;
    float dy = Position.y - m_v3Position.y;
    dx = Position.x - m_v3Position.x;

    float fSqrt = nlSqrt(dx * dx + dy * dy, true);
    fDistBetween = fSqrt;

    if (fSqrt < fIgnoreObjectCloserThanThis || fSqrt > fIgnoreObjectFartherThanThis)
    {
        fDistBetween = 9999.9f;
    }

    float fConverted = 10430.37835f * nlATan2f(dy, dx);
    float fInvWeight = 1.0f - fAngleWeighting;
    s16 angleDiff = (s16)(aDirection - (u16)(s32)fConverted);
    s32 angleAbs;
    if (angleDiff < 0)
    {
        angleAbs = -angleDiff;
    }
    else
    {
        angleAbs = angleDiff;
    }

    return fDistBetween * fInvWeight + fAngleWeighting * (f32)(u16)angleAbs;
}

/**
 * Offset/Address/Size: 0x23D4 | 0x80059924 | size: 0x64
 */
void cPlayer::CollideWithBallCallback(cBall* pBall)
{
    if (pBall->m_pPassTarget != NULL)
    {
        if (pBall->m_pPassTarget != this)
        {
            const float& fNoPickupTimer = g_fPassInterceptNoPickupTimer;
            m_tNoPickupPassInterceptTimer.SetSeconds(fNoPickupTimer);
        }
    }

    if (pBall->m_pOwner == NULL)
    {
        pBall->m_pLastTouch = this;
    }
}

/**
 * Offset/Address/Size: 0x23D0 | 0x80059920 | size: 0x4
 */
void cPlayer::CollideWithCharacterCallback(CollisionPlayerPlayerData* pData)
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x2298 | 0x800597E8 | size: 0x138
 */
cFielder* cPlayer::GetClosestOpponentFielder(nlVector3* pPosition)
{
    return (cFielder*)GetClosestPlayerOnTeam(this, m_pTeam->GetOtherTeam(), 4, pPosition);
}

/**
 * Offset/Address/Size: 0x1FAC | 0x800594FC | size: 0x2EC
 */
void cPlayer::PickupBall(cBall* pBall)
{
    if (m_eClassType == GOALIE && ((Goalie*)this)->mbNoUserControl)
    {
    }
    else
    {
        cGlobalPad* hasPad = (m_pController != NULL) ? m_pController->m_pGlobalPad : NULL;
        if (hasPad == NULL)
        {
            cPlayer* closest = NULL;

            cAIPad* goalieCtrl = m_pTeam->GetGoalie()->m_pController;
            cGlobalPad* goaliePad = (goalieCtrl != NULL) ? goalieCtrl->m_pGlobalPad : NULL;
            if (goaliePad != NULL)
            {
                closest = m_pTeam->GetGoalie();
            }
            else
            {
                f32 bestDistSq;
                for (s32 i = 0; i < 4; i++)
                {
                    cPlayer* player = m_pTeam->GetPlayer(i);
                    cGlobalPad* playerPad = (player->m_pController != NULL) ? player->m_pController->m_pGlobalPad : NULL;
                    if (playerPad == NULL)
                        continue;

                    if (closest != NULL)
                    {
                        f32 dy = player->m_v3Position.y - m_v3Position.y;
                        f32 dx = player->m_v3Position.x - m_v3Position.x;
                        cPlayer* passTarget = g_pBall->m_pPassTarget;
                        f32 distSq = dx * dx + dy * dy;

                        if (passTarget == NULL || closest != g_pBall->m_pPrevOwner)
                        {
                            if (distSq < bestDistSq)
                            {
                            }
                            else
                            {
                                continue;
                            }
                        }

                        if (passTarget != NULL && player == g_pBall->m_pPrevOwner)
                        {
                            continue;
                        }

                        bestDistSq = distSq;
                        closest = player;
                    }
                    else
                    {
                        closest = player;
                    }
                }
            }

            if (closest != NULL)
            {
                SetAIPad(closest->m_pController);
                closest->SetAIPad(NULL);
            }
        }
    }

    Event* event = g_pEventManager->CreateValidEvent(0xD, 0x20);
    ReceiveBallData* data = new (&event->m_data) ReceiveBallData();
    data->pReceiver = this;
    data->eResult = RECEIVEBALL_LOOSE_PICKUP;

    cPlayer* prevPassTarget = pBall->m_pPassTarget;
    if (prevPassTarget != NULL)
    {
        if (prevPassTarget == this)
        {
            data->eResult = RECEIVEBALL_PASS_COMPLETE;
        }
        else if (prevPassTarget->m_pTeam != m_pTeam)
        {
            data->eResult = RECEIVEBALL_PASS_INTERCEPT;
        }
    }

    if (data->eResult == RECEIVEBALL_PASS_COMPLETE)
    {
        bool bOneTouchShot = false;
        if (m_eClassType == FIELDER)
        {
            if (((cFielder*)this)->GetOneTouchShotDesire() != 0)
            {
                bOneTouchShot = true;
            }
        }
        if (!bOneTouchShot)
        {
            EmitBallImpact(this, false);
        }
    }

    m_tBallPossessionTimer.m_uPackedTime = 0;
    m_tBallUnPossessionTimer.m_uPackedTime = 0;
    m_pBall = pBall;
    m_pBall->SetOwner(this);

    if (m_pBall != NULL)
    {
        m_pBall->m_pPhysicsBall->EnableCollisions();
        m_pPhysicsCharacter->ContainObject(m_pBall->m_pPhysicsBall);
    }

    if (m_pBall != NULL)
    {
        nlVector3& jointPos = GetJointPosition(m_nBallJointIndex);
        nlVector3 pos = jointPos;
        m_pPhysicsCharacter->m_SubObject.SetSubObjectPosition(pos, PhysicsObject::WORLD_COORDINATES);
    }

    m_ResetBaseBallOrientation = true;
    if (m_eClassType == FIELDER)
    {
        ((cFielder*)this)->ClearVolleyPass();
    }
}

/**
 * Offset/Address/Size: 0x1E20 | 0x80059370 | size: 0x18C
 */
void cPlayer::PlayAttackReactionSounds(float fScale)
{
    Audio::cCharacterSFX* pSFX = m_pCharacterSFX;

    Audio::gCrowdSFX.Stop(Audio::CROWDSFX_EVENT_OH_SMALL3, cGameSFX::SFX_STOP_FIRST);

    if (pSFX->IsKeepingTrackOf(Audio::CHARSFX_HIT_BODY, NULL))
    {
        pSFX->Stop(Audio::CHARSFX_HIT_BODY, cGameSFX::SFX_STOP_FIRST);
    }
    else if (pSFX->IsKeepingTrackOf(Audio::CHARSFX_HIT_BODY_BONE, NULL))
    {
        pSFX->Stop(Audio::CHARSFX_HIT_BODY_BONE, cGameSFX::SFX_STOP_FIRST);
    }

    Audio::SoundAttributes soundAttr;
    soundAttr.Init();

    if (fScale >= g_pGame->m_pGameTweaks->fMinHitIntensityForHardBodyHitSFX && m_eClassType != GOALIE)
    {
        soundAttr.SetSoundType(Audio::CHARSFX_HIT_BODY_BONE, true);
    }
    else
    {
        soundAttr.SetSoundType(Audio::CHARSFX_HIT_BODY, true);
    }

    soundAttr.UseStationaryPosVector(m_v3Position);

    soundAttr.mf_Volume = fScale;
    if (100.0f != fScale)
    {
        soundAttr.mf_Volume *= m_pCharacterSFX->GetSFXInfo(soundAttr.mu_Type).fVolume;
    }

    PlaySFX(soundAttr);

    if (!pSFX->IsPlayingRandomCharDialogue(CHAR_DIALOGUE_ELECTROCUTE))
    {
        pSFX->StopPlayingAllRandomCharDialogue();
        if (m_eCharacterClass != MYSTERY)
        {
            PlayRandomCharDialogue(1, VECTORS, fScale, -1.0f);
        }
    }
}

/**
 * Offset/Address/Size: 0x197C | 0x80058ECC | size: 0x4A4
 */
void cPlayer::SetAIPad(cAIPad* pPad)
{
    m_pController = pPad;
    if (m_eClassType != FIELDER)
        return;

    cFielder* pFielder = (cFielder*)this;

    if (pFielder->m_pBall != NULL)
    {
        u8 bShotInProgress = 0;
        eShotMeterState state;
        ShotMeter* pMeter = pFielder->m_pShotMeter;

        state = pMeter->m_eShotMeterState;
        if (state == SHOT_METER_ACTIVE || state == SHOT_METER_STS_ACTIVE || state == SHOT_METER_STS_TRANSISTION)
            bShotInProgress = 1;

        if (bShotInProgress || (pFielder->m_eActionState != ACTION_SHOT && pMeter->m_eShotMeterState == SHOT_METER_RELEASED))
        {
            pMeter->ShotReleased(pFielder);
            pFielder->InitActionShot(false);
            return;
        }
    }

    if (m_pController != NULL && pFielder->m_eFielderDesireState < FIELDERDESIRE_USER_CONTROLLED)
    {
        if (g_pGame->IsGameplayOrOvertime() && pFielder->m_eActionState == ACTION_SHOOT_TO_SCORE)
        {
            pFielder->InitDesire(FIELDERDESIRE_FINISH_ACTION, 0.5f, -1.0f, fvNotSet, fvNotSet);
            return;
        }

        pFielder->EndDesire(false);
        return;
    }

    if (m_pController == NULL && pFielder->m_eFielderDesireState == FIELDERDESIRE_USER_CONTROLLED)
    {
        if (g_pGame->IsGameplayOrOvertime() && (pFielder->m_eActionState == ACTION_SLIDE_ATTACK || pFielder->m_eActionState == ACTION_SHOOT_TO_SCORE))
        {
            pFielder->InitDesire(FIELDERDESIRE_FINISH_ACTION, 0.5f, -1.0f, fvNotSet, fvNotSet);
            return;
        }

        pFielder->EndDesire(false);
        return;
    }

    if (m_pController != NULL && pFielder->m_eFielderDesireState == FIELDERDESIRE_WAIT && g_pGame->m_eGameState == GS_KICKOFF)
        pFielder->EndDesire(false);
}

/**
 * Offset/Address/Size: 0x1954 | 0x80058EA4 | size: 0x28
 */
bool cPlayer::IsOnSameTeam(cPlayer* other)
{
    if ((other != NULL) && (other->m_pTeam == m_pTeam))
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x1930 | 0x80058E80 | size: 0x24
 */
bool cPlayer::IsCaptain() const
{
    return ::IsCaptain(m_eCharacterClass);
}

/**
 * Offset/Address/Size: 0x1700 | 0x80058C50 | size: 0x230
 */
cPlayer* cPlayer::DoFindBestPassTarget(bool bAllowLeadPass, bool bIsPerfectPass)
{
    f32 fBestScore = 9000.0f;
    cFielder* pTarget;
    cPlayer* pBestTarget = NULL;

    for (s32 i = 0; i < 4; i++)
    {
        pTarget = GetAIOrderedFielder(m_pTeam, i);

        if (pTarget == this)
            continue;

        u16 aDirection = m_aActualFacingDirection;

        if (m_pController != NULL)
        {
            if (m_pController->GetMovementStickMagnitude() > 0.01f)
            {
                aDirection = m_pController->GetMovementStickDirection();
            }
        }

        f32 fAngleWeighting;
        f32 fDistBetween;
        f32 dx;
        f32 dy;
        f32 fMinDistance;
        f32 fMaxDistance;

        if (bAllowLeadPass || bIsPerfectPass)
        {
            fMaxDistance = 20.0f;
            fAngleWeighting = g_pGame->m_pGameTweaks->fVolleyPassAngleWeighting;
            fMinDistance = g_pGame->m_pGameTweaks->fVolleyPassMinDistance;
        }
        else
        {
            fMinDistance = 1.0f;
            fMaxDistance = 99999.0f;
            fAngleWeighting = g_pGame->m_pGameTweaks->fPassAngleWeighting;
        }

        dy = pTarget->m_v3Position.y - m_v3Position.y;
        dx = pTarget->m_v3Position.x - m_v3Position.x;

        f32 fSqrt = nlSqrt(dx * dx + dy * dy, true);
        fDistBetween = fSqrt;

        if (fSqrt < fMinDistance || fSqrt > fMaxDistance)
        {
            fDistBetween = 9999.9f;
        }

        f32 fConverted = 10430.37835f * nlATan2f(dy, dx);
        f32 fInvWeight = 1.0f - fAngleWeighting;
        s16 angleDiff = (s16)(aDirection - (u16)(s32)fConverted);
        s32 angleAbs;
        if (angleDiff < 0)
        {
            angleAbs = -angleDiff;
        }
        else
        {
            angleAbs = angleDiff;
        }

        f32 fScore = fDistBetween * fInvWeight + fAngleWeighting * (f32)(u16)angleAbs;

        if (!pTarget->CanReceivePass())
        {
            fScore += 99999.9f;
        }

        if (fScore < fBestScore)
        {
            pBestTarget = pTarget;
            fBestScore = fScore;
        }
    }

    if (pBestTarget == NULL)
    {
        pBestTarget = m_pTeam->GetDefence();
        if (pBestTarget == this)
        {
            pBestTarget = m_pTeam->GetMidfield();
        }
    }

    return pBestTarget;
}

/**
 * Offset/Address/Size: 0x16E4 | 0x80058C34 | size: 0x1C
 */
cGlobalPad* cPlayer::GetGlobalPad()
{
    if (m_pController != NULL)
    {
        return m_pController->m_pGlobalPad;
    }
    return NULL;
}

/**
 * Offset/Address/Size: 0x1610 | 0x80058B60 | size: 0xD4
 */
void cPlayer::ReleaseBall()
{
    m_pPhysicsCharacter->ReleaseObject();
    m_pBall->ClearOwner();
    m_pBall = NULL;

    float fPossessionTime = m_tBallPossessionTimer.GetSeconds();
    StatsTracker::Instance()->TrackStat(STATS_POSSESION_TIME, m_pTeam->m_nSide, m_ID, 100.0f * fPossessionTime, 0, 0, 0);

    if (IsPlayingEffect(fxGetGroup("ball_sts_windup")))
    {
        StopSFX(Audio::CHARSFX_SHOT_WINDUP);
        StopSFX(Audio::CHARSFX_EFFORTS_HEAD_SHAKE);
    }

    KillWindups(this);
    StopSFX(Audio::CHARSFX_KICK_ATTEMPT);
    m_pCharacterSFX->StopMovementLoop();
}

/**
 * Offset/Address/Size: 0x15DC | 0x80058B2C | size: 0x34
 */
void cPlayer::ResetUnPossessionTimer()
{
    if (m_pBall == NULL)
    {
        m_tBallUnPossessionTimer.SetSeconds(2.5f);
    }
}

/**
 * Offset/Address/Size: 0xA10 | 0x80057F60 | size: 0xBCC
 */
void cPlayer::DoRegularPassing(cPlayer* pTeammate, bool bVolleyPass, bool bAllowLeadPass, bool bParam3, bool bParam4)
{
    g_pBall->m_tNoPickupTimer.SetSeconds(0.1f);
    cFielder* pPassTarget = (cFielder*)pTeammate;
    cFielder* pPasser = (cFielder*)this;
    g_pBall->mbIsPerfectShot = bVolleyPass;
    nlVector3 teammateLeadPassVelocity;
    nlVector3 v3PassIntercept = { 0.0f, 0.0f, 0.0f };
    bool calcPassIntercept = false;
    float fDistToBall;
    float fPassGroundSpeed = 0.0f;
    bool bLeadPass = false;
    if (bAllowLeadPass)
    {
        if (pPassTarget->ShouldILeadPass())
        {
            nlVector3 suggestedPassDirection;
            nlVector3 suggestedPassTarget;
            float fSearchScore = pPassTarget->SuggestPassTargetPosition(suggestedPassTarget, this, bVolleyPass, bParam3);
            nlVec3Sub(suggestedPassDirection, suggestedPassTarget, pPassTarget->m_v3Position);
            nlVector3* pSuggestedPassDirection = &suggestedPassDirection;
            float distSq2D = pSuggestedPassDirection->GetLengthSq2D();
            if (distSq2D > 1.0f)
            {
                float zSq = pSuggestedPassDirection->z * pSuggestedPassDirection->z;
                float distSq3D = zSq + distSq2D;
                float fRecipDist = nlRecipSqrt(distSq3D, true);
                bLeadPass = false;
                float fRunningSpeed = pPassTarget->m_pTweaks->fRunningSpeed;
                nlVec3Scale(*pSuggestedPassDirection, fRecipDist);
                if (fSearchScore < 0.3f)
                {
                    nlPolarToCartesian(
                        teammateLeadPassVelocity.x,
                        teammateLeadPassVelocity.y,
                        pPassTarget->m_aActualFacingDirection,
                        fRunningSpeed);
                    bLeadPass = true;
                    teammateLeadPassVelocity.z = 0.0f;
                }
                else
                {
                    float fLength = nlSqrt(
                        pSuggestedPassDirection->x * pSuggestedPassDirection->x
                            + pSuggestedPassDirection->y * pSuggestedPassDirection->y
                            + pSuggestedPassDirection->z * pSuggestedPassDirection->z,
                        true);
                    float fScale = fRunningSpeed / fLength;
                    teammateLeadPassVelocity.x = fScale * pSuggestedPassDirection->x;
                    teammateLeadPassVelocity.y = fScale * pSuggestedPassDirection->y;
                    teammateLeadPassVelocity.z = fScale * pSuggestedPassDirection->z;
                }
                if (bVolleyPass || bParam4)
                {
                    const nlVector3& teammatePosition = pPassTarget->m_v3Position;
                    fPassGroundSpeed = DoClampPassSpeed(
                        DoCalculatePassSpeed(
                            teammatePosition,
                            teammateLeadPassVelocity,
                            m_pTweaks->fPassVolleySpeedMin,
                            m_pTweaks->fPassVolleySpeedMax,
                            g_pGame->m_pGameTweaks->fPassSpeedMinDist,
                            g_pGame->m_pGameTweaks->fPassSpeedMaxDist),
                        this);
                }
                else
                {
                    const nlVector3& teammatePosition = pPassTarget->m_v3Position;
                    fPassGroundSpeed = DoClampPassSpeed(
                        DoCalculatePassSpeed(
                            teammatePosition,
                            teammateLeadPassVelocity,
                            m_pTweaks->fPassGroundSpeedMin,
                            m_pTweaks->fPassGroundSpeedMax,
                            g_pGame->m_pGameTweaks->fPassSpeedMinDist,
                            g_pGame->m_pGameTweaks->fPassSpeedMaxDist),
                        this);
                }
                if (!bLeadPass)
                {
                    float dxBall = g_pBall->m_v3Position.x - suggestedPassTarget.x;
                    float dyBall = g_pBall->m_v3Position.y - suggestedPassTarget.y;
                    float distBallToTarget = nlSqrt(dxBall * dxBall + dyBall * dyBall, true);
                    float fDistToBallSave = distBallToTarget;
                    float dxTeam = pPassTarget->m_v3Position.x - suggestedPassTarget.x;
                    float dyTeam = pPassTarget->m_v3Position.y - suggestedPassTarget.y;
                    float distTeammateToTarget = nlSqrt(dxTeam * dxTeam + dyTeam * dyTeam, true);
                    float fTimeBall = distBallToTarget / fPassGroundSpeed;
                    float fRequiredSpeed = distTeammateToTarget / fTimeBall;
                    float fJoggingSpeed = pPassTarget->m_pTweaks->fJoggingSpeed;
                    if (fRequiredSpeed < fJoggingSpeed)
                    {
                        float fMargin = fJoggingSpeed - fRequiredSpeed;
                        if (fMargin < 1.4f)
                        {
                            float fTimeAtJog = distTeammateToTarget / fJoggingSpeed;
                            float fAdjustedSpeed2 = fDistToBallSave / fTimeAtJog;
                            fRequiredSpeed = fJoggingSpeed;
                            if (fAdjustedSpeed2 > 30.0f)
                            {
                                fRequiredSpeed = 0.0f;
                            }
                            else
                            {
                                fPassGroundSpeed = fAdjustedSpeed2;
                            }
                        }
                    }
                    if (fRequiredSpeed >= fJoggingSpeed)
                    {
                        if (fRequiredSpeed <= pPassTarget->m_pTweaks->fRunningSpeed)
                        {
                            v3PassIntercept = suggestedPassTarget;
                            float fVelLength = nlSqrt(
                                teammateLeadPassVelocity.x * teammateLeadPassVelocity.x
                                    + teammateLeadPassVelocity.y * teammateLeadPassVelocity.y
                                    + teammateLeadPassVelocity.z * teammateLeadPassVelocity.z,
                                true);
                            float fVelScale = fRequiredSpeed / fVelLength;
                            nlVec3Scale(teammateLeadPassVelocity, fVelScale);
                            calcPassIntercept = true;
                        }
                        else
                        {
                            bLeadPass = true;
                        }
                    }
                }
                if (bLeadPass)
                {
                    int interceptResult;
                    float interceptTimes[2];
                    CalcInterceptXY(
                        g_pBall->m_v3Position,
                        fPassGroundSpeed,
                        0.0f,
                        pPassTarget->m_v3Position,
                        teammateLeadPassVelocity,
                        interceptResult,
                        interceptTimes);
                    if (interceptResult != 0)
                    {
                        float fTime;
                        if (interceptResult == 2)
                        {
                            fTime = (interceptTimes[0] < interceptTimes[1]) ? interceptTimes[0] : interceptTimes[1];
                        }
                        else
                        {
                            fTime = interceptTimes[0];
                        }
                        v3PassIntercept.x = teammateLeadPassVelocity.x * fTime + pPassTarget->m_v3Position.x;
                        v3PassIntercept.y = teammateLeadPassVelocity.y * fTime + pPassTarget->m_v3Position.y;
                        v3PassIntercept.z = 0.0f;
                        if (!ClipPositionToSidelines(v3PassIntercept, m_pTweaks->fPhysCapsuleRadius))
                        {
                            calcPassIntercept = true;
                        }
                    }
                }
            }
        }
    }
    if (!calcPassIntercept)
    {
        v3PassIntercept = pPassTarget->m_v3Position;
        if (bVolleyPass)
        {
            const nlVector3& teammatePosition = pPassTarget->m_v3Position;
            fPassGroundSpeed = DoClampPassSpeed(
                DoCalculatePassSpeed(
                    teammatePosition,
                    pPassTarget->m_v3Velocity,
                    m_pTweaks->fPassVolleySpeedMin,
                    m_pTweaks->fPassVolleySpeedMax,
                    g_pGame->m_pGameTweaks->fPassSpeedMinDist,
                    g_pGame->m_pGameTweaks->fPassSpeedMaxDist),
                this);
        }
        else
        {
            const nlVector3& teammatePosition = pPassTarget->m_v3Position;
            fPassGroundSpeed = DoClampPassSpeed(
                DoCalculatePassSpeed(
                    teammatePosition,
                    pPassTarget->m_v3Velocity,
                    m_pTweaks->fPassGroundSpeedMin,
                    m_pTweaks->fPassGroundSpeedMax,
                    g_pGame->m_pGameTweaks->fPassSpeedMinDist,
                    g_pGame->m_pGameTweaks->fPassSpeedMaxDist),
                this);
        }
    }
    if (m_pBall != NULL)
    {
        m_pPhysicsCharacter->ReleaseObject();
        m_pBall->ClearOwner();
        m_pBall = NULL;
        float fPossessionTime = m_tBallPossessionTimer.GetSeconds();
        StatsTracker::Track(STATS_POSSESION_TIME, m_pTeam->m_nSide, m_ID, (s32)(100.0f * fPossessionTime), 0, 0, 0);
        if (IsPlayingEffect(fxGetGroup("ball_sts_windup")))
        {
            StopSFX(Audio::CHARSFX_SHOT_WINDUP);
            StopSFX(Audio::CHARSFX_EFFORTS_HEAD_SHAKE);
        }
        KillWindups(this);
        StopSFX(Audio::CHARSFX_KICK_ATTEMPT);
        m_pCharacterSFX->StopMovementLoop();
    }

    float dxIntercept = v3PassIntercept.x - g_pBall->m_v3Position.x;
    float dyIntercept = v3PassIntercept.y - g_pBall->m_v3Position.y;
    fDistToBall = nlSqrt(dxIntercept * dxIntercept + dyIntercept * dyIntercept, true);
    float fPassTime = fDistToBall / fPassGroundSpeed;
    unsigned short facingDirection;

    if (calcPassIntercept)
    {
        s32 facingDirectionTemp = (s32)(nlATan2f(teammateLeadPassVelocity.y, teammateLeadPassVelocity.x) * 10430.378f);
        facingDirection = (u16)facingDirectionTemp;
    }
    else
    {
        s32 facingDirectionTemp = (s32)(nlATan2f(
                                            g_pBall->m_v3Position.y - pPassTarget->m_v3Position.y,
                                            g_pBall->m_v3Position.x - pPassTarget->m_v3Position.x)
                                        * 10430.378f);
        facingDirection = (u16)facingDirectionTemp;
    }

    const LooseBallContactAnimInfo* pAnimInfo = pPassTarget->GetReceivePassBallContactAnimInfo(
        g_pBall,
        v3PassIntercept,
        facingDirection,
        calcPassIntercept,
        bVolleyPass);
    nlVector3 velocity;
    nlVector3 ballContactOffset;
    pPassTarget->GetReceivePassBallContactOffset(ballContactOffset, facingDirection, pAnimInfo);
    eSpinType spinType = SPINTYPE_ROLLING;
    bool bHighArc = false;
    v3PassIntercept.z += ballContactOffset.z;
    v3PassIntercept.y += ballContactOffset.y;
    v3PassIntercept.x += ballContactOffset.x;

    if (bVolleyPass)
    {
        spinType = SPINTYPE_BACK;
        g_pBall->ShootAtFast(velocity, v3PassIntercept, fPassTime);
        if (velocity.z > 14.0f)
        {
            velocity.z = 14.0f;
            bHighArc = true;
        }
    }
    else
    {
        float fInvSpeed = 1.0f / fPassTime;
        float dz = v3PassIntercept.z - g_pBall->m_v3Position.z;
        float dy = v3PassIntercept.y - g_pBall->m_v3Position.y;
        float dx = v3PassIntercept.x - g_pBall->m_v3Position.x;
        velocity.x = fInvSpeed * dx;
        velocity.y = fInvSpeed * dy;
        velocity.z = fInvSpeed * dz;
        if (g_pBall->m_v3Position.z < 0.36f)
        {
            velocity.z = InterpolateRangeClamped(
                0.5f,
                2.0f,
                g_pGame->m_pGameTweaks->fPassSpeedMinDist,
                g_pGame->m_pGameTweaks->fPassSpeedMaxDist,
                fDistToBall);
        }
        else
        {
            velocity.z = 0.0f;
        }
    }
    g_pBall->ShootRelease(velocity, spinType);
    m_pPhysicsCharacter->m_CanCollideWithBall = 0;
    m_tNoPickupTimer.SetSeconds(0.5f);
    if (pPassTarget->CanReceivePass())
    {
        if (!bHighArc)
        {
            Event* pEvent = g_pEventManager->CreateValidEvent(14, 0x24);
            PassBallData* pData = new (&pEvent->m_data) PassBallData();
            pData->pPasser = this;
            pData->pTarget = pPassTarget;
            cGlobalPad* pPad = (m_pController != NULL) ? m_pController->m_pGlobalPad : NULL;
            int controllerID;
            if (pPad != NULL)
            {
                pPad = (m_pController != NULL) ? m_pController->m_pGlobalPad : NULL;
                controllerID = pPad->m_padIndex;
            }
            else
            {
                controllerID = -1;
            }
            pData->mPasserControllerID = controllerID;
            if (calcPassIntercept)
            {
                pPassTarget->InitDesireReceivePassFromRun(pAnimInfo, teammateLeadPassVelocity, bVolleyPass, v3PassIntercept);
            }
            else
            {
                pPassTarget->InitDesireReceivePassFromIdle(pAnimInfo, facingDirection, bVolleyPass);
            }

            if (m_eClassType == FIELDER)
            {
                if (pPasser->m_eActionState != ACTION_ONETOUCH_PASS_FROM_VOLLEY && pPasser->DoCalcCanDoPerfectPass(pPassTarget, v3PassIntercept))
                {
                    g_pBall->SetPerfectPass(true, false);
                    EmitBallShot(this, BALL_EFFECT_PERFECT_PASS, pPassTarget, false);
                }
                else if (bVolleyPass)
                {
                    EmitBallShot(this, BALL_EFFECT_REGULAR_SHOT, NULL, false);
                }
            }
        }
        if (m_eClassType != GOALIE)
        {
            EmitBallPass(this);
        }
    }
    if (m_pController != NULL)
    {
        if (pPassTarget->m_pController == NULL)
        {
            pPassTarget->SetAIPad(m_pController);
            SetAIPad(NULL);
        }
    }
}

/**
 * Offset/Address/Size: 0x990 | 0x80057EE0 | size: 0x80
 */
void cPlayer::ClearPowerupAnimState(bool bIsEndGame)
{
    bool bRemovePowerup = false;
    if ((bIsEndGame != 0) && (m_pPowerupLayer->GetChild(1) != 0))
    {
        bRemovePowerup = 1;
    }

    m_pPowerupLayer->BeginBlendOut(0.1f);
    if (m_eClassType == FIELDER)
    {
        cFielder* pFielder = (cFielder*)this;
        pFielder->m_nPowerupAnimID = -1;
        if (bRemovePowerup != 0)
        {
            pFielder->m_ePowerup = POWER_UP_NONE;
        }
    }
}

/**
 * Offset/Address/Size: 0x978 | 0x80057EC8 | size: 0x18
 */
void cPlayer::ClearSwapControllerTimer()
{
    m_tSwapControllerTimer[0].m_uPackedTime = 0;
    m_tSwapControllerTimer[1].m_uPackedTime = 0;
    m_tSwapControllerTimer[2].m_uPackedTime = 0;
    m_tSwapControllerTimer[3].m_uPackedTime = 0;
}

/**
 * Offset/Address/Size: 0x890 | 0x80057DE0 | size: 0xE8
 */
void cPlayer::SetPowerupAnimState(int animID)
{
    s32 nodeIndex;
    cPN_SAnimController* pController = NewAnimController(animID, false, false, nullptr, 0);
    if (animID == 0x5F || animID == 0x61)
    {
        cSHierarchy* pHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
        nodeIndex = pHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 l clavicle"));
    }
    else
    {
        cSHierarchy* pHierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
        nodeIndex = pHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 r clavicle"));
    }

    m_pPowerupLayer->ClearNodeWeights();
    m_pPowerupLayer->SetNodeWeight(nodeIndex, 1.0f, 0.2f);
    m_pPowerupLayer->SetChild(1, pController);
    m_pPowerupLayer->BeginBlendIn(0.067f);
}

/**
 * Offset/Address/Size: 0x858 | 0x80057DA8 | size: 0x38
 */
void cPlayer::CollideWithWallCallback(const CollisionPlayerWallData* pData)
{
    if (this != g_pBall->m_pOwner)
    {
        return;
    }

    if (m_eBallRotationMode != BRM_ANIMATED)
    {
        return;
    }

    m_eBallRotationMode = BRM_MATCH_VELOCITY;

    if (m_pBall != NULL)
    {
        m_ResetBaseBallOrientation = true;
    }
}

/**
 * Offset/Address/Size: 0x718 | 0x80057C68 | size: 0x140
 */
cPN_SingleAxisBlender* cPlayer::CreateSingleAxisBlender(
    const int* pSABAnims,
    int nNumSABAnims,
    int nPrimaryAnim,
    void (*fWeightCB)(uintptr_t, cPN_SingleAxisBlender*),
    float fWeightSeek,
    cPN_SAnimController* pSynchingController)
{
    cPN_SAnimController* pNewCurrentAnimController;
    pNewCurrentAnimController = NULL;

    cPN_SingleAxisBlender* pSAB = ::new (AllocateSingleAxisBlender()) cPN_SingleAxisBlender(nNumSABAnims, fWeightCB, (uintptr_t)this, fWeightSeek);

    const int* pAnims = pSABAnims;
    for (int i = 0; i < nNumSABAnims; i++)
    {
        cPN_SAnimController* pNewController = NewAnimController(*pAnims, false, false, NULL, 0);
        if (pSynchingController != NULL)
        {
            pNewController->m_bIsSynchronized = true;
            pSynchingController->m_pSynchronizedController = pNewController;
            pSynchingController = pNewController;
        }
        pSAB->SetChild(i, pNewController);
        if (i == nPrimaryAnim)
        {
            pNewCurrentAnimController = pNewController;
        }
        pAnims++;
    }

    SetAnimID(pSABAnims[nPrimaryAnim]);
    m_pCurrentAnimController = pNewCurrentAnimController;

    return pSAB;
}

/**
 * Offset/Address/Size: 0x5D0 | 0x80057B20 | size: 0x148
 */
void cPlayer::PlayerHeadTrackCallback(uintptr_t nSelf, unsigned int nParam2, cPoseAccumulator* pPoseAccumulator,
    unsigned int nJointIndex, int nParentIndex)
{
    cPlayer& self = *(cPlayer*)(void*)nSelf;
    nlMatrix4 m4HeadMatrix;

    nlMultMatrices(m4HeadMatrix, pPoseAccumulator->GetNodeMatrix(nJointIndex), self.m_m4WorldMatrix);

    int nHeadSpinMax = g_nHeadSpinMax;
    int nHeadTiltMax = g_nHeadTiltMax;
    eCharacterClass cc = self.m_eCharacterClass;

    if (cc == WARIO)
    {
        nHeadSpinMax = g_nWarioHeadSpinMax;
        nHeadTiltMax = g_nWarioHeadTiltMax;
    }
    else if (cc == WALUIGI)
    {
        nHeadSpinMax = g_nWaluigiHeadSpinMax;
    }

    eClassTypes classType = self.m_eClassType;
    float fSmoothTime;
    if (classType == GOALIE)
    {
        fSmoothTime = 0.1f;
    }
    else
    {
        fSmoothTime = 0.1f;
    }

    if (classType == GOALIE)
    {
        self.m_pHeadTrack->Update(m4HeadMatrix, m4HeadMatrix, g_fFixedUpdateTick, g_aOOIConstraint, nHeadSpinMax, nHeadTiltMax, fSmoothTime);

        cHeadTrack* pHeadTrack = self.m_pHeadTrack;
        u16 headSpin = (u16)(int)pHeadTrack->m_fHeadSpin;
        u16 headTilt = (u16)(int)pHeadTrack->m_fHeadTilt;
        CalcHeadTrackMatrix(headSpin, headTilt, pPoseAccumulator, nJointIndex);
    }
    else
    {
        nlMatrix4 m4ConstraintMatrix;
        nlMultMatrices(m4ConstraintMatrix, pPoseAccumulator->GetNodeMatrix(self.m_nSpine1JointIndex), self.m_m4WorldMatrix);

        self.m_pHeadTrack->Update(m4HeadMatrix, m4HeadMatrix, g_fFixedUpdateTick, g_aOOIConstraint, nHeadSpinMax, nHeadTiltMax, fSmoothTime);
    }
}

/**
 * Offset/Address/Size: 0x488 | 0x800579D8 | size: 0x148
 */
void cPlayer::PrePhysicsUpdate(float dt)
{
    m_pPoseAccumulator->SetBuildNodeMatrixCallback(m_nHeadJointIndex, PlayerHeadTrackCallback, (uintptr_t)this, 0);

    bool poseLocal = false;
    if (m_eClassType != GOALIE || m_v3Position.x * g_pBall->m_v3Position.x > 0.0f)
    {
        poseLocal = true;
    }

    if (poseLocal)
    {
        PoseLocalSpace();
    }

    m_pPoseAccumulator->SetBuildNodeMatrixCallback(m_nHeadJointIndex, NULL, 0, 0);

    if (m_pBall != NULL)
    {
        m_pBall->m_pPhysicsBall->EnableCollisions();
        m_pPhysicsCharacter->ContainObject(m_pBall->m_pPhysicsBall);
    }

    if (m_pBall != NULL)
    {
        nlVector3 jointPos = GetJointPosition(m_nBallJointIndex);
        float physZ = m_pPhysicsCharacter->GetPosition().z;
        jointPos.z -= physZ;
        m_pPhysicsCharacter->m_SubObject.SetSubObjectPosition(jointPos, PhysicsObject::RELATIVE_TO_PARENT);
    }

    if (poseLocal)
    {
        m_pPhysicsCharacter->UpdatePose(m_pPoseAccumulator, m_v3Position.z);
    }
}

/**
 * Offset/Address/Size: 0x454 | 0x800579A4 | size: 0x34
 */
void cPlayer::PreUpdate(float dt)
{
    cCharacter::PreUpdate(dt);
    m_bCanTestController = true;
}

/**
 * Offset/Address/Size: 0x434 | 0x80057984 | size: 0x20
 */
void cPlayer::PostPhysicsUpdate()
{
    cCharacter::PostPhysicsUpdate();
}

/**
 * Offset/Address/Size: 0x284 | 0x800577D4 | size: 0x1B0
 */
bool cPlayer::CanPickupBall(cBall* pBall)
{
    cPlayer* self = this;

    if (sbNoBallPickups)
    {
        return false;
    }

    if (!g_pGame->IsGameplayOrOvertime())
    {
        return false;
    }

    bool bDoPickUp = false;
    float speedSq = pBall->m_v3Velocity.x * pBall->m_v3Velocity.x
                  + pBall->m_v3Velocity.y * pBall->m_v3Velocity.y
                  + pBall->m_v3Velocity.z * pBall->m_v3Velocity.z;

    if (pBall->m_pOwner == NULL && pBall->m_tNoPickupTimer.m_uPackedTime == 0)
    {
        if (speedSq <= 100.0f || pBall->m_pPassTarget == self)
        {
            if (self->m_tNoPickupPassInterceptTimer.m_uPackedTime == 0
                && self->m_tNoPickupTimer.m_uPackedTime == 0
                && self->m_eAnimID != 0x12 && self->m_eAnimID != 0x13)
            {
                float fPhysicsRadius = 0.0f;
                self->m_pPhysicsCharacter->m_pPlayerPlayerColumn->GetRadius(&fPhysicsRadius);

                nlVector3 v3PrevPlayerPos = self->m_v3PrevPosition;
                nlVector3 v3PlayerPos = self->m_v3Position;
                v3PlayerPos.z = 0.18f;
                v3PrevPlayerPos.z = 0.18f;

                u8 bCollision = TestCollision(fPhysicsRadius, v3PrevPlayerPos, v3PlayerPos, 0.18f, pBall->m_v3PrevPosition, pBall->m_v3Position);
                bDoPickUp = false;
                if (bCollision)
                {
                    s16 delta;
                    delta = self->GetFacingDeltaToPosition(g_pBall->m_v3Position);
                    u16 absDelta = (u16)((delta < 0) ? -delta : delta);
                    if (absDelta < 0x4000)
                    {
                        bDoPickUp = true;
                    }
                }
            }
        }
    }

    return bDoPickUp;
}

/**
 * Offset/Address/Size: 0x1F4 | 0x80057744 | size: 0x90
 */
bool cPlayer::CanPickupBallFromPass(cBall* pBall)
{
    bool result = false;
    if (pBall->m_pOwner == NULL)
    {
        int jointIdx = m_nBallJointIndex;
        result = TestCollision(
            0.18f,
            GetPrevJointPosition(jointIdx),
            GetJointPosition(jointIdx),
            0.18f,
            pBall->m_v3PrevPosition,
            pBall->m_v3Position);
    }
    return result;
}

/**
 * Offset/Address/Size: 0x114 | 0x80057664 | size: 0xE0
 */
nlVector3 cPlayer::GetAIOffNetLocation(const nlVector3* v3ReferencePos)
{
    nlVector3 v3NetLocation = m_pTeam->GetOtherNet()->m_v3NetLocation;
    float yCoord = (v3ReferencePos != NULL) ? v3ReferencePos->y : m_v3Position.y;

    float fNetWidth = 0.5f * cNet::m_fNetWidth;

    if (yCoord < 0.0f)
    {
        yCoord = max_float(yCoord, -1.0f * fNetWidth);
        v3NetLocation.y = yCoord;
    }
    else
    {
        yCoord = min_float(yCoord, fNetWidth);
        v3NetLocation.y = yCoord;
    }

    return v3NetLocation;
}

/**
 * Offset/Address/Size: 0x68 | 0x800575B8 | size: 0xAC
 */
nlVector3 cPlayer::GetAIDefNetLocation(const nlVector3* v3ReferencePos)
{
    nlVector3 v3NetLocation = m_pTeam->m_pNet->m_v3NetLocation;
    float yCoord = (v3ReferencePos != NULL) ? v3ReferencePos->y : m_v3Position.y;

    float fNetWidth = 0.5f * cNet::m_fNetWidth;

    if (yCoord < 0.0f)
    {
        yCoord = max_float(yCoord, -1.0f * fNetWidth);
        v3NetLocation.y = yCoord;
    }
    else
    {
        yCoord = min_float(yCoord, fNetWidth);
        v3NetLocation.y = yCoord;
    }

    return v3NetLocation;
}

/**
 * Offset/Address/Size: 0x20 | 0x80057570 | size: 0x48
 */
void cPlayer::SetNoPickUpTime(float NewNoPickUpTime)
{
    m_pPhysicsCharacter->m_CanCollideWithBall = (NewNoPickUpTime <= 0.0f);
    m_tNoPickupTimer.SetSeconds(NewNoPickUpTime);
}

/**
 * Offset/Address/Size: 0x0 | 0x80057550 | size: 0x20
 */
int cPlayer::GetUniqueID(int nTeamID) const
{
    if (nTeamID == -1)
    {
        nTeamID = *(s32*)m_pTeam;
    }
    nTeamID *= 5;
    return nTeamID + m_ID;
}

bool cPlayer::SuggestPassDirection(nlVector3& suggestedDirection, cPlayer* fromPlayer, bool volleyPass, bool bIsPerfectPass)
{
    nlVector3 bestOpenPosition;
    float bestPositionScore = SuggestPassTargetPosition(bestOpenPosition, fromPlayer, volleyPass, bIsPerfectPass);

    nlVec3Sub(suggestedDirection, bestOpenPosition, m_v3Position);
    bool leadPass = bestPositionScore < 0.3f;
    float distanceSquared2D = suggestedDirection.GetLengthSq2D();
    if (distanceSquared2D > 1.0f)
    {
        float zSquared = suggestedDirection.z * suggestedDirection.z;
        float reciprocalDistance = nlRecipSqrt(zSquared + distanceSquared2D, true);
        nlVec3Scale(suggestedDirection, reciprocalDistance);
        if (leadPass)
        {
            nlPolarToCartesian(suggestedDirection.x, suggestedDirection.y, m_aActualFacingDirection, m_pTweaks->fRunningSpeed);
            suggestedDirection.z = 0.0f;
        }
        else
        {
            float length = nlSqrt(
                suggestedDirection.x * suggestedDirection.x
                    + suggestedDirection.y * suggestedDirection.y
                    + suggestedDirection.z * suggestedDirection.z,
                true);
            float scale = m_pTweaks->fRunningSpeed / length;
            nlVec3Scale(suggestedDirection, scale);
        }
    }
    else
    {
        leadPass = false;
    }

    return leadPass;
}

float cPlayer::SuggestPassTargetPosition(nlVector3& suggestedTarget, cPlayer* fromPlayer, bool volleyPass, bool bIsPerfectPass)
{
    eFieldDirection passDirection = (eFieldDirection)Fuzzy::GetPassDirection(fromPlayer, this).mData.i;
    SSearchBestPass* search = new (nlMalloc(sizeof(SSearchBestPass), 8, false)) SSearchBestPass(fromPlayer, this, volleyPass, bIsPerfectPass);
    if (m_pSpaceSearch != NULL)
    {
        delete m_pSpaceSearch;
    }
    m_pSpaceSearch = search;
    m_pSpaceSearch->m_bDebugOn = false;
    return m_pSpaceSearch->FindBestPosition(suggestedTarget, m_v3Position, passDirection, &fromPlayer->m_v3Position, 6.0f, 0xAAAA);
}
