#include "hambrosgrasssoundproperties.h"

static SoundProperties HAMBROSGRASSSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Grass_01", 0.5f, 0.0f, 0.5f, 1, 0 },
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

HAMBROSGRASSSoundPropAccessor gHAMBROSGRASSSoundPropAccessor;
SoundPropAccessor* gpHAMBROSGRASSSoundPropAccessor = &gHAMBROSGRASSSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801AF4E8 | size: 0x10
 */
void HAMBROSGRASSSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = HAMBROSGRASSSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801AF4CC | size: 0x1C
 */
bool HAMBROSGRASSSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == HAMBROSGRASSSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801AF4C0 | size: 0xC
 */
const char* HAMBROSGRASSSoundPropAccessor::GetHTMLFileName() const
{
    return "hambrosgrasssoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AF4B4 | size: 0xC
 */
const char* HAMBROSGRASSSoundPropAccessor::GetSoundPropTableName() const
{
    return "HAMBROSGRASS";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AF4AC | size: 0x8
 */
u32 HAMBROSGRASSSoundPropAccessor::GetNumSFX() const
{
    return sizeof(HAMBROSGRASSSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AF4A4 | size: 0x8
 */
SoundProperties* HAMBROSGRASSSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AF444 | size: 0x60
 */
SoundProperties* HAMBROSGRASSSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
