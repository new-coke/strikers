#include "stadwoodsoundproperties.h"

static SoundProperties STADWOODSoundProperties[] = { { "STADSFX_BALL_BOUNCE", "SFXBALL_Bounce_Wood", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "STADSFX_BALL_ROLL", "SFXBALL_Roll_Wood", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "PWRUPSFX_BOMB_HIT", "SFXPWRUP_BOMB_Hit_Wood", 1.0f, 0.0f, 0.7f, 1, 0 },
    { "PWRUPSFX_BOMB_END", "SFXPWRUP_BOMB_End_Wood", 1.0f, 0.0f, 0.7f, 1, 0 },
    { "PWRUPSFX_CHOMP_BOUNCE", "SFXPWRUP_CHOMP_Bounce_Wood", 0.9f, 0.0f, 0.7f, 1, 0 } };

STADWOODSoundPropAccessor gSTADWOODSoundPropAccessor;
SoundPropAccessor* gpSTADWOODSoundPropAccessor = &gSTADWOODSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B3EC8 | size: 0x10
 */
void STADWOODSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADWOODSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B3EAC | size: 0x1C
 */
bool STADWOODSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADWOODSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B3EA0 | size: 0xC
 */
const char* STADWOODSoundPropAccessor::GetHTMLFileName() const
{
    return "stadwoodsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3E94 | size: 0xC
 */
const char* STADWOODSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADWOOD";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B3E8C | size: 0x8
 */
u32 STADWOODSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADWOODSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3E84 | size: 0x8
 */
SoundProperties* STADWOODSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3E24 | size: 0x60
 */
SoundProperties* STADWOODSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
