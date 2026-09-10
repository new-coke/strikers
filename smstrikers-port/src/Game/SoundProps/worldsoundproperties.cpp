#include "worldsoundproperties.h"

static SoundProperties WORLDSoundProperties[]
    = { { "WORLDSFX_FE_TOGGLE", "SFXFE_Toggle", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_TOGGLE_02", "SFXFE_Toggle_02", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_TOGGLE_03", "SFXFE_Toggle_03", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_TOGGLE_PULSE_01", "SFXFE_Toggle_Pulse_01", 0.4f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_BUTTON_GEN_SELECT_ACCEPT", "SFXFE_Accept", 0.7f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_BUTTON_GEN_SELECT_BACK", "SFXFE_Accept", 0.7f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_SCREEN_GEN_BEGIN", "SFXFE_SCREEN_GEN_Begin", 0.8f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_SCREEN_GEN_END", "SFXFE_SCREEN_GEN_End", 0.8f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_FLASH", "SFXFE_Flash", 0.9f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_LETTER_FLY_IN", "SFXFE_Letter_Fly_In", 0.5f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_LETTER_HIT", "SFXFE_Letter_Hit", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_LIGHT_TEXT_FLY_IN", "SFXFE_SCREEN_GEN_Begin", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_LIGHT_TEXT_FLY_IN_LEFT", "SFXFE_Letter_Left", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_LIGHT_TEXT_FLY_IN_RIGHT", "SFXFE_Letter_Right", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_MENU_OVERLAY_DROP", "SFXFE_Letter_Drop", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_LETTER_ROTATE_LOOP", "SFXFE_Letter_Rotate", 1.0f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_DENY", "SFXFE_Deny", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_USER_QUESTION", "SFXFE_User_Question", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_USER_POPUP", "SFXFE_User_Popup", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_TOGGLE_STADIUM", "SFXFE_Toggle_Stadium", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_ACCEPT_STADIUM", "SFXFE_Accept_Stadium", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_STADIUM_LETTER_HIT", "SFXFE_Stadium_Letter_Hit", 0.8f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_STADIUM_LETTER_FLY_IN", "SFXFE_Stadium_Letter_Fly_In", 0.6f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_STINGER_SFX_01", "SFXFE_Stinger_SFX_01", 0.5f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_STINGER_SFX_02", "SFXFE_Stinger_SFX_02", 0.5f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_STINGER_SFX_03", "SFXFE_Stinger_SFX_03", 0.5f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_STINGER_SFX_04", "SFXFE_Stinger_SFX_04", 0.8f, 0.0f, 0.0f, 3, 0 },
          { "WORLDSFX_FE_ACCEPT_MARIO", "SFXFE_Accept_Mario", 0.65f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_WARIO", "SFXFE_Accept_Wario", 0.65f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_LUIGI", "SFXFE_Accept_Luigi", 0.65f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_WALUIGI", "SFXFE_Accept_Waluigi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_PEACH", "SFXFE_Accept_Peach", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_DAISY", "SFXFE_Accept_Daisy", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_DK", "SFXFE_Accept_DK", 0.65f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_YOSHI", "SFXFE_Accept_Yoshi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_TOAD", "SFXFE_Accept_Toad", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_KOOPA", "SFXFE_Accept_Koopa", 0.5f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_BIRDO", "SFXFE_Accept_Birdo", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_HAM", "SFXFE_Accept_Ham", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_ACCEPT_MYSTERY", "SFXFE_Accept_Mystery", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_MARIO", "SFXFE_NLG_Mario", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_WARIO", "SFXFE_NLG_Wario", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_LUIGI", "SFXFE_NLG_Luigi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_WALUIGI", "SFXFE_NLG_Waluigi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_PEACH", "SFXFE_NLG_Peach", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_DAISY", "SFXFE_NLG_Daisy", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_DK", "SFXFE_NLG_DK", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_YOSHI", "SFXFE_NLG_Yoshi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_TOAD", "SFXFE_NLG_Toad", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_KOOPA", "SFXFE_NLG_Koopa", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_BIRDO", "SFXFE_NLG_Birdo", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_HAM", "SFXFE_NLG_Ham", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_CRITTER", "SFXFE_NLG_Critter", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NLG_MYSTERY", "SFXFE_NLG_Mystery", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_MARIO", "SFXFE_Nintendo_Mario", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_WARIO", "SFXFE_Nintendo_Wario", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_LUIGI", "SFXFE_Nintendo_Luigi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_WALUIGI", "SFXFE_Nintendo_Waluigi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_PEACH", "SFXFE_Nintendo_Peach", 0.65f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_DAISY", "SFXFE_Nintendo_Daisy", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_DK", "SFXFE_Nintendo_DK", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_YOSHI", "SFXFE_Nintendo_Yoshi", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_TOAD", "SFXFE_Nintendo_Toad", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_KOOPA", "SFXFE_Nintendo_Koopa", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_BIRDO", "SFXFE_Nintendo_Birdo", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_HAM", "SFXFE_Nintendo_Ham", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_CRITTER", "SFXFE_Nintendo_Critter", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_NINTENDO_MYSTERY", "SFXFE_Nintendo_Mystery", 0.7f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_MARIO", "SFXFE_Focus_Mario", 0.65f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_WARIO", "SFXFE_Focus_Wario", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_LUIGI", "SFXFE_Focus_Luigi", 0.55f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_WALUIGI", "SFXFE_Focus_Waluigi", 0.65f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_PEACH", "SFXFE_Focus_Peach", 0.45f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_DAISY", "SFXFE_Focus_Daisy", 0.45f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_DK", "SFXFE_Focus_DK", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_YOSHI", "SFXFE_Focus_Yoshi", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_FE_FOCUS_MYSTERY", "SFXFE_Focus_Mystery", 0.6f, 0.0f, 0.0f, 4, 0 },
          { "WORLDSFX_HUD_WHISTLE_PLAY_START", "SFXREF_Whistle_Play_Start", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "WORLDSFX_HUD_WHISTLE_PLAY_STOP", "SFXREF_Whistle_Play_Stop", 0.7f, 0.0f, 0.5f, 1, 0 },
          { "WORLDSFX_HUD_REPLAY_SWIPE", "SFXHUD_Replay_Screen_Swipe", 0.5f, 0.0f, 0.0f, 1, 0 },
          { "WORLDSFX_HUD_BALL_HIT_CAMERA", "SFXFE_BALL_Hit_Camera", 0.6f, 0.0f, 0.0f, 1, 0 },
          { "WORLDSFX_HUD_CLOCK_COUNT", "SFXHUD_Clock_Count", 0.6f, 0.0f, 0.5f, 1, 0 },
          { "WORLDSFX_HUD_TOGGLE_UP", "SFXHUD_Toggle", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "WORLDSFX_HUD_TOGGLE_DOWN", "SFXHUD_Toggle", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "WORLDSFX_HUD_ACCEPT", "SFXHUD_Accept", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "WORLDSFX_FILTER_START", "SFXFILTER_Start", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "WORLDSFX_FILTER_END", "SFXFILTER_End", 0.5f, 0.0f, 0.5f, 1, 0 },
          { "REPLAYSFX_CAMERA_ZOOM_IN", "SFXREPLAY_CAM_Zoom_In", 0.8f, 0.0f, 0.0f, 1, 0 },
          { "REPLAYSFX_CAMERA_ZOOM_OUT", "SFXREPLAY_CAM_Zoom_Out", 0.8f, 0.0f, 0.0f, 1, 0 },
          { "REPLAYSFX_CAMERA_SPEED_UP", "SFXREPLAY_Speed_Up", 1.0f, 0.0f, 0.0f, 1, 0 },
          { "REPLAYSFX_CAMERA_SLOW_DOWN", "SFXREPLAY_Slow_Down", 1.0f, 0.0f, 0.0f, 1, 0 },
          { "NISSFX_STOS_GEN_HAMMER", "SFXBALL_StoS_Capt", 1.0f, 0.0f, 0.5f, 1, 0 } };

WORLDSoundPropAccessor gWORLDSoundPropAccessor;
SoundPropAccessor* gpWORLDSoundPropAccessor = &gWORLDSoundPropAccessor;

/**
 * Offset/Address/Size: 0xF0 | 0x801B38B4 | size: 0x10
 */
void WORLDSoundPropAccessor::ResetSoundPropTable()
{
    m_pTable = WORLDSoundProperties;
}
/**
 * Offset/Address/Size: 0xD4 | 0x801B3898 | size: 0x1C
 */
bool WORLDSoundPropAccessor::IsUsingOrigTable() const
{
    return (m_pTable == WORLDSoundProperties) ? true : false;
}

/**
 * Offset/Address/Size: 0xC8 | 0x801B388C | size: 0xC
 */
const char* WORLDSoundPropAccessor::GetHTMLFileName() const
{
    return "worldsoundproperties.htm";
}
/**
 * Offset/Address/Size: 0xC0 | 0x801B3884 | size: 0x8
 */
const char* WORLDSoundPropAccessor::GetSoundPropTableName() const
{
    return "WORLD";
}
/**
 * Offset/Address/Size: 0xB8 | 0x801B387C | size: 0x8
 */
u32 WORLDSoundPropAccessor::GetNumSFX() const
{
    // TU returns immediate 0x5C
    return sizeof(WORLDSoundProperties) / sizeof(SoundProperties);
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B3874 | size: 0x8
 */
SoundProperties* WORLDSoundPropAccessor::GetSoundPropTable()
{
    return m_pTable;
}

/**
 * Offset/Address/Size: 0x50 | 0x801B3814 | size: 0x60
 */
SoundProperties* WORLDSoundPropAccessor::GetSoundProperty(unsigned int index) const
{
    u32 n = GetNumSFX();
    if (index >= n)
        return 0;            // matches li r3,0 path
    return &m_pTable[index]; // add mulli index, 0x1C
}
