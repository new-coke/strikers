#include "Game/Character.h"
#include "Game/CharacterTemplate.h"
#include "Game/CharacterTriggers.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/Physics/PhysicsBanana.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsGoalie.h"
#include "Game/Physics/PhysicsNet.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/Render/ElectricFence.h"
#include "Game/Render/SidelineExplodable.h"
#include "Game/Sys/clock.h"

#include "NL/nlString.h"
#include "NL/nlPrint.h"

#include "NL/gl/glState.h"
#include "NL/gl/glTexture.h"

#include "Game/Team.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/ShotMeter.h"
#include "Game/AI/AiUtil.h"
#include "Game/AnimInventory.h"
#include "Game/SAnim/pnBlender.h"
#include "Game/SAnim/AnimRetargeter.h"
#include "NL/nlSlotPool.h"

#include "Game/Ball.h"
#include "Game/Sys/audio.h"
#include "Game/Sys/debug.h"
#include "Game/GL/GLInventory.h"
#include "Game/Render/Bowser.h"
#include "Game/Render/ChainChomp.h"
#include "types.h"

static f32 CANT_COLLIDE = *(f32*)__float_max;

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };
static const char szMushroomBlurTextureBase[23] = "global/mushroomstreak_";
static const char szMushroomBlurTexture[22] = "global/mushroomstreak";

extern unsigned int nlDefaultSeed;
extern PhysicsWorld* g_PhysicsWorld;
static float sfElectrocutionHeightOffset = 0.6f;
static bool sbElectricFenceDebug = false;

eCharacterModelType cCharacter::m_ModelType = CharModel_Rigid;

nlVector3 g_v3PrevJointPosition = { 0.0f, 0.0f, 0.0f };

static inline float CharacterAnimSmoothStep(float x);
inline float ClampMin(float speedRatio, const float min);
inline float ClampMax(float speedRatio, const float max);
static inline AnimRetarget* GetCharacterAnimRetarget(const cCharacter* character, const cSAnim* pSAnim);
static inline Blinker* MakeBlinker(eCharacterClass cc, unsigned long modelID);
inline eVariantType VariantTypeOf(const nlVector3&);

/**
 * Offset/Address/Size: 0x26A0 | 0x800105EC | size: 0x1A58
 */
void AIEventHandler(Event* pEvent, void*)
{
    struct PhysicsBallFlagsView
    {
        unsigned char pad[0x3B];
        unsigned char bUseMagnusEffect;
    };

    switch (pEvent->m_uEventID)
    {
    case 0x25:
    {
        CollisionPlayerPlayerData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPlayerPlayerData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPlayerPlayerData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        pEventData->player1->CollideWithCharacterCallback(pEventData);
        break;
    }

    case 0x24:
    {
        CollisionBallGroundData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionBallGroundData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionBallGroundData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pBall == 0)
        {
            break;
        }

        if (!pEventData->pBall->mbIsPerfectShot)
        {
            if (!pEventData->bIsShot)
            {
                break;
            }
        }

        pEventData->pBall->CollideWithGroundCallback();

        if (pEventData->bIsShot)
        {
            EmitBallWallHit("perfect_shot_catch");
        }
        else
        {
            EmitBallWallHit("goalie_catch");
        }

        break;
    }

    case 0x20:
    {
        CollisionBallWallData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionBallWallData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionBallWallData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pBall == 0)
        {
            break;
        }

        if (pEventData->bIsShot)
        {
            if (pEventData->bIsPerfect)
            {
                EmitBallWallHit("perfect_shot_catch");
            }
            else
            {
                EmitBallWallHit("goalie_catch");
            }
        }

        pEventData->pBall->CollideWithWallCallback();
        EmitElectricFenceBallEffect(pEventData->position, pEventData->normal, (uintptr_t)pEventData->pBall, false);
        break;
    }

    case 0x2E:
    {
        if (g_pBall == 0)
            break;

        CollisionBallGoalpostData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionBallGoalpostData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionBallGoalpostData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        EffectsGroup* pGroup = fxGetGroup("ball_impact");

        bool bHasPerfect = false;
        if (g_pBall->m_tShotTimer.m_uPackedTime != 0 && g_pBall->m_unk_0xA4)
            bHasPerfect = true;

        if (bHasPerfect)
        {
            pGroup = fxGetGroup("perfect_shot_catch");
        }
        else
        {
            bool bHasCanDamage = false;
            if (g_pBall->m_tShotTimer.m_uPackedTime != 0 && g_pBall->mbCanDamage)
                bHasCanDamage = true;

            if (bHasCanDamage)
            {
                cPlayer* pPrevOwner = g_pBall->m_pPrevOwner;
                if (pPrevOwner->m_eClassType == FIELDER)
                {
                    BasicString<char, Detail::TempStringAllocator> effectName(
                        GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((s16)pPrevOwner->m_pTeam->m_nSide)));
                    effectName.AppendInPlace("_shoot_to_score_catch");
                    fxGetGroup(effectName.c_str());
                }
            }
        }

        EmissionController* pControl = EmissionManager::Create(pGroup, 0);
        pControl->SetPosition(pEventData->v3CollisionPosition);
        g_pBall->CollideWithWallCallback();
        break;
    }

    case 0x21:
    {
        CollisionPowerupWallData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPowerupWallData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPowerupWallData*)&pEvent->m_data;
            }
        }
        const nlVector3& pos = pEventData->position;
        const nlVector3& nrm = pEventData->normal;
        uintptr_t powerupID = (uintptr_t)pEventData->pPowerup;

        if (!EmissionManager::IsPlaying(powerupID, fxGetGroup("electric_fence")))
        {
            switch (pEventData->eSize)
            {
            case 2:
                PowerupBase::PlayPowerupSound(pEventData->eType, (PowerupBase::PowerupSound)4, pos, 100.0f);
                break;
            case 1:
                PowerupBase::PlayPowerupSound(pEventData->eType, (PowerupBase::PowerupSound)4, pos, 100.0f);
                break;
            case 0:
                PowerupBase::PlayPowerupSound(pEventData->eType, (PowerupBase::PowerupSound)4, pos, 100.0f);
                break;
            default:
                break;
            }
        }

        EmitElectricFenceBallEffect(pos, nrm, powerupID, false);
        break;
    }

    case 0x1F:
    {
        CollisionPlayerWallData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPlayerWallData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPlayerWallData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pPlayer == 0)
        {
            break;
        }

        pEventData->pPlayer->CollideWithWallCallback(pEventData);
        const nlVector3& normal = pEventData->wallNormal;

        if (sbElectricFenceDebug)
        {
            if (pEventData->pPlayer->m_eClassType != GOALIE)
            {
                nlVector3 p2 = pEventData->contactPoint;
                p2.z += sfElectrocutionHeightOffset;
                CharacterElectrocutionEffect(pEventData->pPlayer, p2, normal);
            }
        }

        if (pEventData->pPlayer->m_eClassType == FIELDER)
        {
            ((cFielder*)pEventData->pPlayer)->CanBreakOutOfSlideTackle();
        }

        break;
    }

    case 0x27:
    {
        CollisionPlayerBallData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPlayerBallData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPlayerBallData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pPlayer == 0)
        {
            break;
        }

        if (pEventData->pBall == 0)
        {
            break;
        }

        pEventData->pPlayer->CollideWithBallCallback(pEventData->pBall);
        pEventData->pBall->CollideWithCharacterCallback(pEventData->pPlayer, pEventData->velocity);

        PhysicsAIBall* pPhysicsBall = pEventData->pBall->m_pPhysicsBall;
        // PORT: was a padded view at console 0x3B. Named member; no cast.
        if (pPhysicsBall->m_bUseMagnusEffect)
        {
            nlVector3 v3AngVel;
            pPhysicsBall->GetAngularVelocity(&v3AngVel);
            nlVec3Scale(v3AngVel, 0.6f);
            pPhysicsBall->SetAngularVelocity(v3AngVel);
        }

        break;
    }

    case 0x28:
    {
        CollisionPlayerShootToScoreBallData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPlayerShootToScoreBallData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPlayerShootToScoreBallData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pBall == 0)
            break;
        if (pEventData->pFielder == 0)
            break;
        if (pEventData->pBall->m_pPrevOwner == 0)
            break;

        float dy = pEventData->pBall->m_v3Position.y - pEventData->pBall->m_pPrevOwner->m_v3Position.y;
        float dx = pEventData->pBall->m_v3Position.x - pEventData->pBall->m_pPrevOwner->m_v3Position.x;
        float angleRad = nlATan2f(dy, dx);
        s32 nAngle = (s32)(angleRad * 10430.378f);

        bool bHit = pEventData->pFielder->InitActionHitReact(pEventData->pBall->m_pPrevOwner, (unsigned short)nAngle, false);
        if (bHit)
        {
            pEventData->pFielder->PlayAttackReactionSounds(g_pGame->m_pGameTweaks->fShootToScoreBallHitReactionVolume);
        }

        break;
    }

    case 0x2F:
    {
        CollisionChainPlayerData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionChainPlayerData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionChainPlayerData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pFielder == 0)
            break;
        if (pEventData->pChain == 0)
            break;

        if (!pEventData->pFielder->IsInvincible())
        {
            pEventData->pFielder->CollideWithChainCallback(pEventData->pChain);
        }

        if (pEventData->pFielder == pEventData->pChain->mpTarget)
        {
            pEventData->pChain->FindTarget(pEventData->pFielder->m_pTeam);
        }

        break;
    }

    case 0x30:
    {
        CollisionBowserPlayerData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionBowserPlayerData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionBowserPlayerData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pFielder == 0)
            break;
        if (pEventData->pBowser == 0)
            break;

        if (pEventData->pFielder->IsFallenDown(0.0f))
            break;

        pEventData->pFielder->CollideWithBowserCallback(pEventData->pBowser);

        if (pEventData->pFielder == pEventData->pBowser->GetTarget())
        {
            pEventData->pBowser->FindTarget();
        }

        pEventData->pBowser->m_pCharacterSFX->PlayRandomCharDialogue((CharDialogueType)2, (PosUpdateMethod)2, 100.0f, -1.0f, true);

        break;
    }

    case 0x32:
    {
        g_pBall->CollideWithWallCallback();
        break;
    }

    case 0x0A:
    {
        if (g_pBall == 0)
            break;
        cFielder* pBallCarrier = g_pBall->GetOwnerFielder();
        if (pBallCarrier != 0)
        {
            pBallCarrier = g_pBall->GetOwnerFielder();
            if (pBallCarrier->GetGlobalPad() == 0)
            {
                int passTargetID = nlRandom(3, &nlDefaultSeed) + 1;
                if (passTargetID > 3)
                {
                    passTargetID = 3;
                }

                pBallCarrier->InitActionPass(pBallCarrier->m_pTeam->GetPlayer(passTargetID), false, true);
                g_pEventManager->CreateValidEvent(0x0B, 0x14);
                pBallCarrier->mbCanKickoff = false;
            }
            else
            {
                pBallCarrier->SetKickOffWaitTime();
            }
        }
        else
        {
            g_pEventManager->CreateValidEvent(0x0B, 0x14);
        }
        break;
    }

    case 0x0B:
    {
        if (g_pGame == 0)
            break;
        g_pGame->ChangeGameState(GS_GAMEPLAY);
        break;
    }

    case 0x05:
    {
        if (g_pTeams[0] == NULL || g_pTeams[1] == NULL)
        {
            break;
        }

        for (s32 i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            for (s32 j = 0; j < 5; j++)
            {
                cPlayer* pPlayer = pTeam->GetPlayer(j);
                if (pPlayer->GetGlobalPad() != NULL)
                {
                    pPlayer->SetAIPad(NULL);
                }
            }
        }
        break;
    }

    case 0x29:
    {
        CollisionPlayerShellData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPlayerShellData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPlayerShellData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pPlayer == 0)
            break;

        bool bIsWeaponSuccessful = pEventData->pPlayer->CollideWithShellCallback((ePowerupSize)pEventData->eSize, (bool)pEventData->bIsExploder, pEventData->v3CollisionLocation, pEventData->v3CollisionVelocity);

        if (bIsWeaponSuccessful)
        {
            if (pEventData->eSize == 2)
            {
                PowerupBase::PlayPowerupSound((ePowerUpType)0, (PowerupBase::PowerupSound)3, pEventData->pPlayer->m_pPhysicsCharacter, 100.0f);
            }
        }

        if (pEventData->pThrower == 0)
            break;
        if (!bIsWeaponSuccessful)
            break;

        if (pEventData->pThrower->IsOnSameTeam(pEventData->pPlayer))
            break;

        Event* pStatsEvent = g_pEventManager->CreateValidEvent(0x55, 0x20);
        CollisionPowerupStatsData* pStatsData = new (&pStatsEvent->m_data) CollisionPowerupStatsData();
        pStatsData->pThrower = pEventData->pThrower;
        pStatsData->nThrowerPadID = (s32)(s8)pEventData->nThrowerPadID;
        break;
    }

    case 0x2A:
    {
        CollisionPlayerFreezeData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPlayerFreezeData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPlayerFreezeData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pPlayer == 0)
            break;

        bool bIsWeaponSuccessful = pEventData->pPlayer->CollideWithFreezeCallback();

        if (bIsWeaponSuccessful)
        {
            if (pEventData->eSize == 2)
            {
                PowerupBase::PlayPowerupSound((ePowerUpType)3, (PowerupBase::PowerupSound)3, pEventData->pPlayer->m_pPhysicsCharacter, 100.0f);
            }
        }

        if (pEventData->pThrower == 0)
            break;
        if (!bIsWeaponSuccessful)
            break;

        if (pEventData->pThrower->IsOnSameTeam(pEventData->pPlayer))
            break;

        Event* pStatsEvent = g_pEventManager->CreateValidEvent(0x55, 0x20);
        CollisionPowerupStatsData* pStatsData = new (&pStatsEvent->m_data) CollisionPowerupStatsData();
        pStatsData->pThrower = pEventData->pThrower;
        pStatsData->nThrowerPadID = pEventData->nThrowerPadID;
        break;
    }

    case 0x2D:
    {
        CollisionBallShellData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionBallShellData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionBallShellData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        cFielder* pBallOwner = g_pBall->GetOwnerFielder();
        if (pBallOwner == 0)
            break;

        pBallOwner->ReleaseBall();
        pBallOwner->InitActionRunning();
        pBallOwner->ShootBallDueToContact(pEventData->v3CollisionVelocity);
        break;
    }

    case 0x2B:
    {
        CollisionPlayerBananaData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionPlayerBananaData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionPlayerBananaData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pPlayer == 0)
            break;

        bool bIsWeaponSuccessful = pEventData->pPlayer->CollideWithBananaCallback(pEventData->v3CollisionLocation);

        if (pEventData->pThrower == 0)
            break;
        if (!bIsWeaponSuccessful)
            break;

        if (pEventData->pThrower->IsOnSameTeam(pEventData->pPlayer))
            break;

        Event* pStatsEvent = g_pEventManager->CreateValidEvent(0x55, 0x20);
        CollisionPowerupStatsData* pStatsData = new (&pStatsEvent->m_data) CollisionPowerupStatsData();
        pStatsData->pThrower = pEventData->pThrower;
        pStatsData->nThrowerPadID = pEventData->nThrowerPadID;
        break;
    }

    case 0x03:
    {
        for (s32 i = 0; i < 2; i++)
        {
            if (g_pTeams[i] != NULL)
            {
                for (s32 j = 0; j < 4; j++)
                {
                    g_pTeams[i]->GetFielder(j)->m_pCharacterSFX->StopMovementLoop();
                }
            }
        }
        break;
    }

    case 0x3F:
    case 0x46:
    {
        if (g_pGame == 0)
            break;
        g_pGame->m_pGameClock->Stop();
        break;
    }

    case 0x41:
    case 0x47:
    {
        if (g_pGame == 0)
            break;

        bool bShouldStart = false;
        int state = g_pGame->m_eGameState;
        if (state == 4 || state == 5)
        {
            bShouldStart = true;
        }

        if (!bShouldStart)
            break;
        g_pGame->m_pGameClock->Start();
        break;
    }

    case 0x2C:
    {
        if (g_pGame == 0)
            break;

        bool bIsGameplay = false;
        int state = g_pGame->m_eGameState;
        if (state == 4 || state == 5)
        {
            bIsGameplay = true;
        }
        if (!bIsGameplay)
            break;

        CollisionBobombData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CollisionBobombData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CollisionBobombData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        for (s32 i = 0; i < 2; i++)
        {
            if (g_pTeams[i] != 0)
            {
                cTeam* pTeam = g_pTeams[i];

                for (s32 j = 0; j < 4; j++)
                {
                    cFielder* pFielder = pTeam->GetFielder(j);

                    if (pFielder->IsInvincible())
                        continue;
                    if (!pFielder->CanBeBlownUp())
                        continue;
                    if (pEventData->pThrower == pFielder)
                        continue;

                    nlMatrix4& nodeMatrix = pFielder->m_pPoseAccumulator->GetNodeMatrix(pFielder->m_nBip01JointIndex_0xA4);

                    float dist = nlSqrt(nlGetLengthSquared3D(
                                            pEventData->v3ExplosionLocation.x - nodeMatrix.e2[3][0],
                                            pEventData->v3ExplosionLocation.y - nodeMatrix.e2[3][1],
                                            pEventData->v3ExplosionLocation.z - nodeMatrix.e2[3][2]),
                        true);

                    if (!(dist < pEventData->fExplosionRadius))
                        continue;

                    bool bWasFallenDown = pFielder->IsFallenDown(0.0f);
                    bool bIsWeaponSuccessful;

                    if (pEventData->bIsFreezeBomb)
                    {
                        bIsWeaponSuccessful = pFielder->CollideWithFreezeCallback();
                    }
                    else
                    {
                        bIsWeaponSuccessful = true;
                        pFielder->SetBombImpactTime(pEventData->v3ExplosionLocation, pEventData->fExplosionRadius / g_pGame->m_pGameTweaks->fPowerupExplosionRadius);
                    }

                    if (pEventData->pThrower == 0)
                        continue;
                    if (bWasFallenDown)
                        continue;
                    if (!bIsWeaponSuccessful)
                        continue;

                    Event* pStatsEvent = g_pEventManager->CreateValidEvent(0x55, 0x20);
                    CollisionPowerupStatsData* pStatsData = new (&pStatsEvent->m_data) CollisionPowerupStatsData();
                    pStatsData->pThrower = pEventData->pThrower;
                    pStatsData->nThrowerPadID = pEventData->nThrowerPadID;
                }
            }

            g_pGame->BlowUpPowerups(pEventData->v3ExplosionLocation, pEventData->fExplosionRadius);
            SidelineExplodableManager::TriggerExplosions(pEventData->v3ExplosionLocation, pEventData->fExplosionRadius);
        }

        break;
    }

    case 0x0D:
    {
        ReceiveBallData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            ReceiveBallData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (ReceiveBallData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        cPlayer* pReceiver = pEventData->pReceiver;
        if (pReceiver == 0)
            break;

        if (pReceiver->IsOnSameTeam(g_pBall->m_pPrevOwner))
            break;

        cTeam* pTeam = pReceiver->m_pTeam;
        pTeam->mtMarkTimer.m_uPackedTime = 0;
        pTeam->mtRoleTimer.m_uPackedTime = 0;
        cTeam* pOtherTeam = pTeam->GetOtherTeam();
        pOtherTeam->mtMarkTimer.m_uPackedTime = 0;
        pOtherTeam = pTeam->GetOtherTeam();
        pOtherTeam->mtRoleTimer.m_uPackedTime = 0;
        break;
    }

    case 0x0E:
    {
        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            break;
        }

        id = port_event_data_id(&pEvent->m_data);
        if (id == 0x131)
            break;

        nlPrintf("Error: GetData() failed! Data types do not match!\n");
        break;
    }

    case 0x14:
    {
        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            break;
        }

        ShotAtGoalData t;
        id = port_event_data_id(&pEvent->m_data);
        if (id == (s32)t.GetID())
            break;

        nlPrintf("Error: GetData() failed! Data types do not match!\n");
        break;
    }

    case 0x3C:
    case 0x3D:
    {
        PenaltyData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            PenaltyData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (PenaltyData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pFoulee == 0)
            break;

        if (!(pEventData->fPenaltyWorth > 0.0f))
            break;

        if (!pEventData->pFoulee->m_pTeam->IncrementPowerupMeter(pEventData->fPenaltyWorth))
            break;

        if (pEventData->fPenaltyWorth > 0.99f)
            return;
        if (pEventData->fPenaltyWorth > 0.75f)
            return;

        return;
    }

    case 0x3E:
    {
        PowerupData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            PowerupData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (PowerupData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        if (pEventData->pFielder == 0)
            break;

        if (!(pEventData->fAwardWorth > 0.0f))
            break;

        if (!pEventData->pFielder->m_pTeam->IncrementPowerupMeter(pEventData->fAwardWorth))
            break;

        if (pEventData->fAwardWorth > 0.99f)
            return;
        if (pEventData->fAwardWorth > 0.75f)
            return;

        return;
    }

    case 0x07:
    {
        CharacterDirectionData* pEventData;

        s32 id = port_event_data_id(&pEvent->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            pEventData = 0;
        }
        else
        {
            CharacterDirectionData t;
            id = port_event_data_id(&pEvent->m_data);
            if (id != (s32)t.GetID())
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                pEventData = 0;
            }
            else
            {
                pEventData = (CharacterDirectionData*)&pEvent->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (pEventData == 0)
        {
            break;
        }

        for (s32 i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            if (g_pTeams[i] == 0)
                continue;

            for (s32 j = 0; j < 4; j++)
            {
                cFielder* pFielder = pTeam->GetFielder(j);

                FuzzyVariant desiredLocation;

                nlVector3* pDirEntry;
                if (i == 0)
                {
                    pDirEntry = &pEventData->home[j];
                }
                else
                {
                    pDirEntry = &pEventData->away[j];
                }

                desiredLocation = FuzzyVariant(*pDirEntry);

                if (pFielder->IsRunning())
                {
                    pFielder->InitDesire((eFielderDesireState)12, 0.5f, 999999.9f, desiredLocation, fvNotSet);
                }
                else
                {
                    pFielder->QueueDesire((eFielderDesireState)12, 999999.9f, desiredLocation, fvNotSet);
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

static inline Blinker* MakeBlinker(eCharacterClass cc, unsigned long modelID)
{
    char matsName[64];
    char eyesName[80];
    const char* szBaseName;
    unsigned long matsHash;
    GLMaterialList* mats0;
    unsigned long eyesHash;
    GLMaterialList* mats1;

    szBaseName = GetCharacterName(cc);
    nlSNPrintf(matsName, 64, "%s/%s", szBaseName, szBaseName);
    matsHash = nlStringHash(matsName);
    mats0 = glInventory.GetMaterialList(matsHash);

    if (mats0 == NULL)
    {
        tDebugPrintManager::Print(DC_RENDER, "Error: %s cannot even begin to blink\n", szBaseName);
        return NULL;
    }

    nlSNPrintf(eyesName, 80, "%s/%s/eyes", szBaseName, szBaseName);
    eyesHash = nlStringHash(eyesName);

    if (mats0->FindMaterial(eyesHash) == NULL)
    {
        tDebugPrintManager::Print(DC_RENDER, "Error: %s cannot even begin to blink (no 'eyes')\n", szBaseName);
        return NULL;
    }

    nlSNPrintf(matsName, 64, "%s/%s_blend", szBaseName, szBaseName);
    matsHash = nlStringHash(matsName);
    mats1 = glInventory.GetMaterialList(matsHash);

    Blinker* blinker = new (nlMalloc(sizeof(Blinker), 8, false)) Blinker(szBaseName, modelID, mats0, mats1, eyesHash);
    return blinker;
}

/**
 * Offset/Address/Size: 0x2238 | 0x80010184 | size: 0x468
 */
cCharacter::cCharacter(eCharacterClass cc, const int* nModelID, cSHierarchy* pHierarchy, cAnimInventory* pAnimInventory,
    const CharacterPhysicsData* pPhysicsData, float fPhysicsCapsuleHeight, float fPhysicsCapsuleWidth,
    AnimRetargetList* pAnimRetargetList, eClassTypes eNewClassType)
{
    m_eCharacterClass = cc;
    m_pPhysicsData = pPhysicsData;
    m_pPhysicsCharacter = NULL;
    m_aDesiredFacingDirection = 0;
    m_aActualFacingDirection = 0;
    m_aPrevFacingDirection = 0;
    m_aDesiredMovementDirection = 0;
    m_aActualMovementDirection = 0;
    m_fDesiredSpeed = 0.0f;
    m_fActualSpeed = 0.0f;
    m_fLeanAmount = 0.0f;
    m_pAnimInventory = pAnimInventory;
    m_pPoseAccumulator = NULL;
    m_pPoseTree = NULL;
    m_pCurrentAnimController = NULL;
    m_eAnimID = 0;
    m_pAnimRetargetList = pAnimRetargetList;
    m_eClassType = eNewClassType;
    m_pCharacterSFX = NULL;
    m_pPropModel = NULL;
    m_uNormalTextureID = 0;
    m_uSwapTextureID = 0;
    m_Dirt = 0.0f;
    m_MinDirt = 0.0f;
    m_pBlurHandler = NULL;
    m_pBlinker = NULL;

    if (pPhysicsData != NULL)
    {
        if (eNewClassType == GOALIE)
        {
            PhysicsGoalie* goalie = new (nlMalloc(sizeof(PhysicsGoalie), 8, false)) PhysicsGoalie(fPhysicsCapsuleWidth, fPhysicsCapsuleHeight);
            m_pPhysicsCharacter = goalie;
        }
        else
        {
            m_pPhysicsCharacter = new (nlMalloc(sizeof(PhysicsCharacter), 8, false)) PhysicsCharacter(fPhysicsCapsuleWidth, fPhysicsCapsuleHeight);
        }
        m_pPhysicsCharacter->m_pAICharacter = this;
    }

    m_m4WorldMatrix.SetIdentity();
    SetPosition(v3Zero);

    m_v3Velocity = v3Zero;
    m_pPhysicsCharacter->SetCharacterVelocityXY(m_v3Velocity);

    // PORT: a hash; bit 31 sign-extends as s32.
    m_pSkinMesh[0] = glInventory.MakeSkinMesh((u32)nModelID[0]);
    if (nModelID[1] == 0)
    {
        m_pSkinMesh[1] = NULL;
    }
    else
    {
        m_pSkinMesh[1] = glInventory.MakeSkinMesh((u32)nModelID[1]);
    }

    m_pPoseAccumulator = new (nlMalloc(sizeof(cPoseAccumulator), 8, false)) cPoseAccumulator(pHierarchy, true);

    if (pPhysicsData != NULL)
    {
        m_pPhysicsCharacter->AddBoneVolumes(g_PhysicsWorld, g_CollisionSpace, m_pPoseAccumulator, m_pPhysicsData, 0x80, 0x20);
        m_szEffectsName = NULL;
    }

    m_pHeadTrack = new (nlMalloc(sizeof(cHeadTrack), 8, false)) cHeadTrack();

    cSHierarchy* hierarchy0 = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nHeadJointIndex = hierarchy0->GetNodeIndexByID(nlStringLowerHash("bip01 head"));

    cSHierarchy* hierarchy1 = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nBip01JointIndex_0xA4 = hierarchy1->GetNodeIndexByID(nlStringLowerHash("bip01"));

    cSHierarchy* hierarchy2 = m_pPoseAccumulator->m_BaseSHierarchy;
    m_nSpine1JointIndex = hierarchy2->GetNodeIndexByID(nlStringLowerHash("bip01 spine1"));

    m_pCharacterSFX = new (nlMalloc(sizeof(Audio::cCharacterSFX), 8, false)) Audio::cCharacterSFX();

    m_pEffectsTexturing = NULL;

    m_pBlinker = MakeBlinker(m_eCharacterClass, (u32)nModelID[0]);

    if (m_pBlinker != NULL && !m_pBlinker->m_bValid)
    {
        delete m_pBlinker;
        m_pBlinker = NULL;
    }

    m_v3ScreenPosition.x = 0.0f;
    m_v3ScreenPosition.y = 0.0f;
    m_v3ScreenPosition.z = 0.0f;
}

inline eVariantType VariantTypeOf(const nlVector3&)
{
    return FT_VECTOR;
}

/**
 * Offset/Address/Size: 0x2050 | 0x8000FF9C | size: 0x1E8
 */
cCharacter::~cCharacter()
{
    u32 characterIndex = GetCharacterIndex(this);
    EmissionManager::Destroy(characterIndex, nullptr);
    m_pEffectsTexturing = nullptr;

    if (m_pPhysicsData != nullptr)
    {
        delete m_pPoseTree;
    }

    delete m_pPoseAccumulator;
    delete m_pSkinMesh[0];

    if (m_pSkinMesh[1] != nullptr)
    {
        delete m_pSkinMesh[1];
    }

    if (m_pPhysicsCharacter != nullptr)
    {
        delete m_pPhysicsCharacter;
    }

    delete m_pHeadTrack;
    delete m_pCharacterSFX;

    if (m_pBlinker != nullptr)
    {
        delete m_pBlinker;
    }
}

/**
 * Offset/Address/Size: 0x202C | 0x8000FF78 | size: 0x24
 */
GLSkinMesh* cCharacter::GetSkinMesh() const
{
    GLSkinMesh* skinMesh = m_pSkinMesh[m_ModelType];
    if (skinMesh == nullptr)
    {
        skinMesh = m_pSkinMesh[0];
    }
    return skinMesh;
}

/**
 * Offset/Address/Size: 0x2000 | 0x8000FF4C | size: 0x2C
 */
void cCharacter::AdjustPoseMatrices()
{
    m_pPoseAccumulator->MultNodeMatrices(&m_m4WorldMatrix);
}

/**
 * Offset/Address/Size: 0x1FA4 | 0x8000FEF0 | size: 0x5C
 */
void cCharacter::AttachEffect(EmissionController* pEmissionController)
{
    u32 characterIndex = GetCharacterIndex(this);
    pEmissionController->m_uUserData = characterIndex;
    pEmissionController->SetPoseAccumulator(*m_pPoseAccumulator);
    pEmissionController->SetAnimController(*m_pCurrentAnimController);
    pEmissionController->m_aFacing = m_aActualFacingDirection;
}

/**
 * Offset/Address/Size: 0x1E5C | 0x8000FDA8 | size: 0x148
 */
s16 cCharacter::CalcAnimTurnAdjust(unsigned short aFacingDirection, unsigned short aDesiredFacingDirection, int nAnimID)
{
    unsigned short aAnimRot;
    unsigned short aFinalFacingDirection;
    cSAnim* const pAnim = m_pAnimInventory->GetAnim(nAnimID);
    cPN_SAnimController* pAnimController = AllocateSAnimController();

    pAnimController = ::new (&*pAnimController) cPN_SAnimController(
        pAnim,
        GetCharacterAnimRetarget(this, pAnim),
        m_pAnimInventory->GetPlayMode(nAnimID),
        NULL,
        0,
        m_pAnimInventory->GetMirrored(nAnimID));

    pAnimController->m_fPrevTime = pAnimController->m_fTime;
    pAnimController->m_fTime = 0.0f;
    pAnimController->m_fPrevTime = pAnimController->m_fTime;
    pAnimController->m_fTime = 1.0f;

    pAnimController->GetRootRot(&aAnimRot);
    aFinalFacingDirection = aFacingDirection + aAnimRot;

    delete pAnimController;

    return (signed short)(aDesiredFacingDirection - aFinalFacingDirection);
}

/**
 * Offset/Address/Size: 0x1DF4 | 0x8000FD40 | size: 0x68
 */
s16 cCharacter::GetFacingDeltaToPosition(const nlVector3& position)
{
    float dy = position.y - m_v3Position.y;
    float dx = position.x - m_v3Position.x;
    float angleRad = nlATan2f(dy, dx);

    // Convert radians to 16-bit angle format (65536.0f / (2*pi) ~= 10430.378f)
    float angle16 = 10430.378f * angleRad;
    u16 targetAngle = (u16)(s32)angle16;

    return (s16)(targetAngle - m_aActualFacingDirection);
}

static inline AnimRetarget* GetCharacterAnimRetarget(const cCharacter* character, const cSAnim* pSAnim)
{
    AnimRetarget* result = NULL;
    if (character->m_pAnimRetargetList != NULL)
    {
        result = character->m_pAnimRetargetList->GetAnimRetargetWithSignature(pSAnim);
    }
    return result;
}

/**
 * Offset/Address/Size: 0x1DCC | 0x8000FD18 | size: 0x28
 */
nlVector3& cCharacter::GetJointPosition(int jointIndex) const
{
    const nlMatrix4& poseMatrix = m_pPoseAccumulator->GetNodeMatrix(jointIndex);
    return *(nlVector3*)&poseMatrix.e2[3];
}

/**
 * Offset/Address/Size: 0x1AA8 | 0x8000F9F4 | size: 0x324
 */
void cCharacter::GetJointPositionFuture(nlVector3* v3Out, int nAnimIndex, int nJointIndex, float fTime, bool bAddRootTrans, bool bAddRootRot, bool bUsePrevPosition)
{
    nlMatrix4 m4RootMat;
    m4RootMat.SetIdentity();

    cPoseAccumulator poseAccumulator(m_pPoseAccumulator->m_BaseSHierarchy, true);

    cSAnim* pAnim = m_pAnimInventory->GetAnim(nAnimIndex);
    AnimRetarget* pRetarget = NULL;
    if (m_pAnimRetargetList != NULL)
    {
        pRetarget = m_pAnimRetargetList->GetAnimRetargetWithSignature(pAnim);
    }

    cPN_SAnimController animController(pAnim, pRetarget, PM_CYCLIC, NULL, 0, false);
    animController.m_bMirror = m_pAnimInventory->GetMirrored(nAnimIndex);
    animController.m_fPrevTime = animController.m_fTime;
    animController.m_fTime = fTime;

    if (bAddRootRot)
    {
        u16 aCurRotation = 0;
        if (bUsePrevPosition)
        {
            aCurRotation = m_aActualFacingDirection;
        }

        u16 aRootRotation;
        animController.GetRootRot(&aRootRotation);
        aCurRotation += aRootRotation;

        nlMakeRotationMatrixZ(m4RootMat, 0.0000958738f * (float)aCurRotation);
    }

    if (bAddRootTrans)
    {
        u16 aCurRotation = 0;
        if (bUsePrevPosition)
        {
            aCurRotation = m_aActualFacingDirection;
        }

        nlVector3 v3RootVelocity;
        animController.GetRootTrans(&v3RootVelocity, aCurRotation);
        v3RootVelocity.z = 0.0f;

        if (bUsePrevPosition)
        {
            v3RootVelocity.x += m_v3Position.x;
            v3RootVelocity.y += m_v3Position.y;
        }

        if (nJointIndex < 0)
        {
            *v3Out = v3RootVelocity;
            return;
        }

        m4RootMat.e2[3][0] = v3RootVelocity.x;
        m4RootMat.e2[3][1] = v3RootVelocity.y;
        m4RootMat.e2[3][2] = v3RootVelocity.z;
        m4RootMat.e2[3][3] = 1.0f;
    }

    poseAccumulator.Pose(animController, m4RootMat);

    const nlMatrix4& m4NodeMatrix = poseAccumulator.GetNodeMatrix(nJointIndex);
    *v3Out = *(nlVector3*)&m4NodeMatrix.e2[3][0];
}

/**
 * Offset/Address/Size: 0x1864 | 0x8000F7B0 | size: 0x244
 */
void cCharacter::GetCurrentAnimFuture(int nJointIndex, float fTime, nlVector3& v3Out, nlVector3& v3FutureRoot, unsigned short& outFacing)
{
    cPN_SAnimController* pAnim = m_pCurrentAnimController;
    float savedPrevTime = pAnim->m_fPrevTime;
    float savedTime = pAnim->m_fTime;

    pAnim->m_fPrevTime = pAnim->m_fTime;
    pAnim->m_fTime = fTime;

    outFacing = m_aActualFacingDirection;
    m_pCurrentAnimController->GetRootTrans(&v3FutureRoot, outFacing);

    unsigned short rootRot;
    m_pCurrentAnimController->GetRootRot(&rootRot);
    outFacing += rootRot;

    v3FutureRoot.x += m_v3Position.x;
    v3FutureRoot.y += m_v3Position.y;
    v3FutureRoot.z = 0.0f;

    if (nJointIndex < 0)
    {
        v3Out = v3FutureRoot;
    }
    else
    {
        cPoseAccumulator* pAccumulator = new (nlMalloc(sizeof(cPoseAccumulator), 8, false)) cPoseAccumulator(m_pPoseAccumulator->m_BaseSHierarchy, true);

        nlMatrix4 m;
        nlMakeRotationMatrixZ(m, 0.0000958738f * (float)outFacing);
        m.e2[3][0] = v3FutureRoot.x;
        m.e2[3][1] = v3FutureRoot.y;
        m.e2[3][2] = v3FutureRoot.z;
        m.e2[3][3] = 1.0f;

        pAccumulator->Pose(*m_pCurrentAnimController, m);
        v3Out = *(nlVector3*)&pAccumulator->GetNodeMatrix(nJointIndex).e2[3][0];

        delete pAccumulator;
    }

    pAnim = m_pCurrentAnimController;
    pAnim->m_fPrevTime = pAnim->m_fTime;
    pAnim->m_fTime = savedPrevTime;

    pAnim = m_pCurrentAnimController;
    pAnim->m_fPrevTime = pAnim->m_fTime;
    pAnim->m_fTime = savedTime;
}

/**
 * Offset/Address/Size: 0x184C | 0x8000F798 | size: 0x18
 */
nlVector3& cCharacter::GetPrevJointPosition(int jointIndex)
{
    nlMatrix4& prevMatrix = m_pPoseAccumulator->m_PrevNodeMatrices.mData[jointIndex];
    return *(nlVector3*)&prevMatrix.e2[3];
}

/**
 * Offset/Address/Size: 0x1790 | 0x8000F6DC | size: 0xBC
 */
void cCharacter::InitBlur(int nLength)
{
    char texturename[64];
    s32 maxEntries = nLength;

    if (m_pBlurHandler != NULL)
    {
        BlurManager::DestroyHandler(m_pBlurHandler, 0.15f);
        m_pBlurHandler = NULL;
    }

    if (maxEntries == 0)
    {
        maxEntries = 30;
    }

    if (m_eClassType == FIELDER)
    {
        nlStrNCat<char>(texturename, szMushroomBlurTextureBase, ((cPlayer*)this)->m_pTeam->GetCaptain()->m_szEffectsName, 0x40);
    }

    const char* finalTextureName;
    if (glTextureLoad(glGetTexture(texturename)))
    {
        finalTextureName = texturename;
    }
    else
    {
        finalTextureName = szMushroomBlurTexture;
    }

    m_pBlurHandler = BlurManager::GetNewHandler(finalTextureName, 0.35f, maxEntries, true);
}

/**
 * Offset/Address/Size: 0x174C | 0x8000F698 | size: 0x44
 */
void cCharacter::EndBlur()
{
    if (m_pBlurHandler != NULL)
    {
        m_pBlurHandler->Die(0.0f);
        m_pBlurHandler = NULL;
    }
}

/**
 * Offset/Address/Size: 0x1740 | 0x8000F68C | size: 0xC
 */
void cCharacter::InitMovementCoast()
{
    m_eMovementState = MOVEMENT_COAST;
}

/**
 * Offset/Address/Size: 0x1730 | 0x8000F67C | size: 0x10
 */
void cCharacter::InitMovementDecelerateExponential(float fDecel)
{
    m_eMovementState = MOVEMENT_DECELERATE_EXPONENTIAL;
    m_fDecel = fDecel;
}

/**
 * Offset/Address/Size: 0x16F4 | 0x8000F640 | size: 0x3C
 */
void cCharacter::InitMovementFromAnim(short fDirectionSeekSpeed, const nlVector3& v3AnimMoveAdjust, float fAdjustEndTime, bool bBlended)
{
    m_eMovementState = MOVEMENT_FROM_ANIM;
    m_nAnimTurnAdjust = fDirectionSeekSpeed;
    m_v3AnimMoveAdjust = v3AnimMoveAdjust;
    m_fAnimAdjustBeginTime = m_pCurrentAnimController->m_fTime;
    m_fAnimAdjustEndTime = fAdjustEndTime;
    m_bFromAnimBlended = bBlended;
}

/**
 * Offset/Address/Size: 0x16E0 | 0x8000F62C | size: 0x14
 */
void cCharacter::InitMovementFromAnimSeek(float fDirectionSeekSpeed, float fDirectionSeekFalloff)
{
    m_eMovementState = MOVEMENT_FROM_ANIM_SEEK;
    m_fDirectionSeekSpeed = fDirectionSeekSpeed;
    m_fDirectionSeekFalloff = fDirectionSeekFalloff;
}

/**
 * Offset/Address/Size: 0x16CC | 0x8000F618 | size: 0x14
 */
void cCharacter::InitMovementNone(float fDirectionSeekSpeed, float fDirectionSeekFalloff)
{
    m_eMovementState = MOVEMENT_NONE;
    m_fDirectionSeekSpeed = fDirectionSeekSpeed;
    m_fDirectionSeekFalloff = fDirectionSeekFalloff;
}

/**
 * Offset/Address/Size: 0x16B0 | 0x8000F5FC | size: 0x1C
 */
void cCharacter::InitMovementRunning(float fDirectionSeekSpeed, float fDirectionSeekFalloff, float fAccel, float fDecel)
{
    m_eMovementState = MOVEMENT_RUNNING;
    m_fDirectionSeekSpeed = fDirectionSeekSpeed;
    m_fDirectionSeekFalloff = fDirectionSeekFalloff;
    m_fAccel = fAccel;
    m_fDecel = fDecel;
}

/**
 * Offset/Address/Size: 0x169C | 0x8000F5E8 | size: 0x14
 */
void cCharacter::InitMovementRunningNoTurn(float fAccel, float fDecel)
{
    m_eMovementState = MOVEMENT_RUNNING_NO_TURN;
    m_fAccel = fAccel;
    m_fDecel = fDecel;
}

/**
 * Offset/Address/Size: 0x1680 | 0x8000F5CC | size: 0x1C
 */
void cCharacter::InitMovementStrafing(float fDirectionSeekSpeed, float fDirectionSeekFalloff, float fAccel, float fDecel)
{
    m_eMovementState = MOVEMENT_STRAFING;
    m_fDirectionSeekSpeed = fDirectionSeekSpeed;
    m_fDirectionSeekFalloff = fDirectionSeekFalloff;
    m_fAccel = fAccel;
    m_fDecel = fDecel;
}

/**
 * Offset/Address/Size: 0x15AC | 0x8000F4F8 | size: 0xD4
 */
void cCharacter::MatchAnimSpeedToCharacterSpeed(uintptr_t nParam, cPN_SAnimController* pController)
{
    cFielder* fielder = (cFielder*)nParam;
    if (fielder->m_eMovementState != MOVEMENT_FROM_ANIM && fielder->m_eMovementState != MOVEMENT_FROM_ANIM_SEEK)
    {
        if (fielder->m_eClassType == FIELDER)
        {
            if (fielder->m_eAnimID == 0x1a && fielder->m_eActionState == ACTION_RUNNING_WB_TURBO)
            {
                pController->m_fPlaybackSpeedScale = 2.0f;
                return;
            }

            pController->m_fPlaybackSpeedScale = ClampMax(ClampMin(fielder->m_fActualSpeed / pController->m_pSAnim->m_fLinearSpeed, 0.6f), 1.4f);
            return;
        }

        pController->m_fPlaybackSpeedScale = ClampMax(ClampMin(fielder->m_fActualSpeed / pController->m_pSAnim->m_fLinearSpeed, 0.6f), 1.4f);
    }
}

/**
 * Offset/Address/Size: 0x1270 | 0x8000F1BC | size: 0x33C
 */
cPN_SAnimController* cCharacter::NewAnimController(int animID, bool bRestartCyclic, bool bForceMirrorSwap, void (*funcPlaybackSpeedCallback)(uintptr_t, cPN_SAnimController*), uintptr_t nPlaybackSpeedCallbackParam)
{
    bool restartCyclic = bRestartCyclic;
    bool forceMirrorSwap = bForceMirrorSwap;
    void (*playbackSpeedCallback)(uintptr_t, cPN_SAnimController*) = funcPlaybackSpeedCallback;
    uintptr_t playbackSpeedCallbackParam = (uintptr_t)this;

    if (m_pAnimInventory->GetMatchCharacterSpeed(animID))
    {
        playbackSpeedCallback = MatchAnimSpeedToCharacterSpeed;
    }
    else if (funcPlaybackSpeedCallback != NULL)
    {
        playbackSpeedCallbackParam = nPlaybackSpeedCallbackParam;
    }

    bool bMirrorSwap = false;
    float startTime = 0.0f;

    if (m_pCurrentAnimController != NULL)
    {
        if (m_eClassType == FIELDER)
        {
            if (restartCyclic || m_pAnimInventory->GetPlayMode(m_eAnimID) == PM_HOLD)
            {
                if (m_pAnimInventory->GetPlayMode(animID) == PM_CYCLIC)
                {
                    if (m_pAnimInventory->GetEndPhase(m_eAnimID) == 1)
                    {
                        bMirrorSwap = true;
                    }
                    else if (m_pAnimInventory->GetMirrored(m_eAnimID))
                    {
                        bMirrorSwap = true;
                    }
                }
                else if (m_pAnimInventory->GetPlayMode(animID) == PM_HOLD)
                {
                    bMirrorSwap = forceMirrorSwap;
                }
            }
            else if (m_pAnimInventory->GetPlayMode(m_eAnimID) == PM_CYCLIC)
            {
                if (m_pAnimInventory->GetPlayMode(animID) == PM_CYCLIC)
                {
                    startTime = m_pCurrentAnimController->m_fTime;
                    if (m_pAnimInventory->GetMirrored(m_eAnimID) != m_pAnimInventory->GetMirrored(animID))
                    {
                        bMirrorSwap = true;
                    }
                }
                else if (forceMirrorSwap)
                {
                    bMirrorSwap = true;
                }
            }
        }
        else
        {
            if (restartCyclic || m_pAnimInventory->GetPlayMode(m_eAnimID) == PM_HOLD)
            {
                if (m_pAnimInventory->GetPlayMode(animID) == PM_CYCLIC)
                {
                    if (m_pAnimInventory->GetEndPhase(m_eAnimID) == 2)
                    {
                        startTime = 0.5f;
                    }
                    else
                    {
                        startTime = 0.0f;
                    }
                }
            }
            else if (m_pAnimInventory->GetPlayMode(m_eAnimID) == PM_CYCLIC)
            {
                if (m_pAnimInventory->GetPlayMode(animID) == PM_CYCLIC)
                {
                    startTime = m_pCurrentAnimController->m_fTime;
                    if (m_pAnimInventory->GetEndPhase(m_eAnimID) != m_pAnimInventory->GetEndPhase(animID))
                    {
                        startTime += 0.5f;
                        if (startTime >= 1.0f)
                        {
                            startTime -= 1.0f;
                        }
                    }
                }
            }
        }
    }

    cSAnim* anim = m_pAnimInventory->GetAnim(animID);
    cPN_SAnimController* controller = AllocateSAnimController();

    const AnimRetarget* retarget;
    controller = ::new (&*controller) cPN_SAnimController(
        anim,
        (retarget = NULL, m_pAnimRetargetList != NULL && (retarget = m_pAnimRetargetList->GetAnimRetargetWithSignature(anim), true), retarget),
        m_pAnimInventory->GetPlayMode(animID),
        playbackSpeedCallback,
        playbackSpeedCallbackParam,
        bMirrorSwap ? !m_pAnimInventory->GetMirrored(animID) : m_pAnimInventory->GetMirrored(animID));

    controller->m_fPrevTime = controller->m_fTime;
    controller->m_fTime = startTime;

    return controller;
}

inline float ClampMin(float speedRatio, const float min)
{
    if (speedRatio >= min)
    {
        return speedRatio;
    }
    return min;
}

inline float ClampMax(float speedRatio, const float max)
{
    if (speedRatio <= max)
    {
        return speedRatio;
    }
    return max;
}

/**
 * Offset/Address/Size: 0x1230 | 0x8000F17C | size: 0x40
 */
void cCharacter::PoseLocalSpace()
{
    nlMatrix4 identityMatrix;
    identityMatrix.SetIdentity();
    m_pPoseAccumulator->Pose(*m_pPoseTree, identityMatrix);
}

/**
 * Offset/Address/Size: 0x11E4 | 0x8000F130 | size: 0x4C
 */
void cCharacter::PoseSkinMesh(cPoseAccumulator* pPoseAccumulator)
{
    GLSkinMesh* skinMesh = m_pSkinMesh[m_ModelType];
    if (skinMesh == nullptr)
    {
        skinMesh = m_pSkinMesh[0];
    }
    skinMesh->Pose(pPoseAccumulator);
}

/**
 * Offset/Address/Size: 0x117C | 0x8000F0C8 | size: 0x68
 */
void cCharacter::PrePhysicsUpdate(float dt)
{
    if ((m_eClassType != GOALIE) || ((m_v3Position.x * g_pBall->m_v3Position.x) > 0.0f))
    {
        nlMatrix4 identityMatrix;
        identityMatrix.SetIdentity();
        m_pPoseAccumulator->Pose(*m_pPoseTree, identityMatrix);
    }
}

/**
 * Offset/Address/Size: 0x1178 | 0x8000F0C4 | size: 0x4
 */
void cCharacter::PreUpdate(float dt)
{
}

/**
 * Offset/Address/Size: 0x110C | 0x8000F058 | size: 0x6C
 */
void cCharacter::CreateWorldMatrix()
{
    nlMakeRotationMatrixZ(m_m4WorldMatrix, 0.0000958738f * (f32)m_aActualFacingDirection);
    m_m4WorldMatrix.e2[3][0] = m_v3Position.x;
    m_m4WorldMatrix.e2[3][1] = m_v3Position.y;
    m_m4WorldMatrix.e2[3][2] = m_v3Position.z;
}

/**
 * Offset/Address/Size: 0x101C | 0x8000EF68 | size: 0xF0
 */
void cCharacter::PostPhysicsUpdate()
{
    m_v3PrevPosition = m_v3Position;
    m_pPhysicsCharacter->GetCharacterPositionXY(&m_v3Position);
    m_pPhysicsCharacter->GetCharacterVelocityXY(&m_v3Velocity);

    m_fActualSpeed = nlGetLength2D(m_v3Velocity.x, m_v3Velocity.y);

    float angleRad = 0.0000958738f * (float)m_aActualFacingDirection;
    nlMakeRotationMatrixZ(m_m4WorldMatrix, angleRad);

    m_m4WorldMatrix.e2[3][0] = m_v3Position.x;
    m_m4WorldMatrix.e2[3][1] = m_v3Position.y;
    m_m4WorldMatrix.e2[3][2] = m_v3Position.z;

    if (m_eClassType != GOALIE || ((m_v3Position.x * g_pBall->m_v3Position.x) > 0.0f))
    {
        m_pPoseAccumulator->MultNodeMatrices(&m_m4WorldMatrix);
    }
}

/**
 * Offset/Address/Size: 0xFE0 | 0x8000EF2C | size: 0x3C
 */
void cCharacter::ResetEffects()
{
    u32 characterIndex = GetCharacterIndex(this);
    EmissionManager::Destroy(characterIndex, nullptr);
    m_pEffectsTexturing = 0;
}

/**
 * Offset/Address/Size: 0xF90 | 0x8000EEDC | size: 0x50
 */
float cCharacter::SeekSpeedExponential(float currentValue, float targetValue, float responsiveness, float deltaTime)
{
    float adjustment;
    float distance;
    float difference;

    difference = targetValue - currentValue;
    distance = fabs(difference);

    if (distance > 0.1f)
    {
        adjustment = distance - (1.0f / ((responsiveness * deltaTime) + (1.0f / distance)));
        if (difference > 0.0f)
        {
            return currentValue + adjustment;
        }
        return currentValue - adjustment;
    }

    return targetValue;
}

/**
 * Offset/Address/Size: 0xF88 | 0x8000EED4 | size: 0x8
 */
void cCharacter::SetAnimID(int animID)
{
    m_eAnimID = animID;
}

/**
 * Offset/Address/Size: 0xE40 | 0x8000ED8C | size: 0x148
 */
void cCharacter::SetAnimState(int animID, bool useBlendTime, float fNonDefaultBlendTime, bool bRestartCyclic, bool bForceMirrorSwap)
{
    float finalBlendTime;
    cPN_SAnimController* newController;
    cPN_Blender* blender;

    if (useBlendTime)
    {
        finalBlendTime = m_pAnimInventory->GetBlendTime(animID);
    }
    else
    {
        finalBlendTime = fNonDefaultBlendTime;
    }

    newController = NewAnimController(animID, bRestartCyclic, bForceMirrorSwap, nullptr, 0);

    if (m_pAILayer[0] != nullptr && finalBlendTime != 0.0f)
    {
        blender = ::new (AllocateBlender()) cPN_Blender(m_pAILayer[0], newController, finalBlendTime);
    }
    else
    {
        delete m_pAILayer[0];
        blender = (cPN_Blender*)newController;
    }

    m_pAILayer[0] = blender;
    m_pCurrentAnimController = newController;

    SetAnimID(animID);
}

/**
 * Offset/Address/Size: 0xE10 | 0x8000ED5C | size: 0x30
 */
void cCharacter::SetFacingDirection(unsigned short dir)
{
    m_aPrevFacingDirection = m_aActualFacingDirection;
    m_aActualFacingDirection = dir;
    m_pPhysicsCharacter->SetFacingDirection(dir);
}

/**
 * Offset/Address/Size: 0xDB8 | 0x8000ED04 | size: 0x58
 */
void cCharacter::SetPosition(const nlVector3& position)
{
    m_v3Position = position;
    m_v3PrevPosition = m_v3Position;
    m_pPhysicsCharacter->SetCharacterPositionXY(m_v3Position);
}

/**
 * Offset/Address/Size: 0xD78 | 0x8000ECC4 | size: 0x40
 */
void cCharacter::SetVelocity(const nlVector3& velocity)
{
    m_v3Velocity = velocity;
    m_pPhysicsCharacter->SetCharacterVelocityXY(m_v3Velocity);
}

/**
 * Offset/Address/Size: 0xCF4 | 0x8000EC40 | size: 0x84
 */
bool cCharacter::ShouldStartCrossBlend(int animID)
{
    float time;
    float threshold = 0.5f * m_pAnimInventory->GetBlendTime(animID);

    time = m_pCurrentAnimController->m_fTime;
    time = 1.0f - time;

    float remaining = time * ((float)(m_pCurrentAnimController->m_pSAnim->m_nNumKeys) / 30.0f);

    return (remaining <= threshold);
}

/**
 * Offset/Address/Size: 0xAF8 | 0x8000EA44 | size: 0x1FC
 */
void cCharacter::Update(float fDeltaT)
{
    m_pPoseTree = m_pPoseTree->Update(fDeltaT);
    UpdateMovementState(fDeltaT);

    if (m_bIsUsingElectrocutionTexture)
    {
        if (!EmissionManager::IsPlaying(GetCharacterIndex(this), fxGetGroup("electric_fence_character")))
        {
            if (m_bIsUsingElectrocutionTexture)
            {
                m_pEffectsTexturing = nullptr;
            }
            m_bIsUsingElectrocutionTexture = false;
        }
    }

    m_Dirt = -((0.013333334f * fDeltaT) - m_Dirt);
    if (m_Dirt < m_MinDirt)
    {
        m_Dirt = m_MinDirt;
    }

    if (m_pBlurHandler != nullptr)
    {
        bool bIsZero = (v3Zero.x == g_v3PrevJointPosition.x && v3Zero.y == g_v3PrevJointPosition.y && v3Zero.z == g_v3PrevJointPosition.z);

        if (bIsZero)
        {
            g_v3PrevJointPosition = m_v3Position;
        }

        nlVector3 jointPosition;
        nlVector3 forwardVector;

        if (m_eClassType == FIELDER)
        {
            cSHierarchy* hierarchy = m_pPoseAccumulator->m_BaseSHierarchy;
            const nlMatrix4& nodeMatrix = m_pPoseAccumulator->GetNodeMatrix(hierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 spine1")));
            jointPosition = *(nlVector3*)&nodeMatrix.e2[3][0];

            nlVec3Set(forwardVector, m_v3Position.x - m_v3PrevPosition.x, m_v3Position.y - m_v3PrevPosition.y, m_v3Position.z - m_v3PrevPosition.z);
        }
        // what happens here, if m_eClassType != FIELDER? ...

        g_v3PrevJointPosition = jointPosition;
        m_pBlurHandler->AddViewOrientedPoint(jointPosition, forwardVector);
    }
}

/**
 * Offset/Address/Size: 0xAC4 | 0x8000EA10 | size: 0x34
 */
void cCharacter::KillEffect(const EffectsGroup* effectGroup)
{
    u32 characterIndex = GetCharacterIndex(this);
    EmissionManager::Destroy(characterIndex, effectGroup);
}

/**
 * Offset/Address/Size: 0xA90 | 0x8000E9DC | size: 0x34
 */
void cCharacter::EndEffect(const EffectsGroup* effectGroup)
{
    u32 characterIndex = GetCharacterIndex(this);
    EmissionManager::Kill(characterIndex, effectGroup);
}

/**
 * Offset/Address/Size: 0xA5C | 0x8000E9A8 | size: 0x34
 */
bool cCharacter::IsPlayingEffect(const EffectsGroup* effectGroup) const
{
    u32 characterIndex = GetCharacterIndex(this);
    return EmissionManager::IsPlaying(characterIndex, effectGroup);
}

/**
 * Offset/Address/Size: 0x40C | 0x8000E358 | size: 0x650
 */
void cCharacter::UpdateMovementState(float fDeltaT)
{
    float fDesiredSpeed = m_fDesiredSpeed;

    if (m_eClassType == FIELDER)
    {
        cFielder* pFielder = (cFielder*)this;
        bool isCharging = false;
        int shotState = *(int*)pFielder->m_pShotMeter;
        if (shotState == 1 || shotState == 3 || shotState == 4)
        {
            isCharging = true;
        }
        if (!isCharging)
        {
            fDesiredSpeed = pFielder->GetSpeedPowerupAdjusted(m_fDesiredSpeed);
        }
    }

    switch (m_eMovementState)
    {
    case MOVEMENT_COAST:
    {
        float mag = nlSqrt(m_v3Velocity.x * m_v3Velocity.x + m_v3Velocity.y * m_v3Velocity.y + m_v3Velocity.z * m_v3Velocity.z, true);
        if (mag > 15.0f)
        {
            nlPolar polar;
            nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
            nlPolarToCartesian(m_v3Velocity.x, m_v3Velocity.y, polar.a, 15.0f);
        }
        break;
    }

    case MOVEMENT_DECELERATE_EXPONENTIAL:
    {
        float difference;
        float actualSpeed = m_fActualSpeed;
        difference = fDesiredSpeed - actualSpeed;
        float fDecel = m_fDecel;
        float distance = fabs(difference);
        float newSpeed;
        if (distance > 0.1f)
        {
            float adjustment = distance - (1.0f / (fDecel * fDeltaT + 1.0f / distance));
            if (difference > 0.0f)
            {
                newSpeed = actualSpeed + adjustment;
            }
            else
            {
                newSpeed = actualSpeed - adjustment;
            }
        }
        else
        {
            newSpeed = fDesiredSpeed;
        }
        m_fActualSpeed = newSpeed;
        nlPolarToCartesian(m_v3Velocity.x, m_v3Velocity.y, m_aActualMovementDirection, m_fActualSpeed);
        break;
    }

    case MOVEMENT_FROM_ANIM:
    {
        cPoseNode* pSourceNode;
        if (m_bFromAnimBlended)
        {
            pSourceNode = *m_pAILayer;
        }
        else
        {
            pSourceNode = m_pCurrentAnimController;
        }

        s16 nAdjust = 0;
        float adjustTime = m_fAnimAdjustEndTime - m_fAnimAdjustBeginTime;
        nlVector3 v3ConsumedMove;
        nlVec3Set(v3ConsumedMove, 0.0f, 0.0f, 0.0f);

        if (adjustTime > 0.0f)
        {
            float smoothStep1 = CharacterAnimSmoothStep((m_pCurrentAnimController->m_fTime - m_fAnimAdjustBeginTime) / adjustTime);
            smoothStep1 = (smoothStep1 <= 1.0f) ? smoothStep1 : 1.0f;

            float smoothStep2 = CharacterAnimSmoothStep((m_pCurrentAnimController->m_fPrevTime - m_fAnimAdjustBeginTime) / adjustTime);
            smoothStep2 = (smoothStep2 <= 1.0f) ? smoothStep2 : 1.0f;

            if (smoothStep2 < 1.0f)
            {
                float fAdjustPercent = (smoothStep1 - smoothStep2) / (1.0f - smoothStep2);
                nAdjust = (s16)((float)m_nAnimTurnAdjust * fAdjustPercent);
                m_nAnimTurnAdjust -= nAdjust;

                nlVec3Scale(v3ConsumedMove, m_v3AnimMoveAdjust, fAdjustPercent);
                nlVec3Sub(m_v3AnimMoveAdjust, m_v3AnimMoveAdjust, v3ConsumedMove);
            }
        }

        u16 aRootRotation;
        pSourceNode->GetRootRot(&aRootRotation);
        u16 prevFacing = m_aActualFacingDirection;
        u16 newFacing = prevFacing + aRootRotation + (u16)nAdjust;
        m_aPrevFacingDirection = prevFacing;
        m_aActualFacingDirection = newFacing;
        m_pPhysicsCharacter->SetFacingDirection(newFacing);

        nlVector3 v3RootTrans;
        pSourceNode->GetRootTrans(&v3RootTrans, m_aPrevFacingDirection);
        nlVec3Add(v3RootTrans, v3RootTrans, v3ConsumedMove);
        m_v3Velocity.x = v3RootTrans.x / fDeltaT;
        m_v3Velocity.y = v3RootTrans.y / fDeltaT;

        nlPolar aSpeed;
        nlCartesianToPolar(aSpeed, m_v3Velocity.x, m_v3Velocity.y);
        m_fActualSpeed = aSpeed.r;
        break;
    }

    case MOVEMENT_FROM_ANIM_SEEK:
    {
        u16 aNewFacingDirection = SeekDirection(m_aActualFacingDirection, m_aDesiredFacingDirection, m_fDirectionSeekSpeed, m_fDirectionSeekFalloff, fDeltaT);
        m_aPrevFacingDirection = m_aActualFacingDirection;
        m_aActualFacingDirection = aNewFacingDirection;
        m_pPhysicsCharacter->SetFacingDirection(aNewFacingDirection);

        nlVector3 v3RootTrans;
        m_pCurrentAnimController->GetRootTrans(&v3RootTrans, m_aPrevFacingDirection);
        m_v3Velocity.x = v3RootTrans.x / fDeltaT;
        m_v3Velocity.y = v3RootTrans.y / fDeltaT;
        break;
    }

    case MOVEMENT_NONE:
    {
        u16 aNewFacingDirection = SeekDirection(m_aActualFacingDirection, m_aDesiredFacingDirection, m_fDirectionSeekSpeed, m_fDirectionSeekFalloff, fDeltaT);
        m_aPrevFacingDirection = m_aActualFacingDirection;
        m_aActualFacingDirection = aNewFacingDirection;
        m_pPhysicsCharacter->SetFacingDirection(aNewFacingDirection);

        m_fActualSpeed = 0.0f;
        nlPolarToCartesian(m_v3Velocity.x, m_v3Velocity.y, m_aActualMovementDirection, m_fActualSpeed);
        break;
    }

    case MOVEMENT_RUNNING:
    {
        u16 aNewFacingDirection = SeekDirection(m_aActualFacingDirection, m_aDesiredFacingDirection, m_fDirectionSeekSpeed, m_fDirectionSeekFalloff, fDeltaT);

        int delta = (s16)(aNewFacingDirection - m_aActualFacingDirection);
        int maxDelta = (int)(fDeltaT * m_fDirectionSeekSpeed);
        int sign = delta >> 31;
        int absDelta = (sign ^ delta) - sign;

        if (absDelta <= 182)
        {
            m_fLeanAmount = 0.0f;
        }
        else
        {
            m_fLeanAmount = NormalizeVal((float)absDelta, 182.0f, (float)maxDelta);
            if (delta < 0)
            {
                m_fLeanAmount = -m_fLeanAmount;
            }
        }

        m_aPrevFacingDirection = m_aActualFacingDirection;
        m_aActualFacingDirection = aNewFacingDirection;
        m_pPhysicsCharacter->SetFacingDirection(aNewFacingDirection);

        m_fActualSpeed = SeekSpeed(m_fActualSpeed, fDesiredSpeed, m_fAccel, m_fDecel, fDeltaT);
        if (m_fActualSpeed > 15.0f)
        {
            m_fActualSpeed = 15.0f;
        }

        nlPolarToCartesian(m_v3Velocity.x, m_v3Velocity.y, m_aActualFacingDirection, m_fActualSpeed);
        break;
    }

    case MOVEMENT_RUNNING_NO_TURN:
    {
        m_fActualSpeed = SeekSpeed(m_fActualSpeed, fDesiredSpeed, m_fAccel, m_fDecel, fDeltaT);
        if (m_fActualSpeed > 15.0f)
        {
            m_fActualSpeed = 15.0f;
        }

        nlPolarToCartesian(m_v3Velocity.x, m_v3Velocity.y, m_aActualMovementDirection, m_fActualSpeed);
        break;
    }

    case MOVEMENT_STRAFING:
    {
        u16 aNewFacingDirection = SeekDirection(m_aActualFacingDirection, m_aDesiredFacingDirection, m_fDirectionSeekSpeed, m_fDirectionSeekFalloff, fDeltaT);
        m_aPrevFacingDirection = m_aActualFacingDirection;
        m_aActualFacingDirection = aNewFacingDirection;
        m_pPhysicsCharacter->SetFacingDirection(aNewFacingDirection);

        m_aActualMovementDirection = SeekDirection(m_aActualMovementDirection, m_aDesiredMovementDirection, m_fDirectionSeekSpeed, m_fDirectionSeekFalloff, fDeltaT);

        m_fActualSpeed = SeekSpeed(m_fActualSpeed, fDesiredSpeed, m_fAccel, m_fDecel, fDeltaT);
        if (m_fActualSpeed > 15.0f)
        {
            m_fActualSpeed = 15.0f;
        }

        nlPolarToCartesian(m_v3Velocity.x, m_v3Velocity.y, m_aActualMovementDirection, m_fActualSpeed);
        break;
    }

    case MOVEMENT_UNUSED:
    default:
        break;
    }

    nlPolar pMovement;
    nlCartesianToPolar(pMovement, m_v3Velocity.x, m_v3Velocity.y);
    m_aActualMovementDirection = pMovement.a;
    m_pPhysicsCharacter->SetCharacterVelocityXY(m_v3Velocity);
}

/**
 * Offset/Address/Size: 0x394 | 0x8000E2E0 | size: 0x78
 */
void cCharacter::SetSFX(SoundPropAccessor* pSoundPropAccessor)
{
    if (Audio::IsInited())
    {
        m_pCharacterSFX->Init();
        m_pCharacterSFX->mpPhysObj = m_pPhysicsCharacter;
        m_pCharacterSFX->SetSFX(pSoundPropAccessor);
    }
}

static inline float CharacterAnimSmoothStep(float x)
{
    return (x * (x * x)) * (x * (6.0f * x + (-15.0f)) + 10.0f);
}

/**
 * Offset/Address/Size: 0x334 | 0x8000E280 | size: 0x60
 */
int cCharacter::PlaySFX(Audio::SoundAttributes& attributes)
{
    if (Audio::IsInited())
    {
        return m_pCharacterSFX->Play(attributes);
    }
    return -1;
}

/**
 * Offset/Address/Size: 0x2E4 | 0x8000E230 | size: 0x50
 */
void cCharacter::StopSFX(Audio::eCharSFX sfxType)
{
    if (Audio::IsInited())
    {
        m_pCharacterSFX->Stop(sfxType, cGameSFX::SFX_STOP_FIRST);
    }
}

/**
 * Offset/Address/Size: 0x2A8 | 0x8000E1F4 | size: 0x3C
 */
void cCharacter::StopPlayingAllTrackedSFX()
{
    if (Audio::IsInited())
    {
        m_pCharacterSFX->StopPlayingAllTrackedSFX();
    }
}

/**
 * Offset/Address/Size: 0x1D8 | 0x8000E124 | size: 0xD0
 */
int cCharacter::Play3DSFX(Audio::eCharSFX sfxType, PosUpdateMethod posUpdateMethod, float fMaxVol)
{
    if (!Audio::IsInited())
    {
        return -1;
    }

    Audio::SoundAttributes attributes;
    attributes.Init();

    attributes.me_ClassType = 1; // CHAR
    attributes.mu_Type = sfxType;
    attributes.mb_Is3D = true;
    attributes.mf_Volume = fMaxVol;
    attributes.mf_Attenuate = 1.0f;
    attributes.posUpdateMethod = posUpdateMethod;

    if (posUpdateMethod == VECTORS)
    {
        attributes.UseStationaryPosVector(m_v3Position);
    }

    return (Audio::IsInited() ? m_pCharacterSFX->Play(attributes) : -1);
}

/**
 * Offset/Address/Size: 0x158 | 0x8000E0A4 | size: 0x80
 */
void cCharacter::PlayRandomCharDialogue(unsigned long dialogueType, PosUpdateMethod posUpdateMethod, float f1, float f2)
{
    if (Audio::IsInited())
    {
        m_pCharacterSFX->PlayRandomCharDialogue((CharDialogueType)dialogueType, posUpdateMethod, f1, f2, true);
    }
}

/**
 * Offset/Address/Size: 0x12C | 0x8000E078 | size: 0x2C
 */
void cCharacter::UpdateBlinking(float fDeltaT)
{
    Blinker* pBlinker = m_pBlinker;
    if (pBlinker != NULL)
    {
        pBlinker->Update(fDeltaT);
    }
}

/**
 * Offset/Address/Size: 0xFC | 0x8000E048 | size: 0x30
 */
void cCharacter::PerformBlinking(GLSkinMesh* skinMesh, glModel* model) const
{
    Blinker* pBlinker = m_pBlinker;
    if (pBlinker != NULL)
    {
        pBlinker->Blink(model);
    }
}

/**
 * Offset/Address/Size: 0x88 | 0x8000DFD4 | size: 0x74
 */
void cCharacter::SetElectrocutionTextureEnabled(bool isEnabled)
{
    if ((m_bIsUsingElectrocutionTexture == false) && (isEnabled != false))
    {
        m_pEffectsTexturing = fxGetTexturing((eEffectsTextureType)4);
    }

    if ((m_bIsUsingElectrocutionTexture != false) && (isEnabled == false))
    {
        m_pEffectsTexturing = 0;
    }

    m_bIsUsingElectrocutionTexture = isEnabled;
}

/**
 * Offset/Address/Size: 0x0 | 0x8000DF4C | size: 0x88
 */
void cCharacter::AddRandomDirt()
{
    m_MinDirt += nlRandomf(0.05f, &nlDefaultSeed);
    if (m_MinDirt > 0.2f)
    {
        m_MinDirt = 0.2f;
    }

    m_Dirt += 0.5f + nlRandomf(0.39999998f, &nlDefaultSeed);
    if (m_Dirt > 1.0f)
    {
        m_Dirt = 1.0f;
    }
}
