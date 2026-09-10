#include "Game/SAnim/pnBlender.h"

#include "math.h"

static f32 CANT_COLLIDE = *(f32*)__float_max;

SlotPool<cPN_Blender> cPN_Blender::m_BlenderSlotPool(0x10, 0x10);

void cPN_Blender::operator delete(void* ptr)
{
    m_BlenderSlotPool.Free((cPN_Blender*)ptr);
}

/**
 * Offset/Address/Size: 0x440 | 0x801EEB60 | size: 0x98
 */
cPN_Blender::cPN_Blender(cPoseNode* child0, cPoseNode* child1, float blendDuration)
    : cPoseNode(2)
{
    SetChild(0, child0);
    SetChild(1, child1);
    m_fBlendTime = 0.0f;
    m_fBlendDuration = blendDuration;
}

/**
 * Offset/Address/Size: 0x32C | 0x801EEA4C | size: 0x114
 */
cPoseNode* cPN_Blender::Update(float dt)
{
    SetChild(0, GetChild(0)->Update(dt));
    SetChild(1, GetChild(1)->Update(dt));

    m_fBlendTime += dt / m_fBlendDuration;

    if (m_fBlendTime > 1.0f)
    {
        cPoseNode* oldChild1 = GetChild(1);
        SetChild(1, nullptr);
        delete this;
        return oldChild1;
    }

    return this;
}

/**
 * Offset/Address/Size: 0x270 | 0x801EE990 | size: 0xBC
 */
void cPN_Blender::Evaluate(float weight, cPoseAccumulator* accum) const
{
    f32 blendFactor;
    f32 blendTime;
    blendTime = m_fBlendTime;
    blendFactor = blendTime * (blendTime * ((-2.0f * blendTime) + 3.0f));

    GetChild(0)->Evaluate(weight * (1.0f - blendFactor), accum);
    GetChild(1)->Evaluate(weight * blendFactor, accum);
}

/**
 * Offset/Address/Size: 0x1A0 | 0x801EE8C0 | size: 0xD0
 */
void cPN_Blender::Evaluate(int nodeIndex, float weight, cPoseAccumulator* accum) const
{
    f32 blendFactor;
    f32 blendTime;
    blendTime = m_fBlendTime;
    blendFactor = blendTime * (blendTime * ((-2.0f * blendTime) + 3.0f));

    GetChild(0)->Evaluate(nodeIndex, weight * (1.0f - blendFactor), accum);
    GetChild(1)->Evaluate(nodeIndex, weight * blendFactor, accum);
}

/**
 * Offset/Address/Size: 0xD0 | 0x801EE7F0 | size: 0xD0
 */
void cPN_Blender::BlendRootTrans(nlVector3* outBase, float weight, float* scratch)
{
    f32 blendFactor;
    f32 blendTime;
    blendTime = m_fBlendTime;
    blendFactor = blendTime * (blendTime * ((-2.0f * blendTime) + 3.0f));

    GetChild(0)->BlendRootTrans(outBase, weight * (1.0f - blendFactor), scratch);
    GetChild(1)->BlendRootTrans(outBase, weight * blendFactor, scratch);
}

/**
 * Offset/Address/Size: 0x0 | 0x801EE720 | size: 0xD0
 */
void cPN_Blender::BlendRootRot(unsigned short* outRot, float weight, float* scratch)
{
    f32 blendFactor;
    f32 blendTime;
    blendTime = m_fBlendTime;
    blendFactor = blendTime * (blendTime * ((-2.0f * blendTime) + 3.0f));

    GetChild(0)->BlendRootRot(outRot, weight * (1.0f - blendFactor), scratch);
    GetChild(1)->BlendRootRot(outRot, weight * blendFactor, scratch);
}
