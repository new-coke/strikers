#include "Game/Physics/PhysicsBox.h"
#include "NL/nlMath.h"
#include "Game/Physics/PhysicsObject.h"

void PhysicsBox::GetSides(float* sides)
{
    dVector3 dimensions;

    dGeomBoxGetLengths(m_geomID, dimensions);
    sides[0] = dimensions[0];
    sides[1] = dimensions[1];
    sides[2] = dimensions[2];
}

/**
 * Offset/Address/Size: 0x0 | 0x801FE628 | size: 0x74
 */
void PhysicsBox::SetDensity(float density)
{
    dVector3 dimensions;

    dMass m;
    dMassSetZero(&m);
    dGeomBoxGetLengths(m_geomID, dimensions);
    dMassSetBox(&m, density, (double)dimensions[0], (double)dimensions[1], (double)dimensions[2]);
    dBodySetMass(m_bodyID, &m);
}

/**
 * Offset/Address/Size: 0x74 | 0x801FE69C | size: 0x104
 */
PhysicsBox::PhysicsBox(CollisionSpace* collisionSpace, PhysicsWorld* world, float lx, float ly, float lz)
    : PhysicsObject(world)
{
    dMass m;
    dMassSetZero(&m);

    dSpaceID spaceID = NULL;
    if (collisionSpace != NULL)
    {
        spaceID = collisionSpace->m_spaceID;
    }

    m_geomID = dCreateBox(spaceID, lx, ly, lz);

    if (m_bodyID != NULL)
    {
        dGeomSetBody(m_geomID, m_bodyID);
        dMassSetBoxTotal(&m, lx * ly * lz, lx, ly, lz);
        dBodySetMass(m_bodyID, &m);
    }

    dGeomSetData(m_geomID, this);
    SetDefaultCollideBits();
}
