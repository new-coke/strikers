#include "stadgrasssoundproperties.h"

static SoundProperties STADGRASSSoundProperties[] = { { "STADSFX_BALL_BOUNCE", "SFXBALL_Bounce_Grass", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "STADSFX_BALL_ROLL", "SFXBALL_Roll_Grass", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "PWRUPSFX_BOMB_HIT", "SFXPWRUP_BOMB_Hit_Grass", 1.0f, 0.0f, 0.7f, 1, 0 },
    { "PWRUPSFX_BOMB_END", "SFXPWRUP_BOMB_End_Grass", 1.0f, 0.0f, 0.7f, 1, 0 },
    { "PWRUPSFX_CHOMP_BOUNCE", "SFXPWRUP_CHOMP_Bounce_Grass", 0.9f, 0.0f, 0.7f, 1, 0 } };

STADGRASSSoundPropAccessor gSTADGRASSSoundPropAccessor;
SoundPropAccessor* gpSTADGRASSSoundPropAccessor = &gSTADGRASSSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B3AB8 | size: 0x10
 */
void STADGRASSSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADGRASSSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B3A9C | size: 0x1C
 */
bool STADGRASSSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADGRASSSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B3A90 | size: 0xC
 */
const char* STADGRASSSoundPropAccessor::GetHTMLFileName() const
{
    return "stadgrasssoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3A84 | size: 0xC
 */
const char* STADGRASSSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADGRASS";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B3A7C | size: 0x8
 */
u32 STADGRASSSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADGRASSSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3A74 | size: 0x8
 */
SoundProperties* STADGRASSSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3A14 | size: 0x60
 */
SoundProperties* STADGRASSSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
