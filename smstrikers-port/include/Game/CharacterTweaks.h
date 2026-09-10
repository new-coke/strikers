#ifndef _CHARACTERTWEAKS_H_
#define _CHARACTERTWEAKS_H_

#include "Game/TweaksBase.h"

class PlayerTweaks : public TweaksBase
{
public:
    PlayerTweaks(const char* name)
        : TweaksBase(name)
    {
        Init();
    } // Inline constructor
    virtual ~PlayerTweaks();
    virtual void Init();

    /* 0x24 */ float fJoggingSpeed;
    /* 0x28 */ float fRunningSpeed;
    /* 0x2C */ float fRunningDirectionSeekSpeed;
    /* 0x30 */ float fRunningDirectionSeekFalloff;
    /* 0x34 */ float fPassGroundSpeedMax;
    /* 0x38 */ float fPassGroundSpeedMin;
    /* 0x3C */ float fPassVolleySpeedMax;
    /* 0x40 */ float fPassVolleySpeedMin;
    /* 0x44 */ float fPhysCapsuleHeight;
    /* 0x48 */ float fPhysCapsuleRadius;
}; // total size: 0x4C

class GoalieTweaks : public PlayerTweaks
{
public:
    GoalieTweaks(const char* name);
    virtual ~GoalieTweaks();
    virtual void Init();

    /* 0x04C */ f32 fThrowingDirectionSeekSpeed;
    /* 0x050 */ f32 fThrowingDirectionSeekFalloff;
    /* 0x054 */ f32 fKickDistanceMin;
    /* 0x058 */ f32 fOverhandThrowDistanceMin;
    /* 0x05C */ f32 fKickVelocityMin;
    /* 0x060 */ f32 fKickVelocityMax;
    /* 0x064 */ f32 fKickAngleMin;
    /* 0x068 */ f32 fKickAngleMax;
    /* 0x06C */ f32 fFatigueRecoverRate;
    /* 0x070 */ f32 fFatigueCatchThreshold;
    /* 0x074 */ f32 fCatchSaveMaxSpeed;
    /* 0x078 */ f32 fGetupEnergyHigh;
    /* 0x07C */ f32 fGetupEnergyLow;
    /* 0x080 */ f32 fGetupSpeedLow;
    /* 0x084 */ f32 fStrafeSpeedLow;
    /* 0x088 */ f32 fGoalieBallTime;
    /* 0x08C */ f32 fGoalieStunTimeMin;
    /* 0x090 */ f32 fGoalieStunTimeMax;
    /* 0x094 */ f32 fLooseBallShotDistance;
    /* 0x098 */ f32 fSaveDirectionSeekSpeed;
    /* 0x09C */ f32 fSaveDirectionSeekFalloff;
    /* 0x0A0 */ f32 fSaveBackRunTimeScale;
    /* 0x0A4 */ f32 fSaveIgnoreMargin;
    /* 0x0A8 */ f32 fSaveMissDelay;
    /* 0x0AC */ f32 fLobShotStumbleChance;
    /* 0x0B0 */ f32 fInterceptSaveTolerance;
    /* 0x0B4 */ f32 fSaveCatchTolerance;
    /* 0x0B8 */ f32 fShotFatigueDefault;
    /* 0x0BC */ f32 fShotFatigueStandCatch;
    /* 0x0C0 */ f32 fShotFatigueDiveCatch;
    /* 0x0C4 */ f32 fShotFatigueStandDeflect;
    /* 0x0C8 */ f32 fShotFatigueDiveDeflect;
    /* 0x0CC */ f32 fShotFatigueStandPunch;
    /* 0x0D0 */ f32 fShotFatigueLegSave;
    /* 0x0D4 */ f32 fShotFatigueSTSSave;
    /* 0x0D8 */ f32 fShotFatigueSTSStun;
    /* 0x0DC */ f32 fShotFatigueMax;
    /* 0x0E0 */ f32 fSTSAttackCloseDistance;
    /* 0x0E4 */ f32 fSTSAttackMaxDistance;
    /* 0x0E8 */ f32 fSTSAttackChancePerFrame;
    /* 0x0EC */ f32 fSTSAttackBallVelMult;
    /* 0x0F0 */ f32 fPounceRange;
    /* 0x0F4 */ f32 fPounceDekeChance;
};

class FielderTweaks : public PlayerTweaks
{
public:
    FielderTweaks(const char* name);
    virtual ~FielderTweaks();
    virtual void Init();

    /* 0x4C */ float fRunningAccel;
    /* 0x50 */ float fRunningDecel;
    /* 0x54 */ float fRunningStopDecel;
    /* 0x58 */ float fRunningStrafeDirectionSeekSpeed;
    /* 0x5C */ float fRunningStrafeDirectionSeekFalloff;
    /* 0x60 */ float fRunningStrafeSpeed;
    /* 0x64 */ float fRunningBackwardsSpeed;
    /* 0x68 */ float fRunningStrafeAccel;
    /* 0x6C */ float fRunningStrafeDecel;
    /* 0x70 */ float fRunningTurboDirectionSeekSpeed;
    /* 0x74 */ float fRunningTurboDirectionSeekFalloff;
    /* 0x78 */ float fRunningTurboSpeed;
    /* 0x7C */ float fRunningTurboAccel;
    /* 0x80 */ float fRunningTurboDecel;
    /* 0x84 */ float fRunningWBDirectionSeekSpeed;
    /* 0x88 */ float fRunningWBDirectionSeekFalloff;
    /* 0x8C */ float fRunningWBSpeed;
    /* 0x90 */ float fRunningWBAccel;
    /* 0x94 */ float fRunningWBDecel;
    /* 0x98 */ float fRunningWBStopDecel;
    /* 0x9C */ float fRunningWBTurboDirectionSeekSpeed;
    /* 0xA0 */ float fRunningWBTurboDirectionSeekFalloff;
    /* 0xA4 */ float fRunningWBTurboSpeedLevel1;
    /* 0xA8 */ float fRunningWBTurboSpeedLevel2;
    /* 0xAC */ float fRunningWBTurboSpeedLevel3;
    /* 0xB0 */ float fRunningWBTurboAccel;
    /* 0xB4 */ float fRunningWBTurboDecel;
    /* 0xB8 */ float fRunningWBTurboTurnSpeed;
    /* 0xBC */ float fRunningWBTurboTurnAccel;
    /* 0xC0 */ float fRunningWBTurboTurnDecel;
    /* 0xC4 */ float fAggression;
    /* 0xC8 */ float fShooting;
    /* 0xCC */ float fPassing;
    /* 0xD0 */ float fDekeing;
    /* 0xD4 */ float fShotWindupDirectionSeekSpeed;
    /* 0xD8 */ float fShotWindupDirectionSeekFalloff;
    /* 0xDC */ float fShotWindupDecel;
    /* 0xE0 */ float fShotMinSpeed;
    /* 0xE4 */ float fShotMaxSpeed;
    /* 0xE8 */ float fShotChipMinSpeed;
    /* 0xEC */ float fShotChipMaxSpeed;
    /* 0xF0 */ float fCaptainS2SNisBeginFrame;
    /* 0xF4 */ float fCaptainS2SNisEndFrame;
    /* 0xF8 */ float fS2SKickFrame;
    /* 0xFC */ float fS2S1stJumpFrame;

    /* 0x100 */ s32 nChanceForRedShell;
    /* 0x104 */ s32 nChanceForGreenShell;
    /* 0x108 */ s32 nChanceForSpinyShell;
    /* 0x10C */ s32 nChanceForFreezeShell;
    /* 0x110 */ s32 nChanceForMushroom;
    /* 0x114 */ s32 nChanceForBanana;
    /* 0x118 */ s32 nChanceForBoBomb;

    /* 0x11C */ float fChanceForBig;
    /* 0x120 */ float fChanceForMultiples;
};

#endif // _CHARACTERTWEAKS_H_
