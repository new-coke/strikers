#ifndef _PHYSICSCHARACTERBASE_H_
#define _PHYSICSCHARACTERBASE_H_

#include "Game/Physics/PhysicsCompositeObject.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/Physics/PhysicsSphere.h"
#include "Game/Physics/PhysicsCapsule.h"

#include "NL/nlList.h"

class cPoseAccumulator;
class CharacterPhysicsData;

enum PhysicsBoneID
{
    PHYSBONE_SINGLE_CAPSULE = 0,
    PHYSBONE_FIELDER_R_ARM = 1,
    PHYSBONE_FIELDER_L_ARM = 2,
    PHYSBONE_FIELDER_R_LEG = 3,
    PHYSBONE_FIELDER_L_LEG = 4,
    PHYSBONE_FIELDER_HEAD = 5,
    PHYSBONE_GOALIE_R_SHOULDER = 6,
    PHYSBONE_GOALIE_R_BICEP = 7,
    PHYSBONE_GOALIE_R_FOREARM = 8,
    PHYSBONE_GOALIE_R_WRIST = 9,
    PHYSBONE_GOALIE_R_HAND = 10,
    PHYSBONE_GOALIE_R_THIGH = 11,
    PHYSBONE_GOALIE_R_THIGHLOWER = 12,
    PHYSBONE_GOALIE_R_CALFUPPER = 13,
    PHYSBONE_GOALIE_R_HEEL = 14,
    PHYSBONE_GOALIE_R_TOE = 15,
    PHYSBONE_GOALIE_L_SHOULDER = 16,
    PHYSBONE_GOALIE_L_BICEP = 17,
    PHYSBONE_GOALIE_L_FOREARM = 18,
    PHYSBONE_GOALIE_L_WRIST = 19,
    PHYSBONE_GOALIE_L_HAND = 20,
    PHYSBONE_GOALIE_L_THIGH = 21,
    PHYSBONE_GOALIE_L_THIGHLOWER = 22,
    PHYSBONE_GOALIE_L_CALFUPPER = 23,
    PHYSBONE_GOALIE_L_HEEL = 24,
    PHYSBONE_GOALIE_L_TOE = 25,
    PHYSBONE_GOALIE_HEAD = 26,
    PHYSBONE_GOALIE_STOMACH = 27,
    NUM_PHYSBONES = 28,
    PHYSBONE_UNKNOWN = 29,
    PHYSBONE_ID_INVALID = 29,
};

class PhysicsCharacterBase : public PhysicsCompositeObject
{
public:
    class PhysicsBoneVolume
    {
    public:
        PhysicsBoneVolume(PhysicsObject* object, unsigned int boneIndex, unsigned int transformHandle, nlMatrix4& transform,
            PhysicsBoneID id)
            : m_pObject(object)
            , m_BoneIndex(boneIndex)
            , m_Transform(transform)
            , m_TransformHandle(transformHandle)
            , m_ID(id)
        {
        }

        /* 0x00 */ PhysicsObject* m_pObject;
        /* 0x04 */ unsigned int m_BoneIndex;
        /* 0x08 */ nlMatrix4 m_Transform;
        /* 0x48 */ unsigned int m_TransformHandle;
        /* 0x4C */ nlVector3 m_PrevPosition;
        /* 0x58 */ PhysicsBoneID m_ID;
    }; // total size: 0x5C

    class PhysicsSphereBone : public PhysicsSphere
    {
    public:
        PhysicsSphereBone(CollisionSpace* collisionSpace, PhysicsWorld* physicsWorld, float radius)
            : PhysicsSphere(collisionSpace, physicsWorld, radius)
            , m_boneVolume(NULL)
        {
        }

        virtual int GetObjectType() const { return 0xD; };

        /* 0x2C */ PhysicsBoneVolume* m_boneVolume;
    }; // total size: 0x30

    class PhysicsCapsuleBone : public PhysicsCapsule
    {
    public:
        PhysicsCapsuleBone(CollisionSpace* collisionSpace, PhysicsWorld* physicsWorld, float radius, float height)
            : PhysicsCapsule(collisionSpace, physicsWorld, radius, height)
            , m_boneVolume(NULL)
        {
        }

        virtual int GetObjectType() const { return 0xE; };

        /* 0x2C */ PhysicsBoneVolume* m_boneVolume;
    }; // total size: 0x30

    PhysicsCharacterBase(CollisionSpace* collisionSpace, PhysicsWorld* physicsWorld, float centreOfMassHeight);
    virtual ~PhysicsCharacterBase();
    virtual int GetObjectType() const { return 0x08; };
    virtual void UpdatePose(cPoseAccumulator* pose, float heightOffset);
    virtual PhysicsBoneID ResolvePhysicsBoneIDFromName(const char*) = 0;

    int GetNumBoneVolumePoints(bool includeEndpoints) const;
    void GetBoneVolumePoints(nlVector3* points, bool includeEndpoints);
    void SetCharacterPosition(const nlVector3& pos);
    bool BaseSetContactInfo(dContact* contact, PhysicsObject* other, bool first);
    void BasePreCollide();
    void ContainObject(PhysicsObject* obj);
    void ReleaseObject();
    void SetFacingDirection(unsigned short angle);
    void AddBoneVolumes(PhysicsWorld* physicsWorld, CollisionSpace* collisionSpace, cPoseAccumulator* poseAccumulator, const CharacterPhysicsData* pPhysicsData, unsigned long categoryBitmask, unsigned long collisionBitmask);
    PhysicsBoneVolume* AddBoneVolume(PhysicsObject* object, unsigned int boneIndex, unsigned int transformHandle, nlMatrix4& transform, PhysicsBoneID id);
    void GetBonePositions(PhysicsBoneID boneID, nlVector3& v3CurPos, nlVector3& v3PrevPos);
    PhysicsBoneID GetBoneIDForSubObject(const PhysicsObject* obj) const;

    /* 0x38 */ unsigned char m_IsSupported;                       // offset 0x38, size 0x1
    /* 0x3C */ float m_CentreOfMassHeight;                        // offset 0x3C, size 0x4
    /* 0x40 */ dxJoint* m_CharMoveJoint;                          // offset 0x40, size 0x4
    /* 0x44 */ PhysicsTransform m_SubObject;                      // offset 0x44, size 0x30
    /* 0x74 */ nlListContainer<PhysicsBoneVolume*> m_BoneVolumes; // offset 0x74, size 0xC
}; // total size: 0x80

#endif // _PHYSICSCHARACTERBASE_H_
