#include "stadpalacesoundproperties.h"

static SoundProperties STADPALACESoundProperties[] = { { "STADSFX_BALL_HITS_POST", "SFXBALL_Post_Concrete", 0.9f, 0.0f, 0.5f, 1, 0 } };

STADPALACESoundPropAccessor gSTADPALACESoundPropAccessor;
SoundPropAccessor* gpSTADPALACESoundPropAccessor = &gSTADPALACESoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B40D0 | size: 0x10
 */
void STADPALACESoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADPALACESoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B40B4 | size: 0x1C
 */
bool STADPALACESoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADPALACESoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B40A8 | size: 0xC
 */
const char* STADPALACESoundPropAccessor::GetHTMLFileName() const
{
    return "stadpalacesoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B409C | size: 0xC
 */
const char* STADPALACESoundPropAccessor::GetSoundPropTableName() const
{
    return "STADPALACE";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B4094 | size: 0x8
 */
u32 STADPALACESoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADPALACESoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B408C | size: 0x8
 */
SoundProperties* STADPALACESoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B402C | size: 0x60
 */
SoundProperties* STADPALACESoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
