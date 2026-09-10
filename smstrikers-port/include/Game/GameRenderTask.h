#ifndef _GAMERENDERTASK_H_
#define _GAMERENDERTASK_H_

#include "NL/nlTask.h"

class GameRenderTask : public nlTask
{
public:
    virtual void Run(float fDeltaT);
    virtual const char* GetName() { return "Game Render"; }

private:
    static void SetupConstants();
};

extern bool g_bRenderWorld;

#endif // _GAMERENDERTASK_H_
