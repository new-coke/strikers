#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/feSoundKeyframeTrigger.h"

/**
 * Offset/Address/Size: 0x0 | 0x802104F4 | size: 0xC
 */
TLSlide* TLComponentInstance::GetActiveSlide()
{
    return m_component->m_pActiveSlide;
}

/**
 * Offset/Address/Size: 0xC | 0x80210500 | size: 0x14
 */
void TLComponentInstance::SetActiveSlide(TLSlide* slide)
{
    m_fCurrentTime = 0.0f;
    m_component->m_pActiveSlide = slide;
}

/**
 * Offset/Address/Size: 0x20 | 0x80210514 | size: 0x2C
 */
void TLComponentInstance::SetActiveSlide(unsigned long hash)
{
    m_fCurrentTime = 0.0f;
    m_component->SetActiveSlide(hash);
}

/**
 * Offset/Address/Size: 0x4C | 0x80210540 | size: 0x2C
 */
void TLComponentInstance::SetActiveSlide(const char* name)
{
    m_fCurrentTime = 0.0f;
    m_component->SetActiveSlide(name);
}

/**
 * Offset/Address/Size: 0x78 | 0x8021056C | size: 0xD8
 */
void TLComponentInstance::Update(float dt)
{
    TLSlide* slide = m_component->m_pActiveSlide;
    if (!slide)
        return;

    // advance local time (0x80)
    m_fCurrentTime += dt;

    const float total = slide->m_start + slide->m_duration;
    if (m_fCurrentTime > total)
    {
        switch (slide->m_uPlayMode)
        {
        case 1: // loop
            m_fCurrentTime = m_fCurrentTime - total;
            break;
        case 0: // clamp
            m_fCurrentTime = total;
            break;
        }
    }

    if (m_bVisible)
    {
        SoundKeyframeTrigger t;
        t.m_slide = slide;
        t.Update(slide->m_time, m_fCurrentTime);
    }

    slide->m_time = m_fCurrentTime;
    slide->Update(dt);
}
