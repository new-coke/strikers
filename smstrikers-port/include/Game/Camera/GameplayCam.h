#ifndef _GAMEPLAYCAM_H_
#define _GAMEPLAYCAM_H_

#include "Game/Camera/BaseCamera.h"

struct CameraData
{
    /* 0x00 */ int numKnotsX;
    /* 0x04 */ int numKnotsY;
    /* 0x08 */ const float* targetKnotsX[3];
    /* 0x14 */ const float* fieldKnotsX[3];
    /* 0x20 */ const float* targetKnotsY[3];
    /* 0x2C */ const float* fieldKnotsY[3];
    /* 0x38 */ float fov;
    /* 0x3C */ float distance;
    /* 0x40 */ float pitch;
    /* 0x44 */ float orientation;
}; // total size: 0x48

class KnotTableBlendEntry
{
public:
    KnotTableBlendEntry()
        : nKnotTable(0)
        , fBlendRiser(1.0f) {
        };

    /* 0x00 */ int nKnotTable;
    /* 0x04 */ float fBlendRiser;
}; // total size: 0x8

class GameplayCameraZoomLevel
{
public:
    GameplayCameraZoomLevel();
    void CalcDesiredTarget();
    void Update(float fDeltaT, bool forceNeutral);

    /* 0x00 */ const CameraData* m_CameraData;
    /* 0x04 */ float m_fDesiredTargetX;
    /* 0x08 */ float m_fDesiredTargetY;
    /* 0x0C */ float m_fDampenedTargetX;
    /* 0x10 */ float m_fDampenedTargetY;
    /* 0x14 */ float m_fTargetSeekSpeedX;
    /* 0x18 */ float m_fTargetSeekSpeedY;
    /* 0x1C */ float m_fTargetSeekTime;
    /* 0x20 */ KnotTableBlendEntry m_KnotTableBlendQueue[5];
    /* 0x48 */ nlVector3 m_v3Target;
    /* 0x54 */ nlVector3 m_v3Camera;
}; // total size: 0x60

class GameplayCamera : public cBaseCamera
{
public:
    GameplayCamera();
    /* 0x0C */ virtual eCameraType GetType() { return eCameraType_Gameplay; };
    /* 0x10 */ virtual void Update(float deltaTime);
    /* 0x20 */ virtual const nlVector3& GetTargetPosition() const { return m_v3Target; };
    /* 0x24 */ virtual const nlVector3& GetCameraPosition() const { return m_v3Camera; };
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const { return m_matView; };
    /* 0x18 */ virtual float GetFOV() const { return m_fFOV; };
    /* 0x1C */ virtual void Reactivate();
    void CalcDynamicZoom();

    /* 0x01C */ GameplayCameraZoomLevel m_nearZoom;
    /* 0x07C */ GameplayCameraZoomLevel m_farZoom;
    /* 0x0DC */ bool m_bDynamicZoom;
    /* 0x0E0 */ float m_fZoom;
    /* 0x0E4 */ float m_fDesiredZoom;
    /* 0x0E8 */ float m_fZoomSeekSpeed;
    /* 0x0EC */ nlVector3 m_v3Target;
    /* 0x0F8 */ nlVector3 m_v3Camera;
    /* 0x104 */ float m_fFOV;
    /* 0x108 */ class nlMatrix4 m_matView;
    /* 0x148 */ bool m_ForceNeutralAndNearZoom;
}; // total size: 0x14C

#endif // _GAMEPLAYCAM_H_
