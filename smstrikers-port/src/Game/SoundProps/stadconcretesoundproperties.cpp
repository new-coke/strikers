#include "stadconcretesoundproperties.h"

static SoundProperties STADCONCRETESoundProperties[]
    = { { "STADSFX_BALL_BOUNCE", "SFXBALL_Bounce_Concrete", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_BALL_ROLL", "SFXBALL_Roll_Concrete", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "PWRUPSFX_BOMB_HIT", "SFXPWRUP_BOMB_Hit_Concrete", 1.0f, 0.0f, 0.7f, 1, 0 },
          { "PWRUPSFX_BOMB_END", "SFXPWRUP_BOMB_End_Concrete", 1.0f, 0.0f, 0.7f, 1, 0 },
          { "PWRUPSFX_CHOMP_BOUNCE", "SFXPWRUP_CHOMP_Bounce_Concrete", 0.9f, 0.0f, 0.7f, 1, 0 } };

STADCONCRETESoundPropAccessor gSTADCONCRETESoundPropAccessor;
SoundPropAccessor* gpSTADCONCRETESoundPropAccessor = &gSTADCONCRETESoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B3CC0 | size: 0x10
 */
void STADCONCRETESoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADCONCRETESoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B3CA4 | size: 0x1C
 */
bool STADCONCRETESoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADCONCRETESoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B3C98 | size: 0xC
 */
const char* STADCONCRETESoundPropAccessor::GetHTMLFileName() const
{
    return "stadconcretesoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3C8C | size: 0xC
 */
const char* STADCONCRETESoundPropAccessor::GetSoundPropTableName() const
{
    return "STADCONCRETE";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B3C84 | size: 0x8
 */
u32 STADCONCRETESoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADCONCRETESoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3C7C | size: 0x8
 */
SoundProperties* STADCONCRETESoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3C1C | size: 0x60
 */
SoundProperties* STADCONCRETESoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
