#ifndef _AUDIOSCRIPTEVENTMGR_H_
#define _AUDIOSCRIPTEVENTMGR_H_

class AudioScriptEventMgr
{
public:
    enum AUDIO_EVENT
    {
        AE_Null = 0,
        AE_Goal = 1,
        AE_KickOff = 2,
        AE_Shot = 3,
        AE_HitPost = 4,
        AE_WinGame = 5,
        AE_SDWin = 6,
        AE_Attack = 7,
        AE_HitPlayer = 8,
        AE_Flyby = 9,
        AE_TeamIntro = 10,
        AE_PreKickOff = 11,
        AE_MissShot = 12,
        AE_GoalieSave = 13,
        AE_GoodPosition = 14,
        AE_WindUp = 15,
        AE_Halftime = 16,
        AE_LastPeriod = 17,
        AE_FinalSeconds = 18,
        AE_FinalSecondsTie = 19,
        AE_SuddenDeath = 20,
        AE_ShootToScore = 21,
        AE_CaptainS2S = 22,
        AE_S2SMiss = 23,
        AE_S2STackled = 24,
        AE_GenericS2SEnd = 25,
        AE_HyperStrike = 26,
        AE_SuperStrikeFloat = 27,
        AE_PowerUpActivate = 28,
        AE_PowerUpDisperse = 29,
        AE_PowerUpHit = 30,
        AE_BoredStart = 31,
        AE_BoredEnd = 32,
        AE_BoredPeriod = 33,
        AE_ChainChomp = 34,
        AE_ChainChompEnd = 35,
        AE_BowserAttackStart = 36,
        AE_BowserAttackEnd = 37,
        AE_BowserTilt = 38,
        AE_BowserLevel = 39,
        AE_BowserLandTilt = 40,
        AE_BowserLandReg = 41,
        AE_PerfectPass = 42,
        AE_PerfectPassEnd = 43,
        AE_LosingBadly = 44,
        AE_Comeback = 45,
        AE_GotPossession = 46,
        AE_COUNT = 47,
    };
    enum AUDIO_EVENT_TEAM
    {
        AET_Neutral = 0,
        AET_Home = 1,
        AET_Away = 2,
        AET_Special = 3,
    };

    static void Init();
    static void Purge();
    // PORT: static, like every other member here and like the one call site, which is qualified.
    static void Update();
    static void FireEvent(AudioScriptEventMgr::AUDIO_EVENT Event, AudioScriptEventMgr::AUDIO_EVENT_TEAM Team);
};

struct AUDIO_EVENT_RECORD;

class _AudioEventRaiser
{
public:
    void RaiseEvent(AUDIO_EVENT_RECORD* pEvent);
};

#endif // _AUDIOSCRIPTEVENTMGR_H_
