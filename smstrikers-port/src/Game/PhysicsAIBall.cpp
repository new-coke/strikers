#include "Game/Physics/PhysicsAIBall.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Fielder.h"
#include "Game/Ball.h"
#include "Game/CharacterTemplate.h"
#include "Game/Field.h"
#include "Game/FixedUpdateTask.h"
#include "Game/Goalie.h"
#include "Game/Physics/PhysicsFakeBall.h"
#include "Game/Physics/PhysicsGoalie.h"
#include "Game/Physics/PhysicsNet.h"

extern CollisionSpace* g_CollisionSpace;
extern PhysicsWorld* g_PhysicsWorld;

static float sfMaxBallBounceSpeed = 20.0f;
static float sfBallGoalieSweepTestVelocityThreshold = 5.0f;

/**
 * Offset/Address/Size: 0x1200 | 0x80134C34 | size: 0x68
 */
PhysicsAIBall::PhysicsAIBall(float radius)
    : PhysicsBall(g_CollisionSpace, g_PhysicsWorld, radius)
{
    m_pAIBall = NULL;
    m_goalieContactFramesAgo = 9999;
    mbIsInsideNet = false;
    m_v3PrevPosition.x = 0.f;
    m_v3PrevPosition.y = 0.f;
    m_v3PrevPosition.z = 0.f;
}

/**
 * Offset/Address/Size: 0xD84 | 0x801347B8 | size: 0x47C
 */
ContactType PhysicsAIBall::Contact(PhysicsObject* obj, dContact* info, int numContacts)
{
    extern bool gbEnableBallGoalieSweepTest;

    int objID;
    cFielder* pFielder;
    nlVector3 ballPosition;
    float radius;
    unsigned char hitWall;
    int i;
    nlVector3 ballVelocity;
    nlPolar aBallSpeed;
    CollisionBallWallData* pEventData;
    float scale;
    nlVector3 vel;

    objID = obj->GetObjectType();

    if (objID == 0xD || objID == 0xE)
    {
        if (gbEnableBallGoalieSweepTest)
        {
            PhysicsCharacter* physicsCharacter = (PhysicsCharacter*)obj->m_parentObject;
            pFielder = (cFielder*)physicsCharacter->m_pAICharacter;

            if (pFielder->m_eClassType == 3)
            {
                if (mbBallSpeedBelowSweepTestThreshold == 0)
                {
                    return NO_CONTACT;
                }
            }
            else if (pFielder->m_eClassType == 2)
            {
                bool isShootToScore = m_pAIBall->m_tShotTimer.m_uPackedTime != 0 && m_pAIBall->mbCanDamage;

                if (isShootToScore)
                {
                    if (((cPlayer*)pFielder)->m_tNoPickupTimer.m_uPackedTime == 0)
                    {
                        Event* event = g_pEventManager->CreateValidEvent(0x28, 0x20);
                        CollisionPlayerShootToScoreBallData* shootData = new (&event->m_data) CollisionPlayerShootToScoreBallData();
                        shootData->pFielder = pFielder;
                        shootData->pBall = m_pAIBall;
                    }
                    return NO_CONTACT;
                }
            }
        }
    }

    if (objID == 0x1A)
    {
        bool canDamage = false;
        cBall* pBall = m_pAIBall;
        if (pBall->m_tShotTimer.m_uPackedTime != 0)
        {
            if (pBall->mbCanDamage)
            {
                canDamage = true;
            }
        }

        if (canDamage)
        {
            return NO_CONTACT;
        }
    }

    if (PhysicsNet::sbSweepTestEnabled)
    {
        if (PhysicsNet::IsAGoalPost(obj))
        {
            return NO_CONTACT;
        }
    }

    if (m_parentObject == NULL)
    {
        if (objID == 0x19 || objID == 0x5)
        {
            if (mbIsInsideNet)
            {
                return NO_CONTACT;
            }

            GetPosition(&ballPosition);
            radius = GetRadius();

            if ((float)fabs(ballPosition.x) > cField::GetGoalLineX(1u) - 2.0f * radius)
            {
                float fNetWidth = cNet::m_fNetWidth;
                double fAbsBallY = __fabs(ballPosition.y);
                float fNetLimitY = 0.5f * fNetWidth - radius;
                if ((float)fAbsBallY < fNetLimitY)
                {
                    double fBallZ = ballPosition.z;
                    float fNetHeight = cNet::m_fNetHeight;
                    double fAbsBallZ = __fabs(fBallZ);
                    float fNetLimit = fNetHeight - radius;
                    if ((float)fAbsBallZ < fNetLimit)
                    {
                        return NO_CONTACT;
                    }
                }
                mbGoalPlaneContact = true;
            }

            cBall* pBall = m_pAIBall;
            if (pBall->m_tNoPickupTimer.m_uPackedTime != 0)
            {
                if (pBall->m_pPassTarget != NULL || pBall->m_tShotTimer.m_uPackedTime != 0)
                {
                    return NO_CONTACT;
                }
            }

            dContact* pContact = info;
            hitWall = 0;
            for (i = 0; i < numContacts; i++)
            {
                if (pContact->geom.normal[2] < 0.08f)
                {
                    hitWall = 1;
                    break;
                }
                pContact++;
            }

            if (hitWall || objID == 0x5)
            {
                pBall->m_unk_0xA6 = false;
                pBall->mpDamageTarget = NULL;

                ballVelocity = GetLinearVelocity();
                float velY = ballVelocity.y;

                nlCartesianToPolar(aBallSpeed, ballVelocity.x, velY);

                if (aBallSpeed.r > 1.0f)
                {
                    Event* event = g_pEventManager->CreateValidEvent(0x20, 0x3C);
                    pEventData = new (&event->m_data) CollisionBallWallData();
                    pEventData->pBall = m_pAIBall;

                    bool bIsShot = m_pAIBall->m_tShotTimer.m_uPackedTime != 0 && m_pAIBall->m_unk_0xA4;

                    pEventData->bIsPerfect = bIsShot;

                    float speedSq = (ballVelocity.z * ballVelocity.z) + ((ballVelocity.x * ballVelocity.x) + (velY * velY));

                    u32 shotTimer = m_pAIBall->m_tShotTimer.m_uPackedTime;
                    pEventData->bIsShot = (shotTimer != 0);

                    float posY;
                    float posZ = info->geom.pos[2];
                    posY = info->geom.pos[1];
                    float posX = info->geom.pos[0];
                    pEventData->position.x = posX;
                    pEventData->position.y = posY;
                    pEventData->position.z = posZ;

                    float normalY;
                    float normalZ = info->geom.normal[2];
                    normalY = info->geom.normal[1];
                    float normalX = info->geom.normal[0];
                    pEventData->normal.x = normalX;
                    pEventData->normal.y = normalY;
                    pEventData->normal.z = normalZ;

                    pEventData->fCollisionVecLen = nlSqrt(speedSq, true);

                    ScaleAngularVelocity(0.9f);
                    m_pAIBall->ClearBallBlur();
                }

                if (aBallSpeed.r > sfMaxBallBounceSpeed)
                {
                    scale = sfMaxBallBounceSpeed / aBallSpeed.r;
                    vel.x = scale * ballVelocity.x;
                    vel.y = scale * velY;
                    vel.z = scale * ballVelocity.z;
                    SetLinearVelocity(vel);
                }
            }
        }
        else if (objID == 0x7)
        {
            if (!mbIsInsideNet)
            {
                return NO_CONTACT;
            }

            if (PhysicsNet::IsAGoalWall(obj))
            {
                info->surface.soft_cfm = PhysicsNet::sfWallSoftness;
            }
        }
    }

    return PhysicsBall::Contact(obj, info, numContacts);
}

/**
 * Offset/Address/Size: 0xD38 | 0x8013476C | size: 0x4C
 */
void PhysicsAIBall::PreUpdate()
{
    PhysicsBall::PreUpdate();
    m_v3PrevPosition = GetPosition();
}

/**
 * Offset/Address/Size: 0x994 | 0x801343C8 | size: 0x3A4
 */
void PhysicsAIBall::PostUpdate()
{
    extern bool gbEnableBallGoalieSweepTest;

    nlVector3 v3IncidentVel;
    CollisionBallGroundData* pEventData;
    nlVector3 ballPosition;
    nlVector3 oldPosition;
    nlVector3 newPosition;

    GetLinearVelocity(&v3IncidentVel);
    PhysicsBall::PostUpdate();

    if (m_bIsSupportedByGround)
    {
        cBall* pBallFields = m_pAIBall;
        if (pBallFields->m_tNoPickupTimer.m_uPackedTime == 0)
        {
            pBallFields->m_unk_0xA6 = false;
            pBallFields->mpDamageTarget = NULL;

            if (v3IncidentVel.z < -1.0f)
            {
                pEventData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&g_pEventManager->CreateValidEvent(0x24, 0x3C)->m_data) CollisionBallGroundData();

                CollisionBallGroundData* pGroundData = (CollisionBallGroundData*)pEventData;
                s32 bIsShot = 0;

                pGroundData->pBall = m_pAIBall;

                cBall* pEventBallFields = pGroundData->pBall;
                if (pEventBallFields->m_tShotTimer.m_uPackedTime != 0)
                {
                    if (pEventBallFields->m_unk_0xA4 != 0)
                    {
                        bIsShot = 1;
                    }
                }

                if ((u8)bIsShot)
                {
                    pGroundData->bIsShot = 1;
                    m_pAIBall->ClearBallBlur();
                }
                else
                {
                    pGroundData->bIsShot = 0;
                }

                GetPosition(&pGroundData->position);
                pGroundData->normal.x = 0.0f;
                pGroundData->normal.y = 0.0f;
                pGroundData->normal.z = 1.0f;
                pGroundData->fVecZComponent = v3IncidentVel.z;
            }
        }
    }

    if (gbEnableBallGoalieSweepTest)
    {
        CheckIfBallWentThroughGoalie();
    }

    if (PhysicsNet::sbSweepTestEnabled)
    {
        CheckIfBallWentThroughGoalPost();
    }

    mbGoalPlaneContact = false;

    GetRadius();
    GetPosition(&ballPosition);

    {
        f32 radius = g_pBall->m_pPhysicsBall->GetRadius();
        f64 absX = (float)fabs(ballPosition.x);
        f32 threshold = cField::GetGoalLineX((unsigned int)1);
        f32 sum;
        f32 fAbsX;
        sum = radius + threshold;
        fAbsX = (f32)absX;
        threshold = sum - 0.08f;

        if (fAbsX < threshold)
        {
            mbIsInsideNet = false;
        }
        else
        {
            GetPosition(&newPosition);
            oldPosition = m_pAIBall->m_v3PrevPosition;

            if (DidBallJustEnterNet(oldPosition, newPosition))
            {
                mbIsInsideNet = true;
            }
        }
    }

    const nlVector3& v3Vel = GetLinearVelocity();
    const float velocitySq = (v3Vel.x * v3Vel.x) + (v3Vel.y * v3Vel.y) + (v3Vel.z * v3Vel.z);
    bool& bSpeedBelowThreshold = mbBallSpeedBelowSweepTestThreshold;
    bSpeedBelowThreshold = velocitySq < (sfBallGoalieSweepTestVelocityThreshold * sfBallGoalieSweepTestVelocityThreshold);
}

/**
 * Offset/Address/Size: 0x534 | 0x80133F68 | size: 0x460
 */
void PhysicsAIBall::CheckIfBallWentThroughGoalie()
{
    if (mbBallSpeedBelowSweepTestThreshold != 0)
    {
        return;
    }

    nlVector3 oldPosition;
    nlVector3 newPosition;

    GetPosition(&newPosition);
    oldPosition = m_v3PrevPosition;

    cPlayer* pGoaliePlayer = (cPlayer*)g_pCharacters[8];
    Goalie* pGoalie = (Goalie*)pGoaliePlayer;

    if ((newPosition.x * pGoaliePlayer->m_v3Position.x) < 0.0f)
    {
        pGoalie = (Goalie*)g_pCharacters[9];
    }

    if (pGoalie->m_pBall != NULL)
    {
        return;
    }

    if (pGoalie->m_tNoPickupTimer.m_uPackedTime != 0)
    {
        return;
    }

    nlVector3 ballPosition = { 0.0f, 0.0f, 0.0f };
    nlVector3 contactNormal = { 0.0f, 0.0f, 0.0f };
    bool contact = false;

    if ((s32)m_goalieContactFramesAgo > 3)
    {
        float radius = GetRadius();
        contact = pGoalie->GetPhysicsGoalie()->SweepTestForBallContact(oldPosition, newPosition, GetLinearVelocity(), radius, ballPosition, contactNormal);
    }

    if (contact)
    {
        SetPosition(ballPosition, PhysicsObject::WORLD_COORDINATES);

        dContact contactInfo;
        SetContactInfo(&contactInfo, pGoalie->m_pPhysicsCharacter, true);

        contactInfo.geom.normal[0] = contactNormal.x;
        contactInfo.geom.normal[1] = contactNormal.y;
        contactInfo.geom.normal[2] = contactNormal.z;
        contactInfo.geom.g1 = m_geomID;
        contactInfo.geom.g2 = NULL;

        nlVector3 contactPos;
        nlVec3ScaleAdd(contactPos, -GetRadius(), contactNormal, ballPosition);

        contactInfo.geom.pos[1] = contactPos.y;
        contactInfo.geom.pos[2] = contactPos.z;
        contactInfo.geom.pos[0] = contactPos.x;
        contactInfo.geom.depth = 0.0f;

        if (!pGoalie->PreCollideWithBallCallback(contactInfo))
        {
            return;
        }

        FakeBallWorld::InvalidateBallCache();
        m_bUseMagnusEffect = false;

        nlVector3 v3Vel;
        GetLinearVelocity(&v3Vel);

        Event* event = g_pEventManager->CreateValidEvent(0x27, 0x30);
        CollisionPlayerBallData* pEventData = new (&event->m_data) CollisionPlayerBallData();

        m_pAIBall->ClearBallBlur();

        CollisionPlayerBallData* pCollisionData = (CollisionPlayerBallData*)pEventData;
        pCollisionData->pPlayer = pGoalie;
        pCollisionData->pBall = m_pAIBall;
        pCollisionData->velocity = v3Vel;
        pCollisionData->boneID = 0;
        m_goalieContactFramesAgo = 0;

        const float normalY = contactNormal.y;
        const float normalX = contactNormal.x;
        float normalZ;
        const float velX = v3Vel.x;
        const float velY = v3Vel.y;
        const float velZ = v3Vel.z;
        const float normalYSq = normalY * normalY;
        const float velYNormalY = velY * normalY;
        const float normalLenXY = (normalX * normalX) + normalYSq;
        normalZ = contactNormal.z;
        const float dotXY = (velX * normalX) + velYNormalY;
        const float normalLengthSq = (normalZ * normalZ) + normalLenXY;
        const float velDotNormal = (velZ * normalZ) + dotXY;
        const float reflectScale = velDotNormal / normalLengthSq;

        nlVector3 v3ExitVel;
        v3ExitVel.x = (-2.0f * (reflectScale * normalX)) + velX;
        v3ExitVel.y = (-2.0f * (reflectScale * normalY)) + velY;
        v3ExitVel.z = (-2.0f * (reflectScale * normalZ)) + velZ;

        SaveData* pSaveData = pGoalie->mpSaveData;
        if (pSaveData != NULL && (pSaveData->muSaveType & 0x38) != 0)
        {
            nlVector3 v3DeflectFudge;
            RotateVectorZAxis(v3DeflectFudge, v3ExitVel, (u16)-pGoalie->m_aActualFacingDirection);

            float exitSpeed = nlSqrt(
                (v3ExitVel.x * v3ExitVel.x) + (v3ExitVel.y * v3ExitVel.y) + (v3ExitVel.z * v3ExitVel.z),
                true);

            v3DeflectFudge.x += 0.5f;

            float saveY = pSaveData->mv3SavePos.y;
            v3DeflectFudge.y = saveY;

            if (saveY > 0.0f)
            {
                v3DeflectFudge.y += 2.0f;
            }
            else
            {
                v3DeflectFudge.y -= 2.0f;
            }

            v3DeflectFudge.z = 0.5f + pSaveData->mv3SavePos.z;
            v3DeflectFudge.y = v3DeflectFudge.y * (exitSpeed * (0.2f + nlRandomf(0.1f, &nlDefaultSeed)));
            v3DeflectFudge.z = v3DeflectFudge.z * (exitSpeed * (0.15f + nlRandomf(0.05f, &nlDefaultSeed)));

            RotateVectorZAxis(v3DeflectFudge, v3DeflectFudge, pGoalie->m_aActualFacingDirection);

            float zero = 0.0f;
            float one = 1.0f;
            v3ExitVel.x = (zero * v3ExitVel.x) + (one * v3DeflectFudge.x);
            v3ExitVel.y = (zero * v3ExitVel.y) + (one * v3DeflectFudge.y);
            v3ExitVel.z = (zero * v3ExitVel.z) + (one * v3DeflectFudge.z);
        }

        float scale = 0.175f;
        v3ExitVel.z = scale * v3ExitVel.z;
        float scaledY = scale * v3ExitVel.y;
        float scaledX = scale * v3ExitVel.x;
        v3ExitVel.x = scaledX;
        v3ExitVel.y = scaledY;

        SetLinearVelocity(v3ExitVel);
    }

    m_goalieContactFramesAgo += 1;
}

/**
 * Offset/Address/Size: 0x224 | 0x80133C58 | size: 0x310
 */
void PhysicsAIBall::CheckIfBallWentThroughGoalPost()
{
    if (m_parentObject == NULL)
    {
        nlVector3 oldPosition;
        nlVector3 newPosition;

        GetPosition(&newPosition);
        oldPosition = m_v3PrevPosition;

        nlVector3 ballPosition = { 0.0f, 0.0f, 0.0f };
        nlVector3 contactNormal = { 0.0f, 0.0f, 0.0f };
        bool contact;
        PhysicsObject* physicsObject = NULL;
        nlVector3 v3ExitVel;
        nlVector3 v3AngVel;

        if (oldPosition.x > 0.0f)
        {
            PhysicsNet* pNet = PhysicsNet::spPhysNetPositiveX;
            float radius = GetRadius();
            contact = pNet->SweepTestForBallContact(oldPosition, newPosition, GetLinearVelocity(), radius, ballPosition, contactNormal, &physicsObject);
        }
        else
        {
            PhysicsNet* pNet = PhysicsNet::spPhysNetNegativeX;
            float radius = GetRadius();
            contact = pNet->SweepTestForBallContact(oldPosition, newPosition, GetLinearVelocity(), radius, ballPosition, contactNormal, &physicsObject);
        }

        if ((contact != 0) && (mbGoalPlaneContact == 0))
        {
            float contactZ = (0.005f * contactNormal.z) + ballPosition.z;
            float contactY = (0.005f * contactNormal.y) + ballPosition.y;
            float contactX = (0.005f * contactNormal.x) + ballPosition.x;
            ballPosition.x = contactX;
            ballPosition.y = contactY;
            ballPosition.z = contactZ;

            const nlVector3& v3BallVel = GetLinearVelocity();
            float velDotNormal = nlVec3DotProduct(v3BallVel, contactNormal);
            float normalLengthSq = nlVec3DotProduct(contactNormal, contactNormal);
            float reflectScale = velDotNormal / normalLengthSq;

            nlVec3Set(v3ExitVel,
                (-2.0f * (reflectScale * contactNormal.x)) + v3BallVel.x,
                (-2.0f * (reflectScale * contactNormal.y)) + v3BallVel.y,
                (-2.0f * (reflectScale * contactNormal.z)) + v3BallVel.z);

            nlVec3Scale(v3ExitVel, 0.35f);

            float velocitySq = nlVec3DotProduct(v3BallVel, v3BallVel);

            if (velocitySq < 1.0f)
            {
                if (ballPosition.x > 0.0f)
                {
                    v3ExitVel.x = v3ExitVel.x - 0.3f;
                }
                else
                {
                    v3ExitVel.x += 0.3f;
                }

                float physicsTick = FixedUpdateTask::GetPhysicsUpdateTick();
                float dt = 0.3f * physicsTick;

                ballPosition.z = (dt * v3ExitVel.z) + ballPosition.z;
                ballPosition.y = (dt * v3ExitVel.y) + ballPosition.y;
                ballPosition.x = (dt * v3ExitVel.x) + ballPosition.x;
            }

            GetAngularVelocity(&v3AngVel);

            v3AngVel.z = 0.8f * v3AngVel.z;
            float scaledAngY = 0.8f * v3AngVel.y;
            float scaledAngX = 0.8f * v3AngVel.x;
            v3AngVel.x = scaledAngX;
            v3AngVel.y = scaledAngY;

            SetPosition(ballPosition, PhysicsObject::WORLD_COORDINATES);
            SetLinearVelocity(v3ExitVel);
            SetAngularVelocity(v3AngVel);

            m_bUseMagnusEffect = false;
            FakeBallWorld::InvalidateBallCache();

            m_pAIBall->m_bBallPathChangeCount += 1;
            cBall* pAIBall = m_pAIBall;
            pAIBall->m_unk_0xA6 = false;
            pAIBall->mpDamageTarget = NULL;
        }
    }
}

/**
 * Offset/Address/Size: 0x84 | 0x80133AB8 | size: 0x1A0
 */
bool PhysicsAIBall::DidBallJustEnterNet(const nlVector3& oldPosition, const nlVector3 newPosition)
{
    float absOldX = std::fabs(oldPosition.x);
    float absNewX = std::fabs(newPosition.x);
    float goalLineX = cField::GetGoalLineX(1U) + g_pBall->GetPhysicsBall()->GetRadius() - 0.08f;

    if ((absOldX < goalLineX) && (absNewX >= goalLineX))
    {
        nlVector3 interpolatedPosition;
        float deltaX;

        deltaX = newPosition.x - oldPosition.x;

        if (std::fabs(deltaX) > 0.0001f)
        {
            float alpha = (newPosition.x > 0.0f) ? goalLineX : -goalLineX;
            alpha = (alpha - oldPosition.x) / deltaX;
            nlVecLerp(interpolatedPosition, oldPosition, newPosition, alpha);
        }
        else
        {
            interpolatedPosition = newPosition;
        }

        if ((interpolatedPosition.z > 0.0f) && (interpolatedPosition.z < cNet::m_fNetHeight)
            && (interpolatedPosition.y > (0.5f * -cNet::m_fNetWidth))
            && (interpolatedPosition.y < (0.5f * cNet::m_fNetWidth)))
        {
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x0 | 0x80133A34 | size: 0x84
 */
bool PhysicsAIBall::IsBallOutsideNet(const nlVector3& ballPosition)
{
    float radius = g_pBall->GetPhysicsBall()->GetRadius();
    return std::fabs(ballPosition.x) < cField::GetGoalLineX(1U) + radius - 0.08f;
}
