#ifndef _PHYSICSSPHERE_H_
#define _PHYSICSSPHERE_H_

#include "Game/Physics/PhysicsObject.h"

class CollisionSpace;
class PhysicsWorld;

class PhysicsSphere : public PhysicsObject
{
public:
    PhysicsSphere(CollisionSpace*collisionSpace, PhysicsWorld*world, float radius);
    // virtual ~PhysicsSphere() { };
    virtual int GetObjectType() const { return 0x0A; };
    void SetRadius(float radius);
    float GetRadius();
}; // total size: 0x2C

#endif // _PHYSICSSPHERE_H_
