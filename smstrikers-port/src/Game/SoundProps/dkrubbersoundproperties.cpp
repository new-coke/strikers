#include "dkrubbersoundproperties.h"

static SoundProperties DKRUBBERSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Rubber_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Rubber_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Rubber_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Rubber_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Rubber_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_BIG_Walk_Rubber_01", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_BIG_Walk_Rubber_02", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_BIG_Walk_Rubber_03", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_BIG_Walk_Rubber_04", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_BIG_Walk_Rubber_05", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_BIG_Land_Rubber", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Rubber", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Rubber", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Rubber", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Rubber_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Rubber_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_BIG_Bodyfall_Rubber", 0.6f, 0.0f, 0.5f, 1, 0 } };

DKRUBBERSoundPropAccessor gDKRUBBERSoundPropAccessor;
SoundPropAccessor* gpDKRUBBERSoundPropAccessor = &gDKRUBBERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B0A28 | size: 0x10
 */
void DKRUBBERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = DKRUBBERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B0A0C | size: 0x1C
 */
bool DKRUBBERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == DKRUBBERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B0A00 | size: 0xC
 */
const char* DKRUBBERSoundPropAccessor::GetHTMLFileName() const
{
    return "dkrubbersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B09F4 | size: 0xC
 */
const char* DKRUBBERSoundPropAccessor::GetSoundPropTableName() const
{
    return "DKRUBBER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B09EC | size: 0x8
 */
u32 DKRUBBERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(DKRUBBERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B09E4 | size: 0x8
 */
SoundProperties* DKRUBBERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B0984 | size: 0x60
 */
SoundProperties* DKRUBBERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
