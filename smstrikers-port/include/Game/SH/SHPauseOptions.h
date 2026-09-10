#ifndef _SHPAUSEOPTIONS_H_
#define _SHPAUSEOPTIONS_H_

#include "Game/FE/feInput.h"
#include "Game/FE/BaseOverlayHandler.h"
#include "Game/FE/feOptionsSubMenus.h"

class PauseOptionsScene : public BaseOverlayHandler
{
public:
    enum Mode
    {
        MODE_INVALID = -1,
        MODE_AUDIO = 0,
        MODE_VISUAL = 1,
        MODE_OLD = 2,
    };

    PauseOptionsScene(PauseOptionsScene::Mode mode);
    virtual ~PauseOptionsScene();
    virtual void SceneCreated();
    virtual void Update(float dt);

    /* 0x28 */ Mode m_desiredMode;
    /* 0x2C */ OptionsSubMenu* m_pauseMenu;
    /* 0x30 */ eFEINPUT_PAD m_controllingInput;
}; // total size: 0x34

#endif // _SHPAUSEOPTIONS_H_
