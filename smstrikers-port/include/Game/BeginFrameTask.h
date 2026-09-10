#ifndef _BEGINFRAMETASK_H_
#define _BEGINFRAMETASK_H_

#include "NL/nlTask.h"
#include "NL/gl/glModel.h"

void SetupMatrices();

extern bool g_bFrameSmiler;
extern bool g_bFrameStatsOnScreen;

enum eModelSkinMethod
{
    eModelSkin_Rigid = 0,
    eModelSkin_Blend = 1,
    eModelSkin_Both = 2,
    eModelSkin_Num = 3,
};

class BeginFrameTask : public nlTask
{
public:
    virtual const char* GetName() { return "Begin Frame"; };
    virtual void Run(float dt);

    static bool s_FramerateLocked;
    static eModelSkinMethod s_GameplaySkin;
    static eModelSkinMethod s_ReplaySkin;
};

#endif // _BEGINFRAMETASK_H_
