#include "Game/FE/fePresentation.h"

#include "NL/nlString.h"

#include "Game/FE/feSoundKeyframeTrigger.h"
#include "Game/FE/feFinder.h"

/**
 * Offset/Address/Size: 0x0 | 0x80210644 | size: 0xD0
 */
void FEPresentation::Update(float deltaTime)
{
    if (m_currentSlide != NULL)
    {
        m_fadeDuration += deltaTime;
        f32 end = m_currentSlide->m_start + m_currentSlide->m_duration;
        if (m_fadeDuration > end)
        {
            switch (m_currentSlide->m_uPlayMode)
            {
            case TLPM_LOOPING:
                m_fadeDuration = m_fadeDuration - end;
                break;
            case TLPM_STOP_AT_END:
                m_fadeDuration = end;
                break;
            }
        }
        SoundKeyframeTrigger trigger;
        trigger.m_slide = m_currentSlide;
        trigger.Update(m_currentSlide->m_time, m_fadeDuration);
        m_currentSlide->m_time = m_fadeDuration;
        m_currentSlide->Update(deltaTime);
    }
}

/**
 * Offset/Address/Size: 0xD0 | 0x80210714 | size: 0x10
 */
void FEPresentation::SetActiveSlide(TLSlide* slide)
{
    m_fadeDuration = 0.0f;
    m_currentSlide = slide;
}

/**
 * Offset/Address/Size: 0xE0 | 0x80210724 | size: 0x3C
 */
void FEPresentation::SetActiveSlide(unsigned long hash)
{
    TLSlide* slide = FindItemByHashID<TLSlide>(m_slides, hash);
    m_fadeDuration = 0.0f;
    m_currentSlide = slide;
}

/**
 * Offset/Address/Size: 0x11C | 0x80210760 | size: 0x4C
 */
void FEPresentation::SetActiveSlide(const char* slideName)
{
    u32 hash = nlStringLowerHash(slideName);
    TLSlide* slide = FindItemByHashID<TLSlide>(m_slides, hash);
    m_fadeDuration = 0.0f;
    m_currentSlide = slide;
}
