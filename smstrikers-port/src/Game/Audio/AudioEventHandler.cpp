#include "Game/Audio/AudioEventHandler.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/BasicStadium.h"
#include "Game/Render/NPCManager.h"
#include "Game/Render/Bowser.h"
#include "Game/CharacterAudio.h"
#include "Game/Goalie.h"
#include "Game/Ball.h"
#include "Game/Player.h"
#include "Game/Sys/audio.h"
#include "Game/AI/Powerups.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsBanana.h"
#include "Game/AI/FielderActions.h"
#include "Game/AI/AiUtil.h"
#include "Game/Team.h"
#include "Game/Game.h"
#include "Game/GameTweaks.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/FE/FEAudio.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Audio/PriorityStream.h"
#include "Game/Audio/StreamTrack.h"
#include "Game/Render/NetMesh.h"
#include "NL/nlMath.h"
#include "NL/nlConfig.h"
#include "NL/nlString.h"
#include "NL/nlTask.h"

// PORT: the definition lives here.
#include "NL/nlWare.h"

namespace PlatAudio
{
void InitEmitter(unsigned long);
bool RemoveEmitter(unsigned long);
} // namespace PlatAudio

namespace Audio
{
bool IsInited();
bool IsWorldSFXLoaded();
void Silence();
PriorityStream* GetPriorityStream();

extern bool gbGameIsPaused;
extern bool gbStartingGame;
extern bool g_bHomeTeamHasJustScored;
cPlayer* g_pLastScorer;
} // namespace Audio

unsigned long g_MusicTrackPrePauseStreamId;

class cGame;
extern cGame* g_pGame;

static void OnEndPause();
static void OnGamePause();

/**
 * Offset/Address/Size: 0x0 | 0x801423B4 | size: 0x1A18
 */
void Audio::AudioEventHandler(Event* pEvent, void* data)
{
    Bowser* pBowser;
    bool bOnlySlot0;
    bool bOnlySlot1;

    if (!Audio::IsInited() || g_pGame == NULL)
    {
        return;
    }

    switch (pEvent->m_uEventID)
    {
    case 0:
        OnGamePause();
        break;

    case 5:
    {
        GoalScoredData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x18A)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (GoalScoredData*)&pEvent->m_data;
        }

        Audio::g_pLastScorer = pData->pLastTouch[pData->uTeamIndex];
        break;
    }

    case 1:
        OnEndPause();
        break;

    case 9:
        if (Audio::gbGameIsPaused)
        {
            Audio::gbGameIsPaused = false;
        }
        break;

    case 6:
        if (Audio::gbGameIsPaused)
        {
            Audio::gbGameIsPaused = false;
        }
        break;

    case 10:
        if (Audio::IsWorldSFXLoaded())
        {
            Audio::gWorldSFX.Play((Audio::eWorldSFX)0x4F, 100.0f, -1.0f, true, 100.0f);

            if (Audio::g_bHomeTeamHasJustScored)
            {
                Audio::g_bHomeTeamHasJustScored = false;
            }

            if (Audio::gbStartingGame)
            {
                int i;
                for (i = 0; i < 64; i++)
                {
                    PlatAudio::RemoveEmitter((unsigned long)i);
                    PlatAudio::InitEmitter((unsigned long)i);
                }
                Audio::gbStartingGame = false;
            }
        }
        break;

    case 3:
    {
        int i;
        for (i = 0; i < 64; i++)
        {
            PlatAudio::RemoveEmitter((unsigned long)i);
            PlatAudio::InitEmitter((unsigned long)i);
        }
        break;
    }

    case 4:
        if (!StatsTracker::Instance()->mIsOvertime)
        {
            Audio::gWorldSFX.Play((Audio::eWorldSFX)0x4F, 100.0f, -1.0f, true, 100.0f);
            Audio::gWorldSFX.Play((Audio::eWorldSFX)0x4F, 100.0f, 0.5f, true, 100.0f);
            Audio::gWorldSFX.Play((Audio::eWorldSFX)0x50, 100.0f, 1.0f, true, 100.0f);
        }
        break;

    case 50:
    {
        if (!Audio::IsWorldSFXLoaded())
            break;
        if (g_pBall == NULL)
            break;

        BallNetmeshEventData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x118)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (BallNetmeshEventData*)&pEvent->m_data;
        }

        nlVector3 ballNetVel = pData->v3CollisionVelocity;
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        float maxDist = tweaks->fBallHitNetMaxAudibleVelocity;
        float satDist = tweaks->fBallHitNetMinAudibleVelocity;
        float minRatio = tweaks->fBallHitNetMinVolume;
        float throttleInterval = tweaks->fBallHitNetMinTimeBeforeNextAudio;
        float rawLen = nlSqrt(
            ballNetVel.x * ballNetVel.x + ballNetVel.y * ballNetVel.y + ballNetVel.z * ballNetVel.z, true);
        float len = rawLen;
        if (rawLen > maxDist)
            len = maxDist;
        if (len < satDist)
            break;
        float vol = len / maxDist;
        if (vol < minRatio)
            vol = minRatio;
        maxDist = (maxDist + satDist) / 2.0f;
        float fSpreadsheetVol;
        if (len < maxDist)
        {
            fSpreadsheetVol = Audio::gStadGenSFX.GetSFXInfo(0xC5).fVolume;
        }
        else
        {
            fSpreadsheetVol = Audio::gStadGenSFX.GetSFXInfo(0xC4).fVolume;
        }
        vol *= fSpreadsheetVol;

        static float fTimer;
        static signed char init;
        if (!init)
        {
            fTimer = 0.0f;
            init = true;
        }
        float currTime = Audio::GetAudioTimer();
        if (currTime < fTimer)
            fTimer = 0.0f;
        if (fTimer != 0.0f && currTime - fTimer < throttleInterval)
            break;
        fTimer = Audio::GetAudioTimer();

        if (nlTaskManager::m_pInstance->m_CurrState & 0x20010)
            break;
        if (g_pBall->GetOwnerGoalie() != NULL)
            break;

        Audio::SoundAttributes sndAtr;
        sndAtr.Init();
        if (len < maxDist)
        {
            sndAtr.SetSoundType(0xC5, true);
        }
        else
        {
            sndAtr.SetSoundType(0xC4, true);
        }
        sndAtr.UseStationaryPosVector(g_pBall->m_v3Position);
        sndAtr.mf_Volume = vol;
        Audio::gStadGenSFX.Play(sndAtr);
        break;
    }

    case 32:
    {
        if (!Audio::IsWorldSFXLoaded())
            break;

        CollisionBallWallData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x78)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (CollisionBallWallData*)&pEvent->m_data;
        }

        float speed = pData->fCollisionVecLen;
        GameTweaks* tweaks = g_pGame->m_pGameTweaks;
        float maxSpeed = tweaks->fBallHitWallMaxAudibleVelocity;
        float minSpeed = tweaks->fBallHitWallMinAudibleVelocity;
        float throttleDelay = tweaks->fBallHitWallMinTimeBeforeNextAudio;
        if (speed > maxSpeed)
            speed = maxSpeed;
        if (speed < minSpeed)
            break;
        float vol = fabsf(speed) / maxSpeed;
        vol *= Audio::gStadGenSFX.GetSFXInfo(0xC7).fVolume;

        static float fTimer;
        static signed char init;
        if (!init)
        {
            fTimer = 0.0f;
            init = true;
        }
        float currTime = Audio::GetAudioTimer();
        if (currTime < fTimer)
            fTimer = 0.0f;
        if (fTimer != 0.0f && currTime - fTimer < throttleDelay)
            break;
        fTimer = Audio::GetAudioTimer();

        if (nlTaskManager::m_pInstance->m_CurrState & 0x20010)
            break;

        Audio::SoundAttributes sndAtr;
        sndAtr.Init();
        sndAtr.SetSoundType(0xC7, true);
        sndAtr.UseStationaryPosVector(g_pBall->m_v3Position);
        sndAtr.mf_Volume = vol;
        Audio::gStadGenSFX.Play(sndAtr);
        break;
    }

    case 36:
    {
        if (nlTaskManager::m_pInstance->m_CurrState & 0x20110)
            break;

        CollisionBallGroundData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x8F)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (CollisionBallGroundData*)&pEvent->m_data;
        }

        if (!(pData->fVecZComponent < -2.0f))
            break;
        float fVol = pData->fVecZComponent / -8.0f;
        if (fVol > 1.0f)
            fVol = 1.0f;

        Audio::SoundAttributes sndAtr;
        sndAtr.Init();
        sndAtr.SetSoundType(0xC8, true);
        sndAtr.UseStationaryPosVector(pData->position);
        sndAtr.mf_Volume = fVol;
        Audio::gStadGenSFX.Play(sndAtr);
        break;
    }

    case 34:
    {
        CollisionPowerupGroundData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x85)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (CollisionPowerupGroundData*)&pEvent->m_data;
        }

        if (pData->fVecZComponent < -1.0f)
        {
            PowerupBase::PlayPowerupSound(
                pData->eType,
                PowerupBase::PWRUP_SOUND_BOUNCE_GROUND,
                pData->position,
                100.0f);
        }
        break;
    }

    case 35:
    {
        CollisionPowerupGroundData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x85)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (CollisionPowerupGroundData*)&pEvent->m_data;
        }

        PowerupBase::PlayPowerupSound(
            pData->eType,
            PowerupBase::PWRUP_SOUND_BOUNCE_GROUND,
            pData->position,
            100.0f);
        break;
    }

    case 31:
    {
        if (Audio::IsWorldSFXLoaded())
        {
            static float fTimer;
            static signed char init;
            float currTime;

            if (!init)
            {
                fTimer = 0.0f;
                init = true;
            }
            currTime = Audio::GetAudioTimer();
            if (currTime < fTimer)
                fTimer = 0.0f;
            if (fTimer != 0.0f && currTime - fTimer < 0.5f)
                break;
            fTimer = Audio::GetAudioTimer();
        }
        break;
    }

    case 15:
        if (Audio::IsWorldSFXLoaded())
        {
            GoalieSaveData* pData;
            if ((int)port_event_data_id(&pEvent->m_data) == -1)
            {
                nlPrintf("Error: Trying to get event data on event with none!\n");
                break;
            }
            else if ((int)port_event_data_id(&pEvent->m_data) != 0x13C)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                break;
            }
            else
            {
                pData = (GoalieSaveData*)&pEvent->m_data;
            }
        }
        break;

    case 17:
        if (Audio::IsWorldSFXLoaded())
        {
            GoalieSaveData* pData;
            if ((int)port_event_data_id(&pEvent->m_data) == -1)
            {
                nlPrintf("Error: Trying to get event data on event with none!\n");
                pData = 0;
            }
            else if ((int)port_event_data_id(&pEvent->m_data) != 0x13C)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pData = 0;
            }
            else
            {
                pData = (GoalieSaveData*)&pEvent->m_data;
            }

            cPlayer* pGoalie = pData->pGoalie;
            Audio::SoundAttributes sndAtr;
            sndAtr.Init();
            sndAtr.SetSoundType(0xB7, true);
            sndAtr.UseStationaryPosVector(pGoalie->m_v3Position);
            Audio::gStadGenSFX.Play(sndAtr);
        }
        break;

    case 16:
        if (Audio::IsWorldSFXLoaded())
        {
            GoalieSaveData* pData;
            if ((int)port_event_data_id(&pEvent->m_data) == -1)
            {
                nlPrintf("Error: Trying to get event data on event with none!\n");
                pData = 0;
            }
            else if ((int)port_event_data_id(&pEvent->m_data) != 0x13C)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pData = 0;
            }
            else
            {
                pData = (GoalieSaveData*)&pEvent->m_data;
            }

            cPlayer* pGoalie = pData->pGoalie;
            Audio::SoundAttributes sndAtr;
            sndAtr.Init();
            sndAtr.SetSoundType(0xB3, true);
            sndAtr.UseStationaryPosVector(pGoalie->m_v3Position);
            Audio::gStadGenSFX.Play(sndAtr);
        }
        break;

    case 18:
        if (Audio::IsWorldSFXLoaded())
        {
            GoalieSaveData* pData;
            if ((int)port_event_data_id(&pEvent->m_data) == -1)
            {
                nlPrintf("Error: Trying to get event data on event with none!\n");
                pData = 0;
            }
            else if ((int)port_event_data_id(&pEvent->m_data) != 0x13C)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pData = 0;
            }
            else
            {
                pData = (GoalieSaveData*)&pEvent->m_data;
            }

            cPlayer* pGoalie = pData->pGoalie;
            Audio::SoundAttributes sndAtr;
            sndAtr.Init();
            sndAtr.SetSoundType(0xB6, true);
            sndAtr.UseStationaryPosVector(pGoalie->m_v3Position);
            Audio::gStadGenSFX.Play(sndAtr);
        }
        break;

    case 19:
        if (Audio::IsWorldSFXLoaded())
        {
            GoalieSaveData* pData;
            if ((int)port_event_data_id(&pEvent->m_data) == -1)
            {
                nlPrintf("Error: Trying to get event data on event with none!\n");
                pData = 0;
            }
            else if ((int)port_event_data_id(&pEvent->m_data) != 0x13C)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pData = 0;
            }
            else
            {
                pData = (GoalieSaveData*)&pEvent->m_data;
            }

            pData->pGoalie->PlayRandomCharDialogue(6, (PosUpdateMethod)2, 100.0f, -1.0f);
        }
        break;

    case 88:
        if (Audio::gStadGenSFX.IsKeepingTrackOf(0xCE, NULL))
        {
            Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xCE, cGameSFX::SFX_STOP_OLDEST);
        }
        break;

    case 89:
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->PlaySFX(
            (Audio::eCharSFX)0x56, (PosUpdateMethod)2, -1.0f, true);
        break;

    case 90:
    {
        pBowser = BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser;
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x56, cGameSFX::SFX_STOP_FIRST);
        pBowser->PlaySFX((Audio::eCharSFX)0x55, (PosUpdateMethod)2, -1.0f, true);
        break;
    }

    case 91:
    {
        pBowser = BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser;
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x57, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x58, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x59, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->PlayRandomCharDialogue(
            (CharDialogueType)6, (PosUpdateMethod)2, 100.0f, -1.0f, true);
        break;
    }

    case 92:
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->PlaySFX(
            (Audio::eCharSFX)0xD, (PosUpdateMethod)2, -1.0f, true);
        Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xCE, cGameSFX::SFX_STOP_FIRST);
        break;

    case 93:
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->PlaySFX(
            (Audio::eCharSFX)0xC, (PosUpdateMethod)2, -1.0f, true);
        break;

    case 94:
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->PlaySFX(
            (Audio::eCharSFX)0xB, (PosUpdateMethod)2, -1.0f, true);
        Audio::gStadGenSFX.Play(
            (Audio::eWorldSFX)0xCE, 100.0f, -1.0f, true, 100.0f);
        break;

    case 95:
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->PlayRandomWalkFootstep(100.0f, true);
        Audio::gStadGenSFX.Play(
            (Audio::eWorldSFX)0xCE, 100.0f, -1.0f, true, 100.0f);
        break;

    case 96:
    {
        pBowser = BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser;
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x57, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x58, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x59, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x5A, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x5B, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x55, cGameSFX::SFX_STOP_FIRST);

        Audio::eCharSFX bowserHowlSFX[3] = {
            (Audio::eCharSFX)0x57, (Audio::eCharSFX)0x58, (Audio::eCharSFX)0x59
        };
        pBowser->PlaySFX(
            bowserHowlSFX[nlRandom(3, &nlDefaultSeed)],
            (PosUpdateMethod)1,
            -1.0f,
            true);
        break;
    }

    case 97:
    {
        pBowser = BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser;
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x57, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x58, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x59, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x5A, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x5B, cGameSFX::SFX_STOP_FIRST);

        Audio::eCharSFX bowserChargeSFX[2] = { (Audio::eCharSFX)0x5A, (Audio::eCharSFX)0x5B };
        pBowser->PlaySFX(
            bowserChargeSFX[nlRandom(2, &nlDefaultSeed)],
            (PosUpdateMethod)1,
            -1.0f,
            true);
        break;
    }

    case 98:
    {
        pBowser = BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser;
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x57, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x58, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x59, cGameSFX::SFX_STOP_FIRST);
        if (!pBowser->m_pCharacterSFX->IsPlayingRandomCharDialogue((CharDialogueType)2))
        {
            pBowser->m_pCharacterSFX->PlayRandomCharDialogue(
                (CharDialogueType)2, (PosUpdateMethod)2, 100.0f, -1.0f, true);
        }
        pBowser->PlaySFX((Audio::eCharSFX)0x18, (PosUpdateMethod)2, -1.0f, true);
        break;
    }

    case 99:
    {
        pBowser = BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser;
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x57, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x58, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x59, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x5A, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x5B, cGameSFX::SFX_STOP_FIRST);
        pBowser->m_pCharacterSFX->PlayRandomCharDialogue(
            (CharDialogueType)0, (PosUpdateMethod)2, 100.0f, -1.0f, true);
        break;
    }

    case 100:
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->PlaySFX(
            (Audio::eCharSFX)0x5C, (PosUpdateMethod)1, -1.0f, true);
        break;

    case 101:
        BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->Stop((Audio::eCharSFX)0x5C, cGameSFX::SFX_STOP_FIRST);
        break;

    case 102:
    {
        CollisionBobombData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0xED)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (CollisionBobombData*)&pEvent->m_data;
        }

        Audio::SoundAttributes sndAtr;
        sndAtr.Init();
        sndAtr.SetSoundType(0xCF, true);
        sndAtr.UseStationaryPosVector(pData->v3ExplosionLocation);
        Audio::gStadGenSFX.Play(sndAtr);
        break;
    }

    case 103:
        Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xCF, cGameSFX::SFX_STOP_OLDEST);
        break;

    case 104:
        Audio::Silence();
        break;

    case 105:
    {
        if (nlTaskManager::m_pInstance->m_CurrState & 0x20110)
            break;

        PowerupAcquireEventData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x1C3)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (PowerupAcquireEventData*)&pEvent->m_data;
        }

        bOnlySlot0 = false;
        if ((int)g_pTeams[pData->mHomeAway]->GetPowerUpByIndex(0).eType != -1)
        {
            if ((int)g_pTeams[pData->mHomeAway]->GetPowerUpByIndex(1).eType == -1)
                bOnlySlot0 = true;
        }

        bOnlySlot1 = false;
        if ((int)g_pTeams[pData->mHomeAway]->GetPowerUpByIndex(0).eType == -1)
        {
            if ((int)g_pTeams[pData->mHomeAway]->GetPowerUpByIndex(1).eType != -1)
                bOnlySlot1 = true;
        }

        if (bOnlySlot0 || bOnlySlot1)
        {
            if (pData->mHomeAway == 0)
            {
                PowerupBase::PlayPowerupSound(
                    (ePowerUpType)0, (PowerupBase::PowerupSound)0, (PhysicsObject*)NULL, 100.0f);
            }
            else
            {
                Audio::gPowerupSFX.Play(
                    (Audio::eWorldSFX)0x62, 100.0f, -1.0f, true, 100.0f);
            }
        }
        else
        {
            if (pData->mHomeAway == 0)
            {
                Audio::gPowerupSFX.Play(
                    (Audio::eWorldSFX)0x61, 100.0f, -1.0f, true, 100.0f);
            }
            else
            {
                Audio::gPowerupSFX.Play(
                    (Audio::eWorldSFX)0x63, 100.0f, -1.0f, true, 100.0f);
            }
        }
        break;
    }

    case 26:
    {
        PlayerAttackData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x19A)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (PlayerAttackData*)&pEvent->m_data;
        }

        ((cPlayer*)pData->pTarget)->PlayAttackReactionSounds(g_pGame->m_pGameTweaks->fSlideAttackHitReactionVolume);
        ((cPlayer*)pData->pAttacker)->PlayRandomCharDialogue(2, (PosUpdateMethod)2, g_pGame->m_pGameTweaks->fSlideAttackHitReactionVolume, -1.0f);
        break;
    }

    case 23:
    {
        PlayerAttackData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x19A)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (PlayerAttackData*)&pEvent->m_data;
        }

        float attackReactionVol = Interpolate(0.5f, 1.0f, pData->fAttackIntensity);
        ((cPlayer*)pData->pTarget)->PlayAttackReactionSounds(attackReactionVol);
        ((cFielder*)pData->pAttacker)->PlayRandomCharDialogue(2, (PosUpdateMethod)2, attackReactionVol, -1.0f);
        break;
    }

    case 27:
    {
        Audio::cCharacterSFX* pCharSFX;
        CollisionPlayerWallData* pData;
        if ((int)port_event_data_id(&pEvent->m_data) == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pData = 0;
        }
        else if ((int)port_event_data_id(&pEvent->m_data) != 0x6E)
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            pData = 0;
        }
        else
        {
            pData = (CollisionPlayerWallData*)&pEvent->m_data;
        }

        pCharSFX = pData->pPlayer->m_pCharacterSFX;
        if (pCharSFX->IsKeepingTrackOf(0x46, NULL))
        {
            pCharSFX->Stop((Audio::eCharSFX)0x46, cGameSFX::SFX_STOP_FIRST);
        }
        pData->pPlayer->Play3DSFX((Audio::eCharSFX)0x46, (PosUpdateMethod)2, 100.0f);
        pData->pPlayer->PlayRandomCharDialogue(4, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;
    }

    case 66:
    {
        Audio::SoundAttributes sndAtr;
        sndAtr.Init();
        sndAtr.SetSoundType(0xBC, true);
        sndAtr.UsePhysObj(g_pBall->m_pPhysicsBall);
        Audio::gStadGenSFX.Play(sndAtr);

        sndAtr.Init();
        sndAtr.SetSoundType(0xBD, true);
        sndAtr.UsePhysObj(g_pBall->m_pPhysicsBall);
        Audio::gStadGenSFX.Play(sndAtr);
        break;
    }

    case 2:
    case 7:
    case 8:
    case 11:
    case 12:
    case 13:
    case 14:
    case 20:
    case 21:
    case 22:
    case 24:
    case 25:
    case 28:
    case 29:
    case 30:
    case 33:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
    case 67:
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
    case 79:
    case 80:
    case 81:
    case 82:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
        break;

    default:
        break;
    }
}

static void OnGamePause()
{
    Audio::gbGameIsPaused = true;
    Audio::GetPriorityStream()->FakePause(0);

    AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
    AudioStreamTrack::StreamTrack* pTrack = (AudioStreamTrack::StreamTrack*)
                                                pTrackMgr->GetTrack(nlStringLowerHash("Music"));
    AudioStreamTrack::StreamTrack::QUEUED_STREAM* qs = pTrack->m_QueuedStreams.GetHead();
    g_MusicTrackPrePauseStreamId = qs ? qs->StreamId : 0;

    bool bMatchPause;
    switch (g_MusicTrackPrePauseStreamId)
    {
    case 0:
        bMatchPause = false;
        break;
    case 0x78058345:
    case 0x78B7044D:
    case 0x8FBB8496:
        bMatchPause = true;
        break;
    default:
        bMatchPause = false;
        break;
    }
    if (bMatchPause)
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        ((AudioStreamTrack::StreamTrack*)pTrackMgr->GetTrack(
             nlStringLowerHash("Music")))
            ->Pause(0, false);
    }

    // PORT: null when audio is disabled, nothing to command.
    if (g_pTrackManager != NULL) g_pTrackManager->StopAllTracks(0);
    int crowdVol = g_FEStreamConfig.Get<int>("InterruptFadeOut", 0xFA);
    CrowdMood::SetCrowdVolume(0, crowdVol);
    CrowdMood::Purge(true);
    CrowdMood::EnableCrowdDecay(false);
    Audio::Silence();
    FEAudio::PlayAnimAudioEvent("sfx_screen_forward", false);
    AudioLoader::PlayPauseMenuMusic();
}

static void OnEndPause()
{
    Audio::gbGameIsPaused = false;
    bool bPadMonkey = Config::Global().Get<bool>("enable_pad_monkey", false);
    if (!bPadMonkey)
    {
        Audio::GetPriorityStream()->FakeResume(true);
    }
    AudioLoader::StopPauseMenuMusic();
    bool bMatchResume;
    switch (g_MusicTrackPrePauseStreamId)
    {
    case 0:
        bMatchResume = false;
        break;
    case 0x78058345:
    case 0x78B7044D:
    case 0x8FBB8496:
        bMatchResume = true;
        break;
    default:
        bMatchResume = false;
        break;
    }
    if (bMatchResume)
    {
        AudioStreamTrack::TrackManagerBase* pTrackMgr = g_pTrackManager;
        ((AudioStreamTrack::StreamTrack*)pTrackMgr->GetTrack(
             nlStringLowerHash("Music")))
            ->Resume();
    }

    bool bNoCrowd = Config::Global().Get<bool>("no_crowd", false);
    if (bNoCrowd == true)
        return;

    CrowdMood::RestartLoops();
    int crowdVol = g_FEStreamConfig.Get<int>("InterruptFadeOut", 0xFA);
    CrowdMood::SetCrowdVolume(0x7F, crowdVol);
    CrowdMood::EnableCrowdDecay(true);
    if (g_pGame->m_eGameState != GS_OVERTIME)
        return;
    Audio::gWorldSFX.Play((Audio::eWorldSFX)0xCB, 100.0f, -1.0f, true, 100.0f);
}
