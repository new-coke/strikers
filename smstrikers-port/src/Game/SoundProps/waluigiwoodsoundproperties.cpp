#include "waluigiwoodsoundproperties.h"

static SoundProperties WALUIGIWOODSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Wood_01", 0.4f, 0.0f, 0.5f, 1, 0 },
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
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Wood", 0.5f, 0.0f, 0.5f, 1, 0 } };

WALUIGIWOODSoundPropAccessor gWALUIGIWOODSoundPropAccessor;
SoundPropAccessor* gpWALUIGIWOODSoundPropAccessor = &gWALUIGIWOODSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B298C | size: 0x10
 */
void WALUIGIWOODSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = WALUIGIWOODSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B2970 | size: 0x1C
 */
bool WALUIGIWOODSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == WALUIGIWOODSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B2964 | size: 0xC
 */
const char* WALUIGIWOODSoundPropAccessor::GetHTMLFileName() const
{
    return "waluigiwoodsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B2958 | size: 0xC
 */
const char* WALUIGIWOODSoundPropAccessor::GetSoundPropTableName() const
{
    return "WALUIGIWOOD";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B2950 | size: 0x8
 */
u32 WALUIGIWOODSoundPropAccessor::GetNumSFX() const
{
    return sizeof(WALUIGIWOODSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B2948 | size: 0x8
 */
SoundProperties* WALUIGIWOODSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B28E8 | size: 0x60
 */
SoundProperties* WALUIGIWOODSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
