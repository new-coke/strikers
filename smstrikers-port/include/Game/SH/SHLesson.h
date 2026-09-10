#ifndef _SHLESSON_H_
#define _SHLESSON_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feButtonComponent.h"

class LessonScene : public BaseSceneHandler
{
public:
    LessonScene();
    virtual ~LessonScene();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);

    static void SetLesson(int index);

    /* 0x1C */ bool mHUDWasVisible;
    /* 0x20 */ BaseSceneHandler* mHUDScene;
    /* 0x24 */ ButtonComponent mButtons;

    static int mLessonIndex;
}; // total size: 0x48

#endif // _SHLESSON_H_
