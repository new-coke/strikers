#ifndef _PHYSICSCAPSULE_H_
#define _PHYSICSCAPSULE_H_

#include "Game/Physics/PhysicsObject.h"

#include "Game/Physics/CollisionSpace.h"

class PhysicsCapsule : public PhysicsObject
{
public:
    void SetDensity(float density);
    void GetDimensions(float* radius, float* length);
    void SetDimensions(float radius, float length);
    void SetRadius(float radius);
    void SetLength(float length);
    float GetFullHeight();
    PhysicsCapsule(CollisionSpace* collisionSpace, PhysicsWorld* world, float radius, float length);
    virtual int GetObjectType() const { return 0x2; };
};

#endif // _PHYSICSCAPSULE_H_
