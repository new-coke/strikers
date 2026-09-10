#include "luigimetalsoundproperties.h"

static SoundProperties LUIGIMETALSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Metal_01", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Metal_02", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Metal_03", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Metal_04", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Metal_05", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_GEN_Walk_Metal_01", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_GEN_Walk_Metal_02", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_GEN_Walk_Metal_03", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_GEN_Walk_Metal_04", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_GEN_Walk_Metal_05", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_GEN_Land_Metal", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Metal", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Metal_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Metal_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Metal", 0.5f, 0.0f, 0.5f, 1, 0 } };

LUIGIMETALSoundPropAccessor gLUIGIMETALSoundPropAccessor;
SoundPropAccessor* gpLUIGIMETALSoundPropAccessor = &gLUIGIMETALSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B0E30 | size: 0x10
 */
void LUIGIMETALSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = LUIGIMETALSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B0E14 | size: 0x1C
 */
bool LUIGIMETALSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == LUIGIMETALSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B0E08 | size: 0xC
 */
const char* LUIGIMETALSoundPropAccessor::GetHTMLFileName() const
{
    return "luigimetalsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B0DFC | size: 0xC
 */
const char* LUIGIMETALSoundPropAccessor::GetSoundPropTableName() const
{
    return "LUIGIMETAL";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B0DF4 | size: 0x8
 */
u32 LUIGIMETALSoundPropAccessor::GetNumSFX() const
{
    return sizeof(LUIGIMETALSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B0DEC | size: 0x8
 */
SoundProperties* LUIGIMETALSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B0D8C | size: 0x60
 */
SoundProperties* LUIGIMETALSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
