#ifndef _PHYSICSNPC_H_
#define _PHYSICSNPC_H_

#include "Game/Physics/PhysicsSphere.h"
#include "Game/Physics/CollisionSpace.h"

class SkinAnimatedMovableNPC;

class PhysicsNPC : public PhysicsSphere
{
public:
    typedef void (*CallbackFn)(PhysicsObject*, PhysicsObject*, const nlVector3&);

    PhysicsNPC(float radius);
    // virtual ~PhysicsNPC() { };
    virtual int GetObjectType() const { return 0x1A; };
    virtual bool SetContactInfo(dContact* contact, PhysicsObject* other, bool first);
    virtual ContactType Contact(PhysicsObject* obj, dContact* info, int what);

    void SetCallbackFunction(CallbackFn cb);
    // void SetCallbackFunction(void (*)(PhysicsObject*, PhysicsObject*, const nlVector3&));

    /* 0x2C */ CallbackFn mpTriggerCallbackFunc;
    /* 0x30 */ SkinAnimatedMovableNPC* mpAINPC;
}; // total size: 0x34

#endif // _PHYSICSNPC_H_
