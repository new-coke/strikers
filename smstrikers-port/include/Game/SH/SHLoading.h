#ifndef _SHLOADING_H_
#define _SHLOADING_H_

#include "types.h"

#include "Game/BaseSceneHandler.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/Team.h"

class BundleFile;

class SuperLoadingScene : public BaseSceneHandler
{
public:
    enum TransitionType
    {
        TT_INVALID = -1,
        TT_IN = 0,
        TT_OUT = 1,
    };

    SuperLoadingScene();
    virtual ~SuperLoadingScene();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);
    void DisplayCupInfo();
    bool TexturesAreValid() const;
    void BuildPlayerStrings(TLTextInstance* pTextInst, int side, bool checkConnected);
    void BuildAndLoadPortraits(eTeamID homecaptain, eTeamID awaycaptain);

    enum TextureType
    {
        TT_MAIN = 0,
        TT_NUM_TYPES = 1,
    };
    static unsigned long LoadImage(BundleFile& bundlefile, eTeamID captain, int playingside, TextureType texturetype);

    /* 0x01C */ TransitionType mType;                   // size 0x4
    /* 0x020 */ float mElapsedTime;                     // size 0x4
    /* 0x024 */ bool mAlreadySwappedTextures;           // size 0x1
    /* 0x026 */ unsigned short mStatsBuffers[4][64];    // size 0x200
    /* 0x228 */ TLImageInstance* mImageInstances[2][1]; // size 0x8
    /* 0x230 */ unsigned long mTextureHandles[2][1];    // size 0x8
    /* 0x238 */ unsigned short mPlayerStrings[2][255];  // size 0x3FC
}; // total size: 0x634

#endif // _SHLOADING_H_
