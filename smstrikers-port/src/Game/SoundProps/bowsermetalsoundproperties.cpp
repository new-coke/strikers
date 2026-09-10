#include "bowsermetalsoundproperties.h"

static SoundProperties BOWSERMETALSoundProperties[] = { { "CHARSFX_RUN_01", "SFXCHAR_BIG_Run_Metal_01", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_02", "SFXCHAR_BIG_Run_Metal_02", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_03", "SFXCHAR_BIG_Run_Metal_03", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_04", "SFXCHAR_BIG_Run_Metal_04", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_RUN_05", "SFXCHAR_BIG_Run_Metal_05", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_01", "SFXCHAR_BOWSER_Walk_Metal_01", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_02", "SFXCHAR_BOWSER_Walk_Metal_02", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_03", "SFXCHAR_BOWSER_Walk_Metal_03", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_04", "SFXCHAR_BOWSER_Walk_Metal_04", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_WALK_05", "SFXCHAR_BOWSER_Walk_Metal_05", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_LAND", "SFXCHAR_BOWSER_Land_Metal", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_SLIDE", "SFXCHAR_BOWSER_Roll_Metal", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_JUMP", "SFXCHAR_BOWSER_Jump", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_TURN", "SFXCHAR_GEN_Turn_Metal", 0.4f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_LEFT", "SFXCHAR_GEN_Deek_Metal_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_DEKE_RIGHT", "SFXCHAR_GEN_Deek_Metal_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BODYFALL", "SFXCHAR_BOWSER_Bodyfall_Metal", 0.8f, 0.0f, 0.5f, 1, 0 },
    { "CHARSFX_BOWSER_ENTER", "SFXCHAR_BOWSER_Enter_Metal", 0.8f, 0.0f, 0.5f, 1, 0 } };

BOWSERMETALSoundPropAccessor gBOWSERMETALSoundPropAccessor;
SoundPropAccessor* gpBOWSERMETALSoundPropAccessor = &gBOWSERMETALSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801AE2AC | size: 0x10
 */
void BOWSERMETALSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = BOWSERMETALSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801AE290 | size: 0x1C
 */
bool BOWSERMETALSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == BOWSERMETALSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801AE284 | size: 0xC
 */
const char* BOWSERMETALSoundPropAccessor::GetHTMLFileName() const
{
    return "bowsermetalsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AE278 | size: 0xC
 */
const char* BOWSERMETALSoundPropAccessor::GetSoundPropTableName() const
{
    return "BOWSERMETAL";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AE270 | size: 0x8
 */
u32 BOWSERMETALSoundPropAccessor::GetNumSFX() const
{
    return sizeof(BOWSERMETALSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AE268 | size: 0x8
 */
SoundProperties* BOWSERMETALSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AE208 | size: 0x60
 */
SoundProperties* BOWSERMETALSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
