#include "Game/Physics/RayCollider.h"
#include "ode/collision.h"

/**
 * Offset/Address/Size: 0x0 | 0x80201E90 | size: 0x12C
 */
bool RayCollider::DoCollide(CollisionSpace* space)
{
    dContactGeom contacts[4];
    int numContacts = dCollide(m_rayID, (dxGeom*)space->m_spaceID, 4, contacts, sizeof(dContactGeom));
    m_hasCollision = numContacts > 0;
    if (!m_hasCollision)
    {
        return m_hasCollision;
    }

    s32 ctr = numContacts - 1;
    dContactGeom* p = contacts + 1;
    s32 closest = 0;
    s32 j = 1;
    for (; numContacts > 1; numContacts--)
    {
        if (p->depth < contacts[closest].depth)
        {
            closest = j;
        }
        p++;
        j++;
    }

    nlVec3Set(m_contactPos, contacts[closest].pos[0], contacts[closest].pos[1], contacts[closest].pos[2]);
    nlVec3Set(m_contactNormal, contacts[closest].normal[0], contacts[closest].normal[1], contacts[closest].normal[2]);
    m_contactDepth = contacts[closest].depth;

    dGeomID geom = contacts[closest].g1;
    if (geom == m_rayID)
    {
        geom = contacts[closest].g2;
    }

    m_collidedObj = (PhysicsObject*)dGeomGetData(geom);
    return m_hasCollision;
}

/**
 * Offset/Address/Size: 0x12C | 0x80201FBC | size: 0x60
 */
RayCollider::~RayCollider()
{
    dGeomDestroy(m_rayID);
}

/**
 * Offset/Address/Size: 0x18C | 0x8020201C | size: 0xA8
 */
RayCollider::RayCollider(float length, const nlVector3& pos, const nlVector3& dir)
{
    m_rayID = dCreateRay(NULL, length);
    dGeomSetData(m_rayID, 0);
    dGeomRaySet(m_rayID, pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);
    dGeomSetCollideBits(m_rayID, 6);
    dGeomSetCategoryBits(m_rayID, 0);
    m_hasCollision = false;
}
