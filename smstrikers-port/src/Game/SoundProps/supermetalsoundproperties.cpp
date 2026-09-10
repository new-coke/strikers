#include "supermetalsoundproperties.h"

static SoundProperties SUPERMETALSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Metal_01", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Metal_02", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Metal_03", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Metal_04", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Metal_05", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_SUPER_Walk_01", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_SUPER_Walk_02", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_SUPER_Walk_03", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_SUPER_Walk_04", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_SUPER_Walk_05", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_SUPER_Land", 0.7f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Metal", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_SUPER_Turn", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_SUPER_Deek_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_SUPER_Deek_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_SUPER_Bodyfall", 0.5f, 0.0f, 0.5f, 1, 0 } };

SUPERMETALSoundPropAccessor gSUPERMETALSoundPropAccessor;
SoundPropAccessor* gpSUPERMETALSoundPropAccessor = &gSUPERMETALSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B1A58 | size: 0x10
 */
void SUPERMETALSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = SUPERMETALSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B1A3C | size: 0x1C
 */
bool SUPERMETALSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == SUPERMETALSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B1A30 | size: 0xC
 */
const char* SUPERMETALSoundPropAccessor::GetHTMLFileName() const
{
    return "supermetalsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B1A24 | size: 0xC
 */
const char* SUPERMETALSoundPropAccessor::GetSoundPropTableName() const
{
    return "SUPERMETAL";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B1A1C | size: 0x8
 */
u32 SUPERMETALSoundPropAccessor::GetNumSFX() const
{
    return sizeof(SUPERMETALSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B1A14 | size: 0x8
 */
SoundProperties* SUPERMETALSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B19B4 | size: 0x60
 */
SoundProperties* SUPERMETALSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
