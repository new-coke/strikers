#ifndef _SHOPTIONS_H_
#define _SHOPTIONS_H_

#include "Game/BaseSceneHandler.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/GameSceneManager.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/feOptionsSubMenus.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/FE/tlComponentInstance.h"
#include "NL/nlSingleton.h"

enum eMenuState
{
    MS_INVALID = -1,
    MS_MAIN = 0,
    MS_AUDIO = 1,
    MS_VISUAL = 2,
    MS_GAMEPLAY = 3,
    MS_CHEATS = 4,
    MS_SAVE_LOAD = 5,
    MS_NUMMENUSTATES = 6,
};

enum ePopupResult
{
    PR_DO_NOTHING = 0,
    PR_APPLY_CHANGES = 1,
    PR_REVERT_CHANGES = 2,
    PR_APPLY_DELAYED_AUDIO_CHANGES = 3,
};

class OptionsScene : public BaseSceneHandler
{
public:
    OptionsScene();
    ~OptionsScene();
    void SceneCreated();
    void Update(float fDeltaT);
    void UpdateForMain(float fDeltaT);
    void UpdateForSubOptionMenus(float fDeltaT);
    void ChangeMenuState(eMenuState newState);
    void OpenItem(TLComponentInstance* compinstance);
    void CloseItem(TLComponentInstance* compinstance);
    bool AllCheatsLocked();

    static s32 mLastSelectedIndex;
    static u32 mUserInfoCRC;

    /* 0x01C */ OptionsSubMenu* m_subMenu;                // size 0x4
    /* 0x020 */ eMenuState m_curMenuState;                // size 0x4
    /* 0x024 */ ButtonComponent mButtons;                 // size 0x24
    /* 0x048 */ MenuList<TLComponentInstance> mMenuItems; // size 0x214
    /* 0x25C */ ePopupResult mPopupResult;                // size 0x4
}; // total size: 0x260

#endif // _SHOPTIONS_H_
