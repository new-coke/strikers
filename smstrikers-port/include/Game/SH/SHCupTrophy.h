#ifndef _SHCUPTROPHY_H_
#define _SHCUPTROPHY_H_

#include "types.h"

#include "Game/BaseSceneHandler.h"
#include "Game/TrophyTextures.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feButtonComponent.h"

struct Spoil;

class CupTrophyScene : public BaseSceneHandler
{
public:
    CupTrophyScene();
    ~CupTrophyScene();
    void SceneCreated();
    void HandleUnlockedTriggers();
    void Update(float fDeltaT);
    void CreateTrophyScene(eTrophyType trophy, ButtonComponent::ButtonState buttonState, bool isNew);
    void SetWinRecord(Spoil& spoil);
    void SetLossRecord(Spoil& spoil);
    void SetHistory(Spoil& spoil);
    void ChangeSlides();

    /* 0x1C, */ eTrophyType mTrophy;                       // size 0x4
    /* 0x20, */ eTrophyType mFirstTrophy;                  // size 0x4
    /* 0x24, */ eTrophyType mLastTrophy;                   // size 0x4
    /* 0x28, */ bool mCreated;                             // size 0x1
    /* 0x29, */ bool mIsNew;                               // size 0x1
    /* 0x2A, */ bool mFirstSlideChange;                    // size 0x1
    /* 0x02C */ unsigned short mFirstWinBuffer[128];       // size 0x100
    /* 0x12C */ unsigned short mHistoryBuffer[128];        // size 0x100
    /* 0x22C */ unsigned short mUnknownBuffer[128];        // size 0x100
    /* 0x32C */ unsigned short mRecordBuffer[3][128];      // size 0x300
    /* 0x62C */ unsigned short mWonBuffer[128];            // size 0x100
    /* 0x72C */ int mScrollOffset;                         // size 0x4
    /* 0x730 */ int mRow;                                  // size 0x4
    /* 0x734 */ AsyncImage* mAsyncTrophy;                  // size 0x4
    /* 0x738 */ bool mDoBlockLoad;                         // size 0x1
    /* 0x73C */ ButtonComponent::ButtonState mButtonState; // size 0x4
    /* 0x740 */ ButtonComponent mButtons;                  // size 0x24
    /* 0x764 */ ButtonComponent mButtons2;                 // size 0x24
}; // total size: 0x788

#endif // _SHCUPTROPHY_H_
