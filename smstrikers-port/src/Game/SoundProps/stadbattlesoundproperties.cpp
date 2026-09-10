#include "stadbattlesoundproperties.h"

static SoundProperties STADBATTLESoundProperties[] = { { "STADSFX_BALL_HITS_POST", "SFXBALL_Post_Metal", 0.9f, 0.0f, 0.5f, 1, 0 } };

STADBATTLESoundPropAccessor gSTADBATTLESoundPropAccessor;
SoundPropAccessor* gpSTADBATTLESoundPropAccessor = &gSTADBATTLESoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B43DC | size: 0x10
 */
void STADBATTLESoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADBATTLESoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B43C0 | size: 0x1C
 */
bool STADBATTLESoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADBATTLESoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B43B4 | size: 0xC
 */
const char* STADBATTLESoundPropAccessor::GetHTMLFileName() const
{
    return "stadbattlesoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B43A8 | size: 0xC
 */
const char* STADBATTLESoundPropAccessor::GetSoundPropTableName() const
{
    return "STADBATTLE";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B43A0 | size: 0x8
 */
u32 STADBATTLESoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADBATTLESoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B4398 | size: 0x8
 */
SoundProperties* STADBATTLESoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B4338 | size: 0x60
 */
SoundProperties* STADBATTLESoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
