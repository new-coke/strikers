#include "Game/Game.h"
#include "Game/Sys/clock.h"
#include "Game/Sys/eventman.h"
#include "Game/Team.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/ScriptAction.h"
#include "Game/AI/Powerups.h"
#include "Game/AI/AISandbox.h"
#include "Game/AI/Scripts/ScriptDefines.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/GameInfo.h"
#include "Game/MathHelpers.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Camera/GameplayCam.h"
#include "Game/BasicStadium.h"
#include "Game/CharacterTemplate.h"
#include "Game/DB/StatsTracker.h"
#include "Game/Formation.h"
#include "Game/NisPlayer.h"
#include "NL/nlConfig.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlMath.h"
#include "Game/AI/FilteredRandom.h"
#include "Game/Render/SidelineExplodable.h"
#include "Game/Render/ElectricFence.h"
#include "Game/Render/Presentation.h"
#include "Game/ReplayChoreo.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
extern cTeam* g_pTeams[];
extern cBall* g_pBall;
#include <stdlib.h>
#include "port/overlay.h"
#include <string.h>
#include "Game/AnimInventory.h"
#include "Game/AI/ShotMeter.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/FE/feManager.h"
#include "Game/GameSceneManager.h"
#include "Game/OverlayManager.h"
#include "Game/Render/Bowser.h"
#include "Game/Render/NPCManager.h"
#include "Game/Drawable/DrawableCharacter.h"
#include "Game/Sys/audio.h"
#include "Game/Net.h"
#include "Game/Goalie.h"
#include "NL/nlDLRing.h"
#include "NL/nlTask.h"
extern eCameraType g_eCurrentCameraType;   // Camera/CameraMan.cpp
extern PowerupBase* g_pPowerups[25];       // AI/Powerups.cpp
#include "port/benchmark.h"
extern PowerupBase* g_pPowerups[];
extern cCharacter* g_pCurrentlyUpdatingCharacter;
extern cTeam* g_pCurrentlyUpdatingTeam;

cGame* g_pGame;

static inline void ApplyDifficulty(eDifficultyID diff0, eDifficultyID diff1, eDifficultyID diff2)
{
    if (diff0 != DIFF_DEFAULT)
    {
        g_pTeams[0]->SetDifficulty(diff0);
    }

    if (diff1 != DIFF_DEFAULT)
    {
        g_pTeams[1]->SetDifficulty(diff1);
    }

    if (diff2 != DIFF_DEFAULT)
    {
        SkillTweaks skillTweaks;
        SkillTweaks* pSkillTweaks;

        skillTweaks.Init(diff2, false);

        for (int i = 0; i < 2; i++)
        {
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotValue1 = skillTweaks.fShotValue1;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotValue2 = skillTweaks.fShotValue2;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotValue3 = skillTweaks.fShotValue3;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance0 = skillTweaks.fShotChance0;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance1 = skillTweaks.fShotChance1;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance2 = skillTweaks.fShotChance2;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance3 = skillTweaks.fShotChance3;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance4 = skillTweaks.fShotChance4;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fSTSWindupTime = skillTweaks.fSTSWindupTime;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fAttackCarrierDistance = skillTweaks.fAttackCarrierDistance;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fLooseBallChaseDistance = skillTweaks.fLooseBallChaseDistance;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fGoalieCanInterceptPass = skillTweaks.fGoalieCanInterceptPass;
        }
    }
}

static inline void DestroyPowerupsImpl(cGame* pGame)
{
    for (int teamIndex = 0; teamIndex < 2; teamIndex++)
    {
        cTeam* pTeam = g_pTeams[teamIndex];
        if (pTeam != NULL)
        {
            pTeam->mfPowerupMeter = 0.0f;
        }
    }

    for (int i = 0; i < 25; i++)
    {
        PowerupBase* pPowerup = g_pPowerups[i];
        if (pPowerup != NULL)
        {
            pPowerup->Destroy(true);
            g_pPowerups[i] = NULL;
        }
    }

    if (BasicStadium::GetCurrentStadium()->mpNPCManager != NULL)
    {
        if (BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp != NULL)
        {
            if (pGame->mbCaptainShotToScoreOn == false)
            {
                BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->Hide(true);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x2274 | 0x8003E7E8 | size: 0x6E0
 */
void CreateGame()
{
    g_pGame = new (nlMalloc(sizeof(cGame), 8, false)) cGame();
    g_pTeams[0] = new (nlMalloc(sizeof(cTeam), 8, false)) cTeam(0);
    g_pTeams[1] = new (nlMalloc(sizeof(cTeam), 8, false)) cTeam(1);
    cField::Init(g_pTeams[0]->m_pNet, g_pTeams[1]->m_pNet);
    FormationManager::LoadFormationSets();

    if (nlSingleton<AISandbox>::s_pInstance == NULL)
    {
        nlSingleton<AISandbox>::s_pInstance = new (nlMalloc(sizeof(AISandbox), 8, false)) AISandbox();
    }

    if (nlSingleton<ScriptQuestionCache>::s_pInstance == NULL)
    {
        nlSingleton<ScriptQuestionCache>::s_pInstance = new (nlMalloc(sizeof(ScriptQuestionCache), 8, false)) ScriptQuestionCache();
    }

    if (nlSingleton<GameInfoManager>::Instance()->GetGameplayOptions().SkillLevel == 0)
    {
        g_pGame->m_fGameDuration = 10800.0f;
    }

    if (Config::Global().Exists("DifficultyOverride"))
    {
        eDifficultyID diff = DIFF_MEDIUM;
        BasicString<char, Detail::TempStringAllocator> diffStr = Config::Global().Get<BasicString<char, Detail::TempStringAllocator> >("DifficultyOverride", BasicString<char, Detail::TempStringAllocator>("Professional"));

        if (diffStr == "Rookie")
        {
            diff = DIFF_EASY;
        }
        else if (diffStr == "Professional")
        {
            diff = DIFF_MEDIUM;
        }
        else if (diffStr == "Superstar")
        {
            diff = DIFF_HARD;
        }
        else if (diffStr == "Legendary")
        {
            diff = DIFF_VERYHARD;
        }
        ApplyDifficulty(diff, diff, diff);
    }
    else
    {
        eDifficultyID retVal = nlSingleton<GameInfoManager>::Instance()->GetSkillLevelAsDifficultyID();
        eDifficultyID diff0 = nlSingleton<GameInfoManager>::Instance()->mCurrentDifficulty[0];
        eDifficultyID diff1 = nlSingleton<GameInfoManager>::Instance()->mCurrentDifficulty[1];
        ApplyDifficulty(diff0, diff1, retVal);
    }
}

/**
 * Offset/Address/Size: 0x2118 | 0x8003E68C | size: 0x15C
 */
void DestroyGame()
{
    Config& cfg = Config::Global();
    bool bWriteStats = GetConfigBool(cfg, "save_stats", false);

    if (bWriteStats)
    {
        nlSingleton<StatsTracker>::Instance()->WriteStats(g_pGame->m_fGameDuration, -1.0f, NULL);
    }

    nlSingleton<ScriptQuestionCache>::DestroyInstance();

    nlSingleton<AISandbox>::DestroyInstance();

    delete g_pTeams[0];
    delete g_pTeams[1];

    g_pTeams[1] = NULL;
    g_pTeams[0] = NULL;

    delete g_pGame;

    g_pGame = NULL;

    FormationManager::UnloadFormationSets();
}

/**
 * Offset/Address/Size: 0x2024 | 0x8003E598 | size: 0xF4
 */
void DestroyPowerups()
{
    DestroyPowerupsImpl(g_pGame);
    CompactPowerups();
}

/**
 * Offset/Address/Size: 0x1D30 | 0x8003E2A4 | size: 0x2F4
 */
cGame::cGame()
    : m_bBallInNet(false)
    , m_eGameState(GS_NONE)
    , m_pScorer(NULL)
    , m_pAssister(NULL)
    , mbCaptainShotToScoreOn(false)
    , mIsPure(false)
    , mInSuddenDeath(false)
{
    m_pPostResetClock = new (nlMalloc(sizeof(Clock), 8, false)) Clock(0.0f, 2.0f, 1.0f, 2, cGame::PostResetCallback);
    m_pPostResetClock->m_uParam1 = (uintptr_t)this;

    m_pGameTweaks = new (nlMalloc(sizeof(GameTweaks), 8, false)) GameTweaks("GameTweaks.ini");
    m_pFuzzyTweaks = new (nlMalloc(sizeof(FuzzyTweaks), 8, false)) FuzzyTweaks("FuzzyTweaks.ini");

    m_fGameDuration = m_pGameTweaks->fGameDuration;

    m_pGameClock = new (nlMalloc(sizeof(Clock), 8, false)) Clock(0.0f, 60000.0f, 1.0f, 2, NULL);
    m_pPostGameDoneClock = new (nlMalloc(sizeof(Clock), 8, false)) Clock(0.0f, 1.4f, 1.0f, 2, NULL);

    m_pTarget = NULL;
    m_pTeamTouch[1] = NULL;
    m_pTeamTouch[0] = NULL;
    m_pRandomPlayersArray = (cPlayer**)nlMalloc(sizeof(cPlayer*) * 10, 8, false);

    Config& cfg = Config::Global();
    mIsPure = GetConfigBool(cfg, "pure_game", false);
    if (GetConfigBool(Config::Global(), "save_stats", false) != false)
    {
        nlSingleton<StatsTracker>::Instance()->WriteCurrentlyPlaying();
    }
}

/**
 * Offset/Address/Size: 0x1BA8 | 0x8003E11C | size: 0x188
 */
cGame::~cGame()
{
    mThoughtsQueue.Clear();

    delete m_pPostResetClock;
    delete m_pGameClock;
    delete m_pGameTweaks;
    delete m_pFuzzyTweaks;
    delete m_pPostGameDoneClock;
    delete[] m_pRandomPlayersArray;
}

#include "Game/FixedUpdateTask.h"
#include "Game/ParticleUpdateTask.h"
#include "Game/Audio/WorldAudio.h"

namespace Audio
{
extern cWorldSFX gWorldSFX;
}

/**
 * Offset/Address/Size: 0x1B2C | 0x8003E0A0 | size: 0x7C
 */
void cGame::DoPerfectPassSlowDown()
{
    if (g_pBall->mbHyperSTS == 0)
    {
        return;
    }

    GameTweaks* pTweaks;
    pTweaks = g_pGame->m_pGameTweaks;
    FixedUpdateTask::mTimeScale = pTweaks->fPerfectPassSlowMo;
    pTweaks = g_pGame->m_pGameTweaks;
    ParticleUpdateTask::SetTimeScale(pTweaks->fPerfectPassSlowMo);
    g_pEventManager->CreateValidEvent(0x46, 0x14);
    Audio::gWorldSFX.Play(Audio::REPLAYSFX_CAMERA_ZOOM_OUT, 100.0f, -1.0f, true, 100.0f);
    Audio::FadeFilterToFullStrength();
}

/**
 * Offset/Address/Size: 0x1B18 | 0x8003E08C | size: 0x14
 */
float cGame::GetNormalizedGameTime()
{
    return m_pGameClock->m_fTimer / m_fGameDuration;
}

/**
 * Offset/Address/Size: 0x1B0C | 0x8003E080 | size: 0xC
 */
float cGame::GetGameTime()
{
    return m_pGameClock->m_fTimer;
}

void cGame::RandomizePlayerUpdateOrder()
{
    int i;
    for (i = 0; i < 5; i++)
    {
        m_pRandomPlayersArray[i] = g_pTeams[0]->GetPlayer(i);
    }
    for (i = 0; i < 5; i++)
    {
        m_pRandomPlayersArray[5 + i] = g_pTeams[1]->GetPlayer(i);
    }
    static FilteredRandomRange randgen;
    for (i = 0; i < 10; i++)
    {
        int j = randgen.genrand(10);
        if (j != i)
        {
            cPlayer* temp = m_pRandomPlayersArray[i];
            m_pRandomPlayersArray[i] = m_pRandomPlayersArray[j];
            m_pRandomPlayersArray[j] = temp;
        }
    }
}

void cGame::ResetCharacters()
{
    RandomizePlayerUpdateOrder();

    int i;
    for (i = 0; i < 2; i++)
    {
        g_pTeams[i]->ResetCharacters();
    }
}

void cGame::ResetBall()
{
    nlVector3 v3Vel = { 0.0f, 0.0f, 0.0f };
    nlVector3 v3Pos = { 0.0f, 0.0f, 0.18f };

    if (g_pBall->m_pOwner != NULL)
    {
        g_pBall->m_pOwner->ReleaseBall();
    }
    g_pBall->WarpTo(v3Pos);
    g_pBall->SetVelocity(v3Vel, SPINTYPE_NONE, NULL);
    cBall* pBall = g_pBall;
    pBall->m_unk_0xA6 = false;
    pBall->mpDamageTarget = NULL;
    m_bBallInNet = false;
    g_pBall->ClearBallEffects();
    g_pBall->HandleBuzzerBeater(-1.0f);
}

void cGame::ResetGameClock()
{
    m_pGameClock->Reset(0.0f, 60000.0f, 1.0f);
    m_pPostGameDoneClock->Reset(0.0f, 1.4f, 1.0f);
    m_pPostGameDoneClock->Stop();
}

/**
 * Offset/Address/Size: 0x1720 | 0x8003DC94 | size: 0x3EC
 */
void cGame::ResetForKickOff()
{
    cFielder* pBallCarrier;
    g_pEventManager->CreateValidEvent(9, 0x14);
    ResetCharacters();
    ResetBall();
    ResetPowerups(false);
    ResetScorerInfo();
    Bowser::SetTiltParameters(0.0f);
    mThoughtsQueue.Clear();
    BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->ActionHide();
    if (mBowserTimer.m_uPackedTime != 0)
    {
        if (mBowserTimer.GetSeconds() < 6.0f)
        {
            mBowserTimer.SetSeconds(6.0f);
        }
    }
    m_pPostResetClock->Reset(0.0f, 2.0f, 1.0f);
    m_pPostResetClock->Start();
    cCameraManager::Remove(eCameraType_MatrixEffect, true);
    GameplayCamera* gpc = cCameraManager::GetCamera<GameplayCamera>(eCameraType_Gameplay);
    if (gpc != NULL)
    {
        gpc->m_ForceNeutralAndNearZoom = true;
    }
    SidelineExplodableManager::DestroyAllActiveFragments(false);
    StopDisplayingElectricFence();
    if (g_pTeams[m_nLastTeamToScore] != NULL)
    {
        pBallCarrier = g_pTeams[!m_nLastTeamToScore]->GetFielder(0);
        if (pBallCarrier->m_pBall == NULL)
        {
            pBallCarrier->PickupBall(g_pBall);
            pBallCarrier->InitActionRunningWB(false);
        }
    }
}

/**
 * Offset/Address/Size: 0x16DC | 0x8003DC50 | size: 0x44
 */
void cGame::PostResetCallback(uintptr_t userData, unsigned long clockId)
{
    g_pEventManager->CreateValidEvent(0xa, 0x14);
    GameplayCamera* pCamera = cCameraManager::GetCamera<GameplayCamera>(eCameraType_Gameplay);
    if (pCamera != nullptr)
    {
        pCamera->m_ForceNeutralAndNearZoom = false;
    }
}

/**
 * Offset/Address/Size: 0x113C | 0x8003D6B0 | size: 0x5A0
 */
void cGame::BeginGame(bool bRematch, bool bStraightToKickoff)
{
    int i;

    OSReport("[match] begin rematch=%d kickoff=%d\n",
             bRematch ? 1 : 0, bStraightToKickoff ? 1 : 0);

    if (m_eGameState != GS_PRE_GAME)
    {
        InitGameState(GS_PRE_GAME);
    }

    m_nLastTeamToScore = 1;
    m_bBallInNet = false;
    mInSuddenDeath = false;
    ResetCharacters();

    ResetBall();

    ResetPowerups(true);
    BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->ActionReset();

    ResetBowser();
    ResetGameClock();

    mfCheatTilt = 0.0f;
    for (i = 0; i < 2; i++)
    {
        g_pTeams[i]->m_nScore = 0;
    }

    for (i = 0; i < 10; i++)
    {
        g_pCharacters[i]->m_Dirt = 0.0f;
    }

    SidelineExplodableManager::DestroyAllActiveFragments(true);
    Presentation::Instance().Reset();
    ReplayChoreo::Instance().FlushHighlights();

    if (bRematch)
    {
        AudioLoader::ResetForRematch();
    }
    else
    {
        AudioLoader::ResetForNewGame();
    }

    if (bStraightToKickoff)
    {
        if (m_eGameState != GS_KICKOFF)
        {
            InitGameState(GS_KICKOFF);
        }
    }
    else
    {
        Presentation::Instance().Call("GameBegin", "");
    }

    ReplayManager::Instance()->ResetSnapshots();
}

/**
 * Offset/Address/Size: 0xE5C | 0x8003D3D0 | size: 0x2E0
 */
void cGame::CheckForGoal()
{
    struct GoalScoredDataExt
    {
        GoalScoredData data;
        int sideOfInterest;
    };

    int nSide;

    if (g_pBall->GetInNet(nSide))
    {
        if (!m_bBallInNet)
        {
            g_pBall->HandleBuzzerBeater(-1.0f);

            nSide += 1;
            nSide %= 2;
            m_nLastTeamToScore = nSide;

            if (m_eGameState == GS_OVERTIME)
            {
                if (m_eGameState != GS_END_GAME)
                {
                    InitGameState(GS_END_GAME);
                }
            }
            else if (m_eGameState != GS_POST_GOAL)
            {
                InitGameState(GS_POST_GOAL);
            }

            if (m_pScorer != g_pBall->m_pLastTouch && g_pBall->m_pLastTouch->m_eClassType != GOALIE)
            {
                float fDirection = g_pBall->m_pLastTouch->m_pTeam->m_pNet->m_v3NetLocation.x * g_pBall->m_v3Position.x;
                if (fDirection >= 0.0f)
                {
                    g_pBall->m_uGoalType = 5;
                }
                else
                {
                    g_pBall->m_uGoalType = 3;
                }

                cPlayer* pPlayer = g_pBall->m_pLastTouch;
                if (m_pScorer != NULL && pPlayer != NULL && m_pScorer != pPlayer && m_pScorer->IsOnSameTeam(pPlayer))
                {
                    m_pAssister = m_pScorer;
                }
                else
                {
                    m_pAssister = NULL;
                }

                m_pScorer = pPlayer;

                if (pPlayer != NULL && pPlayer->m_eClassType == FIELDER)
                {
                    m_pTeamTouch[pPlayer->m_pTeam->m_nSide] = pPlayer;
                }
            }
            else if (m_pScorer != NULL)
            {
                if (nSide != m_pScorer->m_pTeam->m_nSide)
                {
                    g_pBall->m_uGoalType = 5;
                }
            }

            unsigned long uNumGoalsScored;
            if (g_pBall->m_uGoalType == 6)
            {
                World::sbIsHyperShootToScoreRenderingEnabled = false;
                uNumGoalsScored = 2;
            }
            else
            {
                uNumGoalsScored = 1;
            }

            g_pTeams[nSide]->m_nScore += uNumGoalsScored;

            GoalScoredDataExt* pGoalScored = new (/* PORT: m_data is at 0x18 here. */ (u8*)&g_pEventManager->CreateValidEvent(5, 0x3C)->m_data) GoalScoredDataExt();
            pGoalScored->data.uNumGoalsScored = uNumGoalsScored;
            pGoalScored->data.uTeamIndex = nSide;
            pGoalScored->data.uGoalType = g_pBall->m_uGoalType;
            pGoalScored->data.uIsHyper = g_pBall->m_unk_0xA5;
            pGoalScored->data.v3ShotPosition = g_pBall->m_v3ShotOrigin;
            pGoalScored->data.pScorer = m_pScorer;
            pGoalScored->data.pAssister = m_pAssister;

            if (m_pScorer != NULL && m_pScorer->GetGlobalPad() != NULL)
            {
                pGoalScored->sideOfInterest = m_pScorer->GetGlobalPad()->m_padIndex;
            }
            else
            {
                pGoalScored->sideOfInterest = -1;
            }

            pGoalScored->data.pLastTouch[0] = m_pTeamTouch[0];
            pGoalScored->data.pLastTouch[1] = m_pTeamTouch[1];

            g_pBall->m_uGoalType = 4;
            m_bBallInNet = true;
        }
    }
}

void cGame::EnterPostGame()
{
    int homeScore;
    int awayScore;

    SidelineExplodableManager::DestroyAllActiveFragments(false);
    awayScore = g_pTeams[1]->m_nScore;
    homeScore = g_pTeams[0]->m_nScore;
    NisPlayer* pNisPlayer = NisPlayer::Instance();
    pNisPlayer->mWinnerSide[0] = awayScore > homeScore;

    if (!Presentation::Instance().DuringEndOfGamePresentation())
    {
        NisPlayer::Instance()->Reset();
        Presentation::Instance().Call("GameEndNoSuddenDeath", "");
    }
}

/**
 * Offset/Address/Size: 0xDF0 | 0x8003D364 | size: 0x6C
 */
void cGame::BlowUpPowerups(const nlVector3& v3ExplosionPosition, float fExplosionRadius)
{
    float posY, posZ;
    fExplosionRadius *= fExplosionRadius;
    posZ = v3ExplosionPosition.z;
    posY = v3ExplosionPosition.y;

    for (int i = 0; i < 25; i++)
    {
        PowerupBase* pPowerup = g_pPowerups[i];
        if (pPowerup != nullptr)
        {
            float dx, dy, dz;
            dy = posY - pPowerup->m_v3Position.y;
            dz = posZ - pPowerup->m_v3Position.z;
            dx = v3ExplosionPosition.x - pPowerup->m_v3Position.x;

            if (dx * dx + dy * dy + dz * dz < fExplosionRadius)
            {
                pPowerup->m_bShouldDestroy = true;
            }
        }
    }
}

void cGame::BlowUpPlayers(cFielder* pShooter, float fExplosionRadius)
{
    for (int j = 0; j < 2; j++)
    {
        cTeam* pTeam = g_pTeams[j];
        for (int i = 0; i < 4; i++)
        {
            cFielder* pFielder = pTeam->GetFielder(i);
            if (!pFielder->IsInvincible() && pFielder->CanBeBlownUp() && pFielder != pShooter)
            {
                float fMaxSqDistToShooter = fExplosionRadius;
                fMaxSqDistToShooter *= fMaxSqDistToShooter;
                if (nlGetLengthSquared2D(
                        pFielder->m_v3Position.x - pShooter->m_v3Position.x,
                        pFielder->m_v3Position.y - pShooter->m_v3Position.y)
                    < fMaxSqDistToShooter)
                {
                    if (pFielder->InitActionHitReact(
                            pShooter,
                            (unsigned short)(10430.378f * nlATan2f(pFielder->m_v3Position.y - pShooter->m_v3Position.y, pFielder->m_v3Position.x - pShooter->m_v3Position.x)),
                            false))
                    {
                        pFielder->PlayAttackReactionSounds(100.0f);
                    }
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xCE4 | 0x8003D258 | size: 0x10C
 */
void cGame::ResetPowerups(bool clearPowerUps)
{
    for (int i = 0; i < 2; i++)
    {
        cTeam* pTeam = g_pTeams[i];
        if (pTeam != nullptr)
        {
            if (clearPowerUps)
            {
                pTeam->ClearAllPowerUps();
                pTeam->ClearCurrentPowerUp();
            }
            pTeam->mfPowerupMeter = 0.0f;
        }
    }

    for (int i = 0; i < 25; i++)
    {
        PowerupBase* pPowerup = g_pPowerups[i];
        if (pPowerup != nullptr)
        {
            pPowerup->Destroy(true);
            g_pPowerups[i] = nullptr;
        }
    }

    if (BasicStadium::GetCurrentStadium()->mpNPCManager != nullptr)
    {
        if (BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp != nullptr)
        {
            if (!mbCaptainShotToScoreOn)
            {
                BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->Hide(true);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xB00 | 0x8003D074 | size: 0x1E4
 */
void cGame::ResetBowser()
{
    if (GameInfoManager::Instance()->IsTiltingFieldOn() || GameInfoManager::Instance()->mIsInStrikers101Mode)
    {
        mBowserTimer.m_uPackedTime = 0;
        return;
    }

    if (GetConfigBool(Config::Global(), "bowser_repeat", false))
    {
        g_pGame->m_pGameTweaks->fBowserChance = 1.0f;
        g_pGame->m_pGameTweaks->fBowserStartTime = 4.0f;
        g_pGame->m_pGameTweaks->fBowserEndTime = -1.0f;
    }

    GameTweaks* pTweaks_ = g_pGame->m_pGameTweaks;
    if (nlRandomf(1.0f, &nlDefaultSeed) < pTweaks_->fBowserChance)
    {
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        float fMinTime = pTweaks->fBowserStartTime;
        float fMaxTime = pTweaks->fBowserEndTime;

        if (fMinTime < 1.0f)
        {
            fMinTime = 1.0f;
        }
        else if (fMinTime > m_fGameDuration)
        {
            fMinTime = m_fGameDuration - 10.f;
        }

        float fThreshold = 0.0f;
        float fTimeRange = fThreshold;

        if (fMaxTime > fThreshold)
        {
            fTimeRange = m_fGameDuration - fMaxTime - fMinTime;
        }

        if (fTimeRange > fThreshold)
        {
            mBowserTimer.SetSeconds(fMinTime + nlRandomf(fTimeRange, &nlDefaultSeed));
            return;
        }

        mBowserTimer.SetSeconds(fMinTime);
        return;
    }

    mBowserTimer.m_uPackedTime = 0;
}

/**
 * Offset/Address/Size: 0xA80 | 0x8003CFF4 | size: 0x80
 */
void cGame::ResetBowserTimer(float seconds)
{
    if (seconds > 0.0f && !GameInfoManager::Instance()->IsTiltingFieldOn() && !GameInfoManager::Instance()->mIsInStrikers101Mode)
    {
        mBowserTimer.SetSeconds(seconds);
        return;
    }
    mBowserTimer.m_uPackedTime = 0;
}

/**
 * Offset/Address/Size: 0xA20 | 0x8003CF94 | size: 0x60
 */
void cGame::PreUpdate(float deltaTime)
{
    for (int i = 0; i < 2; i++)
    {
        g_pTeams[i]->PreUpdate(deltaTime);
    }
}

void cGame::UpdatePowerUpObjects(float fDeltaT)
{
    for (int i = 0; i < 25; i++)
    {
        if (g_pPowerups[i] != nullptr)
        {
            g_pPowerups[i]->Update(fDeltaT);
        }
    }
}

/**
 * Offset/Address/Size: 0x5A4 | 0x8003CB18 | size: 0x47C
 */
// PORT: defined in GameInfo.cpp, for PDBG_DUMP_USER and PDBG_DUMP_CUP.
extern "C" void PortLogUserInfo(const char* tag);
extern "C" void PortLogCupState(const char* tag);

static void port_DebugPushSession()
{
    PortDebugSession s;
    memset(&s, 0, sizeof s);
    if (nlTaskManager::m_pInstance != NULL)
        s.taskState = (int)nlTaskManager::m_pInstance->m_CurrState;
    GameSceneManager* pFE = nlSingleton<GameSceneManager>::Instance();
    if (pFE != NULL)
    {
        s.feDepth = (int)pFE->mCurrentStackDepth;
        for (int i = 0; i < s.feDepth && i < 8; i++)
            s.feStack[i] = (int)pFE->m_sceneStack[i];
    }
    OverlayManager* pOverlay = nlSingleton<OverlayManager>::Instance();
    if (pOverlay != NULL)
    {
        s.overlayDepth = (int)pOverlay->mCurrentStackDepth;
        for (int i = 0; i < s.overlayDepth && i < 8; i++)
            s.overlayStack[i] = (int)pOverlay->m_sceneStack[i];
    }
    s.inPauseMenu = FrontEnd::m_bInPauseMenuState ? 1 : 0;
    PortDebugSetSession(&s);
}

static cFielder* port_DebugFielder(int index)
{
    if (index < 0 || index >= 10 || g_pCharacters[index] == NULL)
        return NULL;
    cPlayer* pChar = (cPlayer*)g_pCharacters[index];
    return (pChar->m_eClassType == FIELDER) ? (cFielder*)pChar : NULL;
}

extern "C" void PortDebugFrame(void)
{
    port_DebugPushSession();

    PortDebugCommand c;
    while (PortDebugPopCommand(&c))
    {
        GameInfoManager* pInfo = nlSingleton<GameInfoManager>::Instance();
        const bool match = (g_pGame != NULL && g_pTeams[0] != NULL && g_pTeams[1] != NULL
                            && g_pBall != NULL && g_pGame->m_pGameClock != NULL);
        const bool team = (c.a == 0 || c.a == 1);

        switch (c.op)
        {
        case PDBG_SET_SCORE:
            if (match && team)
                g_pTeams[c.a]->m_nScore = c.b;
            break;
        case PDBG_ADD_CLOCK:
            if (match)
            {
                // The clock counts up; time remaining is duration minus it.
                float t = g_pGame->m_pGameClock->m_fTimer - c.f[0];
                if (t < 0.0f)
                    t = 0.0f;
                g_pGame->m_pGameClock->m_fTimer = t;
            }
            break;
        case PDBG_SET_DURATION:
            if (match)
                g_pGame->m_fGameDuration = c.f[0];
            break;
        case PDBG_END_MATCH:
            if (match)
                g_pGame->m_pGameClock->m_fTimer = g_pGame->m_fGameDuration;
            break;
        case PDBG_SUDDEN_DEATH:
            if (match)
            {
                g_pGame->ChangeGameState(GS_OVERTIME);
                g_pGame->mInSuddenDeath = true;
            }
            break;
        case PDBG_KICKOFF:
            if (match && team)
            {
                // ResetForKickOff hands the ball to the team that did not score last.
                g_pGame->m_nLastTeamToScore = (c.a == 0) ? 1 : 0;
                g_pGame->ChangeGameState(GS_KICKOFF);
            }
            break;
        case PDBG_GIVE_BALL:
            if (match && c.a >= 0 && c.a < 10 && g_pCharacters[c.a] != NULL)
            {
                cPlayer* pPlayer = (cPlayer*)g_pCharacters[c.a];
                if (g_pBall->m_pOwner != NULL && g_pBall->m_pOwner != pPlayer)
                    g_pBall->m_pOwner->ReleaseBall();
                if (g_pBall->m_pOwner != pPlayer)
                {
                    pPlayer->PickupBall(g_pBall);
                    if (pPlayer->m_eClassType == FIELDER)
                        ((cFielder*)pPlayer)->InitActionRunningWB(false);
                }
            }
            break;
        case PDBG_WARP_BALL:
            if (match)
            {
                nlVector3 pos = { c.f[0], c.f[1], c.f[2] };
                nlVector3 vel = { 0.0f, 0.0f, 0.0f };
                if (c.f[3] != 0.0f)
                {
                    // Net-relative: f[0] < 0 is the home net. Sit the ball on the net's own location and roll it inward.
                    const int side = (c.f[0] < 0.0f) ? 0 : 1;
                    cNet* pNet = g_pTeams[side]->m_pNet;
                    if (pNet == NULL)
                        break;
                    pos = pNet->m_v3NetLocation;
                    pos.z = c.f[2];
                    vel.x = (pos.x < 0.0f) ? -3.0f : 3.0f;
                }
                if (g_pBall->m_pOwner != NULL)
                    g_pBall->m_pOwner->ReleaseBall();
                g_pBall->WarpTo(pos);
                g_pBall->SetVelocity(vel, SPINTYPE_NONE, NULL);
            }
            break;
        case PDBG_SKIP_PRESENTATION:
            Presentation::Instance().mSkipPressed = true;
            break;
        case PDBG_SET_DIFFICULTY:
            if (match)
                g_pGame->SetDifficulty((eDifficultyID)c.a, (eDifficultyID)c.b, DIFF_DEFAULT);
            break;
        case PDBG_SET_SIDE:
            if (pInfo != NULL && c.a >= 0 && c.a < 4)
            {
                pInfo->SetPlayingSide((unsigned short)c.a, (short)c.b);
                pInfo->ApplyDifficultySettings();
            }
            break;
        case PDBG_FREEZE_PLAYER:
            if (match)
            {
                cFielder* pFielder = port_DebugFielder(c.a);
                if (pFielder != NULL)
                    pFielder->SetFrozen(c.f[0]);
            }
            break;

        case PDBG_GIVE_POWERUP:
            if (match && team && c.b >= 0 && c.b < NUM_POWER_UPS)
                g_pTeams[c.a]->SetCurrentPowerUp((ePowerUpType)c.b, c.c > 0 ? c.c : 1);
            break;
        case PDBG_THROW_POWERUP:
            if (match)
            {
                cFielder* pFielder = port_DebugFielder(c.a);
                if (pFielder != NULL)
                {
                    if (pFielder->m_ePowerup == POWER_UP_NONE && c.b >= 0 && c.b < NUM_POWER_UPS)
                    {
                        pFielder->m_ePowerup = (ePowerUpType)c.b;
                        pFielder->mnNumPowerups = c.c > 0 ? c.c : 1;
                    }
                    if (pFielder->m_ePowerup != POWER_UP_NONE)
                        pFielder->ThrowPowerup();
                }
            }
            break;
        case PDBG_AWARD_POWERUP:
            if (match && team)
                PowerupBase::AwardPowerup(g_pTeams[c.a]);
            break;
        case PDBG_CLEAR_POWERUPS:
            if (match)
                g_pGame->ResetPowerups(c.a != 0);
            break;
        case PDBG_BOWSER_ATTACK:
        case PDBG_BOWSER_HIDE:
            if (match)
            {
                BasicStadium* pStadium = BasicStadium::GetCurrentStadium();
                if (pStadium != NULL && pStadium->mpNPCManager != NULL
                    && pStadium->mpNPCManager->mpBowser != NULL)
                {
                    if (c.op == PDBG_BOWSER_ATTACK)
                        pStadium->mpNPCManager->mpBowser->ActionInit();
                    else
                        pStadium->mpNPCManager->mpBowser->ActionHide();
                }
            }
            break;
        case PDBG_KILL_EFFECTS:
            EmissionManager::DestroyAll(false);
            break;

        // Switching g_eCurrentCameraType is not offered: the retail path deletes the bottom camera and something still holds it.

        case PDBG_SET_OPTION:
            if (pInfo != NULL)
            {
                // Both copies: the running match reads mCurGameGameplayOptions while mUseCurGameSettings holds.
                GameplaySettings& cur = pInfo->mCurGameGameplayOptions;
                GameplaySettings& user = pInfo->mUserInfo.mGameplayOptions;
                CheatSettings& cheats = pInfo->mUserInfo.mCheatOptions;
                const bool on = (c.b != 0);
                switch (c.a)
                {
                case PDBG_OPT_POWERUPS: cur.PowerUps = on; user.PowerUps = on; break;
                case PDBG_OPT_SHOOT2SCORE: cur.Shoot2Score = on; user.Shoot2Score = on; break;
                case PDBG_OPT_BOWSER: cur.BowserAttackEnabled = on; user.BowserAttackEnabled = on; break;
                case PDBG_OPT_RUMBLE: cur.RumbleEnabled = on; user.RumbleEnabled = on; break;
                case PDBG_OPT_SKILL:
                    cur.SkillLevel = (GameplaySettings::eSkillLevel)c.b;
                    user.SkillLevel = (GameplaySettings::eSkillLevel)c.b;
                    break;
                case PDBG_OPT_INFINITE: cheats.mInfinitePowerups = on; break;
                case PDBG_OPT_STUNNED: cheats.mStunnedGoalies = on; break;
                case PDBG_OPT_TILT: cheats.mCheatTBD1Enabled = on; break;
                case PDBG_OPT_PERFECT: cheats.mCheatTBD2Enabled = on; break;
                case PDBG_OPT_CUSTOM: cheats.mCustomPowerups = (CheatSettings::CustomPowerups)c.b; break;
                case PDBG_OPT_PURE:
                    if (g_pGame != NULL)
                        g_pGame->mIsPure = on;
                    break;
                case PDBG_OPT_TROPHIES:
                    pInfo->mUserInfo.mTrophies[0] = on ? 0xFF : 0;
                    pInfo->mUserInfo.mTrophies[1] = on ? 0xFF : 0;
                    break;
                default:
                    break;
                }
            }
            break;
        case PDBG_SET_CLASS_FLAG:
            switch (c.a)
            {
            case PDBG_CF_STADIUM_OFF: World::sbStadiumRenderingDisabled = (c.b != 0); break;
            case PDBG_CF_SKYBOX_OFF: World::sbSkyboxRenderingDisabled = (c.b != 0); break;
            case PDBG_CF_CHAR_SHADOWS_OFF: DrawableCharacter::sShadowRenderingDisabled = (c.b != 0) ? 1 : 0; break;
            default: break;
            }
            break;
        case PDBG_SET_CONFIG_STRING:
        {
            char* eq = strchr(c.s, '=');
            if (eq != NULL)
            {
                *eq = '\0';
                Config::Global().Set((const char*)c.s, (const char*)(eq + 1));
            }
            break;
        }

        case PDBG_PAUSE_MENU:
            if (match)
            {
                if (c.a != 0)
                    FrontEnd::EnterMenuState(FrontEnd::MET_PAUSE);
                else
                    FrontEnd::ExitMenuState();
            }
            break;
        case PDBG_RETURN_TO_FE:
            if (match)
                FrontEnd::ReturnToFE();
            break;
        case PDBG_START_MATCH:
            // The `skipfe` boot path (GameInfo.cpp, main.cpp), from a running front end.
            if (pInfo != NULL && !match)
            {
                // Only from the title or the main menu.
                GameSceneManager* pFE = nlSingleton<GameSceneManager>::Instance();
                const bool onMenu = (pFE != NULL && pFE->mCurrentStackDepth > 0
                    && (pFE->m_sceneStack[pFE->mCurrentStackDepth - 1] == SCENE_TITLE
                        || pFE->m_sceneStack[pFE->mCurrentStackDepth - 1] == SCENE_MAIN_MENU));
                if (!onMenu || nlTaskManager::m_pInstance == NULL
                    || nlTaskManager::m_pInstance->m_CurrState != 4)
                    break;
                pInfo->SetMode(GameInfoManager::GM_FRIENDLY);
                pInfo->SetTeam(0, (eTeamID)c.a);
                pInfo->SetTeam(1, (eTeamID)c.b);
                pInfo->SetSidekick(0, (eSidekickID)(int)c.f[0]);
                pInfo->SetSidekick(1, (eSidekickID)(int)c.f[1]);
                pInfo->SetStadium((eStadiumID)c.c);
                for (int pad = 0; pad < 4; pad++)
                    pInfo->SetPlayingSide((unsigned short)pad, (short)(pad == 0 ? (int)c.f[2] : -1));
                nlTaskManager::SetNextState(2);
            }
            break;

        case PDBG_SET_VOLUME:
            Audio::MasterVolume::SetVolume((Audio::MasterVolume::VOLUME_GROUP)c.a, c.f[0]);
            break;
        case PDBG_PLAY_SFX:
            if (match && c.s[0] != '\0')
                Audio::PlayWorldSFXbyStr(c.s, 1.0f, 0.0f, false, false, NULL, NULL, NULL);
            break;
        case PDBG_SILENCE:
            Audio::Silence();
            break;

        // The two read-only dump ops.
        case PDBG_DUMP_USER:
            PortLogUserInfo("dump");
            break;
        case PDBG_DUMP_CUP:
            PortLogCupState("dump");
            break;

        default:
            break;
        }
    }
}

void cGame::Update(float deltaTime)
{
    mThoughtsAllowedThisUpdate = 1;

    // PORT: the benchmark discards everything before this point.
    PortBenchMatchActive();

    // PORT: hand the debug menu the state it cannot reach for itself.
    if ((PortOverlayMenuOpen() || PortDebugStateWanted())
        && g_pTeams[0] != NULL && g_pTeams[1] != NULL
        && g_pBall != NULL && m_pGameClock != NULL)
    {
        GameInfoManager* pInfo = nlSingleton<GameInfoManager>::Instance();
        PortDebugMatch dbg;
        memset(&dbg, 0, sizeof dbg);

        dbg.valid = 1;
        dbg.gameState = (int)GetGameState();
        dbg.inSuddenDeath = (int)mInSuddenDeath;
        dbg.clock = m_pGameClock->m_fTimer;
        dbg.duration = m_fGameDuration;
        dbg.scoreHome = g_pTeams[0]->m_nScore;
        dbg.scoreAway = g_pTeams[1]->m_nScore;
        dbg.lastTeamToScore = m_nLastTeamToScore;
        dbg.isPure = mIsPure ? 1 : 0;
        dbg.bowserTimer = mBowserTimer.GetSeconds();

        const nlVector3& bp = g_pBall->GetPosition();
        dbg.ballPos[0] = bp.x;
        dbg.ballPos[1] = bp.y;
        dbg.ballPos[2] = bp.z;
        dbg.ballVel[0] = g_pBall->m_v3Velocity.x;
        dbg.ballVel[1] = g_pBall->m_v3Velocity.y;
        dbg.ballVel[2] = g_pBall->m_v3Velocity.z;
        dbg.ballOwner = -1;
        dbg.ballHasPassTarget = (g_pBall->m_pPassTarget != NULL);
        dbg.ballNoPickup = g_pBall->m_tNoPickupTimer.GetSeconds();

        dbg.characterCount = 10;
        for (int i = 0; i < 10; i++)
        {
            cPlayer* pChar = (cPlayer*)g_pCharacters[i];
            PortDebugCharacter& out = dbg.characters[i];
            if (pChar == NULL)
                continue;

            out.pos[0] = pChar->m_v3Position.x;
            out.pos[1] = pChar->m_v3Position.y;
            out.pos[2] = pChar->m_v3Position.z;
            out.team = (pChar->m_pTeam == g_pTeams[1]) ? 1 : 0;
            out.isGoalie = (pChar->m_eClassType == GOALIE);
            out.characterClass = (int)pChar->m_eCharacterClass;
            out.isCaptain = IsCaptain(pChar->m_eCharacterClass) ? 1 : 0;
            out.isHuman = (pChar->m_pController != NULL) ? 1 : 0;
            out.speed = pChar->m_fActualSpeed;
            out.powerup = -1;
            out.card = -1;
            {
                const char* name = GetCharacterName(pChar->m_eCharacterClass);
                if (name != NULL)
                    strncpy(out.name, name, sizeof out.name - 1);
            }
            if (pChar->m_pAnimInventory != NULL && pChar->m_eAnimID >= 0
                && pChar->m_eAnimID < pChar->m_pAnimInventory->m_nNumProperties
                && pChar->m_pAnimInventory->m_pAnimProperties != NULL)
            {
                const char* anim = pChar->m_pAnimInventory->m_pAnimProperties[pChar->m_eAnimID].enumName;
                if (anim != NULL)
                    strncpy(out.anim, anim, sizeof out.anim - 1);
            }
            if (out.isGoalie)
            {
                Goalie* pGoalie = (Goalie*)pChar;
                out.state = (int)pGoalie->mGoalieActionState;
                out.urgency = (int)pGoalie->mUrgency;
                out.energy = pGoalie->mFatigue.mfEnergyLevel;
            }
            else
            {
                cFielder* pFielder = (cFielder*)pChar;
                out.state = (int)pFielder->m_eActionState;
                out.desire = (int)pFielder->m_eFielderDesireState;
                out.role = (int)pFielder->m_eRole;
                out.powerup = (int)pFielder->m_ePowerup;
                out.powerupCount = pFielder->mnNumPowerups;
                out.frozenSeconds = pFielder->m_tFrozenTimer.GetSeconds();
                out.invincible = pFielder->IsInvincible() ? 1 : 0;
                out.fallen = pFielder->IsFallenDown(0.0f) ? 1 : 0;
                out.card = (int)pFielder->m_ePenaltyCardStatus;
                if (pFielder->m_pShotMeter != NULL)
                {
                    out.shotMeterState = (int)pFielder->m_pShotMeter->m_eShotMeterState;
                    out.shotMeterValue = pFielder->m_pShotMeter->m_fScoreValue;
                }
            }
            if (g_pBall->m_pOwner == pChar)
            {
                out.hasBall = 1;
                dbg.ballOwner = i;
            }
        }

        for (int t = 0; t < 2; t++)
        {
            PortDebugTeam& out = dbg.teams[t];
            cTeam* pTeam = g_pTeams[t];
            out.score = pTeam->m_nScore;
            out.powerupMeter = pTeam->mfPowerupMeter;
            for (int slot = 0; slot < 2; slot++)
            {
                out.powerup[slot] = (int)pTeam->m_ePowerupList[slot].eType;
                out.powerupCount[slot] = pTeam->m_ePowerupList[slot].nnumOfPowerups;
            }
            out.situation = (int)pTeam->mpCurrentSituation;
            out.style = (int)pTeam->meCurrentTeamStyle;
            if (pInfo != NULL)
            {
                out.difficulty = (int)pInfo->GetDifficulty(t);
                out.teamId = (int)pInfo->GetTeam((short)t);
                out.sidekickId = (int)pInfo->GetSidekick((short)t);
            }
        }

        {
            int live = 0;
            for (int i = 0; i < 25; i++)
                if (g_pPowerups[i] != NULL)
                    live++;
            dbg.powerupObjects = live;
        }
        {
            BasicStadium* pStadium = BasicStadium::GetCurrentStadium();
            if (pStadium != NULL && pStadium->mpNPCManager != NULL
                && pStadium->mpNPCManager->mpBowser != NULL)
                dbg.bowserAlive = pStadium->mpNPCManager->mpBowser->mbAlive ? 1 : 0;
        }
        {
            efList* pEffects = EmissionManager::GetContainer();
            dbg.effects = (pEffects != NULL) ? pEffects->m_numNodes : 0;
        }
        {
            Presentation& pres = Presentation::Instance();
            strncpy(dbg.presentation, pres.mCurrentFunction, sizeof dbg.presentation - 1);
            dbg.presentationTime = pres.mTimeInFunction;
            dbg.skipAllowed = pres.mInsideByPass ? 1 : 0;
            NisPlayer* pNis = NisPlayer::Instance();
            dbg.nisActive = (pNis != NULL && pNis->mActive) ? 1 : 0;
        }
        {
            cBaseCamera* pCamera = (cCameraManager::m_cameraStack != NULL)
                                       ? cCameraManager::PeekCamera() : NULL;
            if (pCamera != NULL)
            {
                dbg.cameraType = (int)pCamera->GetType();
                dbg.camFov = pCamera->GetFOV();
                const nlVector3& eye = pCamera->GetCameraPosition();
                const nlVector3& at = pCamera->GetTargetPosition();
                dbg.camPos[0] = eye.x; dbg.camPos[1] = eye.y; dbg.camPos[2] = eye.z;
                dbg.camTarget[0] = at.x; dbg.camTarget[1] = at.y; dbg.camTarget[2] = at.z;
            }
            dbg.cameraTypeWanted = (int)g_eCurrentCameraType;
        }
        if (pInfo != NULL)
        {
            const GameplaySettings& opts = pInfo->GetGameplayOptions();
            dbg.skillLevel = (int)opts.SkillLevel;
            dbg.gameTime = (int)opts.GameTime;
            dbg.optPowerUps = opts.PowerUps ? 1 : 0;
            dbg.optShoot2Score = opts.Shoot2Score ? 1 : 0;
            dbg.optBowser = opts.BowserAttackEnabled ? 1 : 0;
            dbg.optRumble = opts.RumbleEnabled ? 1 : 0;
            const CheatSettings& cheats = pInfo->mUserInfo.mCheatOptions;
            dbg.cheatInfinite = cheats.mInfinitePowerups ? 1 : 0;
            dbg.cheatStunned = cheats.mStunnedGoalies ? 1 : 0;
            dbg.cheatTilt = cheats.mCheatTBD1Enabled ? 1 : 0;
            dbg.cheatPerfect = cheats.mCheatTBD2Enabled ? 1 : 0;
            dbg.cheatCustom = (int)cheats.mCustomPowerups;
            dbg.trophies[0] = pInfo->mUserInfo.mTrophies[0];
            dbg.trophies[1] = pInfo->mUserInfo.mTrophies[1];
            for (int pad = 0; pad < 4; pad++)
                dbg.padSide[pad] = (int)pInfo->GetPlayingSide((unsigned short)pad);
            dbg.gameMode = (int)pInfo->GetCurrentMode();
            dbg.demoMode = pInfo->IsInDemoMode() ? 1 : 0;
        }

        PortDebugSetMatch(&dbg);
    }

    if (getenv("STRIKERS_LOG_MATCH") != NULL)
    {
        static int nMatchLog = 0;
        if ((nMatchLog++ % 60) == 0 && g_pTeams[0] != NULL && g_pTeams[1] != NULL
            && g_pBall != NULL && m_pGameClock != NULL)
        {
            const nlVector3& b = g_pBall->GetPosition();
            OSReport("[match] t=%.1f/%.1f state=%d ot=%d score %d-%d "
                     "ball=(%.2f,%.2f,%.2f)\n",
                     (double)m_pGameClock->m_fTimer, (double)m_fGameDuration,
                     (int)GetGameState(), (int)mInSuddenDeath,
                     g_pTeams[0]->m_nScore, g_pTeams[1]->m_nScore,
                     (double)b.x, (double)b.y, (double)b.z);
        }
    }

    if (m_pGameClock->m_fTimer >= m_fGameDuration)
    {
        if (!g_pBall->IsBuzzerBeaterSet())
        {
            if (g_pTeams[0]->m_nScore == g_pTeams[1]->m_nScore)
            {
                if (GetGameState() == GS_GAMEPLAY)
                {
                    ChangeGameState(GS_OVERTIME);

                    nlSingleton<StatsTracker>::Instance()->mIsOvertime = true;
                    mInSuddenDeath = true;
                }
            }
            else
            {
                if (m_eGameState != GS_END_GAME)
                {
                    ChangeGameState(GS_END_GAME);

                    nlSingleton<StatsTracker>::Instance()->TrackWinner(-1);
                    Audio::FadeFilterFromCurrentToZero();
                    FixedUpdateTask::mTimeScale = 1.0f;
                    ParticleUpdateTask::SetTimeScale(1.0f);
                }
            }
        }
    }

    for (int i = 0; i < 2; i++)
    {
        FuzzyScriptSetCurrentTeam(g_pTeams[i]);
        g_pTeams[i]->Update(deltaTime);
        FuzzyScriptClearGlobals();
    }

    for (int i = 0; i < 10; i++)
    {
        g_pCurrentlyUpdatingCharacter = m_pRandomPlayersArray[i];

        if (m_pRandomPlayersArray[i]->m_eClassType == FIELDER)
        {
            FuzzyScriptSetCurrentFielder((cFielder*)m_pRandomPlayersArray[i]);
        }
        else
        {
            FuzzyScriptSetCurrentTeam(m_pRandomPlayersArray[i]->m_pTeam);
        }

        g_pCurrentlyUpdatingTeam = m_pRandomPlayersArray[i]->m_pTeam;
        m_pRandomPlayersArray[i]->Update(deltaTime);
        FuzzyScriptClearGlobals();
    }

    g_pCurrentlyUpdatingCharacter = nullptr;
    g_pBall->Update(deltaTime);

    if (IsGameplayOrOvertime())
    {
        CheckForGoal();
    }

    UpdatePowerUpObjects(deltaTime);

    if (nlSingleton<GameInfoManager>::Instance()->IsTiltingFieldOn())
    {
        float tilt = nlMinEquals(nlMaxEquals(2.0f * (float)(g_pTeams[0]->m_nScore - g_pTeams[1]->m_nScore), -6.0f), 6.0f);
        float currentTilt = mfCheatTilt;

        mfCheatTilt = ((cCharacter*)g_pTeams[0])->SeekSpeedExponential(currentTilt, tilt, 2.0f, deltaTime);
        Bowser::SetTiltParameters(mfCheatTilt);
    }

    if (mBowserTimer.m_uPackedTime != 0 && m_eGameState != GS_END_GAME)
    {
        bool bSTSActive = false;
        cFielder* pOwnerFielder = g_pBall->GetOwnerFielder();
        if (pOwnerFielder != nullptr)
        {
            if (g_pBall->GetOwnerFielder()->m_eActionState == ACTION_SHOOT_TO_SCORE)
            {
                bSTSActive = true;
            }
        }

        eGoalieActionState goalieState = (eGoalieActionState)((Goalie*)g_pCharacters[8])->mGoalieActionState;
        Goalie* pAwayGoalie = (Goalie*)g_pCharacters[9];

        for (int goalie = 0; !bSTSActive && goalie < 2; goalie++)
        {
            switch (goalieState)
            {
            case GOALIEACTION_STS_SETUP:
            case GOALIEACTION_STS:
            case GOALIEACTION_STS_RECOVER:
            case GOALIEACTION_STS_ATTACK_SETUP:
            case GOALIEACTION_STS_ATTACK:
                bSTSActive = true;
                break;
            }
            goalieState = pAwayGoalie->mGoalieActionState;
        }

        if (!bSTSActive)
        {
            bool bChainChompInactive = BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->IsHidden();
            for (int team = 0; bChainChompInactive && team < 2; team++)
            {
                for (int i = 0; i < 2; i++)
                {
                    if (g_pTeams[team]->GetPowerUpByIndex(i).eType == POWER_UP_CHAIN_CHOMP)
                    {
                        bChainChompInactive = false;
                        break;
                    }
                }
            }
            if (bChainChompInactive)
            {
                if (mBowserTimer.Countdown(deltaTime, 0.0f))
                {
                    BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->ActionInit();
                }
            }
        }
    }

    if (m_pPostGameDoneClock->m_clockState == CLOCK_DONE)
    {
        m_pPostGameDoneClock->Reset(0.0f, 1.4f, 1.0f);
        EnterPostGame();
    }
}

void cGame::ResetScorerInfo()
{
    m_pScorer = NULL;
    m_pAssister = NULL;

    for (int i = 0; i < 2; i++)
    {
        m_pTeamTouch[i] = g_pTeams[i]->GetCaptain();
    }
}

/**
 * Offset/Address/Size: 0x508 | 0x8003CA7C | size: 0x9C
 */
void cGame::SetPotentialScorer(cPlayer* pPlayer)
{
    cPlayer* pOldScorer = m_pScorer;

    if (pOldScorer != nullptr && pPlayer != nullptr && pOldScorer != pPlayer && pOldScorer->IsOnSameTeam(pPlayer))
    {
        m_pAssister = m_pScorer;
    }
    else
    {
        m_pAssister = nullptr;
    }

    m_pScorer = pPlayer;

    if (pPlayer != nullptr && pPlayer->m_eClassType == FIELDER)
    {
        m_pTeamTouch[pPlayer->m_pTeam->m_nSide] = pPlayer;
    }
}

/**
 * Offset/Address/Size: 0x4DC | 0x8003CA50 | size: 0x2C
 */
void cGame::ChangeGameState(eGameState state)
{
    if (state != m_eGameState)
    {
        InitGameState(state);
    }
}

/**
 * Offset/Address/Size: 0x374 | 0x8003C8E8 | size: 0x168
 */
void cGame::InitGameState(eGameState state)
{
    // Check if transitioning from GS_GAMEPLAY to GS_OVERTIME
    if (m_eGameState == GS_GAMEPLAY && state == GS_OVERTIME)
    {
        g_pEventManager->CreateValidEvent(0xC, 0x14);
    }

    // Update the game state
    m_eGameState = state;

    // Handle state-specific logic
    switch (state)
    {
    case GS_KICKOFF:
        m_pGameClock->Stop();
        ResetForKickOff();
        break;

    case GS_PRE_GAME:
        m_pGameClock->Stop();
        break;

    case GS_POST_GOAL:
        m_pGameClock->Stop();
        // Loop through all teams and fielders
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 4; j++)
            {
                cFielder* pFielder = pTeam->GetFielder(j);
                pFielder->CleanUpPowerupEffect();
                pFielder->EndBlur();
            }
        }
        break;

    case GS_END_GAME:
        m_pPostGameDoneClock->Start();
        m_pGameClock->Stop();
        break;

    case GS_GAMEPLAY:
    case GS_OVERTIME:
        m_pGameClock->Start();
        // Loop through all teams and fielders
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (int j = 0; j < 4; j++)
            {
                cFielder* pFielder = pTeam->GetFielder(j);
                // End desire if fielder is in WAIT state
                if (pFielder->m_eFielderDesireState == FIELDERDESIRE_WAIT)
                {
                    pFielder->EndDesire(false);
                }
            }
        }
        break;

    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0x25C | 0x8003C7D0 | size: 0x118
 */
bool cGame::IsThoughtAllowed(unsigned long thought_id)
{
    bool bAllowedToThink = false;

    if (mThoughtsAllowedThisUpdate > 0)
    {
        ListEntry<unsigned long>* head = mThoughtsQueue.m_Head;
        if (head == NULL)
        {
            bAllowedToThink = true;
        }
        else if (thought_id == head->entry)
        {
            ListEntry<unsigned long>* removed = nlListRemoveStart<ListEntry<unsigned long> >(&mThoughtsQueue.m_Head, &mThoughtsQueue.m_Tail);
            unsigned long temp;
            if (&temp != NULL)
            {
                temp = removed->entry;
            }
            mThoughtsQueue.DeleteEntry(removed);
            bAllowedToThink = true;
        }
    }

    if (!bAllowedToThink)
    {
        ListEntry<unsigned long>* node = mThoughtsQueue.m_Head;
        bool bFound = false;
        while (node != NULL)
        {
            if (thought_id == node->entry)
            {
                bFound = true;
                break;
            }
            node = node->next;
        }

        if (!bFound)
        {
            ListEntry<unsigned long>* newEntry = (ListEntry<unsigned long>*)nlMalloc(sizeof(ListEntry<unsigned long>), 8, false);
            if (newEntry != NULL)
            {
                newEntry->next = NULL;
                newEntry->entry = thought_id;
            }
            nlListAddEnd(&mThoughtsQueue.m_Head, &mThoughtsQueue.m_Tail, newEntry);
        }
    }

    if (bAllowedToThink)
    {
        mThoughtsAllowedThisUpdate--;
    }

    return bAllowedToThink;
}

/**
 * Offset/Address/Size: 0x1A0 | 0x8003C714 | size: 0xBC
 */
bool cGame::AbortPendingThought(unsigned long thoughtHash)
{
    // PORT: was a hand-decoded list at `this + 0x50` with its tail at `this + 0x54`, two pointers four bytes apart.
    mThoughtsQueue.RemoveEntry(thoughtHash);
    return true;
}

/**
 * Offset/Address/Size: 0x0 | 0x8003C574 | size: 0x1A0
 */
void cGame::SetDifficulty(eDifficultyID diff0, eDifficultyID diff1, eDifficultyID diff2)
{
    if (diff0 != DIFF_DEFAULT)
    {
        g_pTeams[0]->SetDifficulty(diff0);
    }

    if (diff1 != DIFF_DEFAULT)
    {
        g_pTeams[1]->SetDifficulty(diff1);
    }

    if (diff2 != DIFF_DEFAULT)
    {
        SkillTweaks skillTweaks;
        SkillTweaks* pSkillTweaks;

        skillTweaks.Init(diff2, false);

        for (int i = 0; i < 2; i++)
        {
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotValue1 = skillTweaks.fShotValue1;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotValue2 = skillTweaks.fShotValue2;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotValue3 = skillTweaks.fShotValue3;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance0 = skillTweaks.fShotChance0;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance1 = skillTweaks.fShotChance1;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance2 = skillTweaks.fShotChance2;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance3 = skillTweaks.fShotChance3;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fShotChance4 = skillTweaks.fShotChance4;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fSTSWindupTime = skillTweaks.fSTSWindupTime;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fAttackCarrierDistance = skillTweaks.fAttackCarrierDistance;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fLooseBallChaseDistance = skillTweaks.fLooseBallChaseDistance;
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pTeams[i]->m_nSide);
            pSkillTweaks->fGoalieCanInterceptPass = skillTweaks.fGoalieCanInterceptPass;
        }
    }
}
