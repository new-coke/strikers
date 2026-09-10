#include "Game/CharacterTriggers.h"
#include "Game/AnimInventory.h"
#include "Game/Player.h"
#include "Game/Ball.h"
#include "Game/CharacterTemplate.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Game.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/Powerups.h"
#include "Game/GameInfo.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/ReplayManager.h"
#include "Game/Render/ElectricFence.h"
#include "Game/RumbleActions.h"
#include "Game/SAnim/pnSAnimController.h"
#include "Game/SAnim.h"
#include "Game/SAnim/pnSAnimController.h"
#include "Game/Sys/debug.h"

namespace Audio
{
enum eWorldSFX
{
    WORLDSFX_DUMMY = 0,
};

class cWorldSFX : public cGameSFX
{
public:
    void Stop(eWorldSFX, cGameSFX::StopFlag);
    unsigned long Play(eWorldSFX, float, float, bool, float);
    uintptr_t Play(Audio::SoundAttributes&);
};

extern cWorldSFX gCrowdSFX;
extern cWorldSFX gStadGenSFX;
} // namespace Audio

extern cTeam* g_pTeams[2];

static inline void SetDefaultVelocity(EmissionController* pController)
{
    const nlVector3 vel = { 0.0f, 0.0f, 1.0f };
    pController->SetVelocity(vel);
}

/**
 * Offset/Address/Size: 0x5190 | 0x801A3F40 | size: 0x94
 */
void UpdateEmitterFromCharacterIdxWithoutAnimController(EmissionController& emitter, int index)
{
    if (ReplayManager::Instance()->mRender != NULL)
    {
        ReplayManager* mgr1 = ReplayManager::Instance();
        DrawableCharacter* pChar = &mgr1->mRender->mCharacters[index];
        emitter.SetPosition(pChar->mPosition);
        ReplayManager* mgr2 = ReplayManager::Instance();
        pChar = &mgr2->mRender->mCharacters[index];
        emitter.SetVelocity(pChar->mVelocity);
        ReplayManager* mgr3 = ReplayManager::Instance();
        pChar = &mgr3->mRender->mCharacters[index];
        emitter.SetPoseAccumulator(*pChar->mPoseAccumulator);
    }
}

/**
 * Offset/Address/Size: 0x50B0 | 0x801A3E60 | size: 0xE0
 */
void UpdateEmitterFromCharacterIdxWithCoordSys(EmissionController& ec, int characterIdx)
{
    if (ReplayManager::Instance()->mRender != NULL)
    {
        DrawableCharacter* pChar = &ReplayManager::Instance()->mRender->mCharacters[characterIdx];
        nlMatrix4 mRot;
        nlMakeRotationMatrixZ(mRot, 0.0000958738f * (f32)pChar->mFacingDirection);
        nlVector3 forward;
        forward.x = mRot.e2[0][0];
        forward.y = mRot.e2[0][1];
        forward.z = mRot.e2[0][2];
        ec.SetDirection(forward);
        ec.SetPosition(pChar->mPosition);
        ec.SetVelocity(pChar->mVelocity);
        ec.SetPoseAccumulator(*pChar->mPoseAccumulator);
        ec.SetAnimController(pChar->GetAnimController());
    }
}

/**
 * Offset/Address/Size: 0x4FB8 | 0x801A3D68 | size: 0xF8
 */
static void UpdateEmitterFromCharacter(EmissionController& ec)
{
    if (ReplayManager::Instance()->mRender != NULL)
    {
        uintptr_t characterIdx = ec.m_uUserData;
        if (ReplayManager::Instance()->mRender != NULL)
        {
            ReplayManager* mgr1 = ReplayManager::Instance();
            DrawableCharacter* pChar = &mgr1->mRender->mCharacters[characterIdx];
            ec.SetPosition(pChar->mPosition);
            RenderSnapshot* render = ReplayManager::Instance()->mRender;
            ec.SetVelocity(render->mCharacters[characterIdx].mVelocity);
            ReplayManager* mgr3 = ReplayManager::Instance();
            pChar = &mgr3->mRender->mCharacters[characterIdx];
            ec.SetPoseAccumulator(*pChar->mPoseAccumulator);
            ReplayManager* mgr4 = ReplayManager::Instance();
            pChar = &mgr4->mRender->mCharacters[characterIdx];
            ec.SetAnimController(pChar->GetAnimController());
        }

        const cCharacter* pCharacter = DrawableCharacter::OnlyRenderingOneCharacter();
        if (pCharacter != NULL)
        {
            if (g_pCharacters[characterIdx] != pCharacter)
            {
                ec.Die();
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x4F34 | 0x801A3CE4 | size: 0x84
 */
static void UpdateEmitterPoseFromCharacter(EmissionController& emitter)
{
    if (ReplayManager::Instance()->mRender != NULL)
    {
        uintptr_t index = emitter.m_uUserData;
        ReplayManager* mgr1 = ReplayManager::Instance();
        DrawableCharacter* pChar = &mgr1->mRender->mCharacters[index];
        emitter.SetPoseAccumulator(*pChar->mPoseAccumulator);
        ReplayManager* mgr2 = ReplayManager::Instance();
        pChar = &mgr2->mRender->mCharacters[index];
        emitter.SetAnimController(pChar->GetAnimController());
    }
}

/**
 * Offset/Address/Size: 0x4ECC | 0x801A3C7C | size: 0x68
 */
void UpdateEmitterFromBall(EmissionController& emitter)
{
    if (ReplayManager::Instance()->mRender != NULL)
    {
        ReplayManager* mgr1 = ReplayManager::Instance();
        emitter.SetPosition(mgr1->mRender->mBall.mPosition);
        ReplayManager* mgr2 = ReplayManager::Instance();
        emitter.SetVelocity(mgr2->mRender->mBall.mVelocity);
    }
}

static inline void SetFreeUpdateCallback(EmissionController* controller, void (*callback)(EmissionController&))
{
    Function1<void, EmissionController&> update(callback);
    controller->SetUpdateCallback(update);
}

static inline void SetPoseUpdateCallback(EmissionController* controller)
{
    Function1<void, EmissionController&> update(UpdateEmitterPoseFromCharacter);
    controller->SetUpdateCallback(update);
}

/**
 * Offset/Address/Size: 0x4D18 | 0x801A3AC8 | size: 0x1B4
 */
EmissionController* EmitGeneric(cCharacter* pCharacter, const char* baseName, const char* characterName)
{
    EffectsGroup* pGroup;

    if (characterName != NULL)
    {
        char effectName[0x100];
        nlStrNCat<char>(effectName, characterName, "_", 0x100);
        nlStrNCat<char>(effectName, effectName, baseName, 0x100);
        nlStrNCat<char>(effectName, effectName, "_", 0x100);
        nlStrNCat<char>(effectName, effectName, "grass", 0x100);

        pGroup = fxGetGroup(effectName);
        if (pGroup == NULL && characterName[0] != '\0')
        {
            char fallbackName[0x100];
            nlStrNCat<char>(fallbackName, "", "_", 0x100);
            nlStrNCat<char>(fallbackName, fallbackName, baseName, 0x100);
            nlStrNCat<char>(fallbackName, fallbackName, "_", 0x100);
            nlStrNCat<char>(fallbackName, fallbackName, "grass", 0x100);
            pGroup = fxGetGroup(fallbackName);
        }
    }
    else
    {
        pGroup = fxGetGroup(baseName);
    }

    EmissionController* pControl = EmissionManager::Create(pGroup, 0);
    SetDefaultVelocity(pControl);
    pControl->m_fGround = 0.02f;
    SetPoseUpdateCallback(pControl);
    pCharacter->AttachEffect(pControl);
    return pControl;
}

extern cCharacter* g_pCurrentlyUpdatingCharacter;

static void EmitTackleReactTrigger(cCharacter* pCharacter);
static void EmitBombLandingTrigger(cCharacter* pCharacter);
static void EmitPullHeadOutTrigger(cCharacter* pCharacter);
static void EmitLandingTrigger(cCharacter* pCharacter);
static void EmitToadGoalDustTrigger(cCharacter* pCharacter);
static void EmitLandingFeetTrigger(cCharacter* pCharacter);
static void EmitShootToScoreJumpTrigger(void);
static void EmitShootToScoreWindupTrigger(cCharacter* pCharacter);
static void EmitDazedTrigger(cCharacter* pCharacter);
static void EmitBallImpactTrigger(cCharacter* pCharacter);
static void CreateMushroomEffect(cFielder* pFielder);
static void EmitSlideTackleTrailTrigger(cCharacter* pCharacter);
static void EmitDivotTrigger(cCharacter* pCharacter);
static void EmitTackleImpactTrigger(cCharacter* pCharacter);
static void EmitBallPassDialogueTrigger(cCharacter* pCharacter);
static nlMatrix4& GetHeadNodeMatrix(cCharacter* pHeadCharacter);

/**
 * Offset/Address/Size: 0x2B1C | 0x801A18CC | size: 0x21FC
 */
void CharacterTriggerHandler(uintptr_t uParam)
{
    class AnimTriggerCallbackInfo
    {
    public:
        unsigned long m_uEventID;
        float m_fIntensity;
    };

    AnimTriggerCallbackInfo* pInfo = (AnimTriggerCallbackInfo*)uParam;
    cCharacter* pCharacter = g_pCurrentlyUpdatingCharacter;
    switch (pInfo->m_uEventID)
    {
    case 0x002EF345:
    case 0x1333263B:
        break;

    case 0xC9F4F4B0:
    {
        u32 hasPad = ((cPlayer*)pCharacter)->GetGlobalPad() != NULL;
        bool ownsBall = (g_pBall != NULL && g_pCurrentlyUpdatingCharacter == g_pBall->m_pOwner);
        if (g_pBall != NULL && !hasPad && !ownsBall)
            break;
        g_pCurrentlyUpdatingCharacter->m_pCharacterSFX->PlayRandomWalkFootstep(100.0f, true);
        break;
    }

    case 0x50DF765D:
    {
        u32 hasPad = ((cPlayer*)pCharacter)->GetGlobalPad() != NULL;
        bool ownsBall = (g_pBall != NULL && g_pCurrentlyUpdatingCharacter == g_pBall->m_pOwner);
        if (g_pBall != NULL && !hasPad && !ownsBall)
            break;
        g_pCurrentlyUpdatingCharacter->m_pCharacterSFX->PlayRandomWalkFootstep(100.0f, true);
        break;
    }

    case 0x3741435B:
    {
        u32 hasPad = ((cPlayer*)pCharacter)->GetGlobalPad() != NULL;
        bool ownsBall = (g_pBall != NULL && g_pCurrentlyUpdatingCharacter == g_pBall->m_pOwner);
        if (g_pBall != NULL && !hasPad && !ownsBall)
            break;
        g_pCurrentlyUpdatingCharacter->m_pCharacterSFX->PlayRandomWalkFootstep(100.0f, true);
        break;
    }

    case 0xE36392C8:
    {
        u32 hasPad = ((cPlayer*)pCharacter)->GetGlobalPad() != NULL;
        bool ownsBall = (g_pBall != NULL && g_pCurrentlyUpdatingCharacter == g_pBall->m_pOwner);
        if (g_pBall != NULL && !hasPad && !ownsBall)
            break;
        g_pCurrentlyUpdatingCharacter->m_pCharacterSFX->PlayRandomWalkFootstep(100.0f, true);
        break;
    }

    case 0x7D452499:
    {
        s16 teamSlot = (s16)((cPlayer*)pCharacter)->m_pTeam->m_nSide;
        if (nlSingleton<GameInfoManager>::Instance()->GetTeam(teamSlot) != 8)
            break;
        bool isGoalie = (g_pCurrentlyUpdatingCharacter->m_eClassType == GOALIE);
        u32 hasPad = ((cPlayer*)g_pCurrentlyUpdatingCharacter)->GetGlobalPad() != NULL;
        bool ownsBall = (g_pBall != NULL && g_pCurrentlyUpdatingCharacter == g_pBall->m_pOwner);
        if (g_pBall != NULL && !isGoalie && !hasPad && !ownsBall)
            break;
        g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x64, (PosUpdateMethod)2, 100.0f);
        break;
    }

    case 0x8F6B5826:
    {
        s16 teamSlot = (s16)((cPlayer*)pCharacter)->m_pTeam->m_nSide;
        if (nlSingleton<GameInfoManager>::Instance()->GetTeam(teamSlot) != 8)
            break;
        bool isGoalie = (g_pCurrentlyUpdatingCharacter->m_eClassType == GOALIE);
        u32 hasPad = ((cPlayer*)g_pCurrentlyUpdatingCharacter)->GetGlobalPad() != NULL;
        bool ownsBall = (g_pBall != NULL && g_pCurrentlyUpdatingCharacter == g_pBall->m_pOwner);
        if (g_pBall != NULL && !isGoalie && !hasPad && !ownsBall)
            break;
        g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x63, (PosUpdateMethod)2, 100.0f);
        break;
    }

    case 0x0E4E0F3F:
        EmitLandingFeetTrigger(pCharacter);
        break;

    case 0x64D870A7:
        EmitToadGoalDustTrigger(pCharacter);
        break;

    case 0x4DDC1C64:
        if (pCharacter->m_eClassType != FIELDER)
            break;
        ((cFielder*)pCharacter)->ThrowPowerup();
        g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x47, (PosUpdateMethod)2, 100.0f);
        break;

    case 0x5251A784:
        EmitPullHeadOutTrigger(pCharacter);
        break;

    case 0xAC6452C8:
        EmitSlideTackleTrailTrigger(pCharacter);
        break;

    case 0x6D249451:
        pCharacter->EndEffect(fxGetGroup("slide_tackle_trail"));
        break;

    case 0x0F3E9247:
    case 0xC5408AC8:
        if (pCharacter->m_eClassType == FIELDER)
            ((cPlayer*)pCharacter)->ClearPowerupAnimState(false);
        break;

    case 0xC28E3737:
        EmitBallImpactTrigger(pCharacter);
        break;

    case 0x5F5115CE:
        EmitBallPassDialogueTrigger(pCharacter);
        break;

    case 0x31DDC064:
        if (g_pBall->mbHyperSTS)
            pCharacter->PlayRandomCharDialogue(6, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0x8F5F00CF:
        if (pCharacter->m_eClassType == GOALIE)
        {
            s16 teamSlot = (s16)((cPlayer*)pCharacter)->m_pTeam->m_nSide;
            if (nlSingleton<GameInfoManager>::Instance()->GetTeam(teamSlot) == 8)
                g_pCurrentlyUpdatingCharacter->PlayRandomCharDialogue(6, (PosUpdateMethod)2, 100.0f, -1.0f);
            else
                g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x5E, (PosUpdateMethod)2, 100.0f);
        }
        break;

    case 0x8F5ED456:
        if (pCharacter->m_eClassType == GOALIE)
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xC1, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            Audio::gStadGenSFX.Play(attrs);
        }
        break;

    case 0x00266A23:
        EmitDazedTrigger(pCharacter);
        break;

    case 0xD4C678A2:
        EmitDivotTrigger(pCharacter);
        break;

    case 0x77935C1C:
        EmitLandingTrigger(pCharacter);
        break;

    case 0xA1638ABB:
        EmitBombLandingTrigger(pCharacter);
        break;

    case 0x73819990:
        EmitTackleImpactTrigger(pCharacter);
        break;

    case 0x0C3A8B39:
        BeginRumbleAction(RUMBLE_SMALL_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
        pCharacter->Play3DSFX((Audio::eCharSFX)0x0D, (PosUpdateMethod)2, 100.0f);
        break;

    case 0xB8684601:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x0D, (PosUpdateMethod)2, 100.0f);
        break;

    case 0x71FED95E:
        BeginRumbleAction(RUMBLE_SMALL_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
        break;

    case 0x18F99186:
        BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
        break;

    case 0x9DE91576:
        ((cFielder*)pCharacter)->DoSpeedBoost();
        break;

    case 0xD900F524:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x0B, (PosUpdateMethod)2, 100.0f);
        break;

    case 0x19076C94:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x11, (PosUpdateMethod)2, 100.0f);
        g_pCurrentlyUpdatingCharacter->PlayRandomCharDialogue(3, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0xA89AC233:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x11, (PosUpdateMethod)2, 100.0f);
        g_pCurrentlyUpdatingCharacter->PlayRandomCharDialogue(2, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0x97831FA1:
        EmitTackleReactTrigger(pCharacter);
        break;

    case 0x1DF278FA:
        EmitShootToScoreWindupTrigger(pCharacter);
        break;

    case 0xFD96314E:
    {
        pCharacter->StopSFX((Audio::eCharSFX)0x4F);
        if (((cFielder*)g_pCurrentlyUpdatingCharacter)->meS2SResult == S2S_SUPER_SHOT && ((cPlayer*)g_pCurrentlyUpdatingCharacter)->IsCaptain())
            g_pCurrentlyUpdatingCharacter->StopSFX((Audio::eCharSFX)0x50);
        g_pCurrentlyUpdatingCharacter->StopSFX((Audio::eCharSFX)0x3B);
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xB5, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            Audio::gStadGenSFX.Play(attrs);
        }
        g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x3C, (PosUpdateMethod)2, 100.0f);
        break;
    }

    case 0x0A9AD93F:
    {
        if (!g_pCurrentlyUpdatingCharacter->IsPlayingEffect(fxGetGroup("shoot_to_score_windup")))
            break;
        nlMatrix4& nodeMatrix = g_pCurrentlyUpdatingCharacter->m_pPoseAccumulator->GetNodeMatrix(g_pCurrentlyUpdatingCharacter->GetHeadJointIndex());
        nlVector3 nodePos = *(nlVector3*)&nodeMatrix.e2[3][0];
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(0x4E, true);
        attrs.UseStationaryPosVector(nodePos);
        g_pCurrentlyUpdatingCharacter->PlaySFX(attrs);
        attrs.Init();
        attrs.SetSoundType(0x3A, true);
        attrs.UseStationaryPosVector(nodePos);
        g_pCurrentlyUpdatingCharacter->PlaySFX(attrs);
        EmitShootToScoreJumpTrigger();
        break;
    }

    case 0xEE2E062C:
    {
        pCharacter->StopSFX((Audio::eCharSFX)0x4E);
        nlMatrix4& nodeMatrix = g_pCurrentlyUpdatingCharacter->m_pPoseAccumulator->GetNodeMatrix(g_pCurrentlyUpdatingCharacter->GetHeadJointIndex());
        nlVector3 nodePos = *(nlVector3*)&nodeMatrix.e2[3][0];
        Audio::SoundAttributes attrs;
        attrs.Init();
        if (((cFielder*)g_pCurrentlyUpdatingCharacter)->meS2SResult == S2S_SUPER_SHOT && ((cPlayer*)g_pCurrentlyUpdatingCharacter)->IsCaptain())
            attrs.SetSoundType(0x50, true);
        else
            attrs.SetSoundType(0x4F, true);
        attrs.UseStationaryPosVector(nodePos);
        g_pCurrentlyUpdatingCharacter->PlaySFX(attrs);
        g_pCurrentlyUpdatingCharacter->StopSFX((Audio::eCharSFX)0x3A);
        attrs.Init();
        attrs.SetSoundType(0x3B, true);
        attrs.UseStationaryPosVector(nodePos);
        g_pCurrentlyUpdatingCharacter->PlaySFX(attrs);
        break;
    }

    case 0xB631C31A:
        if (pCharacter->m_eClassType == GOALIE)
            ((Goalie*)pCharacter)->DoPassRelease();
        break;

    case 0x6580E428:
    {
        u32 hasPad = ((cPlayer*)pCharacter)->GetGlobalPad() != NULL;
        bool ownsBall = (g_pBall != NULL && g_pCurrentlyUpdatingCharacter == g_pBall->m_pOwner);
        if (!ownsBall && !hasPad)
            break;
        g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x0E, (PosUpdateMethod)2, 100.0f);
        break;
    }

    case 0xEF7B7383:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x12, (PosUpdateMethod)2, 100.0f);
        break;

    case 0x0618ECF3:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x47, (PosUpdateMethod)2, 100.0f);
        if (g_pCurrentlyUpdatingCharacter->m_eClassType == GOALIE)
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xBE, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            Audio::gStadGenSFX.Play(attrs);
        }
    case 0x05120C87:
        g_pCurrentlyUpdatingCharacter->PlayRandomCharDialogue(6, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0xFCE1230C:
        pCharacter->PlayRandomCharDialogue(2, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0xF2DA216B:
        pCharacter->PlayRandomCharDialogue(1, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0x9F338B11:
        if (Audio::gStadGenSFX.IsInited())
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xC6, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            Audio::gStadGenSFX.Play(attrs);
        }
        break;

    case 0x42E9F9CF:
        pCharacter->PlayRandomCharDialogue(3, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0x12D0B9BF:
        pCharacter->PlayRandomCharDialogue(0, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0x95014E78:
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(0xBF, true);
        attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
        Audio::gStadGenSFX.Play(attrs);
        break;
    }

    case 0x21001B24:
        if (pCharacter->m_eClassType == GOALIE)
            pCharacter->Play3DSFX((Audio::eCharSFX)0x37, (PosUpdateMethod)2, 100.0f);
        break;

    case 0xA9BF9E5A:
        pCharacter->PlayRandomCharDialogue(5, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0x479F48A7:
        pCharacter->PlayRandomCharDialogue(8, (PosUpdateMethod)2, 100.0f, -1.0f);
        break;

    case 0xC19CB638:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x0C, (PosUpdateMethod)1, 100.0f);
        break;

    case 0x884CBC6E:
        if (pCharacter->m_eClassType == GOALIE)
        {
            pCharacter->Play3DSFX((Audio::eCharSFX)0x62, (PosUpdateMethod)2, 100.0f);
            g_pCurrentlyUpdatingCharacter->PlayRandomCharDialogue(1, (PosUpdateMethod)2, 100.0f, -1.0f);
        }
        break;

    case 0x93E76D8E:
        if (pCharacter->m_eClassType == GOALIE)
            pCharacter->Play3DSFX((Audio::eCharSFX)0x5D, (PosUpdateMethod)2, 100.0f);
        break;

    case 0xD847ABD3:
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(0x11, true);
        attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
        attrs.m_unk_0x7B = true;
        g_pCurrentlyUpdatingCharacter->PlaySFX(attrs);
        attrs.Init();
        attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
        attrs.m_unk_0x7B = true;
        attrs.mb_Is3D = true;
        g_pCurrentlyUpdatingCharacter->m_pCharacterSFX->PlayRandomCharDialogue((CharDialogueType)3, attrs, true, NULL);
        break;
    }

    case 0x8758B65A:
        if (pCharacter->m_eClassType == GOALIE)
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0x54, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            attrs.m_unk_0x7B = true;
            g_pCurrentlyUpdatingCharacter->PlaySFX(attrs);
        }
        else
            pCharacter->Play3DSFX((Audio::eCharSFX)0x54, (PosUpdateMethod)1, 100.0f);
        break;

    case 0x1DB5C7FF:
        pCharacter->StopSFX((Audio::eCharSFX)0x54);
        break;

    case 0x7E987E12:
        if (pCharacter->m_eClassType == GOALIE)
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xB7, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            Audio::gStadGenSFX.Play(attrs);
        }
        break;

    case 0xD4DEDCAF:
        if (pCharacter->m_eClassType == GOALIE)
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xBE, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            Audio::gStadGenSFX.Play(attrs);
        }
        break;

    case 0x09656D24:
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(0xC0, true);
        attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
        Audio::gStadGenSFX.Play(attrs);
        break;
    }

    case 0x9913FAA3:
        if (pCharacter->m_eClassType == GOALIE)
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xB6, true);
            attrs.UseStationaryPosVector(g_pCurrentlyUpdatingCharacter->m_v3Position);
            Audio::gStadGenSFX.Play(attrs);
        }
        break;

    case 0x7BEE7EA1:
        if (pCharacter->m_eClassType == GOALIE)
        {
            s16 teamSlot = (s16)((cPlayer*)pCharacter)->m_pTeam->m_nSide;
            if (nlSingleton<GameInfoManager>::Instance()->GetTeam(teamSlot) == 8)
                g_pCurrentlyUpdatingCharacter->PlayRandomCharDialogue(0, (PosUpdateMethod)2, 100.0f, -1.0f);
            else
                g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x5F, (PosUpdateMethod)2, 100.0f);
        }
        break;

    case 0x2EF4FA11:
        if (pCharacter->m_eClassType == GOALIE)
            pCharacter->Play3DSFX((Audio::eCharSFX)0x60, (PosUpdateMethod)2, 100.0f);
        break;

    case 0xC7114630:
        if (pCharacter->m_eClassType == GOALIE)
        {
            s16 teamSlot = (s16)((cPlayer*)pCharacter)->m_pTeam->m_nSide;
            if (nlSingleton<GameInfoManager>::Instance()->GetTeam(teamSlot) == 8)
                g_pCurrentlyUpdatingCharacter->PlayRandomCharDialogue(0, (PosUpdateMethod)2, 100.0f, -1.0f);
            else
                g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x61, (PosUpdateMethod)2, 100.0f);
        }
        break;

    case 0x25642360:
    case 0x35B0F74E:
        if (pCharacter->m_eClassType == GOALIE)
            pCharacter->m_pCharacterSFX->PlayRandomWalkFootstep(100.0f, true);
        break;

    case 0xB26140F5:
    {
        if (g_pBall != NULL)
        {
            Event* pEvent = g_pEventManager->CreateValidEvent(0x24, 0x3C);
            CollisionBallGroundData* pEventData = new (&pEvent->m_data) CollisionBallGroundData();

            pEventData->pBall = g_pBall;

            bool bIsShot = false;
            if (g_pBall->m_tShotTimer.m_uPackedTime != 0 && g_pBall->m_unk_0xA4)
                bIsShot = true;

            if (bIsShot)
                pEventData->bIsShot = 1;
            else
                pEventData->bIsShot = 0;

            pEventData->position = g_pBall->m_v3Position;

            pEventData->normal.x = 0.0f;
            pEventData->normal.y = 0.0f;
            pEventData->normal.z = 1.0f;

            f32 speed = pInfo->m_fIntensity;
            if (speed <= 100.0f)
            {
                pEventData->fVecZComponent = -3.0f;
            }
            else if (speed <= 200.0f)
            {
                pEventData->fVecZComponent = -4.5f;
            }
            else
            {
                pEventData->fVecZComponent = -6.0f;
            }
        }
        else
        {
            Audio::gStadGenSFX.Play((Audio::eWorldSFX)0xC8, 100.0f, -1.0f, true, 100.0f);
        }
        break;
    }

    case 0x4C59A919:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x36, (PosUpdateMethod)2, 100.0f);
        break;

    case 0xACDB2215:
        pCharacter->Play3DSFX((Audio::eCharSFX)0x12, (PosUpdateMethod)2, 100.0f);
        break;

    default:
        break;
    }
}

static void EmitSlideTackleTrailTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("slide_tackle_trail"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->m_uUserData = GetCharacterIndex(pCharacter);
    {
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
}

static void EmitDivotTrigger(cCharacter* pCharacter)
{
    if ((cPlayer*)pCharacter == g_pBall->m_pOwner)
    {
        EmissionController* pController = EmissionManager::Create(fxGetGroup("divot"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pController->SetPosition(g_pBall->m_v3Position);
        pController->SetVelocity(pCharacter->m_v3Velocity);
    }
}

static void EmitTackleImpactTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("tackle_impact"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->m_uUserData = GetCharacterIndex(pCharacter);
    {
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    BeginRumbleAction(RUMBLE_SOLID_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
}

static void EmitBallPassDialogueTrigger(cCharacter* pCharacter)
{
    cBall* pBall = g_pBall;
    bool bPlayLine7 = false;
    bool bNoPassTarget = false;
    if (pBall != NULL)
    {
        if (pBall->m_pPassTarget == NULL)
            bNoPassTarget = true;
    }
    if (bNoPassTarget)
    {
        bool bOwnerOrPrev = false;
        if (pBall->m_pOwner == (cPlayer*)pCharacter || pBall->m_pPrevOwner == (cPlayer*)pCharacter)
            bOwnerOrPrev = true;
        if (bOwnerOrPrev)
            bPlayLine7 = true;
    }

    if (pBall == NULL || pBall->m_pPassTarget == (cPlayer*)pCharacter || bPlayLine7)
        pCharacter->PlayRandomCharDialogue(7, (PosUpdateMethod)2, 100.0f, -1.0f);
    else
        pCharacter->PlayRandomCharDialogue(6, (PosUpdateMethod)2, 100.0f, -1.0f);
}

static nlMatrix4& GetHeadNodeMatrix(cCharacter* pHeadCharacter)
{
    return pHeadCharacter->m_pPoseAccumulator->GetNodeMatrix(pHeadCharacter->m_nHeadJointIndex);
}

/**
 * Offset/Address/Size: 0x2A64 | 0x801A1814 | size: 0xB8
 */
void GetAnimTriggerInfo(cCharacter* pCharacter, int animIndex, bool (*callback)(float, float, unsigned long, float, void*), void* pData)
{
    cSAnim* pAnim = pCharacter->m_pAnimInventory->GetAnim(animIndex);
    cSAnimCallback* cb = pAnim->m_pCallbackList;

    while (cb != NULL)
    {
        cSAnim* pTriggerAnim = (cSAnim*)cb->m_nParam1;
        float numKeys = (float)pAnim->m_nNumKeys;
        if (!callback(cb->m_fTime, numKeys / 30.0f, pTriggerAnim->GetHashID(), 0.0f, pData))
        {
            break;
        }
        cb = cb->next;
    }
}

static void EmitShootToScoreWindupTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("shoot_to_score_windup"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pCharacter->Play3DSFX((Audio::eCharSFX)0x1C, (PosUpdateMethod)1, 100.0f);
    Audio::gCrowdSFX.Play((Audio::eWorldSFX)0x9E, 100.0f, -1.0f, true, 1.0f);
    {
        Function1<void, EmissionController&> update2(UpdateEmitterFromBall);
        pController->SetUpdateCallback(update2);
    }
}

/**
 * Offset/Address/Size: 0x2A18 | 0x801A17C8 | size: 0x4C
 */
float GetCurrentAnimTriggerTime(cCharacter* pCharacter, unsigned long uTriggerID, unsigned int uInstanceNumber)
{
    cSAnimCallback* cb = pCharacter->m_pCurrentAnimController->m_pSAnim->m_pCallbackList;
    unsigned int count = 0;

    while (cb != NULL)
    {
        cSAnim* pCallbackAnim = (cSAnim*)cb->m_nParam1;
        if (uTriggerID == pCallbackAnim->GetHashID())
        {
            count++;
            if (count - 1 == uInstanceNumber)
            {
                return cb->m_fTime;
            }
        }
        cb = cb->next;
    }
    return -1.0f;
}

static void EmitDazedTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("dazed"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->m_uUserData = GetCharacterIndex(pCharacter);
    {
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    pCharacter->m_pCharacterSFX->StopPlayingAllRandomCharDialogue();
    if (pCharacter->m_eClassType == GOALIE)
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(0x36, true);
        attrs.UseStationaryPosVector(pCharacter->m_v3Position);
        attrs.m_unk_0x7B = true;
        pCharacter->PlaySFX(attrs);
    }
    else
        pCharacter->Play3DSFX((Audio::eCharSFX)0x36, (PosUpdateMethod)2, 100.0f);
}

/**
 * Offset/Address/Size: 0x281C | 0x801A15CC | size: 0x1FC
 */
void EmitBallImpact(cPlayer* pPlayer, bool bSilent)
{
    if (g_pBall->mbIsPerfectShot)
    {
        Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xB9, cGameSFX::SFX_STOP_FIRST);
    }
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xBA, cGameSFX::SFX_STOP_FIRST);

    EmissionController* pController = EmissionManager::Create(fxGetGroup("ball_impact"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pPlayer->AttachEffect(pController);
    pController->SetPosition(g_pBall->m_v3Position);
    pController->SetVelocity(g_pBall->m_v3Velocity);

    eClassTypes classType = pPlayer->m_eClassType;
    s32 sfxId = 0xB7;
    if (classType == FIELDER)
    {
        switch (pPlayer->m_eAnimID)
        {
        case 0x38:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            sfxId = 0xB2;
            break;
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
        case 0x21:
        case 0x22:
            sfxId = 0xC3;
            break;
        case 0x1A:
        case 0x1B:
        case 0x1C:
            sfxId = 0xC3;
            break;
        }
    }

    if (classType == FIELDER && !bSilent)
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(sfxId, true);
        attrs.UseStationaryPosVector(pPlayer->m_v3Position);
        Audio::gStadGenSFX.Play(attrs);
    }

    BeginRumbleAction(RUMBLE_SMALL_CONTACT, pPlayer->GetGlobalPad());
}

static void EmitBallImpactTrigger(cCharacter* pCharacter)
{
    if (g_pBall->mbIsPerfectShot)
        Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xB9, cGameSFX::SFX_STOP_FIRST);
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xBA, cGameSFX::SFX_STOP_FIRST);
    EmissionController* pController = EmissionManager::Create(fxGetGroup("ball_impact"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->SetPosition(g_pBall->m_v3Position);
    pController->SetVelocity(g_pBall->m_v3Velocity);
    eClassTypes classType = pCharacter->m_eClassType;
    s32 sfxId = 0xB7;
    if (classType == FIELDER)
    {
        switch (pCharacter->m_eAnimID)
        {
        case 0x38:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            sfxId = 0xB2;
            break;
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
        case 0x21:
        case 0x22:
            sfxId = 0xC3;
            break;
        case 0x1A:
        case 0x1B:
        case 0x1C:
            sfxId = 0xC3;
            break;
        }
    }
    if (classType == FIELDER)
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(sfxId, true);
        attrs.UseStationaryPosVector(pCharacter->m_v3Position);
        Audio::gStadGenSFX.Play(attrs);
    }
    BeginRumbleAction(RUMBLE_SMALL_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
}

/**
 * Offset/Address/Size: 0x2658 | 0x801A1408 | size: 0x1C4
 */
void EmitBallPass(cPlayer* pPlayer)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("ball_impact"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pPlayer->AttachEffect(pController);
    pController->SetPosition(g_pBall->m_v3Position);

    Audio::SoundAttributes attrsTrue;
    if (g_pBall->mbIsPerfectShot)
    {
        attrsTrue.Init();
        attrsTrue.SetSoundType(0xB8, true);
        attrsTrue.UseStationaryPosVector(pPlayer->m_v3Position);
        Audio::gStadGenSFX.Play(attrsTrue);

        attrsTrue.Init();
        attrsTrue.SetSoundType(0xB9, true);
        attrsTrue.UsePhysObj(g_pBall->m_pPhysicsBall);
        Audio::gStadGenSFX.Play(attrsTrue);
    }
    else
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(0xB6, true);
        attrs.UseStationaryPosVector(pPlayer->m_v3Position);
        Audio::gStadGenSFX.Play(attrs);
    }

    if (!g_pBall->mbHyperSTS)
    {
        pPlayer->PlayRandomCharDialogue(6, (PosUpdateMethod)2, 100.0f, -1.0f);
    }
}

static inline EmissionController* CreateChipShotDivotEffect(cPlayer* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("divot"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    return pController;
}

/**
 * Offset/Address/Size: 0x1B10 | 0x801A08C0 | size: 0xB48
 */
void EmitBallShot(cPlayer* pCharacter, eBallShotEffectType eNewBallEffect, cPlayer* pPassTarget, bool bSilent)
{
    EmissionController* pControl = NULL;
    EmissionController* pGlowControl = NULL;
    unsigned long kickSound = (unsigned long)-1;

    switch (eNewBallEffect)
    {
    case BALL_EFFECT_S2S_SUPER_SHOT:
    {
        EmissionController* pController = EmissionManager::Create(fxGetGroup("shoot_to_score_shot"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pGlowControl = pController;
        BeginRumbleAction((eRumbleActionPreset)5, pCharacter->GetGlobalPad());
        break;
    }
    case BALL_EFFECT_S2S_SHOT:
    {
        if (pCharacter->IsCaptain() || nlSingleton<GameInfoManager>::Instance()->GetTeam((s16)pCharacter->m_pTeam->m_nSide) == 8)
        {
            BasicString<char, Detail::TempStringAllocator> effectName(GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((s16)pCharacter->m_pTeam->m_nSide)));
            effectName.AppendInPlace("_shoot_to_score_shot");

            if (fxGetGroup(effectName.c_str()) != NULL)
            {
                EmissionController* pController = EmissionManager::Create(fxGetGroup(effectName.c_str()), 0);
                SetDefaultVelocity(pController);
                pController->m_fGround = 0.02f;
                SetPoseUpdateCallback(pController);
                pCharacter->AttachEffect(pController);
                pGlowControl = pController;
            }
        }
        else
        {
            EmissionController* pController = EmissionManager::Create(fxGetGroup("shoot_to_score_shot"), 0);
            SetDefaultVelocity(pController);
            pController->m_fGround = 0.02f;
            SetPoseUpdateCallback(pController);
            pCharacter->AttachEffect(pController);
            pGlowControl = pController;
        }
        BeginRumbleAction((eRumbleActionPreset)5, pCharacter->GetGlobalPad());
        break;
    }
    case BALL_EFFECT_PERFECT_SHOT:
    {
        EmissionController* pController = EmissionManager::Create(fxGetGroup("ball_shot_perfect"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pControl = pController;

        pController = EmissionManager::Create(fxGetGroup("ball_shot_perfect_glow"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pGlowControl = pController;

        BeginRumbleAction((eRumbleActionPreset)3, pCharacter->GetGlobalPad());
        kickSound = 0xBB;
        g_pBall->InitiateBallBlur(eNewBallEffect, NULL);
        break;
    }
    case BALL_EFFECT_PERFECT_PASS:
    {
        EmissionController* pController = EmissionManager::Create(fxGetGroup("ball_pass_perfect"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pControl = pController;

        pController = EmissionManager::Create(fxGetGroup("ball_pass_perfect_glow"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pGlowControl = pController;

        BeginRumbleAction((eRumbleActionPreset)1, pCharacter->GetGlobalPad());
        g_pBall->InitiateBallBlur(eNewBallEffect, NULL);
        pCharacter->Play3DSFX((Audio::eCharSFX)0x3D, (PosUpdateMethod)2, 100.0f);

        const nlVector3& ballPos = g_pBall->m_v3Position;
        const nlVector3& targetPos = g_pBall->m_pPassTarget->m_v3Position;
        nlVector3 delta;
        nlVec3Sub(delta, ballPos, targetPos);
        const float distSq = delta.GetLengthSq3D();
        if (distSq > g_pGame->m_pGameTweaks->fPerfectPassProximityFilterDistSq)
        {
            Audio::SoundAttributes attrs;
            attrs.Init();
            attrs.SetSoundType(0xBA, true);
            attrs.UsePhysObj(g_pBall->m_pPhysicsBall);
            Audio::gStadGenSFX.Play(attrs);
        }
        break;
    }
    case BALL_EFFECT_ONETIMER_SHOT:
    {
        EmissionController* pController = EmissionManager::Create(fxGetGroup("ball_shot_onetimer"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pControl = pController;
        BeginRumbleAction((eRumbleActionPreset)3, pCharacter->GetGlobalPad());
        kickSound = 0xB4;
        g_pBall->InitiateBallBlur(eNewBallEffect, NULL);
        break;
    }
    case BALL_EFFECT_CHIP_SHOT:
    {
        EmissionController* pController = CreateChipShotDivotEffect(pCharacter);
        pController->SetPosition(g_pBall->m_v3Position);
        pController->SetVelocity(pCharacter->m_v3Velocity);
        // fall through
    }
    case BALL_EFFECT_REGULAR_SHOT:
        g_pBall->InitiateBallBlur(eNewBallEffect, NULL);
        // fall through
    default:
    {
        EmissionController* pController = EmissionManager::Create(fxGetGroup("ball_shot"), 0);
        SetDefaultVelocity(pController);
        pController->m_fGround = 0.02f;
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pControl = pController;
        BeginRumbleAction((eRumbleActionPreset)1, pCharacter->GetGlobalPad());
        kickSound = 0xB3;
        break;
    }
    }

    if (pCharacter->m_eAnimID == 0x4C)
    {
        kickSound = 0xB1;
    }

    if (!bSilent && kickSound != (unsigned long)-1)
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(kickSound, true);
        attrs.UseStationaryPosVector(g_pBall->m_v3Position);
        Audio::gStadGenSFX.Play(attrs);
    }

    pCharacter->m_pTeam->GetOtherNet();

    if (pControl != NULL)
    {
        pControl->SetPosition(g_pBall->m_v3Position);
        pControl->SetVelocity(pCharacter->m_v3Velocity);
    }

    if (pGlowControl != NULL)
    {
        pGlowControl->SetPosition(g_pBall->m_v3Position);
        pGlowControl->SetVelocity(pCharacter->m_v3Velocity);

        Function1<void, EmissionController&> update2(UpdateEmitterFromBall);
        pGlowControl->SetUpdateCallback(update2);
    }
}

/**
 * Offset/Address/Size: 0x1A44 | 0x801A07F4 | size: 0xCC
 */
void KillBallShot(const char* name, bool kill)
{
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xB9, cGameSFX::SFX_STOP_FIRST);
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xBA, cGameSFX::SFX_STOP_FIRST);

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            cFielder* pFielder = g_pTeams[i]->GetFielder(j);
            if (pFielder)
            {
                if (kill)
                {
                    pFielder->KillEffect(fxGetGroup(name));
                }
                else
                {
                    pFielder->EndEffect(fxGetGroup(name));
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x1A18 | 0x801A07C8 | size: 0x2C
 */
void EmitSolidRumble(cPlayer* player)
{
    BeginRumbleAction(RUMBLE_SHOT_CONTACT, player->GetGlobalPad());
}

/**
 * Offset/Address/Size: 0x18D0 | 0x801A0680 | size: 0x148
 */
void EmitElectrocutionExplosion(cCharacter* pCharacter)
{
    if (g_pGame->mbCaptainShotToScoreOn)
    {
        return;
    }

    EmissionController* pController = EmissionManager::Create(fxGetGroup("electrocution_explosion"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->m_uUserData = GetCharacterIndex(pCharacter);
    Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
    pController->SetUpdateCallback(update2);
}

/**
 * Offset/Address/Size: 0x1738 | 0x801A04E8 | size: 0x198
 */
void EmitDaze(cPlayer* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("dazed"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;

    {
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(pCharacter);
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }

    pCharacter->m_pCharacterSFX->StopPlayingAllRandomCharDialogue();

    if (pCharacter->m_eClassType == GOALIE)
    {
        Audio::SoundAttributes attrs;
        attrs.Init();
        attrs.SetSoundType(0x36, true);
        attrs.UseStationaryPosVector(pCharacter->m_v3Position);
        attrs.m_unk_0x7B = true;
        pCharacter->PlaySFX(attrs);
    }
    else
    {
        pCharacter->Play3DSFX((Audio::eCharSFX)0x36, (PosUpdateMethod)2, 100.0f);
    }
}

/**
 * Offset/Address/Size: 0x16FC | 0x801A04AC | size: 0x3C
 */
void KillDaze(cPlayer* player)
{
    player->KillEffect(fxGetGroup("dazed"));
}

/**
 * Offset/Address/Size: 0x1598 | 0x801A0348 | size: 0x164
 */
void EmitFreeze(cPlayer* pCharacter)
{
    pCharacter->Play3DSFX((Audio::eCharSFX)0x1f, (PosUpdateMethod)2, 100.0f);
    EmissionController* pController = EmissionManager::Create(fxGetGroup("freeze"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    {
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(pCharacter);
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    BeginRumbleAction(RUMBLE_SHOT_CONTACT, pCharacter->GetGlobalPad());
    pCharacter->m_pEffectsTexturing = fxGetTexturing(eFXTex_Freeze);
}

/**
 * Offset/Address/Size: 0x1420 | 0x801A01D0 | size: 0x178
 */
void EmitUnFreeze(cPlayer* pCharacter)
{
    pCharacter->Play3DSFX((Audio::eCharSFX)0x20, (PosUpdateMethod)2, 100.0f);
    pCharacter->KillEffect(fxGetGroup("freeze"));
    pCharacter->m_pEffectsTexturing = NULL;
    EmissionController* pController = EmissionManager::Create(fxGetGroup("unfreeze"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    {
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(pCharacter);
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    BeginRumbleAction((eRumbleActionPreset)1, pCharacter->GetGlobalPad());
}

static s32 sNumUpdatesBetweenElectrocutionToggles = 5;

/**
 * Offset/Address/Size: 0x1400 | 0x801A01B0 | size: 0x20
 */
static void ElectrocutionFinishedCallback(EmissionController& ec)
{
    g_pCharacters[ec.m_uUserData]->m_pEffectsTexturing = NULL;
}

/**
 * Offset/Address/Size: 0x1290 | 0x801A0040 | size: 0x170
 */
static void ElectrocutionUpdateCallback(EmissionController& ec)
{
    cCharacter* pCharacter = g_pCharacters[ec.m_uUserData];
    s32 charIndex = GetCharacterIndex(pCharacter);

    if (ReplayManager::Instance()->mRender != NULL)
    {
        ReplayManager* mgr1 = ReplayManager::Instance();
        DrawableCharacter* pChar = &mgr1->mRender->mCharacters[charIndex];
        ec.SetPosition(pChar->mPosition);
        ReplayManager* mgr2 = ReplayManager::Instance();
        ec.SetVelocity(mgr2->mRender->mCharacters[charIndex].mVelocity);
        ReplayManager* mgr3 = ReplayManager::Instance();
        pChar = &mgr3->mRender->mCharacters[charIndex];
        ec.SetPoseAccumulator(*pChar->mPoseAccumulator);
        ReplayManager* mgr4 = ReplayManager::Instance();
        pChar = &mgr4->mRender->mCharacters[charIndex];
        ec.SetAnimController(pChar->GetAnimController());
    }

    static s32 numUpdatesUntilNextToggle = 0;
    static bool isEffectOn = false;

    if (numUpdatesUntilNextToggle == 0)
    {
        if (isEffectOn)
        {
            pCharacter->SetElectrocutionTextureEnabled(false);
        }
        else
        {
            pCharacter->SetElectrocutionTextureEnabled(true);
        }
        isEffectOn = !isEffectOn;
        numUpdatesUntilNextToggle = nlRandom(sNumUpdatesBetweenElectrocutionToggles, &nlDefaultSeed);
    }
    else
    {
        numUpdatesUntilNextToggle--;
    }
}

/**
 * Offset/Address/Size: 0x1060 | 0x8019FE10 | size: 0x230
 */
void CharacterElectrocutionEffect(cCharacter* pCharacter, const nlVector3& v3Position, const nlVector3& v3Normal)
{
    if (g_pGame->mbCaptainShotToScoreOn)
    {
        return;
    }

    if (!EmissionManager::IsPlaying(GetCharacterIndex(pCharacter), fxGetGroup("electric_fence_character")))
    {
        pCharacter->m_pCharacterSFX->Stop((Audio::eCharSFX)0x46, cGameSFX::SFX_STOP_FIRST);
        pCharacter->Play3DSFX((Audio::eCharSFX)0x46, (PosUpdateMethod)2, 100.0f);
        pCharacter->PlayRandomCharDialogue(4, (PosUpdateMethod)2, 100.0f, -1.0f);
    }

    EmissionController* pController = EmissionManager::Create(fxGetGroup("electrocution"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    {
        SetFreeUpdateCallback(pController, UpdateEmitterPoseFromCharacter);

        pCharacter->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(pCharacter);
        Function1<void, EmissionController&> finished(ElectrocutionFinishedCallback);
        pController->SetFinishedCallback(finished);
    }
    {
        Function1<void, EmissionController&> update2(ElectrocutionUpdateCallback);
        pController->SetUpdateCallback(update2);
    }
    EmitElectricFenceCharacterEffect(v3Position, v3Normal, GetCharacterIndex(pCharacter));
}

/**
 * Offset/Address/Size: 0xFF4 | 0x8019FDA4 | size: 0x6C
 */
void EmitBallWallHit(const char* name)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup(name), 0);
    const nlVector3 vel = { 0.0f, 0.0f, 1.0f };
    pController->SetVelocity(vel);
    pController->SetPosition(g_pBall->m_v3Position);
}

/**
 * Offset/Address/Size: 0xE7C | 0x8019FC2C | size: 0x178
 */
void EmitGoalieCatch(cPlayer* pPlayer, const char* name, bool bRumble)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup(name), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pPlayer->AttachEffect(pController);
    pController->SetPosition(g_pBall->m_v3Position);

    if (bRumble)
    {
        for (int i = 0; i < 2; i++)
        {
            cTeam* pTeam = g_pTeams[i];
            if (pTeam != NULL)
            {
                for (int j = 0; j < 4; j++)
                {
                    cFielder* pFielder = pTeam->GetFielder(j);
                    if (pFielder != NULL)
                    {
                        BeginRumbleAction(RUMBLE_SHOOT_TO_SCORE, pFielder->GetGlobalPad());
                    }
                }
            }
        }
    }

    Event* pEvent = g_pEventManager->CreateValidEvent(0x12, 0x38);
    GoalieSaveData* pSaveData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) GoalieSaveData();
    pSaveData->pGoalie = pPlayer;
}

static void EmitLandingFeetTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("landing_feet"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->SetPosition(pCharacter->m_v3Position);
}

static void EmitToadGoalDustTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("toad_goal_hi_0_dust"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->SetPosition(pCharacter->m_v3Position);
}

/**
 * Offset/Address/Size: 0xD2C | 0x8019FADC | size: 0x150
 */
void EmitShootToScoreHyperStrike(cFielder* pFielder)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("shoot_to_score_hyper"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pFielder->AttachEffect(pController);
    pController->SetPosition(g_pBall->m_v3Position);
    {
        Function1<void, EmissionController&> update(UpdateEmitterFromBall);
        pController->SetUpdateCallback(update);
    }
    BeginRumbleAction(RUMBLE_SHOOT_TO_SCORE_HYPER, pFielder->GetGlobalPad());
}

static void EmitShootToScoreJumpTrigger(void)
{
    EmissionController* pController;
    cCharacter* pChar2 = g_pCurrentlyUpdatingCharacter;
    pController = EmissionManager::Create(fxGetGroup("shoot_to_score_jump"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pChar2->AttachEffect(pController);
    pController->SetPosition(pChar2->m_v3Position);
}

/**
 * Offset/Address/Size: 0xC04 | 0x8019F9B4 | size: 0x128
 */
void EmitWindupAtBall(cCharacter* pCharacter, const char* name)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup(name), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    Function1<void, EmissionController&> update2(UpdateEmitterFromBall);
    pController->SetUpdateCallback(update2);
}

/**
 * Offset/Address/Size: 0xB00 | 0x8019F8B0 | size: 0x104
 */
void EmitWindupAtCharacter(cCharacter* pCharacter, const char* name)
{
    Audio::gCrowdSFX.Stop((Audio::eWorldSFX)0x9F, cGameSFX::SFX_STOP_FIRST);
    pCharacter->Play3DSFX((Audio::eCharSFX)0x16, (PosUpdateMethod)1, 100.0f);
    EmissionController* pController = EmissionManager::Create(fxGetGroup(name), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
}

/**
 * Offset/Address/Size: 0xA8C | 0x8019F83C | size: 0x74
 */
void KillWindups(cCharacter* pCharacter)
{
    KillWindup(pCharacter, "ball_shot_windup", true);
    KillWindup(pCharacter, "ball_pass_windup", true);
    KillWindup(pCharacter, "ball_sts_windup", true);
    KillWindup(pCharacter, "shoot_to_score_windup", true);
}

/**
 * Offset/Address/Size: 0x87C | 0x8019F62C | size: 0x210
 */
void KillWindup(cCharacter* pCharacter, const char* name, bool bKill)
{
    if (nlStrCmp(name, "ball_pass_windup") == 0 && g_pBall->mbHyperSTS && pCharacter->IsPlayingEffect(fxGetGroup(name)))
    {
        pCharacter->StopSFX(Audio::CHARSFX_SHOT_WINDUP);
        pCharacter->StopSFX(Audio::CHARSFX_EFFORTS_HEAD_SHAKE);
    }
    else if (nlStrCmp(name, "ball_shot_windup") == 0 && pCharacter->IsPlayingEffect(fxGetGroup(name)) && bKill)
    {
        pCharacter->StopSFX(Audio::CHARSFX_SHOT_WINDUP);
        pCharacter->StopSFX(Audio::CHARSFX_EFFORTS_HEAD_SHAKE);
    }
    else if (nlStrCmp(name, "shoot_to_score_windup") == 0 && pCharacter->IsPlayingEffect(fxGetGroup(name)))
    {
        Audio::gCrowdSFX.Stop((Audio::eWorldSFX)0x9e, cGameSFX::SFX_STOP_FIRST);
        pCharacter->StopSFX(Audio::CHARSFX_SUPER_KICK_WINDUP);
        pCharacter->StopSFX(Audio::CHARSFX_SHOT_WINDUP);
        pCharacter->StopSFX(Audio::CHARSFX_EFFORTS_HEAD_SHAKE);
        pCharacter->StopSFX(Audio::CHARSFX_SHOT_RELEASE);
        pCharacter->StopSFX(Audio::CHARSFX_SWISH_DOWN_02);
        pCharacter->StopSFX(Audio::CHARSFX_SWISH_DOWN_03);

        if (((cFielder*)pCharacter)->meS2SResult == S2S_SUPER_SHOT)
        {
            pCharacter->StopSFX(Audio::CHARSFX_GEN_STOS_JUMP);
        }

        if (IsCaptain(pCharacter->m_eCharacterClass))
        {
            pCharacter->StopSFX(Audio::CHARSFX_EFFORTS_KICK_SUPER);
            pCharacter->StopSFX(Audio::CHARSFX_EFFORTS_SHOT_WINDUP);
        }
    }
    else if (nlStrCmp(name, "ball_sts_windup") == 0)
    {
        pCharacter->StopSFX(Audio::CHARSFX_SHOT_WINDUP);
        pCharacter->StopSFX(Audio::CHARSFX_EFFORTS_HEAD_SHAKE);
    }

    pCharacter->KillEffect(fxGetGroup(name));
}

/**
 * Offset/Address/Size: 0x838 | 0x8019F5E8 | size: 0x44
 */
void EmitTurbo(cPlayer* player, const char* unused)
{
    player->InitBlur(12);
    BeginRumbleAction((eRumbleActionPreset)0, player->GetGlobalPad());
}

/**
 * Offset/Address/Size: 0x6F0 | 0x8019F4A0 | size: 0x148
 */
void EmitDust(cPlayer* player, const char* name)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup(name), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    {
        SetPoseUpdateCallback(pController);
        player->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(player);
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    BeginRumbleAction((eRumbleActionPreset)0, player->GetGlobalPad());
}

/**
 * Offset/Address/Size: 0x570 | 0x8019F320 | size: 0x180
 */
void EmitMushroom(cFielder* pFielder)
{
    CreateMushroomEffect(pFielder);
    PowerupBase::PlayPowerupSound(POWER_UP_MUSHROOM, PowerupBase::PWRUP_SOUND_ACTIVATE, pFielder->m_pPhysicsCharacter, 100.0f);
    pFielder->StopSFX(Audio::CHARSFX_PWRUP_MUSH_IN_EFFECT);
    pFielder->Play3DSFX(Audio::CHARSFX_PWRUP_MUSH_IN_EFFECT, (PosUpdateMethod)1, 100.0f);
    tDebugPrintManager::Print(DC_SOUND, "***EmitMushroom()***\n");
}

/**
 * Offset/Address/Size: 0x510 | 0x8019F2C0 | size: 0x60
 */
void KillMushroom(cFielder* pFielder)
{
    pFielder->StopSFX(Audio::CHARSFX_PWRUP_MUSH_IN_EFFECT);
    PowerupBase::PlayPowerupSound(POWER_UP_MUSHROOM, PowerupBase::PWRUP_SOUND_END, pFielder->m_pPhysicsCharacter, 100.0f);
    pFielder->EndBlur();
    tDebugPrintManager::Print(DC_SOUND, "***KillMushroom()***\n");
}

/**
 * Offset/Address/Size: 0x388 | 0x8019F138 | size: 0x188
 */
void EmitStar(cFielder* pFielder)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("star"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    {
        SetPoseUpdateCallback(pController);
        pFielder->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(pFielder);
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    PowerupBase::PlayPowerupSound(POWER_UP_STAR, PowerupBase::PWRUP_SOUND_ACTIVATE, pFielder->m_pPhysicsCharacter, 100.0f);
    pFielder->StopSFX(Audio::CHARSFX_PWRUP_STAR_IN_EFFECT);
    pFielder->Play3DSFX(Audio::CHARSFX_PWRUP_STAR_IN_EFFECT, (PosUpdateMethod)1, 100.0f);
    pFielder->m_pEffectsTexturing = fxGetTexturing(eFXTex_Star);
    tDebugPrintManager::Print(DC_SOUND, "***EmitStar()***\n");
}

/**
 * Offset/Address/Size: 0x320 | 0x8019F0D0 | size: 0x68
 */
void KillStar(cFielder* pFielder)
{
    pFielder->StopSFX(Audio::CHARSFX_PWRUP_STAR_IN_EFFECT);
    pFielder->KillEffect(fxGetGroup("star"));
    pFielder->EndBlur();
    pFielder->m_pEffectsTexturing = NULL;
    tDebugPrintManager::Print(DC_SOUND, "***KillStar()***\n");
}

static void EmitLandingTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("landing"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
}

/**
 * Offset/Address/Size: 0x2C8 | 0x8019F078 | size: 0x58
 */
void EmitChainBite(cFielder* pFielder)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("chainchomp_bite"), 0);
    pController->SetPosition(pFielder->m_v3Position);
    BeginRumbleAction(RUMBLE_SHOT_CONTACT, pFielder->GetGlobalPad());
}

static void CreateMushroomEffect(cFielder* pFielder)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("mushroom"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    {
        SetPoseUpdateCallback(pController);
        pFielder->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(pFielder);
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
}

static void EmitPullHeadOutTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("pull_head_out"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->m_uUserData = GetCharacterIndex(pCharacter);
    {
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    BeginRumbleAction(RUMBLE_MEDIUM_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
    g_pCurrentlyUpdatingCharacter->Play3DSFX((Audio::eCharSFX)0x45, (PosUpdateMethod)2, 100.0f);
}

static void EmitBombLandingTrigger(cCharacter* pCharacter)
{
    pCharacter->Play3DSFX((Audio::eCharSFX)0x1B, (PosUpdateMethod)2, 100.0f);
    EmissionController* pController = EmissionManager::Create(fxGetGroup("bomb_landing"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    BeginRumbleAction(RUMBLE_SHOT_CONTACT, ((cPlayer*)pCharacter)->GetGlobalPad());
}

/**
 * Offset/Address/Size: 0x17C | 0x8019EF2C | size: 0x14C
 */
void EmitTackleImpact(cPlayer* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("tackle_impact"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    {
        SetPoseUpdateCallback(pController);
        pCharacter->AttachEffect(pController);
        pController->m_uUserData = GetCharacterIndex(pCharacter);
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
    BeginRumbleAction(RUMBLE_SOLID_CONTACT, pCharacter->GetGlobalPad());
}

static void EmitTackleReactTrigger(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("tackle_react"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->m_uUserData = GetCharacterIndex(pCharacter);
    {
        Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
        pController->SetUpdateCallback(update2);
    }
}

/**
 * Offset/Address/Size: 0x44 | 0x8019EDF4 | size: 0x138
 */
void EmitSlideTackleTrail(cCharacter* pCharacter)
{
    EmissionController* pController = EmissionManager::Create(fxGetGroup("slide_tackle_trail"), 0);
    SetDefaultVelocity(pController);
    pController->m_fGround = 0.02f;
    SetPoseUpdateCallback(pController);
    pCharacter->AttachEffect(pController);
    pController->m_uUserData = GetCharacterIndex(pCharacter);
    Function1<void, EmissionController&> update2(UpdateEmitterFromCharacter);
    pController->SetUpdateCallback(update2);
}

/**
 * Offset/Address/Size: 0x0 | 0x8019EDB0 | size: 0x44
 */
void KillSlideTackleTrail(cCharacter* pCharacter)
{
    const EffectsGroup* pGroup = fxGetGroup("slide_tackle_trail");
    pCharacter->EndEffect(pGroup);
}
