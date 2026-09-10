#include "Game/Camera/FaceCam.h"

#include "NL/gl/glMatrix.h"

void FaceCam::SetToUserCharacter()
{
}

/**
 * Offset/Address/Size: 0x12C | 0x801A8E6C | size: 0x40
 */
FaceCam::FaceCam(float distance)
{
    mpCharacter = nullptr;
    mDistance = distance;
}

/**
 * Offset/Address/Size: 0x10 | 0x801A8D50 | size: 0x11C
 */
void FaceCam::Update(float dt)
{
    mTargetPosition = mpCharacter->GetJointPosition(mpCharacter->GetHeadJointIndex());
    const nlMatrix4& headMatrix = mpCharacter->m_pPoseAccumulator->GetNodeMatrix(mpCharacter->GetHeadJointIndex());

    float forwardX = headMatrix.e2[1][0];
    float forwardY = 0.f;
    float forwardZ = headMatrix.e2[1][1];

    float len = nlRecipSqrt(forwardY + (forwardX * forwardX + (forwardZ * forwardZ)), 1);

    nlVec3Set(mCameraPosition,
        mDistance * (len * forwardX) + mTargetPosition.x,
        mDistance * (len * forwardZ) + mTargetPosition.y,
        mDistance * (len * forwardY) + mTargetPosition.z);

    glMatrixLookAt(mViewMatrix, GetCameraPosition(), GetTargetPosition(), mUpVector);
}

/**
 * Offset/Address/Size: 0x8 | 0x801A8D48 | size: 0x8
 */
const nlVector3& FaceCam::GetCameraPosition() const
{
    return mCameraPosition;
}

/**
 * Offset/Address/Size: 0x0 | 0x801A8D40 | size: 0x8
 */
const nlVector3& FaceCam::GetTargetPosition() const
{
    return mTargetPosition;
}
