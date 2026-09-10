#include "Game/Physics/PhysicsRoundedCorner.h"

#include "ode/ext/dRoundedCorner.h"

/**
 * Offset/Address/Size: 0x0 | 0x801FEA3C | size: 0xC0
 */
PhysicsRoundedCorner::PhysicsRoundedCorner(CollisionSpace* collisionSpace, const nlVector2& position, float radius, bool isTrigger, bool isStatic)
    : PhysicsObject(NULL)
{
    dMass m;
    dMassSetZero(&m);

    dSpaceID spaceID = NULL;
    if (collisionSpace != NULL)
    {
        spaceID = collisionSpace->m_spaceID;
    }

    m_geomID = dCreateRoundedCorner(spaceID, radius, isTrigger, isStatic);
    m_bodyID = NULL;

    dGeomSetData(m_geomID, this);
    SetDefaultCollideBits();

    dGeomSetPosition(m_geomID, position.x, position.y, 0.0f);
}
