#ifndef _SHSKILLSELECT_H_
#define _SHSKILLSELECT_H_

#include "Game/BaseSceneHandler.h"

class FESlideMenu;

class SkillSelectScene : public BaseSceneHandler
{
public:
    virtual void Update(float dt);
    virtual void SceneCreated();
    virtual ~SkillSelectScene();
    SkillSelectScene(bool isSuperCup);

    /* 0x1C */ bool mIsSuperCup;
    /* 0x20 */ FESlideMenu* m_SlideSkillSelect;
}; // total size: 0x24

#endif // _SHSKILLSELECT_H_
