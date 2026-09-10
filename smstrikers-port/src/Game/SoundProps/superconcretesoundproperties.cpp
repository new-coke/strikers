#include "superconcretesoundproperties.h"

SoundProperties SUPERCONCRETESoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_GEN_Run_Concrete_01", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_GEN_Run_Concrete_02", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_GEN_Run_Concrete_03", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_GEN_Run_Concrete_04", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_GEN_Run_Concrete_05", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_SUPER_Walk_01", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_SUPER_Walk_02", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_SUPER_Walk_03", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_SUPER_Walk_04", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_SUPER_Walk_05", 5.0f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_SUPER_Land", 0.7f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_GEN_Slide_Concrete", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_GEN_Jump_Concrete", 0.45f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_SUPER_Turn", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_SUPER_Deek_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_SUPER_Deek_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_SUPER_Bodyfall", 0.5f, 0.0f, 0.5f, 1, 0 } };

SUPERCONCRETESoundPropAccessor gSUPERCONCRETESoundPropAccessor;
SoundPropAccessor* gpSUPERCONCRETESoundPropAccessor = &gSUPERCONCRETESoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801B1B5C | size: 0x10
 */
void SUPERCONCRETESoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = SUPERCONCRETESoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801B1B40 | size: 0x1C
 */
bool SUPERCONCRETESoundPropAccessor::IsUsingOrigTable() const
{
    return m_pTable == SUPERCONCRETESoundProperties;
}

/**
 * Offset/Address/Size: 0xCC | 0x801B1B34 | size: 0xC
 */
const char* SUPERCONCRETESoundPropAccessor::GetHTMLFileName() const
{
    return "superconcretesoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B1B28 | size: 0xC
 */
const char* SUPERCONCRETESoundPropAccessor::GetSoundPropTableName() const
{
    return "SUPERCONCRETE";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B1B20 | size: 0x8
 */
u32 SUPERCONCRETESoundPropAccessor::GetNumSFX() const
{
    return sizeof(SUPERCONCRETESoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B1B18 | size: 0x8
 */
SoundProperties* SUPERCONCRETESoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B1AB8 | size: 0x60
 */
SoundProperties* SUPERCONCRETESoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
