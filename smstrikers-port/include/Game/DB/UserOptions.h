#ifndef _USEROPTIONS_H_
#define _USEROPTIONS_H_

#include "NL/nlConfig.h"

enum eAudioMode
{
    INVALID = -1,
    STEREO = 0,
    MONO = 1,
    DOLBY = 2,
};

class VisualSettings
{
public:
    VisualSettings();
    void InitializeDefaults();

    /* 0x0 */ bool mIsAutoZoomCamera;
    /* 0x4 */ float mCameraZoomLevel;
    /* 0x8 */ bool mIsWidescreen;
}; // total size: 0xC

class CheatSettings
{
public:
    enum CustomPowerups
    {
        CP_INVALID = -1,
        CP_OFF = 0,
        CP_EXPLOSIVE = 1,
        CP_FREEZING = 2,
        CP_SHELLS = 3,
        CP_GIANT = 4,
        CP_ENCHANCEMENT = 5,
        CP_LAST = 6,
    };

    CheatSettings();
    void InitializeDefaults();
    void OnSettingsUpdated() const;

    /* 0x0 */ CustomPowerups mCustomPowerups;
    /* 0x4 */ bool mStunnedGoalies;
    /* 0x5 */ bool mInfinitePowerups;
    /* 0x6 */ bool mCheatTBD1Enabled;
    /* 0x7 */ bool mCheatTBD2Enabled;
}; // total size: 0x8

typedef CheatSettings::CustomPowerups CustomPowerups;

enum
{
    CP_INVALID = CheatSettings::CP_INVALID,
    CP_OFF = CheatSettings::CP_OFF,
    CP_EXPLOSIVE = CheatSettings::CP_EXPLOSIVE,
    CP_FREEZING = CheatSettings::CP_FREEZING,
    CP_SHELLS = CheatSettings::CP_SHELLS,
    CP_GIANT = CheatSettings::CP_GIANT,
    CP_ENCHANCEMENT = CheatSettings::CP_ENCHANCEMENT,
    CP_LAST = CheatSettings::CP_LAST,
};

class PowerupSettings
{
public:
    PowerupSettings();
    void InitializeDefaults();
    void OnSettingsUpdated() const;

    /* 0x0 */ bool RedShells;
    /* 0x1 */ bool GreenShells;
    /* 0x2 */ bool BlueShells;
    /* 0x3 */ bool SpinyShells;
    /* 0x4 */ bool Starman;
    /* 0x5 */ bool Twister;
    /* 0x6 */ bool Bobombs;
    /* 0x7 */ bool Bananas;
}; // total size: 0x8

class GameplaySettings
{
public:
    enum eSkillLevel
    {
        TRAINING = 0,
        ROOKIE = 1,
        PROFESSIONAL = 2,
        SUPERSTAR = 3,
        LEGEND = 4,
    };

    GameplaySettings();
    void InitializeDefaults();
    void OnSettingsUpdated() const;

    /* 0x0 */ eSkillLevel SkillLevel;
    /* 0x4 */ s32 GameTime;
    /* 0x8 */ bool PowerUps;
    /* 0x9 */ bool Shoot2Score;
    /* 0xA */ bool BowserAttackEnabled;
    /* 0xB */ bool RumbleEnabled;
}; // total size: 0xC

class AudioSettings
{
public:
    AudioSettings();
    void InitializeDefaults();
    void ForceApplySettings(bool bUpdateMode);
    void ApplySettings(bool bApplyMode, bool bUpdateMode);

    /* 0x00 */ int MusicVolume;
    /* 0x04 */ int SFXVolume;
    /* 0x08 */ int VoiceVolume;
    /* 0x0C */ eAudioMode Mode;
    /* 0x10 */ int DefaultMusicVolume;
    /* 0x14 */ int DefaultSFXVolume;
    /* 0x18 */ int DefaultVoiceVolume;
    /* 0x1C */ eAudioMode DefaultMode;
}; // total size: 0x20

#endif // _USEROPTIONS_H_
