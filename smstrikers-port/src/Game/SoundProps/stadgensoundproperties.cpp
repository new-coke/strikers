#include "stadgensoundproperties.h"

static SoundProperties STADGENSoundProperties[]
    = { { "BALLSFX_PASS_RECEIVE", "SFXBALL_Pass_Receive", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_KICK", "SFXBALL_Kick", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_KICK_ONETIMER", "SFXBALL_Kick_OneTimer", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_KICK_S2S_CAPT", "SFXBALL_StoS_Capt", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_PASS", "SFXBALL_Pass", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_PASS_VOLLEY", "SFXBALL_Pass_Volley", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_HEADER", "SFXBALL_Header", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_CHEST", "SFXBALL_Chest", 0.8f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_PASS_VOLLEY_FLY", "SFXBALL_Pass_Volley_Fly", 0.3f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_PASS_PERFECT_FLY", "SFXBALL_Pass_Perfect", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_KICK_HARD", "SFXBALL_Kick_Hard", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_KICK_S2S_REG", "SFXBALL_StoS_Reg", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_KICK_S2S_FLAME", "SFXBALL_Fireball", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_THROW", "SFXBALL_Throw", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_SLAP", "SFXBALL_Slap", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_CATCH", "SFXBALL_Catch", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_CATCH_MOUTH", "SFXBALL_Catch_Mouth", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_DRIBBLE_WALK", "SFXBALL_Dribble_Walk", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "BALLSFX_DRIBBLE_RUN", "SFXBALL_Dribble_Run", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_BALL_HITS_FORCE_FIELD", "SFXBALL_Force_Field", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_BALL_HITS_NET_HARD", "SFXBALL_Net_Hard", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_BALL_HITS_NET_SOFT", "SFXBALL_Net_Soft", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_GEN_HORN_GOAL", "SFXSTAD_GEN_HORN_Goal", 0.9f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_GEN_HALF_END_WARNING", "SFXSTAD_GEN_Half_End_Warning", 0.85f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_GEN_FIREWORKS_FLOOR", "SFXSTAD_GEN_Fireworks_Floor", 0.8f, 0.0f, 0.7f, 1, 0 },
          { "STADSFX_GEN_FIREWORKS_CEILING", "SFXSTAD_GEN_Fireworks_Ceiling", 0.8f, 0.0f, 0.7f, 1, 0 },
          { "STADSFX_GEN_CAMERA_SHAKE", "SFXSTAD_GEN_Camera_Shake", 0.6f, 0.0f, 0.7f, 1, 0 },
          { "STADSFX_GEN_SIDELINE_EXPLODE", "SFXSTAD_GEN_Sideline_Explo", 0.6f, 0.0f, 0.7f, 1, 0 },
          { "STADSFX_GEN_NIS_FORCE_FIELD", "SFXSTAD_GEN_NIS_Force_Field", 1.0f, 0.0f, 0.7f, 1, 0 },
          { "STADSFX_STRIKE_HYPER_SFX", "SFXSTAD_Strike_Hyper_SFX", 1.0f, 0.0f, 0.5f, 1, 0 },
          { "STADSFX_STRIKE_CAPT_SFX", "SFXSTAD_Strike_Capt_SFX", 1.0f, 0.0f, 0.5f, 1, 0 } };

STADGENSoundPropAccessor gSTADGENSoundPropAccessor;
SoundPropAccessor* gpSTADGENSoundPropAccessor = &gSTADGENSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801B39B4 | size: 0x10
 */
void STADGENSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = STADGENSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801B3998 | size: 0x1C
 */
bool STADGENSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == STADGENSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801B398C | size: 0xC
 */
const char* STADGENSoundPropAccessor::GetHTMLFileName() const
{
    return "stadgensoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3984 | size: 0x8
 */
const char* STADGENSoundPropAccessor::GetSoundPropTableName() const
{
    return "STADGEN";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B397C | size: 0x8
 */
u32 STADGENSoundPropAccessor::GetNumSFX() const
{
    return sizeof(STADGENSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3974 | size: 0x8
 */
SoundProperties* STADGENSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3914 | size: 0x60
 */
SoundProperties* STADGENSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
