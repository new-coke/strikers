#include "powerupsoundproperties.h"

static SoundProperties PWRUPSoundProperties[]
    = { { "PWRUPSFX_SHELL_GEN_MOVE", "SFXPWRUP_SHELL_GEN_Move", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_GEN_SHATTER", "SFXPWRUP_SHELL_GEN_Shatter", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_GEN_ACQUIRE", "SFXPWRUP_GEN_Aquire", 0.55f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_GEN_ACQUIRE_02", "SFXPWRUP_GEN_Aquire_02", 0.55f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_GEN_ACQUIRE_03", "SFXPWRUP_GEN_Aquire_03", 0.55f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_GEN_ACQUIRE_04", "SFXPWRUP_GEN_Aquire_04", 0.55f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_RED_ACQUIRE", "SFXPWRUP_SHELL_RED_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_RED_ACTIVATE", "SFXPWRUP_SHELL_RED_Activate", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_RED_MOVE", "SFXPWRUP_SHELL_RED_Move", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_RED_HIT", "SFXPWRUP_SHELL_RED_Hit", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_RED_BOUNCE_WALL", "SFXPWRUP_SHELL_GEN_Force_Field", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_RED_BOUNCE_GROUND", "SFXPWRUP_SHELL_RED_Bounce", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_GREEN_ACQUIRE", "SFXPWRUP_SHELL_GREEN_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_GREEN_ACTIVATE", "SFXPWRUP_SHELL_GREEN_Activate", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_GREEN_MOVE", "SFXPWRUP_SHELL_GREEN_Move", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_GREEN_HIT", "SFXPWRUP_SHELL_GREEN_Hit", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_GREEN_BOUNCE_WALL", "SFXPWRUP_SHELL_GEN_Force_Field", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_GREEN_BOUNCE_GROUND", "SFXPWRUP_SHELL_GREEN_Bounce", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_FREEZE_ACQUIRE", "SFXPWRUP_SHELL_FREEZE_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_FREEZE_ACTIVATE", "SFXPWRUP_SHELL_FREEZE_Activate", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_FREEZE_MOVE", "SFXPWRUP_SHELL_FREEZE_Move", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_FREEZE_HIT", "SFXPWRUP_SHELL_FREEZE_Hit", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_FREEZE_BOUNCE_WALL", "SFXPWRUP_SHELL_GEN_Force_Field", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_FREEZE_BOUNCE_GROUND", "SFXPWRUP_SHELL_FREEZE_Bounce", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_SPINY_ACQUIRE", "SFXPWRUP_SHELL_SPINY_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_SHELL_SPINY_ACTIVATE", "SFXPWRUP_SHELL_SPINY_Activate", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_SPINY_MOVE", "SFXPWRUP_SHELL_SPINY_Move", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_SPINY_HIT", "SFXPWRUP_SHELL_SPINY_Hit", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_SPINY_BOUNCE_WALL", "SFXPWRUP_SHELL_GEN_Force_Field", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_SHELL_SPINY_BOUNCE_GROUND", "SFXPWRUP_SHELL_SPINY_Bounce", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_MUSH_ACQUIRE", "SFXPWRUP_MUSH_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_MUSH_ACTIVATE", "SFXPWRUP_MUSH_Activate", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_MUSH_END", "SFXPWRUP_MUSH_End", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BANA_ACQUIRE", "SFXPWRUP_BANA_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_BANA_ACTIVATE", "SFXPWRUP_BANA_Activate", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BANA_BOUNCE", "SFXPWRUP_BANA_Bounce", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BANA_HIT", "SFXPWRUP_BANA_Hit", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_STAR_ACQUIRE", "SFXPWRUP_STAR_Aquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_STAR_ACTIVATE", "SFXPWRUP_STAR_Activate", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_STAR_HIT", "SFXPWRUP_STAR_Hit", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_STAR_END", "SFXPWRUP_STAR_End", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BOMB_ACQUIRE", "SFXPWRUP_BOMB_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_BOMB_ACTIVATE", "SFXPWRUP_BOMB_Activate", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BOMB_MOVE", "SFXPWRUP_BOMB_Move", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BOMB_HIT", "SFXPWRUP_BOMB_Hit", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BOMB_END", "SFXPWRUP_BOMB_END", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_CHOMP_ACQUIRE", "SFXPWRUP_CHOMP_Acquire", 0.7f, 0.0f, 0.0f, 1, 0 },
          { "PWRUPSFX_CHOMP_ACTIVATE", "SFXPWRUP_CHOMP_Activate", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_CHOMP_MOVE", "SFXPWRUP_CHOMP_Move", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_CHOMP_HIT", "SFXPWRUP_CHOMP_Hit", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_CHOMP_BOUNCE", "SFXPWRUP_CHOMP_Bounce", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_CHOMP_BARK", "SFXPWRUP_CHOMP_Bark", 0.9f, 0.0f, 0.7f, 1, 0 } };

PWRUPSoundPropAccessor gPWRUPSoundPropAccessor;
SoundPropAccessor* gpPWRUPSoundPropAccessor = &gPWRUPSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801B37B4 | size: 0x10
 */
void PWRUPSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = PWRUPSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801B3798 | size: 0x1C
 */
bool PWRUPSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == PWRUPSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801B378C | size: 0xC
 */
const char* PWRUPSoundPropAccessor::GetHTMLFileName() const
{
    return "powerupsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3784 | size: 0x8
 */
const char* PWRUPSoundPropAccessor::GetSoundPropTableName() const
{
    return "PWRUP";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B377C | size: 0x8
 */
u32 PWRUPSoundPropAccessor::GetNumSFX() const
{
    return sizeof(PWRUPSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3774 | size: 0x8
 */
SoundProperties* PWRUPSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3714 | size: 0x60
 */
SoundProperties* PWRUPSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
