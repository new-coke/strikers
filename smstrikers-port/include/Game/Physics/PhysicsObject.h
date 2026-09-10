#ifndef _PHYSICSOBJECT_H_
#define _PHYSICSOBJECT_H_

#include "NL/nlMath.h"

#include "ode/objects.h"
#include "ode/collision.h"

#include "Game/Physics/PhysicsWorld.h"

enum ContactType
{
    NO_CONTACT = 0,
    ONE_WAY_CONTACT_THIS = 1,
    ONE_WAY_CONTACT_OTHER = 2,
    TWO_WAY_CONTACT = 3,
};

class PhysicsObject
{
public:
    static float DefaultGravity;

    enum CoordinateType
    {
        WORLD_COORDINATES = 0,
        RELATIVE_TO_PARENT = 1,
    };

    PhysicsObject(PhysicsWorld* world);
    virtual ~PhysicsObject();

    void CloneObject(const PhysicsObject& obj);
    /* 0x0C */ virtual int GetObjectType() const = 0;
    /* 0x10 */ virtual bool SetContactInfo(dContact* contact, PhysicsObject* otherObject, bool first);
    /* 0x14 */ virtual void PreUpdate();
    /* 0x18 */ virtual void PostUpdate();
    /* 0x1c */ virtual void PreCollide() { }
    /* 0x20 */ virtual ContactType Contact(PhysicsObject* obj1, dContact* contact, int param);
    /* 0x24 */ virtual ContactType Contact(PhysicsObject* obj1, dContact* contact, int param, PhysicsObject* obj2);
    void MakeMovable(dxBody* bodyID);
    void MakeStatic();
    float GetMass() const;
    void SetMass(float mass);
    bool IsConnected();
    void Reconnect(dSpaceID space);
    dSpaceID Disconnect();
    bool IsMotionEnabled();
    void EnableMotion();
    void DisableMotion();
    bool AreCollisionsEnabled();
    void EnableCollisions();
    void DisableCollisions();
    void GetWorldMatrix(nlMatrix4* dest);
    void SetWorldMatrix(const nlMatrix4& in);
    void SetDefaultContactInfo(dContact* contact);
    void SetAxisAndAngle(float x, float y, float z, float angle);
    void ZeroForceAccumulators();
    void AddForceAtCentreOfMass(const nlVector3& force);
    void AddForceAtPoint(const nlVector3& force, const nlVector3& point);
    void AddAcceleration(const nlVector3& accel);
    void GetAngularVelocity(nlVector3* vel) const;
    void SetAngularVelocity(const nlVector3& velocity);
    nlVector3& GetLinearVelocity();
    void GetLinearVelocity(nlVector3* vel) const;
    void SetLinearVelocity(const nlVector3& velocity);
    void GetRotation(nlMatrix4* m_out) const;
    void SetRotation(const nlMatrix4& m4_in);
    void SetRotation(const nlMatrix3& m3_in);
    nlVector3& GetPosition();
    void GetPosition(nlVector3* position) const;
    void SetPosition(const nlVector3& pos, CoordinateType type);
    void CheckForNaN();
    void ApplyGravity();
    void SetCategory(unsigned int categoryBits);
    void SetCollide(unsigned int collideBits);
    void SetDefaultCollideBits();

    inline bool IsObjectType(int type) const { return GetObjectType() == type; }

    /* 0x04 */ dBodyID m_bodyID;
    /* 0x08 */ dGeomID m_geomID;
    /* 0x0c */ PhysicsObject* m_parentObject;
    /* 0x10 */ float m_gravity;
    /* 0x14 */ nlVector3 m_position;
    /* 0x20 */ nlVector3 m_linearVelocity;
}; // total size: 0x2C

#endif // _PHYSICSOBJECT_H_
