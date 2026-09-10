#ifndef _PNBLENDER_H_
#define _PNBLENDER_H_

#include "NL/nlSlotPool.h"
#include "Game/PoseAccumulator.h"

class cPN_Blender : public cPoseNode
{
public:
    cPN_Blender() { }
    cPN_Blender(cPoseNode* child0, cPoseNode* child1, float blendDuration);
    /* 0x08 */ virtual ~cPN_Blender() { };
    static void* operator new(size_t)
    {
        cPN_Blender* result = NULL;
        m_BlenderSlotPool.Allocate(result);
        return result;
    }
    /* 0x10 */ virtual void Evaluate(float weight, cPoseAccumulator* accum) const;
    /* 0x14 */ virtual void Evaluate(int nodeIndex, float weight, cPoseAccumulator* accum) const;
    /* 0x18 */ virtual cPoseNode* Update(float dt);
    /* 0x1C */ virtual int GetType() { return 0x0; };
    /* 0x20 */ virtual void BlendRootTrans(nlVector3* outBase, float weight, float* scratch);
    /* 0x24 */ virtual void BlendRootRot(unsigned short* outRot, float weight, float* scratch);

    template <typename T>
    void Replay(T& frame)
    {
        Replayable<0>(frame, (cPoseNode&)*this);
        Replayable<0>(frame, FloatCompressor<0, 1, 7>(m_fBlendTime));
    }

    // PORT: defined out of line in the .cpp; `inline` promised a body every TU could emit and no TU has.
    void operator delete(void* ptr);

    /* 0x14 */ float m_fBlendTime;
    /* 0x18 */ float m_fBlendDuration;

    static SlotPool<cPN_Blender> m_BlenderSlotPool;
}; // total size: 0x1C

inline cPN_Blender* AllocateBlender()
{
    cPN_Blender* blender = nullptr;

    cPN_Blender::m_BlenderSlotPool.Allocate(blender);

    return blender;
}

inline cPN_Blender* CreateAndAssignBlender(const cPoseNode* child0, const cPoseNode* child1, float blendDuration)
{
    cPN_Blender* blender = nullptr;

    cPN_Blender::m_BlenderSlotPool.Allocate(blender);
    return ::new ((u8*)blender) cPN_Blender((cPoseNode*)child0, (cPoseNode*)child1, blendDuration);
}

#endif // _PNBLENDER_H_
