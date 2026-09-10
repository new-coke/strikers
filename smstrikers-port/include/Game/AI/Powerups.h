#ifndef _POWERUPS_H_
#define _POWERUPS_H_

#include "NL/nlMath.h"
#include "NL/nlTimer.h"
#include "Game/Physics/PhysicsObject.h"
#include "Game/Drawable/DrawableObj.h"
#include "Game/ObjectBlur.h"
#include "Game/Sys/eventman.h"

class cFielder;
class cPlayer;
class cTeam;
class BlurHandler;
struct SFXEmitter;
class Bowser;

struct PowerupSounds
{
    /* 0x00 */ unsigned long sndAcquire;
    /* 0x04 */ unsigned long sndActivate;
    /* 0x08 */ unsigned long sndInEffect;
    /* 0x0C */ unsigned long sndHit;
    /* 0x10 */ unsigned long sndBounceWall;
    /* 0x14 */ unsigned long sndBounceGround;
    /* 0x18 */ unsigned long sndExplode;
    /* 0x1C */ unsigned long sndEnd;
}; // total size: 0x20

struct PowerupModelPool
{
    PowerupModelPool() { mNum = 0; }
    void Initialize(int type, unsigned long objHashName);

    /* 0x000 */ int mNum;
    /* 0x004 */ DrawableObject* mObjs[6][25];
    /* 0x25C */ bool mFree[6][25];
}; // total size: 0x2F4

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
    /* 0x0 */ ePowerUpType eType;
    /* 0x4 */ int nnumOfPowerups;
    /* 0x8 */ unsigned char bIsNew;
}; // total size: 0xC

enum eThrowStyle
{
    THROW_ARROW = 0,
    THROW_HORIZONTAL_LINE = 1,
    THROW_SURROUND = 2,
    THROW_SPREAD = 3,
    NUM_THROW_TYPES = 4,
};

class PowerupBase
{
public:
    enum PowerupSound
    {
        PWRUP_SOUND_ACQUIRE = 0,
        PWRUP_SOUND_ACTIVATE = 1,
        PWRUP_SOUND_IN_EFFECT = 2,
        PWRUP_SOUND_HIT = 3,
        PWRUP_SOUND_BOUNCE_WALL = 4,
        PWRUP_SOUND_BOUNCE_GROUND = 5,
        PWRUP_SOUND_EXPLODE = 6,
        PWRUP_SOUND_END = 7,
    };

    PowerupBase(cFielder* pTarget, ePowerUpType eType, float fRadius, ePowerupSize eSize, bool bExplode, int nIndex);
    /* 0x08 */ virtual ~PowerupBase();
    /* 0x0C */ virtual void Destroy(bool bSilent);
    /* 0x10 */ virtual void PreThrow(cFielder* pFielder, Bowser* pBowser);
    /* 0x14 */ virtual void ThrowAt(cFielder* pThrower, Bowser* pBowser);
    /* 0x18 */ virtual void Init(cFielder* pFielder, Bowser* pBowser);
    /* 0x1C */ virtual void Update(float dt);

    float GetRadius() const;
    static int AwardPowerup(cTeam* pTeam);
    static void CollisionCallback(PhysicsObject* pObjA, PhysicsObject* pObjB, const nlVector3& v3Pos, void* pParam);
    void DecrementTimers(float fDeltaT);
    void SpeedManagement();
    void UpdateTransform();
    static unsigned long GetSoundType(ePowerUpType type, PowerupBase::PowerupSound powerupSnd);
    static unsigned long PlayPowerupSound(ePowerUpType type, PowerupBase::PowerupSound powerupSnd, PhysicsObject* pPhysObj, float fVol);
    static unsigned long PlayPowerupSound(ePowerUpType type, PowerupBase::PowerupSound powerupSnd, const nlVector3& v3Pos, float fVol);
    static void StopPowerupInEffectSound(SFXEmitter* pSFXEmitter);

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

class Bobomb : public PowerupBase
{
public:
    Bobomb(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode);
    virtual ~Bobomb();
    static void* operator new(size_t)
    {
        Bobomb* result = NULL;
        m_BobombSlotPool.Allocate(result);
        return result;
    }
    static void operator delete(void* ptr)
    {
        m_BobombSlotPool.Free((Bobomb*)ptr);
    }
    virtual void Update(float dt);
    virtual void ThrowAt(cFielder* pThrower, Bowser* pBowser);
    void Destroy(bool bSilent);

    /* 0x74 */ bool mbIsMine;
    /* 0x78 */ SFXEmitter* pMovementEmitter;

    static SlotPool<Bobomb> m_BobombSlotPool;
}; // total size: 0x7C

class FreezeShell : public PowerupBase
{
public:
    FreezeShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode);
    virtual ~FreezeShell();
    static void operator delete(void* ptr)
    {
        m_FreezeShellSlotPool.Free((FreezeShell*)ptr);
    }
    virtual void Update(float fDeltaT);
    void Destroy(bool bSilent);

    static SlotPool<FreezeShell> m_FreezeShellSlotPool;
}; // total size: 0x74

class SpinyShell : public PowerupBase
{
public:
    SpinyShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode);
    virtual ~SpinyShell();
    static void operator delete(void* ptr)
    {
        m_SpinyShellSlotPool.Free((SpinyShell*)ptr);
    }
    virtual void Update(float dt);
    void Destroy(bool bSilent);

    static SlotPool<SpinyShell> m_SpinyShellSlotPool;
}; // total size: 0x74

class Banana : public PowerupBase
{
public:
    Banana(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode);
    virtual ~Banana();
    static void operator delete(void* ptr)
    {
        m_BananaSlotPool.Free((Banana*)ptr);
    }
    virtual void Update(float dt);
    void Destroy(bool bSilent);

    static SlotPool<Banana> m_BananaSlotPool;
}; // total size: 0x74

class RedShell : public PowerupBase
{
public:
    RedShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode);
    virtual ~RedShell();
    static void operator delete(void* ptr)
    {
        m_RedShellSlotPool.Free((RedShell*)ptr);
    }
    virtual void Update(float dt);
    void Destroy(bool bSilent);
    void SeekTarget();

    static SlotPool<RedShell> m_RedShellSlotPool;
}; // total size: 0x74

class GreenShell : public PowerupBase
{
public:
    GreenShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode);
    virtual ~GreenShell();
    static void operator delete(void* ptr)
    {
        m_GreenShellSlotPool.Free((GreenShell*)ptr);
    }
    virtual void Update(float dt);
    void Destroy(bool bSilent);

    static SlotPool<GreenShell> m_GreenShellSlotPool;
}; // total size: 0x74

void CompactPowerups();
void InitializePowerups();
PowerupBase* FindPowerUp(unsigned long hashOfDrawable);
u8 PowerupCreateAndThrow(cFielder* pThrower, ePowerUpType eType, int nnumOfPowerups, Bowser* pBowser);
void PowerupThrowPosition(int nThrowOrder, eThrowStyle eStyle, PowerupBase* pNewPowerup, PowerupBase* pFirstPowerup);
cFielder* FindPowerupTarget(cFielder* pThrower, Bowser* pBowser);

// class PhysicsShell
// {
// public:
// };

// class PowerupModelPool
// {
// public:
// };

// class DrawableObject
// {
// public:
// };

struct PowerupUsedEventData : public EventData
{
    /* 0x04 */ ePowerUpType Type;
    /* 0x08 */ cPlayer* Thrower;
    /* 0x0C */ cPlayer* Target;
    u32 GetID() { return 0x1AF; }
};

struct PowerupHitPlayerEventData : public EventData
{
    /* 0x04 */ ePowerUpType Type;
    /* 0x08 */ cPlayer* Thrower;
    /* 0x0C */ cPlayer* Target;
    u32 GetID() { return 0x1B9; }
};

// class Format < BasicString < char, Detail
// {
// public:
// };

// class FormatImpl < BasicString < char, Detail
// {
// public:
// };

// class SlotPool<Bobomb>
// {
// public:
// };

// class SlotPool<Banana>
// {
// public:
// };

// class SlotPool<RedShell>
// {
// public:
// };

// class SlotPool<SpinyShell>
// {
// public:
// };

// class SlotPool<GreenShell>
// {
// public:
// };

// class SlotPool<FreezeShell>
// {
// public:
// };

#endif // _POWERUPS_H_
