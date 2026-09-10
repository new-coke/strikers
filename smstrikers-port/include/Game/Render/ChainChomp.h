#ifndef _CHAINCHOMP_H_
#define _CHAINCHOMP_H_

#include "Game/Render/SkinAnimatedMovableNPC.h"

#include "Game/SHierarchy.h"
#include "Game/SAnim.h"
#include "Game/Physics/PhysicsNPC.h"
#include "Game/Team.h"
#include "Game/AI/Fielder.h"
#include "Game/PoseAccumulator.h"
#include "Game/Inventory.h"

enum eChainChompState
{
    CHAIN_STATE_HIDDEN = 0,
    CHAIN_STATE_FALL = 1,
    CHAIN_STATE_RECOVER = 2,
    CHAIN_STATE_CHASE = 3,
    CHAIN_STATE_LEAVE = 4,
};

void UpdateChainEmitter(EmissionController& controller);

class ChainChomp : public SkinAnimatedMovableNPC
{
public:
    ChainChomp(cSHierarchy& pHierarchy, int nModelID, PhysicsNPC& rPhysObj, cInventory<cSAnim>* pInventorySAnim);
    virtual ~ChainChomp();
    virtual SkinAnimatedNPC_Type GetSkinAnimatedNPC_Type() const { return SkinAnimatedNPC_CHAIN_CHOMP; };
    virtual void Update(float fDeltaT);
    static void CollisionCallback(PhysicsObject* pPhysObj, PhysicsObject* pObjA, const nlVector3& v3Pos);
    void FindTarget(cTeam* pTeam);
    void Fall(cFielder* pThrower, cFielder* pTarget);
    void Hide(bool bIsEndGame);
    bool IsHidden() const;
    bool AvoidSidelines();
    virtual void Move(float fDeltaT);
    virtual void DrawShadow(const cPoseAccumulator& pa, const nlMatrix4& worldMatrix);
    void Leave();
    void EmitTrail();

    /* 0x80 */ cSAnim* mpIdleAnim;
    /* 0x84 */ cSAnim* mpRecoverAnim;
    /* 0x88 */ cSAnim* mpDropAnim;
    /* 0x8C */ cFielder* mpTarget;
    /* 0x90 */ eChainChompState meChainChompState;
    /* 0x94 */ Timer mtStateTimer;
    /* 0x98 */ SFXEmitter* mpInEffectSFX;
    /* 0x9C */ cFielder* mpThrower;
    /* 0xA0 */ int mnThrowerPadID;
}; // total size: 0xA4

#endif // _CHAINCHOMP_H_
