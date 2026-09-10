#include "dkwoodsoundproperties.h"

static SoundProperties DKWOODSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Wood_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Wood_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Wood_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Wood_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Wood_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_BIG_Walk_Wood_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_BIG_Walk_Wood_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_BIG_Walk_Wood_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_BIG_Walk_Wood_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_BIG_Walk_Wood_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_BIG_Land_Wood", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Wood", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Wood", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Wood", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Wood_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Wood_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_BIG_Bodyfall_Wood", 0.6f, 0.0f, 0.5f, 1, 0 } };

DKWOODSoundPropAccessor gDKWOODSoundPropAccessor;
SoundPropAccessor* gpDKWOODSoundPropAccessor = &gDKWOODSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801B0B28 | size: 0x10
 */
void DKWOODSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = DKWOODSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801B0B0C | size: 0x1C
 */
bool DKWOODSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == DKWOODSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801B0B00 | size: 0xC
 */
const char* DKWOODSoundPropAccessor::GetHTMLFileName() const
{
    return "dkwoodsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B0AF8 | size: 0x8
 */
const char* DKWOODSoundPropAccessor::GetSoundPropTableName() const
{
    return "DKWOOD";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B0AF0 | size: 0x8
 */
u32 DKWOODSoundPropAccessor::GetNumSFX() const
{
    return sizeof(DKWOODSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B0AE8 | size: 0x8
 */
SoundProperties* DKWOODSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B0A88 | size: 0x60
 */
SoundProperties* DKWOODSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
