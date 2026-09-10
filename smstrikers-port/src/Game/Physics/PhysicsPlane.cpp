#include "Game/Physics/PhysicsPlane.h"

/**
 * Offset/Address/Size: 0x0 | 0x80201198 | size: 0xA8
 */
PhysicsPlane::PhysicsPlane(CollisionSpace* collision_space, float a, float b, float c, float d)
    : PhysicsObject(NULL)
{
    dSpaceID space = NULL;
    if (collision_space != NULL)
    {
        space = collision_space->m_spaceID;
    }

    m_geomID = dCreatePlane(space, a, b, c, d);
    dGeomSetData(m_geomID, this);
    SetDefaultCollideBits();
}
