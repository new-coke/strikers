#ifndef _RAYCOLLIDER_H_
#define _RAYCOLLIDER_H_

#include "NL/nlMath.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsObject.h"

class RayCollider
{
public:
    RayCollider(float length, const nlVector3& pos, const nlVector3& dir);
    ~RayCollider();

    virtual bool DoCollide(CollisionSpace* space);

    /* 0x04 */ dGeomID m_rayID;
    /* 0x08 */ PhysicsObject* m_collidedObj;
    /* 0x0C */ nlVector3 m_contactPos;
    /* 0x18 */ nlVector3 m_contactNormal;
    /* 0x24 */ f32 m_contactDepth;
    /* 0x28 */ bool m_hasCollision;
};

#endif // _RAYCOLLIDER_H_
