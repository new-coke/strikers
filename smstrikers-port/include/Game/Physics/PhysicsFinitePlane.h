#ifndef _PHYSICSFINITEPLANE_H_
#define _PHYSICSFINITEPLANE_H_

#include "NL/nlMath.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsObject.h"

class PhysicsFinitePlane : public PhysicsObject
{
public:
    PhysicsFinitePlane(CollisionSpace* collision_space, nlVector3& centre, nlVector3& v1, nlVector3& v2, bool isOneSided, float errorCorrectionDepth);
    virtual int GetObjectType() const { return 0x7; };

    /* 0x2C */ float xMin;
    /* 0x30 */ float xMax;
    /* 0x34 */ float yMin;
    /* 0x38 */ float yMax;
    /* 0x3C */ float mErrorCorrectionDepth;
    /* 0x40 */ struct dxJoint* m_CharMoveJoint;
}; // total size: 0x44

#endif // _PHYSICSFINITEPLANE_H_
