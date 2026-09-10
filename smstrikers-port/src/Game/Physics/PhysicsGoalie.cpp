#include "Game/Physics/PhysicsGoalie.h"
#include "Game/Physics/PhysicsBall.h"
#include "Game/Player.h"
#include "Game/Team.h"
#include "Game/AI/AiUtil.h"
#include "Game/FixedUpdateTask.h"
#include "NL/utility.h"
#include "types.h"

static f32 CANT_COLLIDE = HUGE_VALF;

static float ballMaxMotionPerTick = PhysicsBall::GetBallMaxVelocity() * FixedUpdateTask::GetPhysicsUpdateTick();

/**
 * Offset/Address/Size: 0x8F0 | 0x8013A370 | size: 0x34
 */
void PhysicsGoalie::PostUpdate()
{
    PhysicsCharacter::PostUpdate();
    CollideGoalieWithPost();
}

/**
 * Offset/Address/Size: 0x780 | 0x8013A200 | size: 0x170
 */
bool PhysicsGoalie::SweepTestForBallContact(const nlVector3& ballPrevPosition, const nlVector3& ballCurrentPosition, const nlVector3& velocity, float ballRadius, nlVector3& positionWhenHit, nlVector3& contactNormal) const
{
    int testsPassed = 0;
    float goalieRadius = 4.0f * m_CentreOfMassHeight;

    nlVector3 goaliePos;
    GetPosition(&goaliePos);
    goaliePos.z = (2.0 * m_CentreOfMassHeight) + goaliePos.z;

    if (IsBallNearGoalie(goalieRadius, ballRadius, goaliePos, ballPrevPosition))
    {
        testsPassed = 1;
        if (BigBallSweepTest(goalieRadius, ballRadius, goaliePos, ballPrevPosition, ballCurrentPosition))
        {
            testsPassed = 2;
            if (SweepTestEveryBone(ballRadius, ballPrevPosition, ballCurrentPosition, contactNormal, positionWhenHit))
            {
                testsPassed = 3;
            }
        }
    }

    return testsPassed == 3;
}

bool PhysicsGoalie::IsBallNearGoalie(float goalieRadius, float ballRadius, const nlVector3& goaliePos, const nlVector3& ballPrevPosition)
{
    return (nlSqrt(nlGetLengthSquared3D(ballPrevPosition.x - goaliePos.x, ballPrevPosition.y - goaliePos.y, ballPrevPosition.z - goaliePos.z), true) - (goalieRadius + (ballRadius + ballMaxMotionPerTick))) <= 0.0f;
}

bool PhysicsGoalie::BigBallSweepTest(float goalieRadius, float ballRadius, const nlVector3& goaliePos, const nlVector3& ballPrevPosition, const nlVector3& ballCurrentPosition)
{
    float time = SweepSpheres(ballRadius, ballPrevPosition, ballCurrentPosition, goalieRadius, goaliePos, goaliePos);

    if ((time == CANT_COLLIDE) || (time < 0.0f) || (time > 1.0f))
    {
        return false;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x4A8 | 0x80139F28 | size: 0x2D8
 */
bool PhysicsGoalie::SweepTestEveryBone(float ballRadius, const nlVector3& ballPrevPosition, const nlVector3& ballCurrentPosition, nlVector3& contactNormal, nlVector3& positionWhenHit) const
{
    nlVector3 normalAccumulator = { 0.0f, 0.0f, 0.0f };
    float cantCollide;
    float smallestTime = 99999.0f;

    nlListConstIterator<PhysicsBoneVolume*> boneVolumeIterator = m_BoneVolumes.Begin();
    PhysicsBoneVolume* boneVolume;
    bool detectedContact = false;
    int numContactsDetected = 0;
    if (!boneVolumeIterator.IsValid())
    {
        return false;
    }

    cantCollide = CANT_COLLIDE;
    while (boneVolumeIterator.IsValid())
    {
        boneVolume = boneVolumeIterator.Current();
        PhysicsSphere* physSphere = (PhysicsSphere*)boneVolume->m_pObject;
        const nlVector3& boneCurrentPosition = physSphere->GetPosition();
        const nlVector3& bonePreviousPosition = boneVolume->m_PrevPosition;
        float time = SweepSpheres(
            ballRadius,
            ballPrevPosition,
            ballCurrentPosition,
            physSphere->GetRadius(),
            bonePreviousPosition,
            boneCurrentPosition);

        if ((time != cantCollide) && (time > 0.0f) && (time < 1.0f))
        {
            if (time < smallestTime)
            {
                smallestTime = time;
            }

            const float oneMinusTime = 1.0f - time;

            nlVec3WeightedSum(positionWhenHit, oneMinusTime, ballPrevPosition, time, ballCurrentPosition);

            nlVec3Set(contactNormal,
                positionWhenHit.x - ((oneMinusTime * bonePreviousPosition.x) + (time * boneCurrentPosition.x)),
                positionWhenHit.y - ((oneMinusTime * bonePreviousPosition.y) + (time * boneCurrentPosition.y)),
                positionWhenHit.z - ((oneMinusTime * bonePreviousPosition.z) + (time * boneCurrentPosition.z)));

            float contactNormalLengthSq = contactNormal.GetLengthSq3D();
            nlVec3Scale(contactNormal, nlRecipSqrt(contactNormalLengthSq, true));

            detectedContact = true;
            numContactsDetected += 1;

            nlVec3Add(normalAccumulator, normalAccumulator, contactNormal);
        }

        boneVolumeIterator.Next();
    }

    if (detectedContact)
    {
        float oneMinusTime = 1.0f - smallestTime;
        float inverseContactCount = 1.0f / (float)numContactsDetected;

        nlVec3Scale(contactNormal, normalAccumulator, inverseContactCount);
        nlVec3WeightedSum(positionWhenHit, oneMinusTime, ballPrevPosition, smallestTime, ballCurrentPosition);
    }

    return detectedContact;
}

/**
 * Offset/Address/Size: 0x70 | 0x80139AF0 | size: 0x438
 */
void PhysicsGoalie::CollideGoalieWithPost()
{
    Goalie* pGoalie = (Goalie*)m_pAICharacter;
    nlVector3 v3GoaliePos = GetPosition();
    v3GoaliePos.z = 0.0f;

    cNet* pNet = pGoalie->m_pTeam->m_pNet;
    nlVector3 v3PostPos;
    nlVector3 v3PrevHeadJointPos = pGoalie->GetPrevJointPosition(pGoalie->m_nHeadJointIndex);

    if (v3PrevHeadJointPos.y > 0.0f)
    {
        pNet->GetPostLocation(v3PostPos, 1, 0.0f);
    }
    else
    {
        pNet->GetPostLocation(v3PostPos, 0, 0.0f);
    }

    if (v3PostPos.CalculateDistanceSquared2D(v3PrevHeadJointPos) < 4.0f)
    {
        float fJointRadius[3] = { 0.15f, 0.2f, 0.2f };
        float postRadius = cNet::m_fNetPostRadius;
        float headDistLimitSq = nlGetLengthSquared1D(1.0f + postRadius);

        nlVector3 v3JointPos[3];

        v3JointPos[0] = pGoalie->GetJointPosition(pGoalie->m_nHeadJointIndex);
        v3JointPos[1] = pGoalie->GetJointPosition(pGoalie->m_nRightHandJointIndex);
        v3JointPos[2] = pGoalie->GetJointPosition(pGoalie->m_nLeftHandJointIndex);

        float fSin;
        float fCos;
        nlSinCos(&fSin, &fCos, pGoalie->m_aActualFacingDirection);

        nlVector3* pJointPos = v3JointPos;
        float* pJointRadius = fJointRadius;
        u8 bMoved = 0;

        for (int i = 0; i < 3; i++, pJointPos++, pJointRadius++)
        {
            nlVector3 v3JointWorldPos;
            float x = pJointPos->x;
            float y = pJointPos->y;

            v3JointWorldPos.x = v3GoaliePos.x + ((fCos * x) - (fSin * y));
            v3JointWorldPos.y = v3GoaliePos.y + ((fCos * y) + (fSin * x));
            v3JointWorldPos.z = v3PostPos.z;

            nlVector3 v3PostToJoint;
            nlVec3Sub(v3PostToJoint, v3PostPos, v3JointWorldPos);
            float jointDistSq = v3PostToJoint.GetLengthSq2D();
            float fMinDist = postRadius + (*pJointRadius);

            if (i == 0)
            {
                if (jointDistSq < headDistLimitSq)
                {
                    nlVector3 v3Norm;
                    nlVec3Set(v3Norm, v3JointWorldPos.y - v3PrevHeadJointPos.y, v3PrevHeadJointPos.x - v3JointWorldPos.x, 0.0f);

                    if ((v3PostPos.x * v3Norm.x) < 0.0f)
                    {
                        v3Norm.x *= -1.0f;
                        v3Norm.y *= -1.0f;
                    }

                    nlVector4 v4Plane;
                    MakePerpendicularPlane(v3JointWorldPos, v3Norm, v4Plane, 0.0f);

                    float fCurDist = ((v3PostPos.x * v4Plane.x) + (v3PostPos.y * v4Plane.y) + (v3PostPos.z * v4Plane.z)) - v4Plane.w;
                    float fCurDistAbs = (float)fabs(fCurDist);

                    if (fCurDistAbs < fMinDist)
                    {
                        float fJointDist = nlSqrt(jointDistSq, true);
                        float fMoveDist = InterpolateRangeClamped(0.0f, fMinDist - fCurDistAbs, 1.0f + postRadius, 0.0f, fJointDist);

                        if ((fCurDist > 0.0f) || m_CanCollidedWithGoalLine)
                        {
                            fMoveDist *= -1.0f;
                        }

                        v3GoaliePos.z = v3GoaliePos.z + (fMoveDist * v4Plane.z);
                        v3GoaliePos.y = v3GoaliePos.y + (fMoveDist * v4Plane.y);
                        v3GoaliePos.x = v3GoaliePos.x + (fMoveDist * v4Plane.x);
                        bMoved = 1;
                    }
                }
            }
            else
            {
                if (jointDistSq < (fMinDist * fMinDist))
                {
                    nlVector3 v3JointDist;
                    nlVec3Sub(v3JointDist, v3JointWorldPos, v3PostPos);

                    if (jointDistSq > 0.00001f)
                    {
                        float fCurDist = nlSqrt(jointDistSq, true);
                        float fScale = (fMinDist - fCurDist) / fCurDist;

                        nlVec3ScaleAdd(v3GoaliePos, fScale, v3JointDist, v3GoaliePos);
                        bMoved = 1;
                    }
                }
            }
        }

        if (bMoved)
        {
            SetCharacterPosition(v3GoaliePos);
        }
    }
}
