#ifndef _PNSINGLEAXISBLENDER_H_
#define _PNSINGLEAXISBLENDER_H_

#include "Game/PoseAccumulator.h"
#include "NL/nlMath.h"
#include "NL/nlSlotPool.h"

class cPN_SingleAxisBlender : public cPoseNode
{
public:
    cPN_SingleAxisBlender() { }
    cPN_SingleAxisBlender(int numChildren, void (*callback)(uintptr_t, cPN_SingleAxisBlender*), uintptr_t callbackParam, float weightSeek);
    /* 0x08 */ virtual ~cPN_SingleAxisBlender() { };
    static void* operator new(size_t)
    {
        cPN_SingleAxisBlender* result = NULL;
        m_SingleAxisBlenderSlotPool.Allocate(result);
        return result;
    }
    static void operator delete(void* ptr)
    {
        m_SingleAxisBlenderSlotPool.Free((cPN_SingleAxisBlender*)ptr);
    }
    /* 0x14 */ virtual void Evaluate(int nodeIndex, float weight, cPoseAccumulator* accum) const;
    /* 0x10 */ virtual void Evaluate(float weight, cPoseAccumulator* accum) const;
    /* 0x18 */ virtual cPoseNode* Update(float dt);
    /* 0x1C */ virtual int GetType() { return 0x3; };
    /* 0x20 */ virtual void BlendRootTrans(nlVector3* outBase, float weight, float* scratch);
    /* 0x24 */ virtual void BlendRootRot(unsigned short* outRot, float weight, float* scratch);

    template <typename T>
    void Replay(T& frame)
    {
        Replayable<0>(frame, (cPoseNode&)*this);
        Replayable<0>(frame, FloatCompressor<0, 1, 7>(m_fSmoothedWeight));
    }

    /* 0x14 */ float m_fSmoothedWeight;
    /* 0x18 */ void (*m_fWeightCallback)(uintptr_t, class cPN_SingleAxisBlender*);
    /* 0x1C */ uintptr_t m_nCallbackParam1;   // PORT: holds a `this`
    /* 0x20 */ float m_fDesiredWeight;
    /* 0x24 */ float m_fWeightSeek;

    static SlotPool<cPN_SingleAxisBlender> m_SingleAxisBlenderSlotPool;
}; // total size: 0x28

inline cPN_SingleAxisBlender* AllocateSingleAxisBlender()
{
    cPN_SingleAxisBlender* pSAB = NULL;

    cPN_SingleAxisBlender::m_SingleAxisBlenderSlotPool.Allocate(pSAB);

    return pSAB;
}

#endif // _PNSINGLEAXISBLENDER_H_
