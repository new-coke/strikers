#ifndef _PNSANIMCONTROLLER_H_
#define _PNSANIMCONTROLLER_H_

#include "Game/SAnim.h"
#include "NL/nlMath.h"
#include "types.h"
#include <stdint.h>

#include "Game/SHierarchy.h"
#include "Game/PoseAccumulator.h"
#include "NL/nlSlotPool.h"

class AnimRetarget;

class cPN_SAnimController : public cPoseNode
{
public:
    cPN_SAnimController()
        : cPoseNode(0)
        , m_pSAnim(NULL)
        , m_fTime(0.0f)
    {
    }
    cPN_SAnimController(cSAnim* pSAnim, const AnimRetarget* pAnimRetarget, ePlayMode playMode, void (*funcPlaybackSpeedCallback)(uintptr_t, cPN_SAnimController*), uintptr_t nPlaybackSpeedCallbackParam, bool bMirror);
    /* 0x08 */ virtual ~cPN_SAnimController() { };
    static void* operator new(size_t)
    {
        cPN_SAnimController* result = NULL;
        m_SAnimControllerSlotPool.Allocate(result);
        return result;
    }
    static void operator delete(void* ptr)
    {
        m_SAnimControllerSlotPool.Free((cPN_SAnimController*)ptr);
    }
    /* 0x10 */ virtual void Evaluate(float weight, cPoseAccumulator* pAccumulator) const;
    /* 0x14 */ virtual void Evaluate(int nodeIndex, float weight, cPoseAccumulator* pAccumulator) const;
    /* 0x18 */ virtual cPoseNode* Update(float t);
    /* 0x1C */ virtual int GetType() { return 0x2; };
    /* 0x20 */ virtual void BlendRootTrans(nlVector3* pRootTrans, float fNodeWeight, float* fAccumulatedWeight);
    /* 0x24 */ virtual void BlendRootRot(unsigned short* pOutRot, float weight, float* pAccumWeight);

    template <typename T>
    void Replay(T& frame)
    {
        Replayable<0>(frame, (cPoseNode&)*this);
        Replayable<0>(frame, FloatCompressor<0, 1, 15>(m_fTime));

        uintptr_t animPtr = 0;
        if (!ReplayFrameTraits<T>::IsLoadFrame)
        {
            animPtr = (uintptr_t)m_pSAnim;
            if (m_bMirror)
                animPtr |= 1;
        }
        Replayable<0>(frame, animPtr);
        if (ReplayFrameTraits<T>::IsLoadFrame)
        {
            m_bMirror = animPtr & 1;
            m_pSAnim = (cSAnim*)(animPtr & ~(uintptr_t)1);
        }
        Replayable<0>(frame, (uintptr_t&)m_pAnimRetarget);
    }

    void UpdateSynchronized(float time);
    void SetTime(float time)
    {
        m_fPrevTime = m_fTime;
        m_fTime = time;
    };
    void ProcessCallbacks();
    bool TestTrigger(float fTime) const;
    bool TestFrameTrigger(float frame);
    int RemapNode(int nodeIndex) const;

    inline const float get_fTime() const
    {
        return m_fTime;
    }

    inline const bool get_bMirror() const
    {
        return m_bMirror;
    }

    /* 0x14 */ cSAnim* m_pSAnim;
    /* 0x18 */ float m_fTime;
    /* 0x1C */ bool m_bMirror;
    /* 0x20 */ const AnimRetarget* m_pAnimRetarget;
    /* 0x24 */ float m_fPrevTime;
    /* 0x28 */ ePlayMode m_ePlayMode;
    /* 0x2C */ mutable float m_fWeight;
    /* 0x30 */ bool m_bIgnoreTriggers;
    /* 0x34 */ void (*m_funcPlaybackSpeedCallback)(uintptr_t, class cPN_SAnimController*);
    /* 0x38 */ uintptr_t m_nPlaybackSpeedCallbackParam;   // PORT: holds a `this`
    /* 0x3C */ float m_fPlaybackSpeedScale;
    /* 0x40 */ bool m_bIsSynchronized;
    /* 0x44 */ cPN_SAnimController* m_pSynchronizedController;
    /* 0x48 */ void (*m_funcSychronizedWeightCallback)(uintptr_t, class cPN_SAnimController*);
    /* 0x4C */ uintptr_t m_nSynchronizedWeightCallbackParam;   // PORT: holds a `this`
    /* 0x50 */ float m_fSynchronizedWeight;

    static SlotPool<cPN_SAnimController> m_SAnimControllerSlotPool;
}; // total size: 0x54

inline cPN_SAnimController* AllocateSAnimController()
{
    cPN_SAnimController* controller = nullptr;
    cPN_SAnimController::m_SAnimControllerSlotPool.Allocate(controller);
    return controller;
}

#endif // _PNSANIMCONTROLLER_H_
