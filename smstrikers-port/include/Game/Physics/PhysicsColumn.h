#ifndef _PHYSICSCOLUMN_H_
#define _PHYSICSCOLUMN_H_

#include "Game/Physics/PhysicsObject.h"
#include "Game/Physics/CollisionSpace.h"
#include "ode/NLGAdditions.h"
#include "ode/common.h"

class PhysicsColumn : public PhysicsObject
{
public:
    PhysicsColumn(CollisionSpace* collisionSpace, PhysicsWorld* world, float radius);
    virtual ~PhysicsColumn();
    virtual int GetObjectType() const
    {
        return 0x04;
    };
    void SetDensity(float density);
    void SetRadius(float radius);
    void GetRadius(float* radius);

    /* 0x2c */ dJointID m_jointID;
};

#endif // _PHYSICSCOLUMN_H_
