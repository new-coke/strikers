#include "supergrasssoundproperties.h"

static SoundProperties SUPERGRASSSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Grass_01", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Grass_02", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Grass_03", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Grass_04", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Grass_05", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_SUPER_Walk_01", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_SUPER_Walk_02", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_SUPER_Walk_03", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_SUPER_Walk_04", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_SUPER_Walk_05", 0.35f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_SUPER_Land", 0.7f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Grass", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Grass", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_SUPER_Turn", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_SUPER_Deek_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_SUPER_Deek_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_SUPER_Bodyfall", 0.5f, 0.0f, 0.5f, 1, 0 } };

SUPERGRASSSoundPropAccessor gSUPERGRASSSoundPropAccessor;
SoundPropAccessor* gpSUPERGRASSSoundPropAccessor = &gSUPERGRASSSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B1954 | size: 0x10
 */
void SUPERGRASSSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = SUPERGRASSSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B1938 | size: 0x1C
 */
bool SUPERGRASSSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == SUPERGRASSSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B192C | size: 0xC
 */
const char* SUPERGRASSSoundPropAccessor::GetHTMLFileName() const
{
    return "supergrasssoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B1920 | size: 0xC
 */
const char* SUPERGRASSSoundPropAccessor::GetSoundPropTableName() const
{
    return "SUPERGRASS";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B1918 | size: 0x8
 */
u32 SUPERGRASSSoundPropAccessor::GetNumSFX() const
{
    return sizeof(SUPERGRASSSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B1910 | size: 0x8
 */
SoundProperties* SUPERGRASSSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B18B0 | size: 0x60
 */
SoundProperties* SUPERGRASSSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
