#ifndef _KICKOFFCAM_H_
#define _KICKOFFCAM_H_

#include "Game/Camera/BaseCamera.h"

class cKickOffCamera : public cBaseCamera
{
public:
    cKickOffCamera();
    /* 0x08 */ virtual ~cKickOffCamera();
    /* 0x0C */ virtual eCameraType GetType() { return eCameraType_KickOff; };
    /* 0x10 */ virtual void Update(float dt);
    /* 0x24 */ virtual const nlVector3& GetTargetPosition() const { return m_v3Target; };
    /* 0x20 */ virtual const nlVector3& GetCameraPosition() const { return m_v3Camera; };
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const { return m_matView; };

    /*  0x1C */ nlMatrix4 m_matView;
    /*  0x5C */ nlVector3 m_v3Camera;
    /*  0x68 */ nlVector3 m_v3Target;
}; // total size: 0x28

#endif // _KICKOFFCAM_H_
