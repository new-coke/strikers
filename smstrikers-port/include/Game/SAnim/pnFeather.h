#ifndef _PNFEATHER_H_
#define _PNFEATHER_H_

#include "Game/SHierarchy.h"
#include "Game/PoseAccumulator.h"
#include "NL/nlSlotPool.h"

enum eFeatherBlendMode
{
    eFEATHER_BLEND_IN = 0,
    eFEATHER_BLEND_OUT = 1,
};

class cPN_Feather : public cPoseNode
{
public:
    cPN_Feather() { }
    cPN_Feather(cSHierarchy* hierarchy, void (*callback)(uintptr_t, cPN_Feather*), uintptr_t callbackParam);
    /* 0x08 */ virtual ~cPN_Feather();
    static void* operator new(size_t)
    {
        cPN_Feather* result = NULL;
        m_FeatherSlotPool.Allocate(result);
        return result;
    }
    /* 0x14 */ virtual void Evaluate(int nodeIndex, float weight, cPoseAccumulator* accum) const;
    /* 0x10 */ virtual void Evaluate(float weight, cPoseAccumulator* accum) const;
    /* 0x18 */ virtual cPoseNode* Update(float dt);
    /* 0x1C */ virtual int GetType() { return 0x1; };
    /* 0x20 */ virtual void BlendRootTrans(nlVector3* outBase, float weight, float* scratch);
    /* 0x24 */ virtual void BlendRootRot(unsigned short* outRot, float weight, float* scratch);

    template <typename T>
    void Replay(T& frame)
    {
        Replayable<0>(frame, (cPoseNode&)*this);
        if (ReplayFrameTraits<T>::IsLoadFrame)
        {
            m_fBlendTime = 0.0f;
            m_pFeatherWeights = NULL;
        }
    }

    void ClearNodeWeights();
    void SetNodeWeight(int nodeIndex, float weight, float decayFactor);
    void SetChildFeatherWeight(int nodeIndex, float weight);
    void SetNodeWeight(int nodeIndex, float weight);
    void BeginBlendIn(float duration);
    void BeginBlendOut(float duration);

    // PORT: defined out of line in the .cpp; `inline` promised a body every TU could emit and no TU has.
    void operator delete(void* ptr);

    /* 0x14 */ float* m_pFeatherWeights;
    /* 0x18 */ float m_fBlendTime;
    /* 0x1C */ void (*m_fWeightTableCallback)(uintptr_t, class cPN_Feather*);
    /* 0x20 */ uintptr_t m_nCallbackParam1;   // PORT: holds a `this`
    /* 0x24 */ float m_fBlendDuration;
    /* 0x28 */ cSHierarchy* m_pBaseHierarchy;
    /* 0x2C */ eFeatherBlendMode m_eFeatherBlendMode;

    static SlotPool<cPN_Feather> m_FeatherSlotPool;
}; // total size: 0x30

inline cPN_Feather* AllocateFeather()
{
    cPN_Feather* feather = nullptr;

    cPN_Feather::m_FeatherSlotPool.Allocate(feather);

    return feather;
}

#endif // _PNFEATHER_H_
