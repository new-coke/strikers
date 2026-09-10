#ifndef _SHADERSKINMESH_H_
#define _SHADERSKINMESH_H_

#include "NL/nlAVLTree.h"
#include "NL/nlMath.h"

#include "Game/Drawable/DrawableObj.h"
#include "Game/GL/GLSkinMesh.h"
#include "Game/PoseAccumulator.h"

class BoneMapList
{
public:
    /* 0x00 */ BoneMapList* m_next;
    /* 0x04 */ nlAVLTree<unsigned long, unsigned long, DefaultKeyCompare<unsigned long> > boneMap;

}; // total size: 0x18

struct GLSkinUserData
{

    /* 0x00 */ int reg;
    /* 0x04 */ float mat[12]; // size 0x30
}; // total size: 0x34

class UserDataBuilder
{
public:
    void AddEntry(const unsigned long& boneID, unsigned long* registerIndex);

    /* 0x00 */ GLSkinUserData* m_Bone;
    /* 0x04 */ nlAVLTree<unsigned long, SkinMatrix, DefaultKeyCompare<unsigned long> >* m_PoseMatrices;
}; // total size: 0x8

class cPoseAccumulator;

struct GLSkinMeshMatrix
{
    unsigned long boneID; // offset 0x0
    nlMatrix4 matrix;     // offset 0x4
}; // total size: 0x44

class GLSkinMesh
{
public:
    GLSkinMesh()
        : pModel(NULL)
    {
    }
    virtual ~GLSkinMesh() { };
    virtual void ConnectToPose(cPoseAccumulator* pPoseAccumulator) = 0;
    virtual void Pose(cPoseAccumulator*) = 0;
    virtual void PrepareToRender(unsigned long, const nlMatrix4*) = 0;
    virtual nlMatrix4& GetPoseMatrix(unsigned long) = 0;
    virtual void GetPoseMatrices(GLSkinMeshMatrix*) = 0;
    virtual void SetPoseMatrices(int, GLSkinMeshMatrix*) = 0;

    /* 0x04 */ glModel* pModel;
}; // total size: 0x8

class ShaderSkinMesh : public GLSkinMesh
{
public:
    ShaderSkinMesh();
    virtual ~ShaderSkinMesh();
    virtual void ConnectToPose(cPoseAccumulator* pPoseAccumulator);
    virtual void Pose(cPoseAccumulator* pPoseAccumulator);
    virtual void PrepareToRender(unsigned long flags, const nlMatrix4* pMatrix);
    virtual nlMatrix4& GetPoseMatrix(unsigned long boneID);
    virtual void GetPoseMatrices(GLSkinMeshMatrix* pMatrices);
    virtual void SetPoseMatrices(int num, GLSkinMeshMatrix* pMatrices);

    void CreateMorphBuffer();
    void SetMorphIDs(const u32* ids);
    void SetBoneMatrix(unsigned long boneID, const nlMatrix4* matrix);
    void AppendSkinPairList(int numPairs, const SkinPair* pairs);
    void SetSoftwareVertices(int num, const SkinVertex* skinVertices);
    void AppendStitchingInfo(int packetIndex, int _numPackets, int num, const unsigned char* pIndices);
    void* MakeUserData(nlAVLTree<unsigned long, unsigned long, DefaultKeyCompare<unsigned long> >* boneMap);
    void SetMorphNumDeltas(const u32* numDeltas);
    void SetMorphDeltas(int numDeltas, const MorphDelta* p);

    void StitchModel();
    void AttachSkinData(unsigned long program, const nlMatrix4* pReflect);

    /* 0x08 */ nlAVLTree<unsigned long, SkinMatrix, DefaultKeyCompare<unsigned long> > boneMatrices; // offset 0x8, size 0x14
    /* 0x1C */ nlAVLTree<unsigned long, SkinMatrix, DefaultKeyCompare<unsigned long> > poseMatrices; // offset 0x1C, size 0x14
    /* 0x30 */ BoneMapList* boneMaps;
    /* 0x34 */ int numBaseVerts;
    /* 0x38 */ int numSoftwareVerts;
    /* 0x3C */ SkinVertex* softwareVertices;
    /* 0x40 */ nlVector3* tempNormals;
    /* 0x44 */ nlMatrix4* tempMatrices;
    /* 0x48 */ SkinPairList* skinPairs;
    /* 0x4C */ unsigned char** stitchArray;
    /* 0x50 */ int numPackets;
    /* 0x54 */ int numMorphs;
    /* 0x58 */ u32* morphIDs;
    /* 0x5C */ u32* morphNumDeltas;
    /* 0x60 */ MorphDelta* morphData;
    /* 0x64 */ nlVector3* morphBuffer;
    /* 0x68 */ float morphWeights[8];
}; // total size: 0x88

#endif // _SHADERSKINMESH_H_
