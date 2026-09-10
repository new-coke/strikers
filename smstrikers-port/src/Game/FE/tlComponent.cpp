#include "Game/FE/tlComponent.h"
#include "Game/FE/tlSlide.h"

#include "Game/FE/feFinder.h"
#include "NL/nlString.h"

/**
 * Offset/Address/Size: 0x0 | 0x8020FB68 | size: 0x34
 */
void TLComponent::SetActiveSlide(unsigned long hash)
{
    m_pActiveSlide = FindItemByHashID<TLSlide>(pChildren, hash);
}

/**
 * Offset/Address/Size: 0x34 | 0x8020FB9C | size: 0x44
 */
void TLComponent::SetActiveSlide(const char* name)
{
    unsigned long hash = nlStringLowerHash(name);
    m_pActiveSlide = FindItemByHashID<TLSlide>(pChildren, hash);
}
