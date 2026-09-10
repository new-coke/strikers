#include "Game/Physics/PhysicsGroundPlane.h"

#include "NL/nlMath.h"
#include "Game/Physics/PhysicsPlane.h"

/**
 * Offset/Address/Size: 0x0 | 0x8013C390 | size: 0x4C
 */
PhysicsGroundPlane::PhysicsGroundPlane(CollisionSpace* collision_space)
    : PhysicsPlane(collision_space, 0.f, 0.f, 1.f, 0.f)
{
}

// PhysicsGroundPlane::~PhysicsGroundPlane()
// {
// }
