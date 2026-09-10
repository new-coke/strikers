#ifndef _FACECAM_H_
#define _FACECAM_H_

#include "NL/nlMath.h"
#include "Game/Camera/BaseCamera.h"
#include "Game/Character.h"

class FaceCam : public cBaseCamera
{
public:
    FaceCam(float distance);
    virtual ~FaceCam() { };
    virtual eCameraType GetType()
    {
        return eCameraType_FaceCloseup;
    };
    virtual void Update(float dt);
    virtual const nlMatrix4& GetViewMatrix() const
    {
        return mViewMatrix;
    };
    virtual const nlVector3& GetCameraPosition() const;
    virtual const nlVector3& GetTargetPosition() const;

    void SetToUserCharacter();

    /* 0x1C */ cCharacter* mpCharacter;
    /* 0x20 */ float mDistance;
    /* 0x24 */ nlVector3 mTargetPosition;
    /* 0x30 */ nlVector3 mCameraPosition;
    /* 0x3C */ nlMatrix4 mViewMatrix;
    /* 0x7C */ float mfFOV;
};

#endif // _FACECAM_H_
