#include "stadbowsersoundproperties.h"

static SoundProperties STADBOWSERSoundProperties[] = { { "STADSFX_BALL_HITS_POST", "SFXBALL_Post_Metal", 0.9f, 0.0f, 0.5f, 1, 0 } };

STADBOWSERSoundPropAccessor gSTADBOWSERSoundPropAccessor;
SoundPropAccessor* gpSTADBOWSERSoundPropAccessor = &gSTADBOWSERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B42D8 | size: 0x10
 */
void STADBOWSERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADBOWSERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B42BC | size: 0x1C
 */
bool STADBOWSERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADBOWSERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B42B0 | size: 0xC
 */
const char* STADBOWSERSoundPropAccessor::GetHTMLFileName() const
{
    return "stadbowsersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B42A4 | size: 0xC
 */
const char* STADBOWSERSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADBOWSER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B429C | size: 0x8
 */
u32 STADBOWSERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADBOWSERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B4294 | size: 0x8
 */
SoundProperties* STADBOWSERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B4234 | size: 0x60
 */
SoundProperties* STADBOWSERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
