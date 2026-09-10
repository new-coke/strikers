#ifndef _GOALIE_H_
#define _GOALIE_H_

#include "Game/Player.h"
#include "Game/GoalieFatigue.h"
#include "Game/AI/GoalieSave.h"
#include "Game/AI/GoalieLooseBall.h"
#include "Game/Field.h"

#include "NL/nlMath.h"

#include "ode/collision.h"

class cFielder;
class cPlayer;
class cSHierarchy;
class cAnimInventory;
class CharacterPhysicsData;
class GoalieTweaks;
class AnimRetargetList;
class Event;
class cPN_SAnimController;
class CollisionPlayerPlayerData;
class cBall;
class PhysicsGoalie;

enum eGoalieActionState
{
    GOALIEACTION_MOVE = 0,
    GOALIEACTION_MOVE_WB = 1,
    GOALIEACTION_SAVE_SETUP = 2,
    GOALIEACTION_SAVE_REPOSITION = 3,
    GOALIEACTION_SAVE = 4,
    GOALIEACTION_MISS_CHIP_SHOT = 5,
    GOALIEACTION_DIVE_RECOVER = 6,
    GOALIEACTION_STS_SETUP = 7,
    GOALIEACTION_STS = 8,
    GOALIEACTION_STS_RECOVER = 9,
    GOALIEACTION_STS_ATTACK_SETUP = 10,
    GOALIEACTION_STS_ATTACK = 11,
    GOALIEACTION_PASS = 12,
    GOALIEACTION_PASS_INTERCEPT = 13,
    GOALIEACTION_PRE_CROUCH = 14,
    GOALIEACTION_PURSUE_BALL_CARRIER = 15,
    GOALIEACTION_PURSUE_BALL_POUNCE = 16,
    GOALIEACTION_LOOSEBALL_SETUP = 17,
    GOALIEACTION_LOOSEBALL_CATCH = 18,
    GOALIEACTION_LOOSEBALL_PICKUP = 19,
    GOALIEACTION_LOOSEBALL_PURSUE_BOUNCING = 20,
    GOALIEACTION_LOOSEBALL_PURSUE_ROLLING = 21,
    GOALIEACTION_LOOSEBALL_DESPERATE = 22,
    GOALIEACTION_OFFPLAY = 23,
    GOALIEACTION_SNAP_BALL = 24,
    GOALIEACTION_GRAB_BALL = 25,
    NUM_GOALIE_ACTIONS = 26,
};

enum eGoalieMoveDirection
{
    GOALIEDIR_IDLE = 0,
    GOALIEDIR_FORWARD = 1,
    GOALIEDIR_BACKWARD = 2,
    GOALIEDIR_SIDE = 3,
    GOALIEDIR_BACK2FRONT = 4,
    GOALIEDIR_FRONT2BACK = 5,
    NUM_GOALIEDIR = 6,
};

enum eGoalieCrouchType
{
    GOALIECROUCH_SHOT = 0,
    GOALIECROUCH_PASS = 1,
    GOALIECROUCH_LOOSEBALL = 2,
    NUM_GOALIECROUCH_TYPES = 3,
};

enum eGoalieOffplayType
{
    GOALIE_OFFPLAY_NONE = 0,
    GOALIE_OFFPLAY_GOAL_FOR = 1,
    GOALIE_OFFPLAY_GOAL_AGAINST = 2,
    GOALIE_OFFPLAY_ENDGAME_WIN = 3,
    GOALIE_OFFPLAY_ENDGAME_LOSE = 4,
    GOALIE_OFFPLAY_HALFTIME = 5,
    GOALIE_OFFPLAY_PENALTY = 6,
    NUM_GOALIE_OFFPLAY_TYPES = 7,
};

enum eUrgency
{
    URGENCY_LOW = 0,
    URGENCY_MED = 1,
    URGENCY_HIGH = 2,
    NUM_URGENCY_LEVELS = 3,
};

struct GoalieSaveData : public EventData
{
    virtual u32 GetID() { return 0x13C; }

    /* 0x04 */ nlVector3 v3BallVelocity;
    /* 0x10 */ cPlayer* pGoalie;
    /* 0x14 */ cPlayer* pShooter;
    /* 0x18 */ u32 saveType;
    /* 0x1C */ float fWowFactor;
    /* 0x20 */ unsigned int isSTS : 1;
    /* 0x20 */ unsigned int padding : 31;
}; // total size: 0x24

struct GoalScoredData : public EventData
{
    static const u32 ID = 0x18A;

    /* 0x04 */ unsigned int uTeamIndex : 8;
    /* 0x04 */ unsigned int uNumGoalsScored : 8;
    /* 0x04 */ unsigned int uGoalType : 15;
    /* 0x04 */ unsigned int uIsHyper : 1;
    /* 0x08 */ nlVector3 v3ShotPosition;
    /* 0x14 */ cPlayer* pScorer;
    /* 0x18 */ cPlayer* pAssister;
    /* 0x1C */ cPlayer* pLastTouch[2];

    virtual u32 GetID() { return ID; }
}; // total size: 0x24

class Goalie : public cPlayer
{
public:
    enum eNaviMode
    {
        NAVI_FACE_DESIRED = 0,
        NAVI_FACE_BALL = 1,
        NAVI_FOLLOW_TARGET = 2,
    };

    Goalie(eCharacterClass charClass, const int* nModelID, cSHierarchy* pHierarchy, cAnimInventory* pAnimInventory, const CharacterPhysicsData* pPhysicsData, GoalieTweaks* pCharTweaks, AnimRetargetList* pAnimRetargetList);
    ~Goalie();

    void Update(float dt);
    virtual void CollideWithBallCallback(cBall* pBall);
    virtual void CollideWithCharacterCallback(CollisionPlayerPlayerData* pData);
    bool PreCollideWithBallCallback(const dContact& contact);
    void ExecutePounce(cPlayer* pPlayer, bool bCheckHitDistance);
    void InitActionPass(bool useTarget);
    void InitActionPassIntercept();
    void InitActionPassInterceptSave();
    void InitActionPreCrouch(eGoalieCrouchType crouchType);
    void InitActionPursueBallCarrier();
    void InitActionPursueBallPounce();
    void InitActionPursueRecover();
    void DoNavigation(float fDeltaT, float fIdleDistance, Goalie::eNaviMode naviMode);
    void FindDesiredGoaliePosition(nlVector3& pos, nlVector3& dir, nlVector3& focus, unsigned short& ang, const nlVector3* pThreatPos);
    bool ShouldReposition();
    cPlayer* FindOpenPassTarget();
    unsigned char IsTargetViable(cPlayer* pTarget);
    unsigned short FindDumpDirection(unsigned short aDesired, bool bConstrain);
    void HandleSTSSwat();
    void HandleSTSContact(cBall* pBall);
    bool InitiatePickup();
    void InitiatePanicGrab(cPlayer* pPlayer);
    bool IsCloseToPlane(const nlVector3& rPos1, const nlVector3& rPos2, float fThreshold);
    bool IsInsideGoalieBox(const nlVector3& rPos, float fXOffset, float fYOffset);
    bool IsInsideNetArea(const nlVector3& v3Target);
    float CheckForDelflectAwayFromNet();
    bool CheckForLooseBallShotInProgress();
    bool CheckForBallOnHead();
    bool CheckForSTSAttack();
    bool IsLooseBallTowardNet();
    bool IsLooseBallClose(float fDistFromBox);
    bool IsWithinPounceRange();
    bool IsOpponentBallCarrierInRange();
    bool IsOpponentInSTS();
    float IsSoloBreakaway();
    void MakeExertEvent();
    bool IsPassThreat();
    void MakeSaveEvent(bool bIsSTS);
    void UpdateActionState(float fDeltaTime);
    void SetGoalieAction(eGoalieActionState newGoalieState, int newSubstate);
    static void SaveBlendCallback(uintptr_t nParam, cPN_SAnimController* pAnimCtrl);
    cPoseNode* SetupBlender(bool bPrimary, const float* fStartPercent, int nMainAnimID, int nMilestone);
    void PlayBlendedAnims(float fStartTime, int nMilestone);
    void PlayNewAnim(int nAnimID);
    void CleanGoalieAction();
    void InitActionLooseBallCatch();
    void InitActionLooseBallSetup();
    void ChooseDesperationAnim(float fFudgeDist);
    void InitActionSaveReposition();
    void InitActionLooseBallPursueBouncing(const nlVector3& v3TargetPosition, float fTargetTime);
    void InitActionLooseBallPursueRolling();
    void InitActionLooseBallPickup(float fDistance, bool bStartPickup);
    void InitActionOffplay(eGoalieOffplayType type);
    void InitActionSTSAttackSetup(float fWaitTime);
    void InitActionSnapBall();
    void InitActionMove(bool bParam);
    void InitActionMoveWB();
    void InitActionSaveSetup(bool bCanReposition);
    void InitActionSave();
    void InitActionSTSSetup();
    void InitActionSTS();
    void InitActionSTSRecover();
    void InitActionChipShotStumble();
    void InitActionDiveRecover();
    void InitActionSTSAttack();
    bool IsTeammateHoardingBall();
    void InitActionGrabBall();
    unsigned short CalcBestSave(float fTime, const nlVector3& rTargetPos, const nlVector3& rContactPos, unsigned int uSaveType, bool bParam);
    float CalcSaveParameters(float fTimeToContact, unsigned int uSaveType, bool bFromTakeoff, bool bFindFailSave);
    float CalcTimeToPlane();
    bool CanInterceptPass();
    static unsigned char ClampToGoalCone(nlVector3& v3Position, float fDistFromEnd);
    int ChooseRunAnim(short nAngle, const nlVector3& rTargetPos, float fThreshold);
    void CleanSTSRecover();
    void CleanupStun();
    void ChooseSwatAnim(int nParam);
    void DoPassRelease();
    static void EventHandler(Event* event, void* userData);
    void FindSTSStunData();
    bool FindSTSMissData(const nlVector3& rPos);
    void FindSTSSpinData(bool bParam);
    PhysicsGoalie* GetPhysicsGoalie();
    void SetDesiredSaveFacing(const nlVector3& v3BallPosition);
    void TrackTarget(const nlVector3& v3Target, float fRatio);
    void TacklePlayer(cPlayer* pPlayer);
    void StealBall(cPlayer* pPlayer);
    void WhackSTSPlayer(cFielder* pFielder);
    void InitGoalieActionData();
    virtual void InitActionPostWhistle();

    void ActionLooseBallCatch(float deltaTime);
    void ActionLooseBallDesperate(float fDeltaT);
    void ActionLooseBallPickup(float fDeltaT);
    void ActionLooseBallPursueRolling(float deltaTime);
    void ActionLooseBallSetup(float fDeltaT);
    static void MoveDirectionCB(uintptr_t nParam, cPN_SingleAxisBlender* blender);
    static void MoveWeightCB(uintptr_t nParam, cPN_SingleAxisBlender* blender);
    static void StrafeSynchronizedSpeedCallback(uintptr_t nParam, cPN_SAnimController* controller);
    void ActionMove(float deltaTime);
    static void RunWeightCB(uintptr_t nParam, cPN_SingleAxisBlender* blender);

    void StartRunBlend();

    void ActionMoveWB(float fDeltaT);
    void ActionSaveSetup(float deltaTime);
    void ActionSaveReposition(float deltaTime);
    void ActionSave(float fDeltaT);
    void CheckForLimbEndZoneCollision();
    void ActionSTS(float fDeltaT);
    void ActionSTSSetup(float deltaTime);
    void ActionSTSRecover(float deltaTime);
    void ActionChipShotStumble(float deltaTime);
    void ActionDiveRecover(float fDeltaT);
    void ActionPass(float deltaTime);
    void ActionPassIntercept(float deltaTime);
    void ActionPreCrouch(float deltaTime);
    void ActionPursueBallCarrier(float fDeltaT);
    void ActionPursueBallPounce(float fDeltaT);
    void ActionOffplay(float fDeltaT);
    void ActionLooseBallPursueBouncing(float deltaTime);
    void ActionSTSAttackSetup(float deltaTime);
    void ActionSTSAttack(float deltaTime);
    void ActionSnapBall(float fDeltaT);
    void ActionGrabBall(float fDeltaT);

    inline bool IsRecovering() const
    {
        return mGoalieActionState == GOALIEACTION_STS_RECOVER;
    }

    inline bool IsBusy() const
    {
        return HasBall() || (mGoalieActionState == GOALIEACTION_PASS) || (mGoalieActionState == GOALIEACTION_PASS_INTERCEPT) || (mGoalieActionState == GOALIEACTION_MOVE) || (mGoalieActionState == GOALIEACTION_MOVE_WB) || (mGoalieActionState == GOALIEACTION_PASS_INTERCEPT) || (mGoalieActionState == GOALIEACTION_PURSUE_BALL_CARRIER) || (mGoalieActionState == GOALIEACTION_PURSUE_BALL_POUNCE) || (mGoalieActionState == GOALIEACTION_LOOSEBALL_SETUP) || (mGoalieActionState == GOALIEACTION_LOOSEBALL_CATCH) || (mGoalieActionState == GOALIEACTION_LOOSEBALL_PICKUP) || (mGoalieActionState == GOALIEACTION_LOOSEBALL_PURSUE_BOUNCING) || (mGoalieActionState == GOALIEACTION_LOOSEBALL_PURSUE_ROLLING);
    }

    inline unsigned char IsAttacking() const
    {
        unsigned char bAttacking = false;
        if (mGoalieActionState == GOALIEACTION_PURSUE_BALL_CARRIER || mGoalieActionState == GOALIEACTION_PURSUE_BALL_POUNCE)
        {
            bAttacking = true;
        }
        return bAttacking;
    }

    inline bool IsPositionBeyondGoalLine() const
    {
        return (float)fabs(mv3TargetPosition.x) > cField::GetGoalLineX(1U);
    }

    /*
     * Facing direction mirrored onto the near side of the net and clamped to the
     * goal arc, i.e. the direction pass and save decisions are measured against.
     */
    inline u16 GetClampedFacing() const
    {
        u16 aFacing = m_aActualFacingDirection;
        if (m_v3Position.x < 0.0f)
        {
            aFacing += 0x8000;
        }
        if (aFacing < 0x5550)
        {
            aFacing = 0x5550;
        }
        else if (aFacing > 0xAAB0)
        {
            aFacing = (u16)-0x5550;
        }
        if (m_v3Position.x < 0.0f)
        {
            aFacing += 0x8000;
        }
        return aFacing;
    }

    /* 0x1D4 */ eGoalieActionState mGoalieActionState;
    /* 0x1D8 */ eGoalieActionState mPrevGoalieActionState;
    /* 0x1DC */ eUrgency mUrgency;
    /* 0x1E0 */ cPlayer* mpPassTarget;
    /* 0x1E4 */ cFielder* mpShooter;
    /* 0x1E8 */ s32 mnSubstate;
    /* 0x1EC */ eGoalieMoveDirection mMoveDirection;
    /* 0x1F0 */ eGoalieCrouchType mCrouchType;
    /* 0x1F4 */ f32 mfSwitchTime;
    /* 0x1F8 */ SaveData* mpSaveData;
    /* 0x1FC */ SaveBlendInfo mBlendInfo;
    /* 0x28C */ u32 muSaveType;
    /* 0x290 */ f32 mfWaitTime;
    /* 0x294 */ f32 mfTimeTilSave;
    /* 0x298 */ bool mbPlayMiss;
    /* 0x299 */ bool mbShouldMiss;
    /* 0x29A */ bool mbStunEffectActive;
    /* 0x29B */ bool mbDoIntercept;
    /* 0x29C */ bool mbDoNavigate;
    /* 0x29D */ bool mbDoHeadTrack;
    /* 0x29E */ bool mbBallImpacted;
    /* 0x29F */ bool mbNoUserControl;
    /* 0x2A0 */ bool mbIsPosed;
    /* 0x2A1 */ bool mbPickedUp;
    /* 0x2A4 */ nlVector3 mv3LocalContactPosition;
    /* 0x2B0 */ nlVector3 mv3LocalContactVelocity;
    /* 0x2BC */ nlVector3 mv3TargetPosition;
    /* 0x2C8 */ nlVector3 mv3NavTarget;
    /* 0x2D4 */ nlVector3 mv3LocalNavTarget;
    /* 0x2E0 */ s16 maLocalAngle;
    /* 0x2E4 */ f32 mfTargetTime;
    /* 0x2E8 */ f32 mfTargetDist;
    /* 0x2EC */ f32 mfSpeedScale;
    /* 0x2F0 */ const LooseBallInfo* mpLooseBallInfo;
    /* 0x2F4 */ u32 muBallChangeCount;
    /* 0x2F8 */ u32 muBallDeflectCount;
    /* 0x2FC */ GoalieFatigue mFatigue;
    /* 0x30C */ eGoalieOffplayType mnOffplayPending;

    static f32 mfGoalieStepDist;
    static bool mbPosGoalieNetCheck;
    static bool mbNegGoalieNetCheck;
    static u8 mbActionDataSetup;
    static f32 mfGoalieStrafeDist;
    static f32 mfGoalieRunDist;
    static f32 mfGoalieUrgentDist;
}; // total size: 0x310

#endif // _GOALIE_H_
