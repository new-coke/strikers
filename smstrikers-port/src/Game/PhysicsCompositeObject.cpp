#include "Game/Physics/PhysicsCompositeObject.h"

#include "Game/Physics/PhysicsWorld.h"
#include "Game/Physics/PhysicsObject.h"

#include "NL/nlMemory.h"

// NOTE: functions are intentionally in REVERSE target order (ctor, dtor,
// AddObject, AdjustTransform). This TU is built with `-inline deferred`,
// under which MWCC emits bottom-up; the reversal makes .text emit forward
// AND places the weak vtable copy (GetObjectType) before the DLListContainer

/**
 * Offset/Address/Size: 0x274 | 0x801FF91C | size: 0x54
 */
PhysicsCompositeObject::PhysicsCompositeObject(PhysicsWorld* physicsWorld)
    : PhysicsObject(physicsWorld)
{
    numComponents = 0;
    dBodySetData(m_bodyID, this);
}

/**
 * Offset/Address/Size: 0x164 | 0x801FF80C | size: 0x110
 */
PhysicsCompositeObject::~PhysicsCompositeObject()
{
    DLListEntry<PhysicsTransform*>* start;
    DLListEntry<PhysicsTransform*>* head;
    DLListEntry<PhysicsTransform*>* current;

    start = nlDLRingGetStart<DLListEntry<PhysicsTransform*> >(m_Components.m_Head);
    head = m_Components.m_Head;
    current = start;

    while (current != NULL)
    {
        PhysicsTransform* physObj = (PhysicsTransform*)current->entry;

        physObj->m_bodyID = NULL;
        delete physObj;

        if (nlDLRingIsEnd<DLListEntry<PhysicsTransform*> >(head, current) || current == NULL)
        {
            current = NULL;
        }
        else
        {
            current = current->m_next;
        }
    }
}

/**
 * Offset/Address/Size: 0xA8 | 0x801FF750 | size: 0xBC
 */
int PhysicsCompositeObject::AddObject(PhysicsObject* object)
{
    object->MakeStatic();
    PhysicsTransform* transform = new (nlMalloc(sizeof(PhysicsTransform), 8, false)) PhysicsTransform();

    transform->Attach(object, this);

    DLListEntry<PhysicsTransform*>* entry = // PORT: 0xC is the console's three 4-byte pointers.
        (DLListEntry<PhysicsTransform*>*)nlMalloc(
            sizeof(DLListEntry<PhysicsTransform*>), 8, 0);

    if (entry != nullptr)
    {
        entry->m_next = nullptr;
        entry->m_prev = nullptr;
        entry->entry = transform;
    }

    nlDLRingAddEnd<DLListEntry<PhysicsTransform*> >(&m_Components.m_Head, entry);

    numComponents++;

    return numComponents - 1;
}

/**
 * Offset/Address/Size: 0x0 | 0x801FF6A8 | size: 0xA8
 */
void PhysicsCompositeObject::AdjustTransform(int i, nlMatrix4& m)
{
    GetComponent(i)->SetSubObjectTransform(m, PhysicsObject::RELATIVE_TO_PARENT);
}

PhysicsTransform* PhysicsCompositeObject::GetComponent(unsigned int i)
{
    DLListEntry<PhysicsTransform*>* current;
    DLListEntry<PhysicsTransform*>* head;
    unsigned int count;
    DLListEntry<PhysicsTransform*>* start = nlDLRingGetStart<DLListEntry<PhysicsTransform*> >(m_Components.m_Head);

    head = m_Components.m_Head;
    count = 0;
    current = start;

    for (; count < i; count++)
    {
        if (nlDLRingIsEnd<DLListEntry<PhysicsTransform*> >(head, current))
        {
            return NULL;
        }

        if (nlDLRingIsEnd<DLListEntry<PhysicsTransform*> >(head, current) || current == 0)
        {
            current = 0;
        }
        else
        {
            current = current->m_next;
        }
    }

    return current->entry;
}
