#ifndef _COLLISIONSPACE_H_
#define _COLLISIONSPACE_H_

#include "Game/Physics/PhysicsWorld.h"
#include "Game/Physics/PhysicsObject.h"

#include "ode/collision.h"
#include "ode/common.h"

class PhysicsWorld;
class PhysicsObject;
class CollisionSpace;

class CollisionSpace
{
public:
    CollisionSpace(PhysicsWorld* physicsWorld)
    {
        m_physicsWorld = physicsWorld;
        physicsWorld->AddCollisionSpace(this);
    }
    virtual ~CollisionSpace();
    void PreUpdate();
    void PreCollide();
    void CallPreCollide(PhysicsObject* object);
    void DoCollide(void* data, dNearCallback* callback);

    /* 0x04 */ dSpaceID m_spaceID;
    /* 0x08 */ CollisionSpace* m_nextCollisionSpace;
    /* 0x0c */ PhysicsWorld* m_physicsWorld;
};

extern CollisionSpace* g_CollisionSpace;

#endif // _COLLISIONSPACE_H_
