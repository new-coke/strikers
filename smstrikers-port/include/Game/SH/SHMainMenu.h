#ifndef _SHMAINMENU_H_
#define _SHMAINMENU_H_

#include "types.h"

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/feScrollText.h"
#include "Game/FE/tlComponentInstance.h"
#include "NL/nlColour.h"

class SHMainMenu : public BaseSceneHandler
{
public:
    SHMainMenu();
    virtual ~SHMainMenu();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);
    void OpenItem(TLComponentInstance* compinstance);
    void CloseItem(TLComponentInstance* compinstance);

    static bool mSnapMenuIntoPosition;
    static int mLastMenuItem;
    static const int NUM_ITEMS = 7;

    /* 0x01C */ FEScrollText* m_itemDescriptions;
    /* 0x020 */ nlColour mHighlightColour;
    /* 0x024 */ MenuList<TLComponentInstance> mMenuItems;
    /* 0x238 */ ButtonComponent mButtons;
}; // total size: 0x25C

#endif // _SHMAINMENU_H_
