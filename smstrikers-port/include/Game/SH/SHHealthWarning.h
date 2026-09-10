#ifndef _SHHEALTHWARNING_H_
#define _SHHEALTHWARNING_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feAsyncImage.h"

class HealthWarningSceneV2 : public BaseSceneHandler
{
public:
    HealthWarningSceneV2();
    virtual ~HealthWarningSceneV2();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);

    /* 0x1C */ AsyncImage* mMessageImage;
    /* 0x20 */ AsyncImage* mPressButtonImage;
    /* 0x24 */ f32 mElapsedTime;
    /* 0x28 */ bool mIsPressButtonVisible;
};

#endif // _SHHEALTHWARNING_H_
