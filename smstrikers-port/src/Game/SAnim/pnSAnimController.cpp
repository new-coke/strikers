#include "Game/SAnim/pnSAnimController.h"
#include "Game/SAnim.h"
#include "Game/SAnim/AnimRetargeter.h"

SlotPool<cPN_SAnimController> cPN_SAnimController::m_SAnimControllerSlotPool(0x10, 0x10);

/**
 * Offset/Address/Size: 0xDC0 | 0x801EB41C | size: 0x98
 */
cPN_SAnimController::cPN_SAnimController(cSAnim* pSAnim, const AnimRetarget* pAnimRetarget, ePlayMode playMode, void (*funcPlaybackSpeedCallback)(uintptr_t, cPN_SAnimController*), uintptr_t nPlaybackSpeedCallbackParam, bool bMirror)
    : cPoseNode()
{
    m_pSAnim = pSAnim;
    m_fTime = 0.0f;
    m_bMirror = bMirror;
    m_pAnimRetarget = pAnimRetarget;
    m_fPrevTime = 0.0f;
    m_ePlayMode = playMode;
    m_bIgnoreTriggers = false;
    m_funcPlaybackSpeedCallback = funcPlaybackSpeedCallback;
    m_nPlaybackSpeedCallbackParam = nPlaybackSpeedCallbackParam;
    m_fPlaybackSpeedScale = 1.0f;
    m_bIsSynchronized = false;
    m_pSynchronizedController = nullptr;
    m_funcSychronizedWeightCallback = nullptr;
    m_fSynchronizedWeight = 1.0f;
}

static inline float Dur(const cPN_SAnimController* p)
{
    return (float)p->m_pSAnim->m_nNumKeys / 30.0f;
}

/**
 * Offset/Address/Size: 0xAF0 | 0x801EB14C | size: 0x2D0
 */
cPoseNode* cPN_SAnimController::Update(float t)
{
    if (!m_bIsSynchronized)
    {
        if (m_funcPlaybackSpeedCallback != nullptr)
        {
            m_funcPlaybackSpeedCallback(m_nPlaybackSpeedCallbackParam, this);
        }

        float fSpeedScale = m_fPlaybackSpeedScale;

        if (m_pSynchronizedController != nullptr)
        {
            if (m_funcSychronizedWeightCallback != nullptr)
            {
                m_funcSychronizedWeightCallback(m_nSynchronizedWeightCallbackParam, this);
            }

            fSpeedScale *= m_fSynchronizedWeight * ((Dur(this) / m_fPlaybackSpeedScale) / (Dur(m_pSynchronizedController) / m_pSynchronizedController->m_fPlaybackSpeedScale) - 1.0f) + 1.0f;
        }

        m_fPrevTime = m_fTime;

        float duration = (float)m_pSAnim->m_nNumKeys / 30.0f;

        if (duration == 0.0f)
        {
            m_fTime = 1.0f;
        }
        else
        {
            m_fTime += (t * fSpeedScale) / duration;
        }

        if (m_fTime > 1.0f)
        {
            switch (m_ePlayMode)
            {
            case PM_CYCLIC:
                do
                {
                    m_fTime -= 1.0f;
                } while (m_fTime > 1.0f);
                break;
            case PM_HOLD:
                m_fTime = 1.0f;
                break;
            }
        }
        else if (m_fTime < 0.0f)
        {
            switch (m_ePlayMode)
            {
            case PM_CYCLIC:
                do
                {
                    m_fTime += 1.0f;
                } while (m_fTime < 0.0f);
                break;
            case PM_HOLD:
                m_fTime = 0.0f;
                break;
            }
        }

        if (t >= 0.0f)
        {
            ProcessCallbacks();
        }

        if (m_pSynchronizedController != nullptr)
        {
            m_pSynchronizedController->UpdateSynchronized(m_fTime);
        }
    }

    return this;
}

/**
 * Offset/Address/Size: 0x7E8 | 0x801EAE44 | size: 0x308
 */
void cPN_SAnimController::UpdateSynchronized(float time)
{
    SetTime(time);
    ProcessCallbacks();

    if (m_pSynchronizedController != nullptr)
    {
        m_pSynchronizedController->UpdateSynchronized(time);
    }
}

/**
 * Offset/Address/Size: 0x760 | 0x801EADBC | size: 0x88
 */
void cPN_SAnimController::Evaluate(float weight, cPoseAccumulator* pAccumulator) const
{
    m_fWeight = weight;

    for (int i = 0; i < pAccumulator->GetNumNodes(); ++i)
    {
        Evaluate(i, weight, pAccumulator);
    }
}

/**
 * Offset/Address/Size: 0x5DC | 0x801EAC38 | size: 0x184
 */
void cPN_SAnimController::Evaluate(int nodeIndex, float weight, cPoseAccumulator* pAccumulator) const
{
    int remappedNodeIndex;

    if (nodeIndex == 0)
    {
        for (int i = 0; i < m_pSAnim->m_nNumMorphChannels; ++i)
        {
            pAccumulator->m_MorphWeights.mData[i] += weight * m_pSAnim->GetMorphWeight(i, m_fTime);
        }
    }

    int actualNodeIndex = nodeIndex;
    if (m_bMirror != 0)
    {
        actualNodeIndex = pAccumulator->m_BaseSHierarchy->GetMirroredNode(nodeIndex);
    }

    if (m_pAnimRetarget != nullptr)
    {
        remappedNodeIndex = m_pAnimRetarget->m_pMap[actualNodeIndex];
    }
    else
    {
        remappedNodeIndex = actualNodeIndex;
    }

    if (remappedNodeIndex != -1)
    {
        m_pSAnim->BlendRot(nodeIndex, remappedNodeIndex, m_fTime, weight, pAccumulator, get_bMirror());
        m_pSAnim->BlendScale(nodeIndex, remappedNodeIndex, m_fTime, weight, pAccumulator, get_bMirror());
        m_pSAnim->BlendTrans(nodeIndex, remappedNodeIndex, m_fTime, weight, pAccumulator, get_bMirror());
        return;
    }

    nlVector3& translationOffset = pAccumulator->m_BaseSHierarchy->GetTranslationOffset(nodeIndex);
    pAccumulator->BlendRotIdentity(nodeIndex, weight);
    pAccumulator->BlendScaleIdentity(nodeIndex, weight);
    pAccumulator->m_trans.mData[nodeIndex].t = translationOffset;
}

static inline void GetRootTransDelta(cPN_SAnimController* pController, nlVector3* pRootTrans, float fStartTime, float fEndTime)
{
    nlVector3 v3LastFrame;

    pController->m_pSAnim->GetRootTrans(fEndTime, pRootTrans);
    pController->m_pSAnim->GetRootTrans(fStartTime, &v3LastFrame);

    pRootTrans->x = pRootTrans->x - v3LastFrame.x;
    pRootTrans->y = pRootTrans->y - v3LastFrame.y;
    pRootTrans->z = pRootTrans->z - v3LastFrame.z;
}

/**
 * Offset/Address/Size: 0x35C | 0x801EA9B8 | size: 0x280
 */
void cPN_SAnimController::BlendRootTrans(nlVector3* pRootTrans, float fNodeWeight, float* fAccumulatedWeight)
{
    unsigned short aLastFrameFacing;
    nlVector3 v3RootTrans;
    nlVector3 v3RootTransLocal;
    nlVector3* pV3RootTransLocal;
    unsigned short aMirrorAdjust;
    nlVector3 v3Extra;
    float fBlendPercent;

    pV3RootTransLocal = &v3RootTransLocal;
    aMirrorAdjust = 0;

    if (m_fTime < m_fPrevTime)
    {
        GetRootTransDelta(this, &v3RootTrans, m_fPrevTime, 1.0f);
        GetRootTransDelta(this, &v3Extra, 0.0f, m_fTime);

        v3RootTrans.x = v3RootTrans.x + v3Extra.x;
        v3RootTrans.y = v3RootTrans.y + v3Extra.y;
        v3RootTrans.z = v3RootTrans.z + v3Extra.z;
    }
    else
    {
        GetRootTransDelta(this, &v3RootTrans, m_fPrevTime, m_fTime);
    }

    m_pSAnim->GetRootRot(m_fPrevTime, &aLastFrameFacing);

    if (m_bMirror != 0)
    {
        aMirrorAdjust = (unsigned short)((aLastFrameFacing - (unsigned short)(int)(10430.378f * nlATan2f(v3RootTrans.y, v3RootTrans.x))) << 1);
    }

    {
        float fCos;
        float fSin;

        nlSinCos(&fSin, &fCos, aMirrorAdjust - aLastFrameFacing);

        pV3RootTransLocal->x = v3RootTrans.x * fCos - v3RootTrans.y * fSin;
        pV3RootTransLocal->y = v3RootTrans.y * fCos + v3RootTrans.x * fSin;
        pV3RootTransLocal->z = v3RootTrans.z;
        *fAccumulatedWeight += fNodeWeight;
    }

    if (*fAccumulatedWeight != 0.0f)
    {
        fBlendPercent = fNodeWeight / *fAccumulatedWeight;
        pRootTrans->x = (1.0f - fBlendPercent) * pRootTrans->x + fBlendPercent * pV3RootTransLocal->x;
        pRootTrans->y = (1.0f - fBlendPercent) * pRootTrans->y + fBlendPercent * pV3RootTransLocal->y;
        pRootTrans->z = (1.0f - fBlendPercent) * pRootTrans->z + fBlendPercent * pV3RootTransLocal->z;
    }
}

/**
 * Offset/Address/Size: 0x1F4 | 0x801EA850 | size: 0x168
 */
void cPN_SAnimController::BlendRootRot(unsigned short* pOutRot, float weight, float* pAccumWeight)
{
    unsigned short deltaRot;

    float currTime = m_fTime;
    float prevTime = m_fPrevTime;

    if (currTime < prevTime)
    {
        unsigned short rot1, rot2, rot3, rot4;
        m_pSAnim->GetRootRot(prevTime, &rot1);
        m_pSAnim->GetRootRot(1.0f, &rot2);
        m_pSAnim->GetRootRot(0.0f, &rot3);
        m_pSAnim->GetRootRot(m_fTime, &rot4);
        deltaRot = (rot2 - rot1) + (rot4 - rot3);
    }
    else
    {
        unsigned short rot1, rot2;
        m_pSAnim->GetRootRot(currTime, &rot2);
        m_pSAnim->GetRootRot(m_fPrevTime, &rot1);
        deltaRot = rot2 - rot1;
    }

    if (m_bMirror != 0)
    {
        deltaRot *= -1;
    }

    *pAccumWeight += weight;
    float w = *pAccumWeight;

    if (w != 0.0f)
    {
        float blendFactor = weight / *pAccumWeight;
        short signedDelta = deltaRot - *pOutRot;
        float blendedDelta = blendFactor * signedDelta;

        *pOutRot = *pOutRot + (int)blendedDelta;
    }
}

/**
 * Offset/Address/Size: 0x114 | 0x801EA770 | size: 0xE0
 */
void cPN_SAnimController::ProcessCallbacks()
{
    if (m_bIgnoreTriggers)
    {
        return;
    }

    cSAnimCallback* pCurrCallback = m_pSAnim->GetCallbackList();
    while (pCurrCallback != nullptr)
    {
        if (TestTrigger(pCurrCallback->m_fTime))
        {
            pCurrCallback->m_funcCallback(pCurrCallback->m_nParam1);
        }

        pCurrCallback = pCurrCallback->next;
    }
}

/**
 * Offset/Address/Size: 0xB0 | 0x801EA70C | size: 0x64
 */
bool cPN_SAnimController::TestTrigger(float fTime) const
{
    if (fTime == 0.0f)
    {
        extern float __float_min[];
        fTime = __float_min[0];
    }

    if (m_fTime < m_fPrevTime)
    {
        return fTime <= m_fTime || fTime > m_fPrevTime;
    }

    return fTime <= m_fTime && fTime > m_fPrevTime;
}

/**
 * Offset/Address/Size: 0x20 | 0x801EA67C | size: 0x90
 */
bool cPN_SAnimController::TestFrameTrigger(float frame)
{
    bool isTriggering;
    float normalizedTime = frame / (float)m_pSAnim->m_nNumKeys;

    if (normalizedTime == 0.0f)
    {
        extern float __float_min[];
        normalizedTime = __float_min[0];
    }

    float currTime = m_fTime;
    float prevTime = m_fPrevTime;

    if (currTime < prevTime)
    {
        isTriggering = 0;
        if (normalizedTime <= currTime || normalizedTime > prevTime)
        {
            isTriggering = 1;
        }
    }
    else
    {
        isTriggering = 0;
        if ((normalizedTime <= currTime) && (normalizedTime > prevTime))
        {
            isTriggering = 1;
        }
    }
    return isTriggering;
}

/**
 * Offset/Address/Size: 0x0 | 0x801EA65C | size: 0x20
 */
int cPN_SAnimController::RemapNode(int nodeIndex) const
{
    int remappedIndex = nodeIndex;
    if (m_pAnimRetarget != NULL)
    {
        remappedIndex = m_pAnimRetarget->m_pMap[remappedIndex];
    }
    return remappedIndex;
}
