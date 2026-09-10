#include "dkgrasssoundproperties.h"

static SoundProperties DKGRASSSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Grass_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Grass_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Grass_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Grass_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Grass_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_BIG_Walk_Grass_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_BIG_Walk_Grass_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_BIG_Walk_Grass_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_BIG_Walk_Grass_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_BIG_Walk_Grass_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_BIG_Land_Grass", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Grass", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Grass", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Grass", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Grass_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Grass_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_BIG_Bodyfall_Grass", 0.6f, 0.0f, 0.5f, 1, 0 } };

DKGRASSSoundPropAccessor gDKGRASSSoundPropAccessor;
SoundPropAccessor* gpDKGRASSSoundPropAccessor = &gDKGRASSSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801B0720 | size: 0x10
 */
void DKGRASSSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = DKGRASSSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801B0704 | size: 0x1C
 */
bool DKGRASSSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == DKGRASSSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801B06F8 | size: 0xC
 */
const char* DKGRASSSoundPropAccessor::GetHTMLFileName() const
{
    return "dkgrasssoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B06F0 | size: 0x8
 */
const char* DKGRASSSoundPropAccessor::GetSoundPropTableName() const
{
    return "DKGRASS";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B06E8 | size: 0x8
 */
u32 DKGRASSSoundPropAccessor::GetNumSFX() const
{
    return sizeof(DKGRASSSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B06E0 | size: 0x8
 */
SoundProperties* DKGRASSSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B0680 | size: 0x60
 */
SoundProperties* DKGRASSSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
