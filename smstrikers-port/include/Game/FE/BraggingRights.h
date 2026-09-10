#ifndef _BRAGGINGRIGHTS_H_
#define _BRAGGINGRIGHTS_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feMenu.h"

class FEScrollText;

class BraggingRightsScene : public BaseSceneHandler
{
public:
    BraggingRightsScene();
    ~BraggingRightsScene();
    void SceneCreated();
    void Update(float fDeltaT);

    /* 0x001C */ u16 mBuffer[5][256];
    /* 0x0A1C */ u16 mThisCupBuffer[5][256];
    /* 0x141C */ u16 mRatioBuffer[256];
    /* 0x161C */ s8 mUserPlace;
    /* 0x1620 */ ButtonComponent mButtons;
};

class BraggingRightsOverlay : public BaseSceneHandler
{
public:
    BraggingRightsOverlay();
    ~BraggingRightsOverlay();
    void SceneCreated();
    void IngameSceneCreated();
    void TournamentSceneCreated();
    void Update(float fDeltaT);
    void ChangeTicker(int tickerRow);

    /* 0x001C */ u16 mBuffer[256];
    /* 0x021C */ MenuList<TLComponentInstance> mMenuItems;
    /* 0x0430 */ FEScrollText* mTicker;
    /* 0x0434 */ int mHighestStats[5];
    /* 0x0448 */ int mAwardWinners[5];
    /* 0x045C */ u8 mIsTournamentScene;
    /* 0x0460 */ ButtonComponent mButtons;
};

#endif // _BRAGGINGRIGHTS_H_
