#ifndef _PHYSICSSHELL_H_
#define _PHYSICSSHELL_H_

#include "Game/Physics/PhysicsSphere.h"

class PowerupBase;

class PhysicsShell : public PhysicsSphere
{
public:
    PhysicsShell(float radius);
    virtual int GetObjectType() const { return 0x13; };
    virtual bool SetContactInfo(dContact* contact, PhysicsObject* other, bool first);
    virtual void PostUpdate();
    virtual ContactType Contact(PhysicsObject*obj, dContact*info, int numContacts);

    /* 0x2C */ void (*m_pTriggerCallbackFunc)(class PhysicsObject*, class PhysicsObject*, class nlVector3&, void*);
    /* 0x30 */ void* m_pCallbackParam;
    /* 0x34 */ PowerupBase* m_pPowerupObject;
    /* 0x38 */ bool mbIsInNet;
    /* 0x39 */ bool m_bIsSupportedByGround;
};

#endif // _PHYSICSSHELL_H_
