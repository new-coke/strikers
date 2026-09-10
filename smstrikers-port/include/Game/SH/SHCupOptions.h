#ifndef _SHCUPOPTIONS_H_
#define _SHCUPOPTIONS_H_

#include "Game/BaseSceneHandler.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/FE/feOptionsSubMenus.h"

class CupOptionsScene : public BaseSceneHandler
{
public:
    CupOptionsScene(SceneList forwardScene, SceneList backScene);
    virtual ~CupOptionsScene();
    virtual void Update(float dt);
    virtual void SceneCreated();

    /* 0x1C */ OptionsGameplayMenuV2* m_gameplayMenu;
    /* 0x20 */ SceneList m_backScene;
    /* 0x24 */ SceneList m_forwardScene;
}; // total size: 0x28

#endif // _SHCUPOPTIONS_H_
