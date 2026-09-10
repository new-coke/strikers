#ifndef _CLOCK_H_
#define _CLOCK_H_

#include "NL/nlDLRing.h"

enum eClockState
{
    CLOCK_OFF = 0,
    CLOCK_ON = 1,
    CLOCK_DONE = 2,
    CLOCK_PAUSED = 3,
};

typedef void (*ClockCallback)(uintptr_t, unsigned long);

class Clock
{
public:
    Clock(float param_1, float param_2, float param_3, unsigned long param_4, ClockCallback callback);
    virtual ~Clock();
    void Start();
    void Reset(float param_1, float param_2, float param_3);
    void Stop();

    /* 0x04 */ float m_fTimeScale;
    /* 0x08 */ float m_fTimer;
    /* 0x0C */ float m_fEndTime;
    /* 0x10 */ ClockCallback m_callback;
    /* 0x14 */ unsigned long m_uActiveStates;
    /* 0x18 */ eClockState m_clockState;

    /* 0x1C */ uintptr_t m_uParam1;
    /* 0x20 */ uintptr_t m_uParam2;

    /* 0x24 */ Clock* m_next;
    /* 0x28 */ Clock* m_prev;
}; // total size: 0x2C

class ClockManager
{
public:
    static void Initialize();
    static void Update(float fDeltaT);

    /* 0x00 */ static Clock* m_inactiveList;
    /* 0x04 */ static Clock* m_activeList;
    /* 0x08 */ static Clock* m_pendingActiveList;
    /* 0x0C */ static bool m_bUpdatingClocks;
};

#endif // _CLOCK_H_
