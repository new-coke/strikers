#ifndef _NETMESH_H_
#define _NETMESH_H_

#include "NL/nlMath.h"
#include "Game/Sys/eventman.h"

class PhysicsSphere;

// Forward declare the opaque pointer type for NetMesh - specific type is nlAVLTreeSlotPool template
struct nlAVLTreeSlotPoolOpaque;

class cDistanceConstraint
{
public:
    /* 0x0 */ int nParticleA;
    /* 0x4 */ int nParticleB;
    /* 0x8 */ float fDistance;
}; // total size: 0xC

class cPositionConstraint
{
public:
    /* 0x0 */ int nParticle;
    /* 0x4 */ nlVector3 v3Position;
}; // total size: 0x10

struct shortVector2
{
    union
    {
        signed short e[2];
        struct
        {
            s16 x;
            s16 y;
        };
    };
}; // total size: 0x4

class NetMesh
{
public:
    static bool s_bAlwaysActive;
    static bool s_bAnimatedNetMeshEnabled;
    static float s_fInactivityThreshold;
    static float s_fIsBallMovingThreshold;
    static float s_fNetGravityMagnitude;
    static float s_fReboundForceCoefficient;
    static float s_fVelocityDampingCoefficient;
    static float s_fNetStretchLimit;
    static bool s_bUseStretchLimit;
    static float s_fDampening;
    static float s_fLooseness;
    static float s_fBallRadiusExaggerationFactor;
    static float s_fBallRadiusExaggerationFactor2;
    static int s_NumConstraintIterations;

    static uintptr_t sNetTextureHandle;   // PORT: a resolved PlatTexture*
    static bool sbDontUseLowestNetTextureLOD;
    static NetMesh* spPositiveXNetMesh;
    static NetMesh* spNegativeXNetMesh;

    NetMesh(bool positiveEnd);
    ~NetMesh();

    void Allocate(int numParticles, int numDistanceConstraints, int numPositionConstraints);
    int SetPositionConstraint(int particleIndex, const nlVector3& v3Position);
    void SetDistanceConstraint(int nParticleA, int nParticleB, float fDistance);
    void UpdateUntilRelaxed();
    void Update(float dt, const nlVector3& ballPosition, const nlVector3& ballPrevPosition, bool bExaggerateBallSize, PhysicsSphere* sphere);
    void JoltNet(float zDisplacement);
    void SatisfyConstraints(const nlVector3& ballPosition, bool bExaggerateBallSize);
    void AddForcesToBall(const nlVector3& position, PhysicsSphere* sphere);
    void Initialize(unsigned long netMeshDrawableObjectID);
    void SetTriStripIndices(int numIndices, const unsigned short* indices);
    static void SetDontUseLowestNetTextureLOD(bool value);
    void SetTexture(uintptr_t texture);

    /* 0x00 */ int m_numAffectedParticles;
    /* 0x04 */ int m_iClosestParticle;
    /* 0x08 */ bool mbInitialized;
    /* 0x09 */ bool mbFirstUpdate;
    /* 0x0C */ nlVector3* m_v3Position;
    /* 0x10 */ nlVector3* m_v3PrevPosition;
    /* 0x14 */ nlVector3* m_v3Accel;
    /* 0x18 */ bool* m_bIsParticleFixed;
    /* 0x1C */ u16* m_TriStripIndices;
    /* 0x20 */ int m_NumTriStripIndices;
    /* 0x24 */ shortVector2* m_v2TextureCoords;
    /* 0x28 */ nlVector3* m_v3Normal;
    /* 0x2C */ nlVector3 m_v3TempConstraint;
    /* 0x38 */ unsigned long mNetMeshDrawableObjectID;
    /* 0x3C */ bool mbPositiveEnd;
    /* 0x40 */ int m_NumParticles;
    /* 0x44 */ float m_fBallPenetrationDepth;
    /* 0x48 */ unsigned char m_bPenetratingFixedParticle;
    /* 0x4C */ nlVector3 m_v3BallPenetrationNormal;
    /* 0x58 */ float mfMinX;
    /* 0x5C */ float mfMaxX;
    /* 0x60 */ float mfMinY;
    /* 0x64 */ float mfMaxY;
    /* 0x68 */ int m_NumPositionConstraints;
    /* 0x6C */ int m_NumDistanceConstraints;
    /* 0x70 */ bool mbIsActive;
    /* 0x71 */ bool mbBallIsInsideNet;
    /* 0x74 */ float mfMotion;
    /* 0x78 */ float mJolt;
    /* 0x7C */ nlAVLTreeSlotPoolOpaque* mEdgeList;
    /* 0x80 */ nlAVLTreeSlotPoolOpaque* mVertexList;
    /* 0x84 */ cDistanceConstraint* m_aDistanceConstraints;
    /* 0x88 */ cPositionConstraint* m_aPositionConstraints;
}; // total size: 0x8C

class BallNetmeshEventData : public EventData
{
public:
    BallNetmeshEventData() { }
    ~BallNetmeshEventData() { }

    virtual u32 GetID()
    {
        return 0x118;
    };

    /* 0x4 */ NetMesh* netMesh;
    /* 0x8 */ nlVector3 v3CollisionVelocity;
};

#endif // _NETMESH_H_
