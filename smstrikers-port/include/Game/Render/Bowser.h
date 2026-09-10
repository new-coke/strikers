#ifndef _BOWSER_H_
#define _BOWSER_H_

#include "NL/nlMath.h"
#include "NL/nlTimer.h"

#include "Game/Inventory.h"
#include "Game/Physics/PhysicsObject.h"
#include "Game/CharacterAudio.h"
#include "Game/Render/SkinAnimatedMovableNPC.h"
#include "Game/Effects/EmissionController.h"
#include "Game/Character.h"

enum eBowserState
{
    BOWSER_STATE_HIDDEN = 0,
    BOWSER_STATE_FALL = 1,
    BOWSER_STATE_JUMP = 2,
    BOWSER_STATE_IDLE = 3,
    BOWSER_STATE_THROW = 4,
    BOWSER_STATE_ROAR = 5,
    BOWSER_STATE_ROLL = 6,
    BOWSER_STATE_LEAVE = 7,
};
enum eBowserAttackType
{
    BOWSER_ATTACK_ROLL = 0,
    BOWSER_ATTACK_JUMP = 1,
    BOWSER_ATTACK_CRAZY = 2,
    BOWSER_ATTACK_STOMP = 3,
    NUM_BOWSER_ATTACKS = 4,
};
enum eBowserAnim
{
    BOWSER_ANIM_IDLE = 0,
    BOWSER_ANIM_LAND = 1,
    BOWSER_ANIM_JUMP = 2,
    BOWSER_ANIM_WALK = 3,
    BOWSER_ANIM_THROW = 4,
    BOWSER_ANIM_ROAR = 5,
    BOWSER_ANIM_ROLL = 6,
    NUM_BOWSER_ANIMS = 7,
};
enum eBowserMoveResult
{
    BOWSER_MOVE_RESULT_NORMAL = 0,
    BOWSER_MOVE_RESULT_REBOUND = 1,
    BOWSER_MOVE_RESULT_AT_REST = 2,
};

class cHeadTrack;
class cPN_Feather;
class cPN_SAnimController;
class cSAnim;
class cFielder;

class Bowser : public SkinAnimatedMovableNPC
{
public:
    Bowser(cSHierarchy& rSHierarchy, int nTeam, PhysicsNPC& rPhysNPC, cInventory<cSAnim>* pInventorySAnim);
    virtual ~Bowser();
    virtual SkinAnimatedNPC_Type GetSkinAnimatedNPC_Type() const { return SkinAnimatedNPC_BOWSER; };
    virtual void Update(float fDeltaT);
    static void CollisionCallback(PhysicsObject* pPhysObj, PhysicsObject* pObjA, const nlVector3& v3Pos);
    void ActionInit();
    void ActionThrow();
    void ActionRoll();
    void ActionStomp();
    void ActionDescend(float fBlendTime);
    void ActionFall();
    void ActionJump();
    void ActionHide();
    void ActionReset();
    void ActionLeave();
    void SetBowserAnimState(eBowserAnim anim, ePlayMode playMode, float fBlendTime);
    void SetBowserFeatherAnimState(eBowserAnim anim, float fBlendTime);
    void ClearBowserFeatherAnimState(bool bBlendOut);
    void Move(float fDeltaT);
    eBowserMoveResult GravityMove(float fDeltaT);
    void ActionIdle();
    static void SetTiltParameters(float fYAxisTilt);
    bool CheckForAbort();
    bool IsTilted() const;
    static void UpdateFireEmitter(EmissionController& controller);
    static void UpdateBowserLandEmitter(EmissionController& controller);
    void EmitFire();
    void KillFire();
    cFielder* GetTarget() const { return mpTarget; }
    void FindTarget();
    void SetupBaseSFX();
    void PlaySFX(Audio::eCharSFX type, PosUpdateMethod posUpdateMethod, float fDelay, bool bIs3D);
    void CheckFootSteps();
    static bool CheckTargetBounds(nlVector3& v3Target);
    float GetHeadSpin() const;
    float GetHeadTilt() const;
    void DrawShadow(const cPoseAccumulator& poseAccumulator, const nlMatrix4& matrix);

    /* 0x080 */ nlMatrix4 mLastHeadMatrix;
    /* 0x0C0 */ cHeadTrack* mpHeadTrack;
    /* 0x0C4 */ int mnHeadJointIndex;
    /* 0x0C8 */ cPN_Feather* mpFeatherBlender;
    /* 0x0CC */ cPN_SAnimController* mpFeatherController;
    /* 0x0D0 */ cSAnim* mpAnim[7];
    /* 0x0EC */ cFielder* mpTarget;
    /* 0x0F0 */ nlVector3 mv3TargetPos;
    /* 0x0FC */ eBowserState meBowserState;
    /* 0x100 */ Timer mtStateTimer;
    /* 0x104 */ Timer mtActiveTimer;
    /* 0x108 */ int mAnimID;
    /* 0x10C */ bool mbDoTilt;
    /* 0x110 */ enum eBowserAttackType mAttackType;
    /* 0x114 */ int mStompStage;
    /* 0x118 */ bool mbFirstTime;
    /* 0x119 */ bool mbAlive;
    /* 0x11A */ bool mbResetPending;
    /* 0x11C */ Audio::cCharacterSFX* m_pCharacterSFX;

    static float mfYAxisTilt;
}; // total size: 0x120

#endif // _BOWSER_H_
