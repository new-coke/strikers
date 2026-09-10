
#include "Game/TestTask.h"

#include "NL/nlDebugFile.h"
#include "NL/nlMemory.h"
#include "NL/nlConfig.h"
#include "types.h"

#include "NL/nlBasicString.h"
#include "NL/nlFormat.h"

typedef BasicString<char, Detail::TempStringAllocator> NLString;

namespace
{
const char* testLog = "test.log";
static const char* smokeTestSuccessOutput = "smoke_test.success";
static const char* frameRateTestSuccessOutput = "frame_rate.success";
} // namespace

/**
 * Offset/Address/Size: 0xB4C | 0x8016D448 | size: 0x44
 */
TestTask::TestTask()
{
    mTestLog = 0;
    mTestTimeOut = 0.0f;
    mRunSmokeTest = false;
    mRunUnitTests = false;
    mFrameRateTestFailure = false;
    mRunFrameRateTest = false;
    mMinimumFrameRate = 10.0f;
}

/**
 * Offset/Address/Size: 0x774 | 0x8016D070 | size: 0x3D8
 */
void TestTask::Initialize()
{
    mTestTimeOut = GetConfigFloat(Config::Global(), "test/time_out_sec ", 10.0f);
    mRunUnitTests = GetConfigBool(Config::Global(), "test/unit_tests", false);
    mRunSmokeTest = GetConfigBool(Config::Global(), "test/smoke", false);
    mRunFrameRateTest = GetConfigBool(Config::Global(), "test/frame_rate", false);
    mMinimumFrameRate = GetConfigFloat(Config::Global(), "test/frame_rate_min", 20.0f);

    if (GetConfigBool(Config::Global(), "test/enable", false))
    {
        mTestLog = nlOpenFileDebug(testLog, false, false);
    }
}

/**
 * Offset/Address/Size: 0x6F0 | 0x8016CFEC | size: 0x84
 */
void TestTask::Run(float dt)
{
    if (!mTestLog)
    {
        return;
    }

    mTestTimeOut = mTestTimeOut - dt;

    RunSmokeTest(dt);
    RunFrameRateTest(dt);

    if (mTestTimeOut < 0.0f)
    {
        nlCloseFileDebug(mTestLog);
        mTestLog = 0;
    }
}

static inline void CreateSuccessFile(const char* filename)
{
    void* file = nlOpenFileDebug(filename, false, false);
    if (file)
    {
        nlWriteLineDebug(file, filename, false);
        nlFlushFileDebug(file);
        nlCloseFileDebug(file);
    }
}

static inline void WriteToTestLog(TestTask* self, const char* msg)
{
    if (self->mTestLog)
    {
        nlWriteLineDebug(self->mTestLog, msg, false);
        nlWriteLineDebug(self->mTestLog, "\n", false);
        nlFlushFileDebug(self->mTestLog);
    }
}

/**
 * Offset/Address/Size: 0x424 | 0x8016CD20 | size: 0x2CC
 */
void TestTask::RunSmokeTest(float dt)
{
    if (mRunSmokeTest)
    {
        if (mTestTimeOut <= 0.0f)
        {
            CreateSuccessFile(smokeTestSuccessOutput);
            NLString fmt("SUCCES: smoke test successful, didn't crash for {0} seconds");
            WriteToTestLog(this, Format(fmt, GetConfigFloat(Config::Global(), "test/time_out_sec ", 10.0f)).c_str());
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8016C8FC | size: 0x424
 */
void TestTask::RunFrameRateTest(float dt)
{
    if (mRunFrameRateTest)
    {
        float frameRateThreshold = 1.0f / mMinimumFrameRate;

        if (dt > frameRateThreshold)
        {
            mFrameRateTestFailure = true;

            NLString fmt("FAILURE: frame rate dropped below {0}, namely {1}");
            float actualFrameRate = 1.0f / dt;
            WriteToTestLog(this, Format(fmt, mMinimumFrameRate, actualFrameRate).c_str());
        }

        if (mTestTimeOut <= 0.0f && !mFrameRateTestFailure)
        {
            CreateSuccessFile(frameRateTestSuccessOutput);
            WriteToTestLog(this, Format(NLString("SUCCES: frame rate test successful, never dropped below {0}"), mMinimumFrameRate).c_str());
        }
    }
}
