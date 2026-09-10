#ifndef _GOALCAM_H_
#define _GOALCAM_H_

#include "Game/Camera/BaseCamera.h"

class GoalCamera : public cBaseCamera
{
public:
    GoalCamera();
    /* 0x08 */ virtual ~GoalCamera();
    /* 0x0C */ virtual eCameraType GetType() { return eCameraType_Goal; };
    /* 0x10 */ virtual void Update(float dt);
    /* 0x24 */ virtual const nlVector3& GetTargetPosition() const { return m_vecTarget; };
    /* 0x20 */ virtual const nlVector3& GetCameraPosition() const { return m_vecCamera; };
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const { return m_matView; };

    /* 0x1C */ nlMatrix4 m_matView;   // offset 0x1C, size 0x40
    /* 0x5C */ nlVector3 m_vecCamera; // offset 0x5C, size 0xC
    /* 0x68 */ nlVector3 m_vecTarget; // offset 0x68, size 0xC
}; // total size: 0x74

#endif // _GOALCAM_H_
