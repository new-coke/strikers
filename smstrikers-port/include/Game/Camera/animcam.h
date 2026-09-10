#ifndef _ANIMCAM_H_
#define _ANIMCAM_H_

#include "Game/Camera/BaseCamera.h"
#include "Game/SAnim.h"
#include "NL/nlList.h"
#include "NL/nlString.h"

class cCameraData
{
public:
    ~cCameraData()
    {
        if (ownsKeyData)
        {
            delete[] cameraPos;
            delete[] targetPos;
            delete[] cameraRot;
            delete[] fFOV;
            delete[] fFocalLength;
        }
    }

    /*  0x00 */ cCameraData* next;
    /*  0x04 */ unsigned long m_uHashID;
    /*  0x08 */ unsigned long m_uKeyCount;
    /*  0x0C */ nlVector3* cameraPos;
    /*  0x10 */ nlVector3* targetPos;
    /*  0x14 */ nlQuaternion* cameraRot;
    /*  0x18 */ float* fFOV;
    /*  0x1C */ float* fFocalLength;
    /*  0x20 */ bool ownsKeyData;
}; // total size: 0x24

class cAnimCamera : public cBaseCamera
{
public:
    cAnimCamera();
    /* 0x08 */ virtual ~cAnimCamera();
    /* 0x0C */ virtual eCameraType GetType();
    /* 0x24 */ virtual const nlVector3& GetTargetPosition() const;
    /* 0x20 */ virtual const nlVector3& GetCameraPosition() const;
    /* 0x18 */ virtual float GetFOV() const;
    /* 0x14 */ virtual const nlMatrix4& GetViewMatrix() const;

    static bool LoadCameraAnimation(nlChunk* begin, nlChunk* end, const char* cameraName, bool ownsKeyData);
    static bool LoadCameraAnimation(const char* szFilename, const char* szCameraName, bool ownsKeyData);
    static void FreeCameraAnimations();

    void BuildAnimViewMatrix(nlMatrix4& mView);
    void UnselectCameraAnimation();
    void SelectCameraAnimation(const char* name);
    bool CameraAnimationExists(const char* name) const;
    static void FreeCameraAnimation(const char* szCameraName);
    void Update(float dt);
    void ManualUpdate(float dt);

    static cCameraData* m_cameraDataList;

    /* 0x1C */ bool m_bCyclic;
    /* 0x1D */ bool m_bUseSimulationTime;
    /* 0x1E */ bool m_LetManagerDoUpdate;
    /* 0x1F */ bool m_bUnusedPad;
    /* 0x20 */ nlMatrix4 m_matView;
    /* 0x60 */ nlVector3 m_vecCamera;
    /* 0x6C */ nlVector3 m_vecTarget;
    /* 0x78 */ float m_fAnimationTime;
    /* 0x7C */ float m_fAnimationSpeed;
    /* 0x80 */ float m_fLastSimulationTime;
    /* 0x84 */ cCameraData* m_pActiveCameraData;
    /* 0x88 */ nlVector3 m_OffsetPos;
    /* 0x94 */ nlVector3 m_Mirror;
    /* 0xA0 */ unsigned short mFacingAngle;
    /* 0xA4 */ float m_Fov;
    /* 0xA8 */ void (*m_EndOfAnimationCallback)();
}; // total size: 0xAC

inline eCameraType cAnimCamera::GetType()
{
    return eCameraType_Animated;
}

inline const nlVector3& cAnimCamera::GetTargetPosition() const
{
    return m_vecTarget;
}

inline const nlVector3& cAnimCamera::GetCameraPosition() const
{
    return m_vecCamera;
}

inline float cAnimCamera::GetFOV() const
{
    return m_Fov;
}

inline const nlMatrix4& cAnimCamera::GetViewMatrix() const
{
    return m_matView;
}

#endif // _ANIMCAM_H_
