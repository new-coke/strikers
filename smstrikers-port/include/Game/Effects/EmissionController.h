#ifndef _EMISSIONCONTROLLER_H_
#define _EMISSIONCONTROLLER_H_

#include "Game/Replay.h"
#include <stdint.h>

#include "NL/nlMath.h"
#include "NL/nlFunction.h"
#include "NL/gl/gl.h"

#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/efList.h"

class cPN_SAnimController;
class cPoseAccumulator;
class EmissionController;
typedef void FnEmissionController(EmissionController&);

void* fxLoadEntireFileHigh(const char* filename, unsigned long* fileSize);

class EmissionController : public efNode
{
public:
    EmissionController(EffectsGroup* pEffectsGroup, unsigned short id, eGLView view);
    ~EmissionController();
    void InitializeSystemsFromGroup();
    void SetPosition(const nlVector3& pos);
    const nlVector3& GetPosition() const;
    void SetDirection(const nlVector3& dir);
    void SetVelocity(const nlVector3& velocity);
    void SetPoseAccumulator(const cPoseAccumulator& pose);
    void SetAnimController(const cPN_SAnimController& animController);
    void Die();
    float GetRemainingTime() const;
    bool IsLingering() const;
    bool Update(float dt);
    void Render();
    void SetUpdateCallback(const Function1<void, EmissionController&>& callback);
    void SetFinishedCallback(const Function1<void, EmissionController&>& callback);

    template <typename T>
    void Replay(T& frame);

    /* 0x8, */ EffectsGroup* m_pGroup;
    /* 0xC, */ efList m_Systems;
    /* 0x18 */ Function1<void, EmissionController&> mUpdateCallback;
    /* 0x20 */ Function1<void, EmissionController&> mFinishedCallback;
    /* 0x28 */ s8 m_GlView;
    /* 0x29 */ bool m_Replaying;
    /* 0x2C */ f32 m_Age;
    /* 0x30 */ f32 m_ReplayDeltaTime;
    /* 0x34 */ f32 m_fGround;
    /* 0x38 */ u16 m_aFacing;
    /* 0x3A */ bool m_bVisible;
    /* 0x3B */ bool m_bDisabled;
    /* 0x3C */ nlVector3 m_Mirror;
    /* 0x48 */ nlVector3 m_vPosition;
    /* 0x54 */ nlVector3 m_vDirection;
    /* 0x60 */ nlVector3 m_vVelocity;
    /* 0x6C */ const cPoseAccumulator* m_pPose;
    /* 0x70 */ const cPN_SAnimController* m_pAnimController;
    /* 0x74 */ u16 m_Id;
    /* 0x78 */ uintptr_t m_uUserData;
    /* 0x7C */ u32 m_uJointIDOverride;
    /* 0x80 */ s32 m_nUserEffects;
    /* 0x84 */ UserEffectSpec** m_pUserEffects;
    /* 0x88 */ bool m_bPoseErrorDisplayed;
};

template <typename T>
inline void EmissionController::Replay(T& frame)
{
    ::Replayable<0>(frame, (uintptr_t&)m_pPose);
    ::Replayable<0>(frame, (uintptr_t&)m_pAnimController);
    frame.template Replayable<0>(m_uUserData);
    ::Replayable<0>(frame, m_fGround);
    frame.template Replayable<0>(m_aFacing);
    ::Replayable<0>(frame, (char&)m_GlView);
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vPosition.x));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vPosition.y));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vPosition.z));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vDirection.x));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vDirection.y));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vDirection.z));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vVelocity.x));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vVelocity.y));
    ::Replayable<0>(frame, FloatCompressor<-255, 255, 6>(m_vVelocity.z));

    if (ReplayFrameTraits<T>::IsLoadFrame)
    {
        m_Replaying = true;

        float age = 0.0f;
        ::Replayable<0>(frame, age);
        age += reinterpret_cast<LoadFrame&>(frame).mNonBlendableAheadOfFrame;
        m_ReplayDeltaTime = age - m_Age;
        m_Age = age;

        // PORT: the save path writes these as uintptr_t; read the same width or the pointer is truncated and the frame misaligned.
        uintptr_t updateCb = 0;
        ::Replayable<0>(frame, updateCb);
        uintptr_t callback = updateCb;
        if (callback != 0)
        {
            mUpdateCallback = (void (*)(EmissionController&))callback;
        }

        uintptr_t finishedCb = 0;
        ::Replayable<0>(frame, finishedCb);
        callback = finishedCb;
        if (callback != 0)
        {
            mFinishedCallback = (void (*)(EmissionController&))callback;
        }
    }
    else
    {
        m_Replaying = false;
        m_ReplayDeltaTime = 0.0f;
        ::Replayable<0>(frame, m_Age);

        uintptr_t updateCb = (uintptr_t)mUpdateCallback.GetFreeFunction();
        ::Replayable<0>(frame, updateCb);

        uintptr_t finishedCb = (uintptr_t)mFinishedCallback.GetFreeFunction();
        ::Replayable<0>(frame, finishedCb);
    }
}

#endif // _EMISSIONCONTROLLER_H_
