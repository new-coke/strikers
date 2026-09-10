#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsObject.h"
#include "Game/Physics/PhysicsWorld.h"

class SimpleCollisionSpace : public CollisionSpace
{
public:
    SimpleCollisionSpace(PhysicsWorld*);
    /**
     * Offset/Address/Size: 0x1D0 | 0x801FE5B8 | size: 0x70
     */
    virtual ~SimpleCollisionSpace() { };
};

/**
 * Offset/Address/Size: 0x170 | 0x801FE558 | size: 0x60
 */
CollisionSpace::~CollisionSpace()
{
    dSpaceDestroy(m_spaceID);
}

/**
 * Offset/Address/Size: 0x14C | 0x801FE534 | size: 0x24
 */
void CollisionSpace::DoCollide(void* data, dNearCallback* callback)
{
    dSpaceCollide(m_spaceID, data, callback);
}

/**
 * Offset/Address/Size: 0x11C | 0x801FE504 | size: 0x30
 */
void CollisionSpace::CallPreCollide(PhysicsObject* object)
{
    object->PreCollide();
}

/**
 * Offset/Address/Size: 0x64 | 0x801FE44C | size: 0xB8
 */
void CollisionSpace::PreCollide()
{
    void (CollisionSpace::*pmf)(PhysicsObject*) = &CollisionSpace::CallPreCollide;
    int numGeoms = dSpaceGetNumGeoms(m_spaceID);

    for (int i = 0; i < numGeoms; i++)
    {
        dGeomID geom = dSpaceGetGeom(m_spaceID, i);

        if (dGeomGetClass(geom) != dRayClass)
        {
            (this->*pmf)((PhysicsObject*)dGeomGetData(geom));
        }
    }
}

/**
 * Offset/Address/Size: 0x60 | 0x801FE448 | size: 0x4
 */
void CollisionSpace::PreUpdate()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x0 | 0x801FE3E8 | size: 0x60
 */
SimpleCollisionSpace::SimpleCollisionSpace(PhysicsWorld* physicsWorld)
    : CollisionSpace(physicsWorld)
{
    m_spaceID = dSimpleSpaceCreate(0);
}
