#ifndef _FOLLOWCAM_H_
#define _FOLLOWCAM_H_

#include "Game/Camera/BaseCamera.h"

class cFollowCamera : public cBaseCamera
{
public:
    enum FollowTarget
    {
        FOLLOW_BALL = 0,
        FOLLOW_CHARACTER = 1,
        FOLLOW_SELECTABLE = 2,
        FOLLOW_ANIM_VIEWER_CHARACTER = 3,
    };

    cFollowCamera();
    cFollowCamera(cFollowCamera::FollowTarget followTarget);
    virtual ~cFollowCamera() { };

    /* 0x0C */ virtual eCameraType GetType()
    {
        if (m_FollowTarget == FOLLOW_BALL)
            return eCameraType_FollowBall;
        return eCameraType_FollowCharacter;
    };
    /* 0x10 */ virtual void Update(float fDeltaT);
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const { return m_matView; };
    /* 0x20 */ virtual const nlVector3& GetTargetPosition() const { return m_v3OOIDampened; };
    /* 0x24 */ virtual const nlVector3& GetCameraPosition() const { return m_v3CameraPosition; };

    /* 0x1C */ FollowTarget m_FollowTarget;   // offset 0x1C, size 0x4
    /* 0x20 */ nlMatrix4 m_matView;           // offset 0x20, size 0x40
    /* 0x60 */ nlVector3 m_v3CameraPosition;  // offset 0x60, size 0xC
    /* 0x6C */ nlVector3 m_v3OOI;             // offset 0x6C, size 0xC
    /* 0x78 */ nlVector3 m_v3OOIDampened;     // offset 0x78, size 0xC
    /* 0x84 */ nlVector3 m_v3OOIDampenedPrev; // offset 0x84, size 0xC
    /* 0x90 */ bool m_bOOISet;                // offset 0x90, size 0x1
    /* 0x92 */ u16 m_aFacingDirection;        // offset 0x92, size 0x2
    /* 0x94 */ u16 m_aPitch;                  // offset 0x94, size 0x2
    /* 0x98 */ f32 m_fOOIDistance;            // offset 0x98, size 0x4
    /* 0x9C */ bool m_bPitchLimits;           // offset 0x9C, size 0x1
    /* 0x9D */ bool m_bControlsLocked;        // offset 0x9D, size 0x1
}; // total size: 0xA0

#endif // _FOLLOWCAM_H_
