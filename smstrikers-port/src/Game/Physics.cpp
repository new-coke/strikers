#include "NL/nlList.h"
#include "Game/Physics/Physics.h"

#include "NL/nlMemory.h"
#include "Game/Ball.h"
#include "Game/BasicStadium.h"
#include "Game/Field.h"
#include "Game/Inventory.h"
#include "Game/Physics/CharacterPhysicsElement.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/LoadablePhysicsMesh.h"
#include "Game/Physics/PhysicsGroundPlane.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsNet.h"
#include "Game/Physics/PhysicsSphere.h"
#include "Game/Physics/PhysicsWall.h"
#include "ode/NLGAdditions.h"

typedef ListContainerBase<char*, NewAdapter<ListEntry<char*> > > PhysicsCharListBase;

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool sbDisableCollisionDetection;
static bool sbNonMovingAABBsInitialized;
nlListContainer<PhysicsObject*> g_StaticPhysicsPrimitives;
nlListContainer<PhysicsObject*> g_NetPhysicsObjects;
PhysicsMesh* g_TerrainMesh;
PhysicsWorld* g_PhysicsWorld;
CollisionSpace* g_CollisionSpace;
PhysicsLoader ThePhysicsLoader;
static cInventory<LoadablePhysicsMesh> s_PhysicsMeshes;
static PhysicsRoundedCorner* corners[4];
static float sfStaticFinitePlaneThinDepth = 0.75f;
static float sfStaticFinitePlaneThickDepth = 10.0f;

void dClearCachedData();

extern "C"
{
    // PORT: size_t, not unsigned long, ode/memory.h says size_t, and the two are the same type only on LP64.
    typedef void* dAllocFunction(size_t);
    typedef void* dReallocFunction(void*, size_t, size_t);
    typedef void dFreeFunction(void*, size_t);

    void dSetAllocHandler(dAllocFunction* fn);
    void dSetReallocHandler(dReallocFunction* fn);
    void dSetFreeHandler(dFreeFunction* fn);
}

// SimpleCollisionSpace has no dedicated header; it is declared locally in each
// translation unit that constructs one (see CollisionSpace.cpp, PhysicsFakeBall.cpp).
class SimpleCollisionSpace : public CollisionSpace
{
public:
    SimpleCollisionSpace(PhysicsWorld*);
    virtual ~SimpleCollisionSpace() { };
};

/**
 * Offset/Address/Size: 0xA78 | 0x80133588 | size: 0x28
 */
void* ODEAlloc(size_t size)
{
    return nlMalloc(size, 8, false);
}

/**
 * Offset/Address/Size: 0xA08 | 0x80133518 | size: 0x70
 */
void* ODERealloc(void* oldPtr, size_t oldSize, size_t newSize)
{
    void* newPtr = nlMalloc(newSize, 8, false);
    if (oldSize != 0)
    {
        memcpy(newPtr, oldPtr, oldSize);
    }
    nlFree(oldPtr);
    return newPtr;
}

/**
 * Offset/Address/Size: 0x9E8 | 0x801334F8 | size: 0x20
 */
void ODEFree(void* ptr, size_t size)
{
    nlFree(ptr);
}

/**
 * Offset/Address/Size: 0x728 | 0x80133238 | size: 0x2C0
 */
bool PhysicsLoader::StartLoad(LoadingManager* manager)
{
    const char* pBaseName;

    dSetAllocHandler(ODEAlloc);
    dSetReallocHandler(ODERealloc);
    dSetFreeHandler(ODEFree);

    g_PhysicsWorld = new (nlMalloc(sizeof(PhysicsWorld), 8, false)) PhysicsWorld();
    g_CollisionSpace = new (nlMalloc(sizeof(SimpleCollisionSpace), 8, false)) SimpleCollisionSpace(g_PhysicsWorld);

    g_PhysicsWorld->SetCFM(0.00001f);
    g_PhysicsWorld->SetERP(0.2f);

    g_StaticPhysicsPrimitives.AddEnd(
        new (nlMalloc(sizeof(PhysicsGroundPlane), 8, false)) PhysicsGroundPlane(g_CollisionSpace));

    int i;
    for (i = 0; i < 4; i++)
    {
        const sSideLinePlane& sideline = cField::GetSideline(i);
        g_StaticPhysicsPrimitives.AddEnd(
            new (nlMalloc(sizeof(PhysicsWall), 8, false)) PhysicsWall(
                g_CollisionSpace,
                sideline.vNormal.x,
                sideline.vNormal.y,
                sideline.fDistance));
    }

    for (i = 0; i < 4; i++)
    {
        const sCornerSegment& corner = cField::GetCorner(i);
        PhysicsRoundedCorner* pCorner = new (nlMalloc(sizeof(PhysicsRoundedCorner), 8, false)) PhysicsRoundedCorner(
            g_CollisionSpace,
            corner.vCenter,
            corner.fRadius,
            corner.vCenter.x > 0.0f,
            corner.vCenter.y > 0.0f);
        corners[i] = pCorner;
    }

    PhysicsNet::StaticInit(g_CollisionSpace);

    if (NetMesh::s_bAnimatedNetMeshEnabled)
    {
        char szTemp[256];
        unsigned long uPositiveNetMeshID;
        unsigned long uNegativeNetMeshID;
        pBaseName = BasicStadium::GetCurrentStadium()->GetBaseName();

        nlStrNCat<char>(szTemp, pBaseName, "/NetMesh", 0x100);
        uPositiveNetMeshID = nlStringLowerHash(szTemp);

        nlStrNCat<char>(szTemp, pBaseName, "/NetMesh01", 0x100);
        uNegativeNetMeshID = nlStringLowerHash(szTemp);

        PhysicsNet::spPhysNetPositiveX->mpNetMesh->Initialize(uPositiveNetMeshID);
        PhysicsNet::spPhysNetNegativeX->mpNetMesh->Initialize(uNegativeNetMeshID);
    }

    ConstructStaticPhysicsPrimitives(BasicStadium::GetCurrentStadium()->m_pPhysicsData);
    return true;
}

/**
 * Offset/Address/Size: 0x3F0 | 0x80132F00 | size: 0x338
 */
void PhysicsLoader::ConstructStaticPhysicsPrimitives(CharacterPhysicsData* pPhysicsData)
{
    unsigned int i = 0;
    ListEntry<PhysicsObject*>** pStaticTail = &g_StaticPhysicsPrimitives.m_Tail;
    ListEntry<PhysicsObject*>** pStaticHead = &g_StaticPhysicsPrimitives.m_Head;
    ListEntry<PhysicsObject*>** pNetTail = &g_NetPhysicsObjects.m_Tail;
    ListEntry<PhysicsObject*>** pNetHead = &g_NetPhysicsObjects.m_Head;

    while (i < pPhysicsData->physicsElementCount)
    {
        PhysicsObject* obj = NULL;
        CharacterPhysicsElement* physElement = &pPhysicsData->pPhysicsElements[i];

        switch (physElement->uPrimitiveType)
        {
        case 1:
            obj = new (nlMalloc(sizeof(PhysicsSphere), 8, false)) PhysicsSphere(NULL, NULL, physElement->fRadius);
            obj->SetWorldMatrix(physElement->matLocalToParent);
            {
                void* p = nlMalloc(8, 8, false);
                ListEntry<PhysicsObject*>* entry = (ListEntry<PhysicsObject*>*)p;
                if (p != NULL)
                {
                    ((ListEntry<PhysicsObject*>*)p)->next = NULL;
                    ((ListEntry<PhysicsObject*>*)p)->entry = obj;
                }
                nlListAddEnd(pStaticHead, pStaticTail, entry);
            }
            break;

        case 2:
            obj = new (nlMalloc(sizeof(PhysicsCapsule), 8, false)) PhysicsCapsule(NULL, NULL, physElement->fRadius, physElement->fHeight);
            obj->SetWorldMatrix(physElement->matLocalToParent);
            {
                void* p = nlMalloc(8, 8, false);
                ListEntry<PhysicsObject*>* entry = (ListEntry<PhysicsObject*>*)p;
                if (p != NULL)
                {
                    ((ListEntry<PhysicsObject*>*)p)->next = NULL;
                    ((ListEntry<PhysicsObject*>*)p)->entry = obj;
                }
                nlListAddEnd(pStaticHead, pStaticTail, entry);
            }
            break;

        case 4:
        {
            nlVector3 centre;
            nlVector3 v1;
            nlVector3 v2;
            bool normalPointsAwayFromField = false;

            nlVec3Set(centre, physElement->matLocalToParent.m41, physElement->matLocalToParent.m42, physElement->matLocalToParent.m43);
            nlVec3Set(v1, physElement->matLocalToParent.m11, physElement->matLocalToParent.m12, physElement->matLocalToParent.m13);
            nlVec3Set(v2, physElement->matLocalToParent.m21, physElement->matLocalToParent.m22, physElement->matLocalToParent.m23);

            float m31 = physElement->matLocalToParent.m31;

            if ((centre.x > 0.0f && m31 > 0.01f) || (centre.x < 0.0f && m31 < -0.01f))
            {
                normalPointsAwayFromField = true;
            }

            nlVec3Scale(v1, 0.5f * physElement->fWidth);
            nlVec3Scale(v2, 0.5f * physElement->fLength);

            obj = new (nlMalloc(sizeof(PhysicsFinitePlane), 8, false)) PhysicsFinitePlane(NULL, centre, v1, v2, true, normalPointsAwayFromField ? sfStaticFinitePlaneThinDepth : sfStaticFinitePlaneThickDepth);
            {
                void* p = nlMalloc(8, 8, false);
                ListEntry<PhysicsObject*>* entry = (ListEntry<PhysicsObject*>*)p;
                if (p != NULL)
                {
                    ((ListEntry<PhysicsObject*>*)p)->next = NULL;
                    ((ListEntry<PhysicsObject*>*)p)->entry = obj;
                }
                nlListAddEnd(pStaticHead, pStaticTail, entry);
            }
            break;
        }
        }

        obj->SetCategory(0x800);
        obj->SetCollide(0x20);
        {
            void* p = nlMalloc(8, 8, false);
            ListEntry<PhysicsObject*>* entry = (ListEntry<PhysicsObject*>*)p;
            if (p != NULL)
            {
                ((ListEntry<PhysicsObject*>*)p)->next = NULL;
                ((ListEntry<PhysicsObject*>*)p)->entry = obj;
            }
            nlListAddEnd(pNetHead, pNetTail, entry);
        }

        sbNonMovingAABBsInitialized = false;
        i++;
    }
}

/**
 * Offset/Address/Size: 0x14C | 0x80132C5C | size: 0x244
 */
void PhysicsLoader::DestroyPhysics()
{
    PhysicsNet::StaticDestroy();

    for (int i = 0; i < 4; i++)
    {
        delete corners[i];
    }

    ListEntry<PhysicsObject*>* entry = g_StaticPhysicsPrimitives.m_Head;
    while (entry != NULL)
    {
        delete entry->entry;
        entry = entry->next;
    }

    g_StaticPhysicsPrimitives.Clear();
    g_NetPhysicsObjects.Clear();
    s_PhysicsMeshes.Clear();
    g_TerrainMesh = NULL;

    delete g_CollisionSpace;
    g_CollisionSpace = NULL;

    delete g_PhysicsWorld;
    g_PhysicsWorld = NULL;

    dClearCachedData();
}

/**
 * Offset/Address/Size: 0x0 | 0x80132B10 | size: 0x14C
 */
void PhysicsUpdate(PhysicsWorld* pWorld, float fDeltaT)
{
    if (!sbDisableCollisionDetection)
    {
        pWorld->Collide();
    }

    if (pWorld == g_PhysicsWorld && !sbDisableCollisionDetection)
    {
        int ballFlags = dGeomGetGFlags(g_pBall->m_pPhysicsBall->m_geomID);

        if (!sbNonMovingAABBsInitialized)
        {
            ListEntry<PhysicsObject*>* entry = g_NetPhysicsObjects.m_Head;
            while (entry != NULL)
            {
                dGeomComputeAABB(entry->entry->m_geomID);
                entry = entry->next;
            }
            sbNonMovingAABBsInitialized = true;
        }
        else
        {
            ListEntry<PhysicsObject*>* entry = g_NetPhysicsObjects.m_Head;
            while (entry != NULL)
            {
                dGeomMarkAABBAsValid(entry->entry->m_geomID);
                entry = entry->next;
            }
        }

        PhysicsAIBall* pPhysicsBall = g_pBall->m_pPhysicsBall;
        if (pPhysicsBall->mbIsInsideNet)
        {
            dGeomComputeAABB(pPhysicsBall->m_geomID);
            g_PhysicsWorld->DoCollisions(pPhysicsBall, g_NetPhysicsObjects);
        }

        dGeomSetGFlags(g_pBall->m_pPhysicsBall->m_geomID, ballFlags);
    }

    pWorld->PreUpdate();
    pWorld->Update(fDeltaT, true);
    pWorld->PostUpdate();
}

//  */
// void PhysicsRoundedCorner::GetObjectType() const
// {
// }

// void nlWalkList<ListEntry<LoadablePhysicsMesh*>, ListContainerBase<LoadablePhysicsMesh*,
// NewAdapter<ListEntry<LoadablePhysicsMesh*>>>>(ListEntry<LoadablePhysicsMesh*>*, ListContainerBase<LoadablePhysicsMesh*,
// NewAdapter<ListEntry<LoadablePhysicsMesh*>>>*, void (ListContainerBase<LoadablePhysicsMesh*,
// NewAdapter<ListEntry<LoadablePhysicsMesh*>>>::*)(ListEntry<LoadablePhysicsMesh*>*))
// {
// }

// void nlWalkList<ListEntry<PhysicsObject*>, ListContainerBase<PhysicsObject*,
// NewAdapter<ListEntry<PhysicsObject*>>>>(ListEntry<PhysicsObject*>*, ListContainerBase<PhysicsObject*,
// NewAdapter<ListEntry<PhysicsObject*>>>*, void (ListContainerBase<PhysicsObject*,
// NewAdapter<ListEntry<PhysicsObject*>>>::*)(ListEntry<PhysicsObject*>*))
// {
// }

// void nlListAddEnd<ListEntry<PhysicsObject*>>(ListEntry<PhysicsObject*>**, ListEntry<PhysicsObject*>**, ListEntry<PhysicsObject*>*)
// {
// }

// void ListContainerBase<PhysicsObject*, NewAdapter<ListEntry<PhysicsObject*>>>::DeleteEntry(ListEntry<PhysicsObject*>*)
// {
// }

// void ListContainerBase<LoadablePhysicsMesh*, NewAdapter<ListEntry<LoadablePhysicsMesh*>>>::DeleteEntry(ListEntry<LoadablePhysicsMesh*>*)
// {
// }

// void nlListContainer<PhysicsObject*>::~nlListContainer()
// {
// }
