#include "stadrubbersoundproperties.h"

static SoundProperties STADRUBBERSoundProperties[]
    = { { "STADSFX_BALL_BOUNCE", "SFXBALL_Bounce_Rubber", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_BALL_ROLL", "SFXBALL_Roll_Rubber", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BOMB_HIT", "SFXPWRUP_BOMB_Hit_Rubber", 1.0f, 0.0f, 0.7f, 1, 0 },
          { "PWRUPSFX_BOMB_END", "SFXPWRUP_BOMB_End_Rubber", 1.0f, 0.0f, 0.7f, 1, 0 },
          { "PWRUPSFX_CHOMP_BOUNCE", "SFXPWRUP_CHOMP_Bounce_Rubber", 0.9f, 0.0f, 0.7f, 1, 0 } };

STADRUBBERSoundPropAccessor gSTADRUBBERSoundPropAccessor;
SoundPropAccessor* gpSTADRUBBERSoundPropAccessor = &gSTADRUBBERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B3DC4 | size: 0x10
 */
void STADRUBBERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADRUBBERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B3DA8 | size: 0x1C
 */
bool STADRUBBERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADRUBBERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B3D9C | size: 0xC
 */
const char* STADRUBBERSoundPropAccessor::GetHTMLFileName() const
{
    return "stadrubbersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3D90 | size: 0xC
 */
const char* STADRUBBERSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADRUBBER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B3D88 | size: 0x8
 */
u32 STADRUBBERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADRUBBERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3D80 | size: 0x8
 */
SoundProperties* STADRUBBERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3D20 | size: 0x60
 */
SoundProperties* STADRUBBERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
