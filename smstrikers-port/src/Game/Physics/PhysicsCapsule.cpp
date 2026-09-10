#include "Game/Physics/PhysicsCapsule.h"
#include "Game/Physics/PhysicsObject.h"

void PhysicsCapsule::SetDensity(float density)
{
    float radius;
    float length;

    dMass m;
    dMassSetZero(&m);
    dGeomCCylinderGetParams(m_geomID, &radius, &length);
    dMassSetCappedCylinder(&m, density, 3, radius, length);
    dBodySetMass(m_bodyID, &m);
}

void PhysicsCapsule::GetDimensions(float* radius, float* length)
{
    dGeomCCylinderGetParams(m_geomID, radius, length);
}

void PhysicsCapsule::SetDimensions(float radius, float length)
{
    dGeomCCylinderSetParams(m_geomID, radius, length);
}

void PhysicsCapsule::SetRadius(float radius)
{
    float oldRadius;
    float length;

    dGeomCCylinderGetParams(m_geomID, &oldRadius, &length);
    dGeomCCylinderSetParams(m_geomID, radius, length);
}

void PhysicsCapsule::SetLength(float length)
{
    float radius;
    float oldLength;

    dGeomCCylinderGetParams(m_geomID, &radius, &oldLength);
    dGeomCCylinderSetParams(m_geomID, radius, length);
}

float PhysicsCapsule::GetFullHeight()
{
    float radius;
    float length;

    dGeomCCylinderGetParams(m_geomID, &radius, &length);
    return length + radius * 2.0f;
}

/**
 * Offset/Address/Size: 0x0 | 0x801FE7A0 | size: 0xE8
 */
PhysicsCapsule::PhysicsCapsule(CollisionSpace* collisionSpace, PhysicsWorld* world, float radius, float length)
    : PhysicsObject(world)
{
    dMass m;
    dMassSetZero(&m);

    dSpaceID spaceID = NULL;
    if (collisionSpace != NULL)
    {
        spaceID = collisionSpace->m_spaceID;
    }

    m_geomID = dCreateCCylinder(spaceID, radius, length);

    if (m_bodyID != NULL)
    {
        dGeomSetBody(m_geomID, m_bodyID);
        dMassSetCappedCylinderTotal(&m, 1.0f, 3, radius, length);
        dBodySetMass(m_bodyID, &m);
    }

    dGeomSetData(m_geomID, this);
    SetDefaultCollideBits();
}
