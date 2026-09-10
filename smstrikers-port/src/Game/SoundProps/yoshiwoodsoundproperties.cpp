#include "yoshiwoodsoundproperties.h"

// ---- Table (17 entries, sizeof(entry) == 0x1C) ----
static SoundProperties YOSHIWOODSoundProperties[] = {
    { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Wood_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Wood_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Wood_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Wood_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Wood_05", 0.4f, 0.0f, 0.5f, 1, 0 },

    { "CHARSFX_WALK_01", "SFXCHAR_GEN_Walk_Wood_01", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_GEN_Walk_Wood_02", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_GEN_Walk_Wood_03", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_GEN_Walk_Wood_04", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_GEN_Walk_Wood_05", 0.5f, 0.0f, 0.5f, 1, 0 },

    { "CHARSFX_LAND", "SFXCHAR_GEN_Land_Wood", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Wood", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Wood", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Wood", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Wood_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Wood_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Wood", 0.5f, 0.0f, 0.5f, 1, 0 },
};

YOSHIWOODSoundPropAccessor gYOSHIWOODSoundPropAccessor;
SoundPropAccessor* gpYOSHIWOODSoundPropAccessor = &gYOSHIWOODSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B35B4 | size: 0x10
 */
void YOSHIWOODSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = YOSHIWOODSoundProperties;
}
/**
 * Offset/Address/Size: 0xD8 | 0x801B3598 | size: 0x1C
 */
bool YOSHIWOODSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == YOSHIWOODSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B358C | size: 0xC
 */
const char* YOSHIWOODSoundPropAccessor::GetHTMLFileName() const
{
    return "yoshiwoodsoundproperties.htm";
}
/**
 * Offset/Address/Size: 0xC0 | 0x801B3580 | size: 0xC
 */
const char* YOSHIWOODSoundPropAccessor::GetSoundPropTableName() const
{
    return "YOSHIWOOD";
}
/**
 * Offset/Address/Size: 0xB8 | 0x801B3578 | size: 0x8
 */
u32 YOSHIWOODSoundPropAccessor::GetNumSFX() const
{
    return sizeof(YOSHIWOODSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3570 | size: 0x8
 */
SoundProperties* YOSHIWOODSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3510 | size: 0x60
 */
SoundProperties* YOSHIWOODSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
