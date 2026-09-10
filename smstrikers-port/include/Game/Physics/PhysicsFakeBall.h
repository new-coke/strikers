#ifndef _PHYSICSFAKEBALL_H_
#define _PHYSICSFAKEBALL_H_

#include "Game/Ball.h"
#include "Game/Physics/PhysicsBall.h"
#include "Game/Physics/PhysicsPlane.h"
#include "Game/Physics/PhysicsWorld.h"
#include "Game/Physics/CollisionSpace.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlSlotPool.h"

class FakePhysicsBall;

struct BallCacheInfo
{
    /* 0x00 */ float mfTime;
    /* 0x04 */ nlVector3 mv3Position;
    /* 0x10 */ nlVector3 mv3LinearVelocity;

    static SlotPool<BallCacheInfo> mBallCacheInfoSlotPool;
}; // total size: 0x1C

class FakeBallWorld
{
public:
    FakeBallWorld(cBall* pBall);
    ~FakeBallWorld();

    cBall* GetBall() const
    {
        return mpBall;
    }

    static BallCacheInfo* AddCacheEntry(float fTime, PhysicsBall* pPhysicsBall);
    static void GetNextBallPosVel(nlVector3& v3BallPos, nlVector3& v3BallVel);
    void SetHitInfo(dContactGeom* info);
    static bool FindBallIntercept(const nlVector3& v3PlayerPos, float fPlayerReach, float fPlayerSpeed, nlVector3& v3InterceptPos, nlVector3& v3InterceptVel, float& fInterceptTime, float& fClosestDist, float fMaxTime);
    static void GetNextBallPosition(nlVector3& v3BallPos);
    static void ResetBallIterator();
    static float GetPredictedPosAtDistance(float fDistance, nlVector3& v3Position, nlVector3& v3Velocity);
    static float GetPredictedHeightLimitTime(float fHeight, float fMinTime, nlVector3& v3ContactPoint, nlVector3& v3ContactVelocity, bool bDownOnly);
    static float GetPredictedPlaneIntersectTime(const nlVector4& v4Plane, nlVector3& v3ContactPoint, nlVector3& v3ContactVelocity);
    static void ClearBallCache();
    static bool GetPredictedBallPosition(float fDeltaTime, nlVector3& v3Position, nlVector3& v3Velocity);
    static void InvalidateBallCache();
    static void Destroy();
    static void Init(cBall* pBall);

    /* 0x00 */ cBall* mpBall;                    // offset 0x0, size 0x4
    /* 0x04 */ float mfElapsedTime;              // offset 0x4, size 0x4
    /* 0x08 */ FakePhysicsBall* mpPhysicsBall;   // offset 0x8, size 0x4
    /* 0x0C */ CollisionSpace* mpCollisionSpace; // offset 0xC, size 0x4
    /* 0x10 */ PhysicsWorld* mpPhysicsWorld;     // offset 0x10, size 0x4
    /* 0x14 */ PhysicsPlane* mpGroundPlane;      // offset 0x14, size 0x4
    /* 0x18 */ bool mbHitSuccess;                // offset 0x18, size 0x1
    /* 0x1C */ dContactGeom mContactInfo;        // offset 0x1C, size 0x2C

    static nlDLListIterator<BallCacheInfo*>* mpCacheIterator;
    static nlDLListSlotPool<BallCacheInfo*> mBallCacheList;
    static float mfLastCacheTime;
    static FakeBallWorld* mpPredictWorld;
}; // total size: 0x48

class PhysicsGoaliePlane : public PhysicsPlane
{
public:
    PhysicsGoaliePlane(const nlVector4& plane, FakeBallWorld& fakeBallWorld);
    virtual int GetObjectType() const { return 0x6; }
    virtual ContactType Contact(PhysicsObject* object, dContact* info, int numContacts);

    /* 0x2C */ FakeBallWorld& mWorld;
}; // total size: 0x30

class FakePhysicsBall : public PhysicsBall
{
public:
    FakePhysicsBall(float radius, FakeBallWorld& fakeBallWorld);
    virtual int GetObjectType() const { return 0x10; }
    virtual ContactType Contact(PhysicsObject* object, dContact* contact, int numContacts);

    /* 0x40 */ FakeBallWorld& mWorld;
}; // total size: 0x44

#endif // _PHYSICSFAKEBALL_H_
