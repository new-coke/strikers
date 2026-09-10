#include "stadkongasoundproperties.h"

static SoundProperties STADKONGASoundProperties[] = { { "STADSFX_BALL_HITS_POST", "SFXBALL_Post_Metal", 0.9f, 0.0f, 0.5f, 1, 0 } };

STADKONGASoundPropAccessor gSTADKONGASoundPropAccessor;
SoundPropAccessor* gpSTADKONGASoundPropAccessor = &gSTADKONGASoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B44E0 | size: 0x10
 */
void STADKONGASoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADKONGASoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B44C4 | size: 0x1C
 */
bool STADKONGASoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADKONGASoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B44B8 | size: 0xC
 */
const char* STADKONGASoundPropAccessor::GetHTMLFileName() const
{
    return "stadkongasoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B44AC | size: 0xC
 */
const char* STADKONGASoundPropAccessor::GetSoundPropTableName() const
{
    return "STADKONGA";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B44A4 | size: 0x8
 */
u32 STADKONGASoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADKONGASoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B449C | size: 0x8
 */
SoundProperties* STADKONGASoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B443C | size: 0x60
 */
SoundProperties* STADKONGASoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
