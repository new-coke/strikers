#include "toadwoodsoundproperties.h"

static SoundProperties TOADWOODSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Wood_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Wood_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Wood_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Wood_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Wood_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_GEN_Walk_Wood_01", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_GEN_Walk_Wood_02", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_GEN_Walk_Wood_03", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_GEN_Walk_Wood_04", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_GEN_Walk_Wood_05", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_GEN_Land_Wood", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Wood", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Wood", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Wood", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Wood_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Wood_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Wood", 0.5f, 0.0f, 0.5f, 1, 0 } };

TOADWOODSoundPropAccessor gTOADWOODSoundPropAccessor;
SoundPropAccessor* gpTOADWOODSoundPropAccessor = &gTOADWOODSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B2378 | size: 0x10
 */
void TOADWOODSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = TOADWOODSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B235C | size: 0x1C
 */
bool TOADWOODSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == TOADWOODSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B2350 | size: 0xC
 */
const char* TOADWOODSoundPropAccessor::GetHTMLFileName() const
{
    return "toadwoodsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B2344 | size: 0xC
 */
const char* TOADWOODSoundPropAccessor::GetSoundPropTableName() const
{
    return "TOADWOOD";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B233C | size: 0x8
 */
u32 TOADWOODSoundPropAccessor::GetNumSFX() const
{
    return sizeof(TOADWOODSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B2334 | size: 0x8
 */
SoundProperties* TOADWOODSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B22D4 | size: 0x60
 */
SoundProperties* TOADWOODSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
