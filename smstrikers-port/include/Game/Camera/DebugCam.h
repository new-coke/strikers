#ifndef _DEBUGCAM_H_
#define _DEBUGCAM_H_

#include "Game/Camera/BaseCamera.h"

class cDebugCamera : public cBaseCamera
{
public:
    cDebugCamera();
    /* 0x08 */ virtual ~cDebugCamera();
    /* 0x0C */ virtual eCameraType GetType() { return eCameraType_Debug; };
    /* 0x20 */ virtual const nlVector3& GetTargetPosition() const { return m_vecTarget; };
    /* 0x24 */ virtual const nlVector3& GetCameraPosition() const { return m_vecCamera; };
    /* 0x18 */ float GetFOV() const { return m_Fov; };
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const { return m_matView; };
    /* 0x10 */ void Update(float dt);

    void UpdateCameraControls(float dt);

    /* 0x1C */ nlMatrix4 m_matView;
    /* 0x5C */ float m_Fov;
    /* 0x60 */ float m_fRadius;
    /* 0x64 */ float m_fAzimuth;
    /* 0x68 */ float m_fTheta;
    /* 0x6C */ float m_fHeight;
    /* 0x70 */ nlVector3 m_vecCamera;
    /* 0x7C */ nlVector3 m_vecTarget;
    /* 0x88 */ bool m_bEnableControls;
}; // total size: 0x8C

#endif // _DEBUGCAM_H_
