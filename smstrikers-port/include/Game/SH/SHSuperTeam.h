#ifndef _SHSUPERTEAM_H_
#define _SHSUPERTEAM_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feButtonComponent.h"

class SuperTeamScene : public BaseSceneHandler
{
public:
    virtual ~SuperTeamScene();
    virtual void Update(float fDeltaT);
    virtual void SceneCreated();

    /* 0x1C */ ButtonComponent mButtons;
}; // total size: 0x40

// class FEFinder<TLComponentInstance, 4>
// {
// public:
// };

#endif // _SHSUPERTEAM_H_
