#include "Game/Physics/PhysicsSphere.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsObject.h"

/**
 * Offset/Address/Size: 0x0 | 0x80201240 | size: 0x24
 */
void PhysicsSphere::SetRadius(float radius)
{
    dGeomSphereSetRadius(m_geomID, radius);
}

/**
 * Offset/Address/Size: 0x24 | 0x80201264 | size: 0x24
 */
float PhysicsSphere::GetRadius()
{
    return dGeomSphereGetRadius(m_geomID);
}

/**
 * Offset/Address/Size: 0x48 | 0x80201288 | size: 0xC8
 */
PhysicsSphere::PhysicsSphere(CollisionSpace* collisionSpace, PhysicsWorld* world, float radius)
    : PhysicsObject(world)
{
    dMass m;
    dMassSetZero(&m);

    dSpaceID spaceID = NULL;
    if (collisionSpace != NULL)
    {
        spaceID = collisionSpace->m_spaceID;
    }

    m_geomID = dCreateSphere(spaceID, radius);

    if (m_bodyID != NULL)
    {
        dGeomSetBody(m_geomID, m_bodyID);
        dMassSetSphereTotal(&m, 1.0f, radius);
        dBodySetMass(m_bodyID, &m);
    }

    dGeomSetData(m_geomID, this);
    SetDefaultCollideBits();
}
