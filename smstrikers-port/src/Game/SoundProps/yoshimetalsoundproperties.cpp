#include "yoshimetalsoundproperties.h"

static SoundProperties YOSHIMETALSoundProperties[] = {
    { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Metal_01", 0.5f, 0.0f, 0.5f, 1, 0 },
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
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Metal", 0.5f, 0.0f, 0.5f, 1, 0 },
};

YOSHIMETALSoundPropAccessor gYOSHIMETALSoundPropAccessor;
SoundPropAccessor* gpYOSHIMETALSoundPropAccessor = &gYOSHIMETALSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B32A8 | size: 0x10
 */
void YOSHIMETALSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = YOSHIMETALSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B328C | size: 0x1C
 */
bool YOSHIMETALSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == YOSHIMETALSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B3280 | size: 0xC
 */
const char* YOSHIMETALSoundPropAccessor::GetHTMLFileName() const
{
    return "yoshimetalsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3274 | size: 0xC
 */
const char* YOSHIMETALSoundPropAccessor::GetSoundPropTableName() const
{
    return "YOSHIMETAL";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B326C | size: 0x8
 */
u32 YOSHIMETALSoundPropAccessor::GetNumSFX() const
{
    return sizeof(YOSHIMETALSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3264 | size: 0x8
 */
SoundProperties* YOSHIMETALSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3204 | size: 0x60
 */
SoundProperties* YOSHIMETALSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
