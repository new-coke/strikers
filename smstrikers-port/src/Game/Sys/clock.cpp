#include "Game/Sys/clock.h"
#include "NL/nlTask.h"

Clock* ClockManager::m_inactiveList = NULL;
Clock* ClockManager::m_activeList = NULL;
Clock* ClockManager::m_pendingActiveList = NULL;
bool ClockManager::m_bUpdatingClocks = false;

/**
 * Offset/Address/Size: 0x3D8 | 0x80212D90 | size: 0x4
 */
void ClockManager::Initialize()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x238 | 0x80212BF0 | size: 0x1A0
 */
void ClockManager::Update(float fDeltaT)
{
    ClockCallback callback;
    ClockCallback callback2;

    Clock* pClock;
    Clock* next;

    if (m_activeList != NULL)
    {
        m_bUpdatingClocks = true;
        pClock = m_activeList->m_next;

        for (;;)
        {
            next = pClock->m_next;
            if ((pClock->m_clockState == CLOCK_PAUSED) || !(pClock->m_uActiveStates & nlTaskManager::m_pInstance->m_CurrState))
            {
                if (pClock != m_activeList)
                {
                    pClock = next;
                    continue;
                }
            }
            else
            {
                pClock->m_fTimer = (f32)((fDeltaT * pClock->m_fTimeScale) + pClock->m_fTimer);
                if (pClock->m_fTimeScale >= 0.f)
                {
                    if (pClock->m_fTimer >= pClock->m_fEndTime)
                    {
                        pClock->m_clockState = CLOCK_DONE;
                        pClock->m_fTimer = (f32)pClock->m_fEndTime;
                        callback = pClock->m_callback;
                        if (callback != NULL)
                        {
                            callback(pClock->m_uParam1, pClock->m_uParam2);
                        }
                        nlDLRingRemove<Clock>(&m_activeList, pClock);
                        nlDLRingAddEnd<Clock>(&m_inactiveList, pClock);
                    }
                }
                else
                {
                    if (pClock->m_fTimer <= pClock->m_fEndTime)
                    {
                        pClock->m_clockState = CLOCK_DONE;
                        pClock->m_fTimer = (f32)pClock->m_fEndTime;
                        callback2 = pClock->m_callback;
                        if (callback2 != NULL)
                        {
                            callback2(pClock->m_uParam1, pClock->m_uParam2);
                        }
                        nlDLRingRemove<Clock>(&m_activeList, pClock);
                        nlDLRingAddEnd<Clock>(&m_inactiveList, pClock);
                    }
                }

                if ((pClock != m_activeList) && (m_activeList != NULL))
                {
                    pClock = next;
                    continue;
                }
            }

            break;
        }
        m_bUpdatingClocks = 0;
        nlDLRingAppendRing<Clock>(&m_activeList, m_pendingActiveList);
        m_pendingActiveList = 0;
    }
}

/**
 * Offset/Address/Size: 0x1D8 | 0x80212B90 | size: 0x60
 */
Clock::Clock(float param_1, float param_2, float param_3, unsigned long param_4, ClockCallback callback)
{
    m_fTimeScale = param_3;
    m_fTimer = param_1;
    m_fEndTime = param_2;
    m_clockState = CLOCK_OFF;
    m_callback = callback;
    m_uActiveStates = param_4;
    nlDLRingAddEnd<Clock>(&ClockManager::m_inactiveList, this);
}

/**
 * Offset/Address/Size: 0x14C | 0x80212B04 | size: 0x8C
 */
Clock::~Clock()
{
    if ((nlDLRingRemoveSafely<Clock>(&ClockManager::m_activeList, this) == 0)
        && (nlDLRingRemoveSafely<Clock>(&ClockManager::m_pendingActiveList, this) == 0))
    {
        nlDLRingRemoveSafely<Clock>(&ClockManager::m_inactiveList, this);
    }
}

/**
 * Offset/Address/Size: 0xD8 | 0x80212A90 | size: 0x74
 */
void Clock::Reset(float param_1, float param_2, float param_3)
{
    m_fTimeScale = param_3;
    m_fTimer = param_1;
    m_fEndTime = param_2;

    if (m_clockState == CLOCK_ON)
    {
        if (nlDLRingRemoveSafely<Clock>(&ClockManager::m_activeList, this) == 0)
        {
            nlDLRingRemoveSafely<Clock>(&ClockManager::m_pendingActiveList, this);
        }
        nlDLRingAddEnd<Clock>(&ClockManager::m_inactiveList, this);
    }
    m_clockState = CLOCK_OFF;
}

/**
 * Offset/Address/Size: 0x68 | 0x80212A20 | size: 0x70
 */
void Clock::Start()
{
    if (m_clockState != CLOCK_ON)
    {
        nlDLRingRemove<Clock>(&ClockManager::m_inactiveList, this);
        if (ClockManager::m_bUpdatingClocks != 0)
        {
            nlDLRingAddEnd<Clock>(&ClockManager::m_pendingActiveList, this);
        }
        else
        {
            nlDLRingAddEnd<Clock>(&ClockManager::m_activeList, this);
        }
    }
    m_clockState = CLOCK_ON;
}

/**
 * Offset/Address/Size: 0x0 | 0x802129B8 | size: 0x68
 */
void Clock::Stop()
{
    if (m_clockState == CLOCK_ON)
    {
        if (nlDLRingRemoveSafely<Clock>(&ClockManager::m_activeList, this) == 0)
        {
            nlDLRingRemoveSafely<Clock>(&ClockManager::m_pendingActiveList, this);
        }
        nlDLRingAddEnd<Clock>(&ClockManager::m_inactiveList, this);
    }
    m_clockState = CLOCK_OFF;
}
