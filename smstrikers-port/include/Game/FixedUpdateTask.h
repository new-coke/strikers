#ifndef _FIXEDUPDATETASK_H_
#define _FIXEDUPDATETASK_H_

#include "NL/nlTask.h"

class FixedUpdateTask : public nlTask
{
public:
    virtual void Run(float dt);
    virtual const char* GetName();

    static void DecrementFrameLock(float fDeltaT);
    static float GetPhysicsUpdateTick();

    static void CallFixedUpdateTasks();
    static void PostPhysicsAITask(float fDeltaT);
    static void PrePhysicsAITask(float fDeltaT);
    static void AIUpdateTask(float fDeltaT);

    FixedUpdateTask();

    static float mAccumulatedDeltaT;
    static float mSimulationTime;
    static float mfFrameLockTime;
    static float mTimeScale;
};

extern bool g_bRunSimAndRenderInLockStep;

#endif // _FIXEDUPDATETASK_H_
