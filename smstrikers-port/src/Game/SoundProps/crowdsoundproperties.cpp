#include "crowdsoundproperties.h"

static SoundProperties CROWDSoundProperties[]
    = { { "CROWDSFX_GOAL_HOME", "SFXCROWD_GEN_Goal_Home", 0.9f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_GOAL_AWAY", "SFXCROWD_GEN_Goal_Away", 0.9f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_LOOP_NEU", "SFXCROWD_GEN_Loop_Neu", 0.55f, 0.0f, 0.5f, 4294967295, 0 },
          { "CROWDSFX_LOOP_POS", "SFXCROWD_GEN_Loop_Pos", 0.55f, 0.0f, 0.5f, 4294967295, 0 },
          { "CROWDSFX_LOOP_NEG", "SFXCROWD_GEN_Loop_Neg", 0.55f, 0.0f, 0.5f, 4294967295, 0 },
          { "CROWDSFX_LOOP_BOO", "SFXCROWD_GEN_Boo_Loop", 0.55f, 0.0f, 0.5f, 4294967295, 0 },
          { "CROWDSFX_LOOP_ANT", "SFXCROWD_GEN_Ant_Loop", 0.55f, 0.0f, 0.5f, 4294967295, 0 },
          { "CROWDSFX_LOOP_WHISTLE", "SFXCROWD_GEN_Whistle_Loop", 0.55f, 0.0f, 0.5f, 4294967295, 0 },
          { "CROWDSFX_EVENT_YEAH_BIG", "SFXCROWD_GEN_Yeah_Big", 0.9f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_YEAH_SMALL1", "SFXCROWD_GEN_Yeah_Small_01", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_YEAH_SMALL2", "SFXCROWD_GEN_Yeah_Small_02", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_YEAH_SMALL3", "SFXCROWD_GEN_Yeah_Small_03", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_ANT_BIG", "SFXCROWD_GEN_Ant_Big", 0.75f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_ANT_SMALL", "SFXCROWD_GEN_Ant_Small", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_OH_BIG1", "SFXCROWD_GEN_Oh_Big01", 0.8f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_OH_BIG2", "SFXCROWD_GEN_Oh_Big02", 0.8f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_OH_BIG3", "SFXCROWD_GEN_Oh_Big03", 0.8f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_OH_SMALL1", "SFXCROWD_GEN_Oh_Small_01", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_OH_SMALL2", "SFXCROWD_GEN_Oh_Small_02", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_OH_SMALL3", "SFXCROWD_GEN_Oh_Small_03", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_OH_SMALL4", "SFXCROWD_GEN_Oh_Small_04", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_CLAP_BIG", "SFXCROWD_GEN_Clap_Big", 0.8f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_CLAP_SMALL", "SFXCROWD_GEN_Clap_Small", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_JEER_BIG", "SFXCROWD_GEN_Jeer_Big", 0.8f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_JEER_SMALL", "SFXCROWD_GEN_Jeer_Small", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_BOO_SMALL1", "SFXCROWD_GEN_Boo_Small_01", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_BOO_SMALL2", "SFXCROWD_GEN_Boo_Small_02", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_BOO_SMALL3", "SFXCROWD_GEN_Boo_Small_03", 0.7f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_BOO_BIG1", "SFXCROWD_GEN_Boo_Big_01", 0.8f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_BOO_BIG2", "SFXCROWD_GEN_Boo_Big_02", 0.8f, 0.0f, 0.5f, 2, 0 },
          { "CROWDSFX_EVENT_BOO_BIG3", "SFXCROWD_GEN_Boo_Big_03", 0.8f, 0.0f, 0.5f, 2, 0 } };

CROWDSoundPropAccessor gCROWDSoundPropAccessor;
SoundPropAccessor* gpCROWDSoundPropAccessor = &gCROWDSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801B36B4 | size: 0x10
 */
void CROWDSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = CROWDSoundProperties;
}

/**
 * Offset/Address/Size: 0xD4 | 0x801B3698 | size: 0x1C
 */
bool CROWDSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == CROWDSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801B368C | size: 0xC
 */
const char* CROWDSoundPropAccessor::GetHTMLFileName() const
{
    return "crowdsoundproperties.htm";
}

/**
 * Offset/Address/Size: 0xC0 | 0x801B3684 | size: 0x8
 */
const char* CROWDSoundPropAccessor::GetSoundPropTableName() const
{
    return "CROWD";
}

/**
 * Offset/Address/Size: 0xB8 | 0x801B367C | size: 0x8
 */
u32 CROWDSoundPropAccessor::GetNumSFX() const
{
    return sizeof(CROWDSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3674 | size: 0x8
 */
SoundProperties* CROWDSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3614 | size: 0x60
 */
SoundProperties* CROWDSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return NULL;
    return &m_pTable[index];
}
