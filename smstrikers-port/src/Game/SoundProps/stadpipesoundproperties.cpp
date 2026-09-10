#include "stadpipesoundproperties.h"

static SoundProperties STADPIPESoundProperties[] = { { "STADSFX_BALL_HITS_POST", "SFXBALL_Post_Metal", 0.9f, 0.0f, 0.5f, 1, 0 } };

STADPIPESoundPropAccessor gSTADPIPESoundPropAccessor;
SoundPropAccessor* gpSTADPIPESoundPropAccessor = &gSTADPIPESoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B3FCC | size: 0x10
 */
void STADPIPESoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADPIPESoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B3FB0 | size: 0x1C
 */
bool STADPIPESoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADPIPESoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B3FA4 | size: 0xC
 */
const char* STADPIPESoundPropAccessor::GetHTMLFileName() const
{
    return "stadpipesoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3F98 | size: 0xC
 */
const char* STADPIPESoundPropAccessor::GetSoundPropTableName() const
{
    return "STADPIPE";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B3F90 | size: 0x8
 */
u32 STADPIPESoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADPIPESoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3F88 | size: 0x8
 */
SoundProperties* STADPIPESoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3F28 | size: 0x60
 */
SoundProperties* STADPIPESoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
