#include "Game/Physics/PhysicsTransform.h"
#include "NL/nlMath.h"
#include "Game/Physics/PhysicsObject.h"

/**
 * Offset/Address/Size: 0x320 | 0x80201670 | size: 0x60
 */
PhysicsTransform::PhysicsTransform()
    : PhysicsObject(NULL)
{
    m_geomID = dCreateGeomTransform(NULL);
    dGeomTransformSetInfo(m_geomID, 1);

    m_SubObject = nullptr;
}

/**
 * Offset/Address/Size: 0x29C | 0x802015EC | size: 0x84
 */
PhysicsTransform::~PhysicsTransform()
{
    delete m_SubObject;
}

/**
 * Offset/Address/Size: 0x1C0 | 0x80201510 | size: 0xDC
 */
void PhysicsTransform::Attach(PhysicsObject* obj1, PhysicsObject* obj2)
{
    if (m_SubObject == NULL)
    {
        if (obj2 != NULL)
        {
            m_bodyID = obj2->m_bodyID;
        }

        dGeomSetData(m_geomID, obj1);

        dGeomSetCategoryBits(m_geomID, dGeomGetCategoryBits(obj1->m_geomID));

        dGeomSetCollideBits(m_geomID, dGeomGetCollideBits(obj1->m_geomID));

        dSpaceID space = obj1->Disconnect();
        dGeomTransformSetGeom(m_geomID, obj1->m_geomID);

        EnableCollisions();

        dSpaceAdd(space, m_geomID);
        dGeomSetBody(m_geomID, m_bodyID);

        obj1->m_parentObject = obj2;
        m_SubObject = obj1;
    }
}

/**
 * Offset/Address/Size: 0x94 | 0x802013E4 | size: 0x12C
 */
void PhysicsTransform::Release()
{
    nlMatrix4 mat;
    nlVector3 v1, v2, v;

    if (m_SubObject != NULL)
    {
        m_SubObject->m_parentObject = NULL;

        GetPosition(&v1);
        m_SubObject->GetPosition(&v2);

        dSpaceID space = dGeomGetSpace(m_geomID);
        if (space != NULL)
        {
            dSpaceRemove(space, m_geomID);
        }
        m_bodyID = NULL;

        dGeomTransformSetGeom(m_geomID, NULL);

        m_SubObject->Reconnect(space);
        GetRotation(&mat);

        nlMultPosVectorMatrix(v, v2, mat);
        v1.z = v1.z + v.z;
        v1.y = v1.y + v.y;
        v1.x = v1.x + v.x;
        m_SubObject->SetPosition(v1, WORLD_COORDINATES);
        v1.x = 0.f;
        v1.y = 0.f;
        v1.z = 0.f;
        m_SubObject->SetLinearVelocity(v1);
        m_SubObject->SetAngularVelocity(v1);
        m_SubObject->ZeroForceAccumulators();
        m_SubObject = NULL;
        DisableCollisions();
    }
}

/**
 * Offset/Address/Size: 0x70 | 0x802013C0 | size: 0x24
 */
void PhysicsTransform::SetSubObjectPosition(const nlVector3& position, PhysicsObject::CoordinateType coordinateType)
{
    m_SubObject->SetPosition(position, coordinateType);
}

/**
 * Offset/Address/Size: 0x0 | 0x80201350 | size: 0x70
 */
void PhysicsTransform::SetSubObjectTransform(const nlMatrix4& transform, PhysicsObject::CoordinateType coordinateType)
{
    nlVector3 v;
    m_SubObject->SetRotation(transform);
    nlVec3Set(v, transform.e2[3][0], transform.e2[3][1], transform.e2[3][2]);
    // NL_VECTOR3_SET(v, transform.e2[3][0], transform.e2[3][1], transform.e2[3][2]);
    m_SubObject->SetPosition(v, coordinateType);
}
