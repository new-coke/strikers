#include "waluigigrasssoundproperties.h"

static SoundProperties WALUIGIGRASSSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Grass_01", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Grass_02", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Grass_03", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Grass_04", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Grass_05", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_GEN_Walk_Grass_01", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_GEN_Walk_Grass_02", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_GEN_Walk_Grass_03", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_GEN_Walk_Grass_04", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_GEN_Walk_Grass_05", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_GEN_Land_Grass", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Grass", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Grass", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Grass", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Grass_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Grass_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Grass", 0.5f, 0.0f, 0.5f, 1, 0 } };

WALUIGIGRASSSoundPropAccessor gWALUIGIGRASSSoundPropAccessor;
SoundPropAccessor* gpWALUIGIGRASSSoundPropAccessor = &gWALUIGIGRASSSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B257C | size: 0x10
 */
void WALUIGIGRASSSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = WALUIGIGRASSSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B2560 | size: 0x1C
 */
bool WALUIGIGRASSSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == WALUIGIGRASSSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B2554 | size: 0xC
 */
const char* WALUIGIGRASSSoundPropAccessor::GetHTMLFileName() const
{
    return "waluigigrasssoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B2548 | size: 0xC
 */
const char* WALUIGIGRASSSoundPropAccessor::GetSoundPropTableName() const
{
    return "WALUIGIGRASS";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B2540 | size: 0x8
 */
u32 WALUIGIGRASSSoundPropAccessor::GetNumSFX() const
{
    return sizeof(WALUIGIGRASSSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B2538 | size: 0x8
 */
SoundProperties* WALUIGIGRASSSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B24D8 | size: 0x60
 */
SoundProperties* WALUIGIGRASSSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
