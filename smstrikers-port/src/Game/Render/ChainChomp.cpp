#include "Game/Render/ChainChomp.h"
#include "NL/vmath.h"

#include "Game/Game.h"
#include "Game/Player.h"
#include "Game/ReplayManager.h"
#include "Game/Effects/EmissionController.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/AI/Powerups.h"
#include "Game/Physics/PhysicsShell.h"
#include "Game/Physics/PhysicsBanana.h"
#include "Game/Physics/PhysicsCharacter.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/AI/AiUtil.h"
#include "NL/gl/glDraw3.h"
#include "NL/gl/glState.h"
#include "NL/glx/glxTexture.h"
#include "NL/nlString.h"
#include "Game/Camera/animcam.h"

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
    unsigned long Play(Audio::eWorldSFX, float, float, bool, float);
};

extern cWorldSFX gPowerupSFX;
extern cWorldSFX gStadGenSFX;
} // namespace Audio

void FireCameraRumbleFilter(float, float);

static cAnimCamera sNisCam;

static unsigned char gbChainChompProjectedShadow;

const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };
const nlVector3 gv3HomePosition = { 0.0f, 0.0f, -10.0f };

/**
 * Offset/Address/Size: 0x12BC | 0x8015EFC0 | size: 0x1D8
 */
ChainChomp::ChainChomp(cSHierarchy& pHierarchy, int nModelID, PhysicsNPC& rPhysObj, cInventory<cSAnim>* pInventorySAnim)
    : SkinAnimatedMovableNPC(pHierarchy, nModelID, rPhysObj)
    , mpTarget(NULL)
    , meChainChompState(CHAIN_STATE_HIDDEN)
{
    mpInEffectSFX = NULL;
    mtStateTimer.SetSeconds(0.0f);

    mpDropAnim = pInventorySAnim->Find((unsigned int)nlStringHash("drop"));
    mpIdleAnim = pInventorySAnim->Find((unsigned int)nlStringHash("chain_idle"));
    mpRecoverAnim = pInventorySAnim->Find((unsigned int)nlStringHash("landing"));

    mpPhysObj->mpAINPC = this;

    if (mpInEffectSFX != NULL)
    {
        PowerupBase::StopPowerupInEffectSound(mpInEffectSFX);
        mpInEffectSFX = NULL;
    }

    EmissionManager::Destroy((uintptr_t)this, fxGetGroup("chainchomp_trail"));
    meChainChompState = CHAIN_STATE_HIDDEN;
    mfDesiredSpeed = 0.0f;
    SetAnimState(*mpIdleAnim, 0.0f, PM_CYCLIC);

    SetPosition(gv3HomePosition);
    mv3Velocity = v3Zero;
    maFacingDirection = 0;
    mpPhysObj->DisableCollisions();

    mpThrower = NULL;
    mnThrowerPadID = -1;
    mbIsVisible = false;
}

/**
 * Offset/Address/Size: 0x125C | 0x8015EF60 | size: 0x60
 */
ChainChomp::~ChainChomp()
{
}

/**
 * Offset/Address/Size: 0xAA8 | 0x8015E7AC | size: 0x7B4
 */
void ChainChomp::Update(float fDeltaT)
{
    nlVector3 v3RecoverPosition;
    nlVector3 v3UpdatedPosition;
    nlVector3 v3FallPosition;

    switch (meChainChompState)
    {
    case CHAIN_STATE_FALL:
        if (mtStateTimer.m_uPackedTime == 0)
        {
            break;
        }

        if (mpTarget != NULL)
        {
            maDesiredFacingDirection = (u16)(s32)(10430.378f * nlATan2f(mpTarget->m_v3Position.y - mv3Position.y, mpTarget->m_v3Position.x - mv3Position.x));
        }

        Move(fDeltaT);

        if (mtStateTimer.Countdown(fDeltaT, 0.0f))
        {
            meChainChompState = CHAIN_STATE_RECOVER;
            SetAnimState(*mpRecoverAnim, 0.0f, PM_HOLD);

            v3FallPosition = mv3Position;
            v3FallPosition.z = 0.0f;
            SetPosition(v3FallPosition);
            mv3Velocity = v3Zero;

            Event* pEvent = g_pEventManager->CreateValidEvent(0x33, 0x1C);
            ShotAtGoalData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) ShotAtGoalData();
            pData->pShooter = mpThrower;
        }
        break;

    case CHAIN_STATE_RECOVER:
        if (mpAnimController->m_fTime < 0.625)
        {
            v3RecoverPosition = mpTarget->m_v3Position;
            v3RecoverPosition.z = 0.0f;
            SetPosition(v3RecoverPosition);
        }

        if (mpAnimController->TestTrigger(0.75f))
        {
            Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x8D, cGameSFX::SFX_STOP_FIRST);
            mpPhysObj->EnableCollisions();

            EmissionController* pControl = EmissionManager::Create(fxGetGroup("chainchomp_land"), 0);
            pControl->SetPosition(mv3Position);

            PowerupBase::PlayPowerupSound(POWER_UP_CHAIN_CHOMP, PowerupBase::PWRUP_SOUND_BOUNCE_GROUND, mpPhysObj, 100.0f);
            Audio::gStadGenSFX.Play((Audio::eWorldSFX)0xCE, 100.0f, -1.0f, true, 100.0f);

            EmitTrail();

            FireCameraRumbleFilter(0.0f, 0.2f);
        }
        else
        {
            bool bRecoverDone = false;
            if (mpAnimController->m_ePlayMode == PM_HOLD && mpAnimController->m_fTime == 1.0f)
            {
                bRecoverDone = true;
            }

            if (bRecoverDone)
            {
                PowerupBase::PlayPowerupSound(POWER_UP_CHAIN_CHOMP, PowerupBase::PWRUP_SOUND_END, mpPhysObj, 100.0f);
                meChainChompState = CHAIN_STATE_CHASE;
                SetAnimState(*mpIdleAnim, 0.0f, PM_CYCLIC);
                mtStateTimer.SetSeconds(g_pGame->m_pGameTweaks->fChainChompActiveTime);
            }
        }
        break;

    case CHAIN_STATE_CHASE:
        if (mtStateTimer.m_uPackedTime == 0)
        {
            break;
        }

        if (mpTarget != NULL)
        {
            maDesiredFacingDirection = (u16)(s32)(10430.378f * nlATan2f(mpTarget->m_v3Position.y - mv3Position.y, mpTarget->m_v3Position.x - mv3Position.x));
        }

        Move(fDeltaT);

        if (mpInEffectSFX == NULL && !g_pGame->mbCaptainShotToScoreOn)
        {
            Audio::SoundAttributes sndAtr;
            sndAtr.Init();
            sndAtr.SetSoundType(0x8E, true);
            sndAtr.UseVectors(mv3Position, mv3Velocity);
            sndAtr.mf_ReturnEmitterOnPlay = true;
            mpInEffectSFX = (SFXEmitter*)Audio::gPowerupSFX.Play(sndAtr);
        }

        {
            static float fBarkT;
            static signed char init;
            if (!init)
            {
                init = 1;
                fBarkT = 0.0f;
            }

            fBarkT += fDeltaT;
            if (fBarkT > 0.7f)
            {
                Audio::SoundAttributes sndAtr;
                sndAtr.Init();
                sndAtr.SetSoundType(0x91, true);
                sndAtr.UseStationaryPosVector(mv3Position);
                Audio::gPowerupSFX.Play(sndAtr);
                fBarkT = 0.0f;
            }
        }

        if (mtStateTimer.Countdown(fDeltaT, 0.0f))
        {
            Leave();
        }
        break;

    case CHAIN_STATE_LEAVE:
        Move(fDeltaT);

        {
            static float fBarkT;
            static signed char init;
            if (!init)
            {
                init = 1;
                fBarkT = 0.0f;
            }

            fBarkT += fDeltaT;
            if (fBarkT > 0.7f)
            {
                Audio::SoundAttributes sndAtr;
                sndAtr.Init();
                sndAtr.SetSoundType(0x91, true);
                sndAtr.UseStationaryPosVector(mv3Position);
                Audio::gPowerupSFX.Play(sndAtr);
                fBarkT = 0.0f;
            }
        }

        if (fabsf(mv3Position.x) > 22.5f)
        {
            Event* pEvent = g_pEventManager->CreateValidEvent(0x34, 0x1C);
            ShotAtGoalData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) ShotAtGoalData();
            pData->pShooter = mpThrower;

            if (mpInEffectSFX != NULL)
            {
                PowerupBase::StopPowerupInEffectSound(mpInEffectSFX);
                mpInEffectSFX = NULL;
            }

            EmissionManager::Kill((uintptr_t)this, fxGetGroup("chainchomp_trail"));

            meChainChompState = CHAIN_STATE_HIDDEN;
            mfDesiredSpeed = 0.0f;
            SetAnimState(*mpIdleAnim, 0.0f, PM_CYCLIC);
            SetPosition(gv3HomePosition);
            mv3Velocity = v3Zero;
            maFacingDirection = 0;
            mpPhysObj->DisableCollisions();
            mpThrower = NULL;
            mnThrowerPadID = -1;
            mbIsVisible = false;
        }
        break;

    case CHAIN_STATE_HIDDEN:
    default:
        break;
    }

    {
        v3UpdatedPosition.x = mv3Position.x + fDeltaT * mv3Velocity.x;
        v3UpdatedPosition.y = mv3Position.y + fDeltaT * mv3Velocity.y;
        v3UpdatedPosition.z = mv3Position.z + fDeltaT * mv3Velocity.z;
        SetPosition(v3UpdatedPosition);
    }

    if (mpInEffectSFX != NULL)
    {
        static nlVector3 vPos;
        static nlVector3 vDir;

        vPos = mv3Position;
        vDir = mv3Velocity;

        mpInEffectSFX->pos.vPos = vPos;
        mpInEffectSFX->dir.vDir = vDir;
    }

    SkinAnimatedNPC::Update(fDeltaT);
}

/**
 * Offset/Address/Size: 0x97C | 0x8015E680 | size: 0x12C
 */
void ChainChomp::CollisionCallback(PhysicsObject* pPhysObj, PhysicsObject* pObjA, const nlVector3& v3Pos)
{
    ChainChomp* pChainChomp = (ChainChomp*)((PhysicsNPC*)pPhysObj)->mpAINPC;
    cFielder* pFielder = NULL;

    switch (pObjA->GetObjectType())
    {
    case 0x04:
    case 0x0D:
    case 0x0E:
        pFielder = (cFielder*)((PhysicsCharacter*)pObjA->m_parentObject)->m_pAICharacter;
        break;
    case 0x0F:
    {
        cBall* pBall = ((PhysicsAIBall*)pObjA)->m_pAIBall;
        if (pBall->m_pOwner != NULL)
        {
            pFielder = (cFielder*)pBall->m_pOwner;
        }
        else
        {
            pBall->ClearPassTarget();
            pBall->ClearShotInProgress();
        }
        break;
    }
    case 0x13:
        ((PhysicsShell*)pObjA)->m_pPowerupObject->m_bShouldDestroy = true;
        break;
    case 0x14:
        ((PhysicsBanana*)pObjA)->m_pPowerupObject->m_bShouldDestroy = true;
        break;
    }

    if (pFielder != NULL && pFielder->m_eClassType == FIELDER && !pFielder->IsFallenDown(0.0f))
    {
        Event* pEvent = g_pEventManager->CreateValidEvent(0x2F, 0x20);
        CollisionChainPlayerData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) CollisionChainPlayerData();
        pData->pFielder = pFielder;
        pData->pChain = pChainChomp;
    }
}

/**
 * Offset/Address/Size: 0x8E8 | 0x8015E5EC | size: 0x94
 */
void UpdateChainEmitter(EmissionController& controller)
{
    if (ReplayManager::Instance()->mRender != nullptr)
    {
        {
            ReplayManager* replayManager = ReplayManager::Instance();
            controller.SetPosition(replayManager->mRender->mChainChomp.mPosition);
        }
        {
            ReplayManager* replayManager = ReplayManager::Instance();
            controller.SetVelocity(replayManager->mRender->mChainChomp.mVelocity);
        }
        nlVector3 direction = { 0.0f, 0.0f, 1.0f };
        controller.SetDirection(direction);
    }
}

void ChainChomp::EmitTrail()
{
    EffectsTemplate* pTemplate;
    EffectsGroup* pGroup = fxGetGroup("chainchomp_trail");
    EffectsSpec* pSpec = pGroup->m_specs;
    int numSpecs = pGroup->m_numSpecs;
    if (pSpec != NULL && numSpecs > 0)
    {
        int i;
        for (i = numSpecs; i > 0; i--)
        {
            pTemplate = pSpec->m_pTemplate;
            if (pTemplate != NULL)
            {
                pTemplate->m_fFountainLife = 12.0f;
            }
            pSpec++;
        }
    }

    EmissionController* pControl = EmissionManager::Create(pGroup, 0);
    pControl->m_uUserData = (uintptr_t)this;

    pControl->SetUpdateCallback(Function<EmissionController&>(UpdateChainEmitter));
}

static inline void ChainChompTargetScore(ChainChomp* pChomp, cFielder* pCandidate, float& rTempScore)
{
    float dy;
    float dx;

    dy = pCandidate->m_v3Position.y - pChomp->mv3Position.y;
    dx = pCandidate->m_v3Position.x - pChomp->mv3Position.x;
    float fDist = nlSqrt(dx * dx + dy * dy, true);
    float fConverted = 10430.378f * nlATan2f(dy, dx);
    s16 angleDiff = (s16)(pChomp->maFacingDirection - (u16)(s32)fConverted);
    u16 absDelta = (u16)((angleDiff < 0) ? -angleDiff : angleDiff);
    float fDistWeight = 1.0f;
    float fAngleWeighting = g_pGame->m_pGameTweaks->fAngleWeighting;

    rTempScore = fDist * (fDistWeight - fAngleWeighting) + (float)absDelta * fAngleWeighting;
}

/**
 * Offset/Address/Size: 0x608 | 0x8015E30C | size: 0x2E0
 */
#pragma opt_propagation off

void ChainChomp::FindTarget(cTeam* pTeam)
{
    float fTempScore;
    float fBestScore = 99999.9f;
    cFielder* pBestCandidate = NULL;
    cFielder* pFielder;
    cFielder* pFielder2;
    int i;
    cFielder* pCandidate;

    if (g_pBall->GetOwnerFielder() != NULL)
    {
        pFielder = g_pBall->GetOwnerFielder();

        if (pFielder->IsOnSameTeam(pTeam->GetStriker()) && !pFielder->IsFallenDown(0.0f) && pFielder != mpTarget)
        {
            pBestCandidate = g_pBall->GetOwnerFielder();
        }
    }
    else if (g_pBall->GetPassTargetFielder() != NULL)
    {
        pFielder2 = (cFielder*)g_pBall->GetPassTargetFielder();

        if (pFielder2->IsOnSameTeam(pTeam->GetStriker()) && !pFielder2->IsFallenDown(0.0f) && pFielder2 != mpTarget)
        {
            pBestCandidate = pFielder2;
        }
    }

    if (pBestCandidate == NULL)
    {
        for (i = 0; i < 4; i++)
        {
            fTempScore = 999999.9f;
            pCandidate = pTeam->GetFielder(i);

            if (!pCandidate->IsFallenDown(0.0f) && pCandidate != mpTarget)
            {
                ChainChompTargetScore(this, pCandidate, fTempScore);
            }

            if (fTempScore < fBestScore)
            {
                pBestCandidate = pCandidate;
                fBestScore = fTempScore;
            }
        }
    }

    if (pBestCandidate == NULL)
    {
        mtStateTimer.SetSeconds(0.0f);
        Leave();
        pBestCandidate = pTeam->GetStriker();
    }

    mpTarget = pBestCandidate;
}

#pragma opt_propagation reset

/**
 * Offset/Address/Size: 0x4B4 | 0x8015E1B8 | size: 0x154
 */
void ChainChomp::Fall(cFielder* pThrower, cFielder* pTarget)
{
    class nlVector3 v3StartPosition;
    class nlVector3 v3FallVelocity;

    mbIsVisible = true;
    SetAnimState(*mpDropAnim, 0.2f, PM_CYCLIC);
    mpTarget = pTarget;
    mfDesiredSpeed = g_pGame->m_pGameTweaks->fChainChompSpeed;
    PowerupBase::PlayPowerupSound(POWER_UP_CHAIN_CHOMP, PowerupBase::PWRUP_SOUND_ACTIVATE, mpPhysObj, 100.0f);
    meChainChompState = CHAIN_STATE_FALL;

    v3StartPosition = mpTarget->m_v3Position;
    v3StartPosition.z = 100.0f;
    SetPosition(v3StartPosition);

    nlVec3Set(v3FallVelocity, 0.0f, 0.0f, -(75.0f / g_pGame->m_pGameTweaks->fChainChompFallTime));
    mv3Velocity = v3FallVelocity;

    mtStateTimer.SetSeconds(g_pGame->m_pGameTweaks->fChainChompFallTime);
    mpThrower = pThrower;

    bool bHasGlobalPad = pThrower->GetGlobalPad() != nullptr;
    mnThrowerPadID = bHasGlobalPad ? pThrower->GetGlobalPad()->m_padIndex : -1;

    mpPhysObj->DisableCollisions();
}

/**
 * Offset/Address/Size: 0x3BC | 0x8015E0C0 | size: 0xF8
 */
void ChainChomp::Hide(bool bIsEndGame)
{
    if (mpInEffectSFX != nullptr)
    {
        PowerupBase::StopPowerupInEffectSound(mpInEffectSFX);
        mpInEffectSFX = nullptr;
    }

    EffectsGroup* pEffectsGroup = fxGetGroup("chainchomp_trail"); // Chain chomp effects group

    if (!bIsEndGame)
    {
        EmissionManager::Kill((uintptr_t)this, pEffectsGroup);
    }
    else
    {
        EmissionManager::Destroy((uintptr_t)this, pEffectsGroup);
    }

    meChainChompState = CHAIN_STATE_HIDDEN;
    mfDesiredSpeed = 0.0f;

    SetAnimState(*mpIdleAnim, 0.0f, PM_CYCLIC);
    SetPosition(gv3HomePosition);

    mv3Velocity = v3Zero;
    maFacingDirection = 0;

    mpPhysObj->DisableCollisions();

    mpThrower = nullptr;
    mnThrowerPadID = -1;
    mbIsVisible = false;
}

/**
 * Offset/Address/Size: 0x3AC | 0x8015E0B0 | size: 0x10
 */
bool ChainChomp::IsHidden() const
{
    return meChainChompState == CHAIN_STATE_HIDDEN;
}

bool ChainChomp::AvoidSidelines()
{
    nlVector3 v3WallPosition = GetClosestPointOnSidelines(mv3Position);
    float dy = v3WallPosition.y - mv3Position.y;
    float dx = v3WallPosition.x - mv3Position.x;
    float fDist = nlSqrt(dx * dx + dy * dy, true);
    if (fDist < 3.0f)
    {
        return true;
    }
    return false;
}

void ChainChomp::Leave()
{
    float fY;
    float fTargetX;
    float fTargetY;

    meChainChompState = CHAIN_STATE_LEAVE;

    if ((0.5f * g_pBall->m_v3Velocity.x + g_pBall->m_v3Position.x) < 0.0f)
    {
        fTargetX = 40.0f;
    }
    else
    {
        fTargetX = -40.0f;
    }

    fY = mv3Position.y;

    if (fY < 0.0f)
    {
        fTargetY = -8.0f;
    }
    else
    {
        fTargetY = 8.0f;
    }

    float fAngle = nlATan2f(fTargetY - fY, fTargetX - mv3Position.x);
    maDesiredFacingDirection = (u16)(s32)(10430.378f * fAngle);
    mfDesiredSpeed = g_pGame->m_pGameTweaks->fChainChompSpeed;
}

/**
 * Offset/Address/Size: 0x204 | 0x8015DF08 | size: 0x1A8
 */
void ChainChomp::Move(float fDeltaT)
{
    nlPolar aChainSpeed;
    nlVector3 v3NewVelocity;
    float fSeekSpeed;

    fSeekSpeed = 60000.0f;

    if (meChainChompState == CHAIN_STATE_FALL)
    {
        fSeekSpeed = 180000.0f;
        mfDesiredSpeed = g_pGame->m_pGameTweaks->fChainChompSpeed;
    }
    else if (meChainChompState == CHAIN_STATE_RECOVER)
    {
        fSeekSpeed = 300000.0f;
    }
    else if (meChainChompState != CHAIN_STATE_LEAVE && AvoidSidelines())
    {
        float fHalf = 0.5f;
        fSeekSpeed = 180000.0f;
        mfDesiredSpeed = g_pGame->m_pGameTweaks->fChainChompSpeed * fHalf;
    }
    else
    {
        mfDesiredSpeed = g_pGame->m_pGameTweaks->fChainChompSpeed;
    }

    nlCartesianToPolar(aChainSpeed, mv3Velocity);

    if (aChainSpeed.r < 0.05f)
    {
        aChainSpeed.a = maFacingDirection;
    }

    float fNewSpeed = SeekSpeed(aChainSpeed.r, mfDesiredSpeed, 25.0f, 45.0f, fDeltaT);

    maFacingDirection = SeekDirection(aChainSpeed.a, maDesiredFacingDirection, fSeekSpeed, 3000.0f, fDeltaT);

    nlPolarToCartesian(v3NewVelocity.x, v3NewVelocity.y, maFacingDirection, fNewSpeed);
    v3NewVelocity.z = mv3Velocity.z;
    mv3Velocity = v3NewVelocity;
}

/**
 * Offset/Address/Size: 0x0 | 0x8015DD04 | size: 0x204
 */
void ChainChomp::DrawShadow(const cPoseAccumulator& pa, const nlMatrix4& worldMatrix)
{
    if (gbChainChompProjectedShadow)
    {
        SkinAnimatedNPC::DrawShadow(mpLastModel, mWorldMatrix);
        return;
    }

    nlMatrix4& nodeMatrix = pa.GetNodeMatrix(3);

    nlVector3 v3ModelPosition = nodeMatrix.GetTranslation();
    float y = v3ModelPosition.y;

    float frac = (mv3Position.z - 25.0f) / 75.0f;
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 1.0f)
        frac = 1.0f;

    float fAlpha = 128.0f * (1.0f - frac) + 50.0f * frac;
    float radius = g_pGame->m_pGameTweaks->fChainChompRadius;
    float half_dim = 7.0f * frac + (1.0f - frac) * (2.0f * radius);

    int alpha = (int)fAlpha;
    if (alpha < 0)
        alpha = 0;
    if (alpha > 255)
        alpha = 255;

    nlColour c;
    c.c[0] = 255;
    c.c[1] = 255;
    c.c[2] = 255;
    c.c[3] = (unsigned char)alpha;

    glQuad3 quad;
    quad.m_pos[0].x = v3ModelPosition.x - half_dim;
    quad.m_pos[0].y = y - half_dim;
    quad.m_pos[0].z = 0.015625f;
    quad.m_pos[1].x = v3ModelPosition.x - half_dim;
    quad.m_pos[1].y = y + half_dim;
    quad.m_pos[1].z = 0.015625f;
    quad.m_pos[2].x = v3ModelPosition.x + half_dim;
    quad.m_pos[2].y = y + half_dim;
    quad.m_pos[2].z = 0.015625f;
    quad.m_pos[3].x = v3ModelPosition.x + half_dim;
    quad.m_pos[3].y = y - half_dim;
    quad.m_pos[3].z = 0.015625f;

    quad.m_uv[0].x = 1.0f;
    quad.m_uv[0].y = 1.0f;
    quad.m_uv[1].x = 0.0f;
    quad.m_uv[1].y = 1.0f;
    quad.m_uv[2].x = 0.0f;
    quad.m_uv[2].y = 0.0f;
    quad.m_uv[3].x = 1.0f;
    quad.m_uv[3].y = 0.0f;

    quad.m_colour[3] = c;
    quad.m_colour[2] = c;
    quad.m_colour[1] = c;
    quad.m_colour[0] = c;

    glSetDefaultState(true);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTexture(glGetTexture("global/ball_shadow"), GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 3);
    glSetCurrentTextureState(glHandleizeTextureState());

    quad.Attach(GLV_Unshadowed, 0, true);
}
