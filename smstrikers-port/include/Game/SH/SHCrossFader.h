#ifndef _SHCROSSFADER_H_
#define _SHCROSSFADER_H_

#include "types.h"

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feAsyncImage.h"

enum FadeState
{
    FS_INVALID = -1,
    FS_FADE_IN_INIT = 0,
    FS_FADE_IN = 1,
    FS_WAIT = 2,
    FS_CROSS_FADE = 3,
    FS_FADE_TO_BLACK = 4,
};

class CrossFaderScene : public BaseSceneHandler
{
public:
    CrossFaderScene();
    ~CrossFaderScene();
    void SceneCreated();
    void Update(float fDeltaT);

    /* 0x1C */ int mNumImages;
    /* 0x20 */ int mCurrentImage;
    /* 0x24 */ TLImageInstance** mImageInstances;
    /* 0x28 */ TLImageInstance* mCurrentImageInstance;
    /* 0x2C */ float mTimer;
    /* 0x30 */ float mAlpha;
    /* 0x34 */ FadeState mFadeState;
    /* 0x38 */ float mFadeToBlackTimer;
}; // total size: 0x3C

// class FEFinder<TLImageInstance, 2>
// {
// public:
//     void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned
//     long); void Find<FEPresentation>(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher,
// };

// class FEResourceManager
// {
// public:
// };

#endif // _SHCROSSFADER_H_
