#ifndef _TOPDOWNCAMERA_H_
#define _TOPDOWNCAMERA_H_

#include "Game/Camera/BaseCamera.h"

class TopDownCamera : public cBaseCamera
{
public:
    TopDownCamera();
    virtual ~TopDownCamera();
    virtual const nlMatrix4& GetViewMatrix() const;
    virtual void Update(float dt);

    virtual eCameraType GetType()
    {
        return eCameraType_TopDown;
    };

    virtual const nlVector3& GetTargetPosition() const
    {
        return m_Target;
    };

    virtual const nlVector3& GetCameraPosition() const
    {
        return m_Position;
    }

    virtual float GetFOV() const
    {
        return m_Fov;
    };

    /* 0x1C */ nlMatrix4 m_View;
    /* 0x5C */ float m_Fov;
    /* 0x60 */ nlVector3 m_Position;
    /* 0x6C */ nlVector3 m_Target;
}; // total size: 0x78

#endif // _TOPDOWNCAMERA_H_
