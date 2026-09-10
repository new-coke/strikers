
// #include "NL/math.h"

#include "NL/nlDLRing.h"
#include "NL/nlTask.h"
#include "NL/gl/glMatrix.h"

#include "Game/Camera/MatrixEffectCam.h"

#include "Game/Camera/CameraMan.h"
#include "Game/ReplayManager.h"
#include "Game/Camera/BaseCamera.h"

static nlVector3 cameraPosition;
static f32 sfDuration = 3.0f;
static f32 sfSpinRate = 120.0f;
static f32 sfDesiredDistanceFromTarget = 10.0f;
static f32 sfDesiredHeightAboveTarget = 1.0f;
static f32 sfZoomTime = 2.0f;

/**
 * Offset/Address/Size: 0x788 | 0x801A4B50 | size: 0x8C
 */
MatrixEffectCam::MatrixEffectCam()
{
    mfSpinDuration = sfDuration;
    mfPauseAfterSpin = 0.0f;
    mfZoomTime = sfZoomTime;
    mfDesiredDistanceFromTarget = sfDesiredDistanceFromTarget;
    mfDesiredHeightAboveTarget = sfDesiredHeightAboveTarget;
    mfSpinRate = sfSpinRate;
    mfFOV = 60.0f;
    mfElapsedTime = 0.0f;
    mFinishedCallback = NULL;
    mbFollowBall = false;
    mCurrentFilterDataIndex = 0;
    mbUseGameplayTransparencyFlags = false;
    nlVec3Set(mFilteredTargetPosition, 0.0f, 0.0f, 0.0f);
}

/**
 * Offset/Address/Size: 0x5C0 | 0x801A4988 | size: 0x1C8
 */
void MatrixEffectCam::Reset(const nlVector3& cameraStart, const nlVector3& beginTarget, const nlVector3& endTarget)
{
    nlVector3 ballPos;
    nlVector3 targetToCamera;

    mTargetPosition = beginTarget;
    mBeginTarget = mTargetPosition;
    mFinalTarget = endTarget;

    ballPos = ReplayManager::Instance()->mRender->mBall.mPosition;

    for (int i = 0; i < 10; ++i)
        mTargetFilterData[i] = ballPos;

    mFilteredTargetPosition = ballPos;

    const nlVector3& tgt = GetTargetPosition();

    nlVec3Sub(targetToCamera, cameraStart, tgt);

    nlCartesianToPolar(mCurrentPolarFromTarget, targetToCamera);

    mfCurrentCameraHeightAboveTarget = cameraStart.z - tgt.z;
    mfElapsedTime = 0.0f;

    cBaseCamera* top = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack);
    mfFOV = top->GetFOV();

    mbZoomingIn = mCurrentPolarFromTarget.r > mfDesiredDistanceFromTarget;
}

/**
 * Offset/Address/Size: 0x1D8 | 0x801A45A0 | size: 0x3E8
 */
void MatrixEffectCam::Update(float dt)
{
    if (nlTaskManager::m_pInstance->m_CurrState != 1)
    {
        bool bIsSpinNotFinished = true;
        mfElapsedTime += dt;
        if (mfElapsedTime > (mfSpinDuration + mfPauseAfterSpin))
        {
            if (mFinishedCallback)
            {
                mFinishedCallback(this);
            }
        }

        if (mfElapsedTime > mfSpinDuration)
        {
            bIsSpinNotFinished = false;
        }

        if (bIsSpinNotFinished)
        {
            s32 steps = (s32)(65536.0f * (dt * mfSpinRate));
            steps = steps / 360;
            mCurrentPolarFromTarget.a = (u16)((u16)mCurrentPolarFromTarget.a + (u16)steps);
        }

        bool var_r0 = (mbZoomingIn && (mCurrentPolarFromTarget.r > mfDesiredDistanceFromTarget)) || (!mbZoomingIn && (mCurrentPolarFromTarget.r < mfDesiredDistanceFromTarget));
        if (var_r0)
        {
            if (mfElapsedTime >= mfZoomTime)
            {
                mCurrentPolarFromTarget.r = mfDesiredDistanceFromTarget;
            }
            else
            {
                const float remain = mfDesiredDistanceFromTarget - mCurrentPolarFromTarget.r;
                const float step = remain / (mfZoomTime - mfElapsedTime);

                if ((float)fabs(step * dt) < remain)
                {
                    mCurrentPolarFromTarget.r = mCurrentPolarFromTarget.r + step * dt;
                }
                else
                {
                    mCurrentPolarFromTarget.r = mCurrentPolarFromTarget.r + remain;
                }
            }
        }

        if ((float)fabs(mfCurrentCameraHeightAboveTarget - mfDesiredHeightAboveTarget) > 0.01f)
        {
            if (mfElapsedTime >= mfZoomTime)
            {
                mfCurrentCameraHeightAboveTarget = mfDesiredHeightAboveTarget;
            }
            else
            {
                const float heightRemaining = mfDesiredHeightAboveTarget - mfCurrentCameraHeightAboveTarget;
                const float heightRate = (mfDesiredHeightAboveTarget - mfCurrentCameraHeightAboveTarget) / (mfZoomTime - mfElapsedTime);

                if ((float)fabs(heightRate * dt) < heightRemaining)
                {
                    mfCurrentCameraHeightAboveTarget = mfCurrentCameraHeightAboveTarget + heightRate * dt;
                }
                else
                {
                    mfCurrentCameraHeightAboveTarget = mfCurrentCameraHeightAboveTarget + heightRemaining;
                }
            }
        }

        if (mbFollowBall)
        {
            const nlVector3& ballPos = ReplayManager::Instance()->mRender->mBall.mPosition;
            mTargetFilterData[mCurrentFilterDataIndex] = ballPos;

            int i = mCurrentFilterDataIndex + 1;
            mCurrentFilterDataIndex = i - ((i / 10) * 10);

            mFilteredTargetPosition.x = 0.0f;
            mFilteredTargetPosition.y = 0.0f;
            mFilteredTargetPosition.z = 0.0f;
            for (i = 0; i < 10; ++i)
            {
                nlVec3Add(mFilteredTargetPosition, mFilteredTargetPosition, mTargetFilterData[i]);
            }

            nlVec3Scale(mFilteredTargetPosition, mFilteredTargetPosition, 0.1f);
        }
    }

    glMatrixLookAt(mViewMatrix, this->GetCameraPosition(), this->GetTargetPosition(), mUpVector);
}

/**
 * Offset/Address/Size: 0x148 | 0x801A4510 | size: 0x90
 */
const nlVector3& MatrixEffectCam::GetTargetPosition() const
{
    if (mbFollowBall)
    {
        mTargetPosition = mFilteredTargetPosition;
        return mFilteredTargetPosition;
    }

    float alpha = mfElapsedTime / mfSpinDuration;
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }

    nlVector3 diff;
    nlVec3Sub(diff, mFinalTarget, mBeginTarget);

    nlVec3Set(mTargetPosition,
        (alpha * diff.x) + mBeginTarget.x,
        (alpha * diff.y) + mBeginTarget.y,
        (alpha * diff.z) + mBeginTarget.z);

    return mTargetPosition;
}

/**
 * Offset/Address/Size: 0xA8 | 0x801A4470 | size: 0xA0
 */
const nlVector3& MatrixEffectCam::GetCameraPosition() const
{
    nlPolarToCartesian(cameraPosition, mCurrentPolarFromTarget);

    cameraPosition.z = mfCurrentCameraHeightAboveTarget;

    nlVec3Add(cameraPosition, cameraPosition, GetTargetPosition());

    return cameraPosition;
}

/**
 * Offset/Address/Size: 0x88 | 0x801A4450 | size: 0x20
 */
void MatrixEffectCam::SetInitialDistance(float distance)
{
    mCurrentPolarFromTarget.r = distance;
    mbZoomingIn = mCurrentPolarFromTarget.r > mfDesiredDistanceFromTarget;
}

/**
 * Offset/Address/Size: 0x64 | 0x801A442C | size: 0x24
 */
void MatrixEffectCam::SetInitialAngle(float angle)
{
    mCurrentPolarFromTarget.a = 10430.378f * angle;
}

/**
 * Offset/Address/Size: 0x5C | 0x801A4424 | size: 0x8
 */
void MatrixEffectCam::SetInitialHeightAboveTarget(float distance)
{
    mfCurrentCameraHeightAboveTarget = distance;
}

/**
 * Offset/Address/Size: 0x0 | 0x801A43C8 | size: 0x5C
 */
MatrixEffectCam::~MatrixEffectCam()
{
}
