#ifndef _SHPAUSE_H_
#define _SHPAUSE_H_

#include "types.h"

#include "Game/FE/feInput.h"
#include "Game/FE/feMenu.h"
#include "Game/BaseSceneHandler.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/feButtonComponent.h"

class PauseMenuScene : public BaseSceneHandler
{
public:
    enum ScreenContext
    {
        SC_UNKNOWN = -1,
        SC_REGULAR_PAUSE = 0,
        SC_101_PAUSE = 1,
        SC_LAST = 2,
    };

    enum TransitionType
    {
        TT_INVALID = -1,
        TT_IN = 0,
        TT_OUT = 1,
    };

    PauseMenuScene(PauseMenuScene::ScreenContext context);
    ~PauseMenuScene();
    void OnSelectRESUME(TLComponentInstance* instance);
    void OnSelectQUIT(TLComponentInstance* instance);
    void OnSelectCHOOSESIDES(TLComponentInstance* instance);
    void OnSelectAUDIOOPTIONS(TLComponentInstance* instance);
    void OnSelectVISUALOPTIONS(TLComponentInstance* instance);
    void OnSelectSTATISTICS(TLComponentInstance* instance);
    void OnSelectBRAGGING(TLComponentInstance* instance);
    void OnSelectPopupNOFORFEIT();
    void OnSelectPopupYESFORFEIT();
    void OnSelectLESSONS(TLComponentInstance* instance);
    void StartDelayedQuit();
    void SceneCreated();
    void Update(float fDeltaT);
    void SetupForNewGame();
    void TransitionOut(PauseMenuScene::TransitionType newtype);
    void OpenItem(TLComponentInstance* instance);
    void CloseItem(TLComponentInstance* instance);
    void SetQuitTextForJapanese(TLComponentInstance* instance);

    /* 0x01C */ ScreenContext mContext;
    /* 0x020 */ bool mGameIsOver;
    /* 0x024 */ float mQuitDelay;
    /* 0x028 */ eFEINPUT_PAD mQuittingController;
    /* 0x02C */ MenuList<TLComponentInstance> mMenuItems;
    /* 0x240 */ TransitionType mTransitionTo;
    /* 0x244 */ bool mIsInTransition;
    /* 0x245 */ bool mStartAnimAtEnd;
    /* 0x248 */ ButtonComponent mButtons;
    /* 0x26C */ ButtonComponent mButtons2;

    static eFEINPUT_PAD mControllingInput;
    static float mDelayBeforeUnpause;
    static u32 mLastTaskManagerState;
    static s32 mLastSelectedIndex;
}; // total size: 0x290

#endif // _SHPAUSE_H_
