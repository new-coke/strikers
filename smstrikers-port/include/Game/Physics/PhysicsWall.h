#ifndef _PHYSICSWALL_H_
#define _PHYSICSWALL_H_

#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsPlane.h"

class PhysicsWall : public PhysicsPlane
{
public:
    PhysicsWall(CollisionSpace* collision_space, float a, float b, float c);

    virtual ~PhysicsWall() { };

    virtual int GetObjectType() const
    {
        return 0x19;
    };
};

#endif // _PHYSICSWALL_H_
