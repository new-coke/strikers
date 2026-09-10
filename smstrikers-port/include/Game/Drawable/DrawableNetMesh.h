#ifndef _DRAWABLENETMESH_H_
#define _DRAWABLENETMESH_H_

#include "Game/Drawable/DrawableObj.h"
#include "Game/Render/NetMesh.h"

class LoadFrame;
class SaveFrame;

class DrawableNetMesh
{
public:
    DrawableNetMesh(bool isPositiveXNet);
    ~DrawableNetMesh();

    void Replay(SaveFrame& frame);
    void Replay(LoadFrame& frame);
    void Blend(float blendFactor, const DrawableNetMesh& lhs, const DrawableNetMesh& rhs);
    void Grab(NetMesh& netMesh);
    void Render() const;
    void RenderInvisiblePlanes() const;

    /* 0x00 */ nlVector3* mpPosition;    // offset 0x0, size 0x4
    /* 0x04 */ int miNetIndex;           // offset 0x4, size 0x4
    /* 0x08 */ mutable int mNumQuads;    // offset 0x8, size 0x4
    /* 0x0C */ mutable intptr_t mNumVertices; // offset 0xC, size 0x4
    /* 0x10 */ int mNumTriIndices;       // offset 0x10, size 0x4
    /* 0x14 */ int mJolt;                // offset 0x14, size 0x4
    /* 0x18 */ int m_unk18;              // offset 0x18, size 0x4
    /* 0x1C */ float mJoltCache;         // offset 0x1C, size 0x4
    /* 0x20 */ NetMesh* mpNetMesh;       // offset 0x20, size 0x4
    /* 0x24 */ bool mbInitialized;

    static shortVector2* spTexcoord[2];
    static unsigned long* spColour[2];
    static unsigned short* spTriIndices[2];
    static bool sbStaticInitialized[2];
    static int sNumVertices[2];
}; // total size: 0x28

#endif // _DRAWABLENETMESH_H_
