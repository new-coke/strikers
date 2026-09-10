#ifndef _GAMETWEAKS_H_
#define _GAMETWEAKS_H_

#include "Game/TweaksBase.h"
#include "NL/nlMath.h"
#include "NL/nlPrint.h"
#include "NL/nlList.h"

enum eDifficultyID
{
    DIFF_DEFAULT = -1,
    DIFF_BRAINDEAD = 0,
    DIFF_EASY = 1,
    DIFF_MEDIUM = 2,
    DIFF_HARD = 3,
    DIFF_VERYHARD = 4,
    DIFF_SUPERHUMAN = 5,
    DIFF_HUMAN = 6,
    NUM_DIFFS = 7,
};

class SkillTweak
{
public:
    SkillTweak(float* valuePointer, const char* sNameInFile)
    {
        mpValue = valuePointer;
        nlSNPrintf(mNameInFile, 0x7F, "%s", sNameInFile);
    }

    /* 0x00 */ SkillTweak* next;
    /* 0x04 */ char mNameInFile[128];
    /* 0x84 */ float* mpValue;
}; // total size: 0x88

class SkillTweaks
{
public:
    SkillTweaks();
    /* 0x04 */ virtual ~SkillTweaks();
    /* 0x08 */ virtual void Init(eDifficultyID diff, bool blend);

    static SkillTweaks* GetSkillTweaks(int idx);
    void HookupTweakeables(int nSide);

public:
    /* 0x04 */ f32 Shoot_CaptainS2SFirstButtonChance;
    /* 0x08 */ f32 Shoot_CaptainS2SSecondButtonChance;
    /* 0x0C */ f32 Off_Avoidance;
    /* 0x10 */ f32 Off_DekeChance;
    /* 0x14 */ f32 Off_GroundPassChance;
    /* 0x18 */ f32 Off_VolleyPassChance;
    /* 0x1C */ f32 Off_ShootingChance;
    /* 0x20 */ f32 Off_ChipShotChance;
    /* 0x24 */ f32 Off_CaptainS2SChance;
    /* 0x28 */ f32 Off_GroundOneTimerChance;
    /* 0x2C */ f32 Off_VolleyOneTimerChance;
    /* 0x30 */ f32 Off_OneTouchGroundPassChance;
    /* 0x34 */ f32 Off_OneTouchVolleyPassChance;
    /* 0x38 */ f32 Off_PassReceiveHitChance;
    /* 0x3C */ f32 Off_PassReceivePowerupChance;
    /* 0x40 */ f32 Off_WindupDekeChance;
    /* 0x44 */ f32 Off_WindupPassChance;
    /* 0x48 */ f32 Off_WindupPowerupChance;
    /* 0x4C */ f32 Off_CutAndBreakChance;
    /* 0x50 */ f32 Off_TurboChance;
    /* 0x54 */ f32 Off_Reaction;
    /* 0x58 */ f32 Def_BlockPassChance;
    /* 0x5C */ f32 Def_BlockShotChance;
    /* 0x60 */ f32 Def_SlideAttackChance;
    /* 0x64 */ f32 Def_HeavyAttackChance;
    /* 0x68 */ f32 Def_VolleyPassDefendChance;
    /* 0x6C */ f32 Def_Marking;
    /* 0x70 */ f32 Loose_HeavyAttackChance;
    /* 0x74 */ f32 Loose_ShotChance;
    /* 0x78 */ f32 Loose_GroundPassChance;
    /* 0x7C */ f32 Loose_VolleyPassChance;
    /* 0x80 */ f32 fShotValue1;
    /* 0x84 */ f32 fShotValue2;
    /* 0x88 */ f32 fShotValue3;
    /* 0x8C */ f32 fShotChance0;
    /* 0x90 */ f32 fShotChance1;
    /* 0x94 */ f32 fShotChance2;
    /* 0x98 */ f32 fShotChance3;
    /* 0x9C */ f32 fShotChance4;
    /* 0xA0 */ f32 fSTSWindupTime;
    /* 0xA4 */ f32 fAttackCarrierDistance;
    /* 0xA8 */ f32 fLooseBallChaseDistance;
    /* 0xAC */ f32 fGoalieCanInterceptPass;
    /* 0xB0 */ f32 fGoalieDekeChance;
    /* 0xB4 */ f32 fGoalieDekeSpeed;

    /* 0xB8 */ f32 PowerupUsageChance[3][9];

    /* 0x124 */ s8 mszFileName[32];
    /* 0x144 */ nlList<SkillTweak> mSkillTweaksList;
}; // total size: 0x144

class GameTweaks : public TweaksBase
{
public:
    GameTweaks(const char* name);
    virtual ~GameTweaks() { }
    void Init();

public:
    /* 0x24, */ float fGameDuration;                        // offset 0x24, size 0x4
    /* 0x28, */ float fBlockPassLightAttackSpeed;           // offset 0x28, size 0x4
    /* 0x2C, */ float fFielderAttributeWeight;              // offset 0x2C, size 0x4
    /* 0x30, */ nlVector2 vGetInPositionKeyFielderDist;     // offset 0x30, size 0x8
    /* 0x38, */ nlVector2 vGetInPositionInRadius;           // offset 0x38, size 0x8
    /* 0x40, */ nlVector2 vGetInPositionOutRadius;          // offset 0x40, size 0x8
    /* 0x48, */ s16 nStrafeToRunInDirectionDelta;           // offset 0x48, size 0x2
    /* 0x4A, */ s16 nBackwardsToStrafeRunInDirectionDelta;  // offset 0x4A, size 0x2
    /* 0x4C, */ s16 nStrafeToRunOutDirectionDelta;          // offset 0x4C, size 0x2
    /* 0x4E, */ s16 nBackwardsToStrafeRunOutDirectionDelta; // offset 0x4E, size 0x2
    /* 0x50, */ float fSlideAttackRadius;                   // offset 0x50, size 0x4
    /* 0x54, */ float fArrivalInRadius;                     // offset 0x54, size 0x4
    /* 0x58, */ float fArrivalOutRadius;                    // offset 0x58, size 0x4
    /* 0x5C, */ float fNearSeekInRadius;                    // offset 0x5C, size 0x4
    /* 0x60, */ float fNearSeekOutRadius;                   // offset 0x60, size 0x4
    /* 0x64, */ float fPassSpeedMaxDist;                    // offset 0x64, size 0x4
    /* 0x68, */ float fPassSpeedMinDist;                    // offset 0x68, size 0x4
    /* 0x6C, */ float fSwapControllerTime;                  // offset 0x6C, size 0x4
    /* 0x70, */ float fSwapFacingTime;                      // offset 0x70, size 0x4
    /* 0x74, */ float fGreenShellActiveTime;                // offset 0x74, size 0x4
    /* 0x78, */ float fGreenShellSpeed;                     // offset 0x78, size 0x4
    /* 0x7C, */ float fRedShellActiveTime;                  // offset 0x7C, size 0x4
    /* 0x80, */ float fRedShellSpeed;                       // offset 0x80, size 0x4
    /* 0x84, */ float fSpinyShellActiveTime;                // offset 0x84, size 0x4
    /* 0x88, */ float fSpinyShellSpeed;                     // offset 0x88, size 0x4
    /* 0x8C, */ float fFreezeShellActiveTime;               // offset 0x8C, size 0x4
    /* 0x90, */ float fFreezeShellSpeed;                    // offset 0x90, size 0x4
    /* 0x94, */ float fFreezeShellFrozenTime;               // offset 0x94, size 0x4
    /* 0x98, */ float fBananaActiveTime;                    // offset 0x98, size 0x4
    /* 0x9C, */ float fBananaSpeed;                         // offset 0x9C, size 0x4
    /* 0xA0, */ float fBobombActiveTime;                    // offset 0xA0, size 0x4
    /* 0xA4, */ float fBobombSpeed;                         // offset 0xA4, size 0x4
    /* 0xA8, */ float fBobombMaxZSpeed;                     // offset 0xA8, size 0x4
    /* 0xAC, */ float fChainChompActiveTime;                // offset 0xAC, size 0x4
    /* 0xB0, */ float fChainChompFallTime;                  // offset 0xB0, size 0x4
    /* 0xB4, */ float fChainChompSpeed;                     // offset 0xB4, size 0x4
    /* 0xB8, */ int nChanceForRedShell;                     // offset 0xB8, size 0x4
    /* 0xBC, */ int nChanceForGreenShell;                   // offset 0xBC, size 0x4
    /* 0xC0, */ int nChanceForSpinyShell;                   // offset 0xC0, size 0x4
    /* 0xC4, */ int nChanceForFreezeShell;                  // offset 0xC4, size 0x4
    /* 0xC8, */ int nChanceForMushroom;                     // offset 0xC8, size 0x4
    /* 0xCC, */ int nChanceForBanana;                       // offset 0xCC, size 0x4
    /* 0xD0, */ int nChanceForStar;                         // offset 0xD0, size 0x4
    /* 0xD4, */ int nChanceForBoBomb;                       // offset 0xD4, size 0x4
    /* 0xD8, */ int nChanceForChainChomp;                   // offset 0xD8, size 0x4
    /* 0xDC, */ int nScoreDifferenceMaximum;                // offset 0xDC, size 0x4
    /* 0xE0, */ int nScoreDifferenceMinimum;                // offset 0xE0, size 0x4
    /* 0xE4, */ int nPowerupsNumForPenalty;                 // offset 0xE4, size 0x4
    /* 0xE8, */ float fBananaResistance;                    // offset 0xE8, size 0x4
    /* 0xEC, */ float fShellBounceGround;                   // offset 0xEC, size 0x4
    /* 0xF0, */ float fShellBounce;                         // offset 0xF0, size 0x4
    /* 0xF4, */ float fShellSmallRadius;                    // offset 0xF4, size 0x4
    /* 0xF8, */ float fShellMediumRadius;                   // offset 0xF8, size 0x4
    /* 0xFC, */ float fShellBigRadius;                      // offset 0xFC, size 0x4
    /* 0x100 */ float fShellMediumChance;                   // offset 0x100, size 0x4
    /* 0x104 */ float fShellBigChance;                      // offset 0x104, size 0x4
    /* 0x108 */ float fShellFiveChance;                     // offset 0x108, size 0x4
    /* 0x10C */ float fShellThreeChance;                    // offset 0x10C, size 0x4
    /* 0x110 */ float fShellExplodeChance;                  // offset 0x110, size 0x4
    /* 0x114 */ float fBananaSmallRadius;                   // offset 0x114, size 0x4
    /* 0x118 */ float fBananaMediumRadius;                  // offset 0x118, size 0x4
    /* 0x11C */ float fBananaBigRadius;                     // offset 0x11C, size 0x4
    /* 0x120 */ float fBananaMediumChance;                  // offset 0x120, size 0x4
    /* 0x124 */ float fBananaBigChance;                     // offset 0x124, size 0x4
    /* 0x128 */ float fBananaFiveChance;                    // offset 0x128, size 0x4
    /* 0x12C */ float fBananaThreeChance;                   // offset 0x12C, size 0x4
    /* 0x130 */ float fBananaExplodeChance;                 // offset 0x130, size 0x4
    /* 0x134 */ float fBobombSmallRadius;                   // offset 0x134, size 0x4
    /* 0x138 */ float fBobombMediumRadius;                  // offset 0x138, size 0x4
    /* 0x13C */ float fBobombBigRadius;                     // offset 0x13C, size 0x4
    /* 0x140 */ float fBobombMediumChance;                  // offset 0x140, size 0x4
    /* 0x144 */ float fBobombBigChance;                     // offset 0x144, size 0x4
    /* 0x148 */ float fBobombFiveChance;                    // offset 0x148, size 0x4
    /* 0x14C */ float fBobombThreeChance;                   // offset 0x14C, size 0x4
    /* 0x150 */ float fBobombMineChance;                    // offset 0x150, size 0x4
    /* 0x154 */ float fPowerupExplosionRadius;              // offset 0x154, size 0x4
    /* 0x158 */ float fPowerupArrowThrowChance;             // offset 0x158, size 0x4
    /* 0x15C */ float fPowerupSpreadThrowChance;            // offset 0x15C, size 0x4
    /* 0x160 */ float fPowerupSurroundThrowChance;          // offset 0x160, size 0x4
    /* 0x164 */ float fPowerupHorizontalLineThrowChance;    // offset 0x164, size 0x4
    /* 0x168 */ float fPowerupHitWithBallMinAmount;         // offset 0x168, size 0x4
    /* 0x16C */ float fPowerupHitWithBallMaxAmount;         // offset 0x16C, size 0x4
    /* 0x170 */ float fPowerupHitNoBallMinAmount;           // offset 0x170, size 0x4
    /* 0x174 */ float fPowerupHitNoBallMaxAmount;           // offset 0x174, size 0x4
    /* 0x178 */ float fPowerupSlideWithBallMinAmount;       // offset 0x178, size 0x4
    /* 0x17C */ float fPowerupSlideWithBallMaxAmount;       // offset 0x17C, size 0x4
    /* 0x180 */ float fPowerupSlideNoBallMinAmount;         // offset 0x180, size 0x4
    /* 0x184 */ float fPowerupSlideNoBallMaxAmount;         // offset 0x184, size 0x4
    /* 0x188 */ float fPowerupPowerShotMinAmount;           // offset 0x188, size 0x4
    /* 0x18C */ float fPowerupPowerShotMaxAmount;           // offset 0x18C, size 0x4
    /* 0x190 */ float fPowerupInterceptPassMinAmount;       // offset 0x190, size 0x4
    /* 0x194 */ float fPowerupInterceptPassMaxAmount;       // offset 0x194, size 0x4
    /* 0x198 */ float fPowerupPerfectPassMinAmount;         // offset 0x198, size 0x4
    /* 0x19C */ float fPowerupPerfectPassMaxAmount;         // offset 0x19C, size 0x4
    /* 0x1A0 */ float fPowerupContextDekeMinAmount;         // offset 0x1A0, size 0x4
    /* 0x1A4 */ float fPowerupContextDekeMaxAmount;         // offset 0x1A4, size 0x4
    /* 0x1A8 */ float fPowerupIconSpeed;                    // offset 0x1A8, size 0x4
    /* 0x1AC */ int nPowerupBoxMaxActiveOnField;            // offset 0x1AC, size 0x4
    /* 0x1B0 */ float fPowerupBoxDelayDropTime;             // offset 0x1B0, size 0x4
    /* 0x1B4 */ float fChainChompRadius;                    // offset 0x1B4, size 0x4
    /* 0x1B8 */ float fDrawPowerupIconTime;                 // offset 0x1B8, size 0x4
    /* 0x1BC */ float fHitCarrierWeighting;                 // offset 0x1BC, size 0x4
    /* 0x1C0 */ float fAngleWeighting;                      // offset 0x1C0, size 0x4
    /* 0x1C4 */ float fDekeAngleWeighting;                  // offset 0x1C4, size 0x4
    /* 0x1C8 */ float fPassAngleWeighting;                  // offset 0x1C8, size 0x4
    /* 0x1CC */ float fVolleyPassAngleWeighting;            // offset 0x1CC, size 0x4
    /* 0x1D0 */ float fVolleyPassMinDistance;               // offset 0x1D0, size 0x4
    /* 0x1D4 */ float fMushroomEffectTime;                  // offset 0x1D4, size 0x4
    /* 0x1D8 */ float fMushroomSpeed;                       // offset 0x1D8, size 0x4
    /* 0x1DC */ float fStarEffectTime;                      // offset 0x1DC, size 0x4
    /* 0x1E0 */ float fStarSpeed;                           // offset 0x1E0, size 0x4

    // /* 0x1E4 */ float fPerfectPassSlowMo;                   // offset 0x1E4, size 0x4
    // /* 0x1E8 */ float fShootToScoreSlowMoMin;               // offset 0x1E8, size 0x4
    // /* 0x1EC */ float fShootToScoreSlowMoMax;               // offset 0x1EC, size 0x4
    // /* 0x1F0 */ float fFadeFilterSlowMoInTime;              // offset 0x1F0, size 0x4
    // /* 0x1F4 */ float fFadeFilterSlowMoOutTime;             // offset 0x1F4, size 0x4
    // /* 0x1F8 */ float fFadeFilterFreqMin;                   // offset 0x1F8, size 0x4
    // /* 0x1FC */ float fFadeFilterFreqMax;                   // offset 0x1FC, size 0x4
    // /* 0x200 */ float fFadeFilterDropoffDelayTime;          // offset 0x200, size 0x4
    // /* 0x204 */ float fFadeFilterDropoffTime;               // offset 0x204, size 0x4
    // /* 0x208 */ float fFadePitchMin;                        // offset 0x208, size 0x4
    // /* 0x20C */ float fFadePitchMax;                        // offset 0x20C, size 0x4
    // /* 0x210 */ float fMinBobombMoveSFXTime;                // offset 0x210, size 0x4
    // /* 0x214 */ float fPerfectPassProximityFilterDistSq;    // offset 0x214, size 0x4
    // /* 0x218 */ float fPowerupSmallSizeVolCoeff;            // offset 0x218, size 0x4
    // /* 0x21C */ float fPowerupMedSizeVolCoeff;              // offset 0x21C, size 0x4
    // /* 0x220 */ float fPowerupLargeSizeVolCoeff;            // offset 0x220, size 0x4
    // /* 0x224 */ float fFadePerfectPassTrailSFXStartTime;    // offset 0x224, size 0x4
    // /* 0x228 */ float fShootToScoreBallBlurWidth;           // offset 0x228, size 0x4
    // /* 0x22C */ int nShootToScoreBallBlurLength;            // offset 0x22C, size 0x4
    // /* 0x230 */ float fShootToScoreYellowDistance;          // offset 0x230, size 0x4
    // /* 0x234 */ float fShootToScorePerfectFirstButtonTime;  // offset 0x234, size 0x4
    // /* 0x238 */ float fShootToScorePerfectSecondButtonTime; // offset 0x238, size 0x4
    // /* 0x23C */ float fShootToScorePerfectDistanceTimeMin;  // offset 0x23C, size 0x4
    // /* 0x240 */ float fShootToScorePerfectDistanceTimeMax;  // offset 0x240, size 0x4
    // /* 0x244 */ float fSlideAttackTimeToSlide;              // offset 0x244, size 0x4
    // /* 0x248 */ float fSlideAttackTimeToDecelrate;          // offset 0x248, size 0x4
    // /* 0x24C */ float fSlideAttackDeceleration;             // offset 0x24C, size 0x4
    // /* 0x250 */ float fLeftTriggerDownPressure;             // offset 0x250, size 0x4
    // /* 0x254 */ float fIndicatorDistAboveHead;              // offset 0x254, size 0x4
    // /* 0x258 */ float fIndicatorDistInPixels;               // offset 0x258, size 0x4
    // /* 0x25C */ float fClearBallGroundMinSpeed;             // offset 0x25C, size 0x4
    // /* 0x260 */ float fClearBallGroundMaxSpeed;             // offset 0x260, size 0x4
    // /* 0x264 */ float fClearBallMinZSpeed;                  // offset 0x264, size 0x4
    // /* 0x268 */ float fClearBallMaxZSpeed;                  // offset 0x268, size 0x4
    // /* 0x26C */ float fSTSWindupTime;                       // offset 0x26C, size 0x4
    // /* 0x270 */ float fShotWindupTime;                      // offset 0x270, size 0x4
    // /* 0x274 */ float fShotMeterNetOpenAngle;               // offset 0x274, size 0x4
    // /* 0x278 */ float fShotMeterRatingsWeight;              // offset 0x278, size 0x4
    // /* 0x27C */ float fShotMeterNetOpenWeight;              // offset 0x27C, size 0x4
    // /* 0x280 */ float fShotMeterPlayerDistanceWeight;       // offset 0x280, size 0x4
    // /* 0x284 */ float fShotChipMeterGPositionWeight;        // offset 0x284, size 0x4
    // /* 0x288 */ float fShotChipMeterNetOpenWeight;          // offset 0x288, size 0x4
    // /* 0x28C */ float fShotMeterOneTimerMaxSpeed;           // offset 0x28C, size 0x4
    // /* 0x290 */ float fShotPostOffset;                      // offset 0x290, size 0x4
    // /* 0x294 */ float fChipShotPostOffset;                  // offset 0x294, size 0x4
    // /* 0x298 */ float fShotHighDistance;                    // offset 0x298, size 0x4
    // /* 0x29C */ float fShotWidthVariance;                   // offset 0x29C, size 0x4
    // /* 0x2A0 */ float fShotHeightVariance;                  // offset 0x2A0, size 0x4
    // /* 0x2A4 */ float fBowserRadius;                        // offset 0x2A4, size 0x4
    // /* 0x2A8 */ float fBowserChance;                        // offset 0x2A8, size 0x4
    // /* 0x2AC */ float fBowserStartTime;                     // offset 0x2AC, size 0x4
    // /* 0x2B0 */ float fBowserEndTime;                       // offset 0x2B0, size 0x4
    // /* 0x2B4 */ float fBowserMinAttackTime;                 // offset 0x2B4, size 0x4
    // /* 0x2B8 */ float fBowserMaxAttackTime;                 // offset 0x2B8, size 0x4
    // /* 0x2BC */ float fBowserTiltTime;                      // offset 0x2BC, size 0x4
    // /* 0x2C0 */ float fBowserIntervalDelay;                 // offset 0x2C0, size 0x4
    // /* 0x2C4 */ float fBowserMinTiltForce;                  // offset 0x2C4, size 0x4
    // /* 0x2C8 */ float fBowserMaxTiltForce;                  // offset 0x2C8, size 0x4
    // /* 0x2CC */ float fBowserMaxTilt;                       // offset 0x2CC, size 0x4
    // /* 0x2D0 */ float fBowserGravity;                       // offset 0x2D0, size 0x4
    // /* 0x2D4 */ float fBowserRebound;                       // offset 0x2D4, size 0x4
    // /* 0x2D8 */ float fBowserBallForceMult;                 // offset 0x2D8, size 0x4
    // /* 0x2DC */ float fBowserBigPowerupBoost;               // offset 0x2DC, size 0x4

    /* 0x1E4 */ float fPerfectPassSlowMo;
    /* 0x1E8 */ float fShootToScoreSlowMoMin;
    /* 0x1EC */ float fShootToScoreSlowMoMax;

    /* 0x1F0 */ float fFrontAudibleSurrDist;
    /* 0x1F4 */ float fBackAudibleSurrDist;
    /* 0x1F8 */ float fSpeedOfSoundForDoppler;
    /* 0x1FC */ float fEmitterDistFromListenerMaxVol;
    /* 0x200 */ float fMaxAudibleEmitterDistance;
    /* 0x204 */ float fEmitterVolToDistanceValue;
    /* 0x208 */ float fFadeFilterSlowMoInTime;
    /* 0x20C */ float fFadeFilterSlowMoOutTime;
    /* 0x210 */ float fFadeFilterFreqMin;
    /* 0x214 */ float fFadeFilterFreqMax;
    /* 0x218 */ float fFadeFilterDropoffDelayTime;
    /* 0x21C */ float fFadeFilterDropoffTime;
    /* 0x220 */ float fFadePitchMin;
    /* 0x224 */ float fFadePitchMax;
    /* 0x228 */ float fMinBobombMoveSFXTime;
    /* 0x22C */ float fPerfectPassProximityFilterDistSq;
    /* 0x230 */ float fPowerupSmallSizeVolCoeff;
    /* 0x234 */ float fPowerupMedSizeVolCoeff;
    /* 0x238 */ float fPowerupLargeSizeVolCoeff;
    /* 0x23C */ float fFadePerfectPassTrailSFXStartTime;
    /* 0x240 */ float fMinHitIntensityForHardBodyHitSFX;
    /* 0x244 */ float fSlideAttackHitReactionVolume;
    /* 0x248 */ float fShootToScoreBallHitReactionVolume;
    /* 0x24C */ float fBombHitReactionVolume;
    /* 0x250 */ float fBombShockwaveReactionVolume;
    /* 0x254 */ float fSmallShellHitReactionVolume;
    /* 0x258 */ float fMediumShellHitReactionVolume;
    /* 0x25C */ float fLargeShellHitReactionVolume;
    /* 0x260 */ float unk260;
    /* 0x264 */ float fGoalieDropKickHitReactionVolume;
    /* 0x268 */ float fBallHitWallMaxAudibleVelocity;
    /* 0x26C */ float fBallHitWallMinAudibleVelocity;
    /* 0x270 */ float fBallHitWallMinVolume;
    /* 0x274 */ float fBallHitWallMinTimeBeforeNextAudio;
    /* 0x278 */ float fBallHitNetMaxAudibleVelocity;
    /* 0x27C */ float fBallHitNetMinAudibleVelocity;
    /* 0x280 */ float fBallHitNetMinVolume;
    /* 0x284 */ float fBallHitNetMinTimeBeforeNextAudio;
    /* 0x288 */ float fShootToScoreBallBlurWidth;
    /* 0x28C */ int nShootToScoreBallBlurLength;
    /* 0x290 */ float fShootToScoreYellowDistance;
    /* 0x294 */ float fShootToScorePerfectFirstButtonTime;
    /* 0x298 */ float fShootToScorePerfectSecondButtonTime;
    /* 0x29C */ float fShootToScorePerfectDistanceTimeMin;
    /* 0x2A0 */ float fShootToScorePerfectDistanceTimeMax;
    /* 0x2A4 */ float fSlideAttackTimeToSlide;
    /* 0x2A8 */ float fSlideAttackTimeToDecelrate;
    /* 0x2AC */ float fSlideAttackDeceleration;
    /* 0x2B0 */ float fLeftTriggerDownPressure;
    /* 0x2B4 */ float fIndicatorDistAboveHead;
    /* 0x2B8 */ float fIndicatorDistInPixels;
    /* 0x2BC */ float fClearBallGroundMinSpeed;
    /* 0x2C0 */ float fClearBallGroundMaxSpeed;
    /* 0x2C4 */ float fClearBallMinZSpeed;
    /* 0x2C8 */ float fClearBallMaxZSpeed;
    /* 0x2CC */ float unk2CC;
    /* 0x2D0 */ float fShotWindupTime;
    /* 0x2D4 */ float fShotMeterNetOpenAngle;
    /* 0x2D8 */ float fShotMeterRatingsWeight;
    /* 0x2DC */ float fShotMeterNetOpenWeight;
    /* 0x2E0 */ float fShotMeterPlayerDistanceWeight;
    /* 0x2E4 */ float fShotChipMeterGPositionWeight;
    /* 0x2E8 */ float fShotChipMeterNetOpenWeight;
    /* 0x2EC */ float fShotMeterOneTimerMaxSpeed;
    /* 0x2F0 */ float fShotPostOffset;
    /* 0x2F4 */ float fChipShotPostOffset;
    /* 0x2F8 */ float fShotHighDistance;
    /* 0x2FC */ float fShotWidthVariance;
    /* 0x300 */ float fShotHeightVariance;
    /* 0x304 */ float fBowserRadius;
    /* 0x308 */ float fBowserChance;
    /* 0x30C */ float fBowserStartTime;
    /* 0x310 */ float fBowserEndTime;
    /* 0x314 */ float fBowserMinAttackTime;
    /* 0x318 */ float fBowserMaxAttackTime;
    /* 0x31C */ float fBowserTiltTime;
    /* 0x320 */ float fBowserIntervalDelay;
    /* 0x324 */ float fBowserMinTiltForce;
    /* 0x328 */ float fBowserMaxTiltForce;
    /* 0x32C */ float fBowserMaxTilt;
    /* 0x330 */ float fBowserGravity;
    /* 0x334 */ float fBowserRebound;
    /* 0x338 */ float fBowserBallForceMult; // fYAxisTiltForceMultiplier
    /* 0x33C */ float fBowserBigPowerupBoost;

}; // total size: 0x2E0

// class Config
// {
// public:
// };

#endif // _GAMETWEAKS_H_
