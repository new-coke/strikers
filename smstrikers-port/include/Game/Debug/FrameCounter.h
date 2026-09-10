#ifndef _FRAMECOUNTER_H_
#define _FRAMECOUNTER_H_

#include "NL/nlMath.h"
#include "NL/nlColour.h"

class FrameCounter
{
public:
    static int NUM_FRAMES_TO_AVERAGE_OVER;

    FrameCounter(const char* first, const char* second);
    void StartTimer(int timerNum);
    unsigned int FinishCurrTimer();
    void ResetFrameTimers();
    void FinishTiming();
    void DisplayFrameRate();
    void DisplaySlowFrameRatio();
    void DisplayContinuousFrameRate();
    void DisplayFrameTicker();
    void WriteFrameRateStatsToFile(const char* fileName);
    void DisplayFrameSmiler();

    /* 0x000 */ const char* m_FirstName;
    /* 0x004 */ const char* m_SecondName;
    /* 0x008 */ unsigned int m_StartTick;
    /* 0x00C */ int m_CurrTimerNum;
    /* 0x010 */ float m_CurrTimer[2];
    /* 0x018 */ float m_CurrFrame[2];
    /* 0x020 */ float m_LastFrame[2];
    /* 0x028 */ s32 m_NumSlowFrames;
    /* 0x02C */ f32 m_SlowFrameRatio;
    /* 0x030 */ u32 m_Counter;
    /* 0x034 */ s32 m_NextHistoryPos;

    /* 0x038 */ float m_FrameHistory[640];
    /* 0xA38 */ int m_ContinuousFrameHistoryIndex;
    /* 0xA3C */ float m_ContinuousFrameHistory[2][200];
}; // total size: 0x107C

extern FrameCounter g_FrameCounter;

#endif // _FRAMECOUNTER_H_
