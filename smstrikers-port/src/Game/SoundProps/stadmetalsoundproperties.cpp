#include "stadmetalsoundproperties.h"

static SoundProperties STADMETALSoundProperties[] = { { "STADSFX_BALL_BOUNCE", "SFXBALL_Bounce_Metal", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "STADSFX_BALL_ROLL", "SFXBALL_Roll_Metal", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "PWRUPSFX_BOMB_HIT", "SFXPWRUP_BOMB_Hit_Metal", 1.0f, 0.0f, 0.7f, 1, 0 },
    { "PWRUPSFX_BOMB_END", "SFXPWRUP_BOMB_End_Metal", 1.0f, 0.0f, 0.7f, 1, 0 },
    { "PWRUPSFX_CHOMP_BOUNCE", "SFXPWRUP_CHOMP_Bounce_Metal", 0.9f, 0.0f, 0.7f, 1, 0 } };

STADMETALSoundPropAccessor gSTADMETALSoundPropAccessor;
SoundPropAccessor* gpSTADMETALSoundPropAccessor = &gSTADMETALSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B3BBC | size: 0x10
 */
void STADMETALSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADMETALSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B3BA0 | size: 0x1C
 */
bool STADMETALSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADMETALSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B3B94 | size: 0xC
 */
const char* STADMETALSoundPropAccessor::GetHTMLFileName() const
{
    return "stadmetalsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3B88 | size: 0xC
 */
const char* STADMETALSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADMETAL";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B3B80 | size: 0x8
 */
u32 STADMETALSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADMETALSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3B78 | size: 0x8
 */
SoundProperties* STADMETALSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3B18 | size: 0x60
 */
SoundProperties* STADMETALSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
