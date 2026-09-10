#include "Game/Render/SkinAnimatedMovableNPC.h"
#include "Game/AI/AiUtil.h"
#include "Game/SAnim/pnSAnimController.h"

/**
 * Offset/Address/Size: 0x3A4 | 0x80165414 | size: 0xA8
 */
SkinAnimatedMovableNPC::SkinAnimatedMovableNPC(cSHierarchy& pHierarchy, int nModelID, PhysicsNPC& pPhysicsObj)
    : SkinAnimatedNPC(pHierarchy, nModelID)
{
    maDesiredFacingDirection = 0;
    mfDesiredSpeed = 0.0f;
    mpPhysObj = &pPhysicsObj;
    mv3Velocity.x = 0.0f;
    mv3Velocity.y = 0.0f;
    mv3Velocity.z = 0.0f;
    nlVector3 v3Start;
    v3Start.x = 0.0f;
    v3Start.y = 1.0f;
    v3Start.z = 0.0f;
    SetPosition(v3Start);
    mpPhysObj->SetPosition(mv3Position, PhysicsObject::WORLD_COORDINATES);
    mpPhysObj->EnableCollisions();
}

/**
 * Offset/Address/Size: 0x320 | 0x80165390 | size: 0x84
 */
SkinAnimatedMovableNPC::~SkinAnimatedMovableNPC()
{
    delete mpPhysObj;
}

/**
 * Offset/Address/Size: 0x2A4 | 0x80165314 | size: 0x7C
 */
#pragma push
#pragma optimization_level 1 // needed to match..
void SkinAnimatedMovableNPC::SetPosition(const nlVector3& pos)
{
    mv3Position = pos;
    nlVector3 physPos = pos;
    physPos.z += mpPhysObj->GetRadius();
    mpPhysObj->SetPosition(physPos, PhysicsObject::WORLD_COORDINATES);
}
#pragma pop

/**
 * Offset/Address/Size: 0x284 | 0x801652F4 | size: 0x20
 */
void SkinAnimatedMovableNPC::Update(float dt)
{
    SkinAnimatedNPC::Update(dt);
}

/**
 * Offset/Address/Size: 0x208 | 0x80165278 | size: 0x7C
 */
void SkinAnimatedMovableNPC::Render()
{
    nlMakeRotationMatrixZ(mWorldMatrix, maFacingDirection * (6.28318530718f / 65536.0f));
    mWorldMatrix.e2[3][0] = mv3Position.x;
    mWorldMatrix.e2[3][1] = mv3Position.y;
    mWorldMatrix.e2[3][2] = mv3Position.z;
    mWorldMatrix.e2[3][3] = 1.0f;
    SkinAnimatedNPC::Render();
}

/**
 * Offset/Address/Size: 0x1E8 | 0x80165258 | size: 0x20
 */
void SkinAnimatedMovableNPC::RenderFromReplay(const cPoseAccumulator& poseAcc, const nlMatrix4* pMatrix)
{
    SkinAnimatedNPC::RenderFromReplay(poseAcc, pMatrix);
}

/**
 * Offset/Address/Size: 0xFC | 0x8016516C | size: 0xEC
 */
void SkinAnimatedMovableNPC::AnimTranslate(float fDeltaT, bool bUseZ)
{
    nlVector3 v3RootVel;
    nlVector3 v3Pos;

    v3Pos = mv3Position;
    mpAnimController->GetRootTrans(&v3RootVel, maFacingDirection);

    float fInvDeltaT = 1.0f / fDeltaT;
    mv3Velocity.x = v3RootVel.x * fInvDeltaT;
    mv3Velocity.y = v3RootVel.y * fInvDeltaT;

    v3Pos.x += v3RootVel.x;
    v3Pos.y += v3RootVel.y;

    if (bUseZ)
    {
        mv3Velocity.z = v3RootVel.z * fInvDeltaT;
        v3Pos.z += v3RootVel.z;
    }

    SetPosition(v3Pos);
}

/**
 * Offset/Address/Size: 0x84 | 0x801650F4 | size: 0x78
 */
void SkinAnimatedMovableNPC::AnimMove(float speed, bool applyPhysics)
{
    u16 rootRot;
    mpAnimController->GetRootRot(&rootRot);
    AnimTranslate(speed, applyPhysics);
    u16 newDir = maFacingDirection + rootRot;
    maFacingDirection = newDir;
}

/**
 * Offset/Address/Size: 0x0 | 0x80165070 | size: 0x84
 */
void SkinAnimatedMovableNPC::AnimMoveSeek(float speed, float turnRate, float seekRate, bool applyPhysics)
{
    u16 newDir = SeekDirection(maFacingDirection, maDesiredFacingDirection, turnRate, seekRate, speed);
    AnimTranslate(speed, applyPhysics);
    maFacingDirection = newDir;
}
