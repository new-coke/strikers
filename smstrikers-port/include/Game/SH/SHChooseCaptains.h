#ifndef _SHCHOOSECAPTAINS_H_
#define _SHCHOOSECAPTAINS_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feCaptainComponent.h"
#include "Game/FE/feChooseSideComponent.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feScrollText.h"
#include "Game/FE/tlComponentInstance.h"
#include "NL/gl/glStruct.h"

class ChooseCaptainsSceneV2 : public BaseSceneHandler
{
public:
    enum SceneType
    {
        ST_CHOOSE_CAPTAINS = 0,
        ST_CHOOSE_SIDES = 1
    };

    ChooseCaptainsSceneV2(ChooseCaptainsSceneV2::SceneType scenetype);
    ~ChooseCaptainsSceneV2();
    void SceneCreated();
    void ChangeSceneType(ChooseCaptainsSceneV2::SceneType newtype);
    void ResetForCHOOSECAPTAINS();
    void ResetForCHOOSESIDES();
    void Update(float fDeltaT);
    void BindChooseSideInstances();
    void CreateTicker();
    void DestroyTicker();

    /* 0x01C */ IChooseCaptain mChooseCaptain; // size 0xCC
    /* 0x0E8 */ IChooseSide mChooseSide;       // size 0xA0 (G4QE01), 0xB0 (G4QJ01)
    /* 0x188 (G4QE01), 0x198 (G4QJ01) */ TLComponentInstance* mChooseSideComponent;
    /* 0x18C (G4QE01), 0x19C (G4QJ01) */ ButtonComponent mButtons;
    /* 0x1B0 (G4QE01), 0x1C0 (G4QJ01) */ SceneType mSceneType;
    /* 0x1B4 (G4QE01), 0x1C4 (G4QJ01) */ SceneType mDesiredSceneType;
    /* 0x1B8 (G4QE01), 0x1C8 (G4QJ01) */ FEScrollText* mTicker;
    /* 0x1BC (G4QE01), 0x1CC (G4QJ01) */ int mMoveForwardFrameDelay;
}; // total size: 0x1C0 (G4QE01), 0x1D0 (G4QJ01)

#endif // _SHCHOOSECAPTAINS_H_
