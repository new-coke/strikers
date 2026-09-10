#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Audio/AudioScriptEventMgr.h"
#include "Game/Audio/SoundEventScript.h"
#include "Game/EventDataTypes.h"
#include "Game/Game.h"
#include "Game/Goalie.h"
#include "Game/Physics/PhysicsNet.h"
#include "Game/Sys/EventData.h"
#include "Game/Team.h"

#include "Game/AI/FielderActions.h"
#include "Game/AI/FuzzyVariant.h"
#include "Game/AI/Powerups.h"
#include "Game/AI/Variant.h"
#include "Game/AI/Scripts/CommonScript.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/Render/Presentation.h"

#include "NL/nlAlgorithm.h"
#include "NL/nlConfig.h"
#include "NL/nlList.h"
#include "NL/nlSlotPool.h"
#include "NL/nlString.h"
#include "NL/WalkHelper.h"

struct AUDIO_SCRIPT_POLL_STATE
{
    /* 0x00 */ float NextPossibleGoodPositionTime;
    /* 0x04 */ cTeam* pLastBallOwnerTeam;
    /* 0x08 */ float NoOwnerTime;
    /* 0x0C */ unsigned long GameTimeToggle;
    /* 0x10 */ float LastExcitementTime;
    /* 0x14 */ unsigned char AmBored : 1;
    /* 0x18 */ cTeam* LastOwningTeam;
};

AUDIO_SCRIPT_POLL_STATE g_ScriptPollState;

struct AUDIO_SCRIPT_SETTINGS
{
    /* 0x00 */ unsigned long HalfTime;
    /* 0x04 */ unsigned long LastPeriod;
    /* 0x08 */ unsigned long FinalSeconds;
    /* 0x0C */ float GoodToShootThreshold;
    /* 0x10 */ float OnBreakawayThreshold;
    /* 0x14 */ unsigned long MinGoodPositionPeriod;
    /* 0x18 */ float TimeToBored;
    /* 0x1C */ float BoredPeriod;
    /* 0x20 */ unsigned long MaxFreeBallTime;
};

static AUDIO_SCRIPT_SETTINGS g_ScriptSettings = { 0 };

struct AUDIO_EVENT_RECORD
{
    /* 0x0 */ AudioScriptEventMgr::AUDIO_EVENT Event : 16;
    /* 0x2 */ AudioScriptEventMgr::AUDIO_EVENT_TEAM Team : 16;
};

struct NIS_EVENT_LOOKUP
{
    /* 0x0 */ unsigned long Hash;
    /* 0x4 */ const char* Name;
    /* 0x8 */ AUDIO_EVENT_RECORD Event;

    operator unsigned long() const { return Hash; }
};

#define NIS_EVENT_PACK(event, team) \
    { (AudioScriptEventMgr::AUDIO_EVENT)(event), (AudioScriptEventMgr::AUDIO_EVENT_TEAM)(team) }

NIS_EVENT_LOOKUP g_NisEventLookup[4] = {
    { 0, "Flyby", NIS_EVENT_PACK(AudioScriptEventMgr::AE_Flyby, AudioScriptEventMgr::AET_Neutral) },
    { 0, "PreKickOff", NIS_EVENT_PACK(AudioScriptEventMgr::AE_PreKickOff, AudioScriptEventMgr::AET_Neutral) },
    { 0, "EnterStadiumHome", NIS_EVENT_PACK(AudioScriptEventMgr::AE_TeamIntro, AudioScriptEventMgr::AET_Home) },
    { 0, "EnterStadiumAway", NIS_EVENT_PACK(AudioScriptEventMgr::AE_TeamIntro, AudioScriptEventMgr::AET_Away) },
};

class NisEventLookup
{
public:
    NisEventLookup()
    {
        for (unsigned int i = 0; i < 4; i++)
        {
            g_NisEventLookup[i].Hash = nlStringLowerHash(g_NisEventLookup[i].Name);
        }
        nlQSort<NIS_EVENT_LOOKUP>(g_NisEventLookup, 4, &nlDefaultQSortComparer<NIS_EVENT_LOOKUP>);
    }
};

static NisEventLookup g_NisEvents;

char* AUDIO_EVENT_FUNC_NAMES[] = {
    "NULL",
    "Goal",
    "KickOff",
    "Shot",
    "Post",
    "Win",
    "SDWin",
    "Attack",
    "Hit",
    "OpeningFlyby",
    "Intro",
    "PreKickOff",
    "Miss",
    "Save",
    "GoodPosition",
    "WindUp",
    "HalfTime",
    "LastPeriod",
    "FinalSecondsLeading",
    "FinalSecondsCloseScore",
    "SuddenDeath",
    "ShootToScore",
    "CaptainS2S",
    "S2SMiss",
    "S2STackled",
    "GenericS2SEnd",
    "HyperStrike",
    "SuperStrikeFloat",
    "PowerUpActivate",
    "PowerUpDisperse",
    "PowerUpHit",
    "BoredStart",
    "BoredEnd",
    "BoredPeriod",
    "ChainChomp",
    "ChainChompEnd",
    "BowserAttackStart",
    "BowserAttackEnd",
    "BowserTilt",
    "BowserLevel",
    "BowserLandTilt",
    "BowserLandReg",
    "PerfectPass",
    "PerfectPassEnd",
    "LosingBadly",
    "Comeback",
    "GotPossession",
};

typedef ListContainerBase<AUDIO_EVENT_RECORD, BasicSlotPool<ListEntry<AUDIO_EVENT_RECORD> > > AudioEventList;

nlListSlotPool<AUDIO_EVENT_RECORD> g_PendingEvents(0x10, 0x10);
EventHandler* g_pAudioEventHandler;

static inline AudioScriptEventMgr::AUDIO_EVENT_TEAM GetPlayerTeam(cPlayer* pPlayer)
{
    AudioScriptEventMgr::AUDIO_EVENT_TEAM team = AudioScriptEventMgr::AET_Home;
    if (pPlayer->m_pTeam->m_nSide != 0)
    {
        team = AudioScriptEventMgr::AET_Away;
    }
    return team;
}

static unsigned char g_InBowserAttack;
static unsigned char g_InGoal;
_AudioEventRaiser g_AudioEventRaiser;

static void AudioScriptEventHandler(Event*, void*);
static void Poll();

typedef WalkHelper<AUDIO_EVENT_RECORD, ListEntry<AUDIO_EVENT_RECORD>, _AudioEventRaiser> AudioEventWalkHelper;

/**
 * Offset/Address/Size: 0x2248 | 0x8014B39C | size: 0xD0
 */
template <typename T>
static AudioScriptEventMgr::AUDIO_EVENT_TEAM GetEventTeam(Event* pEvent, bool Invert)
{
    T* pEventData;
    pEvent->GetData(&pEventData);

    AudioScriptEventMgr::AUDIO_EVENT_TEAM team = AudioScriptEventMgr::AET_Home;
    if (pEventData->uTeamIndex != 0)
    {
        team = AudioScriptEventMgr::AET_Away;
    }

    s32 invertMask = Invert ? (s32)AudioScriptEventMgr::AET_Special : 0;
    return (AudioScriptEventMgr::AUDIO_EVENT_TEAM)((s32)team ^ invertMask);
}

/**
 * Offset/Address/Size: 0x1BAC | 0x8014AD00 | size: 0x5CC
 */
void AudioScriptEventMgr::Init()
{
    g_pAudioEventHandler = g_pEventManager->AddEventHandler(AudioScriptEventHandler, NULL, (unsigned long)-1);

    AUDIO_SCRIPT_SETTINGS& settings = (AUDIO_SCRIPT_SETTINGS&)g_ScriptSettings;

    settings.HalfTime = GetConfigInt(Config::Global(), "HalfTime", 50);
    settings.LastPeriod = GetConfigInt(Config::Global(), "LastPeriod", 75);
    settings.FinalSeconds = GetConfigInt(Config::Global(), "FinalSeconds", 93);
    settings.GoodToShootThreshold = GetConfigFloat(Config::Global(), "GoodToShoot", 0.5f);
    settings.OnBreakawayThreshold = GetConfigFloat(Config::Global(), "OnBreakaway", 0.5f);
    settings.MinGoodPositionPeriod = (unsigned long)GetConfigFloat(Config::Global(), "MinGoodPositionPeriod", 5.0f);
    settings.TimeToBored = GetConfigFloat(Config::Global(), "TimeToBored", 12.0f);
    settings.BoredPeriod = GetConfigFloat(Config::Global(), "BoredPeriod", 12.0f);
    settings.MaxFreeBallTime = (unsigned long)(GetConfigFloat(Config::Global(), "MaxFreeBallTime", 1000.0f) / 1000.0f);

    memset(&g_ScriptPollState, 0, sizeof(AUDIO_SCRIPT_POLL_STATE));
}

/**
 * Offset/Address/Size: 0x1B18 | 0x8014AC6C | size: 0x94
 */
void AudioScriptEventMgr::Purge()
{
    g_PendingEvents.Clear();
    SlotPoolBase::BaseFreeBlocks(&g_PendingEvents.m_Allocator, sizeof(ListEntry<AUDIO_EVENT_RECORD>));

    if (g_pAudioEventHandler != NULL)
    {
        g_pEventManager->RemoveEventHandler(g_pAudioEventHandler);
    }
    g_pAudioEventHandler = NULL;
}

void RaiseEvents();

/**
 * Offset/Address/Size: 0x1A48 | 0x8014AB9C | size: 0xD0
 */
void AudioScriptEventMgr::Update()
{
    Poll();
    RaiseEvents();
}

void RaiseEvents()
{
    AudioEventWalkHelper helper;
    helper.m_CBClass = &g_AudioEventRaiser;
    helper.m_CB = &_AudioEventRaiser::RaiseEvent;
    nlWalkList(g_PendingEvents.m_Head, &helper, &AudioEventWalkHelper::Callback);
    g_PendingEvents.Clear();
}

/**
 * Offset/Address/Size: 0x19B8 | 0x8014AB0C | size: 0x90
 */
WEAKFUNC void _AudioEventRaiser::RaiseEvent(AUDIO_EVENT_RECORD* pEvent)
{
    char FuncName[64];
    memcpy(FuncName, "Crowd", 5);
    nlStrNCpy(FuncName + 5, AUDIO_EVENT_FUNC_NAMES[pEvent->Event], 0x3b);
    if (pEvent->Team != 0)
    {
        const char* suffix = pEvent->Team == 1 ? "Home" : "Away";
        nlStrNCat(FuncName, FuncName, suffix, 0x40);
    }
    SoundEventScript::Instance().Call(FuncName);
}

/**
 * Offset/Address/Size: 0x1904 | 0x8014AA58 | size: 0xB4
 */
void AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AUDIO_EVENT Event, AudioScriptEventMgr::AUDIO_EVENT_TEAM Team)
{
    AUDIO_EVENT_RECORD aer = { Event, Team };
    g_PendingEvents.AddEnd(aer);
}

static inline void RecordExcitingEventInline()
{
    g_ScriptPollState.LastExcitementTime = g_pGame->GetGameTime();

    if (g_ScriptPollState.AmBored)
    {
        nlPrintf("END bored\n");
        g_ScriptPollState.AmBored = 0;
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BoredEnd, AudioScriptEventMgr::AET_Neutral);
    }
}

/**
 * Offset/Address/Size: 0x10F8 | 0x8014A24C | size: 0x80C
 */
static void Poll()
{
    GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;
    unsigned long gameTimePercentage = (unsigned long)((100.0f * g_pGame->GetGameTime()) / pGameTweaks->fGameDuration);
    AudioScriptEventMgr::AUDIO_EVENT event = (AudioScriptEventMgr::AUDIO_EVENT)-1;
    AudioScriptEventMgr::AUDIO_EVENT_TEAM team = AudioScriptEventMgr::AET_Neutral;

    if (gameTimePercentage == g_ScriptSettings.HalfTime)
    {
        event = AudioScriptEventMgr::AE_Halftime;
        g_ScriptPollState.GameTimeToggle++;
    }
    else if (gameTimePercentage == g_ScriptSettings.LastPeriod)
    {
        event = AudioScriptEventMgr::AE_LastPeriod;
        g_ScriptPollState.GameTimeToggle++;
    }
    else if (gameTimePercentage == g_ScriptSettings.FinalSeconds)
    {
        cTeam* pHome = g_pTeams[0];
        cTeam* pAway = g_pTeams[1];
        int awayScore = pAway->m_nScore;
        int homeScore = pHome->m_nScore;

        if (homeScore == awayScore)
        {
            event = AudioScriptEventMgr::AE_FinalSecondsTie;
        }
        else
        {
            event = AudioScriptEventMgr::AE_FinalSeconds;
            int winnerTeam = 2;
            if (homeScore > awayScore)
            {
                winnerTeam = 1;
            }
            team = (AudioScriptEventMgr::AUDIO_EVENT_TEAM)winnerTeam;
        }

        g_ScriptPollState.GameTimeToggle++;
    }
    else if (g_pGame->m_eGameState != GS_OVERTIME && g_pGame->m_eGameState == GS_END_GAME)
    {
        if (++g_ScriptPollState.GameTimeToggle == 1)
        {
            g_pEventManager->CreateValidEvent(4, 0x14);
        }
    }
    else
    {
        g_ScriptPollState.GameTimeToggle = 0;
    }

    if (g_ScriptPollState.GameTimeToggle == 1 && event != (AudioScriptEventMgr::AUDIO_EVENT)-1)
    {
        AudioScriptEventMgr::FireEvent(event, team);
    }

    cFielder* pBallOwner = g_pBall->GetOwnerFielder();
    if (pBallOwner != NULL)
    {
        cTeam* pOwnerTeam = ((cPlayer*)pBallOwner)->m_pTeam;

        if (pOwnerTeam != g_ScriptPollState.pLastBallOwnerTeam)
        {
            g_ScriptPollState.pLastBallOwnerTeam = pOwnerTeam;
            g_ScriptPollState.NextPossibleGoodPositionTime = g_pGame->GetGameTime() - 0.0001;
        }

        if (Fuzzy::GoodToShoot(pBallOwner).mData.f >= g_ScriptSettings.GoodToShootThreshold || OnBreakaway(pBallOwner) >= g_ScriptSettings.OnBreakawayThreshold)
        {
            if (g_ScriptPollState.NextPossibleGoodPositionTime <= g_pGame->GetGameTime())
            {
                g_ScriptPollState.NextPossibleGoodPositionTime = g_pGame->GetGameTime() + (float)g_ScriptSettings.MinGoodPositionPeriod;
                AudioScriptEventMgr::AUDIO_EVENT_TEAM eventTeam = AudioScriptEventMgr::AET_Home;

                if ((unsigned int)((cPlayer*)pBallOwner)->m_pTeam->m_nSide != 0)
                {
                    eventTeam = AudioScriptEventMgr::AET_Away;
                }

                AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_GoodPosition, eventTeam);
                RecordExcitingEventInline();
            }
        }
    }

    if (g_pBall->m_pOwner != NULL)
    {
        if (g_pBall->m_pOwner->m_pTeam != g_ScriptPollState.LastOwningTeam)
        {
            RecordExcitingEventInline();

            g_ScriptPollState.LastOwningTeam = g_pBall->m_pOwner->m_pTeam;

            if (g_pGame->m_eGameState > GS_END_GAME)
            {
                AudioScriptEventMgr::AUDIO_EVENT_TEAM eventTeam = AudioScriptEventMgr::AET_Home;
                if ((unsigned int)g_pBall->m_pOwner->m_pTeam->m_nSide != 0)
                {
                    eventTeam = AudioScriptEventMgr::AET_Away;
                }
                AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_GotPossession, eventTeam);
            }

            g_ScriptPollState.NoOwnerTime = -1.0f;
        }
    }
    else if (g_ScriptPollState.NoOwnerTime < 0.0f)
    {
        g_ScriptPollState.NoOwnerTime = g_pGame->GetGameTime();
    }

    float timeSinceExcitement = g_pGame->GetGameTime() - g_ScriptPollState.LastExcitementTime;
    if (timeSinceExcitement > g_ScriptSettings.TimeToBored && !g_ScriptPollState.AmBored)
    {
        g_ScriptPollState.AmBored = 1;
        g_ScriptPollState.LastExcitementTime = g_pGame->GetGameTime();
        nlPrintf("START bored %.2f\n", timeSinceExcitement);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BoredStart, AudioScriptEventMgr::AET_Neutral);
    }
    else if (timeSinceExcitement > g_ScriptSettings.BoredPeriod && g_ScriptPollState.AmBored)
    {
        g_ScriptPollState.LastExcitementTime = g_pGame->GetGameTime();
        nlPrintf("PERIOD bored %.2f\n", timeSinceExcitement);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BoredPeriod, AudioScriptEventMgr::AET_Neutral);
    }
}

/**
 * Offset/Address/Size: 0xFF8 | 0x8014A14C | size: 0x100
 */
WEAKFUNC void RecordExcitingEvent()
{
    g_ScriptPollState.LastExcitementTime = g_pGame->GetGameTime();

    if (g_ScriptPollState.AmBored)
    {
        nlPrintf("END bored\n");
        g_ScriptPollState.AmBored = 0;
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BoredEnd, AudioScriptEventMgr::AET_Neutral);
    }
}

static inline bool IsGameplayOrOvertime(cGame* pGame)
{
    bool inGameplay = false;
    if (pGame->m_eGameState == GS_GAMEPLAY || pGame->m_eGameState == GS_OVERTIME)
    {
        inGameplay = true;
    }
    return inGameplay;
}

/**
 * Offset/Address/Size: 0x0 | 0x80149154 | size: 0xFF8
 */
static void AudioScriptEventHandler(Event* pEvent, void*)
{
    if (g_pBall == NULL)
    {
        return;
    }

    if (!AudioLoader::IsInited())
    {
        return;
    }

    switch (pEvent->m_uEventID)
    {
    case 32:
    {
        CollisionBallWallData* pData;
        pEvent->GetData(&pData);
        if (!pData->bIsShot)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_MissShot,
            pData->position.x > 0.0f ? AudioScriptEventMgr::AET_Home : AudioScriptEventMgr::AET_Away);
        return;
    }
    case 46:
    {
        if (!Audio::IsWorldSFXLoaded())
        {
            return;
        }
        CollisionBallGoalpostData* pData;
        pEvent->GetData(&pData);
        nlVector3 vel = pData->v3CollisionVelocity;
        float speed = nlSqrt(vel.z * vel.z + (vel.x * vel.x + vel.y * vel.y), true);
        if (speed > 35.0f)
        {
            speed = 35.0f;
        }
        if (speed < 1.0f)
        {
            return;
        }
        float intensity = speed / 35.0f;
        if (intensity < 0.3f)
        {
            intensity = 0.3f;
        }

        static float fTimer;
        static signed char init;

        intensity = intensity * Audio::gStadGenSFX.GetSFXInfo(Audio::STADSFX_GEN_FIREWORKS_FLOOR).fVolume;
        if (!init)
        {
            init = true;
            fTimer = 0.0f;
        }

        float now = Audio::GetAudioTimer();
        if (now < fTimer)
        {
            fTimer = 0.0f;
        }
        if (fTimer != 0.0f && now - fTimer < 0.2f)
        {
            return;
        }

        fTimer = Audio::GetAudioTimer();
        Audio::gStadGenSFX.Play(Audio::STADSFX_GEN_FIREWORKS_FLOOR, intensity, -1.0f, true, 100.0f);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_HitPost, GetEventTeam<CollisionBallGoalpostData>(pEvent, false));
        return;
    }
    case 11:
        g_InBowserAttack = 0;
        g_InGoal = 0;
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_KickOff, AudioScriptEventMgr::AET_Neutral);
        return;
    case 5:
        g_InBowserAttack = 0;
        g_InGoal = 1;
        if (g_pGame->mInSuddenDeath)
        {
            AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_SDWin, GetEventTeam<GoalScoredData>(pEvent, false));
        }
        else
        {
            AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_Goal, GetEventTeam<GoalScoredData>(pEvent, false));
        }
        return;
    case 4:
    {
        cTeam* pHome = g_pTeams[0];
        cTeam* pAway = g_pTeams[1];
        AudioScriptEventMgr::AUDIO_EVENT_TEAM team = AudioScriptEventMgr::AET_Away;
        if (pHome->m_nScore > pAway->m_nScore)
        {
            team = AudioScriptEventMgr::AET_Home;
        }

        AudioScriptEventMgr::AUDIO_EVENT event = AudioScriptEventMgr::AE_WinGame;
        if (g_pGame->mInSuddenDeath)
        {
            event = AudioScriptEventMgr::AE_SDWin;
        }

        AudioScriptEventMgr::FireEvent(event, team);
        return;
    }
    case 63:
    {
        ShotAtGoalData* pData;
        pEvent->GetData(&pData);
        if (pData->pShooter->IsCaptain())
        {
            AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_CaptainS2S,
                (unsigned int)pData->pShooter->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        }
        else
        {
            AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_ShootToScore,
                (unsigned int)pData->pShooter->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        }
        return;
    }
    case 65:
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_GenericS2SEnd, AudioScriptEventMgr::AET_Neutral);
        return;
    case 68:
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_SuperStrikeFloat,
            (unsigned int)g_pBall->m_pOwner->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    case 67:
    {
        ShotAtGoalData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_HyperStrike,
            (unsigned int)pData->pShooter->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 15:
    {
        GoalieSaveData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_GoalieSave,
            (AudioScriptEventMgr::AUDIO_EVENT_TEAM)(((unsigned int)pData->pGoalie->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home) ^ AudioScriptEventMgr::AET_Special));
        return;
    }
    case 20:
    {
        ShotAtGoalData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_Shot,
            (unsigned int)pData->pShooter->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        void RecordExcitingEvent();
        RecordExcitingEvent();
        return;
    }
    case 22:
    case 24:
    {
        PlayerAttackData* pData;
        pEvent->GetData(&pData);
        if (pData->pTarget == NULL)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_Attack,
            (unsigned int)((cPlayer*)pData->pAttacker)->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 23:
    case 26:
    {
        PlayerAttackData* pData;
        pEvent->GetData(&pData);
        if (pData->pTarget == NULL)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_HitPlayer,
            (AudioScriptEventMgr::AUDIO_EVENT_TEAM)(((unsigned int)((cPlayer*)pData->pAttacker)->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home) ^ AudioScriptEventMgr::AET_Special));
        return;
    }
    case 21:
    {
        ShotAtGoalData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_WindUp,
            (unsigned int)pData->pShooter->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 69:
    {
        PassBallData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_PerfectPass,
            (unsigned int)pData->pPasser->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 71:
    {
        PassBallData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_PerfectPassEnd,
            (unsigned int)pData->pPasser->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 51:
    {
        ShotAtGoalData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_ChainChomp,
            (unsigned int)pData->pShooter->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 52:
    {
        ShotAtGoalData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_ChainChompEnd,
            (unsigned int)pData->pShooter->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 53:
        if (GetConfigBool(Config::Global(), "e3_build", false))
        {
            return;
        }
        if (!IsGameplayOrOvertime(g_pGame))
        {
            return;
        }
        if (g_InGoal)
        {
            return;
        }
        g_InBowserAttack = 1;
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BowserAttackStart, AudioScriptEventMgr::AET_Neutral);
        return;
    case 59:
        if (GetConfigBool(Config::Global(), "e3_build", false))
        {
            return;
        }
        if (!g_InBowserAttack)
        {
            return;
        }
        if (g_InGoal)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BowserAttackEnd, AudioScriptEventMgr::AET_Neutral);
        return;
    case 56:
        if (GetConfigBool(Config::Global(), "e3_build", false))
        {
            return;
        }
        if (g_InGoal)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BowserTilt, AudioScriptEventMgr::AET_Neutral);
        return;
    case 58:
        if (GetConfigBool(Config::Global(), "e3_build", false))
        {
            return;
        }
        if (g_InGoal)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BowserLevel, AudioScriptEventMgr::AET_Neutral);
        return;
    case 57:
        if (g_InGoal)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BowserLandTilt, AudioScriptEventMgr::AET_Neutral);
        return;
    case 54:
        if (g_InGoal)
        {
            return;
        }
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_BowserLandReg, AudioScriptEventMgr::AET_Neutral);
        return;
    case 29:
    {
        PowerupUsedEventData* pData;
        pEvent->GetData(&pData);
        if (pData->Thrower == NULL)
        {
            return;
        }
        AudioScriptEventMgr::AUDIO_EVENT_TEAM team = (unsigned int)pData->Thrower->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home;
        AudioScriptEventMgr::AUDIO_EVENT event = AudioScriptEventMgr::AE_PowerUpActivate;
        if (pData->Type <= 6)
        {
            event = AudioScriptEventMgr::AE_PowerUpDisperse;
        }
        AudioScriptEventMgr::FireEvent(event, team);
        return;
    }
    case 30:
    {
        PowerupHitPlayerEventData* pData;
        pEvent->GetData(&pData);
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_PowerUpHit,
            (unsigned int)pData->Target->m_pTeam->m_nSide != 0 ? AudioScriptEventMgr::AET_Away : AudioScriptEventMgr::AET_Home);
        return;
    }
    case 12:
        AudioScriptEventMgr::FireEvent(AudioScriptEventMgr::AE_SuddenDeath, AudioScriptEventMgr::AET_Neutral);
        return;
    case 86:
    {
        NISData* pData;
        pEvent->GetData(&pData);
        NIS_EVENT_LOOKUP* pFound = nlBSearch<NIS_EVENT_LOOKUP, unsigned long>(
            nlStringLowerHash(pData->Type), g_NisEventLookup, 4);
        AUDIO_EVENT_RECORD* pRecord = pFound != NULL ? &pFound->Event : NULL;
        if (pRecord != NULL)
        {
            AudioScriptEventMgr::FireEvent(
                (AudioScriptEventMgr::AUDIO_EVENT)pRecord->Event,
                (AudioScriptEventMgr::AUDIO_EVENT_TEAM)pRecord->Team);
        }
        else
        {
            if (nlStrCmp(pData->Param, "MusicGoal") == 0 && g_pGame->mInSuddenDeath)
            {
                pData->Param = "MusicSDGoal";
            }
            SoundEventScript::Instance().Call(pData->Param);
        }
        return;
    }
    default:
        return;
    }
}
