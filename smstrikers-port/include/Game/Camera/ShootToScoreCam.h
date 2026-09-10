#ifndef _SHOOTTOSCORECAM_H_
#define _SHOOTTOSCORECAM_H_

#include "Game/Camera/BaseCamera.h"

class cShootToScoreCamera : public cBaseCamera
{
public:
    cShootToScoreCamera();
    virtual ~cShootToScoreCamera();
    virtual void Update(float fDeltaT);

    virtual const nlVector3& GetCameraPosition() const
    {
        return m_v3Camera;
    };

    virtual const nlVector3& GetTargetPosition() const
    {
        return m_v3Target;
    };

    virtual eCameraType GetType()
    {
        return eCameraType_ShootToScore;
    }

    virtual const nlMatrix4& GetViewMatrix() const
    {
        return m_matView;
    };

    /* 0x1C */ nlMatrix4 m_matView;
    /* 0x5C */ nlVector3 m_v3Camera;
    /* 0x68 */ nlVector3 m_v3Target;

}; // total size: 0x74

// #include "NL/nlMath.h"
// #include "Game/Camera/BaseCamera.h"
// #include "Game/Character.h"

// class FaceCam : public cBaseCamera
// {
// public:
//     virtual eCameraType GetType()
//     {
//     };
//     virtual const nlMatrix4& GetViewMatrix() const
//     {
//     };

//     /* 0x1C */ cCharacter* mpCharacter;
//     /* 0x20 */ float mDistance;
//     /* 0x24 */ nlVector3 mTargetPosition;
//     /* 0x30 */ nlVector3 mCameraPosition;
//     /* 0x3C */ nlMatrix4 mViewMatrix;
//     /* 0x7C */ float mfFOV;
// };

#endif // _SHOOTTOSCORECAM_H_
