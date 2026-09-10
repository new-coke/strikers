#ifndef _FIELDER_H_
#define _FIELDER_H_

#include "types.h"
#include "NL/nlMath.h"

#include "Game/Net.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "Game/AI/Fuzzy.h"
#include "Game/AI/Powerups.h"
#include "Game/AI/ScriptAction.h"

enum eTurboRequest
{
    TR_FAR_DISTANCE = 0,
    TR_MOVING_TARGET = 1,
    TR_FORCED_ON = 2,
    TR_FORCED_OFF = 3,
};

enum eFielderActionState
{
    ACTION_NEED_ACTION = -1,
    ACTION_DEKE = 0,
    ACTION_ELECTROCUTION = 1,
    ACTION_HIT = 2,
    ACTION_HIT_REACT = 3,
    ACTION_IDLE_TURN = 4,
    ACTION_LATE_ONETIMER_FROM_VOLLEY = 5,
    ACTION_LOOSE_BALL_PASS = 6,
    ACTION_LOOSE_BALL_SHOT = 7,
    ACTION_ONETIMER = 8,
    ACTION_ONETOUCH_PASS_FROM_VOLLEY = 9,
    ACTION_PASS = 10,
    ACTION_POST_WHISTLE = 11,
    ACTION_RECEIVE_PASS = 12,
    ACTION_RUNNING = 13,
    ACTION_RUNNING_WB = 14,
    ACTION_RUNNING_WB_TURBO = 15,
    ACTION_RUNNING_WB_TURBO_TURN = 16,
    ACTION_SHOT = 17,
    ACTION_SHOOT_TO_SCORE = 18,
    ACTION_SLIDE_ATTACK = 19,
    ACTION_SLIDE_ATTACK_REACT = 20,
    ACTION_BOMB_REACT = 21,
    ACTION_SHELL_REACT = 22,
    ACTION_BANANA_REACT = 23,
    ACTION_STS_HIT_REACT = 24,
    ACTION_SQUISH_REACT = 25,
    ACTION_SLIDE_FAIL_REACT = 26,
    ACTION_WAIT = 27,
    NUM_FIELDER_ACTIONS = 28,
};

enum eAwardPowerupType
{
    AWARD_POWERUP_POWER_SHOT = 0,
    AWARD_POWERUP_INTERCEPT_PASS = 1,
    AWARD_POWERUP_PERFECT_PASS = 2,
    AWARD_POWERUP_CONTEXT_DEKE = 3,
    NUM_AWARD_POWERUP_TYPES = 4,
};

enum ePenaltyType
{
    PEN_TYPE_HIT_WITH_BALL = 0,
    PEN_TYPE_HIT_NO_BALL = 1,
    PEN_TYPE_SLIDE_WITH_BALL = 2,
    PEN_TYPE_SLIDE_NO_BALL = 3,
    NUM_PEN_TYPES = 4,
};

enum ePenaltyCardStatus
{
    PENALTY_CARD_NONE = -1,
    PENALTY_CARD_YELLOW_1 = 0,
    PENALTY_CARD_YELLOW_2 = 1,
    PENALTY_CARD_RED = 2,
    NUM_PENALTY_CARDS = 3,
};

enum eStrafeDirection
{
    STRAFE_IDLE = 0,
    STRAFE_RIGHT = 1,
    STRAFE_LEFT = 2,
    STRAFE_FORWARD = 3,
    STRAFE_BACK = 4,
};

enum eRole
{
    ROLE_STRIKER = 0,
    ROLE_WINGER = 1,
    ROLE_MIDFIELD = 2,
    ROLE_DEFENCE = 3,
    NUM_ROLES = 4,
};

enum eShootToScoreResult
{
    S2S_SAVED = 0,
    S2S_SAVED_YELLOW = 1,
    S2S_SCORE = 2,
    S2S_SUPER_SHOT = 3,
};

enum eSlideAttackState
{
    SLIDE_ATTACK_DOWN = 0,
    SLIDE_ATTACK_DECELERATE = 1,
};

struct ActionShootToScoreVars
{
    ActionShootToScoreVars() { memset(this, 0, sizeof(ActionShootToScoreVars)); }
    /* 0x00 */ float fFrameButtonDownTime2;
    /* 0x04 */ float fFrameButtonDownTime1;
    /* 0x08 */ float fGreenRegionWidth;
    /* 0x0C */ float fShootToScoreActiveTime;
    /* 0x10 */ float fMeterFractionTime;
    /* 0x14 */ nlVector3 v3MeterPosition;
    /* 0x20 */ bool bShootWasPressed;
    /* 0x21 */ bool isCaptainSts;
    /* 0x22 */ bool isCurrentlyInvincible;
    /* 0x23 */ bool isInUnbreakablePart;
    /* 0x24 */ cAnimCamera* captainStsCamera;
    /* 0x28 */ float preCaptainStsPlaybackSpeed;
    /* 0x2C */ float fCaptainYellowWidth;
}; // total size: 0x30

struct LooseBallContactAnimInfo
{
    /* 0x0 */ s32 nAnimID;
    /* 0x4 */ float fAnimContactFrame;
    /* 0x8 */ u16 aIncomingAngleMin;
    /* 0xA */ u16 aIncomingAngleMax;
}; // total size: 0xC

class cBall;
class ChainChomp;
class Bowser;
class AIPlay;
class AvoidController;
class ShotMeter;
class Goalie;

class cFielder : public cPlayer
{
public:
    cFielder(int nPlayerID, int nTeamID, eCharacterClass cc, const int* nModelID, cSHierarchy* pHierarchy, cAnimInventory* pAnimInventory,
        const CharacterPhysicsData* pCharacterPhysicsData, FielderTweaks* pCharTweaks, AnimRetargetList* pAnimRetargetList);
    /* 0x08 */ virtual ~cFielder();

    /* 0x0C */ virtual void PostPhysicsUpdate();
    /* 0x10 */ // virtual void PrePhysicsUpdate(float);
    /* 0x14 */ virtual void PreUpdate(float fTime);
    /* 0x18 */ // virtual void SetAnimID(int);
    /* 0x1C */ virtual void SetPosition(const nlVector3& v3Position);
    /* 0x20 */ virtual void Update(float fDeltaT);
    /* 0x24 */ virtual bool CanPickupBall(cBall* pBall);
    /* 0x28 */ // virtual void CollideWithBallCallback(cBall*);
    /* 0x2C */ virtual void CollideWithCharacterCallback(CollisionPlayerPlayerData* pData);
    /* 0x30 */ virtual void CollideWithWallCallback(const CollisionPlayerWallData* eventData);
    /* 0x34 */ // virtual void CollideWithWallCallback(const CollisionPlayerWallData*);
    /* 0x38 */ virtual void InitActionPostWhistle();

    void AbortPlay();
    void AbortPendingThoughts();
    void CalculateNewDesire();
    float CalcJogRunBlendWeight() const;
    float CalcRunTurboBlendWeight() const;
    bool CanGetElectrocuted(const CollisionPlayerWallData* eventData);
    bool CanDoCaptainShootToScore();
    bool CanLooseBallShoot();
    bool CanLooseBallPass();
    bool CanPassTargetAttemptOneTouch(cFielder* pPassTarget);
    bool CanReceivePass();
    void SetMark(cFielder* pMark);
    cFielder* GetMark() const { return m_pMark; }
    cFielder* GetMarker() const { return m_pMarker; }
    bool CollideWithShellCallback(ePowerupSize eSize, bool bUnknown, const nlVector3& rv3Pos1, const nlVector3& rv3Pos2);
    void CollideWithSidelineFragmentCallback(const nlVector3& v3CollisionLocation, const nlVector3& v3CollisionVelocity);
    bool CollideWithFreezeCallback();
    bool CollideWithBananaCallback(const nlVector3& rv3BananaPosition);
    void CollideWithBobombCallback(const nlVector3& v3CollisionLocation, float fBombRadius);
    void CollideWithChainCallback(ChainChomp* pChainChomp);
    void CollideWithBowserCallback(Bowser* pBowser);
    void ClearPassTargetIfAmThePassTarget();
    bool UsePerfectPass();
    bool IsPlayingPowerupAnim();
    bool IsCharacterInAir(bool bUseOffset) const;
    bool IsTurboing();
    bool IsRunning() const;
    bool IsRunningWithBall() const;
    bool IsInvincible() const;
    bool IsBallAwayFromCarrier() const;
    bool IsReceivingVolleyPass() const;
    bool IsPreparingForOneTimer() const;
    void CleanUpAction();
    void CleanUpPowerupEffect();
    float GetShotProbability(float fValue, Goalie* pGoalie);
    void CalcRegularShot(nlVector3& rv3Vel, nlVector3& rv3Target);
    void CalcShootToScoreShot(nlVector3& v3BallVelocity, nlVector3& v3BallTarget);
    void SetSlideAttackSuccessFlag();
    void SetKickOffWaitTime();
    void SetBombImpactTime(const nlVector3& v3BombImpactLocation, float fBombImpactRadius);
    void SetDesireDuration(float fNewDuration, bool bRandomVariation);
    void ShootBallDueToContact(const nlVector3& v3IncomingVelocity);
    void ShootBallDueToContact(unsigned short aShootDirection);
    void DoClearBall();
    void DoHandleActiveShotMeter();
    bool DoLooseBallContactFromIdle(nlVector3& v3AnimStartPosition, float& fAnimStartTime, nlVector3& v3BallContactPosition, float& fBallContactTime,
        unsigned short aFutureFacingDirection, const LooseBallContactAnimInfo* pBestBallContactAnimInfo);
    bool DoLooseBallContactFromRun(nlVector3& v3AnimStartPosition, float& fAnimStartTime, nlVector3& v3BallContactPosition, float& fBallContactTime,
        const LooseBallContactAnimInfo* pBestBallContactAnimInfo, const nlVector3& v3PassIntercept);
    bool DoLooseBallContactFromRunVolley(nlVector3& v3AnimStartPosition, float& fAnimStartTime, nlVector3& v3BallContactPosition, float& fBallContactTime,
        const LooseBallContactAnimInfo* pBestBallContactAnimInfo, const nlVector3& v3PassIntercept);
    void DoPenaltyCardBooking(cFielder* pFoulee, ePenaltyType eType);
    void DoPositioningInterceptBall();
    void DoAwardPowerupStuff(eAwardPowerupType eType, float fAmountOfAward);
    void DoCalcShootToScoreResult(float fPerfectJumpTime, float fPerfectReleaseTime, float fActualJumpTime, float fActualReleaseTime, float fGreenWidth);
    cFielder* DoFindBestHitTarget();
    void DoFindBestShotTarget(nlVector3& v3PositionOut, float& fShotSpeed, bool bIsSTS);
    void DoRegularShooting();
    void DoDebugShooting();
    void DoResetShotMeter(float fTime);
    bool IsActionDone() const;
    void SetAction(eFielderActionState actionState);
    void InitActionSlideAttackReact(cPlayer* pAttacker, bool bSkipEvent);
    bool GetFormationPosition(nlVector3& v3DestPosition, float fBallPosFormationWeight);
    LooseBallContactAnimInfo* GetOneTimerBallContactAnimInfo(unsigned short aFutureFacingDirection, const nlVector3& v3FuturePosition,
        const nlVector3& v3OneTimerTarget, bool bLeadPass, bool bVolleyPass);
    const LooseBallContactAnimInfo* GetReceivePassBallContactAnimInfo(cBall* pBall, const nlVector3& rv3Pos, unsigned short aAngle, bool bLeadPass,
        bool bVolleyPass);
    void GetReceivePassBallContactOffset(nlVector3& v3Offset, unsigned short aFacingDirection, const LooseBallContactAnimInfo* pBestBallContactAnimInfo);
    bool IsFallenDown(float fThreshold) const;
    bool HasNoDesire() const;
    bool IsHitting() const;
    bool IsSlideTackling() const;
    bool IsStriker() const;
    bool IsWinger() const;
    bool IsMidField() const;
    bool IsDefense() const;
    bool IsFrozen() const;
    inline float GetRunningDistance(float fTime) const
    {
        return fTime * m_pTweaks->fRunningSpeed;
    }
    inline float GetPowerupUsageRandomChance(int ePowerup) const
    {
        cTeam* pTeam = m_pTeam;
        eSituation situation = pTeam->mpCurrentSituation;
        SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(pTeam->m_nSide);
        return RandomChance(pSkillTweaks->PowerupUsageChance[situation][ePowerup]);
    }
    inline unsigned char IsAttacking() const
    {
        unsigned char bAttacking = false;
        if (m_eActionState == ACTION_SLIDE_ATTACK || m_eActionState == ACTION_HIT)
        {
            bAttacking = true;
        }
        return bAttacking;
    }
    void SetFrozen(float seconds);
    float DoFindBestSlideAttackTarget(nlVector3& v3PositionOut, nlVector3& v3VelocityOut);
    bool CanBeBlownUp();
    void CanBreakOutOfSlideTackle();
    eStrafeDirection CalculateStrafeDirection(unsigned short aDesiredFacingDirection, unsigned short aDesiredMovementDirection);
    void CalcPointOnPerimeter(nlVector3& dest, const nlVector3& fromPoint, float fFutureTimeDelta);
    void ClearTimers();
    void ClearVolleyPass();
    void CleanActionShootToScore();
    void CleanActionHit();
    void CleanActionDeke();
    void CleanActionElectrocution();
    void CleanActionLooseBallPass();
    void CleanActionLooseBallShot();
    void CleanActionOnetimer();
    void CleanActionPass();
    void CleanActionPostWhistle();
    void CleanActionRunning();
    void CleanActionRunningWB();
    void CleanActionRunningWBTurbo();
    void CleanActionShot();
    void CleanActionSlideAttack();
    void CleanActionSquishReact();
    void SetAttemptOneTouchPass();
    void SetAttemptOneTouchShot();
    s16 GetOneTouchShotDesire();
    void SetStartAnimState(int animState);
    void SetWindupWBAnimState();
    void SetStartWBAnimState();
    void SetRunTurboAnimState(int animID, bool bForceMirrorSwap);
    void SetHardStopAnimState();
    void SetHardStopRecoverAnimState();
    void SetHardStopTurnAnimState();
    void SetRunBackwardsAnimState();
    void SetRunToBackRunningAnimState();
    void SetBackRunningToRunAnimState();
    void SetBackRunningStopAnimState();
    void SetBackRunningStopStartAnimState();
    void SetBackRunningStopRecoverAnimState();
    void SetStopAnimState();
    void SetStopWBAnimState();
    void SetStrafeLeftAnimState();
    void SetStrafeRightAnimState();
    void SetIdleStrafeAnimState();
    void SetIdleAnimState();
    void SetIdleWBAnimState();
    static void JogRunSynchronizedWeightCallback(uintptr_t nParam, cPN_SAnimController* pController);
    static void JogRunSABcallback(uintptr_t nParam1, cPN_SingleAxisBlender* pSAB);
    static void RunningSABcallback(uintptr_t nParam1, cPN_SingleAxisBlender* pSAB);
    void SetRunLeanSAB(const int* pSABAnims, int nNumSABAnims, int nPrimaryAnim);
    void SetJogRunLeanSAB(
        const int* pRunningAnims, int nNumRunningAnims, int nPrimaryRunningAnim, int nJogAnim, float fBlendTime, float fWeightSeek);
    void SetRunningAnimState(float fBlendTime);
    void SetRunningTurboAnimState();
    void SetRunningWBAnimState(float fBlendTime);
    bool ShouldIClearBall();
    bool ShouldILeadPass();
    bool CanISlideAttack(const nlVector3& v3Position, const nlVector3& v3Velocity, float* fTime);
    void SetDesiredSpeedAndDirectionToPosition(float fDeltaT, const nlVector3& v3Pos, eTurboRequest turboRequest, float fInRadiusMult,
        float fOutRadiusMult);
    void SetDesiredSpeed(float fMinSpeed, float fMaxSpeed);
    float GetSpeedPowerupAdjusted(float fSpeed);
    float GetSlideAttackSpeed();
    bool SetDesire(eFielderDesireState eNewDesire, float fConfidence);
    u8 ShouldIStrafe();
    bool ShouldITurboWithBall();
    bool ShouldITurboWithoutBall();
    void ShouldIWave();
    void TestCollisionForInvicibility(cFielder* pOpponent);
    void TestButtonsToQueueActions(float fTime);
    bool TestQueuedActions();
    void TestOneTimerBallContact();
    bool ShouldHoldShotMeter();
    void TestButtonsRunning();
    void TestButtonsRunningWB(float fTime);
    void ThrowPowerup();
    ePowerUpType GetPowerupType() const { return m_ePowerup; }
    void SetPowerup(ePowerUpType eNewPowerup, int nnumOfPowerups, cFielder* pTarget);
    void UseTeamPowerup(cFielder* pTarget);
    void UpdateActionState(float dt);
    void UpdateHeadTracking(float fDeltaT);
    void UpdateController(float fDeltaT);
    void UpdateMovementLoopSFX();
    void UpdateTimers(float fDeltaT);
    void UpdatePlay(float fTime);
    nlVector3 GetAIDesiredPosition();
    float GetDistanceToDesiredPos();
    bool S2SShootWasPressed();
    void StartRunning();
    bool DoAILooseBallActionSelection();
    bool DoAIReceivePassActionSelection();
    bool DoAIWindupActionSelection();
    void DoSpeedBoost();

    void DesireCutAndBreak(float fDeltaT);
    bool InitDesireCutAndBreak();
    void DesireWindupShot(float fDeltaT);
    void InitDesireWindupShot();
    void DesireWindupPass(float fDeltaT);
    bool InitDesireWindupPass(cPlayer* pTarget, bool bHighPass);
    void DesireUsePowerup(float fDeltaT);
    void InitDesireUsePowerup(ePowerUpType ePowerup, cFielder* pTarget);
    void DesireHit(float fDeltaT);
    void InitDesireHit(cFielder* pTarget);
    void DesireWait(float fDeltaT);
    void InitDesireWait();
    void DesireUserControlled(float fDeltaT);
    void InitDesireUserControlled();
    void InitDesireSupportBallOffensive();
    void InitDesireSupportBallDefensive();
    void DesireSlideAttack(float fDeltaT);
    void InitDesireSlideAttack(cFielder* pTarget);
    void DesireRunToLocation(float fDeltaT);
    void InitDesireRunToLocation(const nlVector3& v3DesiredLocation, bool bTurbo);
    void DesireRunDownField(float fDeltaT);
    void InitDesireRunDownField();
    void DesireRunUpField(float fDeltaT);
    void InitDesireRunUpField();
    void DesireDeke(float fDeltaT);
    void InitDesireDeke();
    void DesireShoot(float fDeltaT);
    void InitDesireShoot(bool bShootToScore, bool bChipShot);
    void DesirePass(float fDeltaT);
    void InitDesirePass(cPlayer* pTarget, bool bHighPass);
    void DesireRunToNet(float fDeltaT);
    bool InitDesireRunToNet();
    void DesireReceivePassFromRun(float fDeltaT);
    void InitDesireReceivePassFromRun(const LooseBallContactAnimInfo* pAnimInfo, const nlVector3& rv3Velocity, bool bVolley, const nlVector3& v3PassIntercept);
    void DesireReceivePassFromIdle(float fDeltaT);
    void InitDesireReceivePassFromIdle(const LooseBallContactAnimInfo* pAnimInfo, unsigned short aAngle, bool bVolley);
    void DesireOneTimer(float fDeltaT);
    bool InitDesireOneTimerFromRun(unsigned short aFutureFacingDirection, const nlVector3& v3FuturePosition, const nlVector3& v3PassIntercept, bool bVolleyPassReceive, bool bIsChipShot);
    bool InitDesireOneTimerFromIdle(bool bVolleyPass, bool bIsChipShot);
    void DesirePostWhistle(float fDeltaT);
    void InitDesirePostWhistle();
    void DesireGetOpen(float fDeltaT);
    bool InitDesireGetOpen();
    void DesireGetInPosition(float fDeltaT);
    void InitDesireGetInPosition();
    void InitDesireFallThrough();
    void DesireSupportBall(float fDeltaT, bool bDefensive);
    void DesireProtectBall(float fDeltaT);
    void InitDesireProtectBall();
    void DesireMark(float fDeltaT);
    void InitDesireMark();
    void DesireFallThrough(float fDeltaT);
    void DesireInterceptBall(float fDeltaT);
    void InitDesireInterceptBall();
    void CleanDesireReceivePassFromRun();
    void CleanDesireReceivePassFromIdle();
    void CleanDesireOneTimer();
    void CleanUpDesire(eFielderDesireState eNewDesireState);
    void EndDesire(bool bCheckTimer);
    void UpdateDesireState(float fDeltaT);
    bool InitDesire(eFielderDesireState eDesireType, float fConfidence, float fDuration, FuzzyVariant opt1, FuzzyVariant opt2);
    bool InitDesire(const sDesireParams* pParams, float fConfidence);
    void ClearQueuedDesire();
    void QueueDesire(eFielderDesireState eDesireType, float fDuration, FuzzyVariant opt1, FuzzyVariant opt2);

    // actions
    void asmRunning();
    void asmRunningWB(float fDeltaT);
    void EndAction();
    bool InitAction(eFielderActionState eAction, FuzzyVariant vOpt1, FuzzyVariant vOpt2);
    void InitActionDeke(ePadActions padAction);
    void ActionDeke(float dt);
    void InitActionElectrocution(const nlVector3& wallPosition, const nlVector3& wallNormal);
    void ActionElectrocution(float dt);
    void InitActionHit(cFielder* pTarget);
    void ActionHit(float fDeltaT);
    bool InitActionHitReact(cPlayer* pAttacker, unsigned short desiredFacingDirection, bool bDoFrameLock);
    void ActionHitReact(float fDeltaT);
    void InitActionIdleTurn(unsigned short desiredFacingDirection);
    void ActionIdleTurn(float fDeltaT);
    void InitActionLateOneTimerFromVolley();
    void ActionLateOneTimerFromVolley(float fDeltaT);
    void DoCommonInitActionLooseBall(const nlVector3& rv3OneTimerTarget);
    void InitActionLooseBallPass(cFielder* pPassTarget, bool bVolleyPass);
    void ActionLooseBallPass(float fDeltaT);
    void InitActionLooseBallShot(bool bIsChipShot);
    void ActionLooseBallShot(float fDeltaT);
    void InitActionOneTimer(int animID, nlVector3& targetPos, float fAdjustEndTime, bool bIsChipShot);
    void ActionOneTimer(float fDeltaT);
    void InitActionOneTouchPassFromVolley(cPlayer* pPlayer);
    void ActionOneTouchPassFromVolley(float fDeltaT);
    bool DoCalcCanDoPerfectPass(cFielder* pPassTarget, const nlVector3& v3PassPosition);
    void InitActionPass(cPlayer* pPassTarget, bool bVolleyPass, bool bAllowLeadPass);
    void ActionPass(float fDeltaT);
    void ActionPostWhistle(float fDeltaT);
    void InitActionBombReact(const nlVector3& v3BombPosition, float fRadius);
    void InitActionBombHitReact(const nlVector3& v3BombPosition);
    void InitActionBananaReact(const nlVector3& fDeltaT);
    void InitActionShellReact(const nlVector3& v3CollisionLocation, const nlVector3& v3CollisionVelocity);
    void InitActionRunning();
    void ActionRunning(float dt);
    void InitActionRunningWB(bool bWaitForAnimToFinish);
    void ActionRunningWB(float dt);
    void InitActionRunningWBTurbo();
    void ActionRunningWBTurbo(float fDeltaT);
    void InitActionRunningWBTurboTurn();
    void ActionRunningWBTurboTurn(float fDeltaT);
    void InitActionShot(bool bIsChipShot);
    void ActionShot(float fDeltaT);
    void InitActionShootToScore();
    void SetupCaptainSTSAnimCam(bool arg1);
    void ActionShootToScore(float fDeltaT);
    void InitActionSlideAttack(cFielder* pTarget, float fTime);
    void ActionSlideAttack(float fDeltaTime);
    void InitActionSlideAttackFailReact();
    void ActionSlideAttackFailReact(float fDeltaT);
    void InitActionSquishReact(const nlVector3& dir);
    void DoSlideAttackStats();
    void InitActionSTSHitReact(cPlayer* fDeltaT);
    void ActionSlideAttackReact(float fDeltaT);
    void ActionBombReact(float fDeltaT);
    void ActionSTSHitReact(float fDeltaT);
    void ActionShellReact(float fDeltaT);
    void ActionBananaReact(float fDeltaT);
    void ActionSquishReact(float fDeltaT);
    void InitActionReceivePass(int animID, nlVector3& v3TargetPos, float fAdjustEndTime);
    void ActionReceivePass(float fDeltaT);
    void InitActionWait();
    void ActionWait(float fDeltaT);

    inline void SetAnim(const u16 facingDelta, const int* animIDs)
    {
        SetAnimState(animIDs[(facingDelta >> 14) & 3], true, 0.2f, false, false);
    }

    bool m_bHasBeenUpdated;             // offset 0x1D4, size 0x1
    eFielderActionState m_eActionState; // offset 0x1D8, size 0x4
    float m_fShootToScoreActiveTime;    // offset 0x1DC, size 0x4
    Timer m_tFrozenTimer;               // offset 0x1E0, size 0x4
    struct                              /* @class$486Powerups_cpp */
    {
        // total size: 0x4
        bool bStickWasReset;             // offset 0x0, size 0x1
        bool bPossibleSuccessfulDeke;    // offset 0x1, size 0x1
        bool bPossibleTurboMove;         // offset 0x2, size 0x1
        bool bTurboButtonDownLastUpdate; // offset 0x3, size 0x1
    } mActionDekeVars;                   // offset 0x1E4, size 0x4
    struct                               /* @class$487Powerups_cpp */
    {
        // total size: 0x4
        float electrocutionTime; // offset 0x0, size 0x4
    } mActionElectrocutionVars;  // offset 0x1E8, size 0x4
    struct                       /* @class$488Powerups_cpp */
    {
        // total size: 0x1
        bool bDoFrameLock;       // offset 0x0, size 0x1
    } mActionHitReactActionVars; // offset 0x1EC, size 0x1
    struct                       /* @class$489Powerups_cpp */
    {
        // total size: 0x4
        float fRumbleDirection; // offset 0x0, size 0x4
    } mActionRumbleVars;        // offset 0x1F0, size 0x4
    struct                      /* @class$490Powerups_cpp */
    {
        // total size: 0x2
        bool bIsShootToScore; // offset 0x0, size 0x1
        bool bIsChipShot;     // offset 0x1, size 0x1
    } mActionShotVars;        // offset 0x1F4, size 0x2
    struct                    /* @class$491Powerups_cpp */
    {
        // total size: 0x8
        cPlayer* passTarget;    // offset 0x0, size 0x4
        bool bVolleyPass;       // offset 0x4, size 0x1
    } mActionLooseBallPassVars; // offset 0x1F8, size 0x8
    struct                      /* @class$492Powerups_cpp */
    {
        // total size: 0x1
        bool bIsChipShot;       // offset 0x0, size 0x1
    } mActionLooseBallShotVars; // offset 0x200, size 0x1
    struct                      /* @class$493Powerups_cpp */
    {
        // total size: 0x4
        float fOneTimerAnimTime; // offset 0x0, size 0x4
    } mActionOneTimerVars;       // offset 0x204, size 0x4
    struct                       /* @class$494Powerups_cpp */
    {
        // total size: 0x8
        cPlayer* pPassTarget; // offset 0x0, size 0x4
        bool bVolleyPass;     // offset 0x4, size 0x1
        bool bAllowLeadPass;  // offset 0x5, size 0x1
    } mActionPassingVars;     // offset 0x208, size 0x8
    struct                    /* @class$495Powerups_cpp */
    {
        // total size: 0x8
        eStrafeDirection eLastStrafeDirection; // offset 0x0, size 0x4
        bool bFirstCycleOfTurbo;               // offset 0x4, size 0x1
    } mActionRunningVars;                      // offset 0x210, size 0x8
    struct                                     /* @class$496Powerups_cpp */
    {
        // total size: 0x1
        bool bWaitForAnimToFinish; // offset 0x0, size 0x1
    } mActionRunningWBVars;        // offset 0x218, size 0x1
    struct                         /* @class$497Powerups_cpp */
    {
        // total size: 0x1
        bool bForcedMirrorSwap;  // offset 0x0, size 0x1
    } mActionRunningWBTurboVars; // offset 0x219, size 0x1
    struct                       /* @class$498Powerups_cpp */
    {
        // total size: 0x8
        eSlideAttackState eSlideAttackState;        // offset 0x0, size 0x4
        bool bAttackSucceeded;                      // offset 0x4, size 0x1
        bool bIsBButtonReset;                       // offset 0x5, size 0x1
        bool bWasStarMushroomUsedDuring;            // offset 0x6, size 0x1
    } mActionSlideAttackVars;                       // offset 0x21C, size 0x8
    ActionShootToScoreVars mActionShootToScoreVars; // offset 0x224, size 0x30
    eFielderDesireState m_eFielderDesireState;      // offset 0x254, size 0x4
    eFielderDesireState m_ePrevFielderDesireState;  // offset 0x258, size 0x4
    int m_eDesireSubState;                          // offset 0x25C, size 0x4
    Timer m_tDesireDuration;                        // offset 0x260, size 0x4
    float m_fDesireConfidence;                      // offset 0x264, size 0x4
    sDesireParams m_sQueuedDesireParams;            // offset 0x268, size 0x68
    struct                                          /* @class$499Powerups_cpp */
    {
        // total size: 0x28
        Timer tMiscTimer;            // offset 0x0, size 0x4
        Timer tAge;                  // offset 0x4, size 0x4
        float fMisc;                 // offset 0x8, size 0x4
        cPlayer* pSBC;               // offset 0xC, size 0x4
        cPlayer* pBallOwner;         // offset 0x10, size 0x4
        nlVector3 v3DesiredPosition; // offset 0x14, size 0xC
        bool bInPosition;            // offset 0x20, size 0x1
        eTurboRequest turboRequest;  // offset 0x24, size 0x4
    } m_DesireCommonVars;            // offset 0x2D0, size 0x28
    struct                           /* @class$500Powerups_cpp */
    {
        // total size: 0x1
        bool bDidWave;     // offset 0x0, size 0x1
    } m_DesireGetOpenVars; // offset 0x2F8, size 0x1
    struct                 /* @class$501Powerups_cpp */
    {
        // total size: 0x2C
        unsigned short aDesiredFacingDirection; // offset 0x0, size 0x2
        nlVector3 v3DesiredPosition;            // offset 0x4, size 0xC
        float fDesiredTime;                     // offset 0x10, size 0x4
        nlVector3 v3BallPosition;               // offset 0x14, size 0xC
        int nOneTimerAnim;                      // offset 0x20, size 0x4
        float fOneTimerAnimTime;                // offset 0x24, size 0x4
        bool bIsChipShot;                       // offset 0x28, size 0x1
        bool bVolleyPassReceive;                // offset 0x29, size 0x1
    } m_DesireOneTimerVars;                     // offset 0x2FC, size 0x2C
    struct                                      /* @class$502Powerups_cpp */
    {
        // total size: 0x38
        nlVector3 v3DesiredPosition;            // offset 0x0, size 0xC
        unsigned short aDesiredFacingDirection; // offset 0xC, size 0x2
        float fDesiredTime;                     // offset 0x10, size 0x4
        nlVector3 v3BallPosition;               // offset 0x14, size 0xC
        int nReceivePassAnim;                   // offset 0x20, size 0x4
        float fReceivePassAnimTime;             // offset 0x24, size 0x4
        int iAttemptOneTouchShot;               // offset 0x28, size 0x4
        bool bFailedToInitOneTouchShot;         // offset 0x2C, size 0x1
        signed short iAttemptOneTouchPass;      // offset 0x2E, size 0x2
        bool bVolleyPassReceive;                // offset 0x30, size 0x1
        cPlayer* pOneTouchPassTarget;           // offset 0x34, size 0x4
    } m_DesireReceivePassSharedVars;            // offset 0x328, size 0x38
    struct                                      /* @class$503Powerups_cpp */
    {
        // total size: 0x4
        cFielder* m_pSlideAttackTarget; // offset 0x0, size 0x4
    } m_DesireSlideAttackVars;          // offset 0x360, size 0x4
    struct                              /* @class$504Powerups_cpp */
    {
        // total size: 0x1
        bool bIsBallAwayFromCarrier;           // offset 0x0, size 0x1
    } m_DesireWindupForShotVars;               // offset 0x364, size 0x1
    Timer mtKickOffWaitTimer;                  // offset 0x368, size 0x4
    Timer m_tPowerupEffectTime;                // offset 0x36C, size 0x4
    ePowerUpType m_ePowerup;                   // offset 0x370, size 0x4
    int mnNumPowerups;                         // offset 0x374, size 0x4
    cFielder* m_pPowerupTarget;                // offset 0x378, size 0x4
    int m_nPowerupAnimID;                      // offset 0x37C, size 0x4
    Timer mtBombImpactTime;                    // offset 0x380, size 0x4
    nlVector3 mv3BombImpactLocation;           // offset 0x384, size 0xC
    float mfBombImpactRadius;                  // offset 0x390, size 0x4
    ePenaltyCardStatus m_ePenaltyCardStatus;   // offset 0x394, size 0x4
    cFielder* m_pMark;                         // offset 0x398, size 0x4
    cFielder* m_pMarker;                       // offset 0x39C, size 0x4
    eRole m_eRole;                             // offset 0x3A0, size 0x4
    AIPlay* m_pCurrentPlay;                    // offset 0x3A4, size 0x4
    nlVector3 m_v3DesiredPosition;             // offset 0x3A8, size 0xC
    float m_fDistanceToDesiredPosition;        // offset 0x3B4, size 0x4
    nlVector3 m_v3AccumDesiredPos;             // offset 0x3B8, size 0xC
    float m_fAccumDesiredPosWeight;            // offset 0x3C4, size 0x4
    AvoidController* m_pAvoidance;             // offset 0x3C8, size 0x4
    ShotMeter* m_pShotMeter;                   // offset 0x3CC, size 0x4
    eShootToScoreResult meS2SResult;           // offset 0x3D0, size 0x4
    bool mbCanKickoff;                         // offset 0x3D4, size 0x1
    bool mbWasHitByPowerupThisFrame;           // offset 0x3D5, size 0x1
    bool mbCaptShootToScoreEffectOn;           // offset 0x3D6, size 0x1
    unsigned long mThoughtHashCalcDesire;      // offset 0x3D8, size 0x4
    unsigned long mThoughtHashInitRunToNet;    // offset 0x3DC, size 0x4
    unsigned long mThoughtHashInitGetOpen;     // offset 0x3E0, size 0x4
    unsigned long mThoughtHashInitWindupPass;  // offset 0x3E4, size 0x4
    unsigned long mThoughtHashInitCutAndBreak; // offset 0x3E8, size 0x4

}; // total size: 0x3EC

#endif // _FIELDER_H_
