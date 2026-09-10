#ifndef _FECHOOSESIDECOMPONENT_H_
#define _FECHOOSESIDECOMPONENT_H_

#include "Game/FE/feCaptainComponent.h"
#include "Game/FE/feInput.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/feTweener.h"

enum Context
{
    CONTEXT_INVALID = -1,
    CONTEXT_FE = 0,
    CONTEXT_PAUSE = 1,
};

enum ControllerPos
{
    CPOS_HOME = 0,
    CPOS_AWAY = 1,
    CPOS_NEUTRAL = 2,
    CPOS_NUM = 3,
};

class IChooseSide
{
public:
    IChooseSide();
    ~IChooseSide();
    UpdateResult Update(float fDeltaT, eFEINPUT_PAD* padresult, int disabledSide);
    UpdateResult UpdateForFE(float fDeltaT, eFEINPUT_PAD* padresult);
    UpdateResult UpdateForPause(float fDeltaT, eFEINPUT_PAD* padresult);
    void CheckControllers(int disabledSide);
    void ResetAndPositionControllers(bool reset);
    void SetReady(int padindex, bool isready);
    void UpdatePressAText();
    void PositionController(int padindex, bool usetween, bool setvisibilities);
    bool AllPlayersReady() const;
    bool AllPluggedInAreReady() const;
    int MoveSideLeft(int padindex);
    int MoveSideRight(int padindex);
    void MoveSideNone(int padindex);
    bool AtLeastOnePlayerReady() const;
    bool AllControllersAreCentred() const;
    static void TweenSetPosCallback(void* obj, const float* value);
    void SaveChanges();
    void SetArrowVisible(int instanceid, bool leftvisible, bool rightvisible);
    void MakeArrowsFollowController();

    /* 0x00 */ int mPlayingSides[4]; // size 0x10
    /* 0x10 */ bool mPlayerReady[4]; // size 0x4
    /* 0x14 */ TLInstance* mInstanceTable[21]; // size 0x54
    /* 0x68 */ float mControllerDestPos[3];    // size 0xC
    /* 0x74 */ FETweenManager mTweenManager;   // size 0x38
    /* 0xAC */ Context mContext;               // size 0x4
}; // total size: 0xA0 (G4QE01), 0xB0 (G4QJ01)

#endif // _FECHOOSESIDECOMPONENT_H_
