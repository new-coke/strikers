#ifndef _SHTOURNSETPARAMS_H_
#define _SHTOURNSETPARAMS_H_

#include "types.h"
#include "NL/nlString.h"
#include "Game/BaseSceneHandler.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/feSlideMenu.h"

class TournSetParamsScene : public BaseSceneHandler
{
public:
    TournSetParamsScene();
    virtual ~TournSetParamsScene();
    virtual void Update(float fDeltaT);
    virtual void SceneCreated();

    void BuildSubMenuList(int menuitem, TLComponentInstance* compinstance, bool wraps, int startindex);
    void SetInitialParams(bool isLeagueMode, int numTeams, int numGames);
    void ApplyMenuDefaults();
    void InitializeMenu();
    void Proceed();

    bool ColourAllText(const nlColour& colour, int menuitem);
    void ColourAllText(TLComponentInstance& component, const nlColour& colour);

    /* 0x01C */ MenuList<TLComponentInstance> mMenuItems; // size 0x214
    /* 0x230 */ SlideMenuList* mSlideMenuLists[3];
    /* 0x23C */ bool m_isLeagueMode;
    /* 0x23D */ char pad23D[3];
    /* 0x240 */ s32 m_numTeams;
    /* 0x244 */ s32 m_numGames;
    /* 0x248 */ ButtonComponent mButtons;
}; // total size: 0x26C

#endif // _SHTOURNSETPARAMS_H_
