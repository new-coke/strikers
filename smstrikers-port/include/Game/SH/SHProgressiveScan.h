#ifndef _SHPROGRESSIVESCAN_H_
#define _SHPROGRESSIVESCAN_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/tlComponentInstance.h"

class ProgressiveScanScene : public BaseSceneHandler
{
public:
    ProgressiveScanScene(bool doRGB60Instead);
    virtual ~ProgressiveScanScene();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);
    void SwitchMessageImage();

    /* 0x1C */ AsyncImage* mUseProgressiveImage[2];
    /* 0x24 */ AsyncImage* mConfirmationImage;
    /* 0x28 */ bool mHasChoiceBeenMade;
    /* 0x29 */ bool mUseProgressiveMode;
    /* 0x2A */ bool mCanProceed;
    /* 0x2B */ bool mFadingOut;
    /* 0x2C */ bool mDoRGB60Instead;
    /* 0x30 */ TLComponentInstance* mSelectorComponent;
    /* 0x34 */ float mElapsedTime;
}; // total size: 0x38

// class FEFinder<TLImageInstance, 2>
// {
// public:
//     void _Find<TLInstance>(TLInstance*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
//     void _Find<TLSlide>(TLSlide*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
//     void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned
//     long); void Find<FEPresentation>(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher,
//     InlineHasher);
// };

// class FEFinder<TLComponentInstance, 4>
// {
// public:
//     void _Find<TLInstance>(TLInstance*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
//     void _Find<TLSlide>(TLSlide*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
//     void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned
//     long); void Find<FEPresentation>(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher,
//     InlineHasher);
// };

#endif // _SHPROGRESSIVESCAN_H_
