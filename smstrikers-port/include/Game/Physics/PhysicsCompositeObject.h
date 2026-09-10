#ifndef _PHYSICSCOMPOSITEOBJECT_H_
#define _PHYSICSCOMPOSITEOBJECT_H_

#include "NL/nlMath.h"
#include "Game/Physics/PhysicsObject.h"
#include "Game/Physics/PhysicsTransform.h"
#include "NL/nlDLListContainer.h"

class PhysicsCompositeObject : public PhysicsObject
{
public:
    PhysicsCompositeObject(PhysicsWorld* physicsWorld);
    virtual ~PhysicsCompositeObject();
    virtual int GetObjectType() const { return 0x9; };
    void AdjustTransform(int i, nlMatrix4& m);
    int AddObject(PhysicsObject* object);
    PhysicsTransform* GetComponent(unsigned int i);

    /* 0x2C */ nlDLListContainer<PhysicsTransform*> m_Components;
    /* 0x34 */ int numComponents;
}; // total size: 0x38

#endif // _PHYSICSCOMPOSITEOBJECT_H_
