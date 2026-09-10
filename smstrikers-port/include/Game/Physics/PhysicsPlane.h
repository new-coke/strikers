#ifndef _PHYSICSPLANE_H_
#define _PHYSICSPLANE_H_

#include "Game/Physics/PhysicsObject.h"
#include "Game/Physics/CollisionSpace.h"

class CollisionSpace;

class PhysicsPlane : public PhysicsObject
{
public:
    virtual int GetObjectType() const { return 0x6; }
    PhysicsPlane(CollisionSpace* collision_space, float a, float b, float c, float d);
};

#endif // _PHYSICSPLANE_H_
