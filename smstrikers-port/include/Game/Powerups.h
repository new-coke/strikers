#ifndef _POWERUPS_H_
#define _POWERUPS_H_

#include "NL/nlMath.h"
#include "NL/nlTimer.h"
#include "Game/Physics/PhysicsObject.h"
#include "Game/Drawable/DrawableObj.h"
#include "Game/ObjectBlur.h"

class cFielder;

enum ePowerUpType
{
    POWER_UP_NONE = -1,
    POWER_UP_GREEN_SHELL = 0,
    POWER_UP_RED_SHELL = 1,
    POWER_UP_SPINY_SHELL = 2,
    POWER_UP_FREEZE_SHELL = 3,
    POWER_UP_BANANA = 4,
    POWER_UP_BOBOMB = 5,
    POWER_UP_CHAIN_CHOMP = 6,
    NUM_DRAWABLE_POWER_UPS = 6,
    POWER_UP_MUSHROOM = 7,
    POWER_UP_STAR = 8,
    NUM_POWER_UPS = 9,
};

enum ePowerupSize
{
    POWERUPSIZE_SMALL = 0,
    POWERUPSIZE_MEDIUM = 1,
    POWERUPSIZE_LARGE = 2,
};

struct PowerUpTeamType
{
    /* 0x0 */ enum ePowerUpType eType;
    /* 0x4 */ int nnumOfPowerups;
    /* 0x8 */ unsigned char bIsNew;
}; // total size: 0xC

class PowerupBase
{
public:
    virtual ~PowerupBase();

    /* 0x04 */ bool m_bShouldDestroy;
    /* 0x08 */ DrawableObject* m_pDrawableObj;
    /* 0x0C */ PhysicsObject* m_pPhysicsObject;
    /* 0x10 */ cFielder* m_pTarget;
    /* 0x14 */ cFielder* m_pThrower;
    /* 0x18 */ ePowerUpType m_eType;
    /* 0x1C */ Timer mtActiveTimer;
    /* 0x20 */ Timer mtNoHitTimer;
    /* 0x24 */ u16 m_aOrientation;
    /* 0x28 */ f32 m_scale;
    /* 0x2C */ nlVector3 m_v3Position;
    /* 0x38 */ nlVector3 m_v3PrevPosition;
    /* 0x44 */ nlVector3 m_v3Velocity;
    /* 0x50 */ const char* m_szStreakTexture;
    /* 0x54 */ f32 m_fBlurWidth;
    /* 0x58 */ f32 m_fBlurLength;
    /* 0x5C */ uintptr_t m_uVoiceID; // PORT: holds an SFXEmitter* despite the name.
    /* 0x60 */ BlurHandler* m_pBlurHandler;
    /* 0x64 */ s32 m_nIndex;
    /* 0x68 */ s32 m_nThrowerPadID;
    /* 0x6C */ ePowerupSize meSize;
    /* 0x70 */ bool mbExploder;
}; // total size: 0x74

#endif // _POWERUPS_H_
