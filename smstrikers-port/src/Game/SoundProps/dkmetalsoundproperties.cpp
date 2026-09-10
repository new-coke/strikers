#include "dkmetalsoundproperties.h"

static SoundProperties DKMETALSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Metal_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Metal_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Metal_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Metal_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Metal_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_BIG_Walk_Metal_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_BIG_Walk_Metal_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_BIG_Walk_Metal_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_BIG_Walk_Metal_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_BIG_Walk_Metal_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_BIG_Land_Metal", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Metal", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Metal_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Metal_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_BIG_Bodyfall_Metal", 0.6f, 0.0f, 0.5f, 1, 0 } };

DKMETALSoundPropAccessor gDKMETALSoundPropAccessor;
SoundPropAccessor* gpDKMETALSoundPropAccessor = &gDKMETALSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801B0820 | size: 0x10
 */
void DKMETALSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = DKMETALSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801B0804 | size: 0x1C
 */
bool DKMETALSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == DKMETALSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801B07F8 | size: 0xC
 */
const char* DKMETALSoundPropAccessor::GetHTMLFileName() const
{
    return "dkmetalsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B07F0 | size: 0x8
 */
const char* DKMETALSoundPropAccessor::GetSoundPropTableName() const
{
    return "DKMETAL";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B07E8 | size: 0x8
 */
u32 DKMETALSoundPropAccessor::GetNumSFX() const
{
    return sizeof(DKMETALSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B07E0 | size: 0x8
 */
SoundProperties* DKMETALSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B0780 | size: 0x60
 */
SoundProperties* DKMETALSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
