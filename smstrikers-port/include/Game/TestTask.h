#ifndef _TESTTASK_H_
#define _TESTTASK_H_

#include "NL/nlTask.h"
#include "NL/nlMemory.h"

class TestTask : public nlTask
{
public:
    TestTask();
    virtual void Run(float dt);
    virtual const char* GetName() { return "Test"; };

    void Initialize();

    void RunSmokeTest(float dt);
    void RunFrameRateTest(float dt);

    /* 0x18 */ void* mTestLog;
    /* 0x1C */ f32 mTestTimeOut;
    /* 0x20 */ bool mRunSmokeTest;
    /* 0x21 */ bool mRunUnitTests;
    /* 0x22 */ bool mFrameRateTestFailure;
    /* 0x23 */ bool mRunFrameRateTest;
    /* 0x24 */ f32 mMinimumFrameRate;
}; // total size: 0x28

#endif // _TESTTASK_H_
