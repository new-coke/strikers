#ifndef _BALL_H_
#define _BALL_H_

#include <types.h>
#include "NL/nlMath.h"
#include "NL/nlTimer.h"

#include "Game/CharacterTriggers.h"

class BlurHandler;
class DrawableObject;
class PhysicsAIBall;
class RayCollider;
class cFielder;
class cPlayer;

enum eSpinType
{
    SPINTYPE_NONE = 0,
    SPINTYPE_BACK = 1,
    SPINTYPE_FORWARD = 2,
    SPINTYPE_ROLLING = 3,
    SPINTYPE_PARAMETER = 4,
};

class cBall
{
public:
    PhysicsAIBall* GetPhysicsBall()
    {
        return m_pPhysicsBall;
    }

    DrawableObject* GetDrawableBall() const
    {
        return m_pDrawableBall;
    }

    const nlVector3& GetPosition() const
    {
        return m_v3Position;
    }

    const nlVector3& GetVelocity() const
    {
        return m_v3Velocity;
    }

    float PredictLandingSpotAndTime(nlVector3& v3Dest);
    void KillBlurHandler();
    void ClearPassTarget();
    void SetPassTargetTimer(float seconds);
    void SetPassTarget(cPlayer* passTargetPlayer, const nlVector3& pos, bool bVolley);
    void SetAngularVelocity(const nlVector3& v3Velocity);
    void WarpTo(const nlVector3& toPos);
    void UpdateOrientation(float fDeltaT);
    void Update(float fDeltaT);
    void ShootAtFast(nlVector3& v3Vel, const nlVector3& v3Target, float fDesiredTime);
    void ShootRelease(const nlVector3& v3Velocity, eSpinType SpinType);
    void Shoot(const nlVector3& v3Dir, const nlVector3& v3Spin, eSpinType spinType, bool bCanDamage, bool bParam5, bool bParam6);
    void SetVisible(bool visible);
    void SetVelocity(const nlVector3& velocity, eSpinType spin, const nlVector3* pAngularVelocity);
    void SetPerfectPass(bool bFlag, bool bNoEvent);
    void SetPosition(const nlVector3& pos);
    void SetOwner(cPlayer* pOwner);
    bool IsBuzzerBeaterSet() const;
    void HandleBuzzerBeater(float seconds);
    void ClearBallBlur();
    void ClearShotInProgress();
    void InitiateBallBlur(eBallShotEffectType effectType, cPlayer* pPlayer);
    bool GetInNet(int& nSide);
    cPlayer* GetPassTargetFielder() const;
    cPlayer* GetOwnerGoalie();
    cFielder* GetOwnerFielder();
    nlVector3* GetDrawablePosition() const;
    nlVector3* GetAIVelocity() const;
    void PostPhysicsUpdate(float fDeltaT);
    void CollideWithWallCallback();
    void CollideWithGroundCallback();
    void CollideWithCharacterCallback(cPlayer* pCharacter, const nlVector3& v3PreBallVelocity);
    void ClearBallEffects();
    void ClearOwner();

    ~cBall();
    cBall();

    /* 0x00 */ u32 m_bBallPathChangeCount;
    /* 0x04 */ u32 m_bBallDeflectCount;
    /* 0x08 */ Timer m_tShotTimer;
    /* 0x0C */ Timer m_tNoPickupTimer;
    /* 0x10 */ Timer m_tPassTargetTimer;
    /* 0x14 */ Timer m_tBuzzerBeaterTimer;
    /* 0x18 */ float m_fTotalPassTime;

    /* 0x1C */ BlurHandler* m_pBlurHandler;
    /* 0x20 */ DrawableObject* m_pDrawableBall;
    /* 0x24 */ cPlayer* m_pOwner;
    /* 0x28 */ cPlayer* m_pPrevOwner;

    /* 0x2C */ cPlayer* m_pLastTouch;
    /* 0x30 */ cPlayer* m_pPassTarget;
    /* 0x34 */ cPlayer* m_pShooter;

    /* 0x38 */ PhysicsAIBall* m_pPhysicsBall;
    /* 0x3C */ RayCollider* m_pBallPosCollider;

    /* 0x40 */ nlVector3 m_v3Position;
    /* 0x4C */ nlVector3 m_v3PrevPosition;
    /* 0x58 */ nlVector3 m_v3Velocity;      // 0x58, 0x5C, 0x60
    /* 0x64 */ nlVector3 m_v3PassIntercept; // 0x64, 0x68, 0x6C

    /* 0x70 */ nlQuaternion m_qOrientation; // 0x70, 0x74, 0x78, 0x7C
    /* 0x80 */ nlVector3 m_v3ShotTarget;    // 0x80, 0x84, 0x88
    /* 0x8C */ nlVector3 m_v3ShotOrigin;    // 0x8C, 0x90, 0x94
    /* 0x98 */ u32 m_uGoalType;
    /* 0x9C */ u32 m_uVoiceID;

    // <--- confirmed until here

    // /* 0x9C */ bool mbIsVolleyPass;
    // /* 0x9D */ bool mbIsPerfectPass;
    // /* 0x9E */ bool mbIsShootToScoreShot;
    // /* 0x9F */ bool mbIsChipShot;
    /* 0xA0 */ bool mbIsPerfectShot;
    /* 0xA1 */ bool mbHyperSTS;
    /* 0xA2 */ bool mbCanDamage;

    /* 0xA3 */ bool m_unk_0xA3;
    /* 0xA4 */ bool m_unk_0xA4;
    /* 0xA5 */ bool m_unk_0xA5;
    /* 0xA6 */ bool m_unk_0xA6;

    /* 0xA8 */ cPlayer* mpDamageTarget;

    // /* 0xA4 */ cPlayer* mpDamageTarget;
    // /* 0xA8 */ u32 m_unk_0xA8;

    // /* 0x00 */ u32 m_unk_0x00;
    // /* 0x04 */ u32 m_unk_0x04;

    // /* 0x08 */ Timer* m_timer_0x08;
    // /* 0x0C */ Timer* m_timer_0x0C;
    // /* 0x10 */ Timer* m_passTargetTimer; // passTargetTimer
    // /* 0x14 */ Timer* m_timer_0x14;
    // /* 0x18 */ float m_passTimeSeconds; // m_unk_0x18;

    // /* 0x1C */ BlurHandler* m_blurHandler;

    // /* 0x20 */ DrawableObject* m_drawableObject;
    // /* 0x24 */ cPlayer* m_playerOwner;
    // /* 0x28 */ cPlayer* m_playerPrevOwner;
    // /* 0x2C */ u8 m_unk_0x2C[0x4];
    // /* 0x30 */ cPlayer* m_passTargetPlayer;
    // /* 0x34 */ u8 m_unk_0x34[0x4];
    // /* 0x38 */ PhysicsAIBall* m_aiBall;
    // /* 0x3C */ RayCollider* m_rayCollider;
    // /* 0x40 */ nlVector3 m_rayPosition;
    // /* 0x4C */ nlVector3 m_unk_0x4C;
    // /* 0x58 */ nlVector3 m_unk_0x58; // playerVelocity?
    // /* 0x64 */ nlVector3 m_unk_0x64; // passTargetPosition?
    // /* 0x70 */ u8 m_unk_0x70[0x32];
    // /* 0xA2 */ u8 m_unk_0xA2;
    // /* 0xA3 */ u8 m_unk_0xA3;
    // /* 0xA4 */ u8 m_unk_0xA4;
    // /* 0xA5 */ u8 m_unk_0xA5;
    // /* 0xA6 */ u8 m_unk_0xA6;
    // /* 0xA7 */ u8 m_unk_0xA7;
    // /* 0xA8 */ u8 m_unk_0xA8;
}; // total size: 0xA8

extern cBall* g_pBall;

// class BasicString<char, Detail
// {
// public:
// };

// class FuzzyVariant
// {
// public:
// };

// class Variant
// {
// public:
// };

// class PassBallData
// {
// public:
// };

// class EventData
// {
// public:
// };

#endif // _BALL_H_
