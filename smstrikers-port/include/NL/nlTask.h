#ifndef _NLTASK_H_
#define _NLTASK_H_

#include "types.h"
#include "NL/nlBasicString.h"

class nlTask
{
public:
    virtual void Run(float) = 0;
    virtual const char* GetName() = 0;
    virtual void StateTransition(unsigned int, unsigned int) { }

    /* 0x04 */ nlTask* m_next;
    /* 0x08 */ nlTask* m_prev;
    /* 0x0C */ u32 nPriority;
    /* 0x10 */ u32 statesActive;
    /* 0x14 */ u32 nPrevTicker;
}; // size 0x18

class nlTaskManager
{
public:
    static void SetTimeDilation(float timeDilation);
    static void SetNextState(unsigned int nextState);
    static void RunAllTasks();
    static void AddTask(nlTask* task, unsigned int priority, unsigned int statesActive);
    static void Startup(unsigned int initialState);
    static void Shutdown();
    static void RemoveTask(nlTask*);
    static void RemoveAllTasks();

    static nlTaskManager* m_pInstance;

    /* 0x00 */ f32 m_TimeDilation;
    /* 0x04 */ nlTask* m_lTaskList; // Head of the task DLRing
    /* 0x08 */ u32 m_CurrState;
    /* 0x0C */ u32 m_PendingState;
    /* 0x10 */ u32 m_PrevState;
    /* 0x14 */ f32 m_fCurrentTimeDelta;
    /* 0x18 */ bool m_Locked;

    nlTaskManager() { }
}; // total size: 0x1C

extern u8 g_DoStackWatermarkTests;
extern u8 g_StackWatermarkFiller;

#endif // _NLTASK_H_
