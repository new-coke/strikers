#include "bowsergensoundproperties.h"

static SoundProperties BOWSERSoundProperties[]
    = { { "CHARSFX_KICK_ATTEMPT", "SFXCHAR_GEN_Kick", 0.3f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_HIT_BODY", "SFXCHAR_BOWSER_GetHit", 0.4f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BODYFALL_BONECRUNCH", "SFXCHAR_GEN_Bodyfall_Bone_Crunch", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SUPER_KICK_WINDUP", "SFXCHAR_GEN_Kick_Super", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_PWRUP_MUSH_IN_EFFECT", "SFXPWRUP_MUSH_In_Effect", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_PWRUP_STAR_IN_EFFECT", "SFXPWRUP_STAR_In_Effect", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SHELL_FREEZE_HIT", "SFXCHAR_GEN_SHELL_FREEZE_Hit", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SHELL_FREEZE_END", "SFXCHAR_GEN_SHELL_FREEZE_End", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_EFFORTS_ATTACK_01", "SFXCHAR_BOWSER_EFFORTS_Attack_01", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_ATTACK_02", "SFXCHAR_BOWSER_EFFORTS_Attack_02", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_ATTACK_03", "SFXCHAR_BOWSER_EFFORTS_Attack_03", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_HIT_01", "SFXCHAR_BOWSER_EFFORTS_Hit_01", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_HIT_02", "SFXCHAR_BOWSER_EFFORTS_Hit_02", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_HIT_03", "SFXCHAR_BOWSER_EFFORTS_Hit_03", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_EXERT_01", "SFXCHAR_BOWSER_EFFORTS_Exert_01", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_EXERT_02", "SFXCHAR_BOWSER_EFFORTS_Exert_02", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_EXERT_03", "SFXCHAR_BOWSER_EFFORTS_Exert_03", 1.0f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_EFFORTS_DAZED", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_EFFORTS_HEAD_SHAKE", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BREATH_WITH_BALL", "SFXCHAR_BOWSER_Breath_Run", 0.8f, 0.0f, 0.5f, 6, 0 },
          { "CHARSFX_BOWSER_BREATH_FIRE", "SFXCHAR_BOWSER_Breath_Fire", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DAZED", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_HEAD_POP", "SFXCHAR_GEN_Head_Pop", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_ELECTROCUTED", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_THROW", "SFXCHAR_GEN_Throw", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SWISH_UP_01", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SWISH_UP_02", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SWISH_UP_03", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SWISH_DOWN_01", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SWISH_DOWN_02", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SWISH_DOWN_03", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_GEN_STOS_JUMP", "SFXBALL_Chest", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_01", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_02", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_03", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_04", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_05", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_06", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_07", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_08", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_09", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_10", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BOWSER_ACTIVATE", "SFXCHAR_BOWSER_Activate", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BOWSER_HOWL_01", "SFXCHAR_BOWSER_EFFORTS_Howl_01", 1.0f, 0.0f, 0.7f, 6, 0 },
          { "CHARSFX_BOWSER_HOWL_02", "SFXCHAR_BOWSER_EFFORTS_Howl_02", 1.0f, 0.0f, 0.7f, 6, 0 },
          { "CHARSFX_BOWSER_HOWL_03", "SFXCHAR_BOWSER_EFFORTS_Howl_03", 1.0f, 0.0f, 0.7f, 6, 0 },
          { "CHARSFX_BOWSER_CHARGE_01", "SFXCHAR_BOWSER_EFFORTS_Charge_01", 1.0f, 0.0f, 0.7f, 6, 0 },
          { "CHARSFX_BOWSER_CHARGE_02", "SFXCHAR_BOWSER_EFFORTS_Charge_02", 1.0f, 0.0f, 0.7f, 6, 0 } };

BOWSERSoundPropAccessor gBOWSERSoundPropAccessor;
SoundPropAccessor* gpBOWSERSoundPropAccessor = &gBOWSERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801AE0A4 | size: 0x10
 */
void BOWSERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = BOWSERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801AE088 | size: 0x1C
 */
bool BOWSERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == BOWSERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801AE07C | size: 0xC
 */
const char* BOWSERSoundPropAccessor::GetHTMLFileName() const
{
    return "bowsergensoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AE074 | size: 0x8
 */
const char* BOWSERSoundPropAccessor::GetSoundPropTableName() const
{
    return "BOWSER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AE06C | size: 0x8
 */
u32 BOWSERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(BOWSERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AE064 | size: 0x8
 */
SoundProperties* BOWSERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AE004 | size: 0x60
 */
SoundProperties* BOWSERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
