#include "types.h"
#include "Game/GameTweaks.h"

#include "NL/nlString.h"
#include "NL/nlConfig.h"
#include "NL/nlMemory.h"
#include "NL/nlPrint.h"

#include "Game/GameInfo.h"

SkillTweaks g_sSkillTweaks[2];
const char* g_sDifficultyFileNames[7] = {
    "DifficultyBraindead.ini",
    "Difficulty0.ini",
    "Difficulty1.ini",
    "Difficulty2.ini",
    "Difficulty3.ini",
    "DifficultySuperhuman.ini",
    "DifficultyHuman.ini",
};

extern char* GetPowerupName(int powerup);

/**
 * Offset/Address/Size: 0x2638 | 0x80042824 | size: 0x50
 */
GameTweaks::GameTweaks(const char* name)
    : TweaksBase(name)
{
    Init();
}

/**
 * Offset/Address/Size: 0x122C | 0x80041418 | size: 0x140C
 */
void GameTweaks::Init()
{
    Config cfg(Config::ALLOCATE_HIGH);
    cfg.LoadFromFile(mszFileName);

    const GameplaySettings& gameSettings = GameInfoManager::Instance()->GetGameplayOptions();

    if (Config::Global().Exists("Game Duration"))
    {
        fGameDuration = Config::Global().Get<float>("Game Duration", 0.0f);
    }
    else
    {
        fGameDuration = gameSettings.GameTime;
    }

    fBlockPassLightAttackSpeed = cfg.Get<float>("Block Ball with LAttack under this Speed", 5.0f);
    fFielderAttributeWeight = cfg.Get<float>("Fielder Attributes Weight", 0.2f);
    vGetInPositionKeyFielderDist.x = cfg.Get<float>("GetInPosition KeyFielder Min Distance", 2.0f);
    vGetInPositionKeyFielderDist.y = cfg.Get<float>("GetInPosition KeyFielder Max Distance", 12.0f);
    vGetInPositionInRadius.x = cfg.Get<float>("At Target Position Min Radius", 0.3f);
    vGetInPositionInRadius.y = cfg.Get<float>("At Target Position Max Radius", 0.6f);
    vGetInPositionOutRadius.x = cfg.Get<float>("At Target Remain At Position Min Radius", 1.0f);
    vGetInPositionOutRadius.y = cfg.Get<float>("At Target Remain At Position Max Radius", 4.5f);
    nStrafeToRunInDirectionDelta = (s16)cfg.Get<int>("Run To Strafe Angle", 0x2CEC);
    nBackwardsToStrafeRunInDirectionDelta = (s16)cfg.Get<int>("Backwards To Strafe Angle", 0x59D8);
    nStrafeToRunOutDirectionDelta = (s16)cfg.Get<int>("Strafe To Run Angle", 0x2328);
    nBackwardsToStrafeRunOutDirectionDelta = (s16)cfg.Get<int>("Strafe To Backwards Angle", 0x61A8);
    fSlideAttackRadius = cfg.Get<float>("Do Slide Attack Radius", 2.15f);
    fArrivalInRadius = cfg.Get<float>("Arrived In Radius", 0.45f);
    fArrivalOutRadius = cfg.Get<float>("Arrived Out Radius", 0.85f);
    fNearSeekInRadius = cfg.Get<float>("Near In Radius", 1.55f);
    fNearSeekOutRadius = cfg.Get<float>("Near Out Radius", 2.0f);
    fPassSpeedMaxDist = cfg.Get<float>("Pass Speed Max Dist", 7.5f);
    fPassSpeedMinDist = cfg.Get<float>("Pass Speed Min Dist", 1.0f);
    fSwapControllerTime = cfg.Get<float>("Swap Controller No Swap Back Time", 0.3f);
    fSwapFacingTime = cfg.Get<float>("Swap Controller Dampen Facing Time", 0.5f);
    fGreenShellActiveTime = cfg.Get<float>("Green Shell Time", 2.0f);
    fGreenShellSpeed = cfg.Get<float>("Green Shell Speed", 18.0f);
    fRedShellActiveTime = cfg.Get<float>("Red Shell Time", 1.5f);
    fRedShellSpeed = cfg.Get<float>("Red Shell Speed", 12.5f);
    fSpinyShellActiveTime = cfg.Get<float>("Spiny Shell Time", 1.5f);
    fSpinyShellSpeed = cfg.Get<float>("Spiny Shell Speed", 12.5f);
    fFreezeShellActiveTime = cfg.Get<float>("Freeze Shell Time", 1.5f);
    fFreezeShellSpeed = cfg.Get<float>("Freeze Shell Speed", 12.5f);
    fFreezeShellFrozenTime = cfg.Get<float>("Freeze Shell Frozen Time", 1.5f);
    fBananaActiveTime = cfg.Get<float>("Banana Time", 5.0f);
    fBananaSpeed = cfg.Get<float>("Banana Speed", 8.5f);
    fBobombActiveTime = cfg.Get<float>("Bobomb Time", 8.5f);
    fBobombSpeed = cfg.Get<float>("Bobomb Speed", 8.5f);
    fBobombMaxZSpeed = cfg.Get<float>("Bobomb Max Z Speed", 8.5f);
    fChainChompActiveTime = cfg.Get<float>("ChainChomp Time", 8.5f);
    fChainChompFallTime = cfg.Get<float>("ChainChomp Fall Time", 8.5f);
    fChainChompSpeed = cfg.Get<float>("ChainChomp Speed", 8.5f);
    nChanceForRedShell = cfg.Get<int>("Red Shell Chance", 0);
    nChanceForGreenShell = cfg.Get<int>("Green Shell Chance", 0);
    nChanceForSpinyShell = cfg.Get<int>("Spiny Shell Chance", 0);
    nChanceForFreezeShell = cfg.Get<int>("Freeze Shell Chance", 0);
    nChanceForMushroom = cfg.Get<int>("Mushroom Chance", 0);
    nChanceForBanana = cfg.Get<int>("Banana Chance", 0);
    nChanceForStar = cfg.Get<int>("Star Chance", 0);
    nChanceForBoBomb = cfg.Get<int>("BoBomb Chance", 0);
    nChanceForChainChomp = cfg.Get<int>("Chain Chomp Chance", 0);

    const PowerupSettings& powerupSettings = GameInfoManager::Instance()->GetPowerupOptions();

    if ((u32)powerupSettings.RedShells == 0)
    {
        nChanceForRedShell = 0;
    }
    if ((u32)powerupSettings.GreenShells == 0)
    {
        nChanceForGreenShell = 0;
    }
    if ((u32)powerupSettings.BlueShells == 0)
    {
        nChanceForFreezeShell = 0;
    }
    if ((u32)powerupSettings.SpinyShells == 0)
    {
        nChanceForSpinyShell = 0;
    }
    if ((u32)powerupSettings.Starman == 0)
    {
        nChanceForStar = 0;
    }
    if ((u32)powerupSettings.Twister == 0)
    {
        nChanceForRedShell = 0;
    }
    if ((u32)powerupSettings.Bobombs == 0)
    {
        nChanceForBoBomb = 0;
    }
    if ((u32)powerupSettings.Bananas == 0)
    {
        nChanceForBanana = 0;
    }

    nScoreDifferenceMaximum = cfg.Get<int>("Score Difference Maximum", 1);
    nScoreDifferenceMinimum = cfg.Get<int>("Score Difference Minimum", 5);
    nPowerupsNumForPenalty = cfg.Get<int>("Powerups Awarded Penalty", 2);
    fBananaResistance = cfg.Get<float>("Banana Resistance", 5.0f);
    fShellBounceGround = cfg.Get<float>("Shell Bounce Ground", 0.0f);
    fShellBounce = cfg.Get<float>("Shell Bounce Other", 1.0f);
    fShellSmallRadius = cfg.Get<float>("Shell Small Radius", 0.3f);
    fShellMediumRadius = cfg.Get<float>("Shell Medium Radius", 0.6f);
    fShellBigRadius = cfg.Get<float>("Shell Big Radius", 1.0f);
    fShellMediumChance = cfg.Get<float>("Shell Medium Chance", 0.33f);
    fShellBigChance = cfg.Get<float>("Shell Big Chance", 0.33f);
    fShellFiveChance = cfg.Get<float>("Shell Five Chance", 0.33f);
    fShellThreeChance = cfg.Get<float>("Shell Three Chance", 0.33f);
    fShellExplodeChance = cfg.Get<float>("Shell Explode Chance", 0.33f);
    fBananaSmallRadius = cfg.Get<float>("Banana Small Radius", 0.3f);
    fBananaMediumRadius = cfg.Get<float>("Banana Medium Radius", 0.6f);
    fBananaBigRadius = cfg.Get<float>("Banana Big Radius", 1.0f);
    fBananaMediumChance = cfg.Get<float>("Banana Medium Chance", 0.33f);
    fBananaBigChance = cfg.Get<float>("Banana Big Chance", 0.33f);
    fBananaFiveChance = cfg.Get<float>("Banana Five Chance", 0.33f);
    fBananaThreeChance = cfg.Get<float>("Banana Three Chance", 0.33f);
    fBananaExplodeChance = cfg.Get<float>("Banana Explode Chance", 0.33f);
    fBobombSmallRadius = cfg.Get<float>("Bobomb Small Radius", 0.3f);
    fBobombMediumRadius = cfg.Get<float>("Bobomb Medium Radius", 0.6f);
    fBobombBigRadius = cfg.Get<float>("Bobomb Big Radius", 1.0f);
    fBobombMediumChance = cfg.Get<float>("Bobomb Medium Chance", 0.33f);
    fBobombBigChance = cfg.Get<float>("Bobomb Big Chance", 0.33f);
    fBobombFiveChance = cfg.Get<float>("Bobomb Five Chance", 0.33f);
    fBobombThreeChance = cfg.Get<float>("Bobomb Three Chance", 0.33f);
    fBobombMineChance = cfg.Get<float>("Bobomb Mine Chance", 0.33f);
    fPowerupExplosionRadius = cfg.Get<float>("Powerup Explosion Radius", 2.0f);
    fPowerupArrowThrowChance = cfg.Get<float>("Powerup Arrow Throw Chance", 0.3f);
    fPowerupSpreadThrowChance = cfg.Get<float>("Powerup Spread Throw Chance", 0.3f);
    fPowerupSurroundThrowChance = cfg.Get<float>("Powerup Surround Throw Chance", 0.3f);
    fPowerupHorizontalLineThrowChance = cfg.Get<float>("Powerup H Line Throw Chance", 0.3f);
    fPowerupHitWithBallMinAmount = cfg.Get<float>("Powerup Hit With Ball Min", 0.1f);
    fPowerupHitWithBallMaxAmount = cfg.Get<float>("Powerup Hit With Ball Max", 0.15f);
    fPowerupHitNoBallMinAmount = cfg.Get<float>("Powerup Hit No Ball Min", 0.25f);
    fPowerupHitNoBallMaxAmount = cfg.Get<float>("Powerup Hit No Ball Max", 0.35f);
    fPowerupSlideWithBallMinAmount = cfg.Get<float>("Powerup Slide With Ball Min", 0.3f);
    fPowerupSlideWithBallMaxAmount = cfg.Get<float>("Powerup Slide With Ball Max", 0.3f);
    fPowerupSlideNoBallMinAmount = cfg.Get<float>("Powerup Slide No Ball Min", 0.3f);
    fPowerupSlideNoBallMaxAmount = cfg.Get<float>("Powerup Slide No Ball Max", 0.3f);
    fPowerupPowerShotMinAmount = cfg.Get<float>("Powerup Power Shot Min", 0.3f);
    fPowerupPowerShotMaxAmount = cfg.Get<float>("Powerup Power Shot Max", 0.3f);
    fPowerupInterceptPassMinAmount = cfg.Get<float>("Powerup Intercept Pass Min", 0.3f);
    fPowerupInterceptPassMaxAmount = cfg.Get<float>("Powerup Intercept Pass Max", 0.3f);
    fPowerupPerfectPassMinAmount = cfg.Get<float>("Powerup Perfect Pass Min", 0.3f);
    fPowerupPerfectPassMaxAmount = cfg.Get<float>("Powerup Perfect Pass Max", 0.3f);
    fPowerupContextDekeMinAmount = cfg.Get<float>("Powerup Context Deke Min", 0.3f);
    fPowerupContextDekeMaxAmount = cfg.Get<float>("Powerup Context Deke Max", 0.3f);
    fPowerupIconSpeed = cfg.Get<float>("Powerup Icon Speed", 0.2f);
    nPowerupBoxMaxActiveOnField = cfg.Get<int>("Powerup Box Max Amount", 2);
    fPowerupBoxDelayDropTime = cfg.Get<float>("Powerup Box Delay Drop Time", 10.0f);
    fChainChompRadius = cfg.Get<float>("Chain Chomp Radius", 0.3f);
    fDrawPowerupIconTime = cfg.Get<float>("Draw Powerup Icon Time", 1.5f);
    fHitCarrierWeighting = cfg.Get<float>("Hit Carrier Bonus", 1.0f);
    fAngleWeighting = cfg.Get<float>("Powerup Angle Weighting", 0.0025f);
    fDekeAngleWeighting = cfg.Get<float>("Deke Angle Weighting", 0.001f);
    fPassAngleWeighting = cfg.Get<float>("Pass Angle Weighting", 0.0025f);
    fVolleyPassAngleWeighting = cfg.Get<float>("Volley Pass Angle Weighting", 0.0025f);
    fVolleyPassMinDistance = cfg.Get<float>("Volley Pass Min Distance", 3.0f);
    fMushroomEffectTime = cfg.Get<float>("On Mushrooms Time", 1.0f);
    fMushroomSpeed = cfg.Get<float>("On Mushrooms Speed", 1.25f);
    fStarEffectTime = cfg.Get<float>("On Star Time", 1.25f);
    fStarSpeed = cfg.Get<float>("On Star Speed", 1.5f);

    fFrontAudibleSurrDist = cfg.Get<float>("Front Audible Surr Dist", 100.0f);
    fBackAudibleSurrDist = cfg.Get<float>("Back Audible Surr Dist", 100.0f);
    fSpeedOfSoundForDoppler = cfg.Get<float>("Speed Of Sound For Doppler", 200.0f);
    fEmitterDistFromListenerMaxVol = cfg.Get<float>("Emitter Dist From Listener Max Vol", 0.0f);
    fMaxAudibleEmitterDistance = cfg.Get<float>("Max Aubible Emitter Distance", 100.0f);
    fEmitterVolToDistanceValue = cfg.Get<float>("Emitter Vol To Distance Value", 0.0f);
    fFadeFilterSlowMoInTime = cfg.Get<float>("Slow Mo Filter Fade In Time", 0.3f);
    fFadeFilterSlowMoOutTime = cfg.Get<float>("Slow Mo Filter Fade Out Time", 0.2f);
    fFadeFilterFreqMin = cfg.Get<float>("Filter Min Freq Pct", 0.0f);
    fFadeFilterFreqMax = cfg.Get<float>("Filter Max Freq Pct", 1.0f);
    fFadeFilterDropoffDelayTime = cfg.Get<float>("Filter Dropoff Delay Time", 0.5f);
    fFadeFilterDropoffTime = cfg.Get<float>("Filter Dropoff Time", 6.0f);
    fFadePitchMin = cfg.Get<float>("Dialogue Pitch Min Pct", 0.0f);
    fFadePitchMax = cfg.Get<float>("Dialogue Pitch Max Pct", 1.0f);
    fMinBobombMoveSFXTime = cfg.Get<float>("Min Bobomb Move SFX Activation Time", 2.5f);
    fPerfectPassProximityFilterDistSq = cfg.Get<float>("Perfect Pass SFX Proximity Filter Dist Sq", 16.0f);
    fPowerupSmallSizeVolCoeff = cfg.Get<float>("Small Powerup Volume Coefficient", 0.5f);
    fPowerupMedSizeVolCoeff = cfg.Get<float>("Medium Powerup Volume Coefficient", 0.75f);
    fPowerupLargeSizeVolCoeff = cfg.Get<float>("Large Powerup Volume Coefficient", 1.0f);
    fFadePerfectPassTrailSFXStartTime = cfg.Get<float>("Perfect Pass Trail SFX Fade Start Time", 0.5f);
    fMinHitIntensityForHardBodyHitSFX = cfg.Get<float>("Min Hit Intensity to play BODY_HIT_HARD SFX", 0.9f);
    fSlideAttackHitReactionVolume = cfg.Get<float>("Slide Attack Hit Reaction Volume", 0.75f);
    fShootToScoreBallHitReactionVolume = cfg.Get<float>("S2S Ball Hit Reaction Volume", 0.75f);
    fBombHitReactionVolume = cfg.Get<float>("Bomb Hit Reaction Volume", 0.75f);
    fBombShockwaveReactionVolume = cfg.Get<float>("Bomb Shockwave Reaction Volume", 0.75f);
    fSmallShellHitReactionVolume = cfg.Get<float>("Small Shell Hit Reaction Volume", 0.75f);
    fMediumShellHitReactionVolume = cfg.Get<float>("Medium Shell Hit Reaction Volume", 0.75f);
    fLargeShellHitReactionVolume = cfg.Get<float>("Large Shell Hit Reaction Volume", 0.75f);
    fGoalieDropKickHitReactionVolume = cfg.Get<float>("Goalie Drop Kick Hit Reaction Volume", 0.75f);
    fBallHitWallMaxAudibleVelocity = cfg.Get<float>("Ball Hit Wall Max Audible Velocity", 35.0f);
    fBallHitWallMinAudibleVelocity = cfg.Get<float>("Ball Hit Wall Min Audible Velocity", 2.0f);
    fBallHitWallMinVolume = cfg.Get<float>("Ball Hit Wall Min Volume", 0.3f);
    fBallHitWallMinTimeBeforeNextAudio = cfg.Get<float>("Ball Hit Wall Min Time Before Next Audio", 0.2f);
    fBallHitNetMaxAudibleVelocity = cfg.Get<float>("Ball Hit Net Max Audible Velocity", 35.0f);
    fBallHitNetMinAudibleVelocity = cfg.Get<float>("Ball Hit Net Min Audible Velocity", 3.0f);
    fBallHitNetMinVolume = cfg.Get<float>("Ball Hit Net Min Volume", 0.3f);
    fBallHitNetMinTimeBeforeNextAudio = cfg.Get<float>("Ball Hit Net Min Time Before Next Audio", 1.5f);
    fPerfectPassSlowMo = cfg.Get<float>("Perfect Pass Slow Mo", 0.5f);
    fShootToScoreSlowMoMin = cfg.Get<float>("S2S Slow Mo Min", 0.4f);
    fShootToScoreSlowMoMax = cfg.Get<float>("S2S Slow Mo Max", 0.6f);
    fShootToScoreBallBlurWidth = cfg.Get<float>("S2S Ball Blur Width", 0.3f);
    nShootToScoreBallBlurLength = cfg.Get<int>("S2S Ball Blur Length", 0x1E);
    fShootToScoreYellowDistance = cfg.Get<float>("S2S 2 Button Press Yellow Dist", 0.05f);
    fShootToScorePerfectFirstButtonTime = cfg.Get<float>("S2S Perfect First Button Time", 0.1f);
    fShootToScorePerfectSecondButtonTime = cfg.Get<float>("S2S Perfect Second Button Time", 0.12f);
    fShootToScorePerfectDistanceTimeMin = cfg.Get<float>("S2S Perfect Distance Min", 0.02f);
    fShootToScorePerfectDistanceTimeMax = cfg.Get<float>("S2S Perfect Distance Max", 0.02f);
    fSlideAttackTimeToSlide = cfg.Get<float>("Slide Attack Maximum Time", 0.35f);
    fSlideAttackTimeToDecelrate = cfg.Get<float>("Slide Attack Fail Up Time", 0.35f);
    fSlideAttackDeceleration = cfg.Get<float>("Slide Attack Deceleration", 0.5f);
    fLeftTriggerDownPressure = cfg.Get<float>("Left Trigger Pressure", 0.7f);
    fIndicatorDistAboveHead = cfg.Get<float>("Indicator Distance Above Head", 0.4525f);
    fIndicatorDistInPixels = cfg.Get<float>("Indicator Pixel Clearance", 6.0f);
    fClearBallGroundMinSpeed = cfg.Get<float>("Clear Ball Min Ground Speed", 0.0f);
    fClearBallGroundMaxSpeed = cfg.Get<float>("Clear Ball Max Ground Speed", 0.0f);
    fClearBallMinZSpeed = cfg.Get<float>("Clear Ball Min Z Speed", 0.0f);
    fClearBallMaxZSpeed = cfg.Get<float>("Clear Ball Max Z Speed", 0.0f);
    fShotWindupTime = cfg.Get<float>("Shot Windup Seconds", 0.75f);
    fShotMeterNetOpenAngle = cfg.Get<float>("Shot Net Open Angle", 45.0f);
    fShotMeterRatingsWeight = cfg.Get<float>("Shot Ratings Weight", 0.5f);
    fShotMeterNetOpenWeight = cfg.Get<float>("Shot Net Open Weight", 0.0f);
    fShotMeterPlayerDistanceWeight = cfg.Get<float>("Shot Player Distance Weight", 0.0f);
    fShotChipMeterGPositionWeight = cfg.Get<float>("Chip Shot Goalie Out Weight", 0.0f);
    fShotChipMeterNetOpenWeight = cfg.Get<float>("Chip Shot Net Open Weight", 0.0f);
    fShotMeterOneTimerMaxSpeed = cfg.Get<float>("Shot Meter Max Onetimer Speed", 0.0f);
    fShotPostOffset = cfg.Get<float>("Shot Offset From Post", 0.3f);
    fChipShotPostOffset = cfg.Get<float>("Chip Shot Offset From Post", 0.3f);
    fShotHighDistance = cfg.Get<float>("High Shot Distance", 18.0f);
    fShotWidthVariance = cfg.Get<float>("Shot Width Variance", 0.12f);
    fShotHeightVariance = cfg.Get<float>("Shot Height Variance", 0.06f);
    fBowserRadius = cfg.Get<float>("Bowser Radius", 1.0f);
    fBowserChance = cfg.Get<float>("Bowser Chance", 0.5f);
    fBowserStartTime = cfg.Get<float>("Bowser Start Time", 30.0f);
    fBowserEndTime = cfg.Get<float>("Bowser End Time", 60.0f);
    fBowserMinAttackTime = cfg.Get<float>("Bowser Min Attack Time", 30.0f);
    fBowserMaxAttackTime = cfg.Get<float>("Bowser Max Attack Time", 30.0f);
    fBowserTiltTime = (float)cfg.Get<int>("Bowser Tilt Time", 0xF);
    fBowserIntervalDelay = cfg.Get<float>("Bowser Interval Delay", 2.0f);
    fBowserMinTiltForce = cfg.Get<float>("Bowser Min Tilt Force", 0.1f);
    fBowserMaxTiltForce = cfg.Get<float>("Bowser Max Tilt Force", 0.2f);
    fBowserMaxTilt = cfg.Get<float>("Bowser Max Tilt", 4.0f);
    fBowserGravity = cfg.Get<float>("Bowser Gravity", -20.0f);
    fBowserRebound = cfg.Get<float>("Bowser Rebound", -0.4f);
    fBowserBallForceMult = cfg.Get<float>("Bowser Force Multiplier", 1.0f);
    fBowserBigPowerupBoost = cfg.Get<float>("Bowser Big Powerup Boost", 0.4f);
}

/**
 * Offset/Address/Size: 0x2E8 | 0x800404D4 | size: 0xF44
 */
SkillTweaks::SkillTweaks()
{
    mSkillTweaksList.m_pEnd = NULL;
    mSkillTweaksList.m_pStart = NULL;
    SkillTweak** pEnd;
    SkillTweak* node;
    int i_sit;

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_Avoidance, "Avoidance Effectiveness");
    pEnd = &mSkillTweaksList.m_pEnd;
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_DekeChance, "Deke Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_GroundPassChance, "Ground Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_VolleyPassChance, "Volley Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_ShootingChance, "Shooting Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_ChipShotChance, "Chip Shot Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_CaptainS2SChance, "Captain ShootToScore Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_GroundOneTimerChance, "Ground OneTimer Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_VolleyOneTimerChance, "Volley OneTimer Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_OneTouchGroundPassChance, "OneTouch Ground Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_OneTouchVolleyPassChance, "OneTouch Volley Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_PassReceiveHitChance, "PassReceive Hit Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_PassReceivePowerupChance, "PassReceive Powerup Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_WindupDekeChance, "Windup Deke Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_WindupPassChance, "Windup Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_WindupPowerupChance, "Windup Powerup Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_CutAndBreakChance, "Cut And Break Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_TurboChance, "Turbo With Ball Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Off_Reaction, "Reaction");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Def_BlockPassChance, "Block Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Def_BlockShotChance, "Block Shot Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Def_SlideAttackChance, "Slide Attack Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Def_HeavyAttackChance, "Heavy Attack Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Def_VolleyPassDefendChance, "Defend VolleyPass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Def_Marking, "Marking Effectiveness");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Loose_HeavyAttackChance, "Loose Heavy Attack Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Loose_ShotChance, "Loose Shot Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Loose_GroundPassChance, "Loose Ground Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Loose_VolleyPassChance, "Loose Volley Pass Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Shoot_CaptainS2SFirstButtonChance, "Captain S2S Perfect First Button Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&Shoot_CaptainS2SSecondButtonChance, "Captain S2S Perfect Second Button Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotValue1, "Shot Value 1");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotValue2, "Shot Value 2");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotValue3, "Shot Value 3");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotChance0, "Shot Chance 0");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotChance1, "Shot Chance 1");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotChance2, "Shot Chance 2");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotChance3, "Shot Chance 3");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fShotChance4, "Shot Chance 4");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fSTSWindupTime, "STS Windup Time");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fAttackCarrierDistance, "Attack Carrier Range");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fLooseBallChaseDistance, "Loose Ball Range");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fGoalieCanInterceptPass, "Goalie Intercept Pass");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fGoalieDekeChance, "Goalie Deke Chance");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&fGoalieDekeSpeed, "Goalie Deke Speed");
    nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);

    for (i_sit = 0; i_sit < 3; i_sit++)
    {
        const char* sSituationName;
        switch (i_sit)
        {
        case 1:
            sSituationName = "Defensive";
            break;
        case 0:
            sSituationName = "Offensive";
            break;
        case 2:
            sSituationName = "Loose";
            break;
        }

        for (int i_powerup = 0; i_powerup < 9; i_powerup++)
        {
            char sTweakName[0x40];
            nlSNPrintf(sTweakName, 0x3F, "%s %s Chance", sSituationName, GetPowerupName(i_powerup));

            node = new (nlMalloc(sizeof(SkillTweak), 8, false)) SkillTweak(&PowerupUsageChance[i_sit][i_powerup], sTweakName);
            nlListAddEnd<SkillTweak>(&mSkillTweaksList.m_pStart, pEnd, node);
        }
    }
}

/**
 * Offset/Address/Size: 0x264 | 0x80040450 | size: 0x84
 */
SkillTweaks::~SkillTweaks()
{
    nlDeleteList(mSkillTweaksList);
}

/**
 * Offset/Address/Size: 0x250 | 0x8004043C | size: 0x14
 */
SkillTweaks* SkillTweaks::GetSkillTweaks(int idx)
{
    return &g_sSkillTweaks[idx];
}

/**
 * Offset/Address/Size: 0x4 | 0x800401F0 | size: 0x24C
 */
void SkillTweaks::Init(eDifficultyID diff, bool blend)
{
    bool bNeedToLoadFile = true;

    nlStrNCpy<char>((char*)mszFileName, g_sDifficultyFileNames[(int)diff], 0x1F);

    if (blend && (int)diff > 1 && (int)diff < 3)
    {
        SkillTweaks minmax[2];
        minmax[0].Init((eDifficultyID)1, 0);
        minmax[1].Init((eDifficultyID)3, 0);

        float fPercent = (float)(diff - 1) / 2.f;

        SkillTweak* dst = mSkillTweaksList.m_pStart;
        SkillTweak* min = minmax[0].mSkillTweaksList.m_pStart;
        SkillTweak* max = minmax[1].mSkillTweaksList.m_pStart;

        while (dst)
        {
            *(dst->mpValue) = *(min->mpValue) + (*(max->mpValue) - *(min->mpValue)) * fPercent;

            dst = dst->next;
            min = min->next;
            max = max->next;
        }

        bNeedToLoadFile = false;
    }

    if (bNeedToLoadFile)
    {
        Config cfg(Config::ALLOCATE_HIGH);
        cfg.LoadFromFile((const char*)mszFileName);

        SkillTweak* it = mSkillTweaksList.m_pStart;
        while (it != 0)
        {
            *it->mpValue = GetConfigFloat(cfg, it->mNameInFile, 0.5f);
            it = it->next;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800401EC | size: 0x4
 */
void SkillTweaks::HookupTweakeables(int nSide)
{
}
