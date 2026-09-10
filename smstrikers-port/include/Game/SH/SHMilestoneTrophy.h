#ifndef _SHMILESTONETROPHY_H_
#define _SHMILESTONETROPHY_H_

#include "types.h"

#include "Game/BaseSceneHandler.h"
#include "Game/TrophyTextures.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feButtonComponent.h"

class MilestoneTrophyScene : public BaseSceneHandler
{
public:
    MilestoneTrophyScene();
    ~MilestoneTrophyScene();
    void SceneCreated();
    void Update(float fDeltaT);
    void CreateTrophyScene(eTrophyType trophy, ButtonComponent::ButtonState state, bool isNew);
    void ChangeSlides();

    /* 0x01C */ eTrophyType mTrophy;                       // size 0x4
    /* 0x020 */ bool mCreated;                             // size 0x1
    /* 0x021 */ bool mIsNew;                               // size 0x1
    /* 0x022 */ bool mFirstSlideChange;                    // size 0x1
    /* 0x024 */ unsigned short mTotalBuffer[128];          // size 0x100
    /* 0x124 */ unsigned short mStatBuffer[128];           // size 0x100
    /* 0x224 */ unsigned short mDescriptionBuffer[128];    // size 0x100
    /* 0x324 */ AsyncImage* mAsyncTrophy;                  // size 0x4
    /* 0x328 */ bool mDoBlockLoad;                         // size 0x1
    /* 0x32C */ ButtonComponent::ButtonState mButtonState; // size 0x4
    /* 0x330 */ ButtonComponent mButtons;                  // size 0x24
    /* 0x354 */ ButtonComponent mButtons2;                 // size 0x24
}; // total size: 0x378

#endif // _SHMILESTONETROPHY_H_
