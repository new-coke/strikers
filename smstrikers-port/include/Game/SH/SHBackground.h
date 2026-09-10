#ifndef _SHBACKGROUND_H_
#define _SHBACKGROUND_H_

#include "types.h"

#include "Game/BaseSceneHandler.h"

enum PlayMode
{
    PM_INVALID = -1,
    PM_STOPPED = 0,
    PM_LOOPING = 1,
    PM_STOP_AT_END = 2,
    PM_USE_SLIDE_DEFAULT = 3,
};

class BackgroundScene : public BaseSceneHandler
{
public:
    BackgroundScene();
    virtual ~BackgroundScene();
    void ActivatePlayMode();
    virtual void Update(float dt);
    virtual void SceneCreated();

    /* 0x1C */ PlayMode mPlayMode;
    /* 0x20 */ PlayMode mDesiredPlayMode;
}; // total size: 0x24

#endif // _SHBACKGROUND_H_
