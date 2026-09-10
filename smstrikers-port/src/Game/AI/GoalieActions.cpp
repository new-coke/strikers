#include "Game/Goalie.h"
#include <stdio.h>
#include <stdlib.h>
// PORT: for eStadiumID, named by the GameInfoManager stand-in below.
#include "Game/DB/BasicGameInfo.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Fielder.h"
#include "Game/Net.h"
#include "Game/Team.h"
#include "Game/Ball.h"
#include "Game/GameTweaks.h"
#include "Game/SAnim/pnSingleAxisBlender.h"
#include "Game/SAnim/pnBlender.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/CharacterTriggers.h"
#include "Game/AI/FilteredRandom.h"
#include "Game/Field.h"
#include "Game/Physics/PhysicsFakeBall.h"
#include "NL/nlMath.h"
#include "Game/MathHelpers.h"

static f32 CANT_COLLIDE = *(f32*)__float_max;

extern cTeam* g_pCurrentlyUpdatingTeam;
extern cBall* g_pBall;
extern FakeBallWorld* g_pFakeBallWorld;
extern f32 gfRepositionThreshold;

static int gOffplayDejected[5] = { 0x90, 0x91, 0x92, 0x93, 0x94 };
static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };

inline float CalculateDistanceSquared2D(const nlVector3& pos1, const nlVector3& pos2)
{
    nlVector3 delta;
    nlVec3Sub(delta, pos1, pos2);
    return delta.GetLengthSq2D();
}

/**
 * Offset/Address/Size: 0x4518 | 0x80052A54 | size: 0x1E8
 */
void Goalie::ActionLooseBallCatch(float deltaTime)
{
    mfTargetTime -= deltaTime;

    if (m_eAnimID == GOALIEACTION_STS)
    {
        float targetTime = mfTargetTime;
        if (targetTime <= mBlendInfo.mfMilestoneTime[2] + 0.01f)
        {
            float clampedValue = mBlendInfo.mfMilestoneTime[2] - targetTime;
            clampedValue = nlMaxEquals(clampedValue, mBlendInfo.mfStartTime);
            PlayBlendedAnims(clampedValue, -1);
        }
    }
    else
    {

        if (mpSaveData == NULL || m_pCurrentAnimController->m_fTime > 0.95f)
        {
            if (m_pBall == NULL)
            {
                InitActionMove(false);
                return;
            }
            InitActionMoveWB();
            return;
        }

        if (g_pBall->m_pOwner != NULL)
        {
            return;
        }

        if (!m_pCurrentAnimController->TestTrigger(mpSaveData->mfMilestonePercent[2]))
        {
            return;
        }

        const nlVector3& leftHandPos = GetJointPosition(m_nLeftHandJointIndex);
        const nlVector3& rightHandPos = GetJointPosition(m_nRightHandJointIndex);

        float distSqLeft = CalculateDistanceSquared(g_pBall->m_v3Position, leftHandPos);
        if (distSqLeft < 1.0f || CalculateDistanceSquared(g_pBall->m_v3Position, rightHandPos) < 1.0f)
        {
            Audio::SoundAttributes soundAttrs;
            soundAttrs.Init();
            soundAttrs.SetSoundType(0xC0, true);
            soundAttrs.UseStationaryPosVector(m_v3Position);
            soundAttrs.mf_Volume = 0.4f;
            Audio::gStadGenSFX.Play(soundAttrs);

            PickupBall(g_pBall);
            mbPickedUp = true;
            g_pBall->ClearShotInProgress();
            EmitGoalieCatch(this, "goalie_catch", false);
        }
    }
}

/**
 * Offset/Address/Size: 0x3E6C | 0x800523A8 | size: 0x6AC
 */
void Goalie::ActionLooseBallDesperate(float fDeltaT)
{
    cBall* pBall = g_pBall;
    const nlVector3& v3BallPosition = pBall->m_v3Position;
    int animID = m_eAnimID;
    const LooseBallInfo* pInfo = mpLooseBallInfo;
    nlVector3 v3GuessBallPos;
    nlVector3 v3AdjPos;
    nlVector3 v3HeadCopy;
    nlVector3 v3AdjPosElse;
    nlVector3 v3HeadCopyElse;
    float fAbsX;
    if (pInfo->mnAnimID == animID)
    {
        cPN_SAnimController* pAnim = m_pCurrentAnimController;
        bool bAnimDone = false;
        if (pAnim->m_ePlayMode == PM_HOLD && pAnim->m_fTime == 1.0f)
            bAnimDone = true;
        if (bAnimDone)
        {
            if (animID == 0x7C)
            {
                InitActionPursueRecover();
                return;
            }
            if (m_pBall == NULL)
            {
                InitActionMove(false);
                return;
            }
            InitActionMoveWB();
            return;
        }
        if (pBall->m_pOwner == NULL)
        {
            float fPickupTime = pInfo->mfPickupTime;
            if (pAnim->m_fTime < fPickupTime)
            {
                float fRatio = pAnim->m_fTime / fPickupTime;
                float fPickupDuration = fPickupTime * pInfo->mfAnimDuration;
                fDeltaT = fPickupDuration - pInfo->mfAnimDuration * pAnim->m_fTime;
                float fGoalLineX = cField::GetGoalLineX(1U);
                float fTimeScale = 0.25f * fDeltaT;
                float fLimit = fGoalLineX - 0.2f;

                nlVec3ScaleAdd(v3GuessBallPos, fTimeScale, g_pBall->m_v3Velocity, v3BallPosition);
                if ((float)fabs(v3GuessBallPos.x) > fLimit)
                {
                    float fClampedX;
                    if (v3GuessBallPos.x > 0.0f)
                        fClampedX = fLimit;
                    else
                        fClampedX = -fLimit;
                    if ((float)fabs(v3BallPosition.x) < fLimit)
                    {
                        float fBallX = pBall->m_v3Position.x;
                        float fBallY = pBall->m_v3Position.y;
                        float fDX = fBallX - fClampedX;
                        float fDY = fBallY - v3GuessBallPos.y;
                        float fDXOrig = fBallX - v3GuessBallPos.x;
                        float fNewY = fBallY - (fDX * fDY) / fDXOrig;
                        v3GuessBallPos.y = fNewY;
                    }
                    v3GuessBallPos.x = fClampedX;
                }
                TrackTarget(v3GuessBallPos, fRatio);
                const nlVector3& v3HeadPos = GetJointPosition(m_nHeadJointIndex);
                v3HeadCopy = v3HeadPos;
                float fDX2 = 0.0f;
                fAbsX = std::fabs(v3HeadCopy.x);
                float fLimitH = cField::GetGoalLineX(1U) - 0.5f;
                float fNetY = 0.5f * cNet::m_fNetWidth;
                if (fAbsX > fLimitH)
                {
                    if ((float)fabs(v3HeadCopy.y) > fNetY)
                        fDX2 = fAbsX - fLimitH;
                }
                nlVector3 v3LHandCopy;
                nlVector3 v3RHandCopy;
                const nlVector3& v3RHandPos = GetJointPosition(m_nRightHandJointIndex);
                v3RHandCopy = v3RHandPos;
                fAbsX = std::fabs(v3RHandCopy.x);
                float fLimitR = cField::GetGoalLineX(1U) - 0.4f;
                if (fAbsX > fLimitR)
                {
                    if ((float)fabs(v3RHandCopy.y) > fNetY)
                    {
                        float fDiff = fAbsX - fLimitR;
                        if (fDiff > fDX2)
                            fDX2 = fDiff;
                    }
                }
                const nlVector3& v3LHandPos = GetJointPosition(m_nLeftHandJointIndex);
                v3LHandCopy = v3LHandPos;
                fAbsX = std::fabs(v3LHandCopy.x);
                if (fAbsX > fLimitR)
                {
                    if ((float)fabs(v3LHandCopy.y) > fNetY)
                    {
                        float fDiff = fAbsX - fLimitR;
                        if (fDiff > fDX2)
                            fDX2 = fDiff;
                    }
                }
                if (fDX2 > 0.0f)
                {
                    v3AdjPos = m_v3Position;
                    if (v3AdjPos.x > 0.0f)
                        fDX2 *= -1.0f;
                    v3AdjPos.x += fDX2;
                    SetPosition(v3AdjPos);
                }
                return;
            }
            else
            {
                const nlVector3& v3BallJoint = GetJointPosition(m_nBallJointIndex);
                if (CalculateDistanceSquared(pBall->m_v3Position, v3BallJoint) < 0.36f)
                {
                    InitiatePanicGrab(NULL);
                }
                return;
            }
        }
        if (m_pBall != NULL)
            return;
        SetGoalieAction(GOALIEACTION_PURSUE_BALL_POUNCE, 0);
    }
    else
    {
        if (muBallChangeCount != pBall->m_bBallPathChangeCount || mnOffplayPending != 0 || pBall->m_pOwner != NULL)
        {
            InitActionMove(false);
            return;
        }
        mfTargetTime = mfTargetTime - fDeltaT;
        DoNavigation(fDeltaT, 0.0f, NAVI_FOLLOW_TARGET);
        const nlVector3& v3HeadPos = GetJointPosition(m_nHeadJointIndex);
        v3HeadCopyElse = v3HeadPos;
        float fNetY;
        float fDX;
        fDX = 0.0f;
        fAbsX = std::fabs(v3HeadCopyElse.x);
        float fLimitH = cField::GetGoalLineX(1U) - 0.5f;
        fNetY = 0.5f * cNet::m_fNetWidth;
        if (fAbsX > fLimitH)
        {
            if ((float)fabs(v3HeadCopyElse.y) > fNetY)
                fDX = fAbsX - fLimitH;
        }
        nlVector3 v3LHandCopy;
        nlVector3 v3RHandCopy;
        const nlVector3& v3RHandPos = GetJointPosition(m_nRightHandJointIndex);
        v3RHandCopy = v3RHandPos;
        fAbsX = std::fabs(v3RHandCopy.x);
        float fLimitR = cField::GetGoalLineX(1U) - 0.4f;
        if (fAbsX > fLimitR)
        {
            if ((float)fabs(v3RHandCopy.y) > fNetY)
            {
                float fDiff = fAbsX - fLimitR;
                if (fDiff > fDX)
                    fDX = fDiff;
            }
        }
        const nlVector3& v3LHandPos = GetJointPosition(m_nLeftHandJointIndex);
        v3LHandCopy = v3LHandPos;
        fAbsX = std::fabs(v3LHandCopy.x);
        if (fAbsX > fLimitR)
        {
            if ((float)fabs(v3LHandCopy.y) > fNetY)
            {
                float fDiff = fAbsX - fLimitR;
                if (fDiff > fDX)
                    fDX = fDiff;
            }
        }
        if (fDX > 0.0f)
        {
            v3AdjPosElse = m_v3Position;
            if (v3AdjPosElse.x > 0.0f)
                fDX *= -1.0f;
            v3AdjPosElse.x += fDX;
            SetPosition(v3AdjPosElse);
        }
        const LooseBallInfo* pInfoE = mpLooseBallInfo;
        cBall* pBallE = g_pBall;
        float dXc;
        float dYc;
        float dXg;
        float dYg;
        float fGuessX;
        float fCatchRadSq;
        float fGuessY;
        float fCatchRadius = 0.6f + pInfoE->mfPickupDistance;
        float fPickupTimeE = pInfoE->mfPickupTime;
        float fAnimDurE = pInfoE->mfAnimDuration;
        float fTimeProduct = fPickupTimeE * fAnimDurE;
        fCatchRadSq = fCatchRadius * fCatchRadius;
        fGuessY = fTimeProduct * pBallE->m_v3Velocity.y + pBall->m_v3Position.y;
        fGuessX = fTimeProduct * pBallE->m_v3Velocity.x + pBall->m_v3Position.x;
        if (mfTargetTime < 0.02f
            || (float)fabs(pBall->m_v3Position.x) > cField::GetGoalLineX(1U) - 1.0f
            || (dYc = m_v3Position.y - pBall->m_v3Position.y,
                   dXc = m_v3Position.x - pBall->m_v3Position.x,
                   dXc * dXc + dYc * dYc)
                   < fCatchRadSq
            || (dYg = m_v3Position.y - fGuessY,
                   dXg = m_v3Position.x - fGuessX,
                   dXg * dXg + dYg * dYg)
                   < fCatchRadSq)
        {
            PlayNewAnim(mpLooseBallInfo->mnAnimID);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);
        }
    }
}

/**
 * Offset/Address/Size: 0x36CC | 0x80051C08 | size: 0x7A0
 */
void Goalie::ActionLooseBallPickup(float fDeltaT)
{
    float fTimeLeft = m_pCurrentAnimController->m_fTime;

    if (fTimeLeft > 0.97f)
    {
        if (g_pBall->m_pOwner != this && mfWaitTime > 0.0f && mpLooseBallInfo->mAnimType != LOOSEBALL_ANIM_KICK)
        {
            m_tNoPickupTimer.SetSeconds(0.0f);
        }
        else
        {
            if (m_eAnimID == 0x7C)
            {
                InitActionPursueRecover();
                return;
            }

            if (m_pBall == NULL)
            {
                InitActionMove(false);
                return;
            }

            InitActionMoveWB();
            return;
        }
    }

    if (mpLooseBallInfo->mAnimType == LOOSEBALL_ANIM_KICK)
    {
        if ((m_eAnimID == 5 || m_eAnimID == 4)
            && fTimeLeft > (0.1f + mpLooseBallInfo->mfPickupTime)
            && m_pBall == NULL)
        {
            InitActionMove(false);
            return;
        }

        if (mpPassTarget != NULL)
        {
            float fAngle = nlATan2f(
                mpPassTarget->m_v3Position.y - m_v3Position.y,
                mpPassTarget->m_v3Position.x - m_v3Position.x);
            m_aDesiredFacingDirection = (u16)(s32)(10430.378f * fAngle);
        }
        else
        {
            unsigned short dir;
            if (m_v3Position.x > 0.0f)
            {
                dir = 0x8000;
            }
            else
            {
                dir = 0;
            }
            m_aDesiredFacingDirection = dir;
        }

        float fSeekSpeed;
        float fSeekFalloff;
        if (mpLooseBallInfo->mnAnimID == 3)
        {
            fSeekSpeed = 150000.0f;
            fSeekFalloff = 2000.0f;
        }
        else
        {
            fSeekSpeed = 30000.0f;
            fSeekFalloff = 3000.0f;
        }

        unsigned short aNewFacingDirection = SeekDirection(
            m_aActualFacingDirection,
            m_aDesiredFacingDirection,
            fSeekSpeed,
            fSeekFalloff,
            fDeltaT);
        SetFacingDirection(aNewFacingDirection);
    }

    if (g_pBall->m_pOwner != this && mfWaitTime > 0.0f)
    {
        TacklePlayer(g_pBall->m_pOwner);

        float fNoPickupTime = m_tNoPickupTimer.GetSeconds();
        if (fNoPickupTime > 0.0f)
        {
            const nlVector3& pickupPos = GetJointPosition(m_nBallJointIndex);
            nlVector3 v3TargetPos = pickupPos;

            float fGoallineX = cField::GetGoalLineX(1U);
            float fDeltaPos = 0.0f;
            if (v3TargetPos.x > fGoallineX)
            {
                fDeltaPos = fGoallineX - v3TargetPos.x;
            }
            else if (v3TargetPos.x < -fGoallineX)
            {
                fDeltaPos = -fGoallineX - v3TargetPos.x;
            }

            if (fDeltaPos != 0.0f)
            {
                v3TargetPos.x += fDeltaPos;

                nlVector3 v3MyPos = m_v3Position;
                v3MyPos.x += fDeltaPos;
                SetPosition(v3MyPos);
            }

            float fBlend;
            float fPercent = fNoPickupTime / mfWaitTime;
            fBlend = 1.0f - fPercent;
            v3TargetPos.x = fBlend * v3TargetPos.x + fPercent * g_pBall->m_v3Position.x;
            v3TargetPos.y = fBlend * v3TargetPos.y + fPercent * g_pBall->m_v3Position.y;
            v3TargetPos.z = fBlend * v3TargetPos.z + fPercent * g_pBall->m_v3Position.z;
            g_pBall->SetPosition(v3TargetPos);

            nlVector3 v3BallVel = g_pBall->m_v3Velocity;
            float fSpeedSq = v3BallVel.x * v3BallVel.x + v3BallVel.y * v3BallVel.y + v3BallVel.z * v3BallVel.z;
            if (fSpeedSq > 64.0f)
            {
                v3BallVel.x = 0.3f * v3BallVel.x;
                v3BallVel.y = 0.3f * v3BallVel.y;
                v3BallVel.z = 0.3f * v3BallVel.z;
                g_pBall->SetVelocity(v3BallVel, SPINTYPE_NONE, NULL);
            }
        }
        else
        {
            Audio::SoundAttributes sndAtr;
            sndAtr.Init();
            sndAtr.SetSoundType(0xB7, true);
            sndAtr.UseStationaryPosVector(m_v3Position);
            sndAtr.mf_Volume = 0.4f;
            Audio::gStadGenSFX.Play(sndAtr);

            PickupBall(g_pBall);
            mbPickedUp = true;
            g_pBall->ClearShotInProgress();
        }
    }

    if (g_pBall->m_pOwner != NULL && g_pBall->m_pOwner != this)
    {
        if (IsOnSameTeam(g_pBall->m_pOwner))
        {
            InitActionMove(false);
            return;
        }

        SetGoalieAction(GOALIEACTION_PURSUE_BALL_POUNCE, 0);
        return;
    }

    if (IsPassThreat())
    {
        InitActionMove(true);
        return;
    }

    if (m_pBall == NULL && mfWaitTime <= 0.0f)
    {
        if (fTimeLeft >= mpLooseBallInfo->mfPickupTime)
        {
            if (mpLooseBallInfo->mAnimType == LOOSEBALL_ANIM_KICK)
            {
                if (!m_pCurrentAnimController->TestTrigger(mpLooseBallInfo->mfPickupTime))
                {
                    return;
                }

                const nlVector3& pickupPos = GetJointPosition(m_nBallJointIndex);
                if (CalculateDistanceSquared(g_pBall->m_v3Position, pickupPos) < 1.0f
                    || CalculateDistanceSquared(g_pBall->m_v3Position, m_v3Position) < 2.25f)
                {
                    InitiatePickup();
                    return;
                }

                InitActionMove(true);
                return;
            }

            const nlVector3& pickupPos = GetJointPosition(m_nBallJointIndex);
            if (CalculateDistanceSquared(g_pBall->m_v3Position, pickupPos) < 1.0f)
            {
                InitiatePickup();
            }
            return;
        }

        float fPercent = (fTimeLeft - mfTargetTime) / (mpLooseBallInfo->mfPickupTime - mfTargetTime);
        fDeltaT = fPercent * (fPercent * ((-2.0f * fPercent) + 3.0f));

        if (!(fDeltaT < 0.99f))
        {
            return;
        }

        nlVector3 v3BallVel;
        FakeBallWorld::GetPredictedBallPosition(mpLooseBallInfo->mfAnimDuration * (mpLooseBallInfo->mfPickupTime - fTimeLeft), mv3TargetPosition, v3BallVel);

        TrackTarget(mv3TargetPosition, fDeltaT);

        nlVector3 v3MyPos;
        nlVector3 animPos = GetJointPosition(m_nHeadJointIndex);
        float fDX = 0.0f;
        fPercent = (float)fabs(animPos.x);
        float fLimit = cField::GetGoalLineX(1U) - 0.5f;
        float fNetY = 0.5f * cNet::m_fNetWidth;
        if (fPercent > fLimit)
        {
            if ((float)fabs(animPos.y) > fNetY)
            {
                fDX = fPercent - fLimit;
            }
        }

        nlVector3 leftHandPos;
        nlVector3 rightHandPos = GetJointPosition(m_nRightHandJointIndex);
        fPercent = (float)fabs(rightHandPos.x);
        fLimit = cField::GetGoalLineX(1U) - 0.4f;
        if (fPercent > fLimit)
        {
            if ((float)fabs(rightHandPos.y) > fNetY)
            {
                float fDiff = fPercent - fLimit;
                if (fDiff > fDX)
                {
                    fDX = fDiff;
                }
            }
        }

        leftHandPos = GetJointPosition(m_nLeftHandJointIndex);
        fPercent = (float)fabs(leftHandPos.x);
        if (fPercent > fLimit)
        {
            if ((float)fabs(leftHandPos.y) > fNetY)
            {
                float fDiff = fPercent - fLimit;
                if (fDiff > fDX)
                {
                    fDX = fDiff;
                }
            }
        }

        if (fDX > 0.0f)
        {
            v3MyPos = m_v3Position;
            if (v3MyPos.x > 0.0f)
            {
                fDX *= -1.0f;
            }
            v3MyPos.x += fDX;
            SetPosition(v3MyPos);
        }
    }
}

/**
 * Offset/Address/Size: 0x3630 | 0x80051B6C | size: 0x9C
 */
void Goalie::ActionLooseBallPursueRolling(float deltaTime)
{
    DoNavigation(deltaTime, 0.2f + mfGoalieStepDist, NAVI_FACE_BALL);

    if ((mnOffplayPending)
        || (!IsLooseBallClose(SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fLooseBallChaseDistance))
        || ((g_pBall->m_pOwner != NULL) && (g_pBall->m_pOwner != this)))
    {
        InitActionMove(true);
        return;
    }

    InitActionLooseBallSetup();
}

/**
 * Offset/Address/Size: 0x35A8 | 0x80051AE4 | size: 0x88
 */
void Goalie::ActionLooseBallSetup(float fDeltaT)
{
    if ((mnOffplayPending)
        || (!IsLooseBallClose(SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fLooseBallChaseDistance))
        || ((g_pBall->m_pOwner != NULL) && (g_pBall->m_pOwner != this)))
    {
        InitActionMove(true);
        return;
    }
    InitActionLooseBallSetup();
}

/**
 * Offset/Address/Size: 0x358C | 0x80051AC8 | size: 0x1C
 */
void Goalie::MoveDirectionCB(uintptr_t nParam, cPN_SingleAxisBlender* blender)
{
    Goalie* pGoalie = (Goalie*)nParam;
    float result = 0.0f;
    if (pGoalie->mv3LocalNavTarget.y < 0.0f)
    {
        result = 1.0f;
    }
    blender->m_fDesiredWeight = result;
}

/**
 * Offset/Address/Size: 0x3544 | 0x80051A80 | size: 0x48
 */
void Goalie::MoveWeightCB(uintptr_t nParam, cPN_SingleAxisBlender* blender)
{
    Goalie* pGoalie = (Goalie*)nParam;
    blender->m_fDesiredWeight = (s32)(u16)abs_s16(pGoalie->maLocalAngle) / 32768.0f;
}

/**
 * Offset/Address/Size: 0x3538 | 0x80051A74 | size: 0xC
 */
void Goalie::StrafeSynchronizedSpeedCallback(uintptr_t nParam, cPN_SAnimController* controller)
{
    Goalie* pGoalie = (Goalie*)nParam;
    controller->m_fPlaybackSpeedScale = pGoalie->mfSpeedScale;
}

/**
 * Offset/Address/Size: 0x30E0 | 0x8005161C | size: 0x458
 */
void Goalie::ActionMove(float deltaTime)
{
    float dt = deltaTime;

    if (mnOffplayPending != GOALIE_OFFPLAY_NONE)
    {
        static FilteredRandomRange randgenDejected;
        int animID = -1;

        switch (mnOffplayPending)
        {
        case GOALIE_OFFPLAY_NONE:
        case GOALIE_OFFPLAY_GOAL_FOR:
            break;

        case GOALIE_OFFPLAY_GOAL_AGAINST:
            animID = gOffplayDejected[randgenDejected.genrand(5)];
            break;

        case GOALIE_OFFPLAY_ENDGAME_WIN:
            break;

        case GOALIE_OFFPLAY_ENDGAME_LOSE:
            animID = gOffplayDejected[randgenDejected.genrand(5)];
            break;

        case GOALIE_OFFPLAY_HALFTIME:
        case GOALIE_OFFPLAY_PENALTY:
            break;

        default:
            break;
        }

        if (animID >= 0)
        {
            mbDoHeadTrack = false;
            SetGoalieAction(GOALIEACTION_OFFPLAY, 0);
            SetAnimState(animID, true, 0.2f, false, false);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);
            return;
        }
    }

    if (g_pBall->m_pOwner != this)
    {
        if (GetGlobalPad() != NULL)
        {
            SwapController();
        }
    }

    bool isPassThreat = IsPassThreat();

    nlVector3 desiredDir;
    nlVector3 targetPos;
    nlVector3 desiredOffset;
    u16 desiredFacing;

    if (isPassThreat)
    {
        if (CanInterceptPass())
        {
            InitActionPassIntercept();
            return;
        }

        cFielder* pPassTarget = (cFielder*)g_pBall->GetPassTargetFielder();
        targetPos = g_pBall->m_v3PassIntercept;
        FindDesiredGoaliePosition(mv3TargetPosition, desiredDir, desiredOffset, desiredFacing, &targetPos);

        if (pPassTarget->m_eFielderDesireState == FIELDERDESIRE_ONETIMER)
        {
            float crouchDuration = GoalieSave::mfCrouchDuration;
            if (g_pBall->m_tPassTargetTimer.GetSeconds() < crouchDuration
                && IsCloseToPlane(mv3TargetPosition, m_v3Position, 1.5f))
            {
                InitActionPreCrouch(GOALIECROUCH_PASS);
                return;
            }

            mUrgency = URGENCY_HIGH;
        }

        mbDoIntercept = true;
    }
    else
    {
        mbDoIntercept = false;
        FindDesiredGoaliePosition(mv3TargetPosition, desiredDir, desiredOffset, desiredFacing, NULL);
        targetPos = g_pBall->m_v3Position;
    }

    mv3NavTarget = mv3TargetPosition;
    m_aDesiredFacingDirection = desiredFacing;

    DoNavigation(dt, 0.2f + mfGoalieStepDist, NAVI_FACE_BALL);

    if (CheckForSTSAttack())
    {
        return;
    }

    if (CheckForLooseBallShotInProgress())
    {
        return;
    }

    if (IsInsideGoalieBox(g_pBall->m_v3Position, 1.0f, 1.0f))
    {
        if (mUrgency == URGENCY_LOW)
        {
            mUrgency = URGENCY_MED;
        }
    }
    else if (mUrgency == URGENCY_MED)
    {
        mUrgency = URGENCY_LOW;
    }

    // PORT: STRIKERS_PROBE_GK, why a goalie beside a loose ball is not going for it.
    {
        static int s_probe = -1;
        if (s_probe < 0)
            s_probe = getenv("STRIKERS_PROBE_GK") != NULL;
        if (s_probe && g_pBall->m_pOwner == NULL
            && mGoalieActionState == GOALIEACTION_MOVE)
        {
            const float dx = g_pBall->m_v3Position.x - m_v3Position.x;
            const float dy = g_pBall->m_v3Position.y - m_v3Position.y;
            const float d = nlSqrt(dx * dx + dy * dy, true);
            static int s_tick = 0;
            if (d < 3.0f && (s_tick++ % 30) == 0)
            {
                const float chase = SkillTweaks::GetSkillTweaks(
                    g_pCurrentlyUpdatingTeam->m_nSide)->fLooseBallChaseDistance;
                fprintf(stderr,
                        "[gk] beside ball d=%.2f speed=%.2f | close=%d "
                        "passThreat=%d passTarget=%d towardNet=%d | "
                        "ballLock=%.2f gkLock=%.2f\n",
                        (double)d,
                        (double)nlSqrt(g_pBall->m_v3Velocity.x * g_pBall->m_v3Velocity.x
                                       + g_pBall->m_v3Velocity.y * g_pBall->m_v3Velocity.y,
                                       true),
                        (int)IsLooseBallClose(chase), (int)isPassThreat,
                        (int)(g_pBall->m_pPassTarget != NULL),
                        (int)IsLooseBallTowardNet(),
                        (double)g_pBall->m_tNoPickupTimer.GetSeconds(),
                        (double)m_tNoPickupTimer.GetSeconds());
            }
        }
    }

    if (IsLooseBallClose(SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fLooseBallChaseDistance))
    {
        if (!isPassThreat)
        {
            InitActionLooseBallSetup();
        }
        return;
    }

    if (IsOpponentBallCarrierInRange())
    {
        if (IsWithinPounceRange())
        {
            InitActionPursueBallCarrier();
            InitActionPursueBallPounce();
            return;
        }

        cFielder* pOwnerFielder = g_pBall->GetOwnerFielder();

        bool shouldPreCrouch = false;
        s32 shotMeterState = *(s32*)pOwnerFielder->m_pShotMeter;
        if (shotMeterState == 1 || shotMeterState == 3 || shotMeterState == 4)
        {
            shouldPreCrouch = true;
        }

        if (shouldPreCrouch)
        {
            if (IsCloseToPlane(mv3TargetPosition, m_v3Position, 1.2f))
            {
                InitActionPreCrouch(GOALIECROUCH_SHOT);
            }
            else
            {
                mUrgency = URGENCY_HIGH;
            }
            return;
        }

        InitActionPursueBallCarrier();
        return;
    }

    if (IsTeammateHoardingBall())
    {
        InitActionGrabBall();
    }
}

inline s16 ClampMin(const s16 diff, const s16 min)
{
    if (diff >= min)
    {
        return diff;
    }
    return min;
}

inline s16 ClampMax(const s16 diff, const s16 max)
{
    if (diff <= max)
    {
        return diff;
    }
    return max;
}

/**
 * Offset/Address/Size: 0x3070 | 0x800515AC | size: 0x70
 */
void Goalie::RunWeightCB(uintptr_t nParam, cPN_SingleAxisBlender* blender)
{
    const Goalie* pGoalie = (Goalie*)nParam;

    s16 diff = (s16)(pGoalie->m_aDesiredFacingDirection - pGoalie->m_aActualFacingDirection);

    s16 minClampedDiff;
    if (diff < -0x31C4)
    {
        minClampedDiff = -0x31C4;
    }
    else
    {
        minClampedDiff = diff;
    }

    s16 clampedDiff;
    if (minClampedDiff > 0x31C4)
    {
        clampedDiff = 0x31C4;
    }
    else
    {
        clampedDiff = minClampedDiff;
    }

    blender->m_fDesiredWeight = (float)(clampedDiff + 0x31C4) / 25480.0f;
}

void Goalie::StartRunBlend()
{
    int runAnims[] = { 0x21, 0x1F, 0x20 };

    cPN_SingleAxisBlender* pRunSAB = CreateSingleAxisBlender(runAnims, 3, 1, RunWeightCB, 0.15f, NULL);

    cPN_SAnimController* pPrevCtrlr = NULL;
    for (int i = 0; i < 3; i++)
    {
        cPN_SAnimController* pCtrlr = (cPN_SAnimController*)pRunSAB->GetChild(i);
        if (pPrevCtrlr == NULL)
        {
            pCtrlr->m_fSynchronizedWeight = 0.0f;
        }
        else
        {
            pCtrlr->m_bIsSynchronized = true;
            pPrevCtrlr->m_pSynchronizedController = pCtrlr;
        }
        pPrevCtrlr = pCtrlr;
    }

    *m_pAILayer = ::new (AllocateBlender()) cPN_Blender(*m_pAILayer, pRunSAB, 0.1f);
    InitMovementFromAnimSeek(60000.0f, 4000.0f);
}

/**
 * Offset/Address/Size: 0x27FC | 0x80050D38 | size: 0x874
 */
void Goalie::ActionMoveWB(float fDeltaT)
{
    if (mnSubstate == 6)
    {
        bool isAnimDone = false;
        cPN_SAnimController* pCtrl = m_pCurrentAnimController;
        if (pCtrl->m_ePlayMode == PM_HOLD)
        {
            if (1.0f == pCtrl->m_fTime)
            {
                isAnimDone = true;
            }
        }
        if (isAnimDone)
        {
            mnSubstate = 0;
        }
        else
        {
            return;
        }
    }

    if (m_pController != NULL && mfWaitTime > 0.0f)
    {
        if (m_pBall == NULL)
        {
            InitActionMove(false);
            return;
        }

        mfWaitTime -= fDeltaT;

        float stickMag = m_pController->GetMovementStickMagnitude();

        if (stickMag > 0.0f)
        {
            mfTargetTime = 0.0f;

            f32 penaltyBoxX, penaltyBoxY;
            penaltyBoxY = cField::GetPenaltyBoxY() - 0.5f;
            penaltyBoxX = 0.5f + cField::GetPenaltyBoxX(1U);

            u16 direction = m_pController->GetMovementStickDirection();
            m_aDesiredFacingDirection = direction;

            float jogging = m_pTweaks->fJoggingSpeed;
            float running = m_pTweaks->fRunningSpeed;
            m_fDesiredSpeed = jogging + stickMag * (running - jogging);

            float posX = m_v3Position.x;
            float posY = m_v3Position.y;
            u16 dir = m_aDesiredFacingDirection;

            if ((float)fabs(posX) < penaltyBoxX)
            {
                if (posX > 0.0f)
                {
                    dir = (u16)(dir + 0x8000);
                }

                u16 d = dir;
                if (d < 0x1C18 || d > 0xE3E7)
                {
                    m_fDesiredSpeed = 0.0f;
                }
                else if (d < 0x43E8)
                {
                    dir = 0x43E8;
                }
                else if (d > 0xBC17)
                {
                    dir = 0xBC17;
                }

                if (posX > 0.0f)
                {
                    dir += 0x8000;
                }
            }

            if ((float)fabs(posY) > penaltyBoxY)
            {
                if (posY < 0.0f)
                {
                    dir = (u16)(dir + 0x8000);
                }

                u16 d = dir;
                if (d < 0x23E8 || d > 0xFC17)
                {
                    dir = 0xFC17;
                }
                else if (d < 0x5C18)
                {
                    m_fDesiredSpeed = 0.0f;
                }
                else if (d < 0x83E8)
                {
                    dir = 0x83E8;
                }

                if (posY < 0.0f)
                {
                    dir += 0x8000;
                }
            }

            if (m_fDesiredSpeed > 0.0f)
            {
                m_aDesiredFacingDirection = dir;
            }
            else
            {
                m_aDesiredFacingDirection = m_aActualFacingDirection;
            }
        }
        else
        {
            mfTargetTime += fDeltaT;
            m_fDesiredSpeed = 0.0f;
            m_aDesiredFacingDirection = m_aActualFacingDirection;
        }

        {
            bool bClamped = false;

            double fAbsX;
            fAbsX = __fabs(m_v3Position.x);
            if ((float)fAbsX < cField::GetPenaltyBoxX(1U))
            {
                u16 dirVal;
                if (m_v3Position.x > 0.0f)
                {
                    dirVal = 0;
                }
                else
                {
                    dirVal = 0x8000;
                }
                m_aDesiredFacingDirection = dirVal;
                bClamped = true;
            }

            double fAbsY;
            fAbsY = __fabs(m_v3Position.y);
            if ((float)fAbsY > cField::GetPenaltyBoxY())
            {
                u16 yDir;
                if (m_v3Position.y > 0.0f)
                {
                    yDir = 0xC000;
                }
                else
                {
                    yDir = 0x4000;
                }

                if (bClamped)
                {
                    u16 currentDir = m_aDesiredFacingDirection;
                    s16 diff = (s16)(yDir - currentDir);
                    currentDir = (u16)(currentDir + (s16)(diff * 0.5f));
                    m_aDesiredFacingDirection = currentDir;
                }
                else
                {
                    m_aDesiredFacingDirection = yDir;
                }
                bClamped = true;
            }

            if (bClamped)
            {
                if (m_fDesiredSpeed < 0.001f)
                {
                    m_fDesiredSpeed = m_pTweaks->fJoggingSpeed;
                }
            }
        }

        if (GetGlobalPad()->JustPressed(0x17, true))
        {
            m_pTeam->TogglePowerup(false);
        }

        if (GetGlobalPad()->JustPressed(0x1c, true))
        {
            m_eLastPadAction = (ePadActions)0x25;
            InitActionPass(false);
            return;
        }

        if (GetGlobalPad()->JustPressed(0x1b, true))
        {
            m_eLastPadAction = (ePadActions)0x25;
            InitActionPass(true);
            return;
        }

        if (mfTargetTime > 1.0f)
        {
            float x = m_v3Position.x;
            nlVector3 v3Center = m_v3Position;
            float m02 = m_m4WorldMatrix.e2[0][2];
            float m01 = m_m4WorldMatrix.e2[0][1];
            float m00 = m_m4WorldMatrix.e2[0][0];
            float dist = nlGetLength3D(x, m_v3Position.y, m_v3Position.z);
            float invDist = -1.0f / dist;
            v3Center.x = invDist * m_v3Position.x;
            v3Center.y = invDist * m_v3Position.y;
            v3Center.z = invDist * m_v3Position.z;

            float dot = m00 * v3Center.x + m01 * v3Center.y + m02 * v3Center.z;

            if (dot > 0.5)
            {
                mfTargetTime = 0.0f;
                PlayNewAnim(14);
                InitMovementFromAnim(0, v3Zero, 1.0f, false);
            }
        }

        if (m_fDesiredSpeed > 0.01f)
        {
            if (m_eAnimID == 0x1F)
            {
                return;
            }

            StartRunBlend();
        }
        else
        {
            if (m_eAnimID == 0x0E)
            {
                return;
            }
            PlayNewAnim(9);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);
        }
        return;
    }

    mfWaitTime = 0.0f;

    switch (mnSubstate)
    {
    case 0:
        mnSubstate = 4;
        return;

    case 5:
    {
        bool isAnimDone = false;
        cPN_SAnimController* pCtrl = m_pCurrentAnimController;
        if (pCtrl->m_ePlayMode == PM_HOLD)
        {
            if (1.0f == pCtrl->m_fTime)
            {
                isAnimDone = true;
            }
        }
        if (!isAnimDone)
        {
            return;
        }
        mnSubstate = 4;
        return;
    }

    case 4:
    {
        do
        {
            float posY = m_v3Position.y;
            float posX = m_v3Position.x;
            float angle = nlATan2f(-posY, -posX);
            m_aDesiredFacingDirection = (u16)(s32)(10430.378f * angle);

            double absX;
            absX = __fabs(m_v3Position.x);
            float goalLineX = cField::GetGoalLineX(1U) - 3.0f;
            if ((float)absX > goalLineX)
            {
            }
            else
            {
                u16 diff = (u16)abs_s16((s16)(m_aDesiredFacingDirection - m_aActualFacingDirection));
                if (diff <= 0xDAC)
                {
                    break;
                }
            }

            m_fDesiredSpeed = m_pTweaks->fRunningSpeed;

            if (m_eAnimID == 0x1F)
            {
                return;
            }

            StartRunBlend();
            return;
        } while (false);

        mnSubstate = 7;
        return;
    }

    case 7:
        InitActionPass(true);
        return;

    default:
        return;
    }
}

/**
 * Offset/Address/Size: 0x2758 | 0x80050C94 | size: 0xA4
 */
void Goalie::ActionSaveSetup(float deltaTime)
{
    float deflectResult = CheckForDelflectAwayFromNet();

    if (deflectResult < 0.0f)
    {
        return;
    }

    if (deflectResult > 0.0f)
    {
        InitActionSaveSetup(false);
        return;
    }

    if (mnOffplayPending != 0)
    {
        InitActionMove(true);
        return;
    }

    mfWaitTime -= deltaTime;
    if (mfWaitTime <= 0.01f)
    {
        InitActionSave();
    }
}

/**
 * Offset/Address/Size: 0x25D8 | 0x80050B14 | size: 0x180
 */
void Goalie::ActionSaveReposition(float deltaTime)
{
    if (mnOffplayPending != 0)
    {
        InitActionMove(true);
        return;
    }

    mfWaitTime -= deltaTime;

    float distSq = nlGetLengthSquared2D(m_v3Position.x - mv3NavTarget.x, m_v3Position.y - mv3NavTarget.y);

    bool shouldReposition = false;
    if ((distSq < gfRepositionThreshold * gfRepositionThreshold) || (distSq > mfTargetDist && distSq < 1.6899998f))
    {
        shouldReposition = true;
    }

    mfTargetDist = distSq;

    float deflectResult = CheckForDelflectAwayFromNet();
    if (deflectResult < 0.0f)
    {
        return;
    }

    if (mfWaitTime <= 0.02f || deflectResult > 0.0f || shouldReposition)
    {
        InitActionSaveSetup(false);
        return;
    }

    if (mfWaitTime < 0.05f)
    {
        PlayNewAnim(10);
        InitMovementFromAnimSeek(m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff);
        return;
    }

    float ballDx = g_pBall->m_v3Position.x - m_v3Position.x;
    float ballDy = g_pBall->m_v3Position.y - m_v3Position.y;
    float angle = nlATan2f(ballDy, ballDx);
    m_aDesiredFacingDirection = (u16)(s32)(10430.378f * angle); // @1734 constant

    DoNavigation(deltaTime, gfRepositionThreshold, NAVI_FACE_DESIRED);
}

inline void Goalie::CheckForLimbEndZoneCollision()
{
    nlVector3 v3RHandCopy;
    nlVector3 v3LHandCopy;
    nlVector3 v3HeadCopy;
    nlVector3 v3AdjPos;

    const nlVector3& v3LHandPos = GetJointPosition(m_nHeadJointIndex);
    v3HeadCopy = v3LHandPos;
    float fDX = 0.0f;
    float fAbsX = (float)fabs(v3HeadCopy.x);
    float fLimit = cField::GetGoalLineX(1U) - 0.5f;
    float fNetY = 0.5f * cNet::m_fNetWidth;

    if (fAbsX > fLimit)
    {
        if ((float)fabs(v3HeadCopy.y) > fNetY)
        {
            fDX = fAbsX - fLimit;
        }
    }

    const nlVector3& v3RHandPos = GetJointPosition(m_nRightHandJointIndex);
    v3RHandCopy = v3RHandPos;
    fAbsX = (float)fabs(v3RHandCopy.x);
    fLimit = cField::GetGoalLineX(1U) - 0.4f;

    if (fAbsX > fLimit)
    {
        if ((float)fabs(v3RHandCopy.y) > fNetY)
        {
            float fDiff = fAbsX - fLimit;
            if (fDiff > fDX)
            {
                fDX = fDiff;
            }
        }
    }

    const nlVector3& v3LHandPos2 = GetJointPosition(m_nLeftHandJointIndex);
    v3LHandCopy = v3LHandPos2;
    float fAbsX3 = (float)fabs(v3LHandCopy.x);

    if (fAbsX3 > fLimit)
    {
        if ((float)fabs(v3LHandCopy.y) > fNetY)
        {
            float fDiff = fAbsX3 - fLimit;
            if (fDiff > fDX)
            {
                fDX = fDiff;
            }
        }
    }

    if (fDX > 0.0f)
    {
        v3AdjPos = m_v3Position;
        if (v3AdjPos.x > 0.0f)
        {
            fDX *= -1.0f;
        }
        v3AdjPos.x = v3AdjPos.x + fDX;
        SetPosition(v3AdjPos);
    }
}

/**
 * Offset/Address/Size: 0x20BC | 0x800505F8 | size: 0x51C
 */
void Goalie::ActionSave(float fDeltaT)
{
    CheckForLimbEndZoneCollision();

    SaveData* pSaveData = mpSaveData;
    float fTakeoffTime = pSaveData->mfMilestonePercent[1];

    float fDX = m_pCurrentAnimController->m_fTime;
    float fCrouchTime = pSaveData->mfMilestonePercent[0];

    if (fTakeoffTime <= 0.0f)
    {
        float fGoalTime = pSaveData->mfMilestonePercent[2];
        fTakeoffTime = 0.7f * fGoalTime;
        fCrouchTime = 0.4f * fGoalTime;
    }

    if (fDX <= fTakeoffTime && m_pBall == NULL)
    {
        float deflectResult = CheckForDelflectAwayFromNet();
        if (deflectResult < 0.0f)
        {
            return;
        }
        if (deflectResult > 0.0f)
        {
            if (fDX < fCrouchTime)
            {
                mGoalieActionState = GOALIEACTION_SAVE_REPOSITION;
            }
            else
            {
                mGoalieActionState = GOALIEACTION_PRE_CROUCH;
            }
            InitActionSaveSetup(false);
            return;
        }
    }

    if (mbDoHeadTrack)
    {
        float dX;
        float dZ;
        float dY = 0.0f;
        dZ = g_pBall->m_v3Position.y - m_v3Position.y;
        dX = g_pBall->m_v3Position.x - m_v3Position.x;
        float distSq = dX * dX + dZ * dZ + dY;
        float m00;
        float m01;
        float m02;
        m02 = m_m4WorldMatrix.e2[0][2];
        m01 = m_m4WorldMatrix.e2[0][1];
        m00 = m_m4WorldMatrix.e2[0][0];
        if (distSq < 9.0f || dX * m00 + dZ * m01 + dY * m02 < 0.0f)
        {
            mbDoHeadTrack = false;
        }
    }

    if (fDX < mpSaveData->mfMilestonePercent[2])
    {
        float t = fDX / mpSaveData->mfMilestonePercent[2];
        s16 delta = (s16)(m_aDesiredFacingDirection - m_aActualFacingDirection);
        s32 adjustedDelta = ((s32)(1024.0f * (t * (t * ((-2.0f * t) + 3.0f)))) * delta) / 1024;
        u16 newFacing = (u16)(adjustedDelta + m_aActualFacingDirection);
        SetFacingDirection(newFacing);
    }

    if (g_pBall->m_tShotTimer.m_uPackedTime != 0 || g_pBall->m_pPassTarget != NULL)
    {
        if (g_pBall->m_pOwner != this)
        {
            if ((mpSaveData->muSaveType & 3) != 0)
            {
                GoalieTweaks* pTweaks = (GoalieTweaks*)m_pTweaks;
                fDX = pTweaks->fSaveCatchTolerance * pTweaks->fSaveCatchTolerance;
                const nlVector3& v3LHand = GetJointPosition(m_nLeftHandJointIndex);
                const nlVector3& v3RHand = GetJointPosition(m_nRightHandJointIndex);

                float distSqL = CalculateDistanceSquared(g_pBall->m_v3Position, v3LHand);

                if (distSqL < fDX || CalculateDistanceSquared(g_pBall->m_v3Position, v3RHand) < fDX)
                {
                    TacklePlayer(g_pBall->m_pOwner);
                    MakeSaveEvent(false);

                    Audio::SoundAttributes sndAtr;
                    sndAtr.Init();
                    sndAtr.SetSoundType(0xC0, true);
                    sndAtr.UseStationaryPosVector(m_v3Position);
                    Audio::gStadGenSFX.Play(sndAtr);

                    bool bIsPerfect = false;
                    if (g_pBall->m_tShotTimer.m_uPackedTime != 0 && g_pBall->m_unk_0xA4)
                    {
                        bIsPerfect = true;
                    }

                    if (bIsPerfect)
                    {
                        EmitGoalieCatch(this, "perfect_shot_catch", false);
                    }
                    else
                    {
                        EmitGoalieCatch(this, "goalie_catch", false);
                    }

                    PickupBall(g_pBall);
                    g_pBall->ClearShotInProgress();
                    if (g_pBall->m_pPassTarget != NULL)
                    {
                        g_pBall->ClearPassTarget();
                    }
                    mbBallImpacted = true;
                }
            }
        }
    }

    if (m_pCurrentAnimController->m_fTime > 0.95f)
    {
        InitActionDiveRecover();
    }
}

template <typename T>
class nlSingleton
{
public:
    static T* s_pInstance;
};

class GameInfoManager : public nlSingleton<GameInfoManager>
{
public:
    eStadiumID GetStadium() const;   // PORT: as GameInfo.h declares it
};

/**
 * Offset/Address/Size: 0x1C30 | 0x8005016C | size: 0x48C
 */
void Goalie::ActionSTS(float fDeltaT)
{
    nlVector3 v3BallDir;
    nlVector4 plane;
    nlVector4 plane2;
    nlVector3 v3Root;
    nlVector3 v3Projected;
    unsigned short aRoot;
    nlVector3 v3GoaliePos;

    f32 fAnimTime = m_pCurrentAnimController->m_fTime;

    if (m_pBall == NULL)
    {
        cBall* pBall = g_pBall;
        float fSavePercent;

        if (mpShooter != NULL)
        {
            nlVector3* pGoaliePos = &m_v3Position;
            fSavePercent = mpLooseBallInfo->mfPickupTime;
            nlVec3Sub(v3BallDir, *pGoaliePos, mpShooter->m_v3Position);
            f32 fDot = v3BallDir.x * pBall->m_v3Velocity.x
                     + v3BallDir.y * pBall->m_v3Velocity.y
                     + v3BallDir.z * pBall->m_v3Velocity.z;
            if (fDot > 0.0f)
            {
                MakePerpendicularPlane(*pGoaliePos, v3BallDir, plane, 0.7f);
                f32 fDist = pBall->m_v3Position.x * plane.x
                          + pBall->m_v3Position.y * plane.y
                          + pBall->m_v3Position.z * plane.z
                          - plane.w;
                if (fDist > 0.0f)
                {
                    fSavePercent = m_pCurrentAnimController->m_fTime - 0.001f;
                }
            }
        }
        else
        {
            f32 fMilestone = mpSaveData->mfMilestonePercent[2];
            fSavePercent = fMilestone;
            if (fAnimTime < fMilestone)
            {
                unsigned int uSaveType = mpSaveData->muSaveType;
                if ((uSaveType & 3) != 0 || (uSaveType == 0x20000 && m_eAnimID != 0x6d))
                {
                    f32 fBallFutureX;
                    f32 fBallFutureY;
                    f32 fBallFutureZ;
                    fBallFutureZ = fDeltaT * pBall->m_v3Velocity.z + pBall->m_v3Position.z;
                    fBallFutureY = fDeltaT * pBall->m_v3Velocity.y + pBall->m_v3Position.y;
                    fBallFutureX = fDeltaT * pBall->m_v3Velocity.x + pBall->m_v3Position.x;
                    MakePerpendicularPlane(m_v3Position, m_aActualFacingDirection, plane2, 0.5f);
                    f32 fPlaneDist = fBallFutureX * plane2.x
                                   + fBallFutureY * plane2.y
                                   + fBallFutureZ * plane2.z
                                   - plane2.w;
                    if (fPlaneDist < 0.0f)
                    {
                        fSavePercent = m_pCurrentAnimController->m_fTime - 0.001f;
                    }
                }
            }
        }
        if (m_pCurrentAnimController->TestTrigger(fSavePercent))
        {
            HandleSTSContact(g_pBall);
        }
    }
    else
    {
        if (mpSaveData != NULL && mpSaveData->muSaveType == 0x20000 && m_eAnimID != 0x6d)
        {
            if (mfTargetTime < fAnimTime && fAnimTime < 1.0f)
            {
                GetCurrentAnimFuture(-1, 1.0f, v3Root, v3Projected, aRoot);
                nlVec3Sub(v3Root, mv3NavTarget, v3Projected);
                f32 fPercent = (fAnimTime - mfTargetTime) / (1.0f - mfTargetTime);
                f32 fSmooth = -2.0f * fPercent + 3.0f;
                fSmooth = fPercent * fSmooth;
                fSmooth = fPercent * fSmooth;
                nlVec3ScaleAdd(v3Root, fSmooth, v3Root, m_v3Position);
                SetPosition(v3Root);
            }
            f32 fGoalieNetYLimit = 0.5f * cNet::m_fNetWidth - 0.7f;
            f32 fStadiumVal = 1.0f;
            switch (nlSingleton<GameInfoManager>::s_pInstance->GetStadium())
            {
            case 1:
                fStadiumVal = 1.6f;
                break;
            case 3:
                fStadiumVal = 1.8f;
                break;
            case 4:
                fStadiumVal = 2.0f;
                break;
            }
            f32 fGoalLineLimit = cField::GetGoalLineX(1U) + fStadiumVal;
            if (fAnimTime > 0.12f
                && (float)fabs(m_v3Position.y) > fGoalieNetYLimit
                && (float)fabs(m_v3Position.x) < fGoalLineLimit)
            {
                v3GoaliePos = m_v3Position;
                v3GoaliePos.y = (GetPosition().y > 0.0f) ? fGoalieNetYLimit : -fGoalieNetYLimit;
                SetPosition(v3GoaliePos);
            }
        }
    }
    if (fAnimTime > 0.95f)
    {
        SaveData* pSaveData = mpSaveData;
        if (pSaveData == NULL || pSaveData->muSaveType != 0x20000)
        {
            if (mnOffplayPending == GOALIE_OFFPLAY_NONE)
            {
                InitActionSTSRecover();
            }
            else if (pSaveData != NULL)
            {
                unsigned int uSaveType = pSaveData->muSaveType;
                if (uSaveType == 8)
                {
                    InitActionDiveRecover();
                }
                else if (uSaveType == 0x40000 || pSaveData->mnAnimID == 0x6d)
                {
                    InitActionSTSRecover();
                }
                else
                {
                    InitActionMove(true);
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x1BD8 | 0x80050114 | size: 0x58
 */
void Goalie::ActionSTSSetup(float deltaTime)
{
    if (mnOffplayPending != 0)
    {
        InitActionMove(true);
        return;
    }

    mfWaitTime -= deltaTime;
    if (mfWaitTime <= 0.01f)
    {
        InitActionSTS();
    }
}

/**
 * Offset/Address/Size: 0x1B18 | 0x80050054 | size: 0xC0
 */
void Goalie::ActionSTSRecover(float deltaTime)
{
    if (CheckForSTSAttack())
    {
        return;
    }

    mfWaitTime -= deltaTime;
    if (mfWaitTime <= 0.0f)
    {
        if (m_eAnimID != 0x70)
        {
            PlayNewAnim(0x70);
            InitMovementFromAnim(0, v3Zero, 1.0f, false);
        }
        else
        {
            if (m_pCurrentAnimController->m_fTime > 0.95f)
            {
                InitActionMove(true);
            }
        }
    }
}

static inline float SubtractPosX(const nlVector3& pos, float x)
{
    return pos.x - x;
}

static inline float SubtractPosY(const nlVector3& pos, const nlVector3& nav)
{
    return pos.y - nav.y;
}

/**
 * Offset/Address/Size: 0x19A0 | 0x8004FEDC | size: 0x178
 */
void Goalie::ActionChipShotStumble(float deltaTime)
{
    bool bShouldRecover = false;
    if (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)
    {
        bShouldRecover = true;
    }

    if (bShouldRecover)
    {
        if (m_eAnimID == 0x70)
        {
            InitActionMove(true);
            return;
        }
        InitActionDiveRecover();
        return;
    }

    if (m_pCurrentAnimController->m_fTime < mpSaveData->mfMilestonePercent[2])
    {
        float deflectResult = CheckForDelflectAwayFromNet();
        if (deflectResult < 0.0f)
        {
            return;
        }

        if (deflectResult > 0.0f)
        {
            m_pPhysicsCharacter->m_CanCollidedWithGoalLine = true;
            InitActionSaveSetup(false);
            return;
        }
    }

    float x = mv3NavTarget.x;

    if ((float)fabs(x) > (0.5f + (float)fabs(m_v3Position.x)) && m_pCurrentAnimController->m_fTime < 0.5f)
    {
        m_aDesiredFacingDirection = (u16)(s32)(10430.378f * nlATan2f(SubtractPosY(m_v3Position, mv3NavTarget), SubtractPosX(m_v3Position, x)));

        GoalieTweaks* pTweaks = static_cast<GoalieTweaks*>(m_pTweaks);
        u16 newFacing = SeekDirection(
            m_aActualFacingDirection,
            m_aDesiredFacingDirection,
            pTweaks->fThrowingDirectionSeekSpeed,
            pTweaks->fThrowingDirectionSeekFalloff,
            deltaTime);
        SetFacingDirection(newFacing);
    }
}

/**
 * Offset/Address/Size: 0x1904 | 0x8004FE40 | size: 0x9C
 */
void Goalie::ActionDiveRecover(float fDeltaT)
{
    if (m_pBall == nullptr)
    {
        GoalieTweaks* pTweaks = static_cast<GoalieTweaks*>(m_pTweaks);
        if (mFatigue.mfEnergyLevel < pTweaks->fGetupEnergyHigh)
        {
            float result = InterpolateRangeClamped(
                pTweaks->fGetupSpeedLow,
                1.0f,
                pTweaks->fGetupEnergyLow,
                pTweaks->fGetupEnergyHigh,
                mFatigue.mfEnergyLevel);
            m_pCurrentAnimController->m_fPlaybackSpeedScale = result;
        }

        if (ShouldStartCrossBlend(8))
        {
            InitActionMove(false);
        }
    }
    else
    {
        if (ShouldStartCrossBlend(9))
        {
            InitActionMoveWB();
        }
    }
}

/**
 * Offset/Address/Size: 0x17FC | 0x8004FD38 | size: 0x108
 */
void Goalie::ActionPass(float deltaTime)
{
    if (m_pBall != nullptr)
    {
        if (mpPassTarget != nullptr)
        {
            float dy = mpPassTarget->m_v3Position.y - m_v3Position.y;
            float dx = mpPassTarget->m_v3Position.x - m_v3Position.x;
            float angleRad = nlATan2f(dy, dx);

            m_aDesiredFacingDirection = (unsigned short)(s32)(10430.378f * angleRad);
        }
        else
        {
            if (m_pTeam->m_pNet->m_v3NetLocation.x > 0.0f)
            {
                m_aDesiredFacingDirection = 0x8000;
            }
            else
            {
                m_aDesiredFacingDirection = 0;
            }
        }

        GoalieTweaks* pTweaks = static_cast<GoalieTweaks*>(m_pTweaks);
        unsigned short newFacing = SeekDirection(
            m_aActualFacingDirection,
            m_aDesiredFacingDirection,
            pTweaks->fThrowingDirectionSeekSpeed,
            pTweaks->fThrowingDirectionSeekFalloff,
            deltaTime);
        SetFacingDirection(newFacing);
    }
    if (ShouldStartCrossBlend(8))
    {
        InitActionMove(false);
    }
}

/**
 * Offset/Address/Size: 0x1554 | 0x8004FA90 | size: 0x2A8
 */
void Goalie::ActionPassIntercept(float deltaTime)
{
    if (muBallDeflectCount != g_pBall->m_bBallDeflectCount)
    {
        InitActionMove(true);
        return;
    }

    mfWaitTime -= deltaTime;

    switch (mnSubstate)
    {
    case 1:
    {
        if (mfWaitTime <= 0.02f)
        {
            InitActionPassInterceptSave();
        }
        return;
    }
    case 4:
    {
        // Calculate angle to target position
        float dy = mv3TargetPosition.y - m_v3Position.y;
        float dx = mv3TargetPosition.x - m_v3Position.x;
        float angleToTarget = nlATan2f(dy, dx);
        u16 targetAngle = (u16)(s32)(10430.378f * angleToTarget);

        // Calculate angle to ball position
        dy = g_pBall->m_v3Position.y - m_v3Position.y;
        dx = g_pBall->m_v3Position.x - m_v3Position.x;
        float angleToBall = nlATan2f(dy, dx);
        u16 ballAngle = (u16)(s32)(10430.378f * angleToBall);

        // Choose run animation based on angle difference
        s16 angleDiff = (s16)(targetAngle - m_aActualFacingDirection);
        int animID = ChooseRunAnim(angleDiff, mv3TargetPosition, 1.0f);

        // Check if we need to adjust the target angle
        s16 ballAngleDiff = (s16)(ballAngle - targetAngle);
        ballAngleDiff = ballAngleDiff < 0 ? -ballAngleDiff : ballAngleDiff;

        u16 absBallAngleDiff = (u16)ballAngleDiff;

        if ((absBallAngleDiff > 0x4000) && (animID != 8) && ((m_eAnimID == 8) || (m_eAnimID == 0x27)))
        {
            targetAngle += 0x8000;
        }

        if ((mfWaitTime > 0.25f) && (animID != 8))
        {
            PlayNewAnim(animID);
            InitMovementFromAnim(0, v3Zero, 0.0f, false);

            GoalieTweaks* pTweaks = static_cast<GoalieTweaks*>(m_pTweaks);
            u16 newFacing = SeekDirection(
                m_aActualFacingDirection,
                targetAngle,
                pTweaks->fRunningDirectionSeekSpeed,
                pTweaks->fRunningDirectionSeekFalloff,
                deltaTime);
            SetFacingDirection(newFacing);
            return;
        }

        if (CanInterceptPass())
        {
            if (mfWaitTime <= 0.02f)
            {
                InitActionPassInterceptSave();
                return;
            }

            mnSubstate = 1;
            PlayNewAnim(8);

            GoalieTweaks* pTweaks = static_cast<GoalieTweaks*>(m_pTweaks);
            InitMovementFromAnimSeek(pTweaks->fRunningDirectionSeekSpeed, pTweaks->fRunningDirectionSeekFalloff);
            return;
        }

        float tmp = GoalieSave::mfCrouchDuration;
        if ((g_pBall->m_tPassTargetTimer.GetSeconds() < tmp) && (IsCloseToPlane(mv3TargetPosition, m_v3Position, 1.2f)))
        {
            InitActionPreCrouch(GOALIECROUCH_PASS);
            return;
        }

        mUrgency = URGENCY_HIGH;
        InitActionMove(true);
    }
    }
}

/**
 * Offset/Address/Size: 0x135C | 0x8004F898 | size: 0x1F8
 */
void Goalie::ActionPreCrouch(float deltaTime)
{
    nlVector3 targetPos = g_pBall->m_v3Position;

    if (!CheckForSTSAttack())
    {
        if (g_pBall->GetOwnerFielder() != NULL)
        {
            cFielder* pOwnerFielder = g_pBall->GetOwnerFielder();
            if (IsOnSameTeam((cPlayer*)pOwnerFielder))
            {
                InitActionMove(false);
            }
            else if (IsWithinPounceRange())
            {
                InitActionPursueBallCarrier();
                InitActionPursueBallPounce();
            }
            else
            {
                if (pOwnerFielder->m_eActionState != ACTION_SHOT && pOwnerFielder->m_eActionState != ACTION_SHOOT_TO_SCORE)
                {
                    InitActionMove(true);
                }
                else if (mCrouchType != GOALIECROUCH_SHOT)
                {
                    InitActionMove(true);
                }
            }
        }
        else if (g_pBall->m_pPassTarget == NULL)
        {
            if (mpShooter == NULL || (mpShooter->m_eActionState != ACTION_LOOSE_BALL_SHOT) || (mCrouchType != GOALIECROUCH_LOOSEBALL))
            {
                InitActionMove(true);
            }
        }
        else
        {
            if (mCrouchType != GOALIECROUCH_PASS)
            {
                InitActionMove(true);
            }

            targetPos = g_pBall->m_pPassTarget->m_v3Position;
        }

        if (mGoalieActionState == GOALIEACTION_PRE_CROUCH)
        {
            float dy = targetPos.y - m_v3Position.y;
            float dx = targetPos.x - m_v3Position.x;
            float angle = nlATan2f(dy, dx);

            m_aDesiredFacingDirection = (u16)(s32)(10430.378f * angle);

            GoalieTweaks* pTweaks = static_cast<GoalieTweaks*>(m_pTweaks);
            u16 newFacing = SeekDirection(
                m_aActualFacingDirection,
                m_aDesiredFacingDirection,
                75000.0f,
                4000.0f,
                deltaTime);
            SetFacingDirection(newFacing);
        }
    }
}

/**
 * Offset/Address/Size: 0xF9C | 0x8004F4D8 | size: 0x3C0
 */
void Goalie::ActionPursueBallCarrier(float fDeltaT)
{
    if (!CheckForSTSAttack())
    {

        cFielder* pOwnerFielder = g_pBall->GetOwnerFielder();

        if (mnOffplayPending != 0 || pOwnerFielder == NULL || IsOnSameTeam((cPlayer*)pOwnerFielder) || !IsOpponentBallCarrierInRange())
        {
            InitActionMove(true);
            return;
        }

        nlVector3& ballPos = g_pBall->m_v3Position;
        GetLocalPoint(mv3LocalContactPosition, ballPos, m_v3Position, m_aActualFacingDirection);

        nlVector3 ballDelta;
        nlVec3Set(ballDelta,
            ballPos.x - m_v3Position.x,
            ballPos.y - m_v3Position.y,
            ballPos.z - m_v3Position.z);

        nlVector3 desiredPos;
        nlVector3 desiredDir;
        nlVector3 desiredOffset;
        unsigned short desiredAngle;
        FindDesiredGoaliePosition(desiredPos, desiredDir, desiredOffset, desiredAngle, NULL);

        float pickupDistance = mpLooseBallInfo->mfPickupDistance;
        float pounceRange = 0.5f * pickupDistance;
        float ballDistSq = ballDelta.GetLengthSq3D();
        float pickupDistSq = pickupDistance * pickupDistance;
        float pounceRangeSq = pounceRange * pounceRange;
        float thresholdDist;
        if (ballDistSq > pickupDistSq)
        {
            thresholdDist = pickupDistance;
        }
        else if (ballDistSq < pounceRangeSq)
        {
            thresholdDist = pounceRange;
        }
        else
        {
            thresholdDist = nlSqrt(ballDistSq, true);
        }

        float scale = -thresholdDist / nlSqrt(desiredDir.GetLengthSq3D(), true);

        nlVec3Set(desiredPos,
            (scale * desiredDir.x) + desiredOffset.x,
            (scale * desiredDir.y) + desiredOffset.y,
            (scale * desiredDir.z) + desiredOffset.z);

        nlVector3 moveDir;
        nlVec3Sub(moveDir, desiredPos, m_v3Position);

        float dotProduct = (moveDir.x * ballDelta.x) + (moveDir.y * ballDelta.y) + (moveDir.z * ballDelta.z);

        if (dotProduct > 0.0f)
        {
            ballDelta = moveDir;
        }

        float angle = nlATan2f(ballDelta.y, ballDelta.x);
        m_aDesiredFacingDirection = (u16)(s32)(10430.378f * angle);

        float pickupDistanceSq = mpLooseBallInfo->mfPickupDistance * mpLooseBallInfo->mfPickupDistance;

        nlVector3 opponentLocalPos;
        GetLocalPoint(opponentLocalPos, pOwnerFielder->m_v3Position, m_v3Position, m_aActualFacingDirection);

        float dist3x = mv3LocalContactPosition.x * mv3LocalContactPosition.x;
        float dist1Sq = CalculateDistanceSquared2D(mv3LocalContactPosition, mpLooseBallInfo->mv3PickupPos);
        float dist3y = mv3LocalContactPosition.y * mv3LocalContactPosition.y;
        float dist2Sq = CalculateDistanceSquared2D(opponentLocalPos, mpLooseBallInfo->mv3PickupPos);
        float dist3Sq = dist3x + dist3y;
        nlVector3 dist4Delta;
        nlVec3Sub2D(dist4Delta, pOwnerFielder->m_v3Position, m_v3Position);
        float dist4Sq = dist4Delta.GetLengthSq2D();

        if ((mv3LocalContactPosition.x < -0.35f) || (dist1Sq > 0.36f && dist2Sq > 0.36f && dist3Sq > pickupDistanceSq && dist4Sq > pickupDistanceSq))
        {
            s16 angleDiff = (s16)(m_aDesiredFacingDirection - m_aActualFacingDirection);
            int animID = ChooseRunAnim(angleDiff, ballPos, 1.0f);
            PlayNewAnim(animID);

            int opponentActionState = pOwnerFielder->m_eActionState;
            float speedScale = 1.5f;

            if (mbPlayMiss && (opponentActionState != 0xF) && (opponentActionState != 0x10))
            {
                speedScale = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide)->fGoalieDekeSpeed;
            }

            if (speedScale != m_pCurrentAnimController->m_fPlaybackSpeedScale)
            {
                m_pCurrentAnimController->m_fPlaybackSpeedScale = speedScale;
            }

            InitMovementFromAnimSeek(m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff);
            return;
        }
        InitActionPursueBallPounce();
    }
}

static inline cPlayer* GetBallOwner(cBall* pBall, cBall** ppBall)
{
    *ppBall = pBall;
    return pBall->m_pOwner;
}

/**
 * Offset/Address/Size: 0xCCC | 0x8004F208 | size: 0x2D0
 */
void Goalie::ActionPursueBallPounce(float fDeltaT)
{
    float animTime = m_pCurrentAnimController->m_fTime;

    if (m_pBall == NULL)
    {
        if (animTime < 0.2f && CheckForSTSAttack())
        {
            PlayNewAnim(8);
            return;
        }

        cBall* pBall;
        cPlayer* pOwner = GetBallOwner(g_pBall, &pBall); // wtf..

        if (pOwner == NULL)
        {
            SetGoalieAction(GOALIEACTION_LOOSEBALL_PICKUP, 0);
            mfTargetTime = 0.0f;
            mfWaitTime = -1.0f;
            return;
        }

        if (IsOnSameTeam(pOwner) || !IsOpponentBallCarrierInRange())
        {
            InitActionMove(true);
            return;
        }

        if (CalculateDistanceSquared(GetJointPosition(m_nBallJointIndex), pBall->m_v3Position) < 0.16000001f
            || CalculateDistanceSquared(GetJointPosition(m_nLeftHandJointIndex), pBall->m_v3Position) < 0.16000001f
            || CalculateDistanceSquared(GetJointPosition(m_nRightHandJointIndex), pBall->m_v3Position) < 0.16000001f)
        {
            ExecutePounce(pOwner, true);
            return;
        }

        float pickupTime = mpLooseBallInfo->mfPickupTime;
        if ((animTime < pickupTime) && g_pBall->m_tShotTimer.m_uPackedTime == 0)
        {
            float ratio = animTime / pickupTime;
            float interpValue = ratio * (ratio * ((-2.0f * ratio) + 3.0f));

            if (interpValue < 0.99f && !mbPlayMiss)
            {
                TrackTarget(g_pBall->m_v3Position, interpValue);
            }
        }
    }

    if (animTime > 0.95f)
    {
        if (m_eAnimID != 0x7C)
        {
            if (m_pBall == NULL)
            {
                const GoalieTweaks* pTweaks = static_cast<const GoalieTweaks*>(m_pTweaks);
                if (mFatigue.mfEnergyLevel < pTweaks->fGetupEnergyHigh)
                {
                    float speed = InterpolateRangeClamped(
                        pTweaks->fGetupSpeedLow,
                        1.0f,
                        pTweaks->fGetupEnergyLow,
                        pTweaks->fGetupEnergyHigh,
                        mFatigue.mfEnergyLevel);
                    m_pCurrentAnimController->m_fPlaybackSpeedScale = speed;
                }

                if (ShouldStartCrossBlend(8))
                {
                    InitActionMove(false);
                    return;
                }
            }
            else
            {
                if (ShouldStartCrossBlend(9))
                {
                    InitActionMoveWB();
                    return;
                }
            }
        }
        else
        {
            InitActionPursueRecover();
        }
    }
}

/**
 * Offset/Address/Size: 0xA7C | 0x8004EFB8 | size: 0x250
 */
void Goalie::ActionOffplay(float fDeltaT)
{
    if (ShouldStartCrossBlend(0x90))
    {
        int animID;
        int currentAnimID = m_eAnimID;
        if (currentAnimID == 0x89 || currentAnimID == 0x8B || currentAnimID == 0x8D)
        {
            animID = 0x8D;
        }
        else if (currentAnimID == 0x88 || currentAnimID == 0x8A || currentAnimID == 0x8C)
        {
            animID = 0x8C;
        }
        else
        {
            static FilteredRandomRange randgenDejected;
            int index = randgenDejected.genrand(5);
            animID = gOffplayDejected[index];
        }

        SetAnimState(animID, true, 0.2f, false, false);
        InitMovementFromAnim(0, v3Zero, 1.0f, false);
    }

    nlVector3 pos = m_v3Position;
    float absX = (float)fabs(pos.x);
    float absY = (float)fabs(pos.y);

    float goalLineX = cField::GetGoalLineX(1U);
    float adjustedGoalLineX = goalLineX - 0.8f;

    if (absX > adjustedGoalLineX)
    {
        float halfNetWidth = 0.5f * cNet::m_fNetWidth;
        float netWidthAdjusted = halfNetWidth - 0.8f;

        if (absY < halfNetWidth)
        {
            if (absY > netWidthAdjusted)
            {
                if (absX < goalLineX)
                {
                    float newY;
                    float adjustedX = netWidthAdjusted + goalLineX;
                    float deltaX = adjustedX - absX;
                    if (pos.y > 0.0f)
                    {
                        newY = deltaX;
                    }
                    else
                    {
                        newY = -deltaX;
                    }
                    pos.y = newY;
                }
                else
                {
                    float newY;
                    if (pos.y > 0.0f)
                    {
                        newY = netWidthAdjusted;
                    }
                    else
                    {
                        newY = -netWidthAdjusted;
                    }
                    pos.y = newY;
                }

                SetPosition(pos);
                SetVelocity(v3Zero);
            }
        }
        else
        {
            float newX;
            if (pos.x > 0.0f)
            {
                newX = adjustedGoalLineX;
            }
            else
            {
                newX = -adjustedGoalLineX;
            }
            pos.x = newX;
            SetPosition(pos);
            SetVelocity(v3Zero);
        }
    }
}

/**
 * Offset/Address/Size: 0x860 | 0x8004ED9C | size: 0x21C
 */
void Goalie::ActionLooseBallPursueBouncing(float deltaTime)
{
    if (IsPassThreat() || mnOffplayPending || !IsLooseBallClose(0.0f) || g_pBall->m_pOwner != NULL)
    {
        InitActionMove(true);
        return;
    }

    if (muBallChangeCount != g_pBall->m_bBallPathChangeCount)
    {
        InitActionLooseBallSetup();
        return;
    }

    mfTargetTime -= deltaTime;
    if ((mfTargetTime < 0.1f) || (g_pBall->m_v3Position.z < 1.0f && g_pBall->m_v3Velocity.z < 3.0f))
    {
        InitActionLooseBallSetup();
        return;
    }

    nlVector3 v3TargetPos;
    nlVector3 v3TargetVel;
    FakeBallWorld::GetPredictedBallPosition(mfTargetTime, v3TargetPos, v3TargetVel);

    nlVector3 delta;
    nlVec3Sub2D(delta, m_v3Position, v3TargetPos);
    if (delta.GetLengthSq2D() < mfTargetDist)
    {
        PlayNewAnim(8);
        InitMovementFromAnim(0, v3Zero, 1.0f, false);

        GetLocalPoint(mv3LocalContactPosition, v3TargetPos, m_v3Position, m_aActualFacingDirection);
        GetLocalPoint(mv3LocalContactVelocity, v3TargetVel, m_v3Position, m_aActualFacingDirection);

        InitActionLooseBallCatch();
        return;
    }

    const nlVector3& pos = m_v3Position;
    float angle = nlATan2f(v3TargetPos.y - pos.y, v3TargetPos.x - pos.x);
    m_aDesiredFacingDirection = (u16)(s32)(10430.378f * angle);

    if (CalculateDistanceSquared(v3TargetPos, mv3TargetPosition) > mfTargetDist)
    {
        InitActionLooseBallSetup();
        return;
    }

    if (m_eAnimID != 0x26)
    {
        PlayNewAnim(0x26);
        InitMovementFromAnimSeek(m_pTweaks->fRunningDirectionSeekSpeed, m_pTweaks->fRunningDirectionSeekFalloff);
    }
}

/**
 * Offset/Address/Size: 0x6CC | 0x8004EC08 | size: 0x194
 */
void Goalie::ActionSTSAttackSetup(float deltaTime)
{
    if (!IsOpponentInSTS())
    {
        InitActionMove(true);
        return;
    }

    mfWaitTime -= deltaTime;
    if (mfWaitTime <= 0.0)
    {
        InitActionSTSAttack();
        return;
    }

    cFielder* pOwnerFielder = g_pBall->GetOwnerFielder();

    float dx = m_v3Position.x - pOwnerFielder->m_v3Position.x;
    float dy = m_v3Position.y - pOwnerFielder->m_v3Position.y;
    float distSq = dx * dx + dy * dy;
    float pickupDistSq = mpLooseBallInfo->mfPickupDistance * mpLooseBallInfo->mfPickupDistance;

    int animID = 8;
    if (distSq > pickupDistSq)
    {
        animID = 0x1A;
    }

    PlayNewAnim(animID);
    InitMovementFromAnim(0, v3Zero, 1.0f, false);

    GetLocalPoint(mv3LocalContactPosition, pOwnerFielder->m_v3Position, m_v3Position, m_aActualFacingDirection);

    float angle = nlATan2f(mv3LocalContactPosition.y, mv3LocalContactPosition.x);
    float progressRatio = (mfTargetTime - mfWaitTime) / mfTargetTime;

    s16 angleDeltaInt = (s16)(u16)(s32)(10430.378f * angle);
    s32 multiplierInt = (s32)(1024.0f * (progressRatio * (progressRatio * ((-2.0f * progressRatio) + 3.0f))));
    s32 adjustedDelta = (angleDeltaInt * multiplierInt) / 1024;

    u16 newFacing = adjustedDelta + m_aActualFacingDirection;

    SetFacingDirection(newFacing);
    m_aDesiredFacingDirection = newFacing;
}

/**
 * Offset/Address/Size: 0x2B8 | 0x8004E7F4 | size: 0x414
 */
void Goalie::ActionSTSAttack(float deltaTime)
{
    float animTime = m_pCurrentAnimController->m_fTime;

    if (IsOpponentInSTS())
    {
        if (animTime < mpLooseBallInfo->mfPickupTime)
        {
            u16 actualFacing = m_aActualFacingDirection;
            GetLocalPoint(mv3LocalContactPosition, mpShooter->m_v3Position, m_v3Position, actualFacing);

            if (animTime > 0.1f)
            {
                float t = (animTime - 0.1f) / (mpLooseBallInfo->mfPickupTime - 0.1f);
                float angle = nlATan2f(mv3LocalContactPosition.y, mv3LocalContactPosition.x);
                u16 aNewAng = (u16)(actualFacing + ((s32)(1024.0f * (t * (t * ((-2.0f * t) + 3.0f)))) * (s16)(u16)(s32)(10430.378f * angle)) / 1024);

                SetFacingDirection(aNewAng);
                m_aDesiredFacingDirection = aNewAng;
            }

            if (animTime > 0.306f)
            {
                float movementDuration;
                float pickupWindow = mpLooseBallInfo->mfPickupTime;
                movementDuration = mpLooseBallInfo->mfAnimDuration;
                float stepScale = deltaTime / (movementDuration * (pickupWindow -= 0.306f));
                nlVector3 movement = { 0.0f, 0.0f, 0.0f };

                movement.x = mfTargetDist;
                RotateVectorZAxis(movement, movement, actualFacing);

                float newX = (stepScale * movement.x) + m_v3Position.x;
                float newZ = (stepScale * movement.z) + m_v3Position.z;
                float newY = (stepScale * movement.y) + m_v3Position.y;
                movement.x = newX;
                movement.y = newY;
                movement.z = newZ;
                SetPosition(movement);
            }
        }

        if (animTime > 0.306f)
        {
            nlVector3 rightFootPos = GetJointPosition(m_nRightFootJointIndex);
            float dx = rightFootPos.x - mpShooter->m_v3Position.x;
            float dy = rightFootPos.y - mpShooter->m_v3Position.y;

            if (((dx * dx) + (dy * dy)) < 1.0f)
            {
                WhackSTSPlayer(mpShooter);
            }
        }
    }
    else
    {
        if (animTime <= 0.1f)
        {
            InitActionMove(true);
            return;
        }
    }

    if (animTime > 0.95f)
    {
        InitActionMove(true);
        return;
    }

    if ((mpShooter->m_eAnimID == 0x73) && (animTime < 0.45f))
    {
        cFielder* pShooter = mpShooter;
        nlVector3 rightFootPos = GetJointPosition(m_nRightFootJointIndex);
        float dx = rightFootPos.x - pShooter->m_v3Position.x;
        float dy = rightFootPos.y - pShooter->m_v3Position.y;
        float pushDist = 1.0f - nlSqrt((dx * dx) + (dy * dy), true);

        if (pushDist > 0.0f)
        {
            nlVector3 pushVec = { 0.0f, 0.0f, 0.0f };
            pushVec.x = pushDist;

            RotateVectorZAxis(pushVec, pushVec, m_aActualFacingDirection);

            float radius;
            double shooterAbsX;
            mpShooter->m_pPhysicsCharacter->GetRadius(&radius);
            radius += 0.2f;
            shooterAbsX = __fabs(pShooter->m_v3Position.x);

            if ((float)shooterAbsX > (cField::GetGoalLineX(1U) - radius))
            {
                float posX = m_v3Position.x - pushVec.x;
                float posZ = m_v3Position.z - pushVec.z;
                float posY = m_v3Position.y - pushVec.y;
                pushVec.x = posX;
                pushVec.y = posY;
                pushVec.z = posZ;
                SetPosition(pushVec);
            }
            else
            {
                float posX = pushVec.x + pShooter->m_v3Position.x;
                float posZ = pushVec.z + pShooter->m_v3Position.z;
                float posY = pushVec.y + pShooter->m_v3Position.y;
                pushVec.x = posX;
                pushVec.y = posY;
                pushVec.z = posZ;
                mpShooter->SetPosition(pushVec);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x168 | 0x8004E6A4 | size: 0x150
 */
void Goalie::ActionSnapBall(float fDeltaT)
{
    unsigned short aRootRot;
    float fTimeLeft;
    nlVector3 v3TargetPos;
    nlVector3 v3RootPos;

    if (g_pBall->m_pOwner != this)
    {
        TacklePlayer(g_pBall->m_pOwner);

        fTimeLeft = m_tNoPickupTimer.GetSeconds();

        if (fTimeLeft > 0.0f)
        {
            GetCurrentAnimFuture(m_nBallJointIndex, m_pCurrentAnimController->m_fTime, v3TargetPos, v3RootPos, aRootRot);

            float invInterpFactor;
            float interpFactor;

            interpFactor = (1.0f / mfWaitTime) * (mfWaitTime - fTimeLeft);
            invInterpFactor = 1.0f - interpFactor;

            v3TargetPos.x = (invInterpFactor * g_pBall->m_v3Position.x) + (interpFactor * v3TargetPos.x);
            v3TargetPos.y = (invInterpFactor * g_pBall->m_v3Position.y) + (interpFactor * v3TargetPos.y);
            v3TargetPos.z = (invInterpFactor * g_pBall->m_v3Position.z) + (interpFactor * v3TargetPos.z);

            g_pBall->SetPosition(v3TargetPos);
            return;
        }
        PickupBall(g_pBall);
        mbPickedUp = true;
        return;
    }

    if (m_pBall == NULL)
    {
        InitActionMove(true);
        return;
    }

    bool shouldMoveWB = false;
    if (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)
    {
        shouldMoveWB = true;
    }

    if (shouldMoveWB)
    {
        InitActionMoveWB();
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8004E53C | size: 0x168
 */
void Goalie::ActionGrabBall(float fDeltaT)
{
    bool bShouldInitMove = false;
    if (m_pCurrentAnimController->m_ePlayMode == PM_HOLD && m_pCurrentAnimController->m_fTime == 1.0f)
    {
        bShouldInitMove = true;
    }

    if (bShouldInitMove)
    {
        if (m_pBall == NULL)
        {
            InitActionMove(true);
            return;
        }
        InitActionMoveWB();
        return;
    }

    if (g_pBall->m_pOwner != this)
    {
        if (g_pBall->GetOwnerFielder() == NULL)
        {
            InitActionMove(false);
            return;
        }

        float fTimeThreshold = 0.1f + mpLooseBallInfo->mfPickupTime;
        if (m_pCurrentAnimController->m_fTime < fTimeThreshold)
        {
            float fInterpFactor = m_pCurrentAnimController->m_fTime / fTimeThreshold;
            TrackTarget(g_pBall->m_v3Position, fInterpFactor);

            const nlVector3& jointPos = GetJointPosition(m_nBallJointIndex);

            nlVector3 delta;
            nlVec3Set(delta, g_pBall->m_v3Position.x - jointPos.x, g_pBall->m_v3Position.y - jointPos.y, g_pBall->m_v3Position.z - jointPos.z);

            if (nlGetLengthSquared3D(delta.x, delta.y, delta.z) < 0.25f)
            {
                StealBall(g_pBall->m_pOwner);
                PickupBall(g_pBall);
                mbPickedUp = true;
                g_pBall->ClearShotInProgress();
                EmitGoalieCatch(this, "goalie_catch", false);
            }
        }
    }
}
