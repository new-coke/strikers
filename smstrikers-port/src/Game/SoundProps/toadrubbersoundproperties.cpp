#include "toadrubbersoundproperties.h"

static SoundProperties TOADRUBBERSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Rubber_01", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Rubber_02", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Rubber_03", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Rubber_04", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Rubber_05", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_GEN_Walk_Rubber_01", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_GEN_Walk_Rubber_02", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_GEN_Walk_Rubber_03", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_GEN_Walk_Rubber_04", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_GEN_Walk_Rubber_05", 0.6f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_GEN_Land_Rubber", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Rubber", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Rubber", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Rubber", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Rubber_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Rubber_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Rubber", 0.5f, 0.0f, 0.5f, 1, 0 } };

TOADRUBBERSoundPropAccessor gTOADRUBBERSoundPropAccessor;
SoundPropAccessor* gpTOADRUBBERSoundPropAccessor = &gTOADRUBBERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B2274 | size: 0x10
 */
void TOADRUBBERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = TOADRUBBERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B2258 | size: 0x1C
 */
bool TOADRUBBERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == TOADRUBBERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B224C | size: 0xC
 */
const char* TOADRUBBERSoundPropAccessor::GetHTMLFileName() const
{
    return "toadrubbersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B2240 | size: 0xC
 */
const char* TOADRUBBERSoundPropAccessor::GetSoundPropTableName() const
{
    return "TOADRUBBER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B2238 | size: 0x8
 */
u32 TOADRUBBERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(TOADRUBBERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B2230 | size: 0x8
 */
SoundProperties* TOADRUBBERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B21D0 | size: 0x60
 */
SoundProperties* TOADRUBBERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
