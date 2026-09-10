#include "Game/SAnim.h"
#include "port/morphwatch.h"
#include <stdlib.h>
extern "C" int port_region_owns(const void*);
#include "NL/nlWare.h"
#include "dolphin/os.h"
#include "port/endian.h"
extern "C" unsigned long port_bmd_swap_headers(void*, unsigned long);
#include "Game/PoseAccumulator.h"

#include "NL/nlMemory.h"
#include "NL/nlList.h"

#pragma inline_depth(8)
#pragma inline_max_size(0x10000)
#pragma inline_max_total_size(0x10000)

static inline void* nlGetChunkDataSAnim(nlChunk* chunk)
{
    u32 alignField = chunk->m_ID & 0x7F000000;
    u32 isAligned = ((-alignField) | alignField) >> 31;
    if (isAligned != 0)
    {
        u32 alignment = 1u << (alignField >> 24);
        uintptr_t addr = (uintptr_t)chunk + alignment;
        u32 mask = alignment - 1;
        addr = (addr + 7) & ~(uintptr_t)mask;
        return (void*)addr;
    }
    return (void*)((u8*)chunk + 8);
}

#define nlGetNextChunk(chunk) ((nlChunk*)((u8*)(chunk) + (chunk)->m_Size + 8))

#pragma inline_depth(8)
#pragma inline_max_size(0x10000)
#pragma inline_max_total_size(0x10000)
/**
 * Offset/Address/Size: 0xD40 | 0x801E9F54 | size: 0x68C
 */
cSAnim* cSAnim::Initialize(nlChunk* pChunk)
{
    // PORT: big-endian, the extent included; a tree bmd_endian.c refuses would be walked out of bounds.
    if (port_bmd_swap_headers(pChunk, port_be32(&pChunk->m_Size) + 8) == 0)
    {
        OSReport("Error: SANIM chunk is not a well-formed chunk tree\n");
        return NULL;
    }

    nlChunk* chunkA = (nlChunk*)((u8*)pChunk + 8);
    nlChunk* end = nlGetNextChunk(pChunk);
    nlChunk* chunkB;

    cSAnim* pRetval;
    {
        // PORT: cSAnim is 0x48 on disc and sizeof(cSAnim) here, ten pointer fields, all wider.
        const u8* disc = (const u8*)nlGetChunkDataSAnim(chunkA);
        pRetval = (cSAnim*)nlMalloc(sizeof(cSAnim), 8, false);
        memset(pRetval, 0, sizeof(cSAnim));
        // PORT: cIdentifier keeps the name hash at disc 0x04, and every inventory lookup is by that value.
        pRetval->m_uHashID = port_be32(disc + 0x04);
        pRetval->m_nNumKeys = port_be32(disc + 0x08);
        pRetval->m_nNumNodes = port_be32(disc + 0x0C);
        pRetval->m_nNumMorphChannels = port_be32(disc + 0x10);
        pRetval->m_nNumRootKeys = port_be32(disc + 0x24);
        pRetval->m_fLinearSpeed = port_bef32(disc + 0x40);
        pRetval->m_nHierarchySignature = port_be32(disc + 0x44);
    }
    pRetval->m_pCallbackList = NULL;

    chunkB = nlGetNextChunk(chunkA);
    pRetval->m_szName = (const char*)nlGetChunkDataSAnim(chunkB);

    chunkA = nlGetNextChunk(chunkB);
    {
        // PORT: allocated, not overlaid, on-disc entries are 4 bytes each.
        const u32 nNodes = pRetval->m_nNumNodes;
        pRetval->m_pRotKeys = nlMalloc(nNodes * sizeof(void*), 8, false);
        memset(pRetval->m_pRotKeys, 0, nNodes * sizeof(void*));

        chunkB = nlGetNextChunk(chunkA);
        pRetval->m_pTransKeys = (PackedTrans**)nlMalloc(nNodes * sizeof(void*), 8, false);
        memset(pRetval->m_pTransKeys, 0, nNodes * sizeof(void*));

        chunkA = nlGetNextChunk(chunkB);
        pRetval->m_pScaleKeys = (PackedScale**)nlMalloc(nNodes * sizeof(void*), 8, false);
        memset(pRetval->m_pScaleKeys, 0, nNodes * sizeof(void*));
    }

    chunkB = nlGetNextChunk(chunkA);
    pRetval->m_pRootRot = (unsigned short*)nlGetChunkDataSAnim(chunkB);
    if (pRetval->m_pRootRot != NULL)
    {
        unsigned short* pRot = (unsigned short*)pRetval->m_pRootRot;
        for (u32 k = 0; k < pRetval->m_nNumRootKeys; k++)
            pRot[k] = port_be16(&pRot[k]);
    }

    chunkA = nlGetNextChunk(chunkB);
    pRetval->m_pRootTrans = (nlVector3*)nlGetChunkDataSAnim(chunkA);
    if (pRetval->m_pRootTrans != NULL)
    {
        // nlVector3 is three floats per root key.
        port_be32_array(pRetval->m_pRootTrans, pRetval->m_nNumRootKeys * 3);
    }

    u32 nodeIndex = 0;
    u32 type;
    nlChunk* nodeChunk = nlGetNextChunk(chunkA);

    while (nodeChunk != end && ((type = nodeChunk->m_ID & 0x80FFFFFF) == 0x80017100 || type == 0x1001))
    {
        if (type == 0x80017100)
        {
            int nNodeIndex = nodeIndex;
            nlChunk* subChunk = (nlChunk*)((u8*)nodeChunk + 8);
            nlChunk* subEnd = nlGetNextChunk(nodeChunk);

            while (subChunk != subEnd)
            {
                u32 subType = subChunk->m_ID & 0x80FFFFFF;
                // PORT: the keys stay in the file buffer, same widths either side, but their contents are big-endian.
                u8* pKeyData = (u8*)nlGetChunkDataSAnim(subChunk);
                unsigned long uKeyBytes =
                    (unsigned long)((u8*)nlGetNextChunk(subChunk) - pKeyData);

                if (subType == 0x17101)
                {
                    // One u16 angle per key, or a quaternion as four s16; 16-bit either way.
                    port_be16_array(pKeyData, uKeyBytes / 2);
                    ((void**)pRetval->m_pRotKeys)[nNodeIndex] = (void*)pKeyData;
                }
                else if (subType == 0x17102)
                {
                    port_be32_array(pKeyData, uKeyBytes / 4);   // PackedTrans: three floats
                    pRetval->m_pTransKeys[nNodeIndex] = (PackedTrans*)pKeyData;
                }
                else if (subType == 0x17103)
                {
                    port_be16_array(pKeyData, uKeyBytes / 2);   // PackedScale: three s16
                    pRetval->m_pScaleKeys[nNodeIndex] = (PackedScale*)pKeyData;
                }

                subChunk = nlGetNextChunk(subChunk);
            }

            nodeIndex++;
        }

        nodeChunk = nlGetNextChunk(nodeChunk);
    }

    nlVector3* rootTrans = pRetval->m_pRootTrans;
    nlVector3 v3PosStart;
    nlVector3 v3PosEnd;

    if (rootTrans != NULL)
    {
        pRetval->GetRootTrans(0.0f, &v3PosStart);
        pRetval->GetRootTrans(1.0f, &v3PosEnd);

        float dist = nlSqrt(
            nlGetLengthSquared3D(
                v3PosEnd.x - v3PosStart.x,
                v3PosEnd.y - v3PosStart.y,
                v3PosEnd.z - v3PosStart.z),
            true);

        pRetval->m_fLinearSpeed = dist / ((float)pRetval->m_nNumKeys / 30.0f);
    }
    else
    {
        pRetval->m_fLinearSpeed = 0.0f;
    }

    pRetval->m_pNumMorphKeys = (const unsigned int*)nlGetChunkDataSAnim(nodeChunk);
    // PORT: plain u32 tables in the file buffer.
    port_be32_array((void*)pRetval->m_pNumMorphKeys, pRetval->m_nNumMorphChannels);

    nodeChunk = nlGetNextChunk(nodeChunk);
    pRetval->m_nMorphIds = (const unsigned int*)nlGetChunkDataSAnim(nodeChunk);
    port_be32_array((void*)pRetval->m_nMorphIds, pRetval->m_nNumMorphChannels);

    nodeChunk = nlGetNextChunk(nodeChunk);
    pRetval->m_pMorphKeys = (unsigned char*)nlGetChunkDataSAnim(nodeChunk);

    nodeChunk = nlGetNextChunk(nodeChunk);
    pRetval->m_pNodeProperties = (const unsigned int*)nlGetChunkDataSAnim(nodeChunk);
    port_be32_array((void*)pRetval->m_pNodeProperties, pRetval->m_nNumNodes);

    PortMorphWatchRegister(pRetval, &pRetval->m_pNumMorphKeys, "m_pNumMorphKeys");
    PortMorphWatchRegister(pRetval, &pRetval->m_pMorphKeys, "m_pMorphKeys");
    PortMorphWatchRegister(pRetval, &pRetval->m_nMorphIds, "m_nMorphIds");

    return pRetval;
}
#pragma inline_depth()

/**
 * Offset/Address/Size: 0x91C | 0x801E9B30 | size: 0x424
 */
void cSAnim::BlendRot(int nodeIndex, int remappedNodeIndex, float tNorm, float weight, cPoseAccumulator* acc, bool additive) const
{
    void* pRawKeys = ((void**)m_pRotKeys)[remappedNodeIndex];
    if (pRawKeys != NULL && (unsigned int)remappedNodeIndex < m_nNumNodes)
    {
        unsigned int props = m_pNodeProperties[remappedNodeIndex];

        if (props & 0x2)
        {
            if (props & 0x1)
            {
                acc->BlendRotAroundZ(nodeIndex, ((unsigned short*)pRawKeys)[0], weight);
                return;
            }

            nlQuaternion q;
            q.x = 0.000061035156f * ((signed short*)pRawKeys)[0];
            q.y = 0.000061035156f * ((signed short*)pRawKeys)[1];
            q.z = 0.000061035156f * ((signed short*)pRawKeys)[2];
            q.w = 0.000061035156f * ((signed short*)pRawKeys)[3];
            acc->BlendRot(nodeIndex, &q, weight, additive);
            return;
        }

        if (1.0f == tNorm)
        {
            int lastIndex = m_nNumKeys - 1;

            if (props & 0x1)
            {
                acc->BlendRotAroundZ(nodeIndex, ((unsigned short*)pRawKeys)[lastIndex], weight);
                return;
            }

            signed short* pLast = ((signed short*)pRawKeys) + (lastIndex * 4);
            nlQuaternion q;
            q.x = 0.000061035156f * pLast[0];
            q.y = 0.000061035156f * pLast[1];
            q.z = 0.000061035156f * pLast[2];
            q.w = 0.000061035156f * pLast[3];
            acc->BlendRot(nodeIndex, &q, weight, additive);
            return;
        }

        float fRealIndex = tNorm * (m_nNumKeys - 1);
        int nKeyIndex = (int)fRealIndex;
        float fFrac = fRealIndex - nKeyIndex;
        float fWeight2 = weight * fFrac;
        float fWeight1 = weight - fWeight2;

        if (props & 0x1)
        {
            unsigned short* pKeys = (unsigned short*)pRawKeys;
            acc->BlendRotAroundZ(nodeIndex, pKeys[nKeyIndex], fWeight1);
        }
        else
        {
            signed short* pKey = ((signed short*)pRawKeys) + (nKeyIndex * 4);
            nlQuaternion q1;
            q1.x = 0.000061035156f * pKey[0];
            q1.y = 0.000061035156f * pKey[1];
            q1.z = 0.000061035156f * pKey[2];
            q1.w = 0.000061035156f * pKey[3];
            acc->BlendRot(nodeIndex, &q1, fWeight1, additive);
        }

        if (m_pNodeProperties[remappedNodeIndex] & 0x1)
        {
            unsigned short* pKeys = (unsigned short*)(((void**)m_pRotKeys)[remappedNodeIndex]);
            unsigned short* pKey = &pKeys[nKeyIndex];
            acc->BlendRotAroundZ(nodeIndex, pKey[1], fWeight2);
            return;
        }

        signed short* pKey = ((signed short*)(((void**)m_pRotKeys)[remappedNodeIndex])) + ((nKeyIndex + 1) * 4);
        nlQuaternion q2;
        q2.x = 0.000061035156f * pKey[0];
        q2.y = 0.000061035156f * pKey[1];
        q2.z = 0.000061035156f * pKey[2];
        q2.w = 0.000061035156f * pKey[3];
        acc->BlendRot(nodeIndex, &q2, fWeight2, additive);
        return;
    }

    acc->BlendRotIdentity(nodeIndex, weight);
}

/**
 * Offset/Address/Size: 0x608 | 0x801E981C | size: 0x314
 */
void cSAnim::BlendScale(int nodeIndex, int remappedNodeIndex, float tNorm, float weight, cPoseAccumulator* acc, bool additive) const
{
    PackedScale* pKeys = m_pScaleKeys[remappedNodeIndex];
    if (pKeys != NULL && (unsigned int)remappedNodeIndex < m_nNumNodes)
    {
        if (m_pNodeProperties[remappedNodeIndex] & 0x8)
        {
            nlVector3 v;
            v.x = 0.000244140625f * pKeys[0].x;
            v.y = 0.000244140625f * pKeys[0].y;
            v.z = 0.000244140625f * pKeys[0].z;
            acc->BlendScale(nodeIndex, &v, weight, additive);
            return;
        }

        if (1.0f == tNorm)
        {
            PackedScale* pLastKey = &pKeys[m_nNumKeys - 1];
            nlVector3 v;
            v.x = 0.000244140625f * pLastKey->x;
            v.y = 0.000244140625f * pLastKey->y;
            v.z = 0.000244140625f * pLastKey->z;
            acc->BlendScale(nodeIndex, &v, weight, additive);
            return;
        }

        float fRealIndex = tNorm * (m_nNumKeys - 1);
        int nKeyIndex = (int)fRealIndex;
        float fFrac = fRealIndex - nKeyIndex;
        float fWeight2 = weight * fFrac;
        float fWeight1 = weight - fWeight2;

        PackedScale* pKey = &pKeys[nKeyIndex];
        nlVector3 v1;
        v1.x = 0.000244140625f * pKey->x;
        v1.y = 0.000244140625f * pKey->y;
        v1.z = 0.000244140625f * pKey->z;
        acc->BlendScale(nodeIndex, &v1, fWeight1, additive);

        PackedScale* pNextKey = &m_pScaleKeys[remappedNodeIndex][nKeyIndex + 1];
        nlVector3 v2;
        v2.x = 0.000244140625f * pNextKey->x;
        v2.y = 0.000244140625f * pNextKey->y;
        v2.z = 0.000244140625f * pNextKey->z;
        acc->BlendScale(nodeIndex, &v2, fWeight2, additive);
    }
    else
    {
        acc->BlendScaleIdentity(nodeIndex, weight);
    }
}

/**
 * Offset/Address/Size: 0x404 | 0x801E9618 | size: 0x204
 */
void cSAnim::BlendTrans(int nAccumulatorNode, int nSAnimNode, float fTime, float fWeight, cPoseAccumulator* pAccumulator, bool bMirror) const
{
    if (pAccumulator->m_BaseSHierarchy->PreserveBoneLength(nAccumulatorNode))
    {
        return;
    }

    PackedTrans* pKeys = m_pTransKeys[nSAnimNode];
    if (pKeys != NULL && (unsigned int)nSAnimNode < m_nNumNodes)
    {
        if (m_pNodeProperties[nSAnimNode] & 0x4)
        {
            nlVector3 v;
            v.x = pKeys[0].x;
            v.y = pKeys[0].y;
            v.z = pKeys[0].z;
            pAccumulator->BlendTrans(nAccumulatorNode, &v, fWeight, bMirror);
            return;
        }

        if (1.0f == fTime)
        {
            PackedTrans* pLastKey = &pKeys[m_nNumKeys - 1];
            nlVector3 v;
            v.x = pLastKey->x;
            v.y = pLastKey->y;
            v.z = pLastKey->z;
            pAccumulator->BlendTrans(nAccumulatorNode, &v, fWeight, bMirror);
            return;
        }

        int nKeyIndex = (int)(fTime * (float)(m_nNumKeys - 1));
        float fFrac = fTime * (float)(m_nNumKeys - 1) - (float)nKeyIndex;
        float fWeight2 = fWeight * fFrac;
        float fWeight1 = fWeight - fWeight2;

        PackedTrans* pKey = &pKeys[nKeyIndex];
        nlVector3 v1;
        v1.x = pKey->x;
        v1.y = pKey->y;
        v1.z = pKey->z;
        pAccumulator->BlendTrans(nAccumulatorNode, &v1, fWeight1, bMirror);

        PackedTrans* pNextKey = &m_pTransKeys[nSAnimNode][nKeyIndex + 1];
        nlVector3 v2;
        v2.x = pNextKey->x;
        v2.y = pNextKey->y;
        v2.z = pNextKey->z;
        pAccumulator->BlendTrans(nAccumulatorNode, &v2, fWeight2, bMirror);
    }
    else
    {
        pAccumulator->BlendTransIdentity(nAccumulatorNode, fWeight);
    }
}

/**
 * Offset/Address/Size: 0x3CC | 0x801E95E0 | size: 0x38
 */
void cSAnim::Destroy()
{
    nlDeleteList<cSAnimCallback>(&m_pCallbackList);
    m_pCallbackList = 0;
}

// The by-value inline return gives MWCC the target signed-conversion slot order.
static inline s16 SAnimRootDiffIdentity(s16 value)
{
    return value;
}

/**
 * Offset/Address/Size: 0x2EC | 0x801E9500 | size: 0xE0
 */
void cSAnim::GetRootRot(float fTime, unsigned short* pRootRot) const
{
    float fRealIndex;
    int nIndex;

    if (m_nNumRootKeys != 0)
    {
        if (fTime == 1.0f || m_nNumRootKeys == 1)
        {
            *pRootRot = m_pRootRot[m_nNumRootKeys - 1];
            return;
        }

        fRealIndex = fTime * (m_nNumRootKeys - 1);
        nIndex = (int)fRealIndex;
        unsigned short* pRoots = m_pRootRot;
        unsigned short val0 = pRoots[nIndex];
        s16 diff = (s16)(pRoots[nIndex + 1] - val0);
        *pRootRot = val0 + (int)((fRealIndex - (float)nIndex) * SAnimRootDiffIdentity(diff));
        return;
    }
    *pRootRot = 0;
}

/**
 * Offset/Address/Size: 0x1E0 | 0x801E93F4 | size: 0x10C
 */
void cSAnim::GetRootTrans(float t, nlVector3* out) const
{
    if (m_nNumRootKeys != 0)
    {
        if (t == 1.0f || m_nNumRootKeys == 1)
        {
            *out = m_pRootTrans[m_nNumRootKeys - 1];
            return;
        }

        float fRealIndex = t * (m_nNumRootKeys - 1);
        int nIndex = (int)fRealIndex;
        float fWeight = fRealIndex - nIndex;
        nlVec3WeightedSum(*out, 1.0f - fWeight, m_pRootTrans[nIndex], fWeight, m_pRootTrans[nIndex + 1]);

        return;
    }
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
}

/**
 * Offset/Address/Size: 0x160 | 0x801E9374 | size: 0x80
 */
void cSAnim::CreateCallback(float fTime, uintptr_t nParam1, void (*funcCallback)(uintptr_t))
{
    cSAnimCallback* pCallback;
    pCallback = (cSAnimCallback*)nlMalloc(sizeof(cSAnimCallback), 8, 0);

    if (pCallback != NULL)
    {
        pCallback->m_fTime = fTime;
        pCallback->m_nParam1 = nParam1;
        pCallback->m_funcCallback = funcCallback;
    }

    nlListAddStart<cSAnimCallback>(&m_pCallbackList, pCallback, NULL);
}

/**
 * Offset/Address/Size: 0x0 | 0x801E9214 | size: 0x160
 */
float cSAnim::GetMorphWeight(int channel, float fTime) const
{
    // PORT: Checked before any dereference: the read that faults is m_pNumMorphKeys[channel] itself.
    if (!port_region_owns(m_pNumMorphKeys) || !port_region_owns(m_pMorphKeys)
        || channel < 0 || (unsigned int)channel >= m_nNumMorphChannels)
    {
        static int nMorphLog = 0;
        if (nMorphLog++ < 8)
        {
            OSReport("[morph] BAD ch=%d nch=%u counts=%p(%d) keys=%p(%d) this=%p\n",
                     channel, (unsigned int)m_nNumMorphChannels,
                     (const void*)m_pNumMorphKeys, port_region_owns(m_pNumMorphKeys),
                     (const void*)m_pMorphKeys, port_region_owns(m_pMorphKeys),
                     (const void*)this);
        }
        return 0.0f;
    }

    const u8* keys = m_pMorphKeys;
    int numKeys = m_pNumMorphKeys[channel];
    int i = 0;

    for (i = 0; i < channel; i++)
    {
        keys += m_pNumMorphKeys[channel];
    }

    if (numKeys == 1 || fTime == 1.0f)
    {
        float weight = (float)keys[numKeys - 1] / 255.0f;
        return weight;
    }

    float fRealIndex = fTime * (float)(numKeys - 1);
    int nIndex = (int)fRealIndex;
    float fWeightB = fRealIndex - (float)nIndex;
    return (1.0f - fWeightB) * ((float)keys[nIndex] / 255.0f) + fWeightB * ((float)keys[nIndex + 1] / 255.0f);
}
