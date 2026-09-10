#ifndef _PHYSICS_H_
#define _PHYSICS_H_

#include "Game/Loader.h"
#include "Game/Physics/PhysicsWorld.h"
#include "Game/Physics/PhysicsObject.h"

#include "Game/Physics/PhysicsRoundedCorner.h"

class LoadingManager;
class CharacterPhysicsData;
class LoadablePhysicsMesh;

void PhysicsUpdate(PhysicsWorld* pWorld, float fDeltaT);
void ODEFree(void* ptr, unsigned long size);
void* ODERealloc(void* oldPtr, unsigned long oldSize, unsigned long newSize);
void* ODEAlloc(unsigned long size);

// void nlListAddEnd<ListEntry<PhysicsObject*>>(ListEntry<PhysicsObject*>**, ListEntry<PhysicsObject*>**, ListEntry<PhysicsObject*>*);

class PhysicsLoader : public Loader
{
public:
    void DestroyPhysics();
    void ConstructStaticPhysicsPrimitives(CharacterPhysicsData* pPhysicsData);
    bool StartLoad(LoadingManager* manager);
};

// class PhysicsRoundedCorner : public PhysicsObject
// {
// public:
//     ~PhysicsRoundedCorner();
//     void GetObjectType() const;
// };

// class nlWalkList<ListEntry<LoadablePhysicsMesh*>, ListContainerBase<LoadablePhysicsMesh*, NewAdapter<ListEntry<LoadablePhysicsMesh*>>>>(ListEntry<LoadablePhysicsMesh*>*, ListContainerBase<LoadablePhysicsMesh*, NewAdapter<ListEntry<LoadablePhysicsMesh*>>>*, void (ListContainerBase<LoadablePhysicsMesh*, NewAdapter<ListEntry<LoadablePhysicsMesh*>>>
// {
// public:
//     void *)(ListEntry<LoadablePhysicsMesh*>*));
// };

// class nlWalkList<ListEntry<PhysicsObject*>, ListContainerBase<PhysicsObject*, NewAdapter<ListEntry<PhysicsObject*>>>>(ListEntry<PhysicsObject*>*, ListContainerBase<PhysicsObject*, NewAdapter<ListEntry<PhysicsObject*>>>*, void (ListContainerBase<PhysicsObject*, NewAdapter<ListEntry<PhysicsObject*>>>
// {
// public:
//     void *)(ListEntry<PhysicsObject*>*));
// };

// class ListContainerBase<PhysicsObject*, NewAdapter<ListEntry<PhysicsObject*>>>
// {
// public:
//     void DeleteEntry(ListEntry<PhysicsObject*>*);
// };

// class ListContainerBase<LoadablePhysicsMesh*, NewAdapter<ListEntry<LoadablePhysicsMesh*>>>
// {
// public:
//     void DeleteEntry(ListEntry<LoadablePhysicsMesh*>*);
// };

// class nlListContainer<PhysicsObject*>
// {
// public:
//     void ~nlListContainer();
// };

// class cInventory<LoadablePhysicsMesh>
// {
// public:
//     void ~cInventory();
// };

#endif // _PHYSICS_H_
