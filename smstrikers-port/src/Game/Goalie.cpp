#include "Game/Goalie.h"
#include "Game/AI/GoalieLooseBall.h"
#include "Game/AI/AiUtil.h"
#include "Game/Ball.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/CharacterAudio.h"
#include "Game/SAnim.h"
#include "Game/SAnim/pnBlender.h"
#include "Game/SAnim/pnSingleAxisBlender.h"
#include "Game/Physics/PhysicsGoalie.h"
#include "Game/Physics/PhysicsFakeBall.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Field.h"
#include "Game/AnimInventory.h"
#include "Game/CharacterTriggers.h"
#include "Game/Game.h"
#include "Game/GameTweaks.h"
#include "Game/MathHelpers.h"
#include "Game/Team.h"
#include "Game/AI/FilteredRandom.h"
#include "Game/FixedUpdateTask.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/Camera/CameraMan.h"
#include "Game/ParticleUpdateTask.h"
#include "NL/plat/plataudio.h"
#include "types.h"

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
    uintptr_t Play(Audio::SoundAttributes&);
};

extern cWorldSFX gCrowdSFX;
extern cWorldSFX gStadGenSFX;
} // namespace Audio

extern cTeam* g_pCurrentlyUpdatingTeam;

namespace Fuzzy
{
FuzzyVariant GetBestPassTarget(cPlayer*);
}

float OpenTo(cPlayer*, cPlayer*);

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };

bool Goalie::mbPosGoalieNetCheck;
bool Goalie::mbNegGoalieNetCheck;
u8 Goalie::mbActionDataSetup;
f32 Goalie::mfGoalieStepDist = -1.0f;
f32 Goalie::mfGoalieStrafeDist = -1.0f;
f32 Goalie::mfGoalieRunDist = -1.0f;
f32 Goalie::mfGoalieUrgentDist = 0.8f;
f32 gfRepositionThreshold = 0.15f;
bool gbEnableBallGoalieSweepTest = true;

/**
 * Offset/Address/Size: 0xB780 | 0x8004E27C | size: 0x2B8
 */
Goalie::Goalie(eCharacterClass charClass, const int* nModelID, cSHierarchy* pHierarchy, cAnimInventory* pAnimInventory, const CharacterPhysicsData* pPhysicsData, GoalieTweaks* pCharTweaks, AnimRetargetList* pAnimRetargetList)
    : cPlayer(4, charClass, nModelID, pHierarchy, pAnimInventory, pPhysicsData, pCharTweaks, pAnimRetargetList, GOALIE)
    , mGoalieActionState(GOALIEACTION_MOVE)
    , mPrevGoalieActionState(GOALIEACTION_MOVE)
    , mpPassTarget(NULL)
    , mpShooter(NULL)
    , mnSubstate(0)
    , mMoveDirection(GOALIEDIR_IDLE)
    , mfSwitchTime(1.0f)
    , mpSaveData(NULL)
    , muSaveType(0xFFFF)
    , mbShouldMiss(false)
    , mbStunEffectActive(false)
    , mbDoIntercept(false)
    , mbDoNavigate(false)
    , mbDoHeadTrack(true)
    , mbBallImpacted(false)
    , mbNoUserControl(false)
    , mbIsPosed(false)
    , mbPickedUp(false)
    , mfSpeedScale(1.0f)
    , mpLooseBallInfo(NULL)
    , muBallChangeCount(0)
    , muBallDeflectCount(0)
    , mnOffplayPending(GOALIE_OFFPLAY_NONE)
{
    CleanGoalieAction();
    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_MOVE;
    mnSubstate = 0;

    SetAnimState(0x08, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    mnSubstate = 1;
    mMoveDirection = GOALIEDIR_IDLE;
    m_pPhysicsCharacter->m_CanCollideWithBall = true;
    mbShouldMiss = false;
    mbDoNavigate = false;
    m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
    m_pPhysicsCharacter->m_CanCollideWithWall = true;

    CleanupStun();

    mpShooter = NULL;
    mUrgency = URGENCY_LOW;
    mfSpeedScale = 1.0f;
    mbPosGoalieNetCheck = false;
    mbNegGoalieNetCheck = false;
    mbDoHeadTrack = true;
    mbBallImpacted = false;
    mbNoUserControl = false;
    mbPickedUp = false;

    GoalieSave::InitData(this);
    LooseBallAnims::Init(this);

    InitGoalieActionData();

    mFatigue.Reset();
    mFatigue.mfRecoverRate = pCharTweaks->fFatigueRecoverRate;

    m_pHeadTrack->m_bTrackOOI = true;
}

/**
 * Offset/Address/Size: 0xB714 | 0x8004E210 | size: 0x6C
 */
Goalie::~Goalie()
{
    GoalieSave::ClearData();
    LooseBallAnims::Destroy();
}

/**
 * Offset/Address/Size: 0xB598 | 0x8004E094 | size: 0x17C
 */
void Goalie::Update(float dt)
{
    cPlayer::Update(dt);

    if (mbDoHeadTrack)
    {
        if (m_pBall == NULL)
        {
            m_pHeadTrack->m_bTrackOOI = true;
            m_pHeadTrack->m_v3OOI = g_pBall->m_v3Position;
        }
        else if (mGoalieActionState == GOALIEACTION_LOOSEBALL_PICKUP && mpLooseBallInfo != NULL && mpLooseBallInfo->mAnimType == LOOSEBALL_ANIM_KICK && mpPassTarget != NULL)
        {
            m_pHeadTrack->m_bTrackOOI = true;
            m_pHeadTrack->m_v3OOI = mpPassTarget->m_v3Position;
        }
        else
        {
            m_pHeadTrack->m_bTrackOOI = false;
        }
    }
    else
    {
        m_pHeadTrack->m_bTrackOOI = false;
    }

    UpdateActionState(dt);
    mFatigue.Update(dt);
    cCharacter::Update(dt);

    if (!mbIsPosed)
    {
        PoseLocalSpace();
        m_pPhysicsCharacter->UpdatePose(m_pPoseAccumulator, m_v3Position.z);
        m_pPhysicsCharacter->GetCharacterPositionXY(&m_v3Position);
        CreateWorldMatrix();
        AdjustPoseMatrices();
        mbIsPosed = true;
    }
}

static inline cFielder* AsCollidingFielder(cPlayer* pPlayer)
{
    return static_cast<cFielder*>(pPlayer);
}

static inline float ScaleGoalieDeflectRandom(float scale, float randomValue)
{
    float result = scale * randomValue;
    result = result;
    return result;
}

/**
 * Offset/Address/Size: 0xA86C | 0x8004D368 | size: 0xD2C
 */
void Goalie::CollideWithBallCallback(cBall* pBall)
{
    cPlayer::CollideWithBallCallback(pBall);

    if (mGoalieActionState != GOALIEACTION_STS || mpShooter == NULL)
    {
        pBall->m_unk_0xA6 = false;
        pBall->mpDamageTarget = NULL;
    }

    cPlayer* pOwner = pBall->m_pOwner;
    cFielder* pFldr;
    if (pOwner != this)
    {
        switch (mGoalieActionState)
        {
        case GOALIEACTION_MOVE:
        case GOALIEACTION_LOOSEBALL_SETUP:
        case GOALIEACTION_LOOSEBALL_PURSUE_BOUNCING:
            CheckForBallOnHead();
            break;

        case GOALIEACTION_LOOSEBALL_PURSUE_ROLLING:
        {
            CleanGoalieAction();
            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_LOOSEBALL_PICKUP;
            mnSubstate = 0;
            SetAnimState(mpLooseBallInfo->mnAnimID, true, 0.2f, false, false);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);
            mMoveDirection = GOALIEDIR_IDLE;
            mfTargetTime = 0.0f;
            mfWaitTime = -1.0f;
            mbPickedUp = false;

            if (0.3f < mpLooseBallInfo->mfPickupDistance)
            {
                float fAdjustedTime = (mpLooseBallInfo->mfPickupDistance - 0.3f) * mpLooseBallInfo->mfPickupTime / mpLooseBallInfo->mfPickupDistance;
                mfTargetTime = fAdjustedTime;
                float fTargetTime = mfTargetTime;
                cPN_SAnimController* pAnim = m_pCurrentAnimController;
                pAnim->m_fPrevTime = pAnim->m_fTime;
                pAnim->m_fTime = fTargetTime;
            }

            InitiatePickup();
            break;
        }

        case GOALIEACTION_LOOSEBALL_PICKUP:
            InitiatePickup();
            break;

        case GOALIEACTION_LOOSEBALL_CATCH:
            if (pOwner == NULL)
            {
                Audio::SoundAttributes sndAtr;
                sndAtr.Init();
                sndAtr.SetSoundType(0xC0, true);
                sndAtr.UseStationaryPosVector(m_v3Position);
                sndAtr.mf_Volume = 0.4f;
                Audio::gStadGenSFX.Play(sndAtr);

                PickupBall(pBall);
                mbPickedUp = true;
                EmitGoalieCatch(this, "goalie_catch", false);
            }
            break;

        case GOALIEACTION_LOOSEBALL_DESPERATE:
        {
            cFielder* pPursueFielder = AsCollidingFielder(pOwner);
            if (pOwner != NULL)
            {
                if (IsOnSameTeam(pOwner))
                    break;

                if (!pPursueFielder->IsFallenDown(0.0f) && !pPursueFielder->IsInvincible() && pOwner != NULL && pOwner->m_eClassType == FIELDER)
                {
                    if (!((cFielder*)pOwner)->IsFallenDown(0.0f))
                    {
                        pOwner->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);
                        if (pOwner->m_pBall != NULL)
                        {
                            pOwner->ReleaseBall();
                        }
                        if (IsOnSameTeam(pOwner))
                        {
                            ((cFielder*)pOwner)->EndDesire(false);
                            ((cFielder*)pOwner)->EndAction();
                        }
                        else
                        {
                            ((cFielder*)pOwner)->InitActionSlideAttackReact(this, false);
                        }
                    }
                }
            }

            int animID = mpLooseBallInfo->mnAnimID;
            if (animID != m_eAnimID)
            {
                bool bShouldSetAnim = false;
                if (animID != m_eAnimID || (bShouldSetAnim = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
                {
                    SetAnimState(animID, true, 0.2f, false, false);
                }

                {
                    cPN_SAnimController* pAnim = m_pCurrentAnimController;
                    float pickupTime = mpLooseBallInfo->mfPickupTime;
                    pickupTime = pickupTime;
                    float targetTime = 0.5f * pickupTime;
                    float currentTime = pAnim->m_fTime;
                    currentTime = currentTime;
                    pAnim->m_fPrevTime = currentTime;
                    pAnim->m_fTime = targetTime;
                }
                InitMovementFromAnim(0, v3Zero, 1.0f, false);
            }

            if (m_pBall == NULL)
            {
                PickupBall(g_pBall);
                mbPickedUp = true;
                g_pBall->ClearShotInProgress();
                EmitGoalieCatch(this, "goalie_catch", false);
            }
            break;
        }

        case GOALIEACTION_MISS_CHIP_SHOT:
            if (m_eAnimID != 0x70 && m_pCurrentAnimController->m_fTime < 0.5555556f)
            {
                SetAnimState(0x70, true, 0.2f, false, false);
                InitMovementFromAnim(0, v3Zero, 1.0f, false);
                EmitGoalieCatch(this, "goalie_deflect", false);

                Audio::SoundAttributes sndAtr;
                sndAtr.Init();
                sndAtr.SetSoundType(0xB7, true);
                sndAtr.UseStationaryPosVector(m_v3Position);
                Audio::gStadGenSFX.Play(sndAtr);
            }
            break;

        case GOALIEACTION_STS:
            HandleSTSContact(pBall);
            break;

        case GOALIEACTION_SAVE:
        {
            if (mbBallImpacted)
                return;

            mbBallImpacted = true;
            u8 bIsPerfect = 0;
            bool bShotPerfect = false;

            if (g_pBall->m_tShotTimer.m_uPackedTime != 0 && g_pBall->m_unk_0xA4)
            {
                bShotPerfect = true;
            }

            if (bShotPerfect)
            {
                EmitGoalieCatch(this, "perfect_shot_catch", false);
                bIsPerfect = true;
            }

            if (!bIsPerfect && mpSaveData != NULL && (mpSaveData->muSaveType & 3) == 0)
            {
                Event* pEvent = g_pEventManager->CreateValidEvent(0x11, 0x38);
                GoalieSaveData* pSaveData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) GoalieSaveData();
                pSaveData->saveType = g_pBall->m_uGoalType;
                pSaveData->pShooter = g_pBall->m_pShooter;
                pSaveData->pGoalie = this;
            }

            if (mpSaveData != NULL && (mpSaveData->muSaveType & 3) != 0)
            {
                pFldr = g_pBall->GetOwnerFielder();
                if (pFldr != NULL && pFldr->m_eClassType == FIELDER)
                {
                    if (!pFldr->IsFallenDown(0.0f))
                    {
                        pFldr->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);
                        if (pFldr->m_pBall != NULL)
                        {
                            pFldr->ReleaseBall();
                        }
                        if (IsOnSameTeam(pFldr))
                        {
                            pFldr->EndDesire(false);
                            pFldr->EndAction();
                        }
                        else
                        {
                            pFldr->InitActionSlideAttackReact(this, false);
                        }
                    }
                }

                MakeSaveEvent(false);
                PickupBall(pBall);

                if (!bIsPerfect)
                {
                    EmitGoalieCatch(this, "goalie_catch", false);
                }
                break;
            }
            else
            {
                if (!mbShouldMiss)
                {
                    MakeSaveEvent(false);

                    pFldr = g_pBall->GetOwnerFielder();
                    if (pFldr != NULL && pFldr->m_eClassType == FIELDER)
                    {
                        if (!pFldr->IsFallenDown(0.0f))
                        {
                            pFldr->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);
                            if (pFldr->m_pBall != NULL)
                            {
                                pFldr->ReleaseBall();
                            }
                            if (IsOnSameTeam(pFldr))
                            {
                                pFldr->EndDesire(false);
                                pFldr->EndAction();
                            }
                            else
                            {
                                pFldr->InitActionSlideAttackReact(this, false);
                            }
                        }
                    }
                }

                if (!bIsPerfect)
                {
                    EmitGoalieCatch(this, "goalie_deflect", false);
                }

                pBall->m_tNoPickupTimer.SetSeconds(0.08f);

                if (pBall->m_tShotTimer.m_uPackedTime == 0 && mpSaveData != NULL && (mpSaveData->muSaveType & 3) == 0)
                {
                    float fForwardX = m_m4WorldMatrix.e2[0][0];
                    float fForwardY = m_m4WorldMatrix.e2[0][1];
                    float fForwardZ = m_m4WorldMatrix.e2[0][2];
                    float fDotX = pBall->m_v3Position.x - m_v3Position.x;
                    float fDotY = pBall->m_v3Position.y - m_v3Position.y;
                    float fDotZ = pBall->m_v3Position.z - m_v3Position.z;
                    float fDot = fDotX * fForwardX + fDotY * fForwardY + fDotZ * fForwardZ;

                    if (fDot > 0.0f)
                    {
                        static FilteredRandomReal randgenFScale;
                        static FilteredRandomReal randgenVScale;
                        static FilteredRandomReal randgenZVel;

                        float fForwardScale = 1.0f + ScaleGoalieDeflectRandom(1.0f, randgenFScale.genrand());
                        float fVelRandom = randgenVScale.genrand();
                        float fVelScale = 0.75f;
                        fVelRandom = fVelScale * fVelRandom;
                        fVelScale += fVelRandom;

                        nlVector3 v3NewVel;
                        nlVec3Set(v3NewVel,
                            fVelScale * m_v3Velocity.x + fForwardScale * fForwardX,
                            fVelScale * m_v3Velocity.y + fForwardScale * fForwardY,
                            fVelScale * m_v3Velocity.z + fForwardScale * fForwardZ);

                        float fZVel = 2.0f * randgenZVel.genrand();
                        v3NewVel.z = 3.0f + fZVel;

                        nlVector3 v3AngVel;
                        pBall->m_pPhysicsBall->GetAngularVelocity(&v3AngVel);
                        nlVec3Scale(v3AngVel, 0.6f);

                        pBall->SetVelocity(v3NewVel, SPINTYPE_PARAMETER, &v3AngVel);
                    }
                }
            }

            if (mpSaveData != NULL && pBall->m_pPassTarget == NULL)
            {
                float fThreshold = mpSaveData->mfMilestonePercent[1];
                if (fThreshold <= 0.0f)
                {
                    fThreshold = 0.7f * mpSaveData->mfMilestonePercent[2];
                }

                if (mpSaveData->muSaveType != 4 && !(m_pCurrentAnimController->m_fTime < fThreshold))
                    break;

                if (g_pBall->m_uGoalType != 0 && g_pBall->m_uGoalType != 1)
                    break;

                cPlayer* pScorer = g_pGame->m_pScorer;
                bool bStunnedOn = GameInfoManager::GetInstance()->IsStunnedGoaliesOn();

                if (!bStunnedOn)
                {
                    if (pScorer == NULL)
                        break;
                    if (pScorer->IsOnSameTeam(this))
                        break;
                    if (pScorer->m_eClassType != FIELDER)
                        break;
                }

                if (!bStunnedOn)
                {
                    if (g_pBall->m_unk_0xA3)
                        break;
                    if (g_pBall->m_tShotTimer.m_uPackedTime == 0)
                        break;
                    if (pScorer != g_pBall->m_pShooter)
                        break;
                    if (!(*(float*)((u8*)((cFielder*)pScorer)->m_pShotMeter + 0x0C) > 0.8f))
                        break;
                }

                mbDoHeadTrack = false;
                CleanGoalieAction();
                mPrevGoalieActionState = mGoalieActionState;
                mGoalieActionState = GOALIEACTION_STS_RECOVER;
                mnSubstate = 0;
                SetAnimState(0x6F, true, 0.2f, false, false);

                mfWaitTime = ((GoalieTweaks*)m_pTweaks)->fGoalieStunTimeMin;
                float fRange = ((GoalieTweaks*)m_pTweaks)->fGoalieStunTimeMax - mfWaitTime;
                if (fRange > 0.0f)
                {
                    mfWaitTime += nlRandomf(fRange, &nlDefaultSeed);
                }

                InitMovementFromAnim(0, v3Zero, 1.0f, false);
                EmitDaze(this);
                mbStunEffectActive = true;
            }
            break;
        }

        case GOALIEACTION_PURSUE_BALL_CARRIER:
        case GOALIEACTION_PURSUE_BALL_POUNCE:
        {
            cPlayer* pPounceTarget = pBall->GetOwnerFielder();
            if (pPounceTarget != NULL && !IsOnSameTeam(pPounceTarget))
            {
                ExecutePounce(pPounceTarget, true);
            }
            break;
        }

        default:
            break;
        }
    }

    if (pBall->m_pPassTarget != NULL)
    {
        pBall->ClearPassTarget();
    }

    if (pBall->m_tShotTimer.m_uPackedTime != 0)
    {
        pBall->ClearShotInProgress();
    }
}

/**
 * Offset/Address/Size: 0xA178 | 0x8004CC74 | size: 0x6F4
 */
void Goalie::CollideWithCharacterCallback(CollisionPlayerPlayerData* pData)
{
    cPlayer* pPlayer = pData->player2;
    cPlayer::CollideWithCharacterCallback(pData);

    bool bHitReactResult;
    nlVector3 v3LocalPos;
    s32 anim;

    switch (mGoalieActionState)
    {
    case GOALIEACTION_MOVE_WB:
    {
        if (IsOnSameTeam(pPlayer))
        {
            return;
        }

        if (pPlayer->m_eClassType != FIELDER)
        {
            return;
        }

        cFielder* pFldr = (cFielder*)pPlayer;
        if (pFldr->IsFallenDown(0.0f))
        {
            return;
        }

        if (m_eAnimID == 0x0B)
        {
            return;
        }
        if (m_eAnimID == 0x0D)
        {
            return;
        }
        if (m_eAnimID == 0x0C)
        {
            return;
        }

        f32 y = pPlayer->m_v3Position.y - m_v3Position.y;
        f32 x = pPlayer->m_v3Position.x - m_v3Position.x;
        u16 aHit = RadToAng16(nlATan2f(y, x));

        bHitReactResult = pFldr->InitActionHitReact(this, aHit, false);
        mnSubstate = 6;

        GetLocalPoint(v3LocalPos, pPlayer->m_v3Position, m_v3Position, m_aActualFacingDirection);

        if (fabsf(v3LocalPos.x / v3LocalPos.y) > 1.0f)
        {
            anim = 0x0B;
        }
        else if (v3LocalPos.y > 0.0f)
        {
            anim = 0x0D;
        }
        else
        {
            anim = 0x0C;
        }

        bool bShouldSetAnim = false;
        if (anim != m_eAnimID || (bShouldSetAnim = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
        {
            SetAnimState(anim, true, 0.2f, false, false);
        }

        InitMovementFromAnim(0, v3Zero, 1.0f, false);

        if (bHitReactResult)
        {
            PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);
            pPlayer->PlayAttackReactionSounds(100.0f);
        }
        return;
    }

    case GOALIEACTION_STS_ATTACK:
    {
        cFielder* pFldr = g_pBall->GetOwnerFielder();
        bool bDoHit;

        if (pFldr != NULL && !IsOnSameTeam(pFldr) && pFldr->m_eActionState == ACTION_SHOOT_TO_SCORE)
        {
            bDoHit = 1;
        }
        else
        {
            bDoHit = 0;
        }

        if (!bDoHit)
        {
            return;
        }

        if (pPlayer != g_pBall->m_pOwner)
        {
            return;
        }

        if (!(m_pCurrentAnimController->m_fTime > 0.3f))
        {
            return;
        }

        pFldr = g_pBall->GetOwnerFielder();
        if (pFldr == NULL)
        {
            return;
        }

        if (pFldr->m_pBall != NULL)
        {
            pFldr->ReleaseBall();
        }

        pFldr->SetFacingDirection(m_aActualFacingDirection + 0x8000);
        pFldr->InitActionSTSHitReact(this);

        PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);
        pFldr->PlayAttackReactionSounds(g_pGame->m_pGameTweaks->fGoalieDropKickHitReactionVolume);

        nlVector3 v3NewVel;
        nlVec3Scale(v3NewVel, m_v3Position, -((GoalieTweaks*)m_pTweaks)->fSTSAttackBallVelMult);

        f32 yRand = nlRandomf(5.0f, &nlDefaultSeed);
        if ((u32)nlRandom(100, &nlDefaultSeed) > 50)
        {
            yRand *= -1.0f;
        }
        v3NewVel.y += yRand;
        v3NewVel.z = 4.0f + nlRandomf(2.0f, &nlDefaultSeed);

        g_pBall->SetVelocity(v3NewVel, SPINTYPE_FORWARD, NULL);
        g_pBall->m_tNoPickupTimer.SetSeconds(0.12f);
        return;
    }

    case GOALIEACTION_PURSUE_BALL_CARRIER:
    case GOALIEACTION_PURSUE_BALL_POUNCE:
        if (mbPlayMiss)
        {
            return;
        }

        if (!IsOnSameTeam(pPlayer))
        {
            ExecutePounce(pPlayer, false);
        }
        return;

    case GOALIEACTION_MOVE:
    {
        s32 jointIndex = m_nRightFootJointIndex;
        f32 zCheck = GetJointPosition(m_nHeadJointIndex).z + 0.4f;
        if (pPlayer->GetJointPosition(jointIndex).z > zCheck)
        {
            return;
        }
    }

    case GOALIEACTION_SAVE:
    case GOALIEACTION_LOOSEBALL_SETUP:
    case GOALIEACTION_LOOSEBALL_CATCH:
    case GOALIEACTION_LOOSEBALL_PICKUP:
    case GOALIEACTION_LOOSEBALL_PURSUE_BOUNCING:
    case GOALIEACTION_LOOSEBALL_PURSUE_ROLLING:
        if (IsOnSameTeam(pPlayer))
        {
            return;
        }
        if (pPlayer == NULL)
        {
            return;
        }
        if (pPlayer->m_eClassType != FIELDER)
        {
            return;
        }
        if (((cFielder*)pPlayer)->IsFallenDown(0.0f))
        {
            return;
        }

        pPlayer->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);

        if (pPlayer->m_pBall != NULL)
        {
            pPlayer->ReleaseBall();
        }

        if (IsOnSameTeam(pPlayer))
        {
            ((cFielder*)pPlayer)->EndDesire(false);
            ((cFielder*)pPlayer)->EndAction();
        }
        else
        {
            ((cFielder*)pPlayer)->InitActionSlideAttackReact(this, false);
        }
        return;

    case GOALIEACTION_LOOSEBALL_DESPERATE:
        if (pPlayer == g_pBall->m_pOwner)
        {
            if (pPlayer != NULL)
            {
                if (IsOnSameTeam(pPlayer))
                {
                    return;
                }

                if (!((cFielder*)pPlayer)->IsFallenDown(0.0f) && !((cFielder*)pPlayer)->IsInvincible())
                {
                    if (pPlayer != NULL && pPlayer->m_eClassType == FIELDER && !((cFielder*)pPlayer)->IsFallenDown(0.0f))
                    {
                        pPlayer->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);

                        if (pPlayer->m_pBall != NULL)
                        {
                            pPlayer->ReleaseBall();
                        }

                        if (IsOnSameTeam(pPlayer))
                        {
                            ((cFielder*)pPlayer)->EndDesire(false);
                            ((cFielder*)pPlayer)->EndAction();
                        }
                        else
                        {
                            ((cFielder*)pPlayer)->InitActionSlideAttackReact(this, false);
                        }
                    }
                }
            }

            anim = mpLooseBallInfo->mnAnimID;
            if (anim != m_eAnimID)
            {
                do
                {
                    if (anim == m_eAnimID)
                    {
                        cPN_SAnimController* pController = m_pCurrentAnimController;
                        u8 bShouldSetAnim = false;
                        if (pController->m_ePlayMode == PM_HOLD && pController->m_fTime == 1.0f)
                        {
                            bShouldSetAnim = true;
                        }

                        if (!bShouldSetAnim)
                        {
                            break;
                        }
                    }

                    SetAnimState(anim, true, 0.2f, false, false);
                } while (0);

                cPN_SAnimController* pController = m_pCurrentAnimController;
                f32 fPickupTime = mpLooseBallInfo->mfPickupTime;
                pController->m_fPrevTime = pController->m_fTime;
                pController->m_fTime = 0.5f * fPickupTime;
                InitMovementFromAnim(0, v3Zero, 1.0f, false);
            }

            if (m_pBall == NULL)
            {
                PickupBall(g_pBall);
                mbPickedUp = true;
                g_pBall->ClearShotInProgress();
                EmitGoalieCatch(this, "goalie_catch", false);
            }
            return;
        }

        if (IsOnSameTeam(pPlayer))
        {
            return;
        }
        if (pPlayer == NULL)
        {
            return;
        }
        if (pPlayer->m_eClassType != FIELDER)
        {
            return;
        }
        if (((cFielder*)pPlayer)->IsFallenDown(0.0f))
        {
            return;
        }

        pPlayer->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);

        if (pPlayer->m_pBall != NULL)
        {
            pPlayer->ReleaseBall();
        }

        if (IsOnSameTeam(pPlayer))
        {
            ((cFielder*)pPlayer)->EndDesire(false);
            ((cFielder*)pPlayer)->EndAction();
        }
        else
        {
            ((cFielder*)pPlayer)->InitActionSlideAttackReact(this, false);
        }
        return;

    default:
        return;
    }
}

/**
 * Offset/Address/Size: 0xA01C | 0x8004CB18 | size: 0x15C
 */
bool Goalie::PreCollideWithBallCallback(const dContact& contact)
{
    switch (mGoalieActionState)
    {
    case GOALIEACTION_STS:
        if (m_eAnimID == 0x6d)
        {
            return false;
        }
        if (mpSaveData != NULL && mpSaveData->muSaveType == 0x40000)
        {
            return false;
        }
        break;

    case GOALIEACTION_LOOSEBALL_PURSUE_ROLLING:
    {
        CleanGoalieAction();

        mPrevGoalieActionState = mGoalieActionState;
        mGoalieActionState = GOALIEACTION_LOOSEBALL_PICKUP;
        mnSubstate = 0;

        SetAnimState(mpLooseBallInfo->mnAnimID, true, 0.2f, false, false);
        InitMovementFromAnim(0, v3Zero, 1.0f, false);

        mMoveDirection = (eGoalieMoveDirection)0;
        mfTargetTime = 0.0f;
        mfWaitTime = -1.0f;
        mbPickedUp = false;

        const LooseBallInfo* pInfo = mpLooseBallInfo;
        f32 pickupDist = pInfo->mfPickupDistance;
        if (0.3f < pickupDist)
        {
            f32 pickupTime = pInfo->mfPickupTime;
            mfTargetTime = (pickupDist - 0.3f) * pickupTime / pickupDist;

            cPN_SAnimController* pAnim = m_pCurrentAnimController;
            f32 targetTime = mfTargetTime;
            pAnim->m_fPrevTime = pAnim->m_fTime;
            pAnim->m_fTime = targetTime;
        }

        InitiatePickup();
        return false;
    }

    case GOALIEACTION_LOOSEBALL_PICKUP:
        if (InitiatePickup())
        {
            return false;
        }
        break;

    case GOALIEACTION_MOVE:
    case GOALIEACTION_MOVE_WB:
    case GOALIEACTION_SAVE_SETUP:
    case GOALIEACTION_SAVE_REPOSITION:
    case GOALIEACTION_SAVE:
    case GOALIEACTION_MISS_CHIP_SHOT:
    case GOALIEACTION_DIVE_RECOVER:
    case GOALIEACTION_STS_SETUP:
    case GOALIEACTION_STS_RECOVER:
    case GOALIEACTION_STS_ATTACK_SETUP:
    case GOALIEACTION_STS_ATTACK:
    case GOALIEACTION_PASS:
    case GOALIEACTION_PASS_INTERCEPT:
    case GOALIEACTION_PRE_CROUCH:
    case GOALIEACTION_PURSUE_BALL_CARRIER:
    case GOALIEACTION_PURSUE_BALL_POUNCE:
    case GOALIEACTION_LOOSEBALL_SETUP:
    case GOALIEACTION_LOOSEBALL_CATCH:
    case GOALIEACTION_LOOSEBALL_PURSUE_BOUNCING:
    case GOALIEACTION_LOOSEBALL_DESPERATE:
    case GOALIEACTION_OFFPLAY:
    case GOALIEACTION_SNAP_BALL:
    case GOALIEACTION_GRAB_BALL:
        break;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x9D18 | 0x8004C814 | size: 0x304
 */
void Goalie::ExecutePounce(cPlayer* pPlayer, bool bCheckHitDistance)
{
    cFielder* pFldr = static_cast<cFielder*>(pPlayer);
    bool bDoHit = false;

    if (!pFldr->IsFallenDown(0.0f) && !pFldr->IsInvincible())
    {
        bDoHit = true;
    }

    if (bDoHit && bCheckHitDistance)
    {
        float fPlayerRadius;
        float fGoalieRadius;

        m_pPhysicsCharacter->GetRadius(&fGoalieRadius);
        pFldr->m_pPhysicsCharacter->GetRadius(&fPlayerRadius);

        float fDeltaX = m_v3Position.x - pFldr->m_v3Position.x;
        float fMinHitDistance = fGoalieRadius + fPlayerRadius + 0.5f;
        float fDeltaY = m_v3Position.y - pFldr->m_v3Position.y;

        bDoHit = nlGetLengthSquared2D(fDeltaX, fDeltaY) < nlGetLengthSquared1D(fMinHitDistance);
    }

    if (pFldr->m_eActionState == ACTION_SHOOT_TO_SCORE)
    {
        bDoHit = true;
    }

    bool bGetBall = false;
    if (pPlayer->m_pBall != NULL && g_pBall->m_v3Position.z < 1.0f)
    {
        Audio::SoundAttributes sndAtr;
        sndAtr.Init();
        sndAtr.SetSoundType(0xB7, true);
        sndAtr.UseStationaryPosVector(m_v3Position);
        sndAtr.mf_Volume = 0.4f;
        Audio::gStadGenSFX.Play(sndAtr);
        bGetBall = true;
    }

    if (bDoHit)
    {
        if (pFldr != NULL && pFldr->m_eClassType == FIELDER && !pFldr->IsFallenDown(0.0f))
        {
            pFldr->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);

            if (pFldr->m_pBall != NULL)
            {
                pFldr->ReleaseBall();
            }

            if (IsOnSameTeam(pFldr))
            {
                pFldr->EndDesire(false);
                pFldr->EndAction();
            }
            else
            {
                pFldr->InitActionSlideAttackReact(this, false);
            }
        }
    }
    else if (pFldr != NULL && pFldr->m_eClassType == FIELDER && pFldr->m_pBall != NULL)
    {
        pFldr->ReleaseBall();

        if (pFldr->m_eFielderDesireState != FIELDERDESIRE_FINISH_ACTION)
        {
            pFldr->EndDesire(false);
            pFldr->EndAction();
        }
    }

    if (bGetBall)
    {
        PickupBall(g_pBall);
        mbPickedUp = true;
        g_pBall->ClearShotInProgress();
        EmitGoalieCatch(this, "goalie_catch", false);

        if (mGoalieActionState != GOALIEACTION_PURSUE_BALL_POUNCE)
        {
            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_PURSUE_BALL_POUNCE;
            mnSubstate = 0;

            bool bShouldSetAnim = false;
            if (m_eAnimID != 0x33 || (bShouldSetAnim = (m_pCurrentAnimController->m_ePlayMode == 1 && m_pCurrentAnimController->m_fTime == 1.0f)))
            {
                SetAnimState(0x33, true, 0.2f, false, false);
            }

            InitMovementFromAnimSeek(m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff);
        }
    }
}

unsigned char Goalie::IsTargetViable(cPlayer* pTarget)
{
    if ((float)fabs(pTarget->m_v3Position.x) > (float)fabs(static_cast<cPlayer*>(this)->m_v3Position.x)
        && (float)fabs(pTarget->m_v3Position.y) < cField::GetPenaltyBoxY())
    {
        return false;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x99F0 | 0x8004C4EC | size: 0x328
 */
void Goalie::InitActionPass(bool useTarget)
{
    int animID;

    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_PASS;
    mnSubstate = 0;
    mpPassTarget = NULL;

    if (useTarget)
    {
        cPlayer* pPassTarget = FindOpenPassTarget();

        mpPassTarget = pPassTarget;

        if (mpPassTarget != NULL && IsTargetViable(mpPassTarget))
        {
            GoalieTweaks* pTweaks = static_cast<GoalieTweaks*>(m_pTweaks);

            float dy = m_v3Position.y - mpPassTarget->m_v3Position.y;
            float dx = m_v3Position.x - mpPassTarget->m_v3Position.x;
            float fDistanceSq = nlGetLengthSquared2D(dx, dy);
            float fKickDistanceSq = nlGetLengthSquared1D(pTweaks->fKickDistanceMin);
            float fOverhandThrowDistanceSq = nlGetLengthSquared1D(pTweaks->fOverhandThrowDistanceMin);
            float fOpenTo = OpenTo(this, mpPassTarget);

            if (GetGlobalPad() != NULL)
            {
                if (GetGlobalPad()->GetPressure(0x15, true) > 0.8f)
                {
                    animID = 2;
                }
                else if ((fDistanceSq > fOverhandThrowDistanceSq) || (fOpenTo < 0.85f))
                {
                    animID = 0;
                }
                else
                {
                    animID = 1;
                }
            }
            else if (fDistanceSq > fKickDistanceSq)
            {
                animID = 2;
            }
            else if ((fDistanceSq > fOverhandThrowDistanceSq) || (fOpenTo < 0.85f))
            {
                animID = 0;
            }
            else
            {
                animID = 1;
            }
        }
        else
        {
            mpPassTarget = NULL;
        }
    }

    if (mpPassTarget == NULL)
    {
        animID = 2;
    }

    SetAnimState(animID, true, 0.2f, false, false);
    InitMovementFromAnimSeek(m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff);
}

/**
 * Offset/Address/Size: 0x987C | 0x8004C378 | size: 0x174
 */
void Goalie::InitActionPassIntercept()
{
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_PASS_INTERCEPT;
    mnSubstate = 0;
    muBallDeflectCount = g_pBall->m_bBallDeflectCount;

    if (mfWaitTime <= 0.02f)
    {
        CleanGoalieAction();

        mPrevGoalieActionState = mGoalieActionState;
        mGoalieActionState = GOALIEACTION_SAVE;
        mnSubstate = 0;

        PlayBlendedAnims(mBlendInfo.mfStartTime, -1);

        m_pPhysicsCharacter->m_CanCollideWithBall = 1;

        mnOffplayPending = GOALIE_OFFPLAY_NONE;
        mbBallImpacted = false;

        MakeExertEvent();
    }
    else
    {
        mnSubstate = 4;
    }
}

/**
 * Offset/Address/Size: 0x9754 | 0x8004C250 | size: 0x128
 */
void Goalie::InitActionPassInterceptSave()
{
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_SAVE;
    mnSubstate = 0;

    PlayBlendedAnims(mBlendInfo.mfStartTime, -1);

    m_pPhysicsCharacter->m_CanCollideWithBall = 1;

    mnOffplayPending = GOALIE_OFFPLAY_NONE;
    mbBallImpacted = false;

    MakeExertEvent();
}

/**
 * Offset/Address/Size: 0x9684 | 0x8004C180 | size: 0xD0
 */
void Goalie::InitActionPreCrouch(eGoalieCrouchType crouchType)
{
    if (mGoalieActionState == GOALIEACTION_STS || mGoalieActionState == GOALIEACTION_STS_RECOVER)
    {
        return;
    }

    mCrouchType = crouchType;
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_PRE_CROUCH;
    mnSubstate = 0;

    do
    {
        if (m_eAnimID == 0x2e)
        {
            bool bShouldSetAnim = false;
            if (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)
            {
                bShouldSetAnim = true;
            }

            if (!bShouldSetAnim)
                break;
        }

        SetAnimState(0x2e, true, 0.2f, false, false);
    } while (false);

    InitMovementFromAnim(0, v3Zero, 0.0f, false);
}

/**
 * Offset/Address/Size: 0x95FC | 0x8004C0F8 | size: 0x88
 */
void Goalie::InitActionPursueBallCarrier()
{
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_PURSUE_BALL_CARRIER;
    mnSubstate = 0;

    mpLooseBallInfo = &LooseBallAnims::mTrapBallInfo;

    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    float random = nlRandomf(1.0f, &nlDefaultSeed);
    mbPlayMiss = (random < pSkillTweaks->fGoalieDekeChance);
}

/**
 * Offset/Address/Size: 0x957C | 0x8004C078 | size: 0x80
 */
void Goalie::InitActionPursueBallPounce()
{
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_PURSUE_BALL_POUNCE;
    mnSubstate = 0;

    SetAnimState(mpLooseBallInfo->mnAnimID, true, 0.2f, false, false);
    InitMovementFromAnimSeek(m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff);

    mbPickedUp = false;
}

/**
 * Offset/Address/Size: 0x94E8 | 0x8004BFE4 | size: 0x94
 */
void Goalie::InitActionPursueRecover()
{
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_DIVE_RECOVER;
    mnSubstate = 0;

    int animID = 0x8f;
    if (m_pBall == NULL)
    {
        animID = 0x8e;
    }

    SetAnimState(animID, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    mbPickedUp = false;
}

void Goalie::InitActionLooseBallPursueBouncing(const nlVector3& v3TargetPosition, float fTargetTime)
{
    CleanGoalieAction();
    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_LOOSEBALL_PURSUE_BOUNCING;
    mnSubstate = 0;

    mv3TargetPosition = v3TargetPosition;
    mfTargetTime = fTargetTime;
    mfTargetDist = 1.4f;

    m_aDesiredFacingDirection = (s16)(nlATan2f(v3TargetPosition.y - m_v3Position.y, v3TargetPosition.x - m_v3Position.x) * 10430.378f);

    s16 nAngDiff = m_aDesiredFacingDirection - m_aActualFacingDirection;
    s32 nAnimID = ChooseRunAnim(nAngDiff, v3TargetPosition, 1.0f);

    bool bShouldSetAnim = false;
    if (nAnimID != m_eAnimID || (bShouldSetAnim = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
    {
        SetAnimState(nAnimID, true, 0.2f, false, false);
    }
    InitMovementFromAnimSeek(((GoalieTweaks*)m_pTweaks)->fRunningDirectionSeekSpeed, ((GoalieTweaks*)m_pTweaks)->fRunningDirectionSeekFalloff);
}

/**
 * Offset/Address/Size: 0x8878 | 0x8004B374 | size: 0xC70
 */
void Goalie::DoNavigation(float fDeltaT, float fIdleDistance, Goalie::eNaviMode naviMode)
{
    u16 deltaToBall;
    cBall* pBall;
    s16 aGoalie2Ball;
    u16 absBallAngleDiff;
    uintptr_t nParam;   // PORT: holds a `this`
    int eAnimID;
    int nCurrentAnimID;
    int nFinalAnim;
    unsigned int aFinalDir;
    bool bNeedChange;
    u16 desiredAng;

    pBall = g_pBall;
    nCurrentAnimID = m_eAnimID;
    eAnimID = nCurrentAnimID;
    f32 fTime = m_pCurrentAnimController->m_fTime;

    GetLocalPoint(mv3LocalNavTarget, mv3NavTarget, m_v3Position, m_aActualFacingDirection);

    float distSq = mv3LocalNavTarget.x * mv3LocalNavTarget.x + mv3LocalNavTarget.y * mv3LocalNavTarget.y;

    if (naviMode == NAVI_FACE_BALL)
    {
        float dy = pBall->m_v3Position.y - m_v3Position.y;
        float dx = pBall->m_v3Position.x - m_v3Position.x;
        m_aDesiredFacingDirection = RadToAng16(nlATan2f(dy, dx));
    }

    aGoalie2Ball = (s16)(m_aDesiredFacingDirection - m_aActualFacingDirection);
    absBallAngleDiff = (u16)abs_s16(aGoalie2Ball);

    if (distSq < fIdleDistance * fIdleDistance)
    {
        int nNewAnim;
        if (absBallAngleDiff > 0x2AA8)
        {
            int idleTurns[2][2] = {
                { 0x10, 0x12 },
                { 0x0F, 0x11 },
            };
            int neg = (aGoalie2Ball <= 0) ? 1 : 0;
            int big = (absBallAngleDiff >= 0x5FFA) ? 1 : 0;
            nNewAnim = idleTurns[neg][big];
        }
        else
        {
            nNewAnim = 0x08;
        }

        bNeedChange = false;
        if (mnSubstate != 1 || (bNeedChange = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
        {
            bNeedChange = false;
            if (nNewAnim != m_eAnimID || (bNeedChange = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
            {
                SetAnimState(nNewAnim, true, 0.2f, false, false);
            }
        }

        if (nNewAnim == 0x08)
        {
            InitMovementFromAnim(0, v3Zero, 0.0f, false);
        }
        else
        {
            InitMovementFromAnimSeek(32768.0f, 2730.6667f);
        }

        if (mnSubstate != 1)
        {
            mnSubstate = 1;
        }
        mMoveDirection = GOALIEDIR_IDLE;
        return;
    }

    float runDistSq = mfGoalieRunDist * mfGoalieRunDist;
    float urgentDistSq = mfGoalieUrgentDist * mfGoalieUrgentDist;

    desiredAng = nlVector3ToAngle(mv3LocalNavTarget);

    if (mMoveDirection != GOALIEDIR_SIDE && (distSq > runDistSq || (mUrgency == URGENCY_HIGH && distSq > urgentDistSq)))
    {
        unsigned int aBaseDir;
        bool bDoBackward;
        bool bDoSeek;

        mnSubstate = 4;

        int ballDiff = (s16)(desiredAng - (u16)aGoalie2Ball);
        aBaseDir = (desiredAng + m_aActualFacingDirection) & 0xFFFF;
        aFinalDir = aBaseDir;
        deltaToBall = abs_ang16((s16)ballDiff);
        nFinalAnim = 0x26;

        if (naviMode == NAVI_FACE_DESIRED && absBallAngleDiff < 0x3FFC)
        {
            if (deltaToBall > 0x4E34)
            {
                aFinalDir += 0x8000;
                nFinalAnim = 0x27;
            }
            else if (deltaToBall > 0x31C4)
            {
                if (mv3LocalNavTarget.y > 0.0f)
                {
                    aFinalDir = aBaseDir - 0x4000;
                    nFinalAnim = 0x23;
                }
                else
                {
                    aFinalDir = aBaseDir + 0x4000;
                    nFinalAnim = 0x22;
                }
            }
        }
        else if (naviMode == NAVI_FACE_BALL)
        {
            bDoBackward = false;

            if (mMoveDirection == GOALIEDIR_IDLE || mMoveDirection == GOALIEDIR_BACKWARD)
            {
                float dy = pBall->m_v3Position.y - mv3NavTarget.y;
                float dx = pBall->m_v3Position.x - mv3NavTarget.x;
                float ballToNavDistSq = nlGetLengthSquared2D(dx, dy);

                if (distSq < ballToNavDistSq)
                {
                    int ballAng = RadToAng16(nlATan2f(dy, dx));
                    s16 diff = (s16)(aFinalDir + ballAng + 0x8000);
                    if ((u16)abs_s16(diff) < 0x1554)
                    {
                        bDoBackward = true;
                    }
                }
            }

            if (mMoveDirection == GOALIEDIR_BACKWARD || mMoveDirection == GOALIEDIR_FRONT2BACK)
            {
                if (deltaToBall > 0x3FFC)
                {
                    bDoBackward = true;
                }
            }
            else
            {
                if (deltaToBall > 0x5550)
                {
                    bDoBackward = true;
                }
            }

            if (bDoBackward)
            {
                if (!mbDoIntercept || (mMoveDirection != GOALIEDIR_FORWARD && mMoveDirection != GOALIEDIR_BACK2FRONT))
                {
                    aFinalDir += 0x8000;
                    nFinalAnim = 0x27;
                }
            }
        }

        s16 finalDiff = (s16)((u16)aFinalDir - m_aActualFacingDirection);
        u16 absFinalDiff = (u16)abs_s16(finalDiff);

        bDoSeek = true;

        if (nFinalAnim == 0x26)
        {
            switch (mMoveDirection)
            {
            case GOALIEDIR_IDLE:
                mMoveDirection = GOALIEDIR_FORWARD;
                if (absFinalDiff < 0x3552)
                {
                    m_aDesiredFacingDirection = aFinalDir;
                    eAnimID = 0x26;
                }
                else
                {
                    if (absFinalDiff > 0x6388)
                    {
                        eAnimID = 0x24;
                        if (aGoalie2Ball > 0)
                            eAnimID = 0x25;
                    }
                    else
                    {
                        eAnimID = 0x24;
                        if (finalDiff > 0)
                            eAnimID = 0x25;
                    }
                    m_aDesiredFacingDirection = aFinalDir;
                    mfSwitchTime = 0.5f;
                }
                break;

            case GOALIEDIR_FORWARD:
                m_aDesiredFacingDirection = aFinalDir;
                if (absFinalDiff < 0x3FFC)
                {
                    if ((nCurrentAnimID == 0x25 || nCurrentAnimID == 0x24) && fTime < mfSwitchTime)
                        break;
                    eAnimID = 0x26;
                }
                else
                {
                    if (absFinalDiff > 0x6388)
                    {
                        eAnimID = 0x24;
                        if (aGoalie2Ball > 0)
                            eAnimID = 0x25;
                    }
                    else
                    {
                        eAnimID = 0x24;
                        if (finalDiff > 0)
                            eAnimID = 0x25;
                    }
                    mfSwitchTime = 0.5f;
                }
                break;

            case GOALIEDIR_BACKWARD:
                mMoveDirection = GOALIEDIR_BACK2FRONT;
                m_aDesiredFacingDirection = (u16)((u16)aFinalDir + 0x8000);
                bDoSeek = false;
                if (absFinalDiff < 0x1FFE)
                {
                    eAnimID = 0x2D;
                    mfSwitchTime = 0.42857143f;
                }
                else
                {
                    eAnimID = 0x2A;
                    if (finalDiff < 0)
                        eAnimID = 0x2B;
                    mfSwitchTime = 0.5714286f;
                }
                break;

            case GOALIEDIR_SIDE:
                break;

            case GOALIEDIR_BACK2FRONT:
                bDoSeek = false;
                if (fTime > mfSwitchTime)
                    mMoveDirection = GOALIEDIR_FORWARD;
                break;

            case GOALIEDIR_FRONT2BACK:
                if (fTime > mfSwitchTime)
                    bDoSeek = true;
                else
                    bDoSeek = false;
                if (fTime < mfSwitchTime)
                    mMoveDirection = GOALIEDIR_FORWARD;
                else
                    mMoveDirection = GOALIEDIR_BACKWARD;
                break;
            }
        }
        else if (nFinalAnim == 0x27)
        {
            switch (mMoveDirection)
            {
            case GOALIEDIR_IDLE:
                mMoveDirection = GOALIEDIR_BACKWARD;
                if (absFinalDiff < 0x3FFC)
                {
                    m_aDesiredFacingDirection = aFinalDir;
                    eAnimID = 0x27;
                }
                else
                {
                    if (absFinalDiff < 0x6388)
                    {
                        eAnimID = 0x28;
                        if (finalDiff > 0)
                            eAnimID = 0x29;
                    }
                    else
                    {
                        eAnimID = 0x28;
                        if (aGoalie2Ball > 0)
                            eAnimID = 0x29;
                    }
                    mMoveDirection = GOALIEDIR_FRONT2BACK;
                    m_aDesiredFacingDirection = (u16)((u16)aFinalDir + 0x8000);
                    bDoSeek = false;
                    mfSwitchTime = 0.71428573f;
                }
                break;

            case GOALIEDIR_FORWARD:
                mMoveDirection = GOALIEDIR_FRONT2BACK;
                m_aDesiredFacingDirection = (u16)((u16)aFinalDir + 0x8000);
                bDoSeek = false;
                if (absFinalDiff < 0x1FFE)
                {
                    eAnimID = 0x2C;
                    mfSwitchTime = 0.5f;
                }
                else
                {
                    if (absFinalDiff < 0x6388)
                    {
                        eAnimID = 0x28;
                        if (finalDiff > 0)
                            eAnimID = 0x29;
                    }
                    else
                    {
                        eAnimID = 0x28;
                        if (aGoalie2Ball > 0)
                            eAnimID = 0x29;
                    }
                    mfSwitchTime = 0.71428573f;
                }
                break;

            case GOALIEDIR_BACKWARD:
                m_aDesiredFacingDirection = aFinalDir;
                eAnimID = 0x27;
                break;

            case GOALIEDIR_SIDE:
                break;

            case GOALIEDIR_BACK2FRONT:
                bDoSeek = false;
                if (fTime < mfSwitchTime)
                    mMoveDirection = GOALIEDIR_BACKWARD;
                else
                    mMoveDirection = GOALIEDIR_FORWARD;
                break;

            case GOALIEDIR_FRONT2BACK:
                if (fTime > mfSwitchTime)
                    bDoSeek = true;
                else
                    bDoSeek = false;
                if (fTime > mfSwitchTime)
                    mMoveDirection = GOALIEDIR_BACKWARD;
                break;
            }
        }
        else
        {
            eAnimID = nFinalAnim;
            mMoveDirection = GOALIEDIR_SIDE;
            m_aDesiredFacingDirection = aFinalDir;
        }

        bNeedChange = false;
        if (eAnimID != m_eAnimID || (bNeedChange = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
        {
            SetAnimState(eAnimID, true, 0.2f, false, false);
        }

        if (eAnimID == 0x27)
        {
            m_pCurrentAnimController->m_fPlaybackSpeedScale = ((GoalieTweaks*)m_pTweaks)->fSaveBackRunTimeScale;
        }

        if (bDoSeek)
        {
            InitMovementFromAnimSeek(m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff);
        }
        else
        {
            InitMovementFromAnim(0, v3Zero, 1.0f, false);
        }
        return;
    }

    u16 aNavDir = SeekDirection(m_aActualFacingDirection, m_aDesiredFacingDirection, m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff, fDeltaT);
    SetFacingDirection(aNavDir);

    GetLocalPoint(mv3LocalNavTarget, mv3NavTarget, m_v3Position, aNavDir);

    bNeedChange = false;
    int* nLeftAnims = 0;
    int* nRightAnims = 0;

    maLocalAngle = nlVector3ToAngle(mv3LocalNavTarget);

    static int gStepLAnims[3] = { 0x1D, 0x1C, 0x1E };
    static int gStepRAnims[3] = { 0x1A, 0x19, 0x1B };
    static int gStrafeLAnims[3] = { 0x16, 0x17, 0x18 };
    static int gStrafeRAnims[3] = { 0x13, 0x14, 0x15 };

    float strafeDistSq = mfGoalieStrafeDist * mfGoalieStrafeDist;

    if (distSq < strafeDistSq && mUrgency == URGENCY_LOW && mnSubstate != 3)
    {
        if (mnSubstate != 2)
        {
            mnSubstate = 2;
            bNeedChange = true;
            nLeftAnims = gStepLAnims;
            nRightAnims = gStepRAnims;
        }
    }
    else
    {
        bool bDoStrafe = true;
        if (mMoveDirection == GOALIEDIR_SIDE)
        {
            float distOldSq = nlGetLengthSquared2D(mv3NavTarget.x - m_v3PrevPosition.x, mv3NavTarget.y - m_v3PrevPosition.y);
            float distNewSq = nlGetLengthSquared2D(mv3NavTarget.x - m_v3Position.x, mv3NavTarget.y - m_v3Position.y);

            if (distNewSq <= distOldSq)
            {
                bool bAnimDone = false;
                if (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)
                {
                    bAnimDone = true;
                }
                if (!bAnimDone)
                {
                    bDoStrafe = false;
                }
            }
        }

        if (bDoStrafe && mnSubstate != 3)
        {
            mnSubstate = 3;
            bNeedChange = true;
            nLeftAnims = gStrafeLAnims;
            nRightAnims = gStrafeRAnims;
        }
    }

    if (mnSubstate == 3)
    {
        f32 fDist = nlSqrt(distSq, true);
        f32 fSpeedScale = InterpolateRangeClamped(0.25f, 2.0f, mfGoalieStepDist, 2.0f * mfGoalieStrafeDist, fDist);

        f32 fHighEnergyLimit = ((GoalieTweaks*)m_pTweaks)->fGetupEnergyHigh;

        if (mFatigue.mfEnergyLevel < fHighEnergyLimit && fSpeedScale > 0.8f)
        {
            f32 fNewScale = InterpolateRangeClamped(0.8f, fSpeedScale, ((GoalieTweaks*)m_pTweaks)->fGetupEnergyLow, fHighEnergyLimit, mFatigue.mfEnergyLevel);
            fSpeedScale = fNewScale;
        }

        mfSpeedScale = SeekSpeed(mfSpeedScale, fSpeedScale, 10.0f, 5.0f, fDeltaT);
    }

    if (!bNeedChange)
    {
        return;
    }

    mMoveDirection = GOALIEDIR_IDLE;

    cPN_SingleAxisBlender* pSAB_L = CreateSingleAxisBlender(nLeftAnims, 3, 1, MoveWeightCB, 0.1f, 0);
    cPN_SingleAxisBlender* pSAB_R = CreateSingleAxisBlender(nRightAnims, 3, 1, MoveWeightCB, 0.1f, 0);

    cPN_SingleAxisBlender* pSAB = pSAB_L;
    nParam = (uintptr_t)this;
    cPN_SAnimController* pPrevCtrlr = 0;
    int j;

    for (j = 0; j < 2; j++)
    {
        for (int i = 0; i < 3; i++)
        {
            cPN_SAnimController* pCtrlr = (cPN_SAnimController*)pSAB->GetChild(i);

            if (i != 1 || j > 0)
            {
                pCtrlr->m_bIgnoreTriggers = true;
            }

            if (mnSubstate == 3)
            {
                if (pPrevCtrlr == 0)
                {
                    pCtrlr->m_funcSychronizedWeightCallback = StrafeSynchronizedSpeedCallback;
                    pCtrlr->m_nSynchronizedWeightCallbackParam = (uintptr_t)this;
                    pCtrlr->m_fSynchronizedWeight = 0.0f;
                }
                else
                {
                    pCtrlr->m_bIsSynchronized = true;
                    pPrevCtrlr->m_pSynchronizedController = pCtrlr;
                }
                pPrevCtrlr = pCtrlr;
            }
        }
        pSAB = pSAB_R;
    }

    cPN_SingleAxisBlender* pDirBlender = ::new (AllocateSingleAxisBlender()) cPN_SingleAxisBlender(2, MoveDirectionCB, nParam, 0.1f);

    pDirBlender->SetChild(0, pSAB_L);
    pDirBlender->SetChild(1, pSAB_R);

    MoveWeightCB(nParam, pSAB_L);
    MoveWeightCB(nParam, pSAB_R);
    MoveDirectionCB(nParam, pDirBlender);

    cPN_Blender* pBlender = ::new (AllocateBlender()) cPN_Blender(*m_pAILayer, pDirBlender, 0.1f);

    *m_pAILayer = (cPoseNode*)pBlender;
    InitMovementFromAnim(0, v3Zero, 1.0f, true);
}

/**
 * Offset/Address/Size: 0x82F0 | 0x8004ADEC | size: 0x588
 */
void Goalie::FindDesiredGoaliePosition(nlVector3& pos, nlVector3& dir, nlVector3& focus, unsigned short& ang, const nlVector3* pThreatPos)
{
    cNet* pNet = m_pTeam->m_pNet;
    nlVector3 targetPos;
    float goalX;
    nlVector3 desiredVec;
    float goalieDist;
    float goalLine;
    float targetDist;
    float fNetY;
    nlVector3 desiredPos;
    float goalY = 0.0f;

    goalX = 0.5f * pNet->m_fDirection + pNet->GetGoalLineX();

    if (pThreatPos == NULL)
    {
        cBall* pBall = g_pBall;
        if (pBall->m_pOwner != NULL)
        {
            cPlayer* pOwner = pBall->m_pOwner;
            nlVec3ScaleAdd(targetPos, 0.18f, pOwner->m_v3Velocity, pBall->m_v3Position);
        }
        else
        {
            nlVec3ScaleAdd(targetPos, 0.18f, pBall->m_v3Velocity, pBall->m_v3Position);
        }
        targetPos.z = 0.0f;
    }
    else
    {
        targetPos = *pThreatPos;
    }

    ClampToGoalCone(targetPos, 3.0f);

    nlVec3Set(desiredVec, targetPos.x - goalX, targetPos.y - goalY, targetPos.z - goalY);

    goalLine = cField::GetGoalLineX(1U) - 0.5f;
    goalieDist = nlSqrt(nlVec3LengthSquared(desiredVec), true);
    targetDist = goalieDist - 0.5f;
    goalieDist = IsSoloBreakaway();

    if (goalieDist > 0.8f)
    {
        goalieDist = Interpolate(2.5f, 8.0f, 5.0000005f * (goalieDist - 0.8f));
        mUrgency = URGENCY_MED;
    }
    else if (targetDist > 23.0f)
    {
        goalieDist = 8.0f;
    }
    else if (targetDist > 19.0f)
    {
        float delta = targetDist - 19.0f;
        float scaled = delta * 5.5f;
        float quarter = 0.25f;
        goalieDist = scaled * quarter + 2.5f;
    }
    else if (targetDist > 12.0f)
    {
        goalieDist = 2.5f;
    }
    else if (targetDist > 7.0f)
    {
        goalieDist = 4.0f + (-1.5f * (targetDist - 7.0f) / 5.0f);
    }
    else
    {
        goalieDist = targetDist - 3.0f;
    }

    if (goalieDist > targetDist - 3.0f)
    {
        goalieDist = targetDist - 3.0f;
    }
    if (goalieDist < 0.5f)
    {
        goalieDist = 0.5f;
    }

    goalieDist += 0.5f;
    targetDist += 0.5f;
    float ratio = goalieDist / targetDist;
    nlVec3Set(desiredPos,
        ratio * desiredVec.x + goalX,
        ratio * desiredVec.y + goalY,
        ratio * desiredVec.z + goalY);

    if ((float)fabs(m_v3Position.x) > cField::GetGoalLineX(1U))
    {
        fNetY = 0.5f * cNet::m_fNetWidth - 1.0f;
        desiredPos.y = nlMaxEquals(desiredPos.y, -fNetY);
        desiredPos.y = nlMinEquals(desiredPos.y, fNetY);
        desiredPos.x = goalLine * pNet->m_fDirection;
        nlVec3Sub(desiredVec, desiredPos, m_v3Position);
    }

    ang = nlVector3ToAngle(desiredVec);

    desiredPos.x = nlMaxEquals(desiredPos.x, -goalLine);
    desiredPos.x = nlMinEquals(desiredPos.x, goalLine);

    const nlVector3& rPos = m_v3Position;
    nlVec3WeightedSum(pos, 0.8f, desiredPos, 0.2f, rPos);
    dir = desiredVec;
    focus = targetPos;
}

u16 Goalie::FindDumpDirection(u16 aDesired, bool bConstrain)
{
    if (bConstrain)
    {
        u16 aCur = GetActualFacing();
        s16 aDiff = (s16)(aDesired - aCur);
        s16 aAbsDiff = (aDiff < 0) ? -aDiff : aDiff;
        if ((u16)aAbsDiff > 0x1554)
        {
            if (aDiff > 0)
            {
                aDesired = aCur;
                aDesired += 0x1554;
            }
            else
            {
                aDesired = aCur;
                aDesired -= 0x1554;
            }
        }
    }

    if (m_v3Position.x < 0.0f)
    {
        aDesired += 0x8000;
    }

    if (aDesired < 0x5550)
    {
        aDesired = 0x5550;
    }
    else if (aDesired > 0xAAB0)
    {
        aDesired = (u16)-0x5550;
    }

    if (m_v3Position.x < 0.0f)
    {
        aDesired += 0x8000;
    }

    return aDesired;
}

cPlayer* Goalie::FindOpenPassTarget()
{
    cPlayer* pPassTarget;
    if (GetGlobalPad() != NULL)
    {
        pPassTarget = DoFindBestPassTarget(false, false);
    }
    else
    {
        FuzzyVariant vBestPassTarget = Fuzzy::GetBestPassTarget(this);
        if (vBestPassTarget.Confidence >= 0.5f)
        {
            pPassTarget = vBestPassTarget.mData.pPlayer;
        }
        else
        {
            pPassTarget = DoFindBestPassTarget(false, false);
        }
    }

    return pPassTarget;
}

/**
 * Offset/Address/Size: 0x8024 | 0x8004AB20 | size: 0x2CC
 */
bool Goalie::ShouldReposition()
{
    if (mfWaitTime < 0.07f)
    {
        return false;
    }

    bool bDesiredDirSet = false;

    if (!mbShouldMiss)
    {
        cBall* pBall = g_pBall;

        if (!pBall->m_unk_0xA6)
        {
            if ((0.3f + mBlendInfo.mv3BlendedSavePos.z) < mv3LocalContactPosition.z)
            {
                nlVector3 v3ContactVel;
                float fDropTime = FakeBallWorld::GetPredictedHeightLimitTime(2.0f, 0.04f, mv3NavTarget, v3ContactVel, true);
                float fGoalGapDist = cField::GetGoalLineX(1U) - 0.5f;
                float fBoxGapDist = 0.5f + cField::GetPenaltyBoxX(1U);
                float fTargetX;
                bool bCalcIntersect = false;
                float navAbsX = (float)fabs(mv3NavTarget.x);

                if (navAbsX > fGoalGapDist)
                {
                    if (m_v3Position.x > 0.0f)
                    {
                        fTargetX = fGoalGapDist;
                    }
                    else
                    {
                        fTargetX = -fGoalGapDist;
                    }

                    bCalcIntersect = true;
                }
                else if (navAbsX < fBoxGapDist)
                {
                    FakeBallWorld::GetPredictedHeightLimitTime(2.0f, 0.25f + fDropTime, mv3NavTarget, v3ContactVel, true);

                    if ((float)fabs(mv3NavTarget.x) < fBoxGapDist)
                    {
                        if (m_v3Position.x > 0.0f)
                        {
                            fTargetX = fBoxGapDist;
                        }
                        else
                        {
                            fTargetX = -fBoxGapDist;
                        }
                    }

                    bCalcIntersect = true;
                }

                if (bCalcIntersect)
                {
                    if ((float)fabs(v3ContactVel.x) > 0.5f)
                    {
                        float ballX = pBall->m_v3Position.x;
                        float ballY = pBall->m_v3Position.y;

                        mv3NavTarget.y = ballY + ((fTargetX - ballX) * (mv3NavTarget.y - ballY) / (mv3NavTarget.x - ballX));
                    }

                    mv3NavTarget.x = fTargetX;
                }

                float fTargetY = mv3NavTarget.y;
                fTargetY = fTargetY;
                fDropTime = mv3NavTarget.x;
                fTargetX = fTargetY - m_v3Position.y;
                fDropTime = (f32)(fDropTime - m_v3Position.x);

                m_aDesiredFacingDirection = RadToAng16(nlATan2f(pBall->m_v3Position.y - m_v3Position.y, pBall->m_v3Position.x - m_v3Position.x));

                if (nlGetLengthSquared2D(fDropTime, fTargetX) > 0.25f)
                {
                    mUrgency = URGENCY_MED;
                    return true;
                }

                bDesiredDirSet = true;
            }
        }
    }

    if ((float)fabs(mBlendInfo.mv3BlendedSavePos.y) > gfRepositionThreshold)
    {
        if (!bDesiredDirSet)
        {
            cBall* pBall = g_pBall;
            m_aDesiredFacingDirection = RadToAng16(nlATan2f(pBall->m_v3Position.y - m_v3Position.y, pBall->m_v3Position.x - m_v3Position.x));
        }

        GetWorldPoint(mv3NavTarget, mBlendInfo.mv3BlendedSavePos, m_v3Position, m_aDesiredFacingDirection);
        mv3NavTarget.z = 0.0f;

        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x7A48 | 0x8004A544 | size: 0x5DC
 */
void Goalie::HandleSTSContact(cBall* pBall)
{
    if (pBall->m_tShotTimer.m_uPackedTime == 0)
    {
        return;
    }
    if (pBall->m_pOwner != NULL)
    {
        return;
    }
    if (m_eAnimID == 0x6D)
    {
        return;
    }

    if (mpSaveData != NULL)
    {
        u32 saveType = mpSaveData->muSaveType;
        if (saveType == 0x40000 || (mbShouldMiss && (saveType & 0xFFFC) != 0))
        {
            return;
        }
    }

    Audio::SoundAttributes sndAtr;
    sndAtr.Init();
    sndAtr.SetSoundType(0xB7, true);
    sndAtr.UseStationaryPosVector(m_v3Position);
    Audio::gStadGenSFX.Play(sndAtr);

    if (mpSaveData != NULL)
    {
        cPlayer* pPrevOwner = g_pBall->m_pPrevOwner;
        if (pPrevOwner != NULL)
        {
            if (pPrevOwner->m_eClassType == 2)
            {
                BasicString<char, Detail::TempStringAllocator> effectName(
                    GetTeamName(nlSingleton<GameInfoManager>::Instance()->GetTeam((s16)pPrevOwner->m_pTeam->m_nSide)));
                effectName.AppendInPlace("_shoot_to_score_catch");
                EmitGoalieCatch(this, effectName.c_str(), true);
            }
        }

        u32 shotFlags = mpSaveData->muSaveType;
        if ((shotFlags & 0x20003) != 0)
        {
            if ((shotFlags & 3) != 0)
            {
                MakeSaveEvent(true);
            }

            PickupBall(pBall);

            cBaseCamera* pCamera = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
            if (pCamera->GetType() == eCameraType_MatrixEffect)
            {
                FireCameraRumbleFilter(0.0f, 0.2f);
            }

            if (m_eAnimID == 0x6C)
            {
                cNet* pNet = m_pTeam->m_pNet;
                if (pNet->m_v3NetLocation.x > 0.0f)
                {
                    Goalie::mbPosGoalieNetCheck = true;
                }
                else
                {
                    Goalie::mbNegGoalieNetCheck = true;
                }
            }
        }
        else if (mpSaveData->muSaveType == 0x10000)
        {
            MakeSaveEvent(true);

            EmitDaze(this);
            mbStunEffectActive = true;
        }
    }
    else
    {
        if (mpShooter != NULL)
        {
            MakeSaveEvent(true);

            PickupBall(pBall);

            FixedUpdateTask::mTimeScale = 0.75f;
            ParticleUpdateTask::SetTimeScale(0.75f);
        }
    }

    pBall->ClearShotInProgress();
}

static inline float GetGoaliePassDistance(float dx, float dy)
{
    return nlSqrt(dx * dx + dy * dy, true);
}

void Goalie::HandleSTSSwat()
{
    nlVector3 v3Target = mpShooter->m_v3Position;
    nlVector3 v3BallVelocity;

    float dx = m_v3Position.x - v3Target.x;
    float dy = m_v3Position.y - v3Target.y;
    float fDistance = GetGoaliePassDistance(dx, dy);
    float fSpeed = InterpolateRangeClamped(22.0f, 28.0f, 5.0f, 15.0f, fDistance);
    v3Target.z += 0.5f;

    ReleaseBall();
    g_pBall->ShootAtFast(v3BallVelocity, v3Target, fDistance / fSpeed);
    g_pBall->ShootRelease(v3BallVelocity, SPINTYPE_FORWARD);

    g_pBall->m_unk_0xA6 = true;
    g_pBall->m_tNoPickupTimer.SetSeconds(0.2f);
    g_pBall->mpDamageTarget = mpShooter;

    EmitBallShot(mpShooter, BALL_EFFECT_S2S_SHOT, NULL, false);
    g_pBall->InitiateBallBlur(BALL_EFFECT_S2S_SHOT, mpShooter);

    Audio::SoundAttributes sndAtr;
    sndAtr.Init();
    sndAtr.SetSoundType(0xBD, true);
    sndAtr.UsePhysObj(g_pBall->m_pPhysicsBall);
    Audio::gStadGenSFX.Play(sndAtr);

    SetNoPickUpTime(0.08f);
    Audio::FadeFilterFromCurrentToZero();
    FixedUpdateTask::mTimeScale = 1.0f;
    ParticleUpdateTask::SetTimeScale(1.0f);
}

/**
 * Offset/Address/Size: 0x75B4 | 0x8004A0B0 | size: 0x494
 */
bool Goalie::InitiatePickup()
{
    if (mpLooseBallInfo->mAnimType == LOOSEBALL_ANIM_KICK)
    {
        if (mbPickedUp)
        {
            return false;
        }

        if (m_pBall == NULL)
        {
            mbNoUserControl = true;

            cFielder* pFldr = g_pBall->GetOwnerFielder();
            if (pFldr != NULL)
            {
                if (IsOnSameTeam(pFldr))
                {
                    CleanGoalieAction();

                    mPrevGoalieActionState = mGoalieActionState;
                    mGoalieActionState = GOALIEACTION_MOVE;
                    mnSubstate = 0;

                    SetAnimState(8, true, 0.2f, false, false);
                    InitMovementFromAnim(0, v3Zero, 1.0f, false);

                    mnSubstate = 1;
                    mMoveDirection = GOALIEDIR_IDLE;

                    m_pPhysicsCharacter->m_CanCollideWithBall = true;
                    mbShouldMiss = false;
                    mbDoNavigate = false;

                    m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
                    m_pPhysicsCharacter->m_CanCollideWithWall = true;

                    CleanupStun();

                    mpShooter = NULL;
                    mUrgency = URGENCY_LOW;
                    mfSpeedScale = 1.0f;

                    mbPosGoalieNetCheck = false;
                    mbNegGoalieNetCheck = false;
                    mbDoHeadTrack = true;
                    mbBallImpacted = false;
                    mbNoUserControl = false;
                    mbPickedUp = false;

                    return false;
                }

                if (pFldr != NULL && pFldr->m_eClassType == FIELDER)
                {
                    if (!pFldr->IsFallenDown(0.0f))
                    {
                        pFldr->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);

                        if (pFldr->m_pBall != NULL)
                        {
                            pFldr->ReleaseBall();
                        }

                        if (IsOnSameTeam(pFldr))
                        {
                            pFldr->EndDesire(false);
                            pFldr->EndAction();
                        }
                        else
                        {
                            pFldr->InitActionSlideAttackReact(this, false);
                        }
                    }
                }
            }
            else
            {
                if (g_pBall->m_tNoPickupTimer.m_uPackedTime != 0)
                {
                    return false;
                }
            }

            nlVector3 v3Pos = g_pBall->m_v3Position;

            PickupBall(g_pBall);
            mbPickedUp = true;
            g_pBall->SetPosition(v3Pos);

            if (mpLooseBallInfo->mnAnimID == 5 || mpLooseBallInfo->mnAnimID == 4)
            {
                mpPassTarget = NULL;
            }
            else
            {
                cPlayer* pPassTarget = FindOpenPassTarget();
                mpPassTarget = pPassTarget;
            }

            if (mpPassTarget != NULL)
            {
                mbDoHeadTrack = true;
            }
            else
            {
                mbDoHeadTrack = false;
            }

            if (g_pBall->m_tShotTimer.m_uPackedTime != 0)
            {
                g_pBall->ClearShotInProgress();
            }

            return true;
        }
    }
    else
    {
        if (mfWaitTime <= 0.0f)
        {
            if (g_pBall->m_tNoPickupTimer.m_uPackedTime == 0)
            {
                mfWaitTime = 0.1f;
                SetNoPickUpTime(mfWaitTime);

                nlVector3 v3BallVel = g_pBall->m_v3Velocity;

                float fSpeedSq = nlVec3LengthSquared(v3BallVel);
                if (fSpeedSq > 64.0f)
                {
                    nlVec3Scale(v3BallVel, 0.5f);
                    g_pBall->SetVelocity(v3BallVel, SPINTYPE_NONE, NULL);
                }

                mbDoHeadTrack = false;

                if (g_pBall->m_tShotTimer.m_uPackedTime != 0)
                {
                    g_pBall->ClearShotInProgress();
                }

                return true;
            }
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x73E4 | 0x80049EE0 | size: 0x1D0
 */
void Goalie::InitiatePanicGrab(cPlayer* pPlayer)
{
    if (pPlayer != NULL)
    {
        if (IsOnSameTeam(pPlayer))
        {
            return;
        }

        cFielder* pFielder = static_cast<cFielder*>(pPlayer);

        if (!pFielder->IsFallenDown(0.0f) && !pFielder->IsInvincible() && pPlayer != NULL && pPlayer->m_eClassType == FIELDER && !pFielder->IsFallenDown(0.0f))
        {
            pPlayer->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);

            if (pPlayer->m_pBall != NULL)
            {
                pPlayer->ReleaseBall();
            }

            if (IsOnSameTeam(pFielder))
            {
                pFielder->EndDesire(false);
                pFielder->EndAction();
            }
            else
            {
                pFielder->InitActionSlideAttackReact(this, false);
            }
        }
    }

    s32 nAnimID = mpLooseBallInfo->mnAnimID;
    if (nAnimID != m_eAnimID)
    {
        do
        {
            if (nAnimID == m_eAnimID)
            {
                cPN_SAnimController* pController = m_pCurrentAnimController;
                u8 bShouldSetAnim = false;
                if (pController->m_ePlayMode == 1 && pController->m_fTime == 1.0f)
                {
                    bShouldSetAnim = true;
                }

                if (!bShouldSetAnim)
                {
                    break;
                }
            }

            SetAnimState(nAnimID, true, 0.2f, false, false);
        } while (0);

        cPN_SAnimController* pController = m_pCurrentAnimController;
        f32 fPickupTime = mpLooseBallInfo->mfPickupTime;
        pController->m_fPrevTime = pController->m_fTime;
        pController->m_fTime = 0.5f * fPickupTime;
        InitMovementFromAnim(0, v3Zero, 1.0f, false);
    }

    if (m_pBall == NULL)
    {
        PickupBall(g_pBall);
        mbPickedUp = true;
        g_pBall->ClearShotInProgress();
        EmitGoalieCatch(this, "goalie_catch", false);
    }
}

/**
 * Offset/Address/Size: 0x7328 | 0x80049E24 | size: 0xBC
 */
bool Goalie::IsCloseToPlane(const nlVector3& rPos1, const nlVector3& rPos2, float fThreshold)
{
    nlVector3 v3Dir;
    nlVector4 plane;

    nlVector3* pBallPos = &g_pBall->m_v3Position;

    v3Dir.x = pBallPos->y - rPos1.y;
    v3Dir.y = rPos1.x - pBallPos->x;
    v3Dir.z = 0.0f;

    MakePerpendicularPlane(*pBallPos, v3Dir, plane, 0.0f);

    float distance = (rPos2.x * plane.x) + (rPos2.y * plane.y) + (rPos2.z * plane.z) - plane.w;
    float absDistance = (float)fabsf(distance);

    return absDistance <= fThreshold;
}

/**
 * Offset/Address/Size: 0x7250 | 0x80049D4C | size: 0xD8
 */
bool Goalie::IsInsideGoalieBox(const nlVector3& rPos, float fXOffset, float fYOffset)
{
    const float x = rPos.x;
    if (((float)fabs(x) > (cField::GetPenaltyBoxX(1U) - fXOffset)) && ((x * m_v3Position.x) > 0.0f))
    {
        if ((float)fabs(rPos.y) < (fYOffset + cField::GetPenaltyBoxY()))
        {
            return true;
        }
    }

    return false;
}

bool Goalie::IsInsideNetArea(const nlVector3& v3Target)
{
    f32 fMargin = ((GoalieTweaks*)m_pTweaks)->fSaveIgnoreMargin;
    double fAbsTargetY;
    f32 fNetWidth;

    if ((float)fabsf(v3Target.x) > (cField::GetGoalLineX(1U) - 1.0f)
        && ((fNetWidth = cNet::m_fNetWidth), (fAbsTargetY = __fabs(v3Target.y)), (float)fAbsTargetY < (0.5f * fNetWidth + fMargin))
        && v3Target.z < (fMargin + cNet::m_fNetHeight))
    {
        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x6FE8 | 0x80049AE4 | size: 0x268
 */
float Goalie::CheckForDelflectAwayFromNet()
{
    if (muBallDeflectCount != g_pBall->m_bBallDeflectCount)
    {
        nlVector3 v3TargetPosition;
        nlVector4 plane;
        nlVector3 localVelocity;

        float netX = m_pTeam->m_pNet->m_v3NetLocation.x;

        if (netX < 0.0f)
        {
            plane.y = 0.0f;
            plane.x = 1.0f;
            plane.z = 0.0f;
            plane.w = -netX;
        }
        else
        {
            plane.y = 0.0f;
            plane.x = -1.0f;
            plane.z = 0.0f;
            plane.w = -netX;
        }

        float result = FakeBallWorld::GetPredictedPlaneIntersectTime(plane, v3TargetPosition, localVelocity);

        if (result <= 0.0f || !IsInsideNetArea(v3TargetPosition))
        {
            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_MOVE;
            mnSubstate = 0;

            SetAnimState(8, true, 0.2f, false, false);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);

            mnSubstate = 1;
            mMoveDirection = GOALIEDIR_IDLE;

            m_pPhysicsCharacter->m_CanCollideWithBall = true;
            mbShouldMiss = false;
            mbDoNavigate = false;
            m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
            m_pPhysicsCharacter->m_CanCollideWithWall = true;

            CleanupStun();

            mpShooter = NULL;
            mUrgency = URGENCY_LOW;
            mfSpeedScale = 1.0f;
            mbPosGoalieNetCheck = false;
            mbNegGoalieNetCheck = false;
            mbDoHeadTrack = true;
            mbBallImpacted = false;
            mbNoUserControl = false;
            mbPickedUp = false;

            result = -1.0f;
        }
        else
        {
            g_pBall->m_v3ShotTarget = v3TargetPosition;
        }

        return result;
    }

    return 0.0f;
}

/**
 * Offset/Address/Size: 0x6D80 | 0x8004987C | size: 0x268
 */
bool Goalie::CheckForLooseBallShotInProgress()
{
    cBall* pBall = g_pBall;
    if (pBall->m_pOwner == NULL)
    {
        cNet* pNet = m_pTeam->m_pNet;
        f32 looseBallShotDistance = ((GoalieTweaks*)m_pTweaks)->fLooseBallShotDistance;
        f32 dx = pBall->m_v3Position.x - pNet->m_v3NetLocation.x;
        f32 dy = pBall->m_v3Position.y - pNet->m_v3NetLocation.y;
        f32 distSq = nlGetLengthSquared2D(dx, dy);

        if (distSq < nlGetLengthSquared1D(looseBallShotDistance))
        {
            cTeam* pOtherTeam = m_pTeam->GetOtherTeam();
            cFielder* pShooter = NULL;
            f32 closestDistSq = 0.0f;

            for (s32 i = 0; i < 4; i++)
            {
                cFielder* pFielder = pOtherTeam->GetFielder(i);
                if (pFielder->m_eActionState == 7)
                {
                    f32 dx = pBall->m_v3Position.x - pFielder->m_v3Position.x;
                    f32 dy = pBall->m_v3Position.y - pFielder->m_v3Position.y;
                    f32 shooterDistSq = nlGetLengthSquared2D(dx, dy);

                    if (pShooter == NULL || shooterDistSq < closestDistSq)
                    {
                        pShooter = pFielder;
                        closestDistSq = shooterDistSq;
                    }
                }
            }

            mpShooter = pShooter;
            if (pShooter != NULL)
            {
                f32 dx = pBall->m_v3Position.x - m_v3Position.x;
                f32 dy = pBall->m_v3Position.y - m_v3Position.y;
                f32 goalieDistSq = nlGetLengthSquared2D(dx, dy);

                if (goalieDistSq > closestDistSq)
                {
                    nlVector4 plane;
                    nlVector3 v3Dir;
                    nlVector3* pBallPos = &g_pBall->m_v3Position;

                    v3Dir.x = pBallPos->y - mv3TargetPosition.y;
                    v3Dir.y = mv3TargetPosition.x - pBallPos->x;
                    v3Dir.z = 0.0f;

                    MakePerpendicularPlane(*pBallPos, v3Dir, plane, 0.0f);

                    float distance = (m_v3Position.x * plane.x) + (m_v3Position.y * plane.y) + (m_v3Position.z * plane.z) - plane.w;
                    float absDistance = (float)fabsf(distance);

                    if (absDistance <= 1.5f)
                    {
                        if (mGoalieActionState != GOALIEACTION_STS && mGoalieActionState != GOALIEACTION_STS_RECOVER)
                        {
                            mCrouchType = GOALIECROUCH_LOOSEBALL;
                            CleanGoalieAction();

                            mPrevGoalieActionState = mGoalieActionState;
                            mGoalieActionState = GOALIEACTION_PRE_CROUCH;
                            mnSubstate = 0;

                            bool bCurrentAnimFinished;

                            if (m_eAnimID != 0x2E || (bCurrentAnimFinished = (m_pCurrentAnimController->m_ePlayMode == 1 && m_pCurrentAnimController->m_fTime == 1.0f)))
                            {
                                SetAnimState(0x2E, true, 0.2f, false, false);
                            }

                            InitMovementFromAnim(0, v3Zero, 0.0f, false);
                        }

                        return true;
                    }

                    mUrgency = URGENCY_HIGH;
                }
            }
        }
    }

    return false;
}

bool Goalie::CheckForBallOnHead()
{
    if (g_pBall->m_tNoPickupTimer.m_uPackedTime == 0
        && g_pBall->m_pOwner == NULL
        && g_pBall->m_pPassTarget == NULL
        && g_pBall->m_v3Position.z > 0.8f)
    {
        InitActionSnapBall();
        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x69D8 | 0x800494D4 | size: 0x3A8
 */
bool Goalie::CheckForSTSAttack()
{
    cFielder* pFielder;
    bool bCanAttack;

    pFielder = g_pBall->GetOwnerFielder();

    if ((pFielder != NULL) && !IsOnSameTeam(pFielder) && (pFielder->m_eActionState == ACTION_SHOOT_TO_SCORE))
    {
        bCanAttack = true;
    }
    else
    {
        bCanAttack = false;
    }

    if (bCanAttack)
    {
        f32 fAnimScale;
        f32 ownerDistSq;
        f32 fCloseDistSq;
        f32 fMaxDistSq;
        f32 fCurrentAnimTime;
        bool bInNetZone;

        cFielder* pOppFielder = g_pBall->GetOwnerFielder();

        fAnimScale = pOppFielder->m_pCurrentAnimController->m_pSAnim->GetDuration();
        fCurrentAnimTime = fAnimScale * pOppFielder->m_pCurrentAnimController->m_fTime;
        f32 fTriggerTime = fAnimScale * GetCurrentAnimTriggerTime(pOppFielder, 0x85181B83, 0);

        const LooseBallInfo* pInfo = &LooseBallAnims::mAttackSTSInfo;
        f32 fPickupDuration = pInfo->mfPickupTime * pInfo->mfAnimDuration;
        if ((fCurrentAnimTime + fPickupDuration) < fTriggerTime)
        {
            do
            {
                ownerDistSq = nlGetLengthSquared2D(
                    pOppFielder->m_v3Position.x - m_v3Position.x,
                    pOppFielder->m_v3Position.y - m_v3Position.y);

                f32 fCloseDist = pInfo->mfPickupDistance + ((GoalieTweaks*)m_pTweaks)->fSTSAttackCloseDistance;
                f32 fMaxDist = pInfo->mfPickupDistance + ((GoalieTweaks*)m_pTweaks)->fSTSAttackMaxDistance;
                fCloseDistSq = nlGetLengthSquared1D(fCloseDist);
                fMaxDistSq = nlGetLengthSquared1D(fMaxDist);

                bInNetZone = IsInsideNetArea(pOppFielder->m_v3Position);

                nlVector3 v3GoalPos = m_pTeam->m_pNet->m_v3NetLocation;

                f32 halfWidth = 0.5f * cNet::m_fNetWidth;
                f32 clampedY = nlMaxEquals(pOppFielder->m_v3Position.y, -halfWidth);
                clampedY = nlMinEquals(clampedY, halfWidth);

                v3GoalPos.y = clampedY;

                f32 distSqFielder = nlGetLengthSquared2D(
                    v3GoalPos.x - pOppFielder->m_v3Position.x,
                    v3GoalPos.y - pOppFielder->m_v3Position.y);
                f32 distSqGoalie = v3GoalPos.CalculateDistanceSquared2D(m_v3Position);

                static FilteredRandomChance randgenSTS;

                if (!bInNetZone)
                {
                    if (distSqFielder < distSqGoalie)
                    {
                    }
                    else
                    {
                        if (ownerDistSq <= fCloseDistSq)
                        {
                        }
                        else if (ownerDistSq <= fMaxDistSq)
                        {
                            if (!randgenSTS.genrand(((GoalieTweaks*)m_pTweaks)->fSTSAttackChancePerFrame))
                            {
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                f32 fEndTime = fAnimScale * GetCurrentAnimTriggerTime(pOppFielder, 0x2C8DABFA, 0);
                f32 fStartTime = fEndTime - fCurrentAnimTime;
                f32 fPickupDuration2 = pInfo->mfPickupTime * pInfo->mfAnimDuration;
                InitActionSTSAttackSetup(fStartTime - fPickupDuration2);

                return true;
            } while (false);
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x659C | 0x80049098 | size: 0x43C
 */
bool Goalie::IsLooseBallClose(float fDistFromBox)
{
    bool bBallIsLoose = true;
    cBall* pBall = g_pBall;

    if (pBall->m_pPassTarget != NULL)
    {
        bool coneResult = IsLooseBallTowardNet();

        do
        {
            if (coneResult)
            {
                cPlayer* pPassTarget = g_pBall->m_pPassTarget;
                float dxToTarget = m_v3Position.x - pPassTarget->m_v3Position.x;
                float dyToTarget = m_v3Position.y - pPassTarget->m_v3Position.y;
                float dyToBall = m_v3Position.y - pBall->m_v3Position.y;
                float dxToBall = m_v3Position.x - pBall->m_v3Position.x;
                float distToPassTargetSq = nlGetLengthSquared2D(dxToTarget, dyToTarget);
                float distToBallSq = nlGetLengthSquared2D(dxToBall, dyToBall);
                if (distToBallSq > distToPassTargetSq)
                {
                }
                else
                {
                    break;
                }
            }
            bBallIsLoose = false;
        } while (false);
    }

    pBall = g_pBall;
    if ((pBall->m_pOwner == NULL) && bBallIsLoose)
    {
        float sideSign = m_pTeam->m_pNet->m_v3NetLocation.x;
        if (sideSign * pBall->m_v3Position.x < 0.0f)
        {
            return false;
        }

        float goalLineX = cField::GetGoalLineX(1U);
        float penaltyY = cField::GetPenaltyBoxY();
        float absBallX = (float)fabs(pBall->m_v3Position.x);
        float absBallY = (float)fabs(pBall->m_v3Position.y);

        if ((absBallX > (goalLineX - 2.0f)) && (absBallY < penaltyY))
        {
            return true;
        }
        if ((GonnaGetBall(m_pTeam) > 0.75f) && (absBallX < (goalLineX - 3.0f)) && (absBallY > penaltyY))
        {
            return false;
        }

        float ballAbsXNew = (float)fabs(pBall->m_v3Position.x);
        float ballPosX = pBall->m_v3Position.x;

        bool innerCheck;
        do
        {
            if (ballAbsXNew > cField::GetPenaltyBoxX(1U) - fDistFromBox)
            {
                if (ballPosX * m_v3Position.x > 0.0f)
                {
                    float ballAbsYNew = (float)fabs(pBall->m_v3Position.y);
                    if (ballAbsYNew < fDistFromBox + cField::GetPenaltyBoxY())
                    {
                        innerCheck = true;
                        break;
                    }
                }
            }
            innerCheck = false;
        } while (false);

        if (innerCheck)
        {
            return true;
        }

        float netSideSign = m_pTeam->m_pNet->m_fDirection;
        cBall* pBallVel = g_pBall;
        float unclampedXLimit = cField::GetPenaltyBoxX(1U) - fDistFromBox;
        float penaltyBoxXLimit = nlMaxEquals(0.0f, unclampedXLimit);
        float unclampedYLimit = penaltyY + fDistFromBox;
        float penaltyBoxYLimit = nlMaxEquals(0.0f, unclampedYLimit);

        if (absBallX < penaltyBoxXLimit)
        {
            float fForwardVelX = pBallVel->m_v3Velocity.x * netSideSign;
            if (fForwardVelX < 1.0f)
            {
                return false;
            }
            float fXtime = (penaltyBoxXLimit - absBallX) / fForwardVelX;
            if (fXtime > 0.3f)
            {
                return false;
            }
            float projectedY = fXtime * pBallVel->m_v3Velocity.y + pBall->m_v3Position.y;
            if ((float)fabs(projectedY) < penaltyBoxYLimit)
            {
                return true;
            }
        }

        if (absBallY > penaltyBoxYLimit)
        {
            float ballY = pBall->m_v3Position.y;
            float ySign;
            if (ballY > 0.0f)
            {
                ySign = 1.0f;
            }
            else
            {
                ySign = -1.0f;
            }
            float fForwardVelY = pBallVel->m_v3Velocity.y * ySign;
            if (fForwardVelY > -1.0f)
            {
                return false;
            }
            float fYtime = (penaltyBoxYLimit - absBallY) / fForwardVelY;
            if (fYtime > 0.3f)
            {
                return false;
            }
            float projectedX = fYtime * pBallVel->m_v3Velocity.x + pBall->m_v3Position.x;
            if (projectedX * netSideSign > penaltyBoxXLimit)
            {
                return true;
            }
        }
    }
    return false;
}

bool Goalie::IsLooseBallTowardNet()
{
    nlVector3 v3BallVel;
    nlVector3 v3Post1;
    nlVector3 v3Post2;

    if (g_pBall->m_pOwner != NULL)
    {
        return false;
    }

    v3BallVel = g_pBall->m_v3Velocity;
    if (nlVec3LengthSquared(v3BallVel) < 0.01f)
    {
        return false;
    }

    m_pTeam->m_pNet->GetPostLocation(v3Post1, 0, 0.0f);
    m_pTeam->m_pNet->GetPostLocation(v3Post2, 1, 0.0f);
    nlVec3Add(v3BallVel, v3BallVel, g_pBall->m_v3Position);
    return IsPointInCone(v3BallVel, g_pBall->m_v3Position, v3Post1, v3Post2);
}

static inline f32 DistSq(const nlVector3& a, const nlVector3& b)
{
    f32 deltaX = b.x;
    f32 deltaY = b.y;
    deltaY = a.y - deltaY;
    deltaX = a.x - deltaX;
    return nlGetLengthSquared2D(deltaX, deltaY);
}

/**
 * Offset/Address/Size: 0x64C0 | 0x80048FBC | size: 0xDC
 */
bool Goalie::IsWithinPounceRange()
{
    cFielder* pFielder = g_pBall->GetOwnerFielder();
    if ((pFielder != NULL) && !IsOnSameTeam(pFielder))
    {
        if (pFielder->m_eActionState == ACTION_SHOOT_TO_SCORE)
        {
            return false;
        }

        f32 range = LooseBallAnims::mTrapBallInfo.mfPickupDistance;
        range += ((GoalieTweaks*)m_pTweaks)->fPounceRange;
        f32 dy = m_v3Position.y - pFielder->m_v3Position.y;
        f32 dx = m_v3Position.x - pFielder->m_v3Position.x;
        range *= range;
        if ((range > nlGetLengthSquared2D(dx, dy))
            || (range > DistSq(m_v3Position, g_pBall->m_v3Position)))
        {
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x6388 | 0x80048E84 | size: 0x138
 */
bool Goalie::IsOpponentBallCarrierInRange()
{
    cFielder* pFielder = g_pBall->GetOwnerFielder();
    if ((pFielder != NULL) && !IsOnSameTeam(pFielder))
    {
        if (pFielder->m_eActionState == ACTION_SHOOT_TO_SCORE)
        {
            return false;
        }

        if (IsInsideGoalieBox(pFielder->m_v3Position, 0.0f, 0.0f))
        {
            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            f32 dy = m_v3Position.y - pFielder->m_v3Position.y;
            f32 dx = m_v3Position.x - pFielder->m_v3Position.x;
            f32 distSq = nlGetLengthSquared1D(pSkillTweaks->fAttackCarrierDistance);
            if (nlGetLengthSquared2D(dx, dy) < distSq)
            {
                return true;
            }
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x631C | 0x80048E18 | size: 0x6C
 */
bool Goalie::IsOpponentInSTS()
{
    cFielder* pFielder = g_pBall->GetOwnerFielder();
    if ((pFielder != NULL) && !IsOnSameTeam(pFielder) && (pFielder->m_eActionState == ACTION_SHOOT_TO_SCORE))
    {
        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x6218 | 0x80048D14 | size: 0x104
 */
bool Goalie::IsPassThreat()
{
    bool var_r0;

    cPlayer* pPassTarget = g_pBall->m_pPassTarget;
    if (pPassTarget != NULL)
    {
        if (!IsOnSameTeam(pPassTarget))
        {
            cBall* pBall = g_pBall;
            float absPassX = (float)fabs(pBall->m_v3PassIntercept.x);
            float ballPassX = pBall->m_v3PassIntercept.x;

            if ((absPassX > (cField::GetPenaltyBoxX(1U) - 1.0f))
                && (ballPassX * m_v3Position.x > 0.0f)
                && ((float)fabs(pBall->m_v3PassIntercept.y) < (cField::GetPenaltyBoxY() + 1.0f)))
            {
                var_r0 = true;
            }
            else
            {
                var_r0 = false;
            }

            if (var_r0)
            {
                mpPassTarget = pPassTarget;
                muBallDeflectCount = g_pBall->m_bBallDeflectCount;
                return true;
            }
        }
    }
    return false;
}

float Goalie::IsSoloBreakaway()
{
    float fScore = 0.0f;
    cFielder* pFldr = g_pBall->GetOwnerFielder();
    int i;
    cFielder* pBuddy;

    if (pFldr != NULL && !IsOnSameTeam(pFldr))
    {
        if (IsOpponentInSTS())
        {
            fScore = 0.0f;
        }
        else
        {
            fScore = OnBreakaway(pFldr);
            if (fScore > 0.8f)
            {
                cTeam* pTeam = pFldr->GetTeam();
                for (i = 0; i < 4; i++)
                {
                    pBuddy = pTeam->GetFielder(i);
                    if (pBuddy == pFldr)
                        continue;

                    cNet* pNet = m_pTeam->m_pNet;
                    const nlVector3& v3BuddyPos = pBuddy->m_v3Position;
                    float fAbsX = (float)fabs(v3BuddyPos.x);
                    if (pBuddy->m_v3Position.x * pNet->m_v3NetLocation.x > 0.0 && fAbsX > 2.0f)
                    {
                        float fFactor = 1.0f - 0.5f * (fAbsX - 2.0f) / 10.0f;
                        fFactor = nlMaxEquals(0.0f, fFactor);
                        fScore *= fFactor;
                    }
                }
            }
        }
    }

    return fScore;
}

void Goalie::MakeExertEvent()
{
    Event* pEvent = g_pEventManager->CreateValidEvent(0x13, 0x38);
    GoalieSaveData* pSaveData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) GoalieSaveData();

    pSaveData->pGoalie = this;
    pSaveData->v3BallVelocity = v3Zero;
    pSaveData->fWowFactor = 0.0f;
    pSaveData->isSTS = 0;

    pSaveData->saveType = g_pBall->m_uGoalType;
    pSaveData->pShooter = g_pBall->m_pShooter;

    if (mpSaveData != NULL)
    {
        pSaveData->padding = mpSaveData->muSaveType;
    }
    else
    {
        pSaveData->padding = 3;
    }
}

/**
 * Offset/Address/Size: 0x6104 | 0x80048C00 | size: 0x114
 */
void Goalie::MakeSaveEvent(bool bIsSTS)
{
    Event* pEvent = g_pEventManager->CreateValidEvent(0xF, 0x38);
    GoalieSaveData* pSaveData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) GoalieSaveData();

    pSaveData->pGoalie = this;
    pSaveData->v3BallVelocity = g_pBall->m_v3Velocity;

    GoalieTweaks* pTweaks = (GoalieTweaks*)m_pTweaks;
    pSaveData->fWowFactor = 1.0f / pTweaks->fShotFatigueMax;

    pSaveData->isSTS = bIsSTS;

    pSaveData->saveType = g_pBall->m_uGoalType;
    pSaveData->pShooter = g_pBall->m_pShooter;

    if (mpSaveData != NULL)
    {
        pSaveData->padding = mpSaveData->muSaveType;
        pSaveData->fWowFactor *= mpSaveData->mfFatigueValue;
    }
    else
    {
        pSaveData->padding = 3;
        pSaveData->fWowFactor *= ((GoalieTweaks*)m_pTweaks)->fShotFatigueDefault;
    }
}

/**
 * Offset/Address/Size: 0x5FF8 | 0x80048AF4 | size: 0x10C
 */
void Goalie::UpdateActionState(float fDeltaTime)
{
    switch (mGoalieActionState)
    {
    case GOALIEACTION_MOVE:
        ActionMove(fDeltaTime);
        break;
    case GOALIEACTION_MOVE_WB:
        ActionMoveWB(fDeltaTime);
        break;
    case GOALIEACTION_SAVE_SETUP:
        ActionSaveSetup(fDeltaTime);
        break;
    case GOALIEACTION_SAVE_REPOSITION:
        ActionSaveReposition(fDeltaTime);
        break;
    case GOALIEACTION_SAVE:
        ActionSave(fDeltaTime);
        break;
    case GOALIEACTION_MISS_CHIP_SHOT:
        ActionChipShotStumble(fDeltaTime);
        break;
    case GOALIEACTION_DIVE_RECOVER:
        ActionDiveRecover(fDeltaTime);
        break;
    case GOALIEACTION_STS_SETUP:
        ActionSTSSetup(fDeltaTime);
        break;
    case GOALIEACTION_STS:
        ActionSTS(fDeltaTime);
        break;
    case GOALIEACTION_STS_RECOVER:
        ActionSTSRecover(fDeltaTime);
        break;
    case GOALIEACTION_PASS:
        ActionPass(fDeltaTime);
        break;
    case GOALIEACTION_PASS_INTERCEPT:
        ActionPassIntercept(fDeltaTime);
        break;
    case GOALIEACTION_PRE_CROUCH:
        ActionPreCrouch(fDeltaTime);
        break;
    case GOALIEACTION_PURSUE_BALL_CARRIER:
        ActionPursueBallCarrier(fDeltaTime);
        break;
    case GOALIEACTION_PURSUE_BALL_POUNCE:
        ActionPursueBallPounce(fDeltaTime);
        break;
    case GOALIEACTION_LOOSEBALL_SETUP:
        ActionLooseBallSetup(fDeltaTime);
        break;
    case GOALIEACTION_LOOSEBALL_CATCH:
        ActionLooseBallCatch(fDeltaTime);
        break;
    case GOALIEACTION_LOOSEBALL_PICKUP:
        ActionLooseBallPickup(fDeltaTime);
        break;
    case GOALIEACTION_LOOSEBALL_PURSUE_BOUNCING:
        ActionLooseBallPursueBouncing(fDeltaTime);
        break;
    case GOALIEACTION_LOOSEBALL_PURSUE_ROLLING:
        ActionLooseBallPursueRolling(fDeltaTime);
        break;
    case GOALIEACTION_LOOSEBALL_DESPERATE:
        ActionLooseBallDesperate(fDeltaTime);
        break;
    case GOALIEACTION_OFFPLAY:
        ActionOffplay(fDeltaTime);
        break;
    case GOALIEACTION_SNAP_BALL:
        ActionSnapBall(fDeltaTime);
        break;
    case GOALIEACTION_GRAB_BALL:
        ActionGrabBall(fDeltaTime);
        break;
    case GOALIEACTION_STS_ATTACK_SETUP:
        ActionSTSAttackSetup(fDeltaTime);
        break;
    case GOALIEACTION_STS_ATTACK:
        ActionSTSAttack(fDeltaTime);
        break;
    }
}

/**
 * Offset/Address/Size: 0x5FA4 | 0x80048AA0 | size: 0x54
 */
void Goalie::SetGoalieAction(eGoalieActionState newGoalieState, int newSubstate)
{
    CleanGoalieAction();
    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = newGoalieState;
    mnSubstate = newSubstate;
}

/**
 * Offset/Address/Size: 0x5F40 | 0x80048A3C | size: 0x64
 */
void Goalie::SaveBlendCallback(uintptr_t nParam, cPN_SAnimController* pAnimCtrl)
{
    Goalie* pThis = reinterpret_cast<Goalie*>(nParam & ~(uintptr_t)3);
    unsigned int saveDataIndex = (unsigned int)(nParam & 3U);

    SaveData* pSaveData = pThis->mBlendInfo.mpSaveData[saveDataIndex];
    if (pSaveData == NULL)
    {
        return;
    }

    float fTime = pAnimCtrl->m_fTime;

    int milestoneIndex = 0;

    while (milestoneIndex < 4 && fTime >= pSaveData->mfMilestonePercent[milestoneIndex])
    {
        milestoneIndex++;
    }

    pAnimCtrl->m_fPlaybackSpeedScale = pThis->mBlendInfo.mfMilestoneScale[saveDataIndex][milestoneIndex];
}

static inline int GetAnimID(SaveBlendInfo& blend, int index)
{
    return blend.mpSaveData[index]->mnAnimID;
}

/**
 * Offset/Address/Size: 0x5D3C | 0x80048838 | size: 0x204
 */
cPoseNode* Goalie::SetupBlender(bool bPrimary, const float* fStartPercent, int nMainAnimID, int nMilestone)
{
    float fBlend;
    int index1;
    cPN_SAnimController* pSaveController1;
    int index2;

    if (bPrimary)
    {
        fBlend = mBlendInfo.mfSaveBlendPrimary;
        index1 = 0;
        index2 = 1;
    }
    else
    {
        fBlend = mBlendInfo.mfSaveBlendSecondary;
        index1 = 2;
        index2 = 3;
    }
    int animID = GetAnimID(mBlendInfo, index1);
    pSaveController1 = NewAnimController(animID, false, false, SaveBlendCallback, index1 + (uintptr_t)this);
    pSaveController1->m_fPlaybackSpeedScale = mBlendInfo.mfMilestoneScale[index1][nMilestone];
    if (fStartPercent[index1] > 0.0f)
    {
        pSaveController1->m_fPrevTime = pSaveController1->m_fTime;
        pSaveController1->m_fTime = fStartPercent[index1];
    }
    cPoseNode* result = pSaveController1;
    if (nMainAnimID == animID)
    {
        m_pCurrentAnimController = pSaveController1;
    }
    else
    {
        pSaveController1->m_bIgnoreTriggers = true;
    }
    if (fBlend >= 0.001f)
    {
        animID = GetAnimID(mBlendInfo, index2);
        cPN_SAnimController* pSaveController2 = NewAnimController(animID, false, false, SaveBlendCallback, index2 + (uintptr_t)this);
        pSaveController2->m_fPlaybackSpeedScale = mBlendInfo.mfMilestoneScale[index2][nMilestone];
        if (fStartPercent[index2] > 0.0f)
        {
            pSaveController2->m_fPrevTime = pSaveController2->m_fTime;
            pSaveController2->m_fTime = fStartPercent[index2];
        }
        if (nMainAnimID == animID)
        {
            m_pCurrentAnimController = pSaveController2;
        }
        else
        {
            pSaveController2->m_bIgnoreTriggers = true;
        }

        cPN_SingleAxisBlender* pPoseNode = ::new (AllocateSingleAxisBlender()) cPN_SingleAxisBlender(2, NULL, 0, 0.1f);
        pPoseNode->m_fDesiredWeight = fBlend;
        pPoseNode->m_fSmoothedWeight = fBlend;
        pPoseNode->SetChild(0, pSaveController1);
        pPoseNode->SetChild(1, pSaveController2);
        result = pPoseNode;
    }
    return result;
}

/**
 * Offset/Address/Size: 0x58E4 | 0x800483E0 | size: 0x458
 */
void Goalie::PlayBlendedAnims(float fStartTime, int nMilestone)
{
    static float fDefaultStartPercent[] = { 0.4f, 0.7f };

    cPoseNode* pMainNode;
    cPoseNode* pNode1;
    cPoseNode* pNode2;
    float fStartPercent[4];
    int nMainAnimID;
    int milestone;

    if (mBlendInfo.mfSaveBlendComposite <= 0.001f && mBlendInfo.mfSaveBlendPrimary <= 0.001f)
    {
        SetAnimState(mpSaveData->mnAnimID, true, 0.2f, false, false);

        if (nMilestone >= 0 && nMilestone < 2)
        {
            fStartTime = mBlendInfo.mfMilestoneTime[nMilestone];
            if (fStartTime <= 0.0f)
            {
                float* pDefaultStartPercent = fDefaultStartPercent;
                fStartTime = pDefaultStartPercent[nMilestone] * (mpSaveData->mfMilestonePercent[2] * mpSaveData->mfDuration);
            }
        }

        if (fStartTime > 0.0f && fStartTime < mpSaveData->mfDuration)
        {
            cPN_SAnimController* pController = m_pCurrentAnimController;
            f32 fAnimTime = fStartTime / mpSaveData->mfDuration;

            pController->m_fPrevTime = pController->m_fTime;
            pController->m_fTime = fAnimTime;
        }
    }
    else
    {
        nMainAnimID = mpSaveData->mnAnimID;

        if (nMilestone >= 0)
        {
            milestone = nMilestone;

            if (mBlendInfo.mfMilestoneTime[nMilestone] > 0.0f)
            {
                for (int i = 0; i < 4; i++)
                {
                    SaveData* pData = mBlendInfo.mpSaveData[i];
                    if (pData != NULL)
                    {
                        fStartPercent[i] = pData->mfMilestonePercent[nMilestone];
                    }
                }
            }
            else
            {
                float* pDefaultStartPercent = fDefaultStartPercent;

                SaveData* pData0 = mBlendInfo.mpSaveData[0];
                if (pData0 != NULL)
                {
                    float fDefaultStart = pDefaultStartPercent[nMilestone];
                    fStartPercent[0] = fDefaultStart * pData0->mfMilestonePercent[2];
                }
                SaveData* pData1 = mBlendInfo.mpSaveData[1];
                if (pData1 != NULL)
                {
                    float fDefaultStart = pDefaultStartPercent[nMilestone];
                    fStartPercent[1] = fDefaultStart * pData1->mfMilestonePercent[2];
                }
                SaveData* pData2 = mBlendInfo.mpSaveData[2];
                if (pData2 != NULL)
                {
                    float fDefaultStart = pDefaultStartPercent[nMilestone];
                    fStartPercent[2] = fDefaultStart * pData2->mfMilestonePercent[2];
                }
                SaveData* pData3 = mBlendInfo.mpSaveData[3];
                if (pData3 != NULL)
                {
                    float fDefaultStart = pDefaultStartPercent[nMilestone];
                    fStartPercent[3] = fDefaultStart * pData3->mfMilestonePercent[2];
                }
            }
        }
        else if (fStartTime > 0.0f)
        {
            float fPrevMilestone = 0.0f;

            milestone = 0;
            while (milestone < 4 && fStartTime >= mBlendInfo.mfMilestoneTime[milestone])
            {
                if (mBlendInfo.mfMilestoneTime[milestone] > 0.0f)
                {
                    fPrevMilestone = mBlendInfo.mfMilestoneTime[milestone];
                }
                milestone++;
            }

            fStartTime = NormalizeVal(fStartTime, fPrevMilestone, mBlendInfo.mfMilestoneTime[milestone]);

            int prevMilestone = milestone - 1;

            for (int i = 0; i < 4; i++)
            {
                SaveData* pData = mBlendInfo.mpSaveData[i];
                if (pData != NULL)
                {
                    float fStart = 0.0f;
                    if (milestone > 0)
                    {
                        fStart = pData->mfMilestonePercent[prevMilestone];
                    }

                    fStartPercent[i] = Interpolate(fStart, pData->mfMilestonePercent[(unsigned int)milestone], fStartTime);
                }
            }
        }
        else
        {
            fStartPercent[0] = 0.0f;
            fStartPercent[1] = 0.0f;
            fStartPercent[2] = 0.0f;
            fStartPercent[3] = 0.0f;
            milestone = 0;
        }

        pNode1 = SetupBlender(true, fStartPercent, nMainAnimID, milestone);
        pMainNode = pNode1;

        if (mBlendInfo.mfSaveBlendComposite >= 0.001f)
        {
            pNode2 = SetupBlender(false, fStartPercent, nMainAnimID, milestone);
            cPN_SingleAxisBlender* pBlend = ::new (AllocateSingleAxisBlender()) cPN_SingleAxisBlender(2, NULL, 0, 0.1f);

            pBlend->m_fDesiredWeight = mBlendInfo.mfSaveBlendComposite;
            pBlend->m_fSmoothedWeight = mBlendInfo.mfSaveBlendComposite;
            pBlend->SetChild(0, pNode1);
            pBlend->SetChild(1, pNode2);

            pMainNode = pBlend;
        }

        cPN_Blender* pBlender = ::new (AllocateBlender()) cPN_Blender(m_pAILayer[0], pMainNode, 0.1f);

        m_pAILayer[0] = pBlender;
        SetAnimID(nMainAnimID);
    }

    InitMovementFromAnim(0, v3Zero, 1.0f, true);
}

/**
 * Offset/Address/Size: 0x5878 | 0x80048374 | size: 0x6C
 */
void Goalie::PlayNewAnim(int nAnimID)
{
    if (nAnimID == m_eAnimID)
    {
        cPN_SAnimController* pController = m_pCurrentAnimController;
        bool bSkipSetAnimState = false;

        if (pController->m_ePlayMode == PM_HOLD && pController->m_fTime == 1.0f)
        {
            bSkipSetAnimState = true;
        }

        if (!bSkipSetAnimState)
        {
            return;
        }
    }

    SetAnimState(nAnimID, true, 0.2f, false, false);
}

/**
 * Offset/Address/Size: 0x5760 | 0x8004825C | size: 0x118
 */
void Goalie::CleanGoalieAction()
{
    switch (mGoalieActionState)
    {
    case GOALIEACTION_SAVE_REPOSITION:
        mbDoNavigate = false;
        break;

    case GOALIEACTION_SAVE:
        if (mpSaveData != NULL)
        {
            if (mpSaveData->mnRecoverAnimID < 0)
            {
                mpSaveData = NULL;
            }
        }
        muSaveType = 0xFFFF;
        mbShouldMiss = false;
        mpPassTarget = NULL;
        mbBallImpacted = false;
        break;

    case GOALIEACTION_DIVE_RECOVER:
        mpSaveData = NULL;
        break;

    case GOALIEACTION_STS:
        mbPosGoalieNetCheck = false;
        mbNegGoalieNetCheck = false;
        break;

    case GOALIEACTION_PRE_CROUCH:
        mpShooter = NULL;
        break;

    case GOALIEACTION_STS_RECOVER:
        CleanSTSRecover();
        break;

    case GOALIEACTION_PURSUE_BALL_POUNCE:
        mbPlayMiss = false;
        break;

    case GOALIEACTION_LOOSEBALL_PICKUP:
        mbPlayMiss = false;
        mbNoUserControl = false;
        mbPickedUp = false;
        break;

    case GOALIEACTION_OFFPLAY:
        mnOffplayPending = GOALIE_OFFPLAY_NONE;
        break;

    case GOALIEACTION_STS_ATTACK:
        mpShooter = NULL;
        break;

    case GOALIEACTION_MOVE:
    case GOALIEACTION_GRAB_BALL:
    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0x56D4 | 0x800481D0 | size: 0x8C
 */
void Goalie::InitActionLooseBallCatch()
{
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_LOOSEBALL_CATCH;
    mnSubstate = 0;

    mv3LocalContactPosition.x = 0.2f;

    mpSaveData = GoalieSave::FindBestSave(mBlendInfo, mv3LocalContactPosition, mfTargetTime, false, 1, true);

    mpLooseBallInfo = NULL;
    mMoveDirection = GOALIEDIR_IDLE;

    if (mpSaveData == NULL)
    {
        InitActionLooseBallSetup();
    }
}

void Goalie::InitActionLooseBallPickup(f32 fDistance, bool bStartPickup)
{
    CleanGoalieAction();
    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_LOOSEBALL_PICKUP;
    mnSubstate = 0;
    SetAnimState(mpLooseBallInfo->mnAnimID, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    mMoveDirection = GOALIEDIR_IDLE;
    mfTargetTime = 0.0f;

    f32 fWaitTime = -1.0f;
    if (bStartPickup)
    {
        fWaitTime = 0.5f;
    }
    mfWaitTime = fWaitTime;
    mbPickedUp = false;

    if (fDistance < mpLooseBallInfo->mfPickupDistance)
    {
        f32 fRemaining = mpLooseBallInfo->mfPickupDistance - fDistance;
        f32 fPickupTime = mpLooseBallInfo->mfPickupTime;
        mfTargetTime = fRemaining * fPickupTime / mpLooseBallInfo->mfPickupDistance;

        cPN_SAnimController* pController = m_pCurrentAnimController;
        f32 fNewAnimTime = mfTargetTime;
        pController->m_fPrevTime = pController->m_fTime;
        pController->m_fTime = fNewAnimTime;
    }
}

void Goalie::InitActionLooseBallPursueRolling()
{
    mv3NavTarget = mv3TargetPosition;
    if (mGoalieActionState != GOALIEACTION_LOOSEBALL_PURSUE_ROLLING)
    {
        CleanGoalieAction();
        mPrevGoalieActionState = mGoalieActionState;
        mGoalieActionState = GOALIEACTION_LOOSEBALL_PURSUE_ROLLING;
        mnSubstate = 0;
    }

    f32 fDy = mv3TargetPosition.y - m_v3Position.y;
    f32 fDx = mv3TargetPosition.x - m_v3Position.x;
    m_aDesiredFacingDirection = (s16)(nlATan2f(fDy, fDx) * 10430.378f);

    mv3NavTarget = mv3TargetPosition;
    mUrgency = URGENCY_MED;
}

void Goalie::ChooseDesperationAnim(f32 fFudgeDist)
{
    mpLooseBallInfo = LooseBallAnims::GetDesperationInfo(0);

    f32 fDy = m_v3Position.y - mv3TargetPosition.y;
    f32 fDx = m_v3Position.x - mv3TargetPosition.x;
    f32 fReachDist = mpLooseBallInfo->mfPickupDistance + fFudgeDist;
    f32 fDistSq = nlGetLengthSquared2D(fDx, fDy);
    f32 fReachDistSq = nlGetLengthSquared1D(fReachDist);

    if (fDistSq > fReachDistSq)
    {
        mpLooseBallInfo = LooseBallAnims::GetDesperationInfo(1);
        const nlVector3& rPos = m_v3Position;

        f32 fDy1 = rPos.y - mv3TargetPosition.y;
        f32 fDx1 = rPos.x - mv3TargetPosition.x;
        f32 fReachDist1 = mpLooseBallInfo->mfPickupDistance + fFudgeDist;
        f32 fDistSq1 = nlGetLengthSquared2D(fDx1, fDy1);
        f32 fReachDistSq1 = nlGetLengthSquared1D(fReachDist1);

        if (fDistSq1 < fReachDistSq1)
        {
            GetLocalPoint(mv3LocalContactPosition, mv3TargetPosition, rPos, m_aActualFacingDirection);

            f32 fSlope;
            if (mv3LocalContactPosition.x > 0.01f)
            {
                fSlope = mv3LocalContactPosition.y / mv3LocalContactPosition.x;
            }
            else if (mv3LocalContactPosition.y < 0.0f)
            {
                fSlope = -10.0f;
            }
            else
            {
                fSlope = 10.0f;
            }

            if ((f32)fabs(fSlope) > 1.5f)
            {
                mpLooseBallInfo = (fSlope < 0.0f) ? LooseBallAnims::GetDesperationInfo(2)
                                                  : LooseBallAnims::GetDesperationInfo(3);
            }
        }
    }
}

void Goalie::InitActionSaveReposition()
{
    mv3NavTarget = mv3TargetPosition;
    mMoveDirection = GOALIEDIR_IDLE;
    CleanGoalieAction();
    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_SAVE_REPOSITION;
    mnSubstate = 0;
    nlVector3 v3Delta;
    nlVec3Sub2D(v3Delta, m_v3Position, mv3NavTarget);
    mfTargetDist = nlGetLengthSquared2D(v3Delta.x, v3Delta.y);
    mUrgency = URGENCY_HIGH;
    f32 fBallDy = g_pBall->m_v3Position.y - m_v3Position.y;
    f32 fBallDx = g_pBall->m_v3Position.x - m_v3Position.x;
    m_aDesiredFacingDirection = (s16)(nlATan2f(fBallDy, fBallDx) * 10430.378f);
    DoNavigation(0.0f, gfRepositionThreshold, NAVI_FACE_DESIRED);
    if (mfWaitTime > 0.4f)
    {
        mUrgency = URGENCY_MED;
    }
    else
    {
        mUrgency = URGENCY_HIGH;
    }
}

/**
 * Offset/Address/Size: 0x3BD0 | 0x800466CC | size: 0x1B04
 */
void Goalie::InitActionLooseBallSetup()
{
    if (CheckForLooseBallShotInProgress())
    {
        return;
    }

    if (!IsLooseBallClose(SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fLooseBallChaseDistance))
    {
        InitActionMove(true);
        return;
    }

    if (g_pBall->m_uGoalType != 6)
    {
        g_pBall->m_uGoalType = 4;
    }

    nlVector3 v3BallPosition;
    f32 fBallDy;
    f32 fTargetDy;
    f32 fBallDz;
    f32 fTargetDx;
    f32 fTargetDz;
    f32 fNormDx;
    f32 fNormDy;
    f32 fNormDz;

    m_pPhysicsCharacter->m_CanCollideWithBall = true;
    mbDoHeadTrack = true;
    mbPickedUp = false;

    const nlVector3* pBallVelocity = &g_pBall->m_v3Velocity;

    v3BallPosition = g_pBall->m_v3Position;
    const nlVector3& v3NetBase = m_pTeam->m_pNet->m_v3NetLocation;
    muBallChangeCount = g_pBall->m_bBallPathChangeCount;
    muBallDeflectCount = g_pBall->m_bBallDeflectCount;

    bool bInCone = IsLooseBallTowardNet();

    f32 fBallSpeed = pBallVelocity->x * pBallVelocity->x + pBallVelocity->y * pBallVelocity->y
                   + pBallVelocity->z * pBallVelocity->z;

    f32 fAbsBallX;

    if (fBallSpeed > 100.0f && bInCone)
    {
        f32 fAbsGoalieX = (f32)fabs(m_v3Position.x);
        fAbsBallX = (f32)fabs(v3BallPosition.x);
        if (fAbsBallX < fAbsGoalieX - 1.5f)
        {
            f32 fTimeTilSave = CalcTimeToPlane();

            if (fTimeTilSave > 0.0f && fTimeTilSave < 2.0f)
            {
                double fAbsTargetY;
                fAbsTargetY = __fabs(mv3TargetPosition.y);
                if ((f32)fAbsTargetY > cField::GetPenaltyBoxY() - 1.0f)
                {
                    muSaveType = 0x0000FFFC;
                }
                else
                {
                    muSaveType = 0x0000FFFF;
                }

                mbShouldMiss = false;
                mfTimeTilSave = CalcSaveParameters(fTimeTilSave, muSaveType, false, false);

                if (mfTimeTilSave > 0.0f)
                {
                    mfWaitTime = mfTimeTilSave - mBlendInfo.mfMilestoneTime[2];
                    if (mfWaitTime <= 0.01f)
                    {
                        InitActionSave();
                        return;
                    }

                    if (ShouldReposition())
                    {
                        InitActionSaveReposition();
                        return;
                    }
                    else
                    {
                        CleanGoalieAction();
                        mPrevGoalieActionState = mGoalieActionState;
                        mGoalieActionState = GOALIEACTION_SAVE_SETUP;
                        mnSubstate = 0;
                        SetAnimState(10, true, 0.2f, false, false);
                        InitMovementFromAnimSeek(((GoalieTweaks*)m_pTweaks)->fSaveDirectionSeekSpeed, ((GoalieTweaks*)m_pTweaks)->fSaveDirectionSeekFalloff);
                        return;
                    }
                }
            }
        }
    }

    if (pBallVelocity->z < 3.0f && v3BallPosition.z < 1.5f)
    {
        if (bInCone && fBallSpeed > 0.25f)
        {
            if (IsInsideGoalieBox(v3BallPosition, -2.0f, -1.0f))
            {
                nlVector3 v3GuessBallPos;
                mpLooseBallInfo = LooseBallAnims::GetDesperationInfo(0);
                f32 fPanicLineX = cField::GetGoalLineX(1U) - 2.0f;

                nlVec3ScaleAdd(v3GuessBallPos, 0.8f * (mpLooseBallInfo->mfPickupTime * mpLooseBallInfo->mfAnimDuration), *pBallVelocity, v3BallPosition);
                f32 fAbsGuessX = (f32)fabs(v3GuessBallPos.x);

                if (fAbsGuessX > fPanicLineX)
                {
                    CleanGoalieAction();
                    mPrevGoalieActionState = mGoalieActionState;
                    mGoalieActionState = GOALIEACTION_LOOSEBALL_DESPERATE;
                    mnSubstate = 0;

                    if ((f32)fabs(v3BallPosition.x) >= fPanicLineX)
                    {
                        mv3TargetPosition = v3BallPosition;
                    }
                    else
                    {
                        f32 fGoalLineX2 = cField::GetGoalLineX(1U);
                        if (fAbsGuessX < fGoalLineX2)
                        {
                            mv3TargetPosition = v3GuessBallPos;
                        }
                        else
                        {
                            mv3TargetPosition.x = (v3NetBase.x > 0.0f) ? fPanicLineX : -fPanicLineX;
                            f32 fGuessY = v3GuessBallPos.y;
                            f32 fBallPosY = v3BallPosition.y;
                            f32 fBallPosX = v3BallPosition.x;
                            f32 fTargetX = mv3TargetPosition.x;
                            f32 fDiffY = fBallPosY - fGuessY;
                            f32 fGuessX = v3GuessBallPos.x;
                            f32 fDiffXTarget = fBallPosX - fTargetX;
                            f32 fDiffXOrig = fBallPosX - fGuessX;
                            mv3TargetPosition.y = fBallPosY - fDiffXTarget * fDiffY / fDiffXOrig;
                        }
                    }

                    mv3TargetPosition.z = 0.0f;
                    ChooseDesperationAnim(0.75f);

                    {
                        s32 nAnimID = mpLooseBallInfo->mnAnimID;
                        bool bShouldSetAnim = false;
                        if (nAnimID != m_eAnimID || (bShouldSetAnim = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
                        {
                            SetAnimState(nAnimID, true, 0.2f, false, false);
                        }
                        InitMovementFromAnim(0, v3Zero, 1.0f, false);
                        return;
                    }
                }
                else
                {
                    nlVector3 v3IntVel;
                    f32 fInterceptTime;
                    f32 fClosestDist;
                    bool bFound = FakeBallWorld::FindBallIntercept(m_v3Position, 1.0f, 6.0f, mv3TargetPosition, v3IntVel, fInterceptTime, fClosestDist, 3.0f);

                    if (bFound && mv3TargetPosition.z < 1.0f && fClosestDist < 0.75f)
                    {
                        CleanGoalieAction();
                        mPrevGoalieActionState = mGoalieActionState;
                        mGoalieActionState = GOALIEACTION_LOOSEBALL_DESPERATE;
                        mnSubstate = 0;

                        f32 fLimitX = cField::GetGoalLineX(1U) - 0.5f;

                        if ((f32)fabs(mv3TargetPosition.x) > fLimitX)
                        {
                            if ((f32)fabs(v3BallPosition.x) > fLimitX)
                            {
                                mv3TargetPosition = v3BallPosition;
                            }
                            else
                            {
                                mv3TargetPosition.y = v3BallPosition.y
                                                    - (v3BallPosition.x - fLimitX)
                                                          * (v3BallPosition.y - mv3TargetPosition.y)
                                                          / (v3BallPosition.x - mv3TargetPosition.x);

                                mv3TargetPosition.x = (v3NetBase.x > 0.0f) ? fLimitX : -fLimitX;
                            }
                        }

                        ChooseDesperationAnim(0.75f);

                        mfTargetTime = fInterceptTime - mpLooseBallInfo->mfPickupTime * mpLooseBallInfo->mfAnimDuration;

                        if (mfTargetTime < 0.02f)
                        {
                            s32 nAnimID = mpLooseBallInfo->mnAnimID;
                            bool bShouldSetAnim = false;
                            if (nAnimID != m_eAnimID || (bShouldSetAnim = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
                            {
                                SetAnimState(nAnimID, true, 0.2f, false, false);
                            }
                            InitMovementFromAnim(0, v3Zero, 1.0f, false);
                            return;
                        }
                        else
                        {
                            mv3NavTarget = mv3TargetPosition;
                            f32 fDyNav = v3BallPosition.y - m_v3Position.y;
                            f32 fDxNav = v3BallPosition.x - m_v3Position.x;
                            m_aDesiredFacingDirection = (s16)(nlATan2f(fDyNav, fDxNav) * 10430.378f);

                            if (mfTargetTime > 1.0f)
                            {
                                mUrgency = URGENCY_LOW;
                            }
                            else if (mfTargetTime > 0.5f)
                            {
                                mUrgency = URGENCY_MED;
                            }
                            else
                            {
                                mUrgency = URGENCY_HIGH;
                            }

                            DoNavigation(0.0f, 0.0f, NAVI_FOLLOW_TARGET);
                            return;
                        }
                    }
                }
            }
        }

        {
            bool bDoGrab = false;
            fAbsBallX = (f32)fabs(v3BallPosition.x);
            f32 fMinKickLine = 0.35f * (cField::GetGoalLineX(1U) - cField::GetPenaltyBoxX(1U)) + cField::GetPenaltyBoxX(1U);

            if (!IsLooseBallClose(0.0f))
            {
                cFielder* pOpponent = GetClosestOpponentFielder(&v3BallPosition);

                f32 fOppDistSq = nlGetLengthSquared2D(pOpponent->m_v3Position.x - v3BallPosition.x,
                    pOpponent->m_v3Position.y - v3BallPosition.y);
                f32 fGoalieDistSq = nlGetLengthSquared2D(m_v3Position.x - v3BallPosition.x,
                    m_v3Position.y - v3BallPosition.y);

                if (fGoalieDistSq > fOppDistSq)
                {
                    if (mGoalieActionState == GOALIEACTION_MOVE)
                    {
                        return;
                    }
                    InitActionMove(true);
                    return;
                }
            }
            else
            {
                if ((f32)fabs(v3BallPosition.x) > fAbsBallX - 2.0f || fAbsBallX > fMinKickLine)
                {
                    bDoGrab = true;
                }
                else
                {
                    cPlayer* pPassTarget = FindOpenPassTarget();

                    if (pPassTarget != NULL)
                    {
                        f32 fBallDx;
                        nlVector3 v3BallDelta;
                        nlVec3Sub(v3BallDelta, v3BallPosition, GetPosition());
                        fBallDy = v3BallPosition.y - GetPosition().y;
                        fBallDx = v3BallDelta.x;
                        fBallDz = v3BallDelta.z;
                        nlVector3 v3TargetDelta;
                        nlVec3Sub(v3TargetDelta, pPassTarget->GetPosition(), GetPosition());
                        fTargetDy = pPassTarget->GetPosition().y - GetPosition().y;
                        fTargetDx = v3TargetDelta.x;
                        fTargetDz = v3TargetDelta.z;
                        f32 fBallDist = nlSqrt(nlVec3LengthSquared(v3BallDelta), true);
                        f32 fInvDist = 1.0f / fBallDist;
                        fNormDz = fInvDist * fBallDz;
                        fNormDy = fInvDist * fBallDy;
                        fNormDx = fInvDist * fBallDx;
                        f32 fInvTargetDist = nlRecipSqrt(nlGetLengthSquared3D(fTargetDx, fTargetDy, fTargetDz), true);
                        f32 fNormTargetDx;
                        f32 fNormTargetDy;
                        f32 fNormTargetDz;
                        fNormTargetDz = fInvTargetDist * fTargetDz;
                        fNormTargetDy = fInvTargetDist * fTargetDy;
                        fNormTargetDx = fInvTargetDist * fTargetDx;

                        f32 fRightX;
                        f32 fRightY;
                        f32 fRightZ;
                        fRightZ = m_m4WorldMatrix.m13;
                        fRightY = m_m4WorldMatrix.m12;
                        fRightX = m_m4WorldMatrix.m11;

                        if (fBallDist < 1.2f)
                        {
                            bDoGrab = true;
                        }
                        else
                        {
                            f32 fDotBallTarget = fNormDx * fNormTargetDx + fNormDy * fNormTargetDy + fNormDz * fNormTargetDz;
                            if (fDotBallTarget < 0.7071f)
                            {
                                bDoGrab = true;
                            }
                            else
                            {
                                f32 fDotRight = fNormDx * fRightX + fNormDy * fRightY + fNormDz * fRightZ;
                                if (fDotRight < 0.0f)
                                {
                                    bDoGrab = true;
                                }
                                else
                                {
                                    cFielder* pOpp = GetClosestOpponentFielder(&v3BallPosition);
                                    f32 fOppDx = pOpp->m_v3Position.x - v3BallPosition.x;
                                    f32 fOppDy = pOpp->m_v3Position.y - v3BallPosition.y;
                                    if (nlGetLengthSquared2D(fOppDx, fOppDy) < fBallDist * fBallDist)
                                    {
                                        bDoGrab = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            nlVector3 v3Velocity;

            if (bDoGrab)
            {
                FakeBallWorld::GetPredictedBallPosition(LooseBallAnims::mpLooseBallInfo->mfPickupTime * LooseBallAnims::mpLooseBallInfo->mfAnimDuration,
                    mv3TargetPosition,
                    v3Velocity);

                GetLocalPoint(mv3LocalContactPosition, mv3TargetPosition, m_v3Position, m_aActualFacingDirection);

                nlVector3 v3CurLocalPos;
                GetLocalPoint(v3CurLocalPos, v3BallPosition, m_v3Position, m_aActualFacingDirection);

                mpLooseBallInfo = LooseBallAnims::FindLooseBallAnim(mv3LocalContactPosition, v3CurLocalPos.x >= 0.0f);
            }
            else
            {
                f32 fAbsGoalieX = (f32)fabs(m_v3Position.x);
                f32 fAbsBallXPos = (f32)fabs(v3BallPosition.x);
                f32 fAbsBallYDist = (f32)fabs(v3BallPosition.y - m_v3Position.y);
                f32 fDiffX = fAbsBallXPos - fAbsGoalieX;
                s16 nAngDiff = (s16)(nlATan2f(fAbsBallYDist, fDiffX) * 10430.378f);

                u16 nAbsAngle = nAngDiff;
                if (fAbsBallX < fMinKickLine && nAbsAngle > 0x4E34)
                {
                    mpLooseBallInfo = &LooseBallAnims::mLooseBallKickInfo[0];
                }
                else if (v3BallPosition.y * v3BallPosition.x > 0.0f)
                {
                    mpLooseBallInfo = &LooseBallAnims::mLooseBallKickInfo[1];
                }
                else
                {
                    mpLooseBallInfo = &LooseBallAnims::mLooseBallKickInfo[2];
                }
            }

            {
                f32 fInterceptTime;
                f32 fClosestDist;
                bool bFound = FakeBallWorld::FindBallIntercept(m_v3Position, 1.0f, 6.0f, mv3TargetPosition, v3Velocity, fInterceptTime, fClosestDist, 3.0f);

                if (bFound && mv3TargetPosition.z < 1.0f && fClosestDist < 0.4f)
                {
                    f32 fReachDist;
                    f32 fDx;
                    f32 fDy;
                    fDy = mv3TargetPosition.y - m_v3Position.y;
                    fDx = mv3TargetPosition.x - m_v3Position.x;
                    f32 fPickupDist = mpLooseBallInfo->mfPickupDistance;
                    fReachDist = 0.4f + fPickupDist;
                    f32 fDistSq = nlGetLengthSquared2D(fDx, fDy);
                    f32 fReachDistSq = nlGetLengthSquared1D(fReachDist);
                    f32 fAnimTime = mpLooseBallInfo->mfPickupTime * mpLooseBallInfo->mfAnimDuration;
                    f32 fTargetTime = fInterceptTime - fAnimTime;

                    if (fDistSq <= fReachDistSq && fTargetTime < 0.02f)
                    {
                        f32 fDist = nlSqrt(fDistSq, true);
                        InitActionLooseBallPickup(fDist, false);
                        return;
                    }
                    else
                    {
                        InitActionLooseBallPursueRolling();
                        return;
                    }
                }
                else
                {
                    if (mGoalieActionState == GOALIEACTION_MOVE)
                    {
                        return;
                    }
                    InitActionMove(true);
                    return;
                }
            }
        }

        return;
    }

    {
        int nNumSolutions;
        f32 pSolutions[2];

        CalcInterceptXY(m_v3Position, 0.85f * ((GoalieTweaks*)m_pTweaks)->fRunningSpeed, 0.5f, v3BallPosition, *pBallVelocity, nNumSolutions, pSolutions);

        if (nNumSolutions != 0)
        {
            f32 fBestTime;
            if (nNumSolutions == 2)
            {
                fBestTime = (pSolutions[0] < pSolutions[1]) ? pSolutions[0] : pSolutions[1];
            }
            else
            {
                fBestTime = pSolutions[0];
            }

            if (fBestTime < 5.0f)
            {
                nlVector3 v3IntPos;
                nlVector3 v3IntVel;
                f32 fHeightTime = FakeBallWorld::GetPredictedHeightLimitTime(3.0f, fBestTime, v3IntPos, v3IntVel, false);

                if (fHeightTime >= 0.0f)
                {
                    if (IsInsideGoalieBox(v3IntPos, 0.0f, 0.0f))
                    {
                        InitActionLooseBallPursueBouncing(v3IntPos, fHeightTime);
                        return;
                    }

                    if (mGoalieActionState == GOALIEACTION_MOVE)
                    {
                        return;
                    }
                    InitActionMove(true);
                    return;
                }
            }
        }

        CleanGoalieAction();
        mPrevGoalieActionState = mGoalieActionState;
        mGoalieActionState = GOALIEACTION_LOOSEBALL_SETUP;
        mnSubstate = 0;

        f32 fDyFace = v3BallPosition.y - m_v3Position.y;
        f32 fDxFace = v3BallPosition.x - m_v3Position.x;
        m_aDesiredFacingDirection = (s16)(nlATan2f(fDyFace, fDxFace) * 10430.378f);

        s16 nAngDiff = m_aDesiredFacingDirection - m_aActualFacingDirection;
        s32 nAnimID = ChooseRunAnim(nAngDiff, v3BallPosition, 1.0f);

        bool bShouldSetAnim = false;
        if (nAnimID != m_eAnimID || (bShouldSetAnim = (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)))
        {
            SetAnimState(nAnimID, true, 0.2f, false, false);
        }
        InitMovementFromAnimSeek(((GoalieTweaks*)m_pTweaks)->fRunningDirectionSeekSpeed, ((GoalieTweaks*)m_pTweaks)->fRunningDirectionSeekFalloff);
    }
}

/**
 * Offset/Address/Size: 0x3AA4 | 0x800465A0 | size: 0x12C
 */
void Goalie::InitActionMove(bool bParam)
{
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_MOVE;
    mnSubstate = 0;

    SetAnimState(8, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    mnSubstate = 1;
    mMoveDirection = GOALIEDIR_IDLE;

    m_pPhysicsCharacter->m_CanCollideWithBall = 1;
    mbShouldMiss = false;
    mbDoNavigate = false;
    m_pPhysicsCharacter->m_CanCollidedWithGoalLine = 1;
    m_pPhysicsCharacter->m_CanCollideWithWall = 1;

    CleanupStun();

    mpShooter = NULL;
    mUrgency = URGENCY_LOW;
    mfSpeedScale = 1.0f;
    mbPosGoalieNetCheck = false;
    mbNegGoalieNetCheck = false;
    mbDoHeadTrack = true;
    mbBallImpacted = false;
    mbNoUserControl = false;
    mbPickedUp = false;

    if (bParam)
    {
        ActionMove(0.0f);
    }
}

/**
 * Offset/Address/Size: 0x39F4 | 0x800464F0 | size: 0xB0
 */
void Goalie::InitActionMoveWB()
{
    if (m_pBall == NULL)
    {
        PickupBall(g_pBall);
    }

    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_MOVE_WB;
    mnSubstate = 0;

    SetAnimState(9, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    mfWaitTime = ((GoalieTweaks*)m_pTweaks)->fGoalieBallTime;
    mfTargetTime = 0.0f;
    mpPassTarget = NULL;
}

/**
 * Offset/Address/Size: 0x32E8 | 0x80045DE4 | size: 0x70C
 */
void Goalie::InitActionSaveSetup(bool bCanReposition)
{
    if (mGoalieActionState == GOALIEACTION_STS || mGoalieActionState == GOALIEACTION_STS_RECOVER)
    {
        return;
    }

    if ((mGoalieActionState == GOALIEACTION_PURSUE_BALL_POUNCE || mGoalieActionState == GOALIEACTION_LOOSEBALL_PICKUP)
        && m_pCurrentAnimController->m_fTime > (0.6f * mpLooseBallInfo->mfPickupTime))
    {
        return;
    }

    if (mGoalieActionState == GOALIEACTION_SAVE)
    {
        return;
    }

    muBallDeflectCount = g_pBall->m_bBallDeflectCount;
    mbDoHeadTrack = true;
    mbBallImpacted = false;
    mnOffplayPending = GOALIE_OFFPLAY_NONE;

    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_SAVE_SETUP;
    mnSubstate = 0;

    m_pPhysicsCharacter->m_CanCollideWithBall = true;

    float fTimeToContact = CalcTimeToPlane();

    if (fTimeToContact > 0.0f)
    {
        bool bFromTakeoff = false;

        if (mPrevGoalieActionState == GOALIEACTION_PRE_CROUCH || mPrevGoalieActionState == GOALIEACTION_PURSUE_BALL_CARRIER)
        {
            bFromTakeoff = true;
        }

        if (mbShouldMiss)
        {
            if (g_pBall->m_unk_0xA3)
            {
                float dY = m_v3Position.y - g_pBall->m_v3ShotTarget.y;
                float dX = m_v3Position.x - g_pBall->m_v3ShotTarget.x;

                if (nlGetLengthSquared2D(dX, dY) > 6.25f)
                {
                    static FilteredRandomChance randgenStumble;
                    if (randgenStumble.genrand(((GoalieTweaks*)m_pTweaks)->fLobShotStumbleChance))
                    {
                        InitActionChipShotStumble();
                        return;
                    }
                }
            }
        }
        else if (mUrgency == URGENCY_HIGH)
        {
            bFromTakeoff = true;
        }

        mfTimeTilSave = CalcSaveParameters(fTimeToContact, muSaveType, bFromTakeoff, false);

        if (mfTimeTilSave < 0.0f)
        {
            mfTimeTilSave = CalcSaveParameters(fTimeToContact, muSaveType, true, true);
        }

        float fMilestone2 = mBlendInfo.mfMilestoneTime[2];
        if (mbShouldMiss)
        {
            if (bFromTakeoff)
            {
                mBlendInfo.mfStartTime = mBlendInfo.mfMilestoneTime[0];
            }
            else
            {
                mBlendInfo.mfStartTime = 0.0f;
            }
        }
        else if (bFromTakeoff)
        {
            if ((fMilestone2 - mBlendInfo.mfMilestoneTime[0]) <= mfTimeTilSave)
            {
                mBlendInfo.mfStartTime = mBlendInfo.mfMilestoneTime[0];
            }
            else
            {
                float diff = fMilestone2 - mfTimeTilSave;
                float cap = mBlendInfo.mfMilestoneTime[1];
                float startTime;
                if (diff <= cap)
                {
                    startTime = diff;
                }
                else
                {
                    startTime = cap;
                }
                mBlendInfo.mfStartTime = startTime;
            }
        }
        else
        {
            if (fMilestone2 <= mfTimeTilSave)
            {
                mBlendInfo.mfStartTime = 0.0f;
            }
            else
            {
                float diff = fMilestone2 - mfTimeTilSave;
                float cap = mBlendInfo.mfMilestoneTime[1];
                float startTime;
                if (diff <= cap)
                {
                    startTime = diff;
                }
                else
                {
                    startTime = cap;
                }
                mBlendInfo.mfStartTime = startTime;
            }
        }

        mfWaitTime = mBlendInfo.mfStartTime + (mfTimeTilSave - fMilestone2);

        if (mfWaitTime <= 0.01f)
        {
            InitActionSave();
            return;
        }

        if (bCanReposition && ShouldReposition())
        {
            mMoveDirection = GOALIEDIR_IDLE;

            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_SAVE_REPOSITION;
            mnSubstate = 0;

            float dX = m_v3Position.x - mv3NavTarget.x;
            float dY = m_v3Position.y - mv3NavTarget.y;
            mfTargetDist = nlGetLengthSquared2D(dX, dY);

            mUrgency = URGENCY_HIGH;

            cBall* pBall = g_pBall;
            m_aDesiredFacingDirection = RadToAng16(nlATan2f(pBall->m_v3Position.y - m_v3Position.y, pBall->m_v3Position.x - m_v3Position.x));

            DoNavigation(0.0f, gfRepositionThreshold, NAVI_FACE_DESIRED);
            return;
        }

        SetAnimState(10, true, 0.2f, false, false);
        GoalieTweaks* pGoalieTweaks = (GoalieTweaks*)m_pTweaks;
        InitMovementFromAnimSeek(pGoalieTweaks->fSaveDirectionSeekSpeed, pGoalieTweaks->fSaveDirectionSeekFalloff);

        return;
    }

    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_MOVE;
    mnSubstate = 0;

    SetAnimState(8, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    mnSubstate = 1;
    mMoveDirection = GOALIEDIR_IDLE;

    m_pPhysicsCharacter->m_CanCollideWithBall = true;
    mbShouldMiss = false;
    mbDoNavigate = false;
    m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
    m_pPhysicsCharacter->m_CanCollideWithWall = true;

    CleanupStun();

    mpShooter = NULL;
    mUrgency = URGENCY_LOW;
    mfSpeedScale = 1.0f;
    mbPosGoalieNetCheck = false;
    mbNegGoalieNetCheck = false;
    mbDoHeadTrack = true;
    mbBallImpacted = false;
    mbNoUserControl = false;
    mbPickedUp = false;
}

/**
 * Offset/Address/Size: 0x2F9C | 0x80045A98 | size: 0x34C
 */
void Goalie::InitActionSave()
{
    float absX = (float)fabs(g_pBall->m_v3ShotTarget.x);
    if (absX > (cField::GetGoalLineX(1U) - 0.2f))
    {
        cBall* pBall = g_pBall;
        float saveIgnoreMargin = ((GoalieTweaks*)m_pTweaks)->fSaveIgnoreMargin;
        double shotAbsX = __fabs(pBall->m_v3ShotTarget.x);
        float netWidth;
        double shotAbsY;

        bool bInNet;
        if ((float)shotAbsX > (cField::GetGoalLineX(1U) - 1.0f)
            && ((netWidth = cNet::m_fNetWidth), (shotAbsY = __fabs(pBall->m_v3ShotTarget.y)), (float)shotAbsY < (0.5f * netWidth + saveIgnoreMargin))
            && pBall->m_v3ShotTarget.z < (saveIgnoreMargin + cNet::m_fNetHeight))
        {
            bInNet = true;
        }
        else
        {
            bInNet = false;
        }

        if (bInNet == false)
        {
            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_MOVE;
            mnSubstate = 0;

            SetAnimState(8, true, 0.2f, false, false);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);

            mnSubstate = 1;
            mMoveDirection = GOALIEDIR_IDLE;

            m_pPhysicsCharacter->m_CanCollideWithBall = true;
            mbShouldMiss = false;
            mbDoNavigate = false;
            m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
            m_pPhysicsCharacter->m_CanCollideWithWall = true;

            CleanupStun();

            mpShooter = NULL;
            mUrgency = URGENCY_LOW;
            mfSpeedScale = 1.0f;
            mbPosGoalieNetCheck = false;
            mbNegGoalieNetCheck = false;
            mbDoHeadTrack = true;
            mbBallImpacted = false;
            mbNoUserControl = false;
            mbPickedUp = false;

            return;
        }
    }

    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_SAVE;
    mnSubstate = 0;

    mFatigue.RegisterShot(mpSaveData->mfFatigueValue);
    mbBallImpacted = false;

    if (mbShouldMiss)
    {
        if (mpSaveData->mpFailAnimData)
        {
            mpSaveData = mpSaveData->mpFailAnimData;

            SetAnimState(mpSaveData->mnAnimID, true, 0.2f, false, false);
            InitMovementFromAnim(0, v3Zero, 0.0f, false);
        }
        else
        {
            PlayBlendedAnims(0.0f, -1);
        }
    }
    else
    {
        PlayBlendedAnims(mBlendInfo.mfStartTime, -1);
    }

    MakeExertEvent();
}

/**
 * Offset/Address/Size: 0x2D4C | 0x80045848 | size: 0x250
 */
void Goalie::InitActionSTSSetup()
{
    if (mGoalieActionState == GOALIEACTION_PURSUE_BALL_POUNCE || mGoalieActionState == GOALIEACTION_DIVE_RECOVER)
    {
        return;
    }

    mnOffplayPending = GOALIE_OFFPLAY_NONE;

    if (mGoalieActionState != GOALIEACTION_STS_RECOVER)
    {
        CleanGoalieAction();
        mPrevGoalieActionState = mGoalieActionState;
        mGoalieActionState = GOALIEACTION_STS_SETUP;
        mnSubstate = 0;
    }
    else
    {
        SaveData* pSavedSaveData = mpSaveData;
        CleanGoalieAction();
        mPrevGoalieActionState = mGoalieActionState;
        mGoalieActionState = GOALIEACTION_STS_SETUP;
        mnSubstate = 0;
        mpSaveData = pSavedSaveData;
    }

    SetDesiredSaveFacing(g_pBall->m_v3Position);

    nlVector4 plane;
    const u16 desiredFacingDirection = m_aDesiredFacingDirection;
    const nlVector3& pPosition = m_v3Position;

    MakePerpendicularPlane(pPosition, desiredFacingDirection, plane, 0.2f);

    nlVector3 localVelocity;
    float time = FakeBallWorld::GetPredictedPlaneIntersectTime(plane, mv3TargetPosition, localVelocity);
    double absX = __fabs(mv3TargetPosition.x);

    if ((float)absX > cField::GetGoalLineX(1U))
    {
        time = -1.0f;
    }
    else if (time > 0.0f)
    {
        GetLocalPoint(mv3LocalContactPosition, mv3TargetPosition, pPosition, desiredFacingDirection);
        GetLocalPoint(mv3LocalContactVelocity, localVelocity, v3Zero, desiredFacingDirection);
    }

    mfTimeTilSave = time;
    if (mfTimeTilSave < 0.0f)
    {
        mfTimeTilSave = 0.0f;
    }

    if (g_pBall->m_unk_0xA6 && mpLooseBallInfo != NULL)
    {
        mpShooter = static_cast<cFielder*>(g_pBall->m_pPrevOwner);
        mfWaitTime = mfTimeTilSave - (mpLooseBallInfo->mfPickupTime * mpLooseBallInfo->mfAnimDuration);
    }
    else
    {
        mpShooter = NULL;
        cBall* pBall = g_pBall;
        pBall->m_unk_0xA6 = false;
        pBall->mpDamageTarget = NULL;
        mfWaitTime = mfTimeTilSave - mBlendInfo.mfMilestoneTime[2];

        if (mbShouldMiss)
        {
            mfWaitTime += 0.11f;
        }
    }

    if (mfWaitTime <= 0.01f)
    {
        InitActionSTS();
        return;
    }

    SetAnimState(0xA, true, 0.2f, false, false);
    GoalieTweaks* pGoalieTweaks = static_cast<GoalieTweaks*>(m_pTweaks);
    InitMovementFromAnimSeek(pGoalieTweaks->fSaveDirectionSeekSpeed, pGoalieTweaks->fSaveDirectionSeekFalloff);
}

/**
 * Offset/Address/Size: 0x29A4 | 0x800454A0 | size: 0x3A8
 */
void Goalie::InitActionSTS()
{
    mbDoHeadTrack = false;
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_STS;
    mnSubstate = 0;

    cBall* pBall;
    if (mpShooter != NULL)
    {
        SetDesiredSaveFacing(g_pBall->m_v3Position);

        nlVector4 plane;
        const u16 desiredFacingDirection = m_aDesiredFacingDirection;
        const nlVector3& pPosition = m_v3Position;

        MakePerpendicularPlane(pPosition, desiredFacingDirection, plane, 0.2f);

        nlVector3 localVelocity;
        float time = FakeBallWorld::GetPredictedPlaneIntersectTime(plane, mv3TargetPosition, localVelocity);
        double absX = __fabs(mv3TargetPosition.x);

        if ((float)absX > cField::GetGoalLineX(1U))
        {
            time = -1.0f;
        }
        else if (time > 0.0f)
        {
            GetLocalPoint(mv3LocalContactPosition, mv3TargetPosition, pPosition, desiredFacingDirection);
            GetLocalPoint(mv3LocalContactVelocity, localVelocity, v3Zero, desiredFacingDirection);
        }

        mfTimeTilSave = time;

        SetAnimState(mpLooseBallInfo->mnAnimID, true, 0.2f, false, false);
        m_pCurrentAnimController->m_fPlaybackSpeedScale = (mpLooseBallInfo->mfPickupTime * mpLooseBallInfo->mfAnimDuration) / mfTimeTilSave;
    }
    else
    {
        if (mpSaveData->muSaveType == 0x20000)
        {
            m_pPhysicsCharacter->m_CanCollidedWithGoalLine = false;
            m_pPhysicsCharacter->m_CanCollideWithWall = false;

            PlayBlendedAnims(-mfWaitTime, -1);
            mFatigue.RegisterShot(mpSaveData->mfFatigueValue);

            pBall = g_pBall;
            float fFinalXPos = cField::GetGoalLineX(1U) + 1.5f;
            if (m_pTeam->m_pNet->m_v3NetLocation.x < 0.0f)
            {
                fFinalXPos = -fFinalXPos;
            }

            mv3NavTarget.x = fFinalXPos;

            float fNetYLimit = (0.5f * cNet::m_fNetWidth) - 1.3f;
            float fFinalYPos = m_v3Position.y
                             + (((m_v3Position.y - pBall->m_v3Position.y) * (fFinalXPos - m_v3Position.x))
                                 / (m_v3Position.x - pBall->m_v3Position.x));
            fFinalYPos = nlMaxEquals(fFinalYPos, -fNetYLimit);
            fFinalYPos = nlMinEquals(fFinalYPos, fNetYLimit);

            mv3NavTarget.y = fFinalYPos;
            mv3NavTarget.z = 0.0f;
            mfTargetTime = mpSaveData->mfMilestonePercent[2];
        }
        else if (mpSaveData->muSaveType & 0x50000)
        {
            PlayBlendedAnims(-mfWaitTime, -1);
            mFatigue.RegisterShot(mpSaveData->mfFatigueValue);
        }
        else
        {
            if (mbShouldMiss)
            {
                mfWaitTime = 0.0f;
            }

            if (mfWaitTime < -mBlendInfo.mfMilestoneTime[1])
            {
                mfWaitTime = -mBlendInfo.mfMilestoneTime[1];
            }

            PlayBlendedAnims(-mfWaitTime, -1);
            mFatigue.RegisterShot(((GoalieTweaks*)m_pTweaks)->fShotFatigueSTSSave);
        }
    }

    MakeExertEvent();
}

/**
 * Offset/Address/Size: 0x2824 | 0x80045320 | size: 0x180
 */
void Goalie::InitActionSTSRecover()
{
    if ((mpSaveData != NULL) && (mpSaveData->mnRecoverAnimID >= 0))
    {
        if (mpSaveData->muSaveType & 0x70000)
        {
            mbDoHeadTrack = false;
            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_STS_RECOVER;
            mnSubstate = 0;

            SetAnimState(mpSaveData->mnRecoverAnimID, true, 0.2f, false, false);

            if ((mpSaveData->muSaveType + 0xFFFF0000) == 0)
            {

                mfWaitTime = ((GoalieTweaks*)m_pTweaks)->fGoalieStunTimeMin;

                float timeRange = ((GoalieTweaks*)m_pTweaks)->fGoalieStunTimeMax - mfWaitTime;
                if (timeRange > 0.0f)
                {
                    static FilteredRandomReal randgenTimeRange;
                    float randomValue = timeRange * randgenTimeRange.genrand();
                    mfWaitTime += randomValue;
                }

                InitMovementFromAnim(0, v3Zero, 1.0f, false);
                g_pBall->m_uGoalType = 4;
            }
            return;
        }
        else
        {
            if ((mpSaveData->muSaveType & 0xFFFC) && (mnOffplayPending == GOALIE_OFFPLAY_NONE))
            {
                g_pBall->m_uGoalType = 4;
            }
        }
    }
    InitActionDiveRecover();
}

/**
 * Offset/Address/Size: 0x2600 | 0x800450FC | size: 0x224
 */
void Goalie::InitActionChipShotStumble()
{
    CleanGoalieAction();
    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_MISS_CHIP_SHOT;
    mnSubstate = 0;

    float dx = m_v3Position.x - g_pBall->m_v3Position.x;
    float dy = m_v3Position.y - g_pBall->m_v3Position.y;
    bool bFar = nlGetLengthSquared2D(dx, dy) > 42.25f;
    bool bContactLow;
    if (mv3LocalContactPosition.y > 0.0f)
        bContactLow = false;
    else
        bContactLow = true;
    mpSaveData = GoalieSave::GetMissChipSaveData(bContactLow, bFar);

    mpLooseBallInfo = NULL;
    SetAnimState(mpSaveData->mnAnimID, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    m_pPhysicsCharacter->m_CanCollidedWithGoalLine = false;

    cBall* pBall = g_pBall;
    nlVector3 v3Ball2Targ;
    nlVector3* const pV = &v3Ball2Targ;
    pV->y = pBall->m_v3ShotTarget.y - pBall->m_v3Position.y;
    pV->z = pBall->m_v3ShotTarget.z - pBall->m_v3Position.z;
    pV->x = pBall->m_v3ShotTarget.x - pBall->m_v3Position.x;
    float yy = pV->y * pV->y;
    float xx = pV->x * pV->x;
    float dist = nlSqrt(xx + yy, true);

    if (dist > 0.5f)
    {
        float scale = (1.5f + dist) / dist;
        nlVec3ScaleAdd(mv3NavTarget, scale, *pV, pBall->m_v3ShotTarget);
    }
    else
    {
        mv3NavTarget = pBall->m_v3ShotTarget;
        float pushX;
        if (mv3NavTarget.x > 0.0f)
            pushX = 1.5f;
        else
            pushX = -1.5f;
        mv3NavTarget.x += pushX;
    }

    float maxY = 0.5f * cNet::m_fNetWidth - 1.0f;
    float clampedY = nlMaxEquals(mv3NavTarget.y, -maxY);
    clampedY = nlMinEquals(clampedY, maxY);
    mv3NavTarget.y = clampedY;

    mv3NavTarget.z = 0.0f;
    mbDoHeadTrack = false;
}

/**
 * Offset/Address/Size: 0x2324 | 0x80044E20 | size: 0x2DC
 */
void Goalie::InitActionDiveRecover()
{
    if (mpSaveData != NULL && mpSaveData->mnRecoverAnimID >= 0)
    {
        mbDoHeadTrack = false;

        if (mnOffplayPending != GOALIE_OFFPLAY_NONE)
        {
            int randomValue = nlRandom(2, &nlDefaultSeed);
            CleanGoalieAction();
            mPrevGoalieActionState = mGoalieActionState;
            InitActionOffplay(GOALIE_OFFPLAY_NONE);

            int animID;
            if (m_pAnimInventory->GetMirrored(m_eAnimID))
            {
                animID = (randomValue == 0) ? 0x89 : 0x8B;
            }
            else
            {
                animID = (randomValue == 0) ? 0x88 : 0x8A;
            }

            SetAnimState(animID, true, 0.2f, false, false);
            mnOffplayPending = GOALIE_OFFPLAY_NONE;
        }
        else
        {
            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_DIVE_RECOVER;
            mnSubstate = 0;

            SetAnimState(mpSaveData->mnRecoverAnimID, true, 0.2f, false, false);
        }

        InitMovementFromAnim(0, v3Zero, 1.0f, false);
    }
    else
    {
        if (m_pBall == NULL)
        {
            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_MOVE;
            mnSubstate = 0;

            SetAnimState(8, true, 0.2f, false, false);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);

            mnSubstate = 1;
            mMoveDirection = GOALIEDIR_IDLE;

            m_pPhysicsCharacter->m_CanCollideWithBall = 1;
            mbShouldMiss = false;
            mbDoNavigate = false;
            m_pPhysicsCharacter->m_CanCollidedWithGoalLine = 1;
            m_pPhysicsCharacter->m_CanCollideWithWall = 1;

            CleanupStun();

            mpShooter = NULL;
            mUrgency = URGENCY_LOW;
            mfSpeedScale = 1.0f;
            mbPosGoalieNetCheck = false;
            mbNegGoalieNetCheck = false;
            mbDoHeadTrack = true;
            mbBallImpacted = false;
            mbNoUserControl = false;
            mbPickedUp = false;

            ActionMove(0.0f);
        }
        else
        {
            if (m_pBall == NULL)
            {
                PickupBall(g_pBall);
            }
            CleanGoalieAction();

            mPrevGoalieActionState = mGoalieActionState;
            mGoalieActionState = GOALIEACTION_MOVE_WB;
            mnSubstate = 0;

            SetAnimState(9, true, 0.2f, false, false);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);

            mfWaitTime = ((GoalieTweaks*)m_pTweaks)->fGoalieBallTime;
            mfTargetTime = 0.0f;
            mpPassTarget = NULL;
        }
    }

    mbPickedUp = false;
}

void Goalie::InitActionOffplay(eGoalieOffplayType type)
{
    mGoalieActionState = GOALIEACTION_OFFPLAY;
    mnSubstate = type;
}

void Goalie::InitActionSTSAttackSetup(f32 fWaitTime)
{
    mfWaitTime = fWaitTime;
    mfTargetTime = nlMaxEquals(fWaitTime, 0.25f);
    mpLooseBallInfo = &LooseBallAnims::mAttackSTSInfo;
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_STS_ATTACK_SETUP;
    mnSubstate = 0;
    mUrgency = URGENCY_LOW;
    ActionSTSAttackSetup(0.0f);
}

/**
 * Offset/Address/Size: 0x224C | 0x80044D48 | size: 0xD8
 */
void Goalie::InitActionSTSAttack()
{
    mbDoHeadTrack = false;
    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_STS_ATTACK;
    mnSubstate = 0;

    SetAnimState(mpLooseBallInfo->mnAnimID, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    cFielder* pFielder = g_pBall->GetOwnerFielder();
    float deltaX = pFielder->m_v3Position.x - m_v3Position.x;
    float deltaY = pFielder->m_v3Position.y - m_v3Position.y;
    float distance = nlSqrt((deltaX * deltaX) + (deltaY * deltaY), true);
    mfTargetDist = distance - mpLooseBallInfo->mfPickupDistance;
    mpShooter = pFielder;
}

void Goalie::InitActionSnapBall()
{
    CleanGoalieAction();
    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_SNAP_BALL;
    mnSubstate = 0;
    SetAnimState(0x7D, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);
    mfWaitTime = 0.1f;
    SetNoPickUpTime(mfWaitTime);
    mbDoHeadTrack = false;
}

/**
 * Offset/Address/Size: 0x214C | 0x80044C48 | size: 0x100
 */
bool Goalie::IsTeammateHoardingBall()
{
    cFielder* pOwner = g_pBall->GetOwnerFielder();
    if (pOwner != NULL && IsOnSameTeam(pOwner))
    {
        f32 myX = m_v3Position.x;
        f32 ownerX = pOwner->m_v3Position.x;
        volatile cFielder* pOwnerVolatile = pOwner;
        cBall* pBall = g_pBall;
        if (myX * ownerX > 0.0f)
        {
            f32 threshold = (f32)fabs(myX) - 0.5f;

            if ((f32)fabs(pOwnerVolatile->m_v3Position.x) > threshold || (f32)fabs(pBall->m_v3Position.x) > threshold)
            {
                f32 distThresh = 1.6899998f;

                if (nlGetLengthSquared2D(m_v3Position.x - pOwner->m_v3Position.x, m_v3Position.y - pOwner->m_v3Position.y) < distThresh
                    || m_v3Position.CalculateDistanceSquared2D(pBall->m_v3Position) < distThresh)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0x2084 | 0x80044B80 | size: 0xC8
 */
void Goalie::InitActionGrabBall()
{
    cFielder* pOwnerFielder = g_pBall->GetOwnerFielder();
    if (pOwnerFielder == NULL)
    {
        return;
    }

    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_GRAB_BALL;
    mnSubstate = 0;

    GetLocalPoint(mv3LocalContactPosition, g_pBall->m_v3Position, m_v3Position, m_aActualFacingDirection);

    mpLooseBallInfo = LooseBallAnims::FindLooseBallAnim(mv3LocalContactPosition, true);

    SetAnimState(mpLooseBallInfo->mnAnimID, true, 0.2f, false, false);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    mbDoHeadTrack = false;
    mbPickedUp = false;
}

/**
 * Offset/Address/Size: 0x1FA0 | 0x80044A9C | size: 0xE4
 */
unsigned short Goalie::CalcBestSave(float fTime, const nlVector3& rTargetPos, const nlVector3& rContactPos, unsigned int uSaveType, bool bParam)
{
    SetDesiredSaveFacing(rTargetPos);

    muSaveType = uSaveType;
    mpLooseBallInfo = NULL;

    GetLocalPoint(mv3LocalContactPosition, rContactPos, m_v3Position, m_aDesiredFacingDirection);

    if (uSaveType == 0 || fTime <= 0.0f)
    {
        mpSaveData = NULL;
        return m_aDesiredFacingDirection;
    }

    mpSaveData = GoalieSave::FindBestSave(mBlendInfo, mv3LocalContactPosition, fTime, bParam, uSaveType, false);

    if (mpSaveData != NULL)
    {
        mbPlayMiss = false;
    }

    return m_aDesiredFacingDirection;
}

/**
 * Offset/Address/Size: 0x1E68 | 0x80044964 | size: 0x138
 */
float Goalie::CalcSaveParameters(float fTimeToContact, unsigned int uSaveType, bool bFromTakeoff, bool bFindFailSave)
{
    float fTime = fTimeToContact;

    if (mbShouldMiss)
    {
        fTime += ((GoalieTweaks*)m_pTweaks)->fSaveMissDelay;
        mpSaveData = NULL;
    }
    else
    {
        mpSaveData = GoalieSave::FindBestSave(mBlendInfo, mv3LocalContactPosition, fTime, false, uSaveType, bFromTakeoff);
    }

    if (mpSaveData != NULL)
    {
        mbPlayMiss = false;
    }
    else
    {
        if (!mbShouldMiss && !bFindFailSave)
        {
            return -1.0f;
        }

        mpSaveData = GoalieSave::FindBestSave(mBlendInfo, mv3LocalContactPosition, 5.0f, true, uSaveType & 0xFFFC, false);
        mbPlayMiss = true;
    }

    const float fDT = (mpSaveData->mv3SavePos.x - mv3LocalContactPosition.x) / mv3LocalContactVelocity.x;

    fTime += fDT;

    nlVec3Set(mv3LocalContactPosition,
        (fDT * mv3LocalContactVelocity.x) + mv3LocalContactPosition.x,
        (fDT * mv3LocalContactVelocity.y) + mv3LocalContactPosition.y,
        (fDT * mv3LocalContactVelocity.z) + mv3LocalContactPosition.z);

    return fTime;
}

/**
 * Offset/Address/Size: 0x1D74 | 0x80044870 | size: 0xF4
 */
float Goalie::CalcTimeToPlane()
{
    nlVector3 localVelocity;
    nlVector4 plane;
    float time;

    SetDesiredSaveFacing(g_pBall->m_v3Position);

    unsigned short desiredFacing = GetDesiredFacing();
    const nlVector3& pos = GetPosition();
    MakePerpendicularPlane(pos, desiredFacing, plane, 0.2f);

    time = FakeBallWorld::GetPredictedPlaneIntersectTime(plane, mv3TargetPosition, localVelocity);

    if ((float)fabsf(mv3TargetPosition.x) > cField::GetGoalLineX(1U))
    {
        return -1.0f;
    }

    if (time > 0.0f)
    {
        GetLocalPoint(mv3LocalContactPosition, mv3TargetPosition, pos, desiredFacing);
        GetLocalPoint(mv3LocalContactVelocity, localVelocity, v3Zero, desiredFacing);
    }

    return time;
}

/**
 * Offset/Address/Size: 0x18C8 | 0x800443C4 | size: 0x4AC
 */
bool Goalie::CanInterceptPass()
{
    GoalieTweaks* pTweaks = (GoalieTweaks*)m_pTweaks;
    f32 fInterceptRangeSq = nlGetLengthSquared1D(pTweaks->fInterceptSaveTolerance);

    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
    if (pSkillTweaks->fGoalieCanInterceptPass < 0.5f)
    {
        return false;
    }

    do
    {
        if (mpPassTarget == NULL)
        {
            break;
        }
        if (IsOnSameTeam(mpPassTarget))
        {
            break;
        }

        cBall* pBall;
        u32 ballDeflectCount = (pBall = g_pBall)->m_bBallDeflectCount;
        if (muBallDeflectCount != ballDeflectCount)
        {
            break;
        }

        nlVector3& goaliePosition = m_v3Position;

        mpSaveData = NULL;

        f32 distTargetSq = nlGetLengthSquared2D(
            pBall->m_v3Position.x - mpPassTarget->m_v3Position.x,
            pBall->m_v3Position.y - mpPassTarget->m_v3Position.y);
        f32 distGoalieSq = nlGetLengthSquared2D(
            pBall->m_v3Position.x - m_v3Position.x,
            pBall->m_v3Position.y - m_v3Position.y);

        u16 saveAngle;
        u32 uSaveType;

        if (distGoalieSq < distTargetSq)
        {
            nlVector3 v3PassNorm;
            nlVec3Scale(v3PassNorm, g_pBall->m_v3Velocity, -1.0f);
            v3PassNorm.z = 0.0f;

            nlVector4 v4Plane;
            MakePerpendicularPlane(goaliePosition, v3PassNorm, v4Plane, 0.2f);

            nlVector3 contactVel;
            mfTimeTilSave = FakeBallWorld::GetPredictedPlaneIntersectTime(v4Plane, mv3TargetPosition, contactVel);

            saveAngle = (u16)(s32)(10430.378f * nlATan2f(v3PassNorm.y, v3PassNorm.x));

            uSaveType = 0xFFFF;

            f32 fAbsTargetX = (f32)fabs(mv3TargetPosition.x);
            f32 fTargetX = mv3TargetPosition.x;
            bool bInPenaltyBox;
            if (fAbsTargetX > cField::GetPenaltyBoxX(1) - (-2.0f)
                && fTargetX * m_v3Position.x > 0.0f
                && (f32)fabs(mv3TargetPosition.y) < (-2.0f) + cField::GetPenaltyBoxY())
            {
                bInPenaltyBox = true;
            }
            else
            {
                bInPenaltyBox = false;
            }

            if (!bInPenaltyBox)
            {
                uSaveType = 0xFFFC;
            }

            GetLocalPoint(mv3LocalContactPosition, mv3TargetPosition, m_v3Position, saveAngle);

            mpSaveData = GoalieSave::FindBestSave(mBlendInfo, mv3LocalContactPosition, mfTimeTilSave, false, uSaveType, false);

            if (mpSaveData != NULL)
            {
                if (nlGetLengthSquared3D(
                        mBlendInfo.mv3BlendedSavePos.x - mv3LocalContactPosition.x,
                        mBlendInfo.mv3BlendedSavePos.y - mv3LocalContactPosition.y,
                        mBlendInfo.mv3BlendedSavePos.z - mv3LocalContactPosition.z)
                    > fInterceptRangeSq)
                {
                    mpSaveData = NULL;
                }
            }
        }

        if (mpSaveData == NULL)
        {
            f32 fLeadTime = 2.0f * FixedUpdateTask::GetPhysicsUpdateTick();
            mfTimeTilSave = g_pBall->m_tPassTargetTimer.GetSeconds() - fLeadTime;

            if (mfTimeTilSave > 0.05f)
            {
                nlVector3 v3BallVel;
                FakeBallWorld::GetPredictedBallPosition(mfTimeTilSave, mv3TargetPosition, v3BallVel);

                f32 dx = mv3TargetPosition.x - m_v3Position.x;
                f32 dy = m_v3Position.y - mv3TargetPosition.y;
                f32 dz = 0.0f;
                f32 dot = dy * g_pBall->m_v3Velocity.x + dx * g_pBall->m_v3Velocity.y + dz * g_pBall->m_v3Velocity.z;

                if (dot > 0.0f)
                {
                    dy = -1.0f * dy;
                    dx = -1.0f * dx;
                }

                saveAngle = RadToAng16(nlATan2f(dx, dy));

                uSaveType = 0xFFFF;

                f32 fAbsTargetX = (f32)fabs(mv3TargetPosition.x);
                f32 fTargetX = mv3TargetPosition.x;
                bool bInPenaltyBox;
                if (fAbsTargetX > cField::GetPenaltyBoxX(1) - (-2.0f)
                    && fTargetX * m_v3Position.x > 0.0f
                    && (f32)fabs(mv3TargetPosition.y) < (-2.0f) + cField::GetPenaltyBoxY())
                {
                    bInPenaltyBox = true;
                }
                else
                {
                    bInPenaltyBox = false;
                }

                if (!bInPenaltyBox)
                {
                    uSaveType = 0xFFFC;
                }

                GetLocalPoint(mv3LocalContactPosition, mv3TargetPosition, m_v3Position, saveAngle);

                mpSaveData = GoalieSave::FindBestSave(mBlendInfo, mv3LocalContactPosition, mfTimeTilSave, false, uSaveType, false);

                if (mpSaveData != NULL)
                {
                    if (nlGetLengthSquared3D(
                            mBlendInfo.mv3BlendedSavePos.x - mv3LocalContactPosition.x,
                            mBlendInfo.mv3BlendedSavePos.y - mv3LocalContactPosition.y,
                            mBlendInfo.mv3BlendedSavePos.z - mv3LocalContactPosition.z)
                        > fInterceptRangeSq)
                    {
                        mpSaveData = NULL;
                    }
                }
            }
        }

        if (mpSaveData != NULL)
        {
            f32 milestone2 = mBlendInfo.mfMilestoneTime[2];
            if (milestone2 <= mfTimeTilSave)
            {
                mBlendInfo.mfStartTime = 0.0f;
                mfWaitTime = mfTimeTilSave - milestone2;
            }
            else
            {
                f32 diff = milestone2 - mfTimeTilSave;
                f32 cap = mBlendInfo.mfMilestoneTime[1];
                f32 startTime;
                if (diff <= cap)
                {
                    startTime = diff;
                }
                else
                {
                    startTime = cap;
                }
                mBlendInfo.mfStartTime = startTime;
                mfWaitTime = 0.0f;
            }
            m_aDesiredFacingDirection = saveAngle;
            return true;
        }
    } while (0);

    return false;
}

/**
 * Offset/Address/Size: 0x1810 | 0x8004430C | size: 0xB8
 */
int Goalie::ChooseRunAnim(short nAngle, const nlVector3& rTargetPos, float fThreshold)
{
    int nCurrentAnimID = m_eAnimID;
    unsigned short nAbsAngle;
    nlVector3 v3Delta;
    nlVec3Sub2D(v3Delta, rTargetPos, m_v3Position);

    if (nlGetLengthSquared2D(v3Delta.x, v3Delta.y) < nlGetLengthSquared1D(fThreshold))
    {
        mMoveDirection = GOALIEDIR_IDLE;
        return 8;
    }

    nAbsAngle = (u16)abs_s16(nAngle);

    mMoveDirection = GOALIEDIR_FORWARD;

    int id;

    if (((nCurrentAnimID == 0x24) || (nCurrentAnimID == 0x25)) && (m_pCurrentAnimController->m_fTime < 0.92f))
    {
        id = nCurrentAnimID;
    }
    else if (nAbsAngle <= 0x2AF8 || nCurrentAnimID == 0x26)
    {
        id = 0x26;
    }
    else if (nAngle > 0)
    {
        id = 0x25;
    }
    else
    {
        id = 0x24;
    }

    return id;
}

unsigned char Goalie::ClampToGoalCone(nlVector3& v3Position, float fDistFromEnd)
{
    float fGoalLineX = cField::GetGoalLineX(1U);
    float fAbsY = (float)fabsf(v3Position.y);
    float fSidelineY = cField::GetSidelineY(1U);
    float fXLimit = fGoalLineX - ((fAbsY * fDistFromEnd) / fSidelineY);

    if (v3Position.x > fXLimit)
    {
        v3Position.x = fXLimit;
        return true;
    }
    if (v3Position.x < -fXLimit)
    {
        v3Position.x = -fXLimit;
        return true;
    }
    return false;
}

void Goalie::CleanSTSRecover()
{
    mpSaveData = NULL;
    CleanupStun();
}

void Goalie::CleanupStun()
{
    if (mbStunEffectActive)
    {
        KillDaze(this);
        mbStunEffectActive = false;
    }
}

/**
 * Offset/Address/Size: 0x17D4 | 0x800442D0 | size: 0x3C
 */
void Goalie::ChooseSwatAnim(int nParam)
{
    mpLooseBallInfo = LooseBallAnims::GetSwatSTSInfo(nParam);
    mpSaveData = NULL;
}

/**
 * Offset/Address/Size: 0xF30 | 0x80043A2C | size: 0x8A4
 */
void Goalie::DoPassRelease()
{
    extern float g_fSimulationTick;

    if (m_pBall == NULL && mGoalieActionState == GOALIEACTION_LOOSEBALL_PICKUP && m_eAnimID == 3)
    {
        nlVector3 v3AnimBallPos = GetJointPosition(m_nBallJointIndex);

        float dy = v3AnimBallPos.y - g_pBall->m_v3Position.y;
        float dx = v3AnimBallPos.x - g_pBall->m_v3Position.x;
        float dz = v3AnimBallPos.z - g_pBall->m_v3Position.z;
        float distanceSq = nlGetLengthSquared3D(dx, dy, dz);

        if (distanceSq < 0.36f)
        {
            InitiatePickup();
        }
        else
        {
            return;
        }
    }

    if (m_pBall == NULL)
    {
        return;
    }

    nlVector3 v3SaveBallVel;
    nlVector3 v3Velocity;
    nlVector3 v3Desired;

    if (mGoalieActionState == GOALIEACTION_LOOSEBALL_PICKUP)
    {
        if (m_eAnimID == 5 || m_eAnimID == 4)
        {
            mpPassTarget = NULL;
        }
        else
        {
            mpPassTarget = FindOpenPassTarget();

            if (mpPassTarget != NULL)
            {
                float dy = mpPassTarget->m_v3Position.y - m_v3Position.y;
                float dx = mpPassTarget->m_v3Position.x - m_v3Position.x;
                u16 aTarget = RadToAng16(nlATan2f(dy, dx));
                s16 aDiff = (s16)(aTarget - GetClampedFacing());

                if ((u16)abs_s16(aDiff) > 0x2AA8 || !IsTargetViable(mpPassTarget))
                {
                    mpPassTarget = NULL;
                }
            }
        }
    }
    else if (mGoalieActionState == GOALIEACTION_STS)
    {
        if (mpSaveData != NULL)
        {
            int nBallJoint = m_nBallJointIndex;
            const nlVector3* pPrevBallPos = &GetPrevJointPosition(nBallJoint);
            const nlVector3* pCurBallPos = &GetJointPosition(nBallJoint);

            float invTick = 1.0f / g_fSimulationTick;
            nlVec3Sub(v3SaveBallVel, *pCurBallPos, *pPrevBallPos);
            nlVec3Scale(v3SaveBallVel, invTick);

            ReleaseBall();
            g_pBall->SetVelocity(v3SaveBallVel, SPINTYPE_NONE, NULL);
            SetNoPickUpTime(2.0f);
            return;
        }

        if (mpShooter != NULL)
        {
            if (m_pBall == NULL)
                return;

            HandleSTSSwat();
            return;
        }
    }

    bool bIsKick = false;
    switch (m_eAnimID)
    {
    case 2:
    case 3:
    case 4:
    case 5:
        bIsKick = true;
        Event* pEvent = g_pEventManager->CreateValidEvent(0x10, 0x38);
        GoalieSaveData* pSaveData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) GoalieSaveData();
        pSaveData->saveType = g_pBall->m_uGoalType;
        pSaveData->pShooter = g_pBall->m_pShooter;
        pSaveData->pGoalie = this;
        break;
    }

    if (mpPassTarget != NULL)
    {
        float fIsOpen = OpenTo(this, mpPassTarget);

        if ((bIsKick
                && nlGetLengthSquared2D(
                       m_v3Position.x - mpPassTarget->m_v3Position.x,
                       m_v3Position.y - mpPassTarget->m_v3Position.y)
                       > 64.0f)
            || (fIsOpen < 0.85f && m_eAnimID != 1))
        {
            DoRegularPassing(mpPassTarget, true, true, false, false);
        }
        else
        {
            DoRegularPassing(mpPassTarget, false, true, false, false);
        }

        if (bIsKick)
        {
            EmitBallPass(this);
        }
        return;
    }

    GoalieTweaks* pTweaks = (GoalieTweaks*)m_pTweaks;

    eSpinType spinType = (nlRandom(2, &nlDefaultSeed) != 0) ? SPINTYPE_FORWARD : SPINTYPE_BACK;

    float fPercent = nlRandomf(1.0f, &nlDefaultSeed);
    float fShotSpeed = Interpolate(pTweaks->fKickVelocityMin, pTweaks->fKickVelocityMax, fPercent);
    float fShotAng = Interpolate(pTweaks->fKickAngleMin, pTweaks->fKickAngleMax, fPercent);

    v3Desired = v3Zero;
    float fYPos = cField::GetPenaltyBoxY() + nlRandomf(1.0f, &nlDefaultSeed);
    float fDesiredY = (m_v3Position.y > 0.0f) ? fYPos : -fYPos;
    v3Desired.y = fDesiredY;

    float dy = v3Desired.y - m_v3Position.y;
    float dx = v3Desired.x - m_v3Position.x;
    u16 aDesired = RadToAng16(nlATan2f(dy, dx));

    aDesired = FindDumpDirection(aDesired, true);

    float fSin;
    float fCos;
    nlSinCos(&fSin, &fCos, aDesired);
    float fBaseCos = fCos;
    float fBaseSin = fSin;

    u16 aShot = RadToAng16((3.1415927f * fShotAng) / 180.0f);
    nlSinCos(&fSin, &fCos, aShot);

    float fXYMag = fCos * fShotSpeed;
    nlVec3Set(v3Velocity, fBaseCos * fXYMag, fBaseSin * fXYMag, fSin * fShotSpeed);

    ReleaseBall();
    g_pBall->ShootRelease(v3Velocity, spinType);
    SetNoPickUpTime(0.25f);
    g_pBall->m_tNoPickupTimer.SetSeconds(0.15f);

    if (bIsKick)
    {
        EmitBallImpact(this, false);
    }
}

/**
 * Offset/Address/Size: 0xAD0 | 0x800435CC | size: 0x460
 */
void Goalie::EventHandler(Event* event, void* userData)
{
    extern cCharacter* g_pCharacters[10];

    switch (event->m_uEventID)
    {
    case 3:
    {
        event = (Event*)g_pBall->m_pOwner;
        cPlayer* pPlayer = (cPlayer*)event;
        g_pBall->m_tNoPickupTimer.SetSeconds(3.0f);

        if (pPlayer != NULL)
        {
            pPlayer->ReleaseBall();

            if (pPlayer->m_eClassType == GOALIE)
            {
                Goalie* pGoalie = (Goalie*)pPlayer;

                pGoalie->CleanGoalieAction();

                pGoalie->mPrevGoalieActionState = pGoalie->mGoalieActionState;
                pGoalie->mGoalieActionState = GOALIEACTION_MOVE;
                pGoalie->mnSubstate = 0;

                pGoalie->SetAnimState(8, true, 0.2f, false, false);
                pGoalie->InitMovementFromAnim(0, v3Zero, 1.0f, false);

                pGoalie->mnSubstate = 1;
                pGoalie->mMoveDirection = GOALIEDIR_IDLE;

                pGoalie->m_pPhysicsCharacter->m_CanCollideWithBall = true;
                pGoalie->mbShouldMiss = false;
                pGoalie->mbDoNavigate = false;
                pGoalie->m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
                pGoalie->m_pPhysicsCharacter->m_CanCollideWithWall = true;

                if (pGoalie->mbStunEffectActive)
                {
                    KillDaze(pGoalie);
                    pGoalie->mbStunEffectActive = false;
                }

                pGoalie->mpShooter = NULL;
                pGoalie->mUrgency = URGENCY_LOW;
                pGoalie->mfSpeedScale = 1.0f;
                mbPosGoalieNetCheck = false;
                mbNegGoalieNetCheck = false;
                pGoalie->mbDoHeadTrack = true;
                pGoalie->mbBallImpacted = false;
                pGoalie->mbNoUserControl = false;
                pGoalie->mbPickedUp = false;
            }
            else if (pPlayer->m_eClassType == FIELDER)
            {
                cFielder* pFielder = (cFielder*)pPlayer;
                pFielder->EndDesire(false);
                pFielder->EndAction();
            }
        }

        Goalie* pHomeGoalie = (Goalie*)g_pCharacters[8];

        pHomeGoalie->CleanGoalieAction();

        pHomeGoalie->mPrevGoalieActionState = pHomeGoalie->mGoalieActionState;
        pHomeGoalie->mGoalieActionState = GOALIEACTION_MOVE;
        pHomeGoalie->mnSubstate = 0;

        pHomeGoalie->SetAnimState(8, true, 0.2f, false, false);
        pHomeGoalie->InitMovementFromAnim(0, v3Zero, 1.0f, false);

        pHomeGoalie->mnSubstate = 1;
        pHomeGoalie->mMoveDirection = GOALIEDIR_IDLE;

        pHomeGoalie->m_pPhysicsCharacter->m_CanCollideWithBall = true;
        pHomeGoalie->mbShouldMiss = false;
        pHomeGoalie->mbDoNavigate = false;
        pHomeGoalie->m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
        pHomeGoalie->m_pPhysicsCharacter->m_CanCollideWithWall = true;

        if (pHomeGoalie->mbStunEffectActive)
        {
            KillDaze(pHomeGoalie);
            pHomeGoalie->mbStunEffectActive = false;
        }

        pHomeGoalie->mpShooter = NULL;
        pHomeGoalie->mUrgency = URGENCY_LOW;
        pHomeGoalie->mfSpeedScale = 1.0f;
        mbPosGoalieNetCheck = false;
        mbNegGoalieNetCheck = false;
        pHomeGoalie->mbDoHeadTrack = true;
        pHomeGoalie->mbBallImpacted = false;
        pHomeGoalie->mbNoUserControl = false;
        pHomeGoalie->mbPickedUp = false;

        Goalie* pAwayGoalie = (Goalie*)g_pCharacters[9];

        pAwayGoalie->CleanGoalieAction();

        pAwayGoalie->mPrevGoalieActionState = pAwayGoalie->mGoalieActionState;
        pAwayGoalie->mGoalieActionState = GOALIEACTION_MOVE;
        pAwayGoalie->mnSubstate = 0;

        pAwayGoalie->SetAnimState(8, true, 0.2f, false, false);
        pAwayGoalie->InitMovementFromAnim(0, v3Zero, 1.0f, false);

        pAwayGoalie->mnSubstate = 1;
        pAwayGoalie->mMoveDirection = GOALIEDIR_IDLE;

        pAwayGoalie->m_pPhysicsCharacter->m_CanCollideWithBall = true;
        pAwayGoalie->mbShouldMiss = false;
        pAwayGoalie->mbDoNavigate = false;
        pAwayGoalie->m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
        pAwayGoalie->m_pPhysicsCharacter->m_CanCollideWithWall = true;

        if (pAwayGoalie->mbStunEffectActive)
        {
            KillDaze(pAwayGoalie);
            pAwayGoalie->mbStunEffectActive = false;
        }

        pAwayGoalie->mpShooter = NULL;
        pAwayGoalie->mUrgency = URGENCY_LOW;
        pAwayGoalie->mfSpeedScale = 1.0f;
        mbPosGoalieNetCheck = false;
        mbNegGoalieNetCheck = false;
        pAwayGoalie->mbDoHeadTrack = true;
        pAwayGoalie->mbBallImpacted = false;
        pAwayGoalie->mbNoUserControl = false;
        pAwayGoalie->mbPickedUp = false;

        break;
    }

    case 5:
    {
        GoalScoredData* data;

        s32 id = port_event_data_id(&event->m_data);

        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            data = 0;
        }
        else
        {
            id = port_event_data_id(&event->m_data);

            if (id != 0x18A)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                data = 0;
            }
            else
            {
                data = (GoalScoredData*)&event->m_data;
            }
        }

        // PORT: the resolver above sets this to 0 when the event carries no data, or data of another type.
        if (data == 0)
        {
            break;
        }

        Goalie* pGoalie = g_pTeams[data->uTeamIndex]->GetGoalie();
        Goalie* pOtherGoalie = g_pTeams[1 - data->uTeamIndex]->GetGoalie();

        if (g_pGame->m_eGameState != GS_KICKOFF)
        {
            pGoalie->mnOffplayPending = GOALIE_OFFPLAY_GOAL_FOR;
            pGoalie->mbPickedUp = false;
            pOtherGoalie->mnOffplayPending = GOALIE_OFFPLAY_GOAL_AGAINST;
            pOtherGoalie->mbPickedUp = false;
        }

        pGoalie->mFatigue.Reset();
        pOtherGoalie->mFatigue.Reset();
        break;
    }

    case 10:
    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0xA84 | 0x80043580 | size: 0x4C
 */
void Goalie::FindSTSStunData()
{
    mpSaveData = GoalieSave::GetRandomSTSSaveData();
    GoalieSave::GetClosestBlendedPos(mBlendInfo, v3Zero, mpSaveData);
    mpLooseBallInfo = NULL;
}

/**
 * Offset/Address/Size: 0x994 | 0x80043490 | size: 0xF0
 */
bool Goalie::FindSTSMissData(const nlVector3& rPos)
{
    nlVector3 localPos = rPos;

    float goalLineX = cField::GetGoalLineX(1U);
    float absY = (float)fabsf(localPos.y);
    float sidelineY = cField::GetSidelineY(1U);

    float scale = 6.0f;
    float threshold = goalLineX - ((absY * scale) / sidelineY);

    bool bIsOutsideRange;
    if (localPos.x > threshold)
    {
        bIsOutsideRange = true;
    }
    else if (localPos.x < -threshold)
    {
        bIsOutsideRange = true;
    }
    else
    {
        bIsOutsideRange = false;
    }

    mpSaveData = GoalieSave::GetRandomSTSMissData(bIsOutsideRange);
    GoalieSave::GetClosestBlendedPos(mBlendInfo, v3Zero, mpSaveData);

    mpLooseBallInfo = NULL;

    return bIsOutsideRange;
}

/**
 * Offset/Address/Size: 0x944 | 0x80043440 | size: 0x50
 */
void Goalie::FindSTSSpinData(bool bParam)
{
    mpSaveData = GoalieSave::GetSTSSpinMissData(bParam);

    GoalieSave::GetClosestBlendedPos(mBlendInfo, v3Zero, mpSaveData);

    mpLooseBallInfo = NULL;
}

/**
 * Offset/Address/Size: 0x93C | 0x80043438 | size: 0x8
 */
PhysicsGoalie* Goalie::GetPhysicsGoalie()
{
    return static_cast<PhysicsGoalie*>(m_pPhysicsCharacter);
}

/**
 * Offset/Address/Size: 0x5B8 | 0x800430B4 | size: 0x384
 */
void Goalie::SetDesiredSaveFacing(const nlVector3& v3BallPosition)
{
    if (m_v3Position.x > (cField::GetGoalLineX(1U) - 0.1f))
    {
        m_aDesiredFacingDirection = 0x8000;
        return;
    }

    if (m_v3Position.x < (0.1f - cField::GetGoalLineX(1U)))
    {
        m_aDesiredFacingDirection = 0;
        return;
    }

    nlVector3 v3Facing;
    nlVector3 v3G2Ball;
    nlVector3 v3G2Post1;
    nlVector3 v3G2Post2;

    nlVec3Sub(v3G2Ball, v3BallPosition, m_v3Position);

    m_pTeam->m_pNet->GetPostLocation(v3G2Post1, 0, 0.5f);
    m_pTeam->m_pNet->GetPostLocation(v3G2Post2, 1, 0.5f);

    nlVec3Sub(v3G2Post1, v3G2Post1, m_v3Position);
    nlVec3Sub(v3G2Post2, v3G2Post2, m_v3Position);

    float fLeftDot = nlVec3DotProduct(v3G2Ball, v3G2Post1);
    float fRightDot = nlVec3DotProduct(v3G2Ball, v3G2Post2);

    if ((fLeftDot > 0.0f) || (fRightDot > 0.0f))
    {
        if (fLeftDot > fRightDot)
        {
            nlVec3Set(v3Facing, v3G2Post1.y, -v3G2Post1.x, 0.0f);

            if (nlVec3DotProduct(v3Facing, v3G2Post2) > 0.0f)
            {
                nlVec3Scale(v3Facing, -1.0f);
            }
        }
        else
        {
            nlVec3Set(v3Facing, v3G2Post2.y, -v3G2Post2.x, 0.0f);

            if (nlVec3DotProduct(v3Facing, v3G2Post1) > 0.0f)
            {
                nlVec3Scale(v3Facing, -1.0f);
            }
        }
    }
    else
    {
        v3Facing = v3G2Ball;
    }

    float fBallOffMagSq = nlVec3DotProduct(v3G2Ball, v3G2Ball);

    if (fBallOffMagSq < 1.44f)
    {
        nlVector3 v3BallToGoal;
        nlVec3Sub(v3BallToGoal, v3BallPosition, m_pTeam->m_pNet->m_v3NetLocation);
        float fLengthSq = nlVec3LengthSquared(v3Facing);

        float fRecip = nlRecipSqrt(fLengthSq, true);
        nlVec3Scale(v3Facing, fRecip);

        float fRecip2 = nlRecipSqrt(nlVec3LengthSquared(v3BallToGoal), true);
        nlVec3Scale(v3BallToGoal, fRecip2);

        nlVec3WeightedSum(v3Facing, 0.5f, v3Facing, 0.5f, v3BallToGoal);
    }

    m_aDesiredFacingDirection = (s16)(nlATan2f(v3Facing.y, v3Facing.x) * (32768.0f / 3.14159265f));
}

/**
 * Offset/Address/Size: 0x3C4 | 0x80042EC0 | size: 0x1F4
 */
void Goalie::TrackTarget(const nlVector3& v3Target, float fRatio)
{
    nlVector3 v3FutureBallPos;
    nlVector3 v3FuturePos;
    unsigned short aRot;

    GetCurrentAnimFuture(m_nBallJointIndex, mpLooseBallInfo->mfPickupTime, v3FutureBallPos, v3FuturePos, aRot);

    float fDeltaY = v3Target.y - v3FutureBallPos.y;
    float fDeltaX = v3Target.x - v3FutureBallPos.x;

    float fAngleToTarget = nlATan2f(v3Target.y - m_v3Position.y, v3Target.x - m_v3Position.x);

    s16 aDiff = (s16)((u16)(s32)(10430.378f * fAngleToTarget)
                      - (u16)(s32)(10430.378f * nlATan2f(v3FutureBallPos.y - m_v3Position.y, v3FutureBallPos.x - m_v3Position.x)));
    s32 iRatio = (s32)(1024.0f * fRatio);
    s32 iTurn = (iRatio * aDiff) / 1024;
    SetFacingDirection((u16)(iTurn + m_aActualFacingDirection));

    nlVector3 v3Velocity;
    float fZero = 0.0f;
    v3Velocity.z = fRatio * fZero;
    v3Velocity.x = fRatio * fDeltaX;
    v3Velocity.y = fRatio * fDeltaY;

    v3Velocity.x = nlMaxEquals(v3Velocity.x, -0.12f);
    v3Velocity.x = nlMinEquals(v3Velocity.x, 0.12f);
    v3Velocity.y = nlMaxEquals(v3Velocity.y, -0.12f);
    v3Velocity.y = nlMinEquals(v3Velocity.y, 0.12f);

    nlVec3Add(v3FuturePos, v3Velocity, m_v3Position);

    SetPosition(v3FuturePos);
}

/**
 * Offset/Address/Size: 0x304 | 0x80042E00 | size: 0xC0
 */
void Goalie::TacklePlayer(cPlayer* pPlayer)
{
    cFielder* pFielder = static_cast<cFielder*>(pPlayer);
    if ((pPlayer != NULL) && (pPlayer->m_eClassType == FIELDER) && (pFielder->IsFallenDown(0.0f) == 0))
    {
        pPlayer->PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);
        if (pPlayer->m_pBall != 0)
        {
            pPlayer->ReleaseBall();
        }
        if (IsOnSameTeam(pFielder) != 0)
        {
            pFielder->EndDesire(false);
            pFielder->EndAction();
            return;
        }
        pFielder->InitActionSlideAttackReact(this, false);
    }
}

/**
 * Offset/Address/Size: 0x298 | 0x80042D94 | size: 0x6C
 */
void Goalie::StealBall(cPlayer* pPlayer)
{
    if (pPlayer == NULL)
    {
        return;
    }

    if (pPlayer->m_eClassType != FIELDER)
    {
        return;
    }

    if (pPlayer->m_pBall == NULL)
    {
        return;
    }

    pPlayer->ReleaseBall();

    cFielder* pFielder = static_cast<cFielder*>(pPlayer);
    if (pFielder->m_eFielderDesireState == FIELDERDESIRE_FINISH_ACTION)
    {
        return;
    }

    pFielder->EndDesire(false);
    pFielder->EndAction();
}

/**
 * Offset/Address/Size: 0x148 | 0x80042C44 | size: 0x150
 */
void Goalie::WhackSTSPlayer(cFielder* pFielder)
{
    if (pFielder == NULL)
    {
        return;
    }

    if (pFielder->m_pBall != NULL)
    {
        pFielder->ReleaseBall();
    }

    pFielder->SetFacingDirection(m_aActualFacingDirection + 0x8000);
    pFielder->InitActionSTSHitReact(this);
    PlayRandomCharDialogue(CHAR_DIALOGUE_HIT, VECTORS, 100.0f, -1.0f);
    pFielder->PlayAttackReactionSounds(g_pGame->m_pGameTweaks->fGoalieDropKickHitReactionVolume);

    nlVector3 v3BallVel;
    float fBallVelMult = ((GoalieTweaks*)m_pTweaks)->fSTSAttackBallVelMult;
    nlVec3Scale(v3BallVel, m_v3Position, -fBallVelMult);

    float yRand = nlRandomf(5.0f, &nlDefaultSeed);
    if ((u32)nlRandom(100, &nlDefaultSeed) > 50)
    {
        yRand *= -1.0f;
    }
    v3BallVel.y += yRand;

    v3BallVel.z = 4.0f + nlRandomf(2.0f, &nlDefaultSeed);

    g_pBall->SetVelocity(v3BallVel, SPINTYPE_FORWARD, NULL);
    g_pBall->m_tNoPickupTimer.SetSeconds(0.12f);
}

void Goalie::InitGoalieActionData()
{
    if (!mbActionDataSetup)
    {
        nlVector3 v3Trans;

        GetJointPositionFuture(&v3Trans, 0x19, -1, 1.0f, true, true, false);
        mfGoalieStepDist = fabsf(0.6f * v3Trans.y);

        GetJointPositionFuture(&v3Trans, 0x14, -1, 1.0f, true, true, false);

        mbActionDataSetup = true;
        mfGoalieStrafeDist = fabsf(0.6f * v3Trans.y);
        mfGoalieRunDist = fabsf(1.8f * v3Trans.y);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80042AFC | size: 0x148
 */
void Goalie::InitActionPostWhistle()
{
    if (m_pBall != NULL)
    {
        ReleaseBall();
    }

    mnOffplayPending = GOALIE_OFFPLAY_NONE;
    mbPickedUp = false;
    SetAnimState(8, false, 0.0f, false, false);

    CleanGoalieAction();

    mPrevGoalieActionState = mGoalieActionState;
    mGoalieActionState = GOALIEACTION_MOVE;
    mnSubstate = 0;

    SetAnimState(8, true, 0.2f, false, false);

    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    mnSubstate = 1;
    mMoveDirection = GOALIEDIR_IDLE;

    m_pPhysicsCharacter->m_CanCollideWithBall = 1;
    mbShouldMiss = false;
    mbDoNavigate = false;

    m_pPhysicsCharacter->m_CanCollidedWithGoalLine = 1;

    m_pPhysicsCharacter->m_CanCollideWithWall = 1;

    CleanupStun();

    mpShooter = NULL;
    mUrgency = URGENCY_LOW;
    mfSpeedScale = 1.0f;
    mbPosGoalieNetCheck = false;
    mbNegGoalieNetCheck = false;
    mbDoHeadTrack = true;
    mbBallImpacted = false;
    mbNoUserControl = false;
    mbPickedUp = false;
}
