#include "peachmetalsoundproperties.h"

static SoundProperties PEACHMETALSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Metal_01", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Metal_02", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Metal_03", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Metal_04", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Metal_05", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_GEN_Walk_Metal_01", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_GEN_Walk_Metal_02", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_GEN_Walk_Metal_03", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_GEN_Walk_Metal_04", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_GEN_Walk_Metal_05", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_GEN_Land_Metal", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Metal", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Metal_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Metal_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Metal", 0.5f, 0.0f, 0.5f, 1, 0 } };

PEACHMETALSoundPropAccessor gPEACHMETALSoundPropAccessor;
SoundPropAccessor* gpPEACHMETALSoundPropAccessor = &gPEACHMETALSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B1444 | size: 0x10
 */
void PEACHMETALSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = PEACHMETALSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B1428 | size: 0x1C
 */
bool PEACHMETALSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == PEACHMETALSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B141C | size: 0xC
 */
const char* PEACHMETALSoundPropAccessor::GetHTMLFileName() const
{
    return "peachmetalsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B1410 | size: 0xC
 */
const char* PEACHMETALSoundPropAccessor::GetSoundPropTableName() const
{
    return "PEACHMETAL";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B1408 | size: 0x8
 */
u32 PEACHMETALSoundPropAccessor::GetNumSFX() const
{
    return sizeof(PEACHMETALSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B1400 | size: 0x8
 */
SoundProperties* PEACHMETALSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B13A0 | size: 0x60
 */
SoundProperties* PEACHMETALSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
