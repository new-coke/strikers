#include "bowserconcretesoundproperties.h"

static SoundProperties BOWSERCONCRETESoundProperties[]
    = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Concrete_01", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Concrete_02", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Concrete_03", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Concrete_04", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Concrete_05", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_WALK_01", "SFXCHAR_BOWSER_Walk_Concrete_01", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_WALK_02", "SFXCHAR_BOWSER_Walk_Concrete_02", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_WALK_03", "SFXCHAR_BOWSER_Walk_Concrete_03", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_WALK_04", "SFXCHAR_BOWSER_Walk_Concrete_04", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_WALK_05", "SFXCHAR_BOWSER_Walk_Concrete_05", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_LAND", "SFXCHAR_BOWSER_Land_Concrete", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_SLIDE", "SFXCHAR_BOWSER_Roll_Concrete", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_JUMP", "SFXCHAR_BOWSER_Jump", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Concrete", 0.4f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Concrete_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Concrete_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BODYFALL", "SFXCHAR_BOWSER_Bodyfall_Concrete", 0.8f, 0.0f, 0.8f, 1, 0 },
          { "CHARSFX_BOWSER_ENTER", "SFXCHAR_BOWSER_Enter_Concrete", 0.8f, 0.0f, 0.5f, 1, 0 } };

BOWSERCONCRETESoundPropAccessor gBOWSERCONCRETESoundPropAccessor;
SoundPropAccessor* gpBOWSERCONCRETESoundPropAccessor = &gBOWSERCONCRETESoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801AE3B0 | size: 0x10
 */
void BOWSERCONCRETESoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = BOWSERCONCRETESoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801AE394 | size: 0x1C
 */
bool BOWSERCONCRETESoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == BOWSERCONCRETESoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801AE388 | size: 0xC
 */
const char* BOWSERCONCRETESoundPropAccessor::GetHTMLFileName() const
{
    return "bowserconcretesoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AE37C | size: 0xC
 */
const char* BOWSERCONCRETESoundPropAccessor::GetSoundPropTableName() const
{
    return "BOWSERCONCRETE";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AE374 | size: 0x8
 */
u32 BOWSERCONCRETESoundPropAccessor::GetNumSFX() const
{
    return sizeof(BOWSERCONCRETESoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AE36C | size: 0x8
 */
SoundProperties* BOWSERCONCRETESoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AE30C | size: 0x60
 */
SoundProperties* BOWSERCONCRETESoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
