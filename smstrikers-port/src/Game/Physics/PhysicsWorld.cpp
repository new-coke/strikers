#include <stdio.h>
#include <stdlib.h>
#include "port/input.h"
#include "Game/Physics/CollisionSpace.h"

#include "NL/nlList.h"
#include "ode/NLGAdditions.h"

/**
 * Offset/Address/Size: 0x744 | 0x80201E44 | size: 0x4C
 */
PhysicsWorld::PhysicsWorld()
{
    m_World = dWorldCreate();
    m_ContactGroup = dJointGroupCreate(0);
    m_SpaceList = NULL;
    m_SyncLogFile = NULL;
}

/**
 * Offset/Address/Size: 0x6E8 | 0x80201DE8 | size: 0x5C
 */
PhysicsWorld::~PhysicsWorld()
{
    dJointGroupDestroy(m_ContactGroup);
    dWorldDestroy(m_World);
}

/**
 * Offset/Address/Size: 0x6C4 | 0x80201DC4 | size: 0x24
 */
void PhysicsWorld::SetCFM(float cfm)
{
    dWorldSetCFM(m_World, cfm);
}

/**
 * Offset/Address/Size: 0x6A0 | 0x80201DA0 | size: 0x24
 */
void PhysicsWorld::SetERP(float erp)
{
    dWorldSetERP(m_World, erp);
}

/**
 * Offset/Address/Size: 0x690 | 0x80201D90 | size: 0x10
 */
void PhysicsWorld::AddCollisionSpace(CollisionSpace* collisionSpace)
{
    collisionSpace->m_nextCollisionSpace = m_SpaceList;
    m_SpaceList = collisionSpace;
}

/**
 * Offset/Address/Size: 0x66C | 0x80201D6C | size: 0x24
 */
void PhysicsWorld::PreCollide(CollisionSpace* collisionSpace)
{
    collisionSpace->PreCollide();
}

/**
 * Offset/Address/Size: 0x63C | 0x80201D3C | size: 0x30
 */
void PhysicsWorld::PreCollide(PhysicsObject* object)
{
    object->PreCollide();
}

/**
 * Offset/Address/Size: 0x618 | 0x80201D18 | size: 0x24
 */
void PhysicsWorld::PreUpdate(CollisionSpace* collisionSpace)
{
    collisionSpace->PreUpdate();
}

/**
 * Offset/Address/Size: 0x5E8 | 0x80201CE8 | size: 0x30
 */
void PhysicsWorld::PreUpdate(PhysicsObject* object)
{
    object->PreUpdate();
}

/**
 * Offset/Address/Size: 0x5B8 | 0x80201CB8 | size: 0x30
 */
void PhysicsWorld::PostUpdate(PhysicsObject* object)
{
    object->PostUpdate();
}

/**
 * Offset/Address/Size: 0x584 | 0x80201C84 | size: 0x34
 */
void PhysicsWorld::DoCollide(CollisionSpace* collisionSpace)
{
    collisionSpace->DoCollide(this, &SpaceCollideCallback);
}

/**
 * Offset/Address/Size: 0x464 | 0x80201B64 | size: 0x120
 */
void PhysicsWorld::Collide()
{
    void (PhysicsWorld::*pmf)(PhysicsObject*) = &PhysicsWorld::PreCollide;
    for (dBodyID bodyID = dWorldGetFirstBody(m_World); bodyID != NULL; bodyID = dBodyGetNextBody(bodyID))
    {
        (this->*pmf)((PhysicsObject*)dBodyGetData(bodyID));
    }

    void (PhysicsWorld::*pmf2)(CollisionSpace*) = &PhysicsWorld::PreCollide;
    for (CollisionSpace* space = m_SpaceList; space != NULL; space = space->m_nextCollisionSpace)
    {
        (this->*pmf2)(space);
    }

    void (PhysicsWorld::*pmf3)(CollisionSpace*) = &PhysicsWorld::DoCollide;
    for (CollisionSpace* space = m_SpaceList; space != NULL; space = space->m_nextCollisionSpace)
    {
        (this->*pmf3)(space);
    }
}

/**
 * Offset/Address/Size: 0x38C | 0x80201A8C | size: 0xD8
 */
void PhysicsWorld::PreUpdate()
{
    void (PhysicsWorld::*pmf)(CollisionSpace*) = &PhysicsWorld::PreUpdate;
    for (CollisionSpace* space = m_SpaceList; space != NULL; space = space->m_nextCollisionSpace)
    {
        (this->*pmf)(space);
    }

    void (PhysicsWorld::*pmf2)(PhysicsObject*) = &PhysicsWorld::PreUpdate;
    for (dBodyID bodyID = dWorldGetFirstBody(m_World); bodyID != NULL; bodyID = dBodyGetNextBody(bodyID))
    {
        (this->*pmf2)((PhysicsObject*)dBodyGetData(bodyID));
    }
}

/**
 * Offset/Address/Size: 0x2FC | 0x802019FC | size: 0x90
 */
void PhysicsWorld::PostUpdate()
{
    void (PhysicsWorld::*pmf)(PhysicsObject*) = &PhysicsWorld::PostUpdate;
    for (dBodyID bodyID = dWorldGetFirstBody(m_World); bodyID != NULL; bodyID = dBodyGetNextBody(bodyID))
    {
        (this->*pmf)((PhysicsObject*)dBodyGetData(bodyID));
    }
}

/**
 * Offset/Address/Size: 0x29C | 0x8020199C | size: 0x60
 */
void PhysicsWorld::Update(float quickStepSize, bool doClear)
{
    dWorldSetClearAccumulators(m_World, doClear ? 1 : 0);
    dWorldQuickStep(m_World, quickStepSize);
    dJointGroupEmpty(m_ContactGroup);
}

/**
 * Offset/Address/Size: 0x78 | 0x80201778 | size: 0x224
 */
void PhysicsWorld::SpaceCollideCallback(void* data, dxGeom* geom1, dxGeom* geom2)
{
    int i;
    dContact contacts[20];
    dxBody* body1;
    dxBody* body2;
    PhysicsObject* physObj1;
    PhysicsObject* physObj2;

    body1 = dGeomGetBody(geom1);
    body2 = dGeomGetBody(geom2);

    if (body1 != NULL && body2 != NULL)
    {
        if (dAreConnectedExcluding(body1, body2, 4) != 0)
        {
            return;
        }
    }

    physObj1 = (PhysicsObject*)dGeomGetData(geom1);
    physObj2 = (PhysicsObject*)dGeomGetData(geom2);

    bool bCanCollide1 = physObj1->SetContactInfo(&contacts[0], physObj2, true);
    bool bCanCollide2 = physObj2->SetContactInfo(&contacts[0], physObj1, false);

    if (!bCanCollide1 || !bCanCollide2)
    {
        return;
    }

    // PORT: was `(u8*)contacts + 0x2C`, the console's offsetof(dContact, geom).
    int numContacts = dCollide(geom1, geom2, 20, &contacts[0].geom, sizeof(dContact));

    {
        static const bool bPairProbe = getenv("STRIKERS_PROBE_PAIRS") != NULL;
        if (bPairProbe && numContacts > 0)
        {
            double dmax = 0.0;
            for (int ci = 0; ci < numContacts; ci++)
                if (contacts[ci].geom.depth > dmax) dmax = contacts[ci].geom.depth;
            if (dmax > 0.5)
                fprintf(stderr, "[pair] frame %lu: 0x%X vs 0x%X  n=%d maxdepth %.3f\n",
                        PortInputFrame(), physObj1->GetObjectType(),
                        physObj2->GetObjectType(), numContacts, dmax);
        }
    }

    if (numContacts <= 0)
    {
        return;
    }

    ContactType contactResult1 = physObj1->Contact(physObj2, contacts, numContacts);
    ContactType contactResult2 = physObj2->Contact(physObj1, contacts, numContacts);

    ContactType transformedResult2;
    if (contactResult2 == ONE_WAY_CONTACT_OTHER)
    {
        transformedResult2 = ONE_WAY_CONTACT_THIS;
    }
    else if (contactResult2 == ONE_WAY_CONTACT_THIS)
    {
        transformedResult2 = ONE_WAY_CONTACT_OTHER;
    }
    else
    {
        transformedResult2 = contactResult2;
    }

    int combinedResult = contactResult1 & transformedResult2;
    if (combinedResult == 0)
    {
        return;
    }

    if (combinedResult == 2)
    {
        body1 = NULL;
    }
    if (combinedResult == 1)
    {
        body2 = NULL;
    }

    for (i = 0; i < numContacts; i++)
    {
        if (i != 0)
        {
            contacts[i].surface.mode = contacts[0].surface.mode;
            contacts[i].surface.mu = contacts[0].surface.mu;
            contacts[i].surface.mu2 = contacts[0].surface.mu2;
            contacts[i].surface.bounce = contacts[0].surface.bounce;
            contacts[i].surface.bounce_vel = contacts[0].surface.bounce_vel;
            contacts[i].surface.soft_erp = contacts[0].surface.soft_erp;
            contacts[i].surface.soft_cfm = contacts[0].surface.soft_cfm;
            contacts[i].surface.motion1 = contacts[0].surface.motion1;
            contacts[i].surface.motion2 = contacts[0].surface.motion2;
            contacts[i].surface.slip1 = contacts[0].surface.slip1;
            contacts[i].surface.slip2 = contacts[0].surface.slip2;
        }

        PhysicsWorld* world = (PhysicsWorld*)data;
        dJointID joint = dJointCreateContact(world->m_World, world->m_ContactGroup, &contacts[i]);
        dJointAttach(joint, body1, body2);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80201700 | size: 0x78
 */
void PhysicsWorld::DoCollisions(PhysicsObject* obj, nlListContainer<PhysicsObject*>& container)
{
    dGeomID geom = obj->m_geomID;

    for (ListEntry<PhysicsObject*>* entry = container.m_Head; entry != NULL; entry = entry->next)
    {
        PhysicsObject* otherObj = entry->entry;
        dGeomID otherGeom = otherObj->m_geomID;
        dGeomCollideAABBs(geom, otherGeom, this, SpaceCollideCallback);
    }
}
