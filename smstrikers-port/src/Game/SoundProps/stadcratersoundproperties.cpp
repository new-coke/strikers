#include "stadcratersoundproperties.h"

static SoundProperties STADCRATERSoundProperties[] = { { "STADSFX_BALL_HITS_POST", "SFXBALL_Post_Metal", 0.9f, 0.0f, 0.5f, 1, 0 } };

STADCRATERSoundPropAccessor gSTADCRATERSoundPropAccessor;
SoundPropAccessor* gpSTADCRATERSoundPropAccessor = &gSTADCRATERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B45E4 | size: 0x10
 */
void STADCRATERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADCRATERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B45C8 | size: 0x1C
 */
bool STADCRATERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADCRATERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B45BC | size: 0xC
 */
const char* STADCRATERSoundPropAccessor::GetHTMLFileName() const
{
    return "stadcratersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B45B0 | size: 0xC
 */
const char* STADCRATERSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADCRATER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B45A8 | size: 0x8
 */
u32 STADCRATERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADCRATERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B45A0 | size: 0x8
 */
SoundProperties* STADCRATERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B4540 | size: 0x60
 */
SoundProperties* STADCRATERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
