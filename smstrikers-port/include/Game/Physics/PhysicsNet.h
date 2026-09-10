#ifndef _PHYSICSNET_H_
#define _PHYSICSNET_H_

#include "Game/Physics/PhysicsObject.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsFinitePlane.h"
#include "Game/Physics/PhysicsCapsule.h"
#include "Game/Render/NetMesh.h"
#include "Game/Net.h"

#include "Game/Sys/eventman.h"
#include "Game/EventDataTypes.h"

class PhysicsNet
{
public:
    static PhysicsNet* spPhysNetPositiveX;
    static PhysicsNet* spPhysNetNegativeX;
    static float sfPhysicsNetWidth;
    static float sfPhysicsNetHeight;
    static float sfPhysicsNetDepth;
    static bool sbSweepTestEnabled;
    static float sfWallSoftness;

    PhysicsNet(CollisionSpace* space, bool positive_x);
    virtual ~PhysicsNet();
    virtual int GetObjectType() const { return 0x1b; };

    static bool IsAGoalPost(PhysicsObject* obj);
    static bool IsAGoalWall(PhysicsObject* obj);
    bool SweepTestForBallContact(const nlVector3& startPos, const nlVector3& endPos, const nlVector3& ballVelocity, float ballRadius, nlVector3& contactPos, nlVector3& contactNormal, PhysicsObject** hitObject) const;
    static void StaticInit(CollisionSpace* pCollisionSpace);
    static void StaticDestroy();

    /* 0x4, */ PhysicsFinitePlane* mpBackWall;
    /* 0x8, */ PhysicsFinitePlane* mpSideWall1;
    /* 0xC, */ PhysicsFinitePlane* mpSideWall2;
    /* 0x10 */ PhysicsCapsule* mpSideGoalPost1;
    /* 0x14 */ PhysicsCapsule* mpSideGoalPost2;
    /* 0x18 */ PhysicsCapsule* mpTopGoalPost;
    /* 0x1C */ NetMesh* mpNetMesh;
    /* 0x20 */ cNet* mpNet;
    /* 0x24 */ float errorCorrectionDepth;

}; // total size: 0x28

#endif // _PHYSICSNET_H_
