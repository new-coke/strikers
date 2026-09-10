#include "Game/Physics/PhysicsNet.h"

#include "NL/nlMemory.h"
#include "types.h"

#include "Game/Physics/PhysicsObject.h"
#include "Game/FixedUpdateTask.h"
#include "Game/Physics/PhysicsBall.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Field.h"
#include "Game/Team.h"

extern cTeam* g_pTeams[2];

static f32 CANT_COLLIDE = *(f32*)__float_max;

PhysicsNet* PhysicsNet::spPhysNetNegativeX = nullptr;
PhysicsNet* PhysicsNet::spPhysNetPositiveX = nullptr;
float PhysicsNet::sfPhysicsNetWidth = 0.0f;
float PhysicsNet::sfPhysicsNetHeight = 0.0f;
float PhysicsNet::sfPhysicsNetDepth = 0.0f;
static bool sbTestLowerHorizontalGoalpost = false;

bool PhysicsNet::sbSweepTestEnabled = true;
float PhysicsNet::sfWallSoftness = 0.03f;

/**
 * Offset/Address/Size: 0x9A8 | 0x8013AD98 | size: 0x6F8
 */
PhysicsNet::PhysicsNet(CollisionSpace* space, bool positive_x)
{
    nlVector3 _c;
    nlVector3 _v1;
    nlVector3 _v2;
    nlVector3 postPosition;
    nlMatrix4 rotation;
    float netHeight;
    float netWidth;
    float goalLineX_;
    float goalLineX;
    float goalPostRadius;

    mpBackWall = NULL;
    mpSideWall1 = NULL;
    mpSideWall2 = NULL;
    mpSideGoalPost1 = NULL;
    mpSideGoalPost2 = NULL;
    mpTopGoalPost = NULL;
    mpNetMesh = NULL;
    errorCorrectionDepth = 0.0f;

    static float ballMaxMotionPerTick = PhysicsBall::GetBallMaxVelocity() * FixedUpdateTask::GetPhysicsUpdateTick();

    errorCorrectionDepth = ballMaxMotionPerTick;

    netHeight = cNet::m_fNetHeight;
    netWidth = cNet::m_fNetWidth;
    goalLineX = cField::GetGoalLineX((unsigned int)1);
    goalPostRadius = cNet::m_fNetPostRadius;

    if (NetMesh::s_bAnimatedNetMeshEnabled)
    {
        if (positive_x)
        {
            mpNetMesh = new (nlMalloc(sizeof(NetMesh), 8, false)) NetMesh(true);
        }
        else
        {
            mpNetMesh = new (nlMalloc(sizeof(NetMesh), 8, false)) NetMesh(false);
        }
    }

    if (positive_x)
    {
        float t0 = 0.0f;

        nlVec3Set(_c,
            ((0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth)) + goalLineX) - t0,
            0.5f * PhysicsNet::sfPhysicsNetWidth,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        nlVec3Set(_v1,
            0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth),
            0.0f,
            0.0f);

        nlVec3Set(_v2,
            0.0f,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        mpSideWall1 = new (nlMalloc(sizeof(PhysicsFinitePlane), 8, false)) PhysicsFinitePlane(g_CollisionSpace, _c, _v1, _v2, true, errorCorrectionDepth);

        nlVec3Set(_c,
            ((0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth)) + goalLineX) - t0,
            -0.5f * PhysicsNet::sfPhysicsNetWidth,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        nlVec3Set(_v1,
            0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth),
            0.0f,
            0.0f);

        nlVec3Set(_v2,
            0.0f,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        mpSideWall2 = new (nlMalloc(sizeof(PhysicsFinitePlane), 8, false)) PhysicsFinitePlane(g_CollisionSpace, _c, _v2, _v1, true, errorCorrectionDepth);

        nlVec3Set(_c,
            (goalLineX + PhysicsNet::sfPhysicsNetDepth) - t0,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        nlVec3Set(_v1,
            0.0f,
            (0.5f * PhysicsNet::sfPhysicsNetWidth) + errorCorrectionDepth,
            0.0f);

        nlVec3Set(_v2,
            0.0f,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        mpBackWall = new (nlMalloc(sizeof(PhysicsFinitePlane), 8, false)) PhysicsFinitePlane(g_CollisionSpace, _c, _v2, _v1, true, errorCorrectionDepth);

        mpSideWall1->SetCategory(0x200);
        mpSideWall1->SetCollide(0x20);
        mpSideWall2->SetCategory(0x200);
        mpSideWall2->SetCollide(0x20);
        mpBackWall->SetCategory(0x200);
        mpBackWall->SetCollide(0x20);
    }
    else
    {
        goalLineX_ = -goalLineX;
        float t0 = 0.0f;

        nlVec3Set(_c,
            t0 + (-(0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth)) + goalLineX_),
            0.5f * PhysicsNet::sfPhysicsNetWidth,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        nlVec3Set(_v1,
            0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth),
            0.0f,
            0.0f);

        nlVec3Set(_v2,
            0.0f,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        mpSideWall1 = new (nlMalloc(sizeof(PhysicsFinitePlane), 8, false)) PhysicsFinitePlane(g_CollisionSpace, _c, _v1, _v2, true, errorCorrectionDepth);

        nlVec3Set(_c,
            t0 + (-(0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth)) + goalLineX_),
            -0.5f * PhysicsNet::sfPhysicsNetWidth,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        nlVec3Set(_v1,
            0.5f * (PhysicsNet::sfPhysicsNetDepth + errorCorrectionDepth),
            0.0f,
            0.0f);

        nlVec3Set(_v2,
            0.0f,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        mpSideWall2 = new (nlMalloc(sizeof(PhysicsFinitePlane), 8, false)) PhysicsFinitePlane(g_CollisionSpace, _c, _v2, _v1, true, errorCorrectionDepth);

        nlVec3Set(_c,
            t0 + (goalLineX_ - PhysicsNet::sfPhysicsNetDepth),
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        nlVec3Set(_v1,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetWidth,
            0.0f);

        nlVec3Set(_v2,
            0.0f,
            0.0f,
            0.5f * PhysicsNet::sfPhysicsNetHeight);

        mpBackWall = new (nlMalloc(sizeof(PhysicsFinitePlane), 8, false)) PhysicsFinitePlane(g_CollisionSpace, _c, _v1, _v2, true, errorCorrectionDepth);

        mpSideWall1->SetCategory(0x200);
        mpSideWall1->SetCollide(0x20);
        mpSideWall2->SetCategory(0x200);
        mpSideWall2->SetCollide(0x20);
        mpBackWall->SetCategory(0x200);
        mpBackWall->SetCollide(0x20);
    }

    if (!PhysicsNet::sbSweepTestEnabled)
    {
        double f0 = 0.2;
        goalPostRadius = (float)(goalPostRadius + f0);

        mpSideGoalPost1 = new (nlMalloc(sizeof(PhysicsCapsule), 8, false)) PhysicsCapsule(g_CollisionSpace, NULL, goalPostRadius, netHeight);
        mpSideGoalPost2 = new (nlMalloc(sizeof(PhysicsCapsule), 8, false)) PhysicsCapsule(g_CollisionSpace, NULL, goalPostRadius, netHeight);
        mpTopGoalPost = new (nlMalloc(sizeof(PhysicsCapsule), 8, false)) PhysicsCapsule(g_CollisionSpace, NULL, goalPostRadius, netWidth);

        nlVec3Set(postPosition, (positive_x) ? goalLineX : -goalLineX, 0.5f * netWidth, 0.5f * netHeight);
        mpSideGoalPost1->SetPosition(postPosition, (PhysicsObject::CoordinateType)0);

        postPosition.y = -0.5f * netWidth;
        mpSideGoalPost2->SetPosition(postPosition, (PhysicsObject::CoordinateType)0);

        postPosition.y = 0.0f;
        postPosition.z = netHeight;
        mpTopGoalPost->SetPosition(postPosition, (PhysicsObject::CoordinateType)0);

        nlMakeRotationMatrixX(rotation, 1.5707964f);
        mpTopGoalPost->SetRotation(rotation);
    }

    mpNet = g_pTeams[0]->m_pNet;
    const float sign = mpNet->m_fDirection;
    if (((sign < 0.0f) && (positive_x != 0)) || ((sign > 0.0f) && (positive_x == 0)))
    {
        mpNet = g_pTeams[1]->m_pNet;
    }
}

/**
 * Offset/Address/Size: 0x7BC | 0x8013ABAC | size: 0x12C
 */
PhysicsNet::~PhysicsNet()
{
    delete mpBackWall;
    delete mpSideWall1;
    delete mpSideWall2;
    delete mpSideGoalPost1;
    delete mpSideGoalPost2;
    delete mpTopGoalPost;

    if (mpNetMesh != nullptr)
    {
        delete mpNetMesh;
    }
}

/**
 * Offset/Address/Size: 0x75C | 0x8013AB4C | size: 0x60
 */
bool PhysicsNet::IsAGoalPost(PhysicsObject* obj)
{
    if ((obj == spPhysNetNegativeX->mpSideGoalPost1)
        || (obj == spPhysNetNegativeX->mpSideGoalPost2)
        || (obj == spPhysNetNegativeX->mpTopGoalPost)
        || (obj == spPhysNetPositiveX->mpSideGoalPost1)
        || (obj == spPhysNetPositiveX->mpSideGoalPost2)
        || (obj == spPhysNetPositiveX->mpTopGoalPost))
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x6FC | 0x8013AAEC | size: 0x60
 */
bool PhysicsNet::IsAGoalWall(PhysicsObject* obj)
{
    if ((obj == spPhysNetNegativeX->mpSideWall1)
        || (obj == spPhysNetNegativeX->mpSideWall2)
        || (obj == spPhysNetNegativeX->mpBackWall)
        || (obj == spPhysNetPositiveX->mpSideWall1)
        || (obj == spPhysNetPositiveX->mpSideWall2)
        || (obj == spPhysNetPositiveX->mpBackWall))
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0xD0 | 0x8013A4C0 | size: 0x62C
 */
bool PhysicsNet::SweepTestForBallContact(const nlVector3& startPos, const nlVector3& endPos, const nlVector3& ballVelocity, float ballRadius, nlVector3& contactPos, nlVector3& contactNormal, PhysicsObject** hitObject) const
{
    nlVector3 leftPostPos, rightPostPos;
    nlVector3 goalPost0Location;
    nlVector3 goalPost1Location;
    nlVector3 goalPostSpherePos0;
    nlVector3 goalPostSpherePos1;
    float height;
    nlVector3 ballLinVelocity;
    CollisionBallGoalpostData* pEventData;

    cField::GetGoalLineX((unsigned int)1);
    float netPostRadius = cNet::m_fNetPostRadius;

    bool hitHorizontalGoalpost = false;
    bool hitLeftVerticalGoalpost = false;
    bool hitRightVerticalGoalpost = false;

    mpNet->GetPostLocation(leftPostPos, 0, 0.0f);
    mpNet->GetPostLocation(rightPostPos, 1, 0.0f);

    if ((float)fabs(endPos.x) < ((float)fabs(leftPostPos.x) - (netPostRadius + ballRadius)))
    {
        return false;
    }

    goalPost0Location = leftPostPos;
    goalPost0Location.z = startPos.z;
    goalPost1Location = leftPostPos;
    goalPost1Location.z = endPos.z;

    float sweepResult = SweepSpheres(ballRadius, startPos, endPos, netPostRadius, goalPost0Location, goalPost1Location);
    if (sweepResult >= 0.0f && sweepResult <= 1.0f)
    {
        hitLeftVerticalGoalpost = true;
    }

    if ((sweepResult < 0.0f) || (sweepResult > 1.0f))
    {
        goalPost0Location = rightPostPos;
        goalPost0Location.z = startPos.z;
        goalPost1Location = rightPostPos;
        goalPost1Location.z = endPos.z;

        sweepResult = SweepSpheres(ballRadius, startPos, endPos, netPostRadius, goalPost0Location, goalPost1Location);
        if (sweepResult >= 0.0f && sweepResult <= 1.0f)
        {
            hitRightVerticalGoalpost = true;
        }
    }

    float horizontalPostHeight = cNet::m_fNetHeight;
    if (sbTestLowerHorizontalGoalpost)
    {
        horizontalPostHeight = 0.5f * cNet::m_fNetHeight;
    }

    goalPost0Location = rightPostPos;
    goalPost0Location.y = startPos.y;
    goalPost0Location.z = horizontalPostHeight;
    goalPost1Location = rightPostPos;
    goalPost1Location.y = endPos.y;
    goalPost1Location.z = horizontalPostHeight;

    float sweepResult3 = SweepSpheres(ballRadius, startPos, endPos, netPostRadius, goalPost0Location, goalPost1Location);
    if (sweepResult3 >= 0.0f && sweepResult3 <= 1.0f)
    {
        hitHorizontalGoalpost = true;
    }

    float finalSweepResult = -1.0f;

    if ((hitHorizontalGoalpost != 0) && ((hitLeftVerticalGoalpost != 0) || (hitRightVerticalGoalpost != 0)))
    {
        if (sweepResult3 < sweepResult)
        {
            finalSweepResult = sweepResult3;
            hitLeftVerticalGoalpost = 0;
            hitRightVerticalGoalpost = 0;
        }
        else
        {
            finalSweepResult = sweepResult;
            hitHorizontalGoalpost = 0;
        }
    }
    else if (hitHorizontalGoalpost != 0)
    {
        finalSweepResult = sweepResult3;
    }
    else if ((hitLeftVerticalGoalpost != 0) || (hitRightVerticalGoalpost != 0))
    {
        finalSweepResult = sweepResult;
    }

    if ((hitLeftVerticalGoalpost) || (hitRightVerticalGoalpost) || (hitHorizontalGoalpost))
    {

        if (hitLeftVerticalGoalpost != 0)
        {
            goalPost0Location = leftPostPos;
            goalPost0Location.z = startPos.z;

            goalPost1Location = leftPostPos;
            goalPost1Location.z = endPos.z;
        }
        else if (hitRightVerticalGoalpost != 0)
        {
            goalPost0Location = rightPostPos;
            goalPost0Location.z = startPos.z;

            goalPost1Location = rightPostPos;
            goalPost1Location.z = endPos.z;
        }
        else if (hitHorizontalGoalpost != 0)
        {
            goalPost0Location = rightPostPos;
            goalPost0Location.y = startPos.y;
            goalPost0Location.z = cNet::m_fNetHeight;

            goalPost1Location = rightPostPos;
            goalPost1Location.y = endPos.y;
            goalPost1Location.z = cNet::m_fNetHeight;
        }

        nlVec3Sub(goalPostSpherePos0, endPos, startPos);
        nlVec3ScaleAdd(contactPos, finalSweepResult, goalPostSpherePos0, startPos);

        if (hitHorizontalGoalpost != 0)
        {
            goalPost0Location.y = contactPos.y;
        }
        else
        {
            goalPost0Location.z = contactPos.z;
        }

        nlVec3Set(contactNormal, contactPos.x - goalPost0Location.x, contactPos.y - goalPost0Location.y, contactPos.z - goalPost0Location.z);

        float normalLength = nlRecipSqrt(contactNormal.x * contactNormal.x + contactNormal.y * contactNormal.y + contactNormal.z * contactNormal.z, true);

        nlVec3Scale(contactNormal, normalLength);
        if (nlGetLengthSquared3D(contactPos.x - goalPost0Location.x, contactPos.y - goalPost0Location.y, contactPos.z - goalPost0Location.z) < ((ballRadius + netPostRadius) * (ballRadius + netPostRadius)))
        {
            nlVec3ScaleAdd(contactPos, ballRadius + netPostRadius, contactNormal, goalPost0Location);
        }

        cBall* ball = g_pBall;
        ball->m_unk_0xA6 = false;
        ball->mpDamageTarget = NULL;

        nlVector3 ballLinearVelocity = g_pBall->m_pPhysicsBall->GetLinearVelocity();

        float velocitySquared = ballLinearVelocity.x * ballLinearVelocity.x + ballLinearVelocity.y * ballLinearVelocity.y + ballLinearVelocity.z * ballLinearVelocity.z;

        if (velocitySquared > 25.0f)
        {
            Event* event = g_pEventManager->CreateValidEvent(0x2E, 0x34);
            pEventData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&event->m_data) CollisionBallGoalpostData();

            pEventData->v3CollisionVelocity = ballLinearVelocity;
            pEventData->v3CollisionPosition = contactPos;
            pEventData->uTeamIndex = (g_pBall->m_v3Position.x < 0.0f) ? 0 : 1;
        }
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x5C | 0x8013A44C | size: 0x74
 */
void PhysicsNet::StaticInit(CollisionSpace* pCollisionSpace)
{
    spPhysNetPositiveX = new (nlMalloc(sizeof(PhysicsNet), 8, false)) PhysicsNet(g_CollisionSpace, true);
    spPhysNetNegativeX = new (nlMalloc(sizeof(PhysicsNet), 8, false)) PhysicsNet(g_CollisionSpace, false);
}

/**
 * Offset/Address/Size: 0x0 | 0x8013A3F0 | size: 0x5C
 */
void PhysicsNet::StaticDestroy()
{
    delete spPhysNetNegativeX;
    delete spPhysNetPositiveX;
}
