#include "Game/SAnim/pnFeather.h"

#include "math.h"
#include "NL/nlMemory.h"
#include "NL/nlSlotPool.h"

static f32 CANT_COLLIDE = *(f32*)__float_max;

SlotPool<cPN_Feather> cPN_Feather::m_FeatherSlotPool(0x10, 0x10);

void cPN_Feather::operator delete(void* ptr)
{
    m_FeatherSlotPool.Free((cPN_Feather*)ptr);
}

/**
 * Offset/Address/Size: 0x890 | 0x801EFDC4 | size: 0xEC
 */
cPN_Feather::cPN_Feather(cSHierarchy* hierarchy, void (*callback)(uintptr_t, cPN_Feather*), uintptr_t callbackParam)
    : cPoseNode(2)
{
    m_fBlendTime = 0.0f;
    m_fWeightTableCallback = callback;
    m_nCallbackParam1 = callbackParam;
    m_fBlendDuration = 0.0f;
    m_eFeatherBlendMode = eFEATHER_BLEND_OUT;
    m_pBaseHierarchy = hierarchy;

    m_pFeatherWeights = (float*)nlMalloc(hierarchy->m_nNumNodes * sizeof(float), 8, 0);

    ClearNodeWeights();

    SetChild(0, nullptr);
    SetChild(1, nullptr);
}

/**
 * Offset/Address/Size: 0x818 | 0x801EFD4C | size: 0x78
 */
cPN_Feather::~cPN_Feather()
{
    delete[] m_pFeatherWeights;
}

/**
 * Offset/Address/Size: 0x7E4 | 0x801EFD18 | size: 0x34
 */
void cPN_Feather::ClearNodeWeights()
{
    for (int i = 0; i < m_pBaseHierarchy->m_nNumNodes; ++i)
    {
        m_pFeatherWeights[i] = 0.0f;
    }
}

/**
 * Offset/Address/Size: 0x73C | 0x801EFC70 | size: 0xA8
 */
void cPN_Feather::SetNodeWeight(int nodeIndex, float weight, float decayFactor)
{
    SetChildFeatherWeight(nodeIndex, weight);
    while ((nodeIndex != -1) && (weight > 0.001f))
    {
        m_pFeatherWeights[nodeIndex] = weight;
        weight *= decayFactor;
        nodeIndex = m_pBaseHierarchy->GetParent(nodeIndex);
    }
}

/**
 * Offset/Address/Size: 0x4B8 | 0x801EF9EC | size: 0x284
 */
void cPN_Feather::SetChildFeatherWeight(int nodeIndex, float weight)
{
    int childNodeIndex;
    for (int i = 0; i < m_pBaseHierarchy->GetNumChildren(nodeIndex); i++)
    {
        childNodeIndex = m_pBaseHierarchy->GetChild(nodeIndex, i);
        m_pFeatherWeights[childNodeIndex] = weight;
        SetChildFeatherWeight(childNodeIndex, weight);
    }
}

/**
 * Offset/Address/Size: 0x4A8 | 0x801EF9DC | size: 0x10
 */
void cPN_Feather::SetNodeWeight(int nodeIndex, float weight)
{
    m_pFeatherWeights[nodeIndex] = weight;
}

/**
 * Offset/Address/Size: 0x490 | 0x801EF9C4 | size: 0x18
 */
void cPN_Feather::BeginBlendIn(float duration)
{
    m_eFeatherBlendMode = eFEATHER_BLEND_IN;
    m_fBlendTime = 0.0f;
    m_fBlendDuration = duration;
}

/**
 * Offset/Address/Size: 0x478 | 0x801EF9AC | size: 0x18
 */
void cPN_Feather::BeginBlendOut(float duration)
{
    m_eFeatherBlendMode = eFEATHER_BLEND_OUT;
    m_fBlendTime = 1.0f;
    m_fBlendDuration = duration;
}

/**
 * Offset/Address/Size: 0x2C8 | 0x801EF7FC | size: 0x1B0
 */
cPoseNode* cPN_Feather::Update(float dt)
{
    if (GetChild(0))
        SetChild(0, GetChild(0)->Update(dt));

    if (GetChild(1))
        SetChild(1, GetChild(1)->Update(dt));

    if (m_fWeightTableCallback)
    {
        m_fWeightTableCallback(m_nCallbackParam1, this);
    }

    if (GetChild(1))
    {
        switch (m_eFeatherBlendMode)
        {
        case eFEATHER_BLEND_IN:
        {
            m_fBlendTime += dt / m_fBlendDuration;
            if (m_fBlendTime > 1.0f)
            {
                m_fBlendTime = 1.0f;
            }
            break;
        }
        case eFEATHER_BLEND_OUT:
        {
            m_fBlendTime -= dt / m_fBlendDuration;
            if ((m_fBlendTime <= 0.0f) && (GetChild(1) != NULL))
            {
                delete GetChild(1);
                SetChild(1, nullptr);
            }
            break;
        }
        }
    }

    return this;
}

/**
 * Offset/Address/Size: 0x19C | 0x801EF6D0 | size: 0x12C
 */
void cPN_Feather::Evaluate(float weight, cPoseAccumulator* accum) const
{
    if ((m_pFeatherWeights != NULL) && (GetChild(0) != NULL) && (GetChild(1) != NULL))
    {
        for (int i = 0; i < accum->GetNumNodes(); i++)
        {
            Evaluate(i, weight, accum);
        }
        return;
    }

    if (GetChild(0) != NULL)
    {
        GetChild(0)->Evaluate(weight, accum);
        return;
    }

    if (GetChild(1) != NULL)
    {
        GetChild(1)->Evaluate(weight, accum);
    }
}

/**
 * Offset/Address/Size: 0x8 | 0x801EF53C | size: 0x194
 */
void cPN_Feather::Evaluate(int nodeIndex, float weight, cPoseAccumulator* accum) const
{
    f32 blendTime;
    f32 featherWeight;

    if ((GetChild(0) != NULL) && (GetChild(1) != NULL))
    {
        blendTime = m_fBlendTime;
        featherWeight = m_pFeatherWeights[nodeIndex];
        if (blendTime < 1.0f)
        {
            featherWeight *= blendTime * (blendTime * ((-2.0f * blendTime) + 3.0f));
        }
        GetChild(0)->Evaluate(nodeIndex, weight * (1.0f - featherWeight), accum);
        GetChild(1)->Evaluate(nodeIndex, weight * featherWeight, accum);
        return;
    }

    if (GetChild(0) != NULL)
    {
        GetChild(0)->Evaluate(nodeIndex, weight, accum);
        return;
    }

    if (GetChild(1) != NULL)
    {
        GetChild(1)->Evaluate(nodeIndex, weight, accum);
    }
}

/**
 * Offset/Address/Size: 0x4 | 0x801EF538 | size: 0x4
 */
void cPN_Feather::BlendRootTrans(nlVector3* outBase, float weight, float* scratch)
{
}

/**
 * Offset/Address/Size: 0x0 | 0x801EF534 | size: 0x4
 */
void cPN_Feather::BlendRootRot(unsigned short* outRot, float weight, float* scratch)
{
}
