#ifndef _SHCUPCHOOSECAPTAIN_H_
#define _SHCUPCHOOSECAPTAIN_H_

#include "Game/BaseSceneHandler.h"
#include "Game/DB/Cup.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feCaptainGridComponent.h"
#include "Game/FE/feScrollText.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feSidekickGridComponent.h"
#include "Game/FE/tlComponentInstance.h"

class CupChooseCaptainSceneV2 : public BaseSceneHandler
{
public:
    enum eCupCaptainState
    {
        CUP_STATE_CAPTAIN = 0,
        CUP_STATE_SK = 1,
        CUP_STATE_READY = 2,
    };

    CupChooseCaptainSceneV2(bool isSuperCup);
    virtual ~CupChooseCaptainSceneV2();
    virtual void Update(float fDeltaT);
    virtual void SceneCreated();
    void UpdateCaptainName();
    void UpdateSKName();
    void ChangeState(CupChooseCaptainSceneV2::eCupCaptainState from, CupChooseCaptainSceneV2::eCupCaptainState to);
    void CreateLineup();
    void StartSidekickMiniHead(eSidekickID sidekickId);
    void UpdateImages();

    /* 0x1C */ TLComponentInstance* mComponents[5];                                      // size 0x14
    /* 0x30 */ TLComponentInstance* mPressAComponent;                                    // size 0x4
    /* 0x34 */ TLComponentInstance* mSidekickMiniHead;                                   // size 0x4
    /* 0x38 */ eTeamID mCurrentCaptain;                                                  // size 0x4
    /* 0x3C */ eSidekickID mCurrentSK;                                                   // size 0x4
    /* 0x40 */ eCupCaptainState mState;                                                  // size 0x4
    /* 0x44 */ AsyncImage* mCaptainImageMain;                                            // size 0x4
    /* 0x48 */ AsyncImage* mCaptainImageBG;                                              // size 0x4
    /* 0x4C */ AsyncImage* mCaptainImageFlash;                                           // size 0x4
    /* 0x50 */ AsyncImage* mSKImageMain;                                                 // size 0x4
    /* 0x54 */ AsyncImage* mSKImageBG;                                                   // size 0x4
    /* 0x58 */ ICaptainGridComponent* mCaptainGrid;                                      // size 0x4
    /* 0x5C */ ISidekickGridComponent* mSKGrid;                                          // size 0x4
    /* 0x60 */ float mAnimationDelay;                                                    // size 0x4
    /* 0x64 */ float mSoundDelay;                                                        // size 0x4
    /* 0x68 */ float mRemainingSoundDelay;                                               // size 0x4
    /* 0x6C */ FEScrollText* mTicker;                                                    // size 0x4
    /* 0x70 */ bool mIsSuperCup;                                                         // size 0x1
    /* 0x74 */ BasicString<unsigned short, Detail::TempStringAllocator> mCupStartString; // size 0x4
    /* 0x78 */ int mNumImagesLoaded;                                                     // size 0x4
    /* 0x7C */ ButtonComponent mButtons;                                                 // size 0x24
}; // total size: 0xA0

// class FEFinder<TLImageInstance, 2>
// {
// public:
// };

// class FEFinder<TLComponentInstance, 4>
// {
// public:
// };

// class FEFinder<TLTextInstance, 3>
// {
// public:
// };

#endif // _SHCUPCHOOSECAPTAIN_H_
