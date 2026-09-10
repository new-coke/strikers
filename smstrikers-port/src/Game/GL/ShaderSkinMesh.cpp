#include "Game/GL/ShaderSkinMesh.h"

#include "Game/GL/GLSkinMesh.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glUserData.h"
#include "NL/nlDLRing.h"
#include "NL/nlMemory.h"
#include "string.h"
#include "stdlib.h"
extern "C" void OSReport(const char*, ...);
#include "types.h"

static nlVector3 sharedMorphBuffer[0x1000];

/**
 * Offset/Address/Size: 0xD28 | 0x801E136C | size: 0xC8
 */
ShaderSkinMesh::ShaderSkinMesh()
{
    numBaseVerts = 0;
    numMorphs = 0;
    morphNumDeltas = NULL;
    morphData = NULL;
    morphBuffer = NULL;
    boneMaps = NULL;
    morphIDs = NULL;

    morphWeights[0] = 0.0f;
    morphWeights[0] = 0.0f;
    morphWeights[0] = 0.0f;
    morphWeights[0] = 0.0f;
    morphWeights[0] = 0.0f;
    morphWeights[0] = 0.0f;
    morphWeights[0] = 0.0f;
    morphWeights[0] = 0.0f;

    numSoftwareVerts = 0;
    softwareVertices = NULL;
    tempNormals = NULL;
    tempMatrices = NULL;
    skinPairs = NULL;
    stitchArray = NULL;
    numPackets = 0;
}

/**
 * Offset/Address/Size: 0xBE4 | 0x801E1228 | size: 0xE4
 */
ShaderSkinMesh::~ShaderSkinMesh()
{
    delete[] morphNumDeltas;
    delete[] morphData;
    delete[] morphIDs;

    if (boneMaps != nullptr)
    {
        void (*deleteFn)(BoneMapList**) = nlDeleteRing<BoneMapList>;
        deleteFn(&boneMaps);
    }

    delete[] tempNormals;
    delete[] tempMatrices;

    if (skinPairs != nullptr)
    {
        nlDeleteRing<SkinPairList>(&skinPairs);
    }

    if (stitchArray != nullptr)
    {
        delete[] stitchArray;
    }
}

/**
 * Offset/Address/Size: 0xB74 | 0x801E11B8 | size: 0x70
 */
void ShaderSkinMesh::SetMorphIDs(const u32* ids)
{
    if (morphIDs != nullptr)
    {
        delete[] morphIDs;
    }
    morphIDs = (u32*)nlMalloc(numMorphs * sizeof(u32), 8, false);
    memcpy(morphIDs, ids, numMorphs * sizeof(u32));
}

/**
 * Offset/Address/Size: 0xB70 | 0x801E11B4 | size: 0x4
 */
void ShaderSkinMesh::ConnectToPose(cPoseAccumulator* pPoseAccumulator)
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0xB04 | 0x801E1148 | size: 0x6C
 */
void ShaderSkinMesh::SetBoneMatrix(unsigned long boneID, const nlMatrix4* matrix)
{
    SkinMatrix skinMatrix;

    skinMatrix.Set(*matrix);
    boneMatrices.Add(boneID, skinMatrix);
}

/**
 * Offset/Address/Size: 0xA94 | 0x801E10D8 | size: 0x70
 */
nlMatrix4& ShaderSkinMesh::GetPoseMatrix(unsigned long boneID)
{
    nlMatrix4* foundMatrix = (nlMatrix4*)poseMatrices.FindGet(boneID);
    return *foundMatrix;
}

/**
 * Offset/Address/Size: 0x8E8 | 0x801E0F2C | size: 0x1AC
 */
struct TreeStack
{
    AVLTreeEntry<unsigned long, SkinMatrix>** m_Stack;
    unsigned int m_Count;
};

/**
 * Offset/Address/Size: 0x8E8 | 0x801E0F2C | size: 0x1AC
 */
void ShaderSkinMesh::GetPoseMatrices(GLSkinMeshMatrix* pMatrices)
{
    TreeStack* stack = (TreeStack*)nlMalloc(sizeof(TreeStack), 8, false);
    if (stack != NULL)
    {
        unsigned int numElements = poseMatrices.m_NumElements;
        AVLTreeEntry<unsigned long, SkinMatrix>* node = poseMatrices.m_Root;

        stack->m_Stack = (AVLTreeEntry<unsigned long, SkinMatrix>**)nlMalloc((numElements + 1) * sizeof(AVLTreeEntry<unsigned long, SkinMatrix>*), 8, false);
        stack->m_Count = 0;

        if (node != NULL)
        {
            while (node->node.left != NULL)
            {
                stack->m_Stack[stack->m_Count] = node;
                stack->m_Count++;
                node = (AVLTreeEntry<unsigned long, SkinMatrix>*)node->node.left;
            }

            stack->m_Stack[stack->m_Count] = node;
            stack->m_Count++;
        }
    }

    while (stack->m_Count > 0)
    {
        pMatrices->boneID = stack->m_Stack[stack->m_Count - 1]->key;
        memcpy(&pMatrices->matrix, &stack->m_Stack[stack->m_Count - 1]->value, sizeof(pMatrices->matrix));
        pMatrices++;

        stack->m_Count--;

        AVLTreeEntry<unsigned long, SkinMatrix>* right = (AVLTreeEntry<unsigned long, SkinMatrix>*)stack->m_Stack[stack->m_Count]->node.right;
        if (right == NULL)
            continue;

        while (right->node.left != NULL)
        {
            stack->m_Stack[stack->m_Count] = right;
            stack->m_Count++;
            right = (AVLTreeEntry<unsigned long, SkinMatrix>*)right->node.left;
        }

        stack->m_Stack[stack->m_Count] = right;
        stack->m_Count++;
    }

    if (stack != NULL)
    {
        delete[] stack->m_Stack;
        delete stack;
    }
}

/**
 * Offset/Address/Size: 0x76C | 0x801E0DB0 | size: 0x17C
 */
void ShaderSkinMesh::SetPoseMatrices(int num, GLSkinMeshMatrix* pMatrices)
{
    int i = 0;
    for (; i < num; i++)
    {
        SkinMatrix* foundValue;
        unsigned long boneID = pMatrices[i].boneID;

        if (boneMatrices.FindGet(boneID, &foundValue))
        {
            SkinMatrix skinMatrix;
            skinMatrix.Set(pMatrices[i].matrix);

            foundValue = poseMatrices.Add(boneID, skinMatrix);
            if (foundValue != NULL)
            {
                *foundValue = skinMatrix;
            }
        }
    }
}

static inline void ClearUserData(ShaderSkinMesh* mesh)
{
    if (mesh->pModel != NULL)
    {
        glModelPacket* pPacket = mesh->pModel->packets;
        glModelPacket* pEndPacket = pPacket + mesh->pModel->numPackets;
        while (pPacket < pEndPacket)
        {
            pPacket->userData = 0;
            pPacket++;
        }
    }
}

static inline void CopySoftwareVerts(ShaderSkinMesh* mesh)
{
    for (int i = 0; i < mesh->numSoftwareVerts; i++)
    {
        mesh->morphBuffer[i].x = mesh->softwareVertices[i].position.x;
        mesh->morphBuffer[i].y = mesh->softwareVertices[i].position.y;
        mesh->morphBuffer[i].z = mesh->softwareVertices[i].position.z;
    }
}

void ShaderSkinMesh::CreateMorphBuffer()
{
    morphBuffer = sharedMorphBuffer;
    CopySoftwareVerts(this);

    MorphDelta* pCurrentMorph = morphData;
    int morphIndex = 0;

    while (morphIndex < numMorphs)
    {
        MorphDelta* pEndMorph = pCurrentMorph + morphNumDeltas[morphIndex];

        if (morphWeights[morphIndex] > 0.0f)
        {
            while (pCurrentMorph != pEndMorph)
            {
                float w = morphWeights[morphIndex];
                nlVector3* dst = &morphBuffer[pCurrentMorph->index];
                float rx = dst->x + w * pCurrentMorph->delta.x;
                float rz = dst->z + w * pCurrentMorph->delta.z;
                float ry = dst->y + w * pCurrentMorph->delta.y;
                dst->x = rx;
                dst->y = ry;
                dst->z = rz;
                pCurrentMorph++;
            }
        }
        else
        {
            pCurrentMorph = pEndMorph;
        }

        morphIndex++;
    }
}

/**
 * Offset/Address/Size: 0x58C | 0x801E0BD0 | size: 0x1E0
 */
void ShaderSkinMesh::PrepareToRender(unsigned long flags, const nlMatrix4* pMatrix)
{
    ClearUserData(this);

    if (numMorphs == 0)
    {
        morphBuffer = NULL;
    }
    else
    {
        CreateMorphBuffer();
    }

    AttachSkinData(flags, pMatrix);
}

/**
 * Offset/Address/Size: 0x500 | 0x801E0B44 | size: 0x8C
 */
void ShaderSkinMesh::AppendSkinPairList(int numPairs, const SkinPair* pairs)
{
    SkinPairList* node = (SkinPairList*)nlMalloc(sizeof(SkinPairList), 8, false);

    if (node != nullptr)
    {
        node->pairs = nullptr;
        node->m_next = nullptr;
    }

    node->num = numPairs;

    if (numPairs == 0)
    {
        node->pairs = nullptr;
    }
    else
    {
        node->pairs = (SkinPair*)pairs;
    }

    nlRingAddEnd<SkinPairList>(&skinPairs, node);
}

/**
 * Offset/Address/Size: 0x4F4 | 0x801E0B38 | size: 0xC
 */
void ShaderSkinMesh::SetSoftwareVertices(int num, const SkinVertex* skinVertices)
{
    numSoftwareVerts = num;
    softwareVertices = (SkinVertex*)skinVertices;
}

/**
 * Offset/Address/Size: 0x458 | 0x801E0A9C | size: 0x9C
 */
void ShaderSkinMesh::AppendStitchingInfo(int packetIndex, int _numPackets, int num, const unsigned char* pIndices)
{
    if (stitchArray == NULL)
    {
        numPackets = _numPackets;
        stitchArray = (unsigned char**)nlMalloc(numPackets * sizeof(unsigned char*), 8, false);
        memset(stitchArray, 0, numPackets * sizeof(unsigned char*));
    }

    if (num > 0)
    {
        stitchArray[packetIndex] = (unsigned char*)pIndices;
    }
}

/**
 * Offset/Address/Size: 0x398 | 0x801E09DC | size: 0xC0
 */
inline void UserDataBuilder::AddEntry(const unsigned long& boneID, unsigned long* registerIndex)
{
    unsigned long id = boneID;
    SkinMatrix* foundMatrix = (SkinMatrix*)m_PoseMatrices->FindGet(id);

    static const bool bProbe = getenv("STRIKERS_PROBE_SKIN") != NULL;
    if (bProbe)
    {
        OSReport("skin: boneID %lu registerIndex %lu -> slot %d\n",
                 (unsigned long)boneID, (unsigned long)*registerIndex,
                 (int)((*registerIndex) * 3 - 0x60) + 99);
    }
    m_Bone->reg = (*registerIndex) * 3 - 0x60;
    foundMatrix->Get4x3(m_Bone->mat);

    m_Bone++;
}

/**
 * Offset/Address/Size: 0x2E4 | 0x801E0928 | size: 0xB4
 */
void* ShaderSkinMesh::MakeUserData(nlAVLTree<unsigned long, unsigned long, DefaultKeyCompare<unsigned long> >* boneMap)
{
    unsigned int count = boneMap->m_NumElements;
    static const bool bUDProbe = getenv("STRIKERS_PROBE_SKIN") != NULL;
    if (bUDProbe)
        OSReport("skin: MakeUserData count %u\n", count);
    unsigned long size = count * 0x34 + 4;

    void* userData = glUserAlloc(GLUD_Skin, size, false);
    if (userData == nullptr)
    {
        return nullptr;
    }

    void* data = glUserGetData(userData);
    *(unsigned int*)data = count;

    GLSkinUserData* skinDataArray = (GLSkinUserData*)((char*)data + 4);

    UserDataBuilder builder;
    builder.m_Bone = skinDataArray;
    builder.m_PoseMatrices = (nlAVLTree<unsigned long, SkinMatrix, DefaultKeyCompare<unsigned long> >*)&poseMatrices;

    boneMap->Walk(&builder, &UserDataBuilder::AddEntry);

    return userData;
}

static inline cSHierarchy* GetPoseHierarchy(cPoseAccumulator* pPoseAccumulator)
{
    return pPoseAccumulator->m_BaseSHierarchy;
}

static inline nlMatrix4* GetPoseNodeMatrixPtr(cPoseAccumulator* pPoseAccumulator, int i)
{
    return &pPoseAccumulator->GetNodeMatrix(i);
}

/**
 * Offset/Address/Size: 0x108 | 0x801E074C | size: 0x1DC
 */
void ShaderSkinMesh::Pose(cPoseAccumulator* pPoseAccumulator)
{
    SkinMatrix* foundMatrix;
    unsigned long nodeID;
    SkinMatrix result;
    SkinMatrix skinMat;

    for (int i = 0; i < pPoseAccumulator->GetNumNodes(); i++)
    {
        cSHierarchy* hierarchy = GetPoseHierarchy(pPoseAccumulator);
        nlMatrix4* nodeMatrix = GetPoseNodeMatrixPtr(pPoseAccumulator, i);
        nodeID = hierarchy->GetNodeID(i);

        u8 found = boneMatrices.FindGet(nodeID, &foundMatrix);
        if (found)
        {
            skinMat.Set(*nodeMatrix);

            nlMultMatrices(result, *foundMatrix, skinMat);

            foundMatrix = poseMatrices.Add(nodeID, result);
            if (foundMatrix != nullptr)
            {
                *foundMatrix = result;
            }
        }
    }

    for (int j = 0; j < numMorphs; j++)
    {
        morphWeights[j] = pPoseAccumulator->m_MorphWeights.mData[j];
    }
}

/**
 * Offset/Address/Size: 0x98 | 0x801E06DC | size: 0x70
 */
void ShaderSkinMesh::SetMorphNumDeltas(const u32* numDeltas)
{
    if (morphNumDeltas != nullptr)
    {
        delete[] morphNumDeltas;
    }
    morphNumDeltas = (u32*)nlMalloc(numMorphs * sizeof(u32), 8, false);
    memcpy(morphNumDeltas, numDeltas, numMorphs * sizeof(u32));
}

/**
 * Offset/Address/Size: 0x0 | 0x801E0644 | size: 0x98
 */
void ShaderSkinMesh::SetMorphDeltas(int numDeltas, const MorphDelta* p)
{
    if (morphData != nullptr)
    {
        delete[] morphData;
    }

    unsigned int largestBlock = nlVirtualLargestBlock();
    unsigned long size = numDeltas * sizeof(MorphDelta);
    MorphDelta* newData;

    if (largestBlock >= size + 0x100)
    {
        newData = (MorphDelta*)nlVirtualAlloc(size, false);
    }
    else
    {
        newData = (MorphDelta*)nlMalloc(size, 0x20, false);
    }

    morphData = newData;
    memcpy(morphData, p, size);
}
