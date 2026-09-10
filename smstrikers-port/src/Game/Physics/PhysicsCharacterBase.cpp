#include "Game/Physics/CharacterPhysicsElement.h"
#include "Game/Physics/PhysicsCharacterBase.h"
#include "Game/PoseAccumulator.h"

void dBodySetUpdateMode(dxBody*, int, int);
dxJoint* dJointCreateCharacter(dxWorld*, dxJointGroup*);
void dJointSetCharacterNoMotionDirection(dxJoint*, float*);

/**
 * Offset/Address/Size: 0x9D0 | 0x801FF4CC | size: 0x30
 */
int PhysicsCharacterBase::GetNumBoneVolumePoints(bool includeEndpoints) const
{
    int count = 0;
    for (ListEntry<PhysicsBoneVolume*>* entry = m_BoneVolumes.m_Head; entry != NULL; entry = entry->next)
    {
        count++;
    }
    if (includeEndpoints)
    {
        return count * 2;
    }
    return count * 6;
}

/**
 * Offset/Address/Size: 0x864 | 0x801FF360 | size: 0x16C
 */
void PhysicsCharacterBase::GetBoneVolumePoints(nlVector3* points, bool includeEndpoints)
{
    for (ListEntry<PhysicsBoneVolume*>* entry = m_BoneVolumes.m_Head; entry != NULL; entry = entry->next)
    {
        PhysicsBoneVolume* boneVolume = entry->entry;
        nlVector3 pos;
        boneVolume->m_pObject->GetPosition(&pos);

        if (includeEndpoints)
        {
            f32 radius = 0.333f;
            nlVec3Set(points[0], radius + pos.x, radius + pos.y, radius + pos.z);
            nlVec3Set(points[1], pos.x - radius, pos.y - radius, pos.z - radius);
            points += 2;
        }
        else
        {
            f32 radius = 0.333f;
            nlVec3Set(points[0], radius + pos.x, pos.y, pos.z);
            nlVec3Set(points[1], pos.x - radius, pos.y, pos.z);
            nlVec3Set(points[2], pos.x, radius + pos.y, pos.z);
            nlVec3Set(points[3], pos.x, pos.y - radius, pos.z);
            nlVec3Set(points[4], pos.x, pos.y, radius + pos.z);
            nlVec3Set(points[5], pos.x, pos.y, pos.z - radius);
            points += 6;
        }
    }
}

/**
 * Offset/Address/Size: 0x784 | 0x801FF280 | size: 0xE0
 */
PhysicsCharacterBase::PhysicsCharacterBase(CollisionSpace* collisionSpace, PhysicsWorld* physicsWorld, float centreOfMassHeight)
    : PhysicsCompositeObject(physicsWorld)
    , m_CentreOfMassHeight(centreOfMassHeight)
{
    dBodySetUpdateMode(m_bodyID, 1, 0);
    dBodySetGravityMode(m_bodyID, 0);

    m_CharMoveJoint = dJointCreateCharacter(physicsWorld->m_World, NULL);
    dJointAttach(m_CharMoveJoint, m_bodyID, NULL);

    float norm[4];
    norm[0] = 0.0f;
    norm[1] = 0.0f;
    norm[2] = 1.0f;
    dJointSetCharacterNoMotionDirection(m_CharMoveJoint, norm);

    dBodySetAutoDisableFlag(m_bodyID, 0);
}

/**
 * Offset/Address/Size: 0x69C | 0x801FF198 | size: 0xE8
 */
PhysicsCharacterBase::~PhysicsCharacterBase()
{
    dJointDestroy(m_CharMoveJoint);

    ListEntry<PhysicsBoneVolume*>* entry = m_BoneVolumes.m_Head;
    while (entry != NULL)
    {
        delete entry->entry;
        entry = entry->next;
    }
}

/**
 * Offset/Address/Size: 0x654 | 0x801FF150 | size: 0x48
 */
void PhysicsCharacterBase::SetCharacterPosition(const nlVector3& pos)
{
    nlVector3 newPos;
    newPos.x = pos.x;
    newPos.y = pos.y;
    newPos.z = pos.z + m_CentreOfMassHeight;
    SetPosition(newPos, WORLD_COORDINATES);
}

static inline PhysicsCharacterBase* IsRootParentCharacter(PhysicsObject* obj)
{
    PhysicsObject* temp = obj;
    int id = temp->GetObjectType();

    if (id == 0xE || id == 0xD)
    {
        PhysicsObject* parent;
        while ((parent = temp->m_parentObject) != NULL)
        {
            temp = parent;
        }
    }

    return temp->GetObjectType() == 0x8 ? (PhysicsCharacterBase*)temp : NULL;
}

/**
 * Offset/Address/Size: 0x524 | 0x801FF020 | size: 0x130
 */
bool PhysicsCharacterBase::BaseSetContactInfo(dContact* contact, PhysicsObject* other, bool first)
{
    int objectType = other->GetObjectType();

    if (first)
    {
        SetDefaultContactInfo(contact);

        PhysicsCharacterBase* owner = IsRootParentCharacter(other);
        if (owner != NULL)
        {
            contact->surface.mode &= ~(dContactBounce);
            contact->surface.bounce = 0.0f;
            contact->surface.bounce_vel = 0.0f;
        }
    }

    contact->surface.mode |= 0x18;
    contact->surface.soft_erp = 0.2f;
    contact->surface.soft_cfm = 1e-5f;
    contact->surface.mu = 0.0f;

    if (objectType == 0x6 || objectType == 0xB)
    {
        contact->surface.mode &= ~(dContactBounce);
        contact->surface.bounce = 0.0f;
        contact->surface.bounce_vel = 0.0f;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x518 | 0x801FF014 | size: 0xC
 */
void PhysicsCharacterBase::BasePreCollide()
{
    m_IsSupported = 0;
}

/**
 * Offset/Address/Size: 0x4E4 | 0x801FEFE0 | size: 0x34
 */
void PhysicsCharacterBase::ContainObject(PhysicsObject* obj)
{
    if (obj->m_parentObject != this)
    {
        m_SubObject.Attach(obj, this);
    }
}

/**
 * Offset/Address/Size: 0x4C0 | 0x801FEFBC | size: 0x24
 */
void PhysicsCharacterBase::ReleaseObject()
{
    m_SubObject.Release();
}

/**
 * Offset/Address/Size: 0x460 | 0x801FEF5C | size: 0x60
 */
void PhysicsCharacterBase::SetFacingDirection(unsigned short angle)
{
    float radians = (float)angle * (6.28318530718f / 65536.0f);
    nlMatrix4 matrix;
    nlMakeRotationMatrixZ(matrix, radians);
    SetRotation(matrix);
}

/**
 * Offset/Address/Size: 0x300 | 0x801FEDFC | size: 0x160
 */
void PhysicsCharacterBase::UpdatePose(cPoseAccumulator* pose, float heightOffset)
{
    for (ListEntry<PhysicsBoneVolume*>* entry = m_BoneVolumes.m_Head; entry != NULL; entry = entry->next)
    {
        PhysicsBoneVolume* boneVolume = entry->entry;

        // Store previous position
        nlVector3& pos = boneVolume->m_pObject->GetPosition();
        boneVolume->m_PrevPosition = pos;

        // Get bone matrix from pose accumulator and multiply with transform
        nlMatrix4 worldMatrix = pose->GetNodeMatrix(boneVolume->m_BoneIndex);
        nlMultMatrices(worldMatrix, boneVolume->m_Transform, pose->GetNodeMatrix(boneVolume->m_BoneIndex));

        // Adjust Z position for center of mass
        worldMatrix.m43 -= m_CentreOfMassHeight;
        worldMatrix.m43 += heightOffset;

        // Apply the transform
        AdjustTransform(boneVolume->m_TransformHandle, worldMatrix);
    }
}

/**
 * Not present in the retail image: this TU holds the only definition and no
 * call site survives, so the linker drops the body. DWARF (debug build CU for
 * PhysicsCharacterBase.cpp) still records the member, its signature and its
 * single PhysicsBoneVolume* local.
 */
void PhysicsCharacterBase::GetBonePositions(PhysicsBoneID boneID, nlVector3& v3CurPos, nlVector3& v3PrevPos)
{
    for (ListEntry<PhysicsBoneVolume*>* entry = m_BoneVolumes.m_Head; entry != NULL; entry = entry->next)
    {
        PhysicsBoneVolume* boneVolume = entry->entry;
        if (boneVolume->m_ID == boneID)
        {
            v3CurPos = boneVolume->m_pObject->GetPosition();
            v3PrevPos = boneVolume->m_PrevPosition;
            return;
        }
    }

    nlVec3Set(v3CurPos, 0.0f, 0.0f, 0.0f);
    nlVec3Set(v3PrevPos, 0.0f, 0.0f, 0.0f);
}

PhysicsCharacterBase::PhysicsBoneVolume* PhysicsCharacterBase::AddBoneVolume(
    PhysicsObject* object, unsigned int boneIndex, unsigned int transformHandle, nlMatrix4& transform, PhysicsBoneID id)
{
    return new (nlMalloc(sizeof(PhysicsBoneVolume), 8, false))
        PhysicsBoneVolume(object, boneIndex, transformHandle, transform, id);
}

/**
 * Offset/Address/Size: 0x80 | 0x801FEB7C | size: 0x280
 */
void PhysicsCharacterBase::AddBoneVolumes(PhysicsWorld* physicsWorld, CollisionSpace* collisionSpace, cPoseAccumulator* poseAccumulator,
    const CharacterPhysicsData* pPhysicsData, unsigned long categoryBitmask, unsigned long collisionBitmask)
{
    unsigned int i;
    CharacterPhysicsElement* physElement;
    PhysicsObject* obj;
    unsigned long parentNodeIndex;
    int handle;
    PhysicsBoneID id;

    for (i = 0; i < pPhysicsData->physicsElementCount; i++)
    {
        physElement = &pPhysicsData->pPhysicsElements[i];
        obj = NULL;

        switch (physElement->uPrimitiveType)
        {
        case 1:
        {
            PhysicsSphereBone* sphere = new (nlMalloc(sizeof(PhysicsSphereBone), 8, false))
                PhysicsSphereBone(collisionSpace, physicsWorld, physElement->fRadius);
            obj = sphere;
            obj->GetObjectType();
            break;
        }

        case 2:
        {
            PhysicsCapsuleBone* capsule = new (nlMalloc(sizeof(PhysicsCapsuleBone), 8, false))
                PhysicsCapsuleBone(collisionSpace, physicsWorld, physElement->fRadius, physElement->fHeight);
            obj = capsule;
            break;
        }

        default:
            break;
        }

        obj->SetCategory(categoryBitmask);
        obj->SetCollide(collisionBitmask);

        parentNodeIndex = poseAccumulator->m_BaseSHierarchy->GetNodeIndexByID(physElement->uParentHashID);
        handle = AddObject(obj);
        id = ResolvePhysicsBoneIDFromName((const char*)physElement->szName);
        PhysicsBoneVolume* boneVolume = AddBoneVolume(obj, parentNodeIndex, handle, physElement->matLocalToParent, id);

        m_BoneVolumes.AddEntry(boneVolume);

        switch (physElement->uPrimitiveType)
        {
        case 1:
            ((PhysicsSphereBone*)obj)->m_boneVolume = boneVolume;
            break;

        case 2:
            ((PhysicsCapsuleBone*)obj)->m_boneVolume = boneVolume;
            break;

        default:
            break;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801FEAFC | size: 0x80
 */
PhysicsBoneID PhysicsCharacterBase::GetBoneIDForSubObject(const PhysicsObject* obj) const
{
    switch (obj->GetObjectType())
    {
    case 0xD:
    {
        const PhysicsSphereBone* bone = static_cast<const PhysicsSphereBone*>(obj);
        if (bone->m_boneVolume != NULL)
        {
            return bone->m_boneVolume->m_ID;
        }
        break;
    }
    case 0xE:
    {
        const PhysicsCapsuleBone* bone = static_cast<const PhysicsCapsuleBone*>(obj);
        if (bone->m_boneVolume != NULL)
        {
            return bone->m_boneVolume->m_ID;
        }
        break;
    }
    }
    return PHYSBONE_SINGLE_CAPSULE;
}
