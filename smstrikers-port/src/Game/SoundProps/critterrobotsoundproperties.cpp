#include "critterrobotsoundproperties.h"

static SoundProperties CRITTERROBOTSoundProperties[]
    = { { "CHARSFX_RUN_01", "SFXCHAR_SUPER_Walk_01", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_02", "SFXCHAR_SUPER_Walk_02", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_03", "SFXCHAR_SUPER_Walk_03", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_04", "SFXCHAR_SUPER_Walk_04", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_RUN_05", "SFXCHAR_SUPER_Walk_05", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_01", "SFXCHAR_SUPER_Walk_01", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_02", "SFXCHAR_SUPER_Walk_02", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_03", "SFXCHAR_SUPER_Walk_03", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_04", "SFXCHAR_SUPER_Walk_04", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_WALK_05", "SFXCHAR_SUPER_Walk_05", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_LAND", "SFXCHAR_SUPER_Land", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_TURN", "SFXCHAR_SUPER_Turn", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_LEFT", "SFXCHAR_SUPER_Deek_Left", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_DEKE_RIGHT", "SFXCHAR_SUPER_Deek_Right", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BODYFALL", "SFXCHAR_SUPER_Bodyfall", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_HIT_BODY", "SFXCHAR_SUPER_GetHit_01", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_BODYFALL_BONECRUNCH", "SFXCHAR_SUPER_Bodyfall", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_EFFORTS_ATTACK_01", "SFXCHAR_SUPER_EFFORTS_Attack_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_ATTACK_02", "SFXCHAR_SUPER_EFFORTS_Attack_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_ATTACK_03", "SFXCHAR_SUPER_EFFORTS_Attack_03", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_HIT_01", "SFXCHAR_SUPER_EFFORTS_Hit_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_HIT_02", "SFXCHAR_SUPER_EFFORTS_Hit_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_HIT_03", "SFXCHAR_SUPER_EFFORTS_Hit_03", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_GET_HIT_01", "SFXCHAR_SUPER_EFFORTS_GetHit_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_GET_HIT_02", "SFXCHAR_SUPER_EFFORTS_GetHit_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_GET_HIT_03", "SFXCHAR_SUPER_EFFORTS_GetHit_03", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_PAIN_01", "SFXCHAR_SUPER_EFFORTS_Pain_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_PAIN_02", "SFXCHAR_SUPER_EFFORTS_Pain_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_PAIN_03", "SFXCHAR_SUPER_EFFORTS_Pain_03", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_PAIN_04", "SFXCHAR_SUPER_EFFORTS_Pain_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_PAIN_05", "SFXCHAR_SUPER_EFFORTS_Pain_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_EXERT_01", "SFXCHAR_SUPER_EFFORTS_Exert_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_EXERT_02", "SFXCHAR_SUPER_EFFORTS_Exert_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_EXERT_03", "SFXCHAR_SUPER_EFFORTS_Exert_03", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_KICK_01", "SFXCHAR_SUPER_EFFORTS_Kick_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_KICK_02", "SFXCHAR_SUPER_EFFORTS_Kick_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_KICK_03", "SFXCHAR_SUPER_EFFORTS_Kick_03", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_DAZED", "SFXCHAR_SUPER_EFFORTS_Dazed", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_EFFORTS_HEAD_SHAKE", "SFXCHAR_SUPER_EFFORTS_Dazed", 0.7f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CRITTER_EFFORTS_CATCH_MOUTH", "SFXCHAR_SUPER_EFFORTS_Exert_01", 0.7f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CRITTER_EFFORTS_FEAR", "SFXCHAR_SUPER_EFFORTS_Attack_01", 0.7f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CRITTER_EFFORTS_PUZZLED", "SFXCHAR_SUPER_EFFORTS_Dazed", 0.7f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CRITTER_EFFORTS_ANGER", "SFXCHAR_SUPER_EFFORTS_Attack_01", 0.7f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CALL_HEY_01", "SFXCHAR_SUPER_CALL_Hey_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CALL_HEY_02", "SFXCHAR_SUPER_CALL_Hey_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CALL_WO_01", "SFXCHAR_SUPER_CALL_Wo_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CALL_WO_02", "SFXCHAR_SUPER_CALL_Wo_02", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_CALL_PERFECT_PASS", "SFXCHAR_SUPER_CALL_PPass_01", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_BREATH_WITH_BALL", "SFXCHAR_SUPER_Breath_Run", 0.5f, 0.0f, 0.5f, 4, 0 },
          { "CHARSFX_SUPER_LEFT_LEG_SERVO", "SFXCHAR_SUPER_Leg_Left", 0.3f, 0.0f, 0.5f, 1, 0 },
          { "CHARSFX_SUPER_RIGHT_LEG_SERVO", "SFXCHAR_SUPER_Leg_Right", 0.3f, 0.0f, 0.5f, 1, 0 } };

CRITTERROBOTSoundPropAccessor gCRITTERROBOTSoundPropAccessor;
SoundPropAccessor* gpCRITTERROBOTSoundPropAccessor = &gCRITTERROBOTSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF4 | 0x801AEBCC | size: 0x10
 */
void CRITTERROBOTSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = CRITTERROBOTSoundProperties;
}

/**
 * Offset/Address/Size: 0xD8 | 0x801AEBB0 | size: 0x1C
 */
bool CRITTERROBOTSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == CRITTERROBOTSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xCC | 0x801AEBA4 | size: 0xC
 */
const char* CRITTERROBOTSoundPropAccessor::GetHTMLFileName() const
{
    return "critterrobotsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801AEB98 | size: 0xC
 */
const char* CRITTERROBOTSoundPropAccessor::GetSoundPropTableName() const
{
    return "CRITTERROBOT";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801AEB90 | size: 0x8
 */
u32 CRITTERROBOTSoundPropAccessor::GetNumSFX() const
{
    return sizeof(CRITTERROBOTSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801AEB88 | size: 0x8
 */
SoundProperties* CRITTERROBOTSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801AEB28 | size: 0x60
 */
SoundProperties* CRITTERROBOTSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
