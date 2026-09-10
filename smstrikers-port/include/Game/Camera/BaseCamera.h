#ifndef _BASECAMERA_H_
#define _BASECAMERA_H_

#include "NL/nlMath.h"
#include "Game/Camera/rumblefilter.h"

enum eCameraType
{
    eCameraType_Debug = 0,
    eCameraType_FollowCharacter = 1,
    eCameraType_FollowBall = 2,
    eCameraType_Replay = 3,
    eCameraType_Animated = 4,
    eCameraType_KickOff = 5,
    eCameraType_TopDown = 6,
    eCameraType_Gameplay = 7,
    eCameraType_MatrixEffect = 8,
    eCameraType_Goal = 9,
    eCameraType_ShootToScore = 10,
    eCameraType_AnimViewer = 11,
    eCameraType_FaceCloseup = 12,
    NUM_CAMERA_TYPES = 13,
};

class cBaseCamera
{
public:
    cBaseCamera()
    {
        m_pFilter = nullptr;
        mUpVector.x = 0.0f;
        mUpVector.y = 0.0f;
        mUpVector.z = 1.0f;
    };
    /* 0x08 */ virtual ~cBaseCamera() { };
    /* 0x0C */ virtual eCameraType GetType() = 0;
    /* 0x10 */ virtual void Update(float) = 0;
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const = 0;
    /* 0x18 */ virtual float GetFOV() const { return 27.0f; }
    /* 0x1C */ virtual void Reactivate() { };
    /* 0x20 */ virtual const nlVector3& GetTargetPosition() const = 0;
    /* 0x24 */ virtual const nlVector3& GetCameraPosition() const = 0;

    /* 0x04 */ cBaseCamera* m_next;
    /* 0x08 */ cBaseCamera* m_prev;
    /* 0x0C */ cRumbleFilter* m_pFilter;
    /* 0x10 */ nlVector3 mUpVector;
}; // total size: 0x1C

#endif // _BASECAMERA_H_
