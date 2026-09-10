#pragma pool_data off

#include "Game/CharacterTweaks.h"

#include "NL/nlLexicalCast.h"

#include "NL/nlConfig.h"

f32 g_pTweaks[0x17] = {
    4000.0f,
    25.0f,
    45.0f,
    9.5f,
    4000.0f,
    17.5f,
    17.5f,
    2600.0f,
    22.5f,
    18.0f,
    3200.0f,
    25.0f,
    18.0f,
    10.0f,
    2600.0f,
    20.0f,
    20.0f,
    5.5f,
    22.5f,
    14.0f,
    75000.0f,
    4000.0f,
    6.0f,
};

/**
 * Offset/Address/Size: 0x342C | 0x80017E1C | size: 0x60
 */
PlayerTweaks::~PlayerTweaks()
{
}

/**
 * Offset/Address/Size: 0x2E9C | 0x8001788C | size: 0x590
 */
void PlayerTweaks::Init()
{
    Config cfg(Config::ALLOCATE_HIGH);
    cfg.LoadFromFile(mszFileName);

    fJoggingSpeed = cfg.Get<float>("Jog Speed", 1.6f);
    fRunningSpeed = cfg.Get<float>("Running Speed", 6.25f);
    fPhysCapsuleHeight = cfg.Get<float>("Capsule Height", 0.5f);
    fPhysCapsuleRadius = cfg.Get<float>("Capsule Width", 0.3f);
    fRunningDirectionSeekSpeed = cfg.Get<float>("Running Turn Speed", 105000.0f);
    fRunningDirectionSeekFalloff = g_pTweaks[0];
    fPassGroundSpeedMax = cfg.Get<float>("Pass Ground Speed Max", 16.0f);
    fPassGroundSpeedMin = cfg.Get<float>("Pass Ground Speed Min", 5.5f);
    fPassVolleySpeedMax = cfg.Get<float>("Pass Volley Speed Max", 14.0f);
    fPassVolleySpeedMin = cfg.Get<float>("Pass Volley Speed Min", 5.5f);
}

/**
 * Offset/Address/Size: 0x2E1C | 0x8001780C | size: 0x80
 */
FielderTweaks::FielderTweaks(const char* name)
    : PlayerTweaks(name)
{
    Init();
}

/**
 * Offset/Address/Size: 0x2DAC | 0x8001779C | size: 0x70
 */
FielderTweaks::~FielderTweaks()
{
}

/**
 * Offset/Address/Size: 0x19F4 | 0x800163E4 | size: 0x13B8
 */
void FielderTweaks::Init()
{
    PlayerTweaks::Init();

    Config cfg(Config::ALLOCATE_HIGH);
    cfg.LoadFromFile(mszFileName);

    fRunningAccel = g_pTweaks[1];
    fRunningDecel = g_pTweaks[2];
    fRunningStopDecel = g_pTweaks[3];
    fRunningStrafeDirectionSeekSpeed = cfg.Get<float>("Strafe Turn Speed", 140000.0f);
    fRunningStrafeDirectionSeekFalloff = g_pTweaks[4];
    fRunningStrafeSpeed = cfg.Get<float>("Strafe Speed", 3.5f);
    fRunningBackwardsSpeed = cfg.Get<float>("Run Backwards Speed", 4.0f);
    fRunningStrafeAccel = g_pTweaks[5];
    fRunningStrafeDecel = g_pTweaks[6];
    fRunningTurboDirectionSeekSpeed = cfg.Get<float>("Turbo Turn Speed", 65000.0f);
    fRunningTurboDirectionSeekFalloff = g_pTweaks[7];
    fRunningTurboSpeed = cfg.Get<float>("Turbo Speed", 7.5f);
    fRunningTurboAccel = g_pTweaks[8];
    fRunningTurboDecel = g_pTweaks[9];
    fRunningWBDirectionSeekSpeed = cfg.Get<float>("WB Run Turn Speed", 90000.0f);
    fRunningWBDirectionSeekFalloff = g_pTweaks[10];
    fRunningWBSpeed = cfg.Get<float>("WB Run Speed", 6.0f);
    fRunningWBAccel = g_pTweaks[11];
    fRunningWBDecel = g_pTweaks[12];
    fRunningWBStopDecel = g_pTweaks[13];
    fRunningWBTurboDirectionSeekSpeed = cfg.Get<float>("WB Run Turbo Turn Speed", 70000.0f);
    fRunningWBTurboDirectionSeekFalloff = g_pTweaks[14];
    fRunningWBTurboSpeedLevel1 = cfg.Get<float>("WB Run Turbo Level 1 Speed", 6.5f);

    fRunningWBTurboSpeedLevel2 = cfg.Get<float>("WB Run Turbo Level 2 Speed", 7.0f);
    fRunningWBTurboSpeedLevel3 = cfg.Get<float>("WB Run Turbo Level 3 Speed", 7.5f);

    fRunningWBTurboAccel = g_pTweaks[15];
    fRunningWBTurboDecel = g_pTweaks[16];
    fRunningWBTurboTurnSpeed = g_pTweaks[17];
    fRunningWBTurboTurnAccel = g_pTweaks[18];
    fRunningWBTurboTurnDecel = g_pTweaks[19];

    fShotWindupDirectionSeekSpeed = g_pTweaks[20];
    fShotWindupDirectionSeekFalloff = g_pTweaks[21];
    fShotWindupDecel = g_pTweaks[22];

    fCaptainS2SNisBeginFrame = cfg.Get<float>("S2S Nis Begin Frame", 9.0f);
    fCaptainS2SNisEndFrame = cfg.Get<float>("S2S Nis End Frame", 9.0f);
    fS2SKickFrame = cfg.Get<float>("S2S Kick Frame", 9.0f);
    fS2S1stJumpFrame = cfg.Get<float>("S2S 1st Jump Frame", 9.0f);

    nChanceForRedShell = cfg.Get<int>("Red Shell Chance", 0x17);
    nChanceForGreenShell = cfg.Get<int>("Green Shell Chance", 0x17);
    nChanceForSpinyShell = cfg.Get<int>("Spiny Shell Chance", 0x17);
    nChanceForFreezeShell = cfg.Get<int>("Freeze Shell Chance", 0x17);
    nChanceForMushroom = cfg.Get<int>("Mushroom Chance", 0x17);
    nChanceForBanana = cfg.Get<int>("Banana Chance", 0x17);
    nChanceForBoBomb = cfg.Get<int>("Bobomb Chance", 0x17);

    fChanceForBig = cfg.Get<float>("Big Chance", 23.0f);
    fChanceForMultiples = cfg.Get<float>("Multiples Chance", 23.0f);

    fAggression = cfg.Get<float>("Aggression", 0.5f);
    fShooting = cfg.Get<float>("Shooting", 0.5f);
    fPassing = cfg.Get<float>("Passing", 0.5f);
    fDekeing = cfg.Get<float>("Dekeing", 0.5f);

    fShotMinSpeed = cfg.Get<float>("Shot Min Speed", 0.0f);
    fShotMaxSpeed = cfg.Get<float>("Shot Max Speed", 0.0f);
    fShotChipMinSpeed = cfg.Get<float>("Chip Shot Min Speed", 0.0f);
    fShotChipMaxSpeed = cfg.Get<float>("Chip Shot Max Speed", 0.0f);
}

/**
 * Offset/Address/Size: 0x1974 | 0x80016364 | size: 0x80
 */
GoalieTweaks::GoalieTweaks(const char* name)
    : PlayerTweaks(name)
{
    Init();
}

/**
 * Offset/Address/Size: 0x1904 | 0x800162F4 | size: 0x70
 */
GoalieTweaks::~GoalieTweaks()
{
}

inline float max_float(float a, float b)
{
    return (a >= b) ? a : b;
}

/**
 * Offset/Address/Size: 0x0 | 0x800149F0 | size: 0x1904
 */
void GoalieTweaks::Init()
{
    PlayerTweaks::Init();

    Config cfg(Config::ALLOCATE_HIGH);
    cfg.LoadFromFile(mszFileName);

    fThrowingDirectionSeekSpeed = cfg.Get<float>("Throwing Turn Speed", 50000.0f);
    fThrowingDirectionSeekFalloff = cfg.Get<float>("Throwing Turn falloff", 4000.0f);
    fKickDistanceMin = cfg.Get<float>("Minimum Kick Distance", 13.0f);
    fOverhandThrowDistanceMin = cfg.Get<float>("Minimum Overhand Throw Distance", 7.0f);
    fKickVelocityMin = cfg.Get<float>("Minimum Kick Velocity", 12.0f);
    fKickVelocityMax = cfg.Get<float>("Maximum Kick Velocity", 16.0f);
    fKickAngleMin = cfg.Get<float>("Minimum Kick Angle", 50.0f);
    fKickAngleMax = cfg.Get<float>("Maximum Kick Angle", 60.0f);
    fFatigueRecoverRate = cfg.Get<float>("Fatigue Recover Rate", 2.0f);
    fFatigueCatchThreshold = cfg.Get<float>("Fatigue Catch Threshold", 60.0f);
    fCatchSaveMaxSpeed = cfg.Get<float>("Catch Save Max Speed", 0.5f);
    fGetupEnergyHigh = cfg.Get<float>("High Getup Energy", 80.0f);
    fGetupEnergyLow = cfg.Get<float>("Low Getup Energy", 20.0f);
    fGetupSpeedLow = cfg.Get<float>("Low Getup Speed", 0.5f);
    fStrafeSpeedLow = cfg.Get<float>("Low Strafe Speed", 0.8f);
    fGoalieBallTime = cfg.Get<float>("Goalie Ball Time", 5.0f);
    fGoalieStunTimeMin = cfg.Get<float>("Min STS Stun Time", 1.5f);
    fGoalieStunTimeMax = cfg.Get<float>("Max STS Stun Time", 3.0f);
    fLooseBallShotDistance = cfg.Get<float>("Loose Ball Shot Distance", 10.0f);
    fSaveDirectionSeekSpeed = cfg.Get<float>("Save Turn Speed", 100000.0f);
    fSaveDirectionSeekFalloff = cfg.Get<float>("Save Turn Falloff", 4000.0f);
    fSaveBackRunTimeScale = cfg.Get<float>("Save Back Run Scale", 1.5f);
    fSaveIgnoreMargin = cfg.Get<float>("Save Ignore Margin", 1.0f);
    fSaveMissDelay = cfg.Get<float>("Miss Save Delay", 0.1f);
    fLobShotStumbleChance = cfg.Get<float>("Lob Shot Stumble Chance", 0.3f);
    fInterceptSaveTolerance = cfg.Get<float>("Intercept Save Tolerance", 0.5f);
    fSaveCatchTolerance = cfg.Get<float>("Catch Save Tolerance", 0.7f);
    fShotFatigueDefault = cfg.Get<float>("Shot Fatigue Default", 10.0f);
    fShotFatigueStandCatch = cfg.Get<float>("Shot Fatigue Stand Catch", 5.0f);
    fShotFatigueDiveCatch = cfg.Get<float>("Shot Fatigue Dive Catch", 10.0f);
    fShotFatigueStandDeflect = cfg.Get<float>("Shot Fatigue Stand Deflection", 5.0f);
    fShotFatigueDiveDeflect = cfg.Get<float>("Shot Fatigue Dive Deflection", 10.0f);
    fShotFatigueStandPunch = cfg.Get<float>("Shot Fatigue Stand Punch", 7.0f);
    fShotFatigueLegSave = cfg.Get<float>("Shot Fatigue Leg Save", 6.0f);
    fShotFatigueSTSSave = cfg.Get<float>("Shot Fatigue STS Save", 15.0f);
    fShotFatigueSTSStun = cfg.Get<float>("Shot Fatigue STS Stun", 20.0f);

    // Find the maximum fatigue value
    fShotFatigueMax = max_float(fShotFatigueDefault, fShotFatigueStandCatch);
    fShotFatigueMax = max_float(fShotFatigueMax, fShotFatigueDiveCatch);
    fShotFatigueMax = max_float(fShotFatigueMax, fShotFatigueStandDeflect);
    fShotFatigueMax = max_float(fShotFatigueMax, fShotFatigueDiveDeflect);
    fShotFatigueMax = max_float(fShotFatigueMax, fShotFatigueStandPunch);
    fShotFatigueMax = max_float(fShotFatigueMax, fShotFatigueLegSave);
    fShotFatigueMax = max_float(fShotFatigueMax, fShotFatigueSTSSave);
    fShotFatigueMax = max_float(fShotFatigueMax, fShotFatigueSTSStun);

    fSTSAttackCloseDistance = cfg.Get<float>("STS Attack Close Distance", 1.0f);
    fSTSAttackMaxDistance = cfg.Get<float>("STS Attack Max Distance", 7.0f);
    fSTSAttackChancePerFrame = cfg.Get<float>("STS Attack Chance Per Frame", 0.01f);
    fSTSAttackBallVelMult = cfg.Get<float>("STS Attack Ball Speed Multiplier", 0.3f);
    fPounceRange = cfg.Get<float>("Pounce Range", 1.0f);
}
