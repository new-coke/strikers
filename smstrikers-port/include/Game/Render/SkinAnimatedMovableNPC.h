#ifndef _SKINANIMATEDMOVABLENPC_H_
#define _SKINANIMATEDMOVABLENPC_H_

#include "Game/Render/SkinAnimatedNPC.h"

#include "Game/SHierarchy.h"
#include "Game/Physics/PhysicsNPC.h"
#include "Game/PoseAccumulator.h"

class SkinAnimatedMovableNPC : public SkinAnimatedNPC
{
public:
    SkinAnimatedMovableNPC(cSHierarchy& pHierarchy, int nModelID, PhysicsNPC& pPhysicsObj);
    /* 0x08 */ virtual ~SkinAnimatedMovableNPC();
    /* 0x0C */ virtual SkinAnimatedNPC_Type GetSkinAnimatedNPC_Type() const { return SkinAnimatedNPC_MOVABLE; };
    /* 0x10 */ virtual void Render();
    /* 0x14 */ virtual void RenderFromReplay(const cPoseAccumulator& poseAcc, const nlMatrix4* pMatrix);
    /* 0x18 */ virtual void Update(float dt);
    // /* 0x1C */ virtual void DrawShadow(const glModel*, const nlMatrix4&);
    // /* 0x20 */ virtual void DrawShadow(const cPoseAccumulator&, const nlMatrix4&);
    /* 0x24 */ virtual float GetHeadSpin() const;
    /* 0x28 */ virtual float GetHeadTilt() const;
    /* 0x2C */ virtual void SetPosition(const nlVector3& pos);
    /* 0x30 */ virtual void Move(float) = 0;
    /* 0x34 */ virtual void AnimTranslate(float fDeltaT, bool bUseZ);
    /* 0x38 */ virtual void AnimMove(float speed, bool applyPhysics);
    /* 0x3C */ virtual void AnimMoveSeek(float speed, float turnRate, float seekRate, bool applyPhysics);

    /* 0x5C */ nlVector3 mv3Position;
    /* 0x68 */ nlVector3 mv3Velocity;
    /* 0x74 */ u16 maFacingDirection;
    /* 0x76 */ u16 maDesiredFacingDirection;
    /* 0x78 */ float mfDesiredSpeed;
    /* 0x7C */ PhysicsNPC* mpPhysObj;
}; // total size: 0x80

inline float SkinAnimatedMovableNPC::GetHeadTilt() const
{
    return 0.0f;
}

inline float SkinAnimatedMovableNPC::GetHeadSpin() const
{
    return 0.0f;
}

#endif // _SKINANIMATEDMOVABLENPC_H_
