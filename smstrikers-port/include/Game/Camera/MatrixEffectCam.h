#ifndef _MATRIXEFFECTCAM_H_
#define _MATRIXEFFECTCAM_H_

#include "NL/nlMath.h"
#include "Game/Camera/BaseCamera.h"

class MatrixEffectCam : public cBaseCamera
{
public:
    MatrixEffectCam();
    /* 0x08 */ virtual ~MatrixEffectCam();
    /* 0x0C */ virtual eCameraType GetType() { return eCameraType_MatrixEffect; }
    /* 0x10 */ virtual void Update(float dt);
    /* 0x18 */ virtual float GetFOV() const { return mfFOV; };
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const { return mViewMatrix; };
    /* 0x20 */ virtual const nlVector3& GetTargetPosition() const;
    /* 0x24 */ virtual const nlVector3& GetCameraPosition() const;

    void Reset(const nlVector3& cameraStart, const nlVector3& beginTarget, const nlVector3& endTarget);
    void SetInitialDistance(float distance);
    void SetInitialAngle(float angle);
    void SetInitialHeightAboveTarget(float distance);

    /* 0x01C */ f32 mfSpinDuration;
    /* 0x020 */ f32 mfPauseAfterSpin;
    /* 0x024 */ f32 mfZoomTime;
    /* 0x028 */ f32 mfDesiredDistanceFromTarget;
    /* 0x02C */ f32 mfDesiredHeightAboveTarget;
    /* 0x030 */ f32 mfSpinRate;
    /* 0x034 */ nlVector3 mFinalTarget;
    /* 0x040 */ nlVector3 mBeginTarget;
    /* 0x04C */ mutable nlVector3 mTargetPosition;
    /* 0x058 */ nlMatrix4 mViewMatrix;
    /* 0x098 */ f32 mfFOV;
    /* 0x09C */ nlPolar mCurrentPolarFromTarget;
    /* 0x0A4 */ f32 mfCurrentCameraHeightAboveTarget;
    /* 0x0A8 */ f32 mfElapsedTime;
    /* 0x0AC */ void (*mFinishedCallback)(MatrixEffectCam*);
    /* 0x0B0 */ bool mbZoomingIn;
    /* 0x0B1 */ bool mbFollowBall;
    /* 0x0B4 */ nlVector3 mTargetFilterData[10];
    /* 0x12C */ s32 mCurrentFilterDataIndex;
    /* 0x130 */ nlVector3 mFilteredTargetPosition;
    /* 0x13C */ bool mbUseGameplayTransparencyFlags;
}; // total size: 0x140

#endif // _MATRIXEFFECTCAM_H_
