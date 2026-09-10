#include "bowserrubbersoundproperties.h"

static SoundProperties BOWSERRUBBERSoundProperties[]
    = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Rubber_01", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Rubber_02", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Rubber_03", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Rubber_04", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Rubber_05", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_01", "SFXCHAR_BOWSER_Walk_Rubber_01", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_02", "SFXCHAR_BOWSER_Walk_Rubber_02", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_03", "SFXCHAR_BOWSER_Walk_Rubber_03", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_04", "SFXCHAR_BOWSER_Walk_Rubber_04", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_05", "SFXCHAR_BOWSER_Walk_Rubber_05", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_LAND", "SFXCHAR_BOWSER_Land_Rubber", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SLIDE", "SFXCHAR_BOWSER_Roll_Rubber", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_JUMP", "SFXCHAR_BOWSER_Jump", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Rubber", 0.4f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Rubber_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Rubber_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BODYFALL", "SFXCHAR_BOWSER_Bodyfall_Rubber", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BOWSER_ENTER", "SFXCHAR_BOWSER_Enter_Rubber", 0.8f, 0.0f, 0.5f, 1, 0 } };

BOWSERRUBBERSoundPropAccessor gBOWSERRUBBERSoundPropAccessor;
SoundPropAccessor* gpBOWSERRUBBERSoundPropAccessor = &gBOWSERRUBBERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801AE4B4 | size: 0x10
 */
void BOWSERRUBBERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = BOWSERRUBBERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801AE498 | size: 0x1C
 */
bool BOWSERRUBBERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == BOWSERRUBBERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801AE48C | size: 0xC
 */
const char* BOWSERRUBBERSoundPropAccessor::GetHTMLFileName() const
{
    return "bowserrubbersoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AE480 | size: 0xC
 */
const char* BOWSERRUBBERSoundPropAccessor::GetSoundPropTableName() const
{
    return "BOWSERRUBBER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AE478 | size: 0x8
 */
u32 BOWSERRUBBERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(BOWSERRUBBERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AE470 | size: 0x8
 */
SoundProperties* BOWSERRUBBERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AE410 | size: 0x60
 */
SoundProperties* BOWSERRUBBERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
