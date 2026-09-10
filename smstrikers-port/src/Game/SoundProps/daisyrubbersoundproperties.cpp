#include "daisyrubbersoundproperties.h"

static SoundProperties DAISYRUBBERSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Rubber_01", 0.5f, 0.0f, 0.5f, 1, 0 },
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

DAISYRUBBERSoundPropAccessor gDAISYRUBBERSoundPropAccessor;
SoundPropAccessor* gpDAISYRUBBERSoundPropAccessor = &gDAISYRUBBERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801AF1E0 | size: 0x10
 */
void DAISYRUBBERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = DAISYRUBBERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801AF1C4 | size: 0x1C
 */
bool DAISYRUBBERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == DAISYRUBBERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801AF1B8 | size: 0xC
 */
const char* DAISYRUBBERSoundPropAccessor::GetHTMLFileName() const
{
    return "daisyrubbersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AF1AC | size: 0xC
 */
const char* DAISYRUBBERSoundPropAccessor::GetSoundPropTableName() const
{
    return "DAISYRUBBER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AF1A4 | size: 0x8
 */
u32 DAISYRUBBERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(DAISYRUBBERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AF19C | size: 0x8
 */
SoundProperties* DAISYRUBBERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AF13C | size: 0x60
 */
SoundProperties* DAISYRUBBERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
