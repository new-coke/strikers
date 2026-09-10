#ifndef _SHSPOILS_H_
#define _SHSPOILS_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/feButtonComponent.h"

class SpoilsScene : public BaseSceneHandler
{
public:
    enum eSpoils
    {
        SPOILS_INVALID = -1,
        SPOILS_CUP = 0,
        SPOILS_SUPER_CUP = 1,
        SPOILS_MILESTONE = 2,
        SPOILS_NUM_CHOICES = 3,
    };

    SpoilsScene();
    virtual ~SpoilsScene();
    virtual void Update(float dt);
    virtual void SceneCreated();
    void ShowSpoils(SpoilsScene::eSpoils spoils);

    /* 0x01C */ MenuList<TLComponentInstance> mMenuItems; // size 0x214
    /* 0x230 */ ButtonComponent mButtons;                 // size 0x24

    static s32 mLastSelectedIndex;
}; // total size: 0x254

#endif // _SHSPOILS_H_
