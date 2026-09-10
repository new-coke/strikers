#include "crittergensoundproperties.h"

static SoundProperties CRITTERSoundProperties[]
    = { { "CHARSFX_KICK_ATTEMPT", "SFXCHAR_GEN_Kick", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SHOT_WINDUP", "SFXCHAR_GEN_Charge_Windup", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_HIT_BODY", "SFXCHAR_GEN_Hit_Body", 0.75f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_HIT_BODY_BONE", "SFXCHAR_GEN_Hit_Body_Bone", 0.75f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_HIT_SLIDE", "SFXCHAR_GEN_Hit_Slide", 0.75f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BODYFALL_BONECRUNCH", "SFXCHAR_GEN_Bodyfall_Bone_Crunch", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SUPER_KICK_WINDUP", "SFXCHAR_GEN_Kick_Super", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BACK_FLIP", "SFXCHAR_GEN_BackFlip", 0.3f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_PWRUP_MUSH_IN_EFFECT", "SFXPWRUP_MUSH_In_Effect", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_PWRUP_STAR_IN_EFFECT", "SFXPWRUP_STAR_In_Effect", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SHELL_FREEZE_HIT", "SFXCHAR_GEN_SHELL_FREEZE_Hit", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SHELL_FREEZE_END", "SFXCHAR_GEN_SHELL_FREEZE_End", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_EFFORTS_ATTACK_01", "SFXCHAR_CRITTER_EFFORTS_Attack_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_ATTACK_02", "SFXCHAR_CRITTER_EFFORTS_Attack_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_ATTACK_03", "SFXCHAR_CRITTER_EFFORTS_Attack_03", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_HIT_01", "SFXCHAR_CRITTER_EFFORTS_Hit_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_HIT_02", "SFXCHAR_CRITTER_EFFORTS_Hit_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_HIT_03", "SFXCHAR_CRITTER_EFFORTS_Hit_03", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_GET_HIT_01", "SFXCHAR_CRITTER_EFFORTS_GetHit_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_GET_HIT_02", "SFXCHAR_CRITTER_EFFORTS_GetHit_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_GET_HIT_03", "SFXCHAR_CRITTER_EFFORTS_GetHit_03", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_PAIN_01", "SFXCHAR_CRITTER_EFFORTS_Pain_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_PAIN_02", "SFXCHAR_CRITTER_EFFORTS_Pain_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_PAIN_03", "SFXCHAR_CRITTER_EFFORTS_Pain_03", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_EXERT_01", "SFXCHAR_CRITTER_EFFORTS_Exert_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_EXERT_02", "SFXCHAR_CRITTER_EFFORTS_Exert_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_EXERT_03", "SFXCHAR_CRITTER_EFFORTS_Exert_03", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_KICK_01", "SFXCHAR_CRITTER_EFFORTS_Kick_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_KICK_02", "SFXCHAR_CRITTER_EFFORTS_Kick_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_KICK_03", "SFXCHAR_CRITTER_EFFORTS_Kick_03", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_DAZED", "SFXCHAR_CRITTER_EFFORTS_Dazed", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_EFFORTS_HEAD_SHAKE", "SFXCHAR_CRITTER_Head_Shake", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CRITTER_EFFORTS_CATCH_MOUTH", "SFXCHAR_CRITTER_EFFORTS_Catch_Mouth_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CRITTER_EFFORTS_FEAR", "SFXCHAR_CRITTER_EFFORTS_Fear_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CRITTER_EFFORTS_PUZZLED", "SFXCHAR_CRITTER_EFFORTS_Puzzled_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CRITTER_EFFORTS_ANGER", "SFXCHAR_CRITTER_EFFORTS_Anger_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CRITTER_HIT_HYPER", "SFXCHAR_CRITTER_Hit_Hyper", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_CALL_HEY_01", "SFXCHAR_CRITTER_CALL_Hey_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CALL_HEY_02", "SFXCHAR_CRITTER_CALL_Hey_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CALL_WO_01", "SFXCHAR_CRITTER_CALL_Wo_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CALL_WO_02", "SFXCHAR_CRITTER_CALL_Wo_02", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_CALL_PERFECT_PASS", "SFXCHAR_CRITTER_CALL_PPass_01", 0.7f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_BREATH_WITH_BALL", "SFXCHAR_CRITTER_Breath_Run", 0.5f, 0.0f, 0.5f, 7, 0 },
          { "CHARSFX_HEAD_POP", "SFXCHAR_GEN_Head_Pop", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DAZED", "SFXBALL_Chest", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_ELECTROCUTED", "SFXSTAD_GEN_Electrocute", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_THROW", "SFXCHAR_GEN_Throw", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_GEN_STOS_JUMP", "SFXCHAR_GEN_StoS_Jump", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_CLAP_01", "SFXCHAR_GEN_NIS_Clap_01", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_CLAP_02", "SFXCHAR_GEN_NIS_Clap_02", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_CLAP_03", "SFXCHAR_GEN_NIS_Clap_03", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_NIS_SPIN_LOOP", "SFXCHAR_GEN_NIS_Spin_Loop", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_CRITTER_HIT_NET", "SFXCHAR_GEN_Hit_Net", 1.0f, 0.0f, 0.5f, 1, 0 } };

CRITTERSoundPropAccessor gCRITTERSoundPropAccessor;
SoundPropAccessor* gpCRITTERSoundPropAccessor = &gCRITTERSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801AE6B8 | size: 0x10
 */
void CRITTERSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = CRITTERSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801AE69C | size: 0x1C
 */
bool CRITTERSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == CRITTERSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801AE690 | size: 0xC
 */
const char* CRITTERSoundPropAccessor::GetHTMLFileName() const
{
    return "crittergensoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AE688 | size: 0x8
 */
const char* CRITTERSoundPropAccessor::GetSoundPropTableName() const
{
    return "CRITTER";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AE680 | size: 0x8
 */
u32 CRITTERSoundPropAccessor::GetNumSFX() const
{
    return sizeof(CRITTERSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AE678 | size: 0x8
 */
SoundProperties* CRITTERSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AE618 | size: 0x60
 */
SoundProperties* CRITTERSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
