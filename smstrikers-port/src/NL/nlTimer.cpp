#include "NL/nlTimer.h"

#include "PowerPC_EABI_Support/Runtime/runtime.h"

/**
 * Offset/Address/Size: 0x0 | 0x80212F6C | size: 0xB8
 */
bool Timer::Countup(float dt, float thresh)
{
    const u32 addTicks = (u32)(1024.0f * dt + 0.5f);

    const f32 sum = (f32)m_uPackedTime + (f32)addTicks;
    m_uPackedTime = (u32)sum;

    const f32 seconds = (f32)m_uPackedTime / 1024.0f;
    return seconds >= thresh;
}

/**
 * Offset/Address/Size: 0xB8 | 0x80213024 | size: 0xB8
 */
bool Timer::Countdown(float dt, float thresh)
{
    if (m_uPackedTime != 0)
    {
        const u32 subTicks = (u32)(1024.0f * dt + 0.5f);
        if (subTicks > m_uPackedTime)
            m_uPackedTime = 0;
        else
            m_uPackedTime = m_uPackedTime - subTicks;

        const f32 seconds = (f32)m_uPackedTime / 1024.0f;
        return seconds <= thresh;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x170 | 0x802130DC | size: 0x30
 */
f32 Timer::GetSeconds() const
{
    return m_uPackedTime / 1024.0f;
}

/**
 * Offset/Address/Size: 0x1A0 | 0x8021310C | size: 0x3C
 */
void Timer::SetSeconds(float seconds)
{
    m_uPackedTime = (u32)(1024.0f * seconds + 0.5f);
}
