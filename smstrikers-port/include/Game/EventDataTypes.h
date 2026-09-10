#ifndef _EVENTDATATYPES_H_
#define _EVENTDATATYPES_H_

#include "types.h"
#include "NL/nlMath.h"
#include "Game/Sys/eventman.h"
#include "Game/AI/Powerups.h"

// Forward declarations
class cPlayer;
class cFielder;
class cBall;
class Bowser;
class ChainChomp;

struct NISData : public EventData
{
    virtual u32 GetID() { return 0x1A5; }

    /* 0x04 */ const char* Type;
    /* 0x08 */ const char* Param;
}; // total size: 0xC

// CharacterDirectionData has a vtbl but does NOT inherit from EventData per DWARF
class CharacterDirectionData
{
public:
    virtual u32 GetID() { return 0x175; }

    /* 0x04 */ nlVector3* home;
    /* 0x08 */ nlVector3* away;
};

struct PowerupData : public EventData
{
    PowerupData() { }
    virtual u32 GetID() { return 0x15C; }

    /* 0x04 */ cFielder* pFielder;
    /* 0x08 */ float fAwardWorth;
};

struct PowerupAcquireEventData : public EventData
{
    PowerupAcquireEventData() { }
    virtual u32 GetID() { return 0x1C3; }

    /* 0x04 */ int mHomeAway;
}; // total size: 0x8

struct PenaltyData : public EventData
{
    PenaltyData() { }
    virtual u32 GetID() { return 0x152; }

    /* 0x04 */ cFielder* pFouler;
    /* 0x08 */ cFielder* pFoulee;
    /* 0x0C */ float fPenaltyWorth;
};

struct ShotAtGoalData : public EventData
{
    ShotAtGoalData() { }
    virtual u32 GetID() { return 0x14A; }

    /* 0x04 */ cPlayer* pShooter;
}; // total size: 0x8

struct PassBallData : public EventData
{
    virtual u32 GetID() { return 0x131; }

    /* 0x04 */ cPlayer* pPasser;
    /* 0x08 */ cPlayer* pTarget;
    /* 0x0C */ s32 mPasserControllerID;
}; // total size: 0x10

enum eReceiveBallResult
{
    RECEIVEBALL_LOOSE_PICKUP = 0,
    RECEIVEBALL_PASS_COMPLETE = 1,
    RECEIVEBALL_PASS_INTERCEPT = 2,
};

class ReceiveBallData : public EventData
{
public:
    ReceiveBallData() { }
    virtual u32 GetID() { return 0x121; }

    /* 0x04 */ cPlayer* pReceiver;
    /* 0x08 */ eReceiveBallResult eResult;
}; // total size: 0xC

class CollisionBobombData : public EventData
{
public:
    virtual u32 GetID() { return 0xED; }

    /* 0x04 */ nlVector3 v3ExplosionLocation;
    /* 0x10 */ float fExplosionRadius;
    /* 0x14 */ cFielder* pThrower;
    /* 0x18 */ int nThrowerPadID;
    /* 0x1C */ bool bIsFreezeBomb;
}; // total size: 0x20

class CollisionExplosionFragmentPlayerData : public EventData
{
public:
    virtual u32 GetID() { return 0xFA; }

    /* 0x04 */ cFielder* pPlayer;
    /* 0x08 */ nlVector3 v3CollisionLocation;
    /* 0x14 */ nlVector3 v3CollisionVelocity;
}; // total size: 0x20

class CollisionPlayerBananaData : public EventData
{
public:
    virtual u32 GetID() { return 0xE2; }

    /* 0x04 */ cFielder* pPlayer;
    /* 0x08 */ cFielder* pThrower;
    /* 0x0C */ s32 nThrowerPadID;
    /* 0x10 */ nlVector3 v3CollisionLocation;
}; // total size: 0x1C

class CollisionBallShellData : public EventData
{
public:
    virtual u32 GetID() { return 0xC1; }

    /* 0x04 */ nlVector3 v3CollisionVelocity;
}; // total size: 0x10

class CollisionPlayerFreezeData : public EventData
{
public:
    virtual u32 GetID() { return 0xD7; }

    /* 0x04 */ cFielder* pPlayer;
    /* 0x08 */ cFielder* pThrower;
    /* 0x0C */ s32 nThrowerPadID;
    /* 0x10 */ int eSize;
}; // total size: 0x14

class CollisionPowerupStatsData : public EventData
{
public:
    virtual u32 GetID() { return 0x104; }

    /* 0x04 */ cFielder* pThrower;
    /* 0x08 */ s32 nThrowerPadID;
}; // total size: 0x0C

class CollisionPlayerShellData : public EventData
{
public:
    virtual u32 GetID() { return 0xC9; }

    /* 0x04 */ cFielder* pPlayer;
    /* 0x08 */ cFielder* pThrower;
    /* 0x0C */ u8 nThrowerPadID;
    /* 0x0D */ u8 bIsExploder;
    /* 0x10 */ int eSize;
    /* 0x14 */ nlVector3 v3CollisionLocation;
    /* 0x20 */ nlVector3 v3CollisionVelocity;
}; // total size: 0x2C

class CollisionBowserPlayerData : public EventData
{
public:
    virtual u32 GetID() { return 0x65; }

    /* 0x04 */ cFielder* pFielder;
    /* 0x08 */ Bowser* pBowser;
}; // total size: 0xC

class CollisionChainPlayerData : public EventData
{
public:
    virtual u32 GetID() { return 0x5C; }

    /* 0x04 */ cFielder* pFielder;
    /* 0x08 */ ChainChomp* pChain;
}; // total size: 0xC

class CollisionPlayerShootToScoreBallData : public EventData
{
public:
    virtual u32 GetID() { return 0xB8; }

    /* 0x04 */ cFielder* pFielder;
    /* 0x08 */ cBall* pBall;
}; // total size: 0xC

class CollisionPlayerBallData : public EventData
{
public:
    virtual u32 GetID() { return 0xA7; }

    /* 0x04 */ cPlayer* pPlayer;
    /* 0x08 */ cBall* pBall;
    /* 0x0C */ nlVector3 velocity;
    /* 0x18 */ int boneID;
}; // total size: 0x1C

class CollisionPlayerWallData : public EventData
{
public:
    virtual u32 GetID() { return 0x6E; }

    /* 0x04 */ cPlayer* pPlayer;
    /* 0x08 */ nlVector3 contactPoint;
    /* 0x14 */ nlVector3 wallNormal;
}; // total size: 0x20

struct CollisionPowerupWallData : public EventData
{
    virtual u32 GetID() { return 0x9B; }

    /* 0x04 */ PowerupBase* pPowerup;
    /* 0x08 */ ePowerupSize eSize;
    /* 0x0C */ ePowerUpType eType;
    /* 0x10 */ nlVector3 position;
    /* 0x1C */ nlVector3 normal;
};

struct CollisionBallGoalpostData : public EventData
{
    static const u32 ID = 0x10e;

    virtual u32 GetID() { return ID; }

    /* 0x4, */ nlVector3 v3CollisionVelocity;
    /* 0x10 */ nlVector3 v3CollisionPosition;
    /* 0x1C */ u32 uTeamIndex;
}; // total size: 0x20

class CollisionBallWallData : public EventData
{
public:
    virtual u32 GetID() { return 0x78; }

    /* 0x04 */ cBall* pBall;
    /* 0x08 */ u8 bIsPerfect;
    /* 0x09 */ u8 bIsShot;
    /* 0x0C */ nlVector3 position;
    /* 0x18 */ nlVector3 normal;
    /* 0x24 */ float fCollisionVecLen;
}; // total size: 0x28

class CollisionBallGroundData : public EventData
{
public:
    virtual u32 GetID() { return 0x8F; }

    /* 0x04 */ cBall* pBall;
    /* 0x08 */ u8 bIsShot;
    /* 0x0C */ nlVector3 position;
    /* 0x18 */ nlVector3 normal;
    /* 0x24 */ float fVecZComponent;
}; // total size: 0x28

class CollisionPlayerPlayerData : public EventData
{
public:
    virtual u32 GetID() { return 0x51; }

    /* 0x04 */ cPlayer* player1;
    /* 0x08 */ cPlayer* player2;
    /* 0x0C */ nlVector3 velocity1;
    /* 0x18 */ nlVector3 velocity2;
}; // total size: 0x24

#endif // _EVENTDATATYPES_H_
