#include "luigiconcretesoundproperties.h"

static SoundProperties LUIGICONCRETESoundProperties[]
    = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Concrete_01", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Concrete_02", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Concrete_03", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Concrete_04", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Concrete_05", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_01", "SFXCHAR_GEN_Walk_Concrete_01", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_02", "SFXCHAR_GEN_Walk_Concrete_02", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_03", "SFXCHAR_GEN_Walk_Concrete_03", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_04", "SFXCHAR_GEN_Walk_Concrete_04", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_05", "SFXCHAR_GEN_Walk_Concrete_05", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_LAND", "SFXCHAR_GEN_Land_Concrete", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Concrete", 0.4f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Concrete", 0.45f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Concrete", 0.4f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Concrete_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Concrete_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BODYFALL", "SFXCHAR_GEN_Bodyfall_Concrete", 0.5f, 0.0f, 0.5f, 1, 0 } };

LUIGICONCRETESoundPropAccessor gLUIGICONCRETESoundPropAccessor;
SoundPropAccessor* gpLUIGICONCRETESoundPropAccessor = &gLUIGICONCRETESoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B0F34 | size: 0x10
 */
void LUIGICONCRETESoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = LUIGICONCRETESoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B0F18 | size: 0x1C
 */
bool LUIGICONCRETESoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == LUIGICONCRETESoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B0F0C | size: 0xC
 */
const char* LUIGICONCRETESoundPropAccessor::GetHTMLFileName() const
{
    return "luigiconcretesoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B0F00 | size: 0xC
 */
const char* LUIGICONCRETESoundPropAccessor::GetSoundPropTableName() const
{
    return "LUIGICONCRETE";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B0EF8 | size: 0x8
 */
u32 LUIGICONCRETESoundPropAccessor::GetNumSFX() const
{
    return sizeof(LUIGICONCRETESoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B0EF0 | size: 0x8
 */
SoundProperties* LUIGICONCRETESoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B0E90 | size: 0x60
 */
SoundProperties* LUIGICONCRETESoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
