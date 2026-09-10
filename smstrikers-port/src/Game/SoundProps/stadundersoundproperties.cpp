#include "stadundersoundproperties.h"

static SoundProperties STADUNDERSoundProperties[] = { { "STADSFX_BALL_HITS_POST", "SFXBALL_Post_Concrete", 0.9f, 0.0f, 0.5f, 1, 0 } };

STADUNDERSoundPropAccessor gSTADUNDERSoundPropAccessor;
SoundPropAccessor* gpSTADUNDERSoundPropAccessor = &gSTADUNDERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B41D4 | size: 0x10
 */
void STADUNDERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADUNDERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B41B8 | size: 0x1C
 */
bool STADUNDERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADUNDERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B41AC | size: 0xC
 */
const char* STADUNDERSoundPropAccessor::GetHTMLFileName() const
{
    return "stadundersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B41A0 | size: 0xC
 */
const char* STADUNDERSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADUNDER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B4198 | size: 0x8
 */
u32 STADUNDERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADUNDERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B4190 | size: 0x8
 */
SoundProperties* STADUNDERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B4130 | size: 0x60
 */
SoundProperties* STADUNDERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
