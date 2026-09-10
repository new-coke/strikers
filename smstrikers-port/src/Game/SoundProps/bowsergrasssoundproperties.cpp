#include "bowsergrasssoundproperties.h"

static SoundProperties BOWSERGRASSSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Grass_01", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Grass_02", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Grass_03", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Grass_04", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Grass_05", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_BOWSER_Walk_Grass_01", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_BOWSER_Walk_Grass_02", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_BOWSER_Walk_Grass_03", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_BOWSER_Walk_Grass_04", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_BOWSER_Walk_Grass_05", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_BOWSER_Land_Grass", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_BOWSER_Roll_Grass", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_BOWSER_Jump", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Grass", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Grass_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Grass_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_BOWSER_Bodyfall_Grass", 0.8f, 0.0f, 0.8f, 1, 0 },
    { "CHARSFX_BOWSER_ENTER", "SFXCHAR_BOWSER_Enter_Grass", 0.8f, 0.0f, 0.5f, 1, 0 } };

BOWSERGRASSSoundPropAccessor gBOWSERGRASSSoundPropAccessor;
SoundPropAccessor* gpBOWSERGRASSSoundPropAccessor = &gBOWSERGRASSSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801AE1A8 | size: 0x10
 */
void BOWSERGRASSSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = BOWSERGRASSSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801AE18C | size: 0x1C
 */
bool BOWSERGRASSSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == BOWSERGRASSSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801AE180 | size: 0xC
 */
const char* BOWSERGRASSSoundPropAccessor::GetHTMLFileName() const
{
    return "bowsergrasssoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AE174 | size: 0xC
 */
const char* BOWSERGRASSSoundPropAccessor::GetSoundPropTableName() const
{
    return "BOWSERGRASS";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AE16C | size: 0x8
 */
u32 BOWSERGRASSSoundPropAccessor::GetNumSFX() const
{
    return sizeof(BOWSERGRASSSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AE164 | size: 0x8
 */
SoundProperties* BOWSERGRASSSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AE104 | size: 0x60
 */
SoundProperties* BOWSERGRASSSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
