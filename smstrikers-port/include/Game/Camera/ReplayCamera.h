#ifndef _REPLAYCAMERA_H_
#define _REPLAYCAMERA_H_

#include "NL/gl/glMatrix.h"
#include "Game/Camera/BaseCamera.h"

enum ReplayCameraPosition
{
    REPLAY_CAMERA_POSITION_INSIDE_NET = 0,
    REPLAY_CAMERA_POSITION_SIDELINE = 1,
    REPLAY_CAMERA_POSITION_BALL_TO_GOAL = 2,
    REPLAY_CAMERA_POSITION_HIGH_UP = 3,
    REPLAY_CAMERA_POSITION_GENERIC_0 = 4,
    REPLAY_CAMERA_POSITION_GENERIC_1 = 5,
    REPLAY_CAMERA_POSITION_GENERIC_2 = 6,
    REPLAY_CAMERA_POSITION_GENERIC_3 = 7,
    REPLAY_CAMERA_POSITION_GENERIC_4 = 8,
    REPLAY_CAMERA_POSITION_GENERIC_5 = 9,
    REPLAY_CAMERA_POSITION_GENERIC_6 = 10,
    REPLAY_CAMERA_POSITION_GENERIC_7 = 11,
    REPLAY_CAMERA_POSITION_GENERIC_8 = 12,
    REPLAY_CAMERA_POSITION_GENERIC_9 = 13,
    REPLAY_CAMERA_POSITION_GENERIC_10 = 14,
    REPLAY_CAMERA_POSITION_GENERIC_11 = 15,
    REPLAY_CAMERA_POSITION_GENERIC_12 = 16,
    REPLAY_CAMERA_POSITION_GENERIC_13 = 17,
    REPLAY_CAMERA_POSITION_GENERIC_14 = 18,
    REPLAY_CAMERA_POSITION_GENERIC_15 = 19,
    REPLAY_CAMERA_POSITION_GENERIC_LAST = 20,
    REPLAY_CAMERA_POSITION_NUM_POSITIONS = 21,
};

class ReplayCamera : public cBaseCamera
{
public:
    ReplayCamera();
    /* 0x0C */ virtual eCameraType GetType() { return eCameraType_Replay; };
    /* 0x10 */ virtual void Update(float fDeltaT);
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const;
    /* 0x20 */ virtual const nlVector3& GetTargetPosition() const { return mLookAt; };
    /* 0x24 */ virtual const nlVector3& GetCameraPosition() const { return mPosition; };
    /* 0x18 */ virtual float GetFOV() const { return mFov; };

    static void UpdateTweakMode();
    void ManualUpdate(float deltaT);
    void SetSideOfInterest(int sideOfInterest);
    void CutTo(ReplayCameraPosition camPos);
    float GetFov(ReplayCameraPosition position) const;
    nlVector3 GetPosition(ReplayCameraPosition position, float direction) const;

private:
    static void Dampen(nlVector3& from, const nlVector3& to, float dampFactor);

public:
    /* 0x1C */ f32 mDeltaFov;
    /* 0x20 */ f32 mFov;
    /* 0x24 */ s32 mSideOfInterest;
    /* 0x28 */ bool mNoDampenForOneUpdate;
    /* 0x29 */ bool mFrozen;
    /* 0x2C */ u32 mFocus;
    /* 0x30 */ ReplayCameraPosition mCamPos;
    /* 0x34 */ nlVector3 mPosition;
    /* 0x40 */ nlVector3 mLookAt;
    /* 0x4C */ nlMatrix4 mViewMatrix;
}; // total size: 0x8C

#endif // _REPLAYCAMERA_H_
