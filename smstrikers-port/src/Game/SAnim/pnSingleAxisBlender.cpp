#include "Game/SAnim/pnSingleAxisBlender.h"

#include "NL/nlMemory.h"

SlotPool<cPN_SingleAxisBlender> cPN_SingleAxisBlender::m_SingleAxisBlenderSlotPool(0x10, 0x10);

/**
 * Offset/Address/Size: 0x5DC | 0x801EF320 | size: 0xD4
 */
cPN_SingleAxisBlender::cPN_SingleAxisBlender(int numChildren, void (*callback)(uintptr_t, cPN_SingleAxisBlender*), uintptr_t callbackParam, float weightSeek)
    : cPoseNode(numChildren)
{
    m_fSmoothedWeight = 0.0f;
    m_fWeightCallback = callback;
    m_nCallbackParam1 = callbackParam;
    m_fDesiredWeight = 0.0f;
    m_fWeightSeek = weightSeek;

    for (int i = 0; i < m_numChildren; ++i)
    {
        SetChild(i, nullptr);
    }

    if (m_fWeightCallback != nullptr)
    {
        m_fWeightCallback(m_nCallbackParam1, this);
        m_fSmoothedWeight = m_fDesiredWeight;
    }
}

/**
 * Offset/Address/Size: 0x518 | 0x801EF25C | size: 0xC4
 */
cPoseNode* cPN_SingleAxisBlender::Update(float dt)
{
    for (int i = 0; i < m_numChildren; ++i)
    {
        SetChild(i, GetChild(i)->Update(dt));
    }

    if (m_fWeightCallback != nullptr)
    {
        m_fWeightCallback(m_nCallbackParam1, this);
        m_fSmoothedWeight = m_fSmoothedWeight + m_fWeightSeek * (m_fDesiredWeight - m_fSmoothedWeight);
    }

    return this;
}

/**
 * Offset/Address/Size: 0x3CC | 0x801EF110 | size: 0x14C
 */
void cPN_SingleAxisBlender::Evaluate(int nodeIndex, float weight, cPoseAccumulator* accum) const
{
    if (m_fSmoothedWeight >= 1.0f)
    {
        GetChild(m_numChildren - 1)->Evaluate(nodeIndex, weight, accum);
        return;
    }

    float scaledWeight = m_fSmoothedWeight * (float)(m_numChildren - 1);

    int childIndex = (int)scaledWeight;

    float frac = scaledWeight - (float)childIndex;

    GetChild(childIndex)->Evaluate(nodeIndex, weight * (1.0f - frac), accum);
    GetChild(childIndex + 1)->Evaluate(nodeIndex, weight * frac, accum);
}

/**
 * Offset/Address/Size: 0x298 | 0x801EEFDC | size: 0x134
 */
void cPN_SingleAxisBlender::Evaluate(float weight, cPoseAccumulator* accum) const
{
    if (m_fSmoothedWeight >= 1.0f)
    {
        GetChild(m_numChildren - 1)->Evaluate(weight, accum);
        return;
    }

    float scaledWeight = m_fSmoothedWeight * (float)(m_numChildren - 1);

    int childIndex = (int)scaledWeight;

    float frac = scaledWeight - (float)childIndex;

    GetChild(childIndex)->Evaluate(weight * (1.0f - frac), accum);
    GetChild(childIndex + 1)->Evaluate(weight * frac, accum);
}

/**
 * Offset/Address/Size: 0x14C | 0x801EEE90 | size: 0x14C
 */
void cPN_SingleAxisBlender::BlendRootTrans(nlVector3* outBase, float weight, float* scratch)
{
    if (m_fSmoothedWeight >= 1.0f)
    {
        GetChild(m_numChildren - 1)->BlendRootTrans(outBase, weight, scratch);
        return;
    }

    float scaledWeight = m_fSmoothedWeight * (float)(m_numChildren - 1);

    int childIndex = (int)scaledWeight;

    float frac = scaledWeight - (float)childIndex;

    GetChild(childIndex)->BlendRootTrans(outBase, weight * (1.0f - frac), scratch);
    GetChild(childIndex + 1)->BlendRootTrans(outBase, weight * frac, scratch);
}

/**
 * Offset/Address/Size: 0x0 | 0x801EED44 | size: 0x14C
 */
void cPN_SingleAxisBlender::BlendRootRot(unsigned short* outRot, float weight, float* scratch)
{
    if (m_fSmoothedWeight >= 1.0f)
    {
        GetChild(m_numChildren - 1)->BlendRootRot(outRot, weight, scratch);
        return;
    }

    float scaledWeight = m_fSmoothedWeight * (float)(m_numChildren - 1);

    int childIndex = (int)scaledWeight;

    float frac = scaledWeight - (float)childIndex;

    GetChild(childIndex)->BlendRootRot(outRot, weight * (1.0f - frac), scratch);
    GetChild(childIndex + 1)->BlendRootRot(outRot, weight * frac, scratch);
}
