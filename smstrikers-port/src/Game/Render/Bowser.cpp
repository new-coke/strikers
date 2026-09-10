#include "Game/Render/Bowser.h"
#include "NL/vmath.h"
#include "Game/SAnim/pnFeather.h"
#include "Game/SAnim/pnBlender.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/HeadTrack.h"
#include "Game/AI/Powerups.h"
#include "Game/AI/Fielder.h"
#include "Game/Field.h"
#include "Game/Ball.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsShell.h"
#include "Game/Physics/PhysicsBanana.h"
#include "Game/Sys/eventman.h"
#include "Game/ReplayManager.h"
#include "NL/nlTask.h"
#include "NL/nlString.h"
#include "Game/PoseAccumulator.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Render/NetMesh.h"
#include "Game/Game.h"
#include "Game/World/worldanim.h"
#include "Game/Audio/AudioLoader.h"

#include "math.h"

#pragma inline_max_total_size(5000)

/* Owned by src/Game/SoundProps/bowsergensoundproperties.cpp. */
extern SoundPropAccessor* gpBOWSERSoundPropAccessor;

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };
static const nlVector3 gv3BowserHomePosition = { 0.0f, 100.0f, -10.0f };

float Bowser::mfYAxisTilt = 0.0f;

/**
 * Offset/Address/Size: 0x4BBC | 0x8015D930 | size: 0x2C
 */
static void AnimSoundCallback(uintptr_t eventID)
{
    g_pEventManager->CreateValidEvent(eventID, 0x14);
}

/**
 * Offset/Address/Size: 0x4638 | 0x8015D3AC | size: 0x584
 */
Bowser::Bowser(cSHierarchy& rSHierarchy, int nTeam, PhysicsNPC& rPhysNPC, cInventory<cSAnim>* pInventorySAnim)
    : SkinAnimatedMovableNPC(rSHierarchy, nTeam, rPhysNPC)
    , mpFeatherController(NULL)
    , mpTarget(NULL)
    , meBowserState((eBowserState)0)
{
    mAnimID = 0;
    mbDoTilt = false;
    mAttackType = (eBowserAttackType)0;
    mStompStage = 0;
    mbFirstTime = false;
    mbAlive = false;
    mbResetPending = false;
    m_pCharacterSFX = NULL;
    mtStateTimer.SetSeconds(0.0f);
    mtActiveTimer.SetSeconds(0.0f);

    mpAnim[0] = pInventorySAnim->Find((unsigned int)nlStringHash("bowser_idle"));
    mpAnim[1] = pInventorySAnim->Find((unsigned int)nlStringHash("bowser_drop_and_land"));
    mpAnim[2] = pInventorySAnim->Find((unsigned int)nlStringHash("bowser_exit"));
    mpAnim[3] = pInventorySAnim->Find((unsigned int)nlStringHash("bowser_walk_fwd"));
    mpAnim[4] = pInventorySAnim->Find((unsigned int)nlStringHash("bowser_throw"));
    mpAnim[5] = pInventorySAnim->Find((unsigned int)nlStringHash("bowser_fire_up"));
    mpAnim[6] = pInventorySAnim->Find((unsigned int)nlStringHash("bowser_roll"));

    mpPhysObj->mpAINPC = this;
    mpHeadTrack = new (nlMalloc(sizeof(cHeadTrack), 8, false)) cHeadTrack();

    cSHierarchy* pHierarchy = mpPoseAccumulator->m_BaseSHierarchy;
    mnHeadJointIndex = pHierarchy->GetNodeIndexByID(nlStringLowerHash("bip01 head"));
    mLastHeadMatrix.SetIdentity();

    mpFeatherBlender = ::new (AllocateFeather()) cPN_Feather(mpPoseAccumulator->m_BaseSHierarchy, NULL, 0);
    if (mpPoseTree != NULL)
    {
        mpFeatherBlender->SetChild(0, mpPoseTree);
    }
    mpPoseTree = mpFeatherBlender;
    mbIsVisible = false;
    EmissionManager::Destroy((uintptr_t)this, fxGetGroup("bowser_fire"));
    g_pEventManager->CreateValidEvent(0x65, 0x14);

    eBowserAttackType savedAttackType;
    bool savedVisible = mbIsVisible;
    mbIsVisible = false;
    meBowserState = (eBowserState)0;
    mfDesiredSpeed = 0.0f;

    if (mpFeatherBlender->GetChild(1) != NULL)
    {
        delete mpFeatherBlender->GetChild(1);
        mpFeatherBlender->SetChild(1, NULL);
    }

    mpFeatherController = NULL;
    SetPosition(gv3BowserHomePosition);
    mv3Velocity = v3Zero;
    maFacingDirection = 0;
    mpPhysObj->DisableCollisions();

    if (mAttackType != BOWSER_ATTACK_STOMP || mStompStage == 2)
    {
        savedAttackType = mAttackType;
        SetTiltParameters(0.0f);
        mAttackType = (eBowserAttackType)0;

        if (g_pGame->m_pGameTweaks->fBowserEndTime < 0.0f)
        {
            g_pGame->ResetBowser();
        }

        if (mbAlive)
        {
            mbAlive = false;
            if (GameInfoManager::Instance()->IsBowserAttackEnabled() && savedAttackType != BOWSER_ATTACK_STOMP && savedVisible)
            {
                g_pEventManager->CreateValidEvent(0x37, 0x14);
            }
        }
    }
    else
    {
        g_pGame->ResetBowserTimer(g_pGame->m_pGameTweaks->fBowserTiltTime);
    }

    m_pCharacterSFX = new (nlMalloc(sizeof(Audio::cCharacterSFX), 8, false)) Audio::cCharacterSFX();
    SetupBaseSFX();
}

/**
 * Offset/Address/Size: 0x44F0 | 0x8015D264 | size: 0x148
 */
Bowser::~Bowser()
{
    SetTiltParameters(0.0f);
    delete mpHeadTrack;
    delete m_pCharacterSFX;
}

/**
 * Offset/Address/Size: 0x325C | 0x8015BFD0 | size: 0x1294
 */
void Bowser::Update(float fDeltaT)
{
    bool bIsSTS = false;
    GameTweaks* pTweaks;
    if (g_pBall->GetOwnerFielder() != NULL)
    {
        if (g_pBall->GetOwnerFielder()->m_eActionState == ACTION_SHOOT_TO_SCORE)
        {
            bIsSTS = true;
            if (mbIsVisible)
            {
                float animTime = mpAnimController->m_fTime;
                switch (meBowserState)
                {
                case BOWSER_STATE_IDLE:
                case BOWSER_STATE_THROW:
                case BOWSER_STATE_ROAR:
                    ActionLeave();
                    break;
                case BOWSER_STATE_ROLL:
                    if (animTime > 0.58536583f)
                    {
                        ActionLeave();
                    }
                    break;
                case BOWSER_STATE_FALL:
                    if (animTime > 0.34920636f)
                    {
                        ActionLeave();
                    }
                    break;
                default:
                    break;
                }
            }
            else if (meBowserState != BOWSER_STATE_HIDDEN)
            {
                ActionHide();
            }
        }
    }

    if (mbResetPending)
    {
        mbResetPending = false;
        if (mbAlive)
        {
            ActionReset();
            return;
        }
    }

    if (!GameInfoManager::Instance()->IsBowserAttackEnabled())
    {
        if (mbIsVisible)
        {
            ActionHide();
        }
        return;
    }

    if (g_pGame->mIsPure)
    {
        return;
    }

    if (mtActiveTimer.m_uPackedTime != 0)
    {
        mtActiveTimer.Countdown(fDeltaT, 0.0f);
    }

    switch (meBowserState)
    {
    case BOWSER_STATE_HIDDEN:
        mbIsVisible = false;
        break;
    case BOWSER_STATE_FALL:
    {
        if (!mbIsVisible)
        {
            if (mAttackType != BOWSER_ATTACK_STOMP)
            {
                if (fabsf(mv3TargetPos.x - nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->GetTargetPosition().x) > 8.0f)
                {
                    return;
                }
            }

            mbIsVisible = true;
            if (GameInfoManager::Instance()->IsBowserAttackEnabled())
            {
                if (mAttackType != BOWSER_ATTACK_STOMP)
                {
                    g_pEventManager->CreateValidEvent(0x35, 0x14);
                }
                else if (mStompStage != 2)
                {
                    g_pEventManager->CreateValidEvent(0x38, 0x14);
                }
                else
                {
                    g_pEventManager->CreateValidEvent(0x3A, 0x14);
                }
            }
            g_pEventManager->CreateValidEvent(0x59, 0x14);
        }

        Move(fDeltaT);

        bool bLand1 = mpAnimController->TestTrigger(0.14f);
        bool bLand2 = mpAnimController->TestTrigger(0.3f);
        if (bLand1 || bLand2)
        {
            if (!g_pGame->mbCaptainShotToScoreOn)
            {
                EffectsGroup* pGroup = fxGetGroup("bowser_land");
                EmissionController* pControl = EmissionManager::Create(pGroup, 0);
                pControl->SetPosition(mv3Position);
                pControl->SetPoseAccumulator(*mpPoseAccumulator);
                pControl->SetUpdateCallback(UpdateBowserLandEmitter);
            }

            if (mbFirstTime)
            {
                mbFirstTime = false;
                if (GameInfoManager::Instance()->IsBowserAttackEnabled())
                {
                    if (mAttackType == BOWSER_ATTACK_STOMP)
                    {
                        g_pEventManager->CreateValidEvent(0x39, 0x14);
                    }
                    else
                    {
                        g_pEventManager->CreateValidEvent(0x36, 0x14);
                    }
                }
            }

            if (bLand1)
            {
                FireCameraRumbleFilter(0.0f, 0.2f);
                NetMesh::spPositiveXNetMesh->JoltNet(0.04f);
                NetMesh::spNegativeXNetMesh->JoltNet(0.04f);
            }

            mpPhysObj->EnableCollisions();

            if (mbDoTilt)
            {
                pTweaks = g_pGame->m_pGameTweaks;
                float fTiltForce = pTweaks->fBowserMinTiltForce + nlRandomf(pTweaks->fBowserMaxTiltForce - pTweaks->fBowserMinTiltForce, &nlDefaultSeed);
                float fNewTilt = mfYAxisTilt - (fTiltForce * mv3Position.x);
                float fMaxTilt = g_pGame->m_pGameTweaks->fBowserMaxTilt;
                if (fNewTilt > fMaxTilt)
                {
                    fNewTilt = fMaxTilt;
                }
                else if (fNewTilt < -fMaxTilt)
                {
                    fNewTilt = -fMaxTilt;
                }
                SetTiltParameters(fNewTilt);
            }

            mfDesiredSpeed = 0.0f;
        }

        if (mpAnimController->m_fTime >= 0.3f)
        {
            if (CheckForAbort())
            {
                break;
            }
        }

        if (mpAnimController->TestTrigger(0.4f))
        {
            KillFire();
            EmitFire();
        }

        bool bAnimDone = false;
        if (mpAnimController->m_ePlayMode == PM_HOLD && mpAnimController->m_fTime == 1.0f)
        {
            bAnimDone = true;
        }
        if (bAnimDone)
        {
            if (mtActiveTimer.m_uPackedTime != 0)
            {
                if (mAttackType == BOWSER_ATTACK_ROLL)
                {
                    ActionRoll();
                }
                else
                {
                    ActionIdle();
                }
            }
            else
            {
                if (fabsf(mfYAxisTilt) < g_pGame->m_pGameTweaks->fBowserMaxTilt)
                {
                    ActionIdle();
                }
                else
                {
                    ActionLeave();
                }
            }
        }
    }
    break;
    case BOWSER_STATE_IDLE:
    {
        if (!CheckForAbort())
        {
            nlVector3 v3BallPos = g_pBall->m_v3Position;
            CheckTargetBounds(v3BallPos);

            maDesiredFacingDirection = (u16)(s32)(10430.378f * nlATan2f(v3BallPos.y - mv3Position.y, v3BallPos.x - mv3Position.x));
            AnimMoveSeek(fDeltaT, 40000.0f, 3000.0f, false);

            nlVector3 v3Pos = mv3Position;
            if (CheckTargetBounds(v3Pos))
            {
                SetPosition(v3Pos);
            }

            CheckFootSteps();

            if (mAttackType == BOWSER_ATTACK_STOMP)
            {
                if (!mbDoTilt || fabsf(mfYAxisTilt) >= g_pGame->m_pGameTweaks->fBowserMaxTilt)
                {
                    KillFire();
                    ActionLeave();
                }
            }
            else if (mtStateTimer.Countdown(fDeltaT, 0.0f))
            {
                KillFire();
                if (mtActiveTimer.m_uPackedTime == 0)
                {
                    ActionLeave();
                }
                else if (mAttackType == BOWSER_ATTACK_JUMP)
                {
                    ActionJump();
                }
                else
                {
                    ActionThrow();
                }
            }
        }
        break;
    }
    case BOWSER_STATE_JUMP:
        if (mpAnimController->m_fTime > 0.6458333f)
        {
            Move(fDeltaT);
            if (mpAnimController->TestFrameTrigger(16.0f))
            {
                mpPhysObj->DisableCollisions();
            }
        }
        {
            bool bAnimDone = false;
            if (mpAnimController->m_ePlayMode == PM_HOLD && mpAnimController->m_fTime == 1.0f)
            {
                bAnimDone = true;
            }
            if (bAnimDone)
            {
                ActionDescend(0.1f);
            }
        }
        break;
    case BOWSER_STATE_THROW:
    {
        if (!CheckForAbort())
        {
            nlVector3 v3TargetPos = mpTarget->m_v3Position;
            CheckTargetBounds(v3TargetPos);

            maDesiredFacingDirection = (u16)(s32)(10430.378f * nlATan2f(v3TargetPos.y - mv3Position.y, v3TargetPos.x - mv3Position.x));
            AnimMoveSeek(fDeltaT, 40000.0f, 3000.0f, false);

            nlVector3 v3Pos = mv3Position;
            if (CheckTargetBounds(v3Pos))
            {
                SetPosition(v3Pos);
            }

            if (mpFeatherController->TestTrigger(0.5f))
            {
                ePowerUpType eType = (ePowerUpType)nlRandom(3, &nlDefaultSeed);
                PowerupCreateAndThrow(NULL, eType, 1, this);
            }

            bool bFeatherDone = false;
            if (mpFeatherController->m_ePlayMode == PM_HOLD && mpFeatherController->m_fTime == 1.0f)
            {
                bFeatherDone = true;
            }
            if (bFeatherDone)
            {
                if (mtActiveTimer.m_uPackedTime == 0)
                {
                    ActionLeave();
                }
                else if (mAttackType == BOWSER_ATTACK_CRAZY && nlRandom(100, &nlDefaultSeed) < 70)
                {
                    ClearBowserFeatherAnimState(true);
                    ActionJump();
                }
                else
                {
                    float fDeltaX = mv3Position.x - mpTarget->m_v3Position.x;
                    float fDeltaY = mv3Position.y - mpTarget->m_v3Position.y;
                    if ((fDeltaX * fDeltaX) + (fDeltaY * fDeltaY) < 64.0f)
                    {
                        if (nlRandom(100, &nlDefaultSeed) < 50)
                        {
                            ActionThrow();
                        }
                        else
                        {
                            ActionIdle();
                        }
                    }
                    else
                    {
                        ClearBowserFeatherAnimState(true);
                        ActionRoll();
                    }
                }
            }

            CheckFootSteps();
        }
        break;
    }
    case BOWSER_STATE_ROAR:
        if (!CheckForAbort())
        {
            bool bRoarDone = false;
            if (mpAnimController->m_ePlayMode == PM_HOLD && mpAnimController->m_fTime == 1.0f)
            {
                bRoarDone = true;
            }
            if (bRoarDone)
            {
                if (mtActiveTimer.m_uPackedTime == 0)
                {
                    ActionLeave();
                }
                else
                {
                    ActionThrow();
                }
            }
        }
        break;
    case BOWSER_STATE_ROLL:
    {
        nlVector3 v3TargetPos;
        if (bIsSTS)
        {
            v3TargetPos = g_pBall->GetOwnerFielder()->m_pTeam->m_pNet->m_v3NetLocation;
        }
        else if (mpTarget != NULL)
        {
            v3TargetPos = mpTarget->m_v3Position;
        }
        else
        {
            v3TargetPos = mv3TargetPos;
        }

        CheckTargetBounds(v3TargetPos);

        maDesiredFacingDirection = (u16)(s32)(10430.378f * nlATan2f(v3TargetPos.y - mv3Position.y, v3TargetPos.x - mv3Position.x));
        AnimMoveSeek(fDeltaT, 40000.0f, 3000.0f, false);

        nlVector3 v3Pos = mv3Position;
        if (CheckTargetBounds(v3Pos))
        {
            SetPosition(v3Pos);
        }

        bool bRollDone = false;
        if (mpAnimController->m_ePlayMode == PM_HOLD && mpAnimController->m_fTime == 1.0f)
        {
            bRollDone = true;
        }
        if (bRollDone)
        {
            if (mtActiveTimer.m_uPackedTime == 0)
            {
                ActionLeave();
            }
            else
            {
                ActionThrow();
            }
        }
        break;
    }
    case BOWSER_STATE_LEAVE:
    {
        if (mpAnimController->m_fTime > 0.6666667f)
        {
            maFacingDirection = SeekDirection(maFacingDirection, maDesiredFacingDirection, 60000.0f, 4000.0f, fDeltaT);
            mpPhysObj->DisableCollisions();

            GravityMove(fDeltaT);

            if (mAttackType != BOWSER_ATTACK_STOMP || mStompStage == 2)
            {
                float fNewTilt = 0.95f * mfYAxisTilt;
                if (fabsf(fNewTilt) < 0.01f)
                {
                    fNewTilt = 0.0f;
                }
                SetTiltParameters(fNewTilt);
            }
        }

        if (mtStateTimer.Countdown(fDeltaT, 0.0f))
        {
            ActionHide();
        }
        break;
    }
    }

    SkinAnimatedNPC::Update(fDeltaT);

    switch (meBowserState)
    {
    case BOWSER_STATE_IDLE:
    case BOWSER_STATE_THROW:
        mpHeadTrack->m_bTrackOOI = true;
        break;
    case BOWSER_STATE_HIDDEN:
    case BOWSER_STATE_FALL:
    case BOWSER_STATE_JUMP:
    case BOWSER_STATE_ROAR:
    case BOWSER_STATE_ROLL:
    case BOWSER_STATE_LEAVE:
        mpHeadTrack->m_bTrackOOI = false;
        break;
    }

    if (ReplayManager::Instance()->mRender != NULL)
    {
        mpHeadTrack->m_v3OOI = ReplayManager::Instance()->mRender->mBall.mPosition;
        mpHeadTrack->Update(mLastHeadMatrix, mLastHeadMatrix, nlTaskManager::m_pInstance->m_fCurrentTimeDelta, 0x8000, 0x3555, 0x0AAA, 0.4f);
    }
}

/**
 * Offset/Address/Size: 0x3054 | 0x8015BDC8 | size: 0x208
 */
void Bowser::CollisionCallback(PhysicsObject* pPhysObj, PhysicsObject* pObjA, const nlVector3& v3Pos)
{
    Bowser* pObj = (Bowser*)((PhysicsNPC*)pPhysObj)->mpAINPC;
    cCharacter* pCharacter = NULL;
    int type = pObjA->GetObjectType();

    switch (type)
    {
    case 4:
    case 0x0D:
    case 0x0E:
        pCharacter = (cCharacter*)((PhysicsCharacter*)pObjA->m_parentObject)->m_pAICharacter;
        break;

    case 0x0F:
    {
        cBall* pBall = ((PhysicsAIBall*)pObjA)->m_pAIBall;
        bool shouldDamage = false;
        u32 timer = pBall->m_tShotTimer.m_uPackedTime;
        if (timer != 0)
        {
            if (pBall->mbCanDamage)
            {
                shouldDamage = true;
            }
        }
        if (shouldDamage || (shouldDamage = (timer != 0 && pBall->m_unk_0xA4)))
        {
            g_pEventManager->CreateValidEvent(0x62, 0x14);
        }
        if (pBall->m_pOwner != NULL)
        {
            pCharacter = (cCharacter*)pBall->m_pOwner;
        }
        else
        {
            if (pBall->m_pPassTarget != NULL)
            {
                pBall->ClearPassTarget();
            }
            if (pBall->m_tShotTimer.m_uPackedTime != 0)
            {
                pBall->ClearShotInProgress();
            }
        }
        break;
    }

    case 0x13:
    {
        PowerupBase* pPowerup = ((PhysicsShell*)pObjA)->m_pPowerupObject;
        if (!pPowerup->m_bShouldDestroy)
        {
            if (pPowerup->meSize == POWERUPSIZE_LARGE)
            {
                g_pEventManager->CreateValidEvent(0x62, 0x14);
            }
        }
        ((PhysicsShell*)pObjA)->m_pPowerupObject->m_bShouldDestroy = true;
        break;
    }

    case 0x14:
    {
        PowerupBase* pPowerup = ((PhysicsBanana*)pObjA)->m_pPowerupObject;
        if (!pPowerup->m_bShouldDestroy)
        {
            if (pPowerup->meSize == POWERUPSIZE_LARGE)
            {
                g_pEventManager->CreateValidEvent(0x62, 0x14);
            }
        }
        ((PhysicsBanana*)pObjA)->m_pPowerupObject->m_bShouldDestroy = true;
        break;
    }
    }

    if (pCharacter != NULL)
    {
        if (pCharacter->m_eClassType == FIELDER)
        {
            if (!((cFielder*)pCharacter)->IsFallenDown(0.0f))
            {
                if (!((cFielder*)pCharacter)->IsInvincible())
                {
                    Event* pEvent = g_pEventManager->CreateValidEvent(0x30, 0x20);
                    CollisionBowserPlayerData* pData = new (&pEvent->m_data) CollisionBowserPlayerData();
                    pData->pFielder = (cFielder*)pCharacter;
                    pData->pBowser = pObj;
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x2B64 | 0x8015B8D8 | size: 0x4F0
 */
void Bowser::ActionInit()
{
    eBowserAttackType savedAttackType;
    u32 savedVisible;

    if (!nlSingleton<GameInfoManager>::Instance()->IsTiltingFieldOn()
        && nlSingleton<GameInfoManager>::Instance()->IsBowserAttackEnabled()
        && !nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
    {
    }
    else
    {
        return;
    }

    mbIsVisible = false;
    mbFirstTime = true;

    if (mAttackType == BOWSER_ATTACK_STOMP)
    {
        mStompStage++;
        if (fabsf(cCameraManager::m_UpVectorStack[cCameraManager::m_UpVectorStackSize].x) < 0.001f
            && fabsf(cCameraManager::m_UpVectorStack[cCameraManager::m_UpVectorStackSize].y) < 0.001f
            && mStompStage == 2)
        {
            mAttackType = BOWSER_ATTACK_ROLL;
            EmissionManager::Destroy((uintptr_t)this, fxGetGroup("bowser_fire"));
            g_pEventManager->CreateValidEvent(0x65, 0x14);
            savedVisible = mbIsVisible;
            mbIsVisible = false;
            meBowserState = BOWSER_STATE_HIDDEN;
            mfDesiredSpeed = 0.0f;
            if (mpFeatherBlender->GetChild(1) != NULL)
            {
                delete mpFeatherBlender->GetChild(1);
                mpFeatherBlender->SetChild(1, NULL);
            }
            mpFeatherController = NULL;
            SetPosition(gv3BowserHomePosition);
            mv3Velocity = v3Zero;
            maFacingDirection = 0;
            mpPhysObj->DisableCollisions();
            if (mAttackType != BOWSER_ATTACK_STOMP || mStompStage == 2)
            {
                savedAttackType = mAttackType;
                SetTiltParameters(0.0f);
                mAttackType = BOWSER_ATTACK_ROLL;
                if (g_pGame->m_pGameTweaks->fBowserEndTime < 0.0f)
                    g_pGame->ResetBowser();
                if (!mbAlive)
                    return;
                mbAlive = false;
                if (!nlSingleton<GameInfoManager>::Instance()->IsBowserAttackEnabled())
                    return;
                if (savedAttackType == BOWSER_ATTACK_STOMP)
                    return;
                if (savedVisible == 0)
                    return;
                g_pEventManager->CreateValidEvent(0x37, 0x14);
                return;
            }
            g_pGame->ResetBowserTimer(g_pGame->m_pGameTweaks->fBowserTiltTime);
            return;
        }
    }
    else
    {
        float savedDuration = g_pGame->m_fGameDuration;
        float gameTime = g_pGame->GetGameTime();
        if (savedDuration - gameTime < 15.0f)
            return;
        mbAlive = true;
        mAttackType = (eBowserAttackType)nlRandom(NUM_BOWSER_ATTACKS, &nlDefaultSeed);
        mbDoTilt = true;
        switch (mAttackType)
        {
        case BOWSER_ATTACK_ROLL:
            ActionFall();
            mbDoTilt = false;
            break;
        case BOWSER_ATTACK_JUMP:
        case BOWSER_ATTACK_CRAZY:
            ActionFall();
            break;
        case BOWSER_ATTACK_STOMP:
            mStompStage = 0;
            break;
        }
    }

    if (mAttackType != BOWSER_ATTACK_STOMP)
        return;
    if (mStompStage == 0)
    {
        cBaseCamera* cam = nlDLRingGetStart(cCameraManager::m_cameraStack);
        const nlVector3& target = cam->GetTargetPosition();
        mv3TargetPos = target;
    }
    else
    {
        mv3TargetPos = v3Zero;
        if (mfYAxisTilt > 0.0f)
            mv3TargetPos.x = 8.0f;
        else
            mv3TargetPos.x = -8.0f;
    }
    if (mStompStage != 2)
    {
        float clampX = cField::GetGoalLineX(1u) - 3.0f;
        if (fabsf(mv3TargetPos.x) > clampX)
        {
            if (mv3TargetPos.x > 0.0f)
                mv3TargetPos.x = clampX;
            else
                mv3TargetPos.x = -clampX;
        }
        float clampY = cField::GetSidelineY(1u) - 3.0f;
        if (fabsf(mv3TargetPos.y) > clampY)
        {
            if (mv3TargetPos.y < 0.0f)
                mv3TargetPos.y = -clampY;
            else
                mv3TargetPos.y = clampY;
        }
        if (fabsf(mv3TargetPos.x) < 8.0f)
        {
            if (mv3TargetPos.x < 0.0f)
                mv3TargetPos.x = -8.0f;
            else
                mv3TargetPos.x = 8.0f;
        }
    }
    ActionStomp();
}

/**
 * Offset/Address/Size: 0x28A4 | 0x8015B618 | size: 0x2C0
 */
void Bowser::ActionThrow()
{
    if (CheckForAbort())
    {
        return;
    }

    if (!GameInfoManager::Instance()->GetGameplayOptions().PowerUps)
    {
        ActionIdle();
        return;
    }

    if (mAnimID != BOWSER_ANIM_WALK)
    {
        mAnimID = BOWSER_ANIM_WALK;
        SetBowserAnimState(BOWSER_ANIM_WALK, PM_CYCLIC, 0.15f);
    }

    meBowserState = BOWSER_STATE_THROW;

    if (mpFeatherBlender->GetChild(1) != NULL)
    {
        delete mpFeatherBlender->GetChild(1);
        mpFeatherBlender->SetChild(1, NULL);
    }

    mpFeatherController = NULL;
    cPN_SAnimController* controller = AllocateSAnimController();
    controller = ::new (&*controller) cPN_SAnimController(
        mpAnim[BOWSER_ANIM_THROW],
        (const AnimRetarget*)0,
        PM_HOLD,
        (void (*)(uintptr_t, cPN_SAnimController*))0,
        (unsigned int)0,
        (bool)0);
    mpFeatherController = controller;

    mpFeatherBlender->ClearNodeWeights();
    mpFeatherBlender->SetNodeWeight(0xE, 1.0f, 0.2f);
    mpFeatherBlender->SetChild(1, mpFeatherController);
    mpFeatherBlender->BeginBlendIn(0.2f);

    if (g_pBall->GetOwnerFielder() != NULL && !g_pBall->GetOwnerFielder()->IsInvincible())
    {
        mpTarget = g_pBall->GetOwnerFielder();
    }
    else
    {
        mpTarget = FindPowerupTarget((cFielder*)NULL, this);
    }
}

/**
 * Offset/Address/Size: 0x26DC | 0x8015B450 | size: 0x1C8
 */
void Bowser::ActionRoll()
{
    EmissionManager::Destroy((uintptr_t)this, fxGetGroup("bowser_fire"));
    g_pEventManager->CreateValidEvent(0x65, 0x14);
    g_pEventManager->CreateValidEvent(0x61, 0x14);

    meBowserState = BOWSER_STATE_ROLL;

    mAnimID = BOWSER_ANIM_ROLL;
    SetBowserAnimState(BOWSER_ANIM_ROLL, PM_HOLD, 0.2f);

    if (g_pBall->GetOwnerFielder() != NULL && !g_pBall->GetOwnerFielder()->IsInvincible())
    {
        mpTarget = g_pBall->GetOwnerFielder();
    }
    else
    {
        mpTarget = FindPowerupTarget((cFielder*)NULL, this);
    }
}

/**
 * Offset/Address/Size: 0x24B4 | 0x8015B228 | size: 0x228
 */
void Bowser::ActionStomp()
{
    nlVector3 vel;
    mAnimID = 1;
    cPN_SAnimController* controller = AllocateSAnimController();
    controller = ::new (controller) cPN_SAnimController(mpAnim[1], (const AnimRetarget*)0, PM_HOLD, (void (*)(uintptr_t, cPN_SAnimController*))0, (unsigned int)0, (bool)0);
    if (mpFeatherBlender->GetChild(0) != NULL)
    {
        delete mpFeatherBlender->GetChild(0);
    }
    mpFeatherBlender->SetChild(0, controller);
    mpAnimController = controller;
    meBowserState = BOWSER_STATE_FALL;
    mfDesiredSpeed = 0.0f;
    nlVector3 targetPos = mv3TargetPos;
    float goalLineX = cField::GetGoalLineX(1U);
    float limitX = goalLineX - 3.0f;
    if ((float)fabs(targetPos.x) > limitX)
    {
        if (targetPos.x > 0.0f)
        {
            targetPos.x = limitX;
        }
        else
        {
            targetPos.x = -limitX;
        }
    }
    float sidelineY = cField::GetSidelineY(1U);
    float limitY = sidelineY - 3.0f;
    if ((float)fabs(targetPos.y) > limitY)
    {
        if (targetPos.y < 0.0f)
        {
            targetPos.y = -limitY;
        }
        else
        {
            targetPos.y = limitY;
        }
    }
    targetPos.z = 10.0f;
    targetPos.z = 0.0f;
    targetPos.y += 3.0f;
    SetPosition(targetPos);
    maFacingDirection = 0xC000;
    maDesiredFacingDirection = 0xC000;
    vel = v3Zero;
    vel.y = -3.0f;
    mv3Velocity = vel;
    mtActiveTimer.m_uPackedTime = 0;
}

/**
 * Offset/Address/Size: 0x2314 | 0x8015B088 | size: 0x1A0
 */
void Bowser::ActionDescend(float fBlendTime)
{
    FORCE_DONT_INLINE;

    mAnimID = 1;
    SetBowserAnimState((eBowserAnim)1, PM_HOLD, fBlendTime);
    meBowserState = BOWSER_STATE_FALL;
}

/**
 * Offset/Address/Size: 0x2080 | 0x8015ADF4 | size: 0x294
 */
void Bowser::ActionFall()
{
    float timerSeconds = g_pGame->m_pGameTweaks->fBowserMinAttackTime;
    nlVector3 vel;

    if (timerSeconds < g_pGame->m_pGameTweaks->fBowserMaxAttackTime)
    {
        timerSeconds += nlRandomf(g_pGame->m_pGameTweaks->fBowserMaxAttackTime - timerSeconds, &nlDefaultSeed);
    }

    mtActiveTimer.SetSeconds(timerSeconds);
    mAnimID = BOWSER_ANIM_LAND;

    cPN_SAnimController* controller = ::new (AllocateSAnimController()) cPN_SAnimController(
        mpAnim[BOWSER_ANIM_LAND], (const AnimRetarget*)0, PM_HOLD, (void (*)(uintptr_t, cPN_SAnimController*))0, (unsigned int)0, (bool)0);

    if (mpFeatherBlender->GetChild(0) != NULL)
    {
        delete mpFeatherBlender->GetChild(0);
    }

    mpFeatherBlender->SetChild(0, controller);
    mpAnimController = controller;

    meBowserState = BOWSER_STATE_FALL;
    mfDesiredSpeed = 0.0f;

    cBaseCamera* camera = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
    mv3TargetPos = camera->GetTargetPosition();

    nlVector3 targetPos = mv3TargetPos;

    float goalLineX = cField::GetGoalLineX(1U);
    float limitX = goalLineX - 3.0f;
    if ((float)fabs(targetPos.x) > limitX)
    {
        if (targetPos.x > 0.0f)
        {
            targetPos.x = limitX;
        }
        else
        {
            targetPos.x = -limitX;
        }
    }

    float sidelineY = cField::GetSidelineY(1U);
    float limitY = sidelineY - 3.0f;
    if ((float)fabs(targetPos.y) > limitY)
    {
        if (targetPos.y < 0.0f)
        {
            targetPos.y = -limitY;
        }
        else
        {
            targetPos.y = limitY;
        }
    }

    targetPos.z = 10.0f;
    targetPos.z = 0.0f;
    targetPos.y += 3.0f;
    SetPosition(targetPos);

    maFacingDirection = 0xC000;
    maDesiredFacingDirection = 0xC000;

    vel = v3Zero;
    vel.y = -3.0f;
    mv3Velocity = vel;
}

/**
 * Offset/Address/Size: 0x1CE4 | 0x8015AA58 | size: 0x39C
 */
void Bowser::ActionJump()
{
    meBowserState = BOWSER_STATE_JUMP;
    mAnimID = BOWSER_ANIM_JUMP;

    SetBowserAnimState(BOWSER_ANIM_JUMP, PM_HOLD, 0.2f);

    nlVector3 v3JumpPos = g_pBall->m_v3Position;
    v3JumpPos.x += nlRandomf(3.0f, &nlDefaultSeed) - 1.5f;

    float fMaxTilt = g_pGame->m_pGameTweaks->fBowserMaxTilt;
    if (v3JumpPos.x > 0.0f && mfYAxisTilt <= -fMaxTilt)
    {
        v3JumpPos.x *= -1.0f;
    }
    else if (v3JumpPos.x < 0.0f && mfYAxisTilt >= fMaxTilt)
    {
        v3JumpPos.x *= -1.0f;
    }

    if (v3JumpPos.y < 0.0f)
    {
        v3JumpPos.y += 1.0f + nlRandomf(3.0f, &nlDefaultSeed);
    }
    else
    {
        v3JumpPos.y -= 1.0f + nlRandomf(3.0f, &nlDefaultSeed);
    }

    float goalLineX = cField::GetGoalLineX(1U);
    float limitX = goalLineX - 3.0f;
    if ((float)fabs(v3JumpPos.x) > limitX)
    {
        if (v3JumpPos.x > 0.0f)
        {
            v3JumpPos.x = limitX;
        }
        else
        {
            v3JumpPos.x = -limitX;
        }
    }

    float sidelineY = cField::GetSidelineY(1U);
    float limitY = sidelineY - 3.0f;
    if ((float)fabs(v3JumpPos.y) > limitY)
    {
        if (v3JumpPos.y < 0.0f)
        {
            v3JumpPos.y = -limitY;
        }
        else
        {
            v3JumpPos.y = limitY;
        }
    }

    nlVector3 v3JumpVel;
    v3JumpVel.x = v3JumpPos.x - mv3Position.x;
    v3JumpVel.y = v3JumpPos.y - mv3Position.y;
    v3JumpVel.z = v3JumpPos.z - mv3Position.z;
    v3JumpVel.z = 0.0f;

    float vz = v3JumpVel.z;
    float vy = v3JumpVel.y;
    float speed = nlGetLength3D(v3JumpVel.x, vy, vz);

    mfDesiredSpeed = 2.2f * speed;
    v3JumpVel.y = vy * 0.5f;
    v3JumpVel.x = v3JumpVel.x * 0.5f;
    v3JumpVel.z = vz * 0.5f;

    maDesiredFacingDirection = (u16)(s32)(10430.378f * nlATan2f(v3JumpVel.y, v3JumpVel.x));
    mv3Velocity = v3JumpVel;
}

/**
 * Offset/Address/Size: 0x1A90 | 0x8015A804 | size: 0x254
 */
void Bowser::ActionHide()
{
    EmissionManager::Destroy((uintptr_t)this, fxGetGroup("bowser_fire"));
    g_pEventManager->CreateValidEvent(0x65, 0x14);

    bool wasVisible = mbIsVisible;
    mbIsVisible = false;
    meBowserState = BOWSER_STATE_HIDDEN;
    mfDesiredSpeed = 0.0f;

    if (mpFeatherBlender->GetChild(1) != NULL)
    {
        cPoseNode* pChild = mpFeatherBlender->GetChild(1);
        delete pChild;
        mpFeatherBlender->SetChild(1, NULL);
    }

    mpFeatherController = NULL;
    SetPosition(gv3BowserHomePosition);

    mv3Velocity = v3Zero;
    maFacingDirection = 0;

    mpPhysObj->DisableCollisions();

    if (!(mAttackType == BOWSER_ATTACK_STOMP && mStompStage != 2))
    {
        eBowserAttackType oldAttackType = mAttackType;

        SetTiltParameters(0.0f);
        mAttackType = BOWSER_ATTACK_ROLL;

        if (g_pGame->m_pGameTweaks->fBowserEndTime < 0.0f)
        {
            g_pGame->ResetBowser();
        }

        if (mbAlive)
        {
            mbAlive = false;

            if (GameInfoManager::Instance()->IsBowserAttackEnabled() && oldAttackType != BOWSER_ATTACK_STOMP && wasVisible)
            {
                g_pEventManager->CreateValidEvent(0x37, 0x14);
            }
        }
    }
    else
    {
        g_pGame->ResetBowserTimer(g_pGame->m_pGameTweaks->fBowserTiltTime);
    }
}

static inline void ResetBowserAttackState(Bowser* pBowser)
{
    Bowser::SetTiltParameters(0.0f);
    pBowser->mAttackType = BOWSER_ATTACK_ROLL;
}

/**
 * Offset/Address/Size: 0x177C | 0x8015A4F0 | size: 0x314
 */
void Bowser::ActionReset()
{
    mAttackType = BOWSER_ATTACK_ROLL;

    if (mbAlive)
    {
        eBowserAttackType oldAttackType;
        bool wasVisible;

        EmissionManager::Destroy((uintptr_t)this, fxGetGroup("bowser_fire"));
        g_pEventManager->CreateValidEvent(0x65, 0x14);

        wasVisible = mbIsVisible;
        mbIsVisible = false;
        meBowserState = BOWSER_STATE_HIDDEN;
        mfDesiredSpeed = 0.0f;

        if (mpFeatherBlender->GetChild(1) != NULL)
        {
            cPoseNode* pChild = mpFeatherBlender->GetChild(1);
            delete pChild;
            mpFeatherBlender->SetChild(1, NULL);
        }

        mpFeatherController = NULL;
        SetPosition(gv3BowserHomePosition);

        mv3Velocity = v3Zero;
        maFacingDirection = 0;

        mpPhysObj->DisableCollisions();

        if (!(mAttackType == BOWSER_ATTACK_STOMP && mStompStage != 2))
        {
            oldAttackType = mAttackType;
            ResetBowserAttackState(this);

            if (g_pGame->m_pGameTweaks->fBowserEndTime < 0.0f)
            {
                g_pGame->ResetBowser();
            }

            if (mbAlive)
            {
                mbAlive = false;

                if (GameInfoManager::Instance()->IsBowserAttackEnabled() && oldAttackType != BOWSER_ATTACK_STOMP && wasVisible)
                {
                    g_pEventManager->CreateValidEvent(0x37, 0x14);
                }
            }
        }
        else
        {
            g_pGame->ResetBowserTimer(g_pGame->m_pGameTweaks->fBowserTiltTime);
        }
    }

    mbFirstTime = true;
    SetTiltParameters(0.0f);
}

/**
 * Offset/Address/Size: 0x1534 | 0x8015A2A8 | size: 0x248
 */
void Bowser::ActionLeave()
{
    if (meBowserState == BOWSER_STATE_LEAVE)
        return;

    if (GameInfoManager::Instance()->IsBowserAttackEnabled())
    {
        g_pEventManager->CreateValidEvent(0x3b, 0x14);
    }

    meBowserState = BOWSER_STATE_LEAVE;
    mAnimID = BOWSER_ANIM_JUMP;

    SetBowserAnimState(BOWSER_ANIM_JUMP, PM_HOLD, 0.2f);

    cBaseCamera* camera = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
    nlVector3 v3CameraTarget = camera->GetTargetPosition();

    nlVector3 v3Velocity = { 25.0f, 0.0f, 25.0f };

    unsigned short aDesired = 0;

    if (v3CameraTarget.x > mv3Position.x)
    {
        v3Velocity.x *= -1.0f;
        aDesired = 0x8000;
    }

    maDesiredFacingDirection = aDesired;
    mv3Velocity = v3Velocity;

    mtStateTimer.SetSeconds(2.0f);

    ClearBowserFeatherAnimState(true);
}

/**
 * Offset/Address/Size: 0x1384 | 0x8015A0F8 | size: 0x1B0
 */
void Bowser::Move(float fDeltaT)
{
    float speed = nlSqrt(mv3Velocity.x * mv3Velocity.x + mv3Velocity.y * mv3Velocity.y + mv3Velocity.z * mv3Velocity.z, true);

    float newSpeed = SeekSpeed(speed, mfDesiredSpeed, 25.0f, 75.0f, fDeltaT);

    maFacingDirection = SeekDirection(maFacingDirection, maDesiredFacingDirection, 60000.0f, 4000.0f, fDeltaT);

    nlVector3 vel;
    nlVector3 newPos;
    nlPolarToCartesian(vel.x, vel.y, maDesiredFacingDirection, newSpeed);
    vel.z = mv3Velocity.z;
    mv3Velocity = vel;

    newPos = mv3Position;
    newPos.x += vel.x * fDeltaT;
    newPos.y += vel.y * fDeltaT;

    float goalLineX = cField::GetGoalLineX(1U);
    float limitX = goalLineX - 3.0f;
    if ((float)fabs(newPos.x) > limitX)
    {
        if (newPos.x > 0.0f)
        {
            newPos.x = limitX;
        }
        else
        {
            newPos.x = -limitX;
        }
    }

    float sidelineY = cField::GetSidelineY(1U);
    float limitY = sidelineY - 3.0f;
    if ((float)fabs(newPos.y) > limitY)
    {
        if (newPos.y < 0.0f)
        {
            newPos.y = -limitY;
        }
        else
        {
            newPos.y = limitY;
        }
    }

    SetPosition(newPos);
}

eBowserMoveResult Bowser::GravityMove(float fDeltaT)
{
    float fHalfGrav = 0.5f * g_pGame->m_pGameTweaks->fBowserGravity;
    nlVector3 v3Vel = mv3Velocity;
    nlVector3 v3Pos = mv3Position;
    eBowserMoveResult result = BOWSER_MOVE_RESULT_NORMAL;

    if ((float)fabs(v3Vel.z) < 1.0f && v3Pos.z < 0.05f)
    {
        v3Pos.z = 0.0f;
        v3Vel.z = 0.0f;
        result = BOWSER_MOVE_RESULT_AT_REST;
    }
    else
    {
        v3Pos.z += (v3Vel.z + fHalfGrav * fDeltaT) * fDeltaT;
        v3Vel.z += fDeltaT * g_pGame->m_pGameTweaks->fBowserGravity;
        if (v3Pos.z < 0.0f)
        {
            v3Pos.z = 0.0f;
            v3Vel.z *= g_pGame->m_pGameTweaks->fBowserRebound;
            result = BOWSER_MOVE_RESULT_REBOUND;
        }
    }

    v3Pos.x += v3Vel.x * fDeltaT;
    v3Pos.y += v3Vel.y * fDeltaT;
    SetPosition(v3Pos);
    mv3Velocity = v3Vel;

    return result;
}

/**
 * Offset/Address/Size: 0x1114 | 0x80159E88 | size: 0x270
 */
#pragma dont_inline on
void Bowser::ActionIdle()
{
    if (CheckForAbort())
    {
        return;
    }

    meBowserState = BOWSER_STATE_IDLE;

    if (mAnimID != BOWSER_ANIM_WALK)
    {
        mAnimID = BOWSER_ANIM_WALK;
        SetBowserAnimState(BOWSER_ANIM_WALK, PM_CYCLIC, 0.15f);
    }

    nlVector3 pos = mv3Position;
    pos.z = 0.0f;
    SetPosition(pos);

    mv3Velocity = v3Zero;

    mtStateTimer.SetSeconds(g_pGame->m_pGameTweaks->fBowserIntervalDelay);

    EmissionManager::Destroy((uintptr_t)this, fxGetGroup("bowser_fire"));
    g_pEventManager->CreateValidEvent(0x65, 0x14);

    EmissionController* controller = EmissionManager::Create(fxGetGroup("bowser_fire"), 0);
    controller->m_uUserData = (uintptr_t)this;
    controller->SetUpdateCallback(Function<EmissionController&>(UpdateFireEmitter));

    g_pEventManager->CreateValidEvent(0x64, 0x14);
}
#pragma dont_inline reset

/**
 * Offset/Address/Size: 0x1044 | 0x80159DB8 | size: 0xD0
 */
void Bowser::SetTiltParameters(float fYAxisTilt)
{
    mfYAxisTilt = fYAxisTilt;
    cCameraManager::SetWorldUpVectorTilt(0.0f, fYAxisTilt);

    if ((g_pBall != NULL) && (g_pBall->m_pPhysicsBall != NULL))
    {
        if ((float)fabs(fYAxisTilt) > 0.01f)
        {
            nlVector3 tiltForce = { 0.0f, 0.0f, 0.0f };
            tiltForce.x = -fYAxisTilt * g_pGame->m_pGameTweaks->fBowserBallForceMult;

            g_pBall->m_pPhysicsBall->m_v3TiltForce = tiltForce;
            g_pBall->m_pPhysicsBall->m_bUseTiltForce = true;
            return;
        }
        g_pBall->m_pPhysicsBall->m_bUseTiltForce = false;
    }
}

/**
 * Offset/Address/Size: 0x954 | 0x801596C8 | size: 0x6F0
 */
bool Bowser::CheckForAbort()
{
    Bowser* const pBowser = this;
    bool isGameplayOrOvertime = false;
    eGameState gameState = g_pGame->m_eGameState;
    if (gameState == GS_GAMEPLAY || gameState == GS_OVERTIME)
        isGameplayOrOvertime = true;

    if (!isGameplayOrOvertime)
    {
        pBowser->ActionLeave();
        return true;
    }
    if (gameState == GS_END_GAME)
    {
        EmissionManager::Destroy((uintptr_t)pBowser, fxGetGroup("bowser_fire"));
        g_pEventManager->CreateValidEvent(0x65, 0x14);
        eBowserAttackType savedAttackType;
        bool savedVisible = pBowser->mbIsVisible;
        pBowser->mbIsVisible = false;
        pBowser->meBowserState = BOWSER_STATE_HIDDEN;
        pBowser->mfDesiredSpeed = 0.0f;
        if (pBowser->mpFeatherBlender->GetChild(1) != NULL)
        {
            delete pBowser->mpFeatherBlender->GetChild(1);
            pBowser->mpFeatherBlender->SetChild(1, NULL);
        }
        pBowser->mpFeatherController = NULL;
        pBowser->SetPosition(gv3BowserHomePosition);
        pBowser->mv3Velocity = v3Zero;
        pBowser->maFacingDirection = 0;
        pBowser->mpPhysObj->DisableCollisions();
        if (!(pBowser->mAttackType == BOWSER_ATTACK_STOMP && pBowser->mStompStage != 2))
        {
            savedAttackType = pBowser->mAttackType;
            ResetBowserAttackState(pBowser);
            if (g_pGame->m_pGameTweaks->fBowserEndTime < 0.0f)
                g_pGame->ResetBowser();
            if (pBowser->mbAlive)
            {
                pBowser->mbAlive = false;
                if (nlSingleton<GameInfoManager>::Instance()->IsBowserAttackEnabled() && savedAttackType != BOWSER_ATTACK_STOMP && savedVisible)
                    g_pEventManager->CreateValidEvent(0x37, 0x14);
            }
        }
        else
        {
            g_pGame->ResetBowserTimer(g_pGame->m_pGameTweaks->fBowserTiltTime);
        }
        return true;
    }
    float fDuration = g_pGame->m_fGameDuration;
    float fRemainingTime = fDuration - g_pGame->GetGameTime();
    if (fRemainingTime < 15.0f && pBowser->mAttackType != BOWSER_ATTACK_STOMP)
    {
        pBowser->ActionLeave();
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x888 | 0x801595FC | size: 0xCC
 */
void Bowser::UpdateFireEmitter(EmissionController& controller)
{
    if (ReplayManager::Instance()->mRender != nullptr)
    {
        RenderSnapshot* pRenderSnapshot = ReplayManager::Instance()->mRender;
        cPoseAccumulator* pPoseAccumulator = pRenderSnapshot->mBowser.mPoseAccumulator;

        controller.SetPosition(pRenderSnapshot->mBowser.mPosition);
        controller.SetVelocity(pRenderSnapshot->mBowser.mVelocity);
        controller.SetPoseAccumulator(*pPoseAccumulator);

        static unsigned int HeadJointID = nlStringHash("bip01 head");

        nlMatrix4& headMatrix = pPoseAccumulator->GetNodeMatrixByHashID(HeadJointID);
        nlVector3 direction;
        // = headMatrix.GetTranslation();
        nlVec3Set(direction, headMatrix.m21, headMatrix.m22, headMatrix.m23);
        // direction.x = headMatrix.m21; // row 1, column 0
        // direction.y = headMatrix.m22; // row 1, column 1
        // direction.z = headMatrix.m23; // row 1, column 2
        controller.SetDirection(direction);
    }
}

/**
 * Offset/Address/Size: 0x810 | 0x80159584 | size: 0x78
 */
void Bowser::UpdateBowserLandEmitter(EmissionController& controller)
{
    if (ReplayManager::Instance()->mRender != nullptr)
    {
        RenderSnapshot* pRenderSnapshot = ReplayManager::Instance()->mRender;
        cPoseAccumulator* pPoseAccumulator = pRenderSnapshot->mBowser.mPoseAccumulator;

        controller.SetPosition(pRenderSnapshot->mBowser.mPosition);
        controller.SetVelocity(pRenderSnapshot->mBowser.mVelocity);
        controller.SetPoseAccumulator(*pPoseAccumulator);
    }
}

void Bowser::EmitFire()
{
    EffectsGroup* pGroup = fxGetGroup("bowser_fire");
    EmissionController* pControl = EmissionManager::Create(pGroup, 0);
    pControl->m_uUserData = (uintptr_t)this;
    pControl->SetUpdateCallback(Function<EmissionController&>(UpdateFireEmitter));
    g_pEventManager->CreateValidEvent(0x64, 0x14);
}

void Bowser::KillFire()
{
    const EffectsGroup* pGroup = fxGetGroup("bowser_fire");
    EmissionManager::Destroy((uintptr_t)this, pGroup);
    g_pEventManager->CreateValidEvent(0x65, 0x14);
}

/**
 * Offset/Address/Size: 0x7A4 | 0x80159518 | size: 0x6C
 */
void Bowser::FindTarget()
{
    if (g_pBall->GetOwnerFielder() != NULL && !g_pBall->GetOwnerFielder()->IsInvincible())
    {
        mpTarget = g_pBall->GetOwnerFielder(); // at 0x0EC
    }
    else
    {
        mpTarget = FindPowerupTarget((cFielder*)NULL, this);
    }
}

/**
 * Offset/Address/Size: 0x334 | 0x801590A8 | size: 0x470
 */
#pragma dont_inline on
void Bowser::SetupBaseSFX()
{
    if (!AudioLoader::IsInited())
    {
        return;
    }

    m_pCharacterSFX->Init();
    m_pCharacterSFX->mGroup = 8;
    m_pCharacterSFX->mpPhysObj = (PhysicsObject*)mpPhysObj;
    m_pCharacterSFX->SetSFX(gpBOWSERSoundPropAccessor);
    AudioLoader::SetupBowserStadiumSoundTable(this);

    mpAnim[1]->CreateCallback(8.2f / mpAnim[1]->m_nNumKeys, 0x5A, AnimSoundCallback);
    mpAnim[1]->CreateCallback(18.3f / mpAnim[1]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[1]->CreateCallback(18.3f / mpAnim[1]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[1]->CreateCallback(25.0f / mpAnim[1]->m_nNumKeys, 0x60, AnimSoundCallback);
    mpAnim[3]->CreateCallback(3.0f / mpAnim[3]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[3]->CreateCallback(25.5f / mpAnim[3]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[2]->CreateCallback(8.0f / mpAnim[2]->m_nNumKeys, 0x5B, AnimSoundCallback);
    mpAnim[2]->CreateCallback(16.0f / mpAnim[2]->m_nNumKeys, 0x5C, AnimSoundCallback);
    mpAnim[5]->CreateCallback(13.0f / mpAnim[5]->m_nNumKeys, 0x60, AnimSoundCallback);
    mpAnim[6]->CreateCallback(3.0f / mpAnim[6]->m_nNumKeys, 0x5B, AnimSoundCallback);
    mpAnim[6]->CreateCallback(13.0f / mpAnim[6]->m_nNumKeys, 0x5D, AnimSoundCallback);
    mpAnim[6]->CreateCallback(22.7f / mpAnim[6]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[6]->CreateCallback(22.7f / mpAnim[6]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[6]->CreateCallback(32.0f / mpAnim[6]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[6]->CreateCallback(32.8f / mpAnim[6]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[4]->CreateCallback(15.7f / mpAnim[4]->m_nNumKeys, 0x5F, AnimSoundCallback);
    mpAnim[4]->CreateCallback(20.0f / mpAnim[4]->m_nNumKeys, 0x63, AnimSoundCallback);
    mpAnim[4]->CreateCallback(39.0f / mpAnim[4]->m_nNumKeys, 0x5F, AnimSoundCallback);
}
#pragma dont_inline reset

/**
 * Offset/Address/Size: 0x290 | 0x80159004 | size: 0xA4
 */
void Bowser::PlaySFX(Audio::eCharSFX type, PosUpdateMethod posUpdateMethod, float fDelay, bool bIs3D)
{
    Audio::SoundAttributes attrs;
    attrs.Init();
    attrs.SetSoundType(type, bIs3D);
    attrs.mf_DelayTime = fDelay;
    attrs.posUpdateMethod = posUpdateMethod;

    if (posUpdateMethod == VECTORS)
    {
        attrs.UseStationaryPosVector(mv3Position);
    }

    m_pCharacterSFX->Play(attrs);
}

void Bowser::SetBowserAnimState(eBowserAnim anim, ePlayMode playMode, float fBlendTime)
{
    cPN_SAnimController* controller = ::new (AllocateSAnimController()) cPN_SAnimController(mpAnim[anim], (const AnimRetarget*)0, playMode, (void (*)(uintptr_t, cPN_SAnimController*))0, (unsigned int)0, (bool)0);

    cPoseNode* pNewPoseTree;

    if (mpFeatherBlender->GetChild(0) != NULL)
    {
        if (fBlendTime > 0.0f)
        {
            pNewPoseTree = ::new (AllocateBlender()) cPN_Blender(*mpFeatherBlender->GetChildPtr(0), controller, fBlendTime);
        }
        else
        {
            delete mpFeatherBlender->GetChild(0);
            pNewPoseTree = controller;
        }
    }
    else
    {
        pNewPoseTree = controller;
    }

    mpFeatherBlender->SetChild(0, pNewPoseTree);
    mpAnimController = controller;
}

void Bowser::SetBowserFeatherAnimState(eBowserAnim anim, float fBlendTime)
{
    if (mpFeatherBlender->GetChild(1) != NULL)
    {
        delete mpFeatherBlender->GetChild(1);
        mpFeatherBlender->SetChild(1, NULL);
    }

    mpFeatherController = ::new (AllocateSAnimController()) cPN_SAnimController(
        mpAnim[anim], (const AnimRetarget*)0, PM_HOLD, (void (*)(uintptr_t, cPN_SAnimController*))0, (unsigned int)0, (bool)0);
    mpFeatherBlender->ClearNodeWeights();
    mpFeatherBlender->SetNodeWeight(0xE, 1.0f, 0.2f);
    mpFeatherBlender->SetChild(1, mpFeatherController);
    mpFeatherBlender->BeginBlendIn(fBlendTime);
}

void Bowser::ClearBowserFeatherAnimState(bool bBlendOut)
{
    if (bBlendOut)
    {
        if (mpFeatherBlender->GetChild(1) != NULL)
        {
            mpFeatherBlender->BeginBlendOut(0.1f);
        }
    }
    else
    {
        if (mpFeatherBlender->GetChild(1) != NULL)
        {
            delete mpFeatherBlender->GetChild(1);
            mpFeatherBlender->SetChild(1, NULL);
        }
    }

    mpFeatherController = NULL;
}

/**
 * Offset/Address/Size: 0x9C | 0x80158E10 | size: 0x1F4
 */
void Bowser::CheckFootSteps()
{
    FORCE_DONT_INLINE;

    if (mAnimID != 3)
        return;

    if (!mpAnimController->TestTrigger(0.08f) && !mpAnimController->TestTrigger(0.58f))
        return;

    if (mbDoTilt)
    {
        f32 fNewTilt = mfYAxisTilt;
        GameTweaks* pTweaks = g_pGame->m_pGameTweaks;
        f32 fTiltForce = pTweaks->fBowserMinTiltForce + nlRandomf(pTweaks->fBowserMaxTiltForce - pTweaks->fBowserMinTiltForce, &nlDefaultSeed);

        if (mAttackType != BOWSER_ATTACK_STOMP)
        {
            fNewTilt -= 0.5f * (fTiltForce * mv3Position.x);
        }
        else
        {
            fNewTilt -= fTiltForce * mv3Position.x;
        }

        f32 fMaxTilt = g_pGame->m_pGameTweaks->fBowserMaxTilt;
        if (fNewTilt > fMaxTilt)
        {
            fNewTilt = fMaxTilt;
        }
        else if (fNewTilt < -fMaxTilt)
        {
            fNewTilt = -fMaxTilt;
        }

        if (mAttackType == BOWSER_ATTACK_STOMP && mStompStage == 2)
        {
            if (mfYAxisTilt * fNewTilt < 0.0f)
            {
                fNewTilt = 0.0f;
                mbDoTilt = false;
            }
        }

        SetTiltParameters(fNewTilt);
    }

    NetMesh::spPositiveXNetMesh->JoltNet(0.02f);
    NetMesh::spNegativeXNetMesh->JoltNet(0.02f);
    FireCameraRumbleFilter(0.0f, 0.05f);
}

bool Bowser::CheckTargetBounds(nlVector3& v3Target)
{
    bool bClipped = false;

    float fXLimit = cField::GetGoalLineX(1U) - 3.0f;
    if (fabsf(v3Target.x) > fXLimit)
    {
        if (v3Target.x > 0.0f)
        {
            v3Target.x = fXLimit;
        }
        else
        {
            v3Target.x = -fXLimit;
        }
        bClipped = true;
    }

    float fYLimit = cField::GetSidelineY(1U) - 3.0f;
    if (fabsf(v3Target.y) > fYLimit)
    {
        if (v3Target.y < 0.0f)
        {
            v3Target.y = -fYLimit;
        }
        else
        {
            v3Target.y = fYLimit;
        }
        bClipped = true;
    }

    return bClipped;
}

/**
 * Offset/Address/Size: 0x60 | 0x80158DD4 | size: 0x3C
 */
float Bowser::GetHeadSpin() const
{
    return (float)(unsigned short)(int)this->mpHeadTrack->m_fHeadSpin;
}

/**
 * Offset/Address/Size: 0x24 | 0x80158D98 | size: 0x3C
 */
float Bowser::GetHeadTilt() const
{
    return (float)(unsigned short)(int)this->mpHeadTrack->m_fHeadTilt;
}

/**
 * Offset/Address/Size: 0x0 | 0x80158D74 | size: 0x24
 */
void Bowser::DrawShadow(const cPoseAccumulator& poseAccumulator, const nlMatrix4& matrix)
{
    SkinAnimatedNPC::DrawShadow(mpLastModel, matrix);
}
