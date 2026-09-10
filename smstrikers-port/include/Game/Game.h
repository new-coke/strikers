#ifndef _GAME_H_
#define _GAME_H_

#include "NL/nlMath.h"
#include "NL/nlList.h"
#include "Game/Player.h"

#include "Game/GameTweaks.h"
#include "Game/ScriptTuning.h"

void DestroyPowerups();
void DestroyGame();
void CreateGame();

class Clock;
class BaseTarget
{
public:
    BaseTarget();
    ~BaseTarget();
};

enum eGameState
{
    GS_NONE = -1,
    GS_PRE_GAME = 0,
    GS_KICKOFF = 1,
    GS_POST_GOAL = 2,
    GS_END_GAME = 3,
    GS_GAMEPLAY = 4,
    GS_OVERTIME = 5,
};

class cGame
{
public:
    cGame();
    virtual ~cGame();
    void DoPerfectPassSlowDown();
    float GetNormalizedGameTime();
    float GetGameTime();
    void ResetForKickOff();
    void RandomizePlayerUpdateOrder();
    void ResetCharacters();
    void ResetBall();
    void ResetGameClock();
    static void PostResetCallback(uintptr_t userData, unsigned long clockId);
    void BeginGame(bool bRematch, bool bStraightToKickoff);
    void CheckForGoal();
    static void EnterPostGame();
    void BlowUpPowerups(const nlVector3& v3ExplosionPosition, float fExplosionRadius);
    void BlowUpPlayers(cFielder* pShooter, float fExplosionRadius);
    void ResetPowerups(bool clearPowerUps);
    void ResetBowser();
    void ResetBowserTimer(float seconds);
    void PreUpdate(float deltaTime);
    static void UpdatePowerUpObjects(float fDeltaT);
    void Update(float deltaTime);
    void ResetScorerInfo();
    void SetPotentialScorer(cPlayer* pPlayer);
    void ChangeGameState(eGameState state);
    void InitGameState(eGameState state);
    bool IsThoughtAllowed(unsigned long thought_id);
    bool AbortPendingThought(unsigned long thoughtHash);
    void SetDifficulty(eDifficultyID diff0, eDifficultyID diff1, eDifficultyID diff2);

    inline GameTweaks* GetGameTweaks() { return m_pGameTweaks; }

    inline bool IsGameplayOrOvertime()
    {
        return (m_eGameState == GS_GAMEPLAY || m_eGameState == GS_OVERTIME);
    }

    inline eGameState GetGameState() const { return m_eGameState; }

    /* 0x04 */ GameTweaks* m_pGameTweaks;
    /* 0x08 */ FuzzyTweaks* m_pFuzzyTweaks;
    /* 0x0C */ Clock* m_pGameClock;
    /* 0x10 */ bool m_bBallInNet;
    /* 0x14 */ Clock* m_pPostResetClock;
    /* 0x18 */ Clock* m_pPostGameDoneClock;
    /* 0x1C */ float m_fGameDuration;
    /* 0x20 */ int m_nLastTeamToScore;
    /* 0x24 */ eGameState m_eGameState;
    /* 0x28 */ BaseTarget* m_pTarget;
    /* 0x2C */ cPlayer* m_pScorer;
    /* 0x30 */ cPlayer* m_pAssister;
    /* 0x34 */ cPlayer* m_pTeamTouch[2];
    /* 0x3C */ cPlayer** m_pRandomPlayersArray;
    /* 0x40 */ bool mbCaptainShotToScoreOn;
    /* 0x41 */ bool mIsPure;
    /* 0x42 */ bool mInSuddenDeath;
    /* 0x44 */ Timer mBowserTimer;
    /* 0x48 */ f32 mfCheatTilt;
    /* 0x4C */ nlListContainer<unsigned long> mThoughtsQueue;
    /* 0x58 */ int mThoughtsAllowedThisUpdate;
}; // total size: 0x5C

extern cGame* g_pGame;

#endif // _GAME_H_
