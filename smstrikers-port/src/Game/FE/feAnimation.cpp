#include "Game/FE/feAnimation.h"
#include "NL/nlMath.h"
#include "NL/nlDLRing.h"

/**
 * Offset/Address/Size: 0x0 | 0x8020E580 | size: 0x19C
 */
void FEAnimation::Update(float fCurrentTime)
{
    fAnimationKeyframe* currentFrame;
    float fAnimatedResult;

    switch (m_cast_type)
    {
    case 1:
        AnimateTargetAtTimeWithVector3(fCurrentTime);
        return;

    case 0:
        currentFrame = nlDLRingGetStart<fAnimationKeyframe>((fAnimationKeyframe*)m_DLRingHead);
        if (currentFrame != currentFrame->m_next)
        {
            if (fCurrentTime >= currentFrame->pKeyFrameData.m_fTime)
            {
                while (fCurrentTime > currentFrame->pKeyFrameData.m_fTime)
                {
                    currentFrame = currentFrame->m_next;
                    if (nlDLRingIsEnd<fAnimationKeyframe>((fAnimationKeyframe*)m_DLRingHead, currentFrame))
                    {
                        break;
                    }
                }

                float fTime = currentFrame->pKeyFrameData.m_fTime;
                if (fCurrentTime == fTime)
                {
                    fAnimatedResult = currentFrame->pKeyFrameData.m_fPoint;
                }
                else if (!(fCurrentTime > fTime) || currentFrame->pKeyFrameData.m_fControl1 != -1.0f)
                {
                    fAnimationKeyframe* prevFrame = currentFrame->m_prev;
                    float fPrevTime = prevFrame->pKeyFrameData.m_fTime;
                    float controlPoints[4] = { 0.0f };
                    controlPoints[0] = prevFrame->pKeyFrameData.m_fPoint;
                    controlPoints[1] = prevFrame->pKeyFrameData.m_fControl1;
                    controlPoints[2] = prevFrame->pKeyFrameData.m_fControl2;
                    controlPoints[3] = currentFrame->pKeyFrameData.m_fPoint;
                    fAnimatedResult = nlBezier(controlPoints, 3, (fCurrentTime - fPrevTime) / (fTime - fPrevTime));
                }
                else
                {
                    fAnimatedResult = currentFrame->pKeyFrameData.m_fPoint;
                }

                if (fAnimatedResult != -1.0f && m_type == eAnimOpacity)
                {
                    nlColour newColour = m_pTLInstanceTarget->GetAssetColour();
                    newColour.c[3] = (u8)fAnimatedResult;
                    m_pTLInstanceTarget->SetAssetColour(newColour);
                }
            }
        }
        return;
    }
}

/**
 * Offset/Address/Size: 0x19C | 0x8020E71C | size: 0x298
 */
void FEAnimation::AnimateTargetAtTimeWithVector3(float fCurrentTime)
{
    v3AnimationKeyframe* currentFrame;
    f32 result[3];
    f32 fMu;

    currentFrame = nlDLRingGetStart<v3AnimationKeyframe>((v3AnimationKeyframe*)this->m_DLRingHead);

    if (fCurrentTime < currentFrame->pKeyFrameDataX.m_fTime)
    {
        fCurrentTime = currentFrame->pKeyFrameDataX.m_fTime;
    }

    result[0] = -1.0f;
    while (fCurrentTime > currentFrame->pKeyFrameDataX.m_fTime
           && (result[0] != currentFrame->pKeyFrameDataX.m_fControl1
               || result[0] != currentFrame->pKeyFrameDataX.m_fControl2))
    {
        currentFrame = currentFrame->m_next;
        if (nlDLRingIsEnd<v3AnimationKeyframe>((v3AnimationKeyframe*)this->m_DLRingHead, currentFrame))
        {
            break;
        }
    }

    f32 currentTime = currentFrame->pKeyFrameDataX.m_fTime;

    if (fCurrentTime == currentTime)
    {
        result[0] = currentFrame->pKeyFrameDataX.m_fPoint;
        result[1] = currentFrame->pKeyFrameDataY.m_fPoint;
        result[2] = currentFrame->pKeyFrameDataZ.m_fPoint;
    }
    else if (!(fCurrentTime > currentTime) || currentFrame->pKeyFrameDataX.m_fControl1 != -1.0f)
    {
        v3AnimationKeyframe* prevFrame = currentFrame->m_prev;
        f32 prevTime = prevFrame->pKeyFrameDataX.m_fTime;

        float controlPointsX[4] = { 0 };
        controlPointsX[0] = prevFrame->pKeyFrameDataX.m_fPoint;
        controlPointsX[1] = prevFrame->pKeyFrameDataX.m_fControl1;
        controlPointsX[2] = prevFrame->pKeyFrameDataX.m_fControl2;
        controlPointsX[3] = currentFrame->pKeyFrameDataX.m_fPoint;

        float controlPointsY[4] = { 0 };
        controlPointsY[0] = prevFrame->pKeyFrameDataY.m_fPoint;
        controlPointsY[1] = prevFrame->pKeyFrameDataY.m_fControl1;
        controlPointsY[2] = prevFrame->pKeyFrameDataY.m_fControl2;
        controlPointsY[3] = currentFrame->pKeyFrameDataY.m_fPoint;

        float controlPointsZ[4] = { 0 };
        controlPointsZ[0] = prevFrame->pKeyFrameDataZ.m_fPoint;
        controlPointsZ[1] = prevFrame->pKeyFrameDataZ.m_fControl1;
        controlPointsZ[2] = prevFrame->pKeyFrameDataZ.m_fControl2;
        controlPointsZ[3] = currentFrame->pKeyFrameDataZ.m_fPoint;

        fMu = (fCurrentTime - prevTime) / (currentTime - prevTime);
        result[0] = nlBezier(controlPointsX, 3, fMu);
        result[1] = nlBezier(controlPointsY, 3, fMu);
        result[2] = nlBezier(controlPointsZ, 3, fMu);
    }
    else
    {
        result[0] = currentFrame->pKeyFrameDataX.m_fPoint;
        result[1] = currentFrame->pKeyFrameDataY.m_fPoint;
        result[2] = currentFrame->pKeyFrameDataZ.m_fPoint;
    }

    switch (m_type)
    {
    case eAnimPosition:
        m_pTLInstanceTarget->SetAssetPosition(result[0], result[1], result[2]);
        break;
    case eAnimRotation:
        m_pTLInstanceTarget->SetAssetRotation(result[0], result[1], result[2]);
        break;
    case eAnimScale:
        m_pTLInstanceTarget->SetAssetScale(result[0], result[1], result[2]);
        break;
    }
}
