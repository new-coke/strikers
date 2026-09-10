#include "superwoodsoundproperties.h"

static SoundProperties SUPERWOODSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Wood_01", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Wood_02", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Wood_03", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Wood_04", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Wood_05", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_SUPER_Walk_01", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_SUPER_Walk_02", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_SUPER_Walk_03", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_SUPER_Walk_04", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_SUPER_Walk_05", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_SUPER_Land", 0.7f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Wood", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Wood", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_SUPER_Turn", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_SUPER_Deek_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_SUPER_Deek_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_SUPER_Bodyfall", 0.5f, 0.0f, 0.5f, 1, 0 } };

SUPERWOODSoundPropAccessor gSUPERWOODSoundPropAccessor;
SoundPropAccessor* gpSUPERWOODSoundPropAccessor = &gSUPERWOODSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B1D64 | size: 0x10
 */
void SUPERWOODSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = SUPERWOODSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B1D48 | size: 0x1C
 */
bool SUPERWOODSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == SUPERWOODSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B1D3C | size: 0xC
 */
const char* SUPERWOODSoundPropAccessor::GetHTMLFileName() const
{
    return "superwoodsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B1D30 | size: 0xC
 */
const char* SUPERWOODSoundPropAccessor::GetSoundPropTableName() const
{
    return "SUPERWOOD";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B1D28 | size: 0x8
 */
u32 SUPERWOODSoundPropAccessor::GetNumSFX() const
{
    return sizeof(SUPERWOODSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B1D20 | size: 0x8
 */
SoundProperties* SUPERWOODSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B1CC0 | size: 0x60
 */
SoundProperties* SUPERWOODSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
