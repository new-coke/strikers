#ifndef _ANIMVIEWERCAM_H_
#define _ANIMVIEWERCAM_H_

#include "Game/Camera/BaseCamera.h"
#include "Game/Camera/FollowCam.h"
#include "Game/Player.h"

class cAnimViewerCamera : public cFollowCamera
{
public:
    cAnimViewerCamera();

    virtual eCameraType GetType();
    virtual ~cAnimViewerCamera();
    virtual void Update(float fDeltaT);

    cPlayer* m_pCurrentPlayer;
};

#endif // _ANIMVIEWERCAM_H_
