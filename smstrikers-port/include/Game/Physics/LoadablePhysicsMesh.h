#ifndef _LOADABLEPHYSICSMESH_H_
#define _LOADABLEPHYSICSMESH_H_

#include "Game/Physics/PhysicsObject.h"

struct PhysTriMeshHeader
{
    // total size: 0x0
};

struct dxTriMeshData
{
    // total size: 0x0
};

class PhysicsMesh : public PhysicsObject
{
public:
    /* 0x2C */ PhysTriMeshHeader* m_Header;
    /* 0x30 */ nlVector3* m_Vertices;
    /* 0x34 */ unsigned long* m_Indices;
    /* 0x38 */ unsigned char* m_MaterialIDs;
    /* 0x3C */ struct dxTriMeshData* m_TriMeshData;
};

class LoadablePhysicsMesh : public PhysicsMesh
{
public:
    typedef char* MemType;

    void Destroy();
};

#endif // _LOADABLEPHYSICSMESH_H_
