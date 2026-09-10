#ifndef _SHQUICKGAMEPLAYOPTIONS_H_
#define _SHQUICKGAMEPLAYOPTIONS_H_

#include "Game/BaseSceneHandler.h"

class OptionsGameplayMenuV2;

class QuickGameplayOptionsScene : public BaseSceneHandler
{
public:
    QuickGameplayOptionsScene();
    virtual ~QuickGameplayOptionsScene();
    virtual void Update(float dt);
    virtual void SceneCreated();

    /* 0x1C */ OptionsGameplayMenuV2* m_pOptionsMenu;
}; // size: 0x20

#endif // _SHQUICKGAMEPLAYOPTIONS_H_
