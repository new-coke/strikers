#include "Game/Physics/PhysicsWall.h"

/**
 * Offset/Address/Size: 0x0 | 0x8013C454 | size: 0x48
 */
PhysicsWall::PhysicsWall(CollisionSpace* collision_space, float a, float b, float c)
    : PhysicsPlane(collision_space, a, b, 0.f, -c)
{
}
