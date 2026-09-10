#ifndef _SHLOADINGTRANSITION_H_
#define _SHLOADINGTRANSITION_H_

#include "Game/BaseSceneHandler.h"

class LoadingTransitionScene : public BaseSceneHandler
{
public:
    LoadingTransitionScene();
    virtual ~LoadingTransitionScene();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);
};

#endif // _SHLOADINGTRANSITION_H_
