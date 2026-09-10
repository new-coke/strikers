#ifndef _PHYSICSGROUNDPLANE_H_
#define _PHYSICSGROUNDPLANE_H_

#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsPlane.h"

class PhysicsGroundPlane : public PhysicsPlane
{
public:
    PhysicsGroundPlane(CollisionSpace* collision_space);
    virtual ~PhysicsGroundPlane() { };

    virtual int GetObjectType() const
    {
        return 0x11;
    }
};

#endif // _PHYSICSGROUNDPLANE_H_
