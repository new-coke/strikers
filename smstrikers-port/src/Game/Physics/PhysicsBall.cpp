#include "Game/Physics/PhysicsBall.h"
#include "Game/Physics/CollisionSpace.h"

#include "NL/nlFont.h"
#include "NL/nlMath.h"
#include "Game/Physics/PhysicsObject.h"
#include "Game/Physics/PhysicsFakeBall.h"

#include "Game/FixedUpdateTask.h"

#include "Game/Ball.h"

float g_BallFriction = 3.f;
float g_BallFrictionWall = 4.f;
float g_BallBounce = 0.25f;
float g_BallBounceGround = 0.375f;
float g_BallBounceWall = 0.45f;
float g_BallRollingResistance = 5.f;
float g_BallAirResistance = 0.1f;
static const float kZeroF[1] = { 0.0f };

/**
 * Offset/Address/Size: 0x0 | 0x80134D14 | size: 0xD4
 */
void PhysicsBall::CalcAngularFromLinearVelocity(nlVector3& v3AngularVel)
{
    nlVector3 v3Velocity;
    GetLinearVelocity(&v3Velocity);

    nlVector3 v3Up = { 0.0f, 0.0f, 0.0f };
    v3Up.z = 1.0f / GetRadius();
    nlVector3 v3Look = { 0.0f, 0.0f, 0.0f };

    v3Look.x = v3Velocity.x;
    v3Look.y = v3Velocity.y;

    nlVector3 v3Cross;
    nlVec3CrossProductAlt(v3Cross, v3Up, v3Look);
    v3AngularVel.x = v3Cross.z;
    v3AngularVel.y = v3Cross.y;
    v3AngularVel.z = v3Cross.x;
}

/**
 * Offset/Address/Size: 0xD4 | 0x80134DE8 | size: 0x28
 */
void PhysicsBall::SetUseAngularVelocity(bool param_1)
{
    m_bUseAngularVel = 0;
    if (param_1)
    {
        m_fSpinTimer = 0.08f;
        return;
    }
    m_fSpinTimer = kZeroF[0];
}

/**
 * Offset/Address/Size: 0xFC | 0x80134E10 | size: 0x80
 */
void PhysicsBall::ScaleAngularVelocity(float scale)
{
    nlVector3 v;
    if (m_bUseAngularVel != 0)
    {
        GetAngularVelocity(&v);
        nlVec3Scale(v, scale);
        SetAngularVelocity(v);
    }
}

void PhysicsBall::CalcSurfaceVelocity(nlVector3& v3VelocityOut)
{
    nlVector3 v3AngVelocity;
    GetAngularVelocity(&v3AngVelocity);
    v3AngVelocity.z = kZeroF[0];

    nlVector3 v3Up = { 0.0f, 0.0f, 0.0f };
    v3Up.z = GetRadius();

    f32 x = (v3AngVelocity.y * v3Up.z) - (v3AngVelocity.z * v3Up.y);
    f32 y = (-v3AngVelocity.x * v3Up.z) + (v3AngVelocity.z * v3Up.x);
    f32 z = (v3AngVelocity.x * v3Up.y) - (v3AngVelocity.y * v3Up.x);
    nlVec3Set(v3VelocityOut, x, y, z);
}

/**
 * Offset/Address/Size: 0x17C | 0x80134E90 | size: 0x51C
 */
void PhysicsBall::AddResistanceForces()
{
    nlVector3 velocity;
    nlVector3 resistance;
    nlVector3 v3CurAngularVel;
    nlVector3 v3BallSurfaceSpeed;
    nlVector3 v3CurBallSpeed;
    nlVector3 v3CurLinVel;
    nlVector3 v3MagnusForce;
    nlVector3 v3CurAngVel;
    u8 bApply;

    velocity = GetLinearVelocity();
    if (m_parentObject == NULL)
    {
        if (m_bIsSupportedByGround != 0 && m_bUseAngularVel == 0)
        {
            f32 speed = nlSqrt(velocity.z * velocity.z + (velocity.x * velocity.x + velocity.y * velocity.y), true);
            if (speed > 0.01f)
            {
                f32 factor = -g_BallRollingResistance / speed;
                resistance.x = factor * velocity.x;
                resistance.y = factor * velocity.y;
                resistance.z = factor * velocity.z;
                AddForceAtCentreOfMass(resistance);
            }
        }
        f32 drag = -g_BallAirResistance;
        resistance.x = drag * velocity.x;
        resistance.y = drag * velocity.y;
        resistance.z = drag * velocity.z;
        AddForceAtCentreOfMass(resistance);
    }
    if (m_bUseTiltForce != 0 && g_pBall->m_pPassTarget == NULL)
    {
        bApply = 0;
        if (g_pBall->m_tShotTimer.m_uPackedTime != 0 && g_pBall->mbCanDamage != 0)
            bApply = 1;
        if (bApply == 0)
            AddForceAtCentreOfMass(m_v3TiltForce);
    }
    if (m_fSpinTimer > kZeroF[0])
    {
        m_fSpinTimer = m_fSpinTimer - FixedUpdateTask::GetPhysicsUpdateTick();
        if (m_fSpinTimer <= kZeroF[0])
            m_bUseAngularVel = 1;
    }
    if (m_parentObject == NULL && m_bUseAngularVel != 0)
    {
        f32 threshold = 0.02f + GetRadius();
        if (GetPosition().z < threshold)
        {
            nlVector3 v3DesiredAngularVel;
            CalcAngularFromLinearVelocity(v3DesiredAngularVel);
            GetAngularVelocity(&v3CurAngularVel);
            f32 torqueX;
            f32 torqueY;
            f32 torqueZ;
            torqueZ = 0.25f * (v3DesiredAngularVel.z - v3CurAngularVel.z);
            torqueY = 0.25f * (v3DesiredAngularVel.y - v3CurAngularVel.y);
            torqueX = 0.25f * (v3DesiredAngularVel.x - v3CurAngularVel.x);
            dBodyAddTorque(m_bodyID, torqueX, torqueY, torqueZ);
            CalcSurfaceVelocity(v3BallSurfaceSpeed);
            GetLinearVelocity(&v3CurBallSpeed);
            v3BallSurfaceSpeed.z = v3BallSurfaceSpeed.z - v3CurBallSpeed.z;
            v3BallSurfaceSpeed.y = v3BallSurfaceSpeed.y - v3CurBallSpeed.y;
            v3BallSurfaceSpeed.x = v3BallSurfaceSpeed.x - v3CurBallSpeed.x;
            nlVec3Scale(v3BallSurfaceSpeed, 5.f);
            AddForceAtCentreOfMass(v3BallSurfaceSpeed);
            v3BallSurfaceSpeed.z = kZeroF[0];
            if (torqueX * torqueX + torqueY * torqueY + torqueZ * torqueZ < 0.0001f
                && v3BallSurfaceSpeed.x * v3BallSurfaceSpeed.x + v3BallSurfaceSpeed.y * v3BallSurfaceSpeed.y + v3BallSurfaceSpeed.z * v3BallSurfaceSpeed.z < 0.00003f)
                m_bUseAngularVel = 0;
        }
    }
    if (m_parentObject == NULL && m_bUseMagnusEffect != 0)
    {
        f32 threshold = 0.02f + GetRadius();
        nlVector3& pos = GetPosition();
        if (pos.z > threshold)
        {
            GetLinearVelocity(&v3CurLinVel);
            if (v3CurLinVel.x * v3CurLinVel.x + v3CurLinVel.y * v3CurLinVel.y + v3CurLinVel.z * v3CurLinVel.z > 1.f)
            {
                GetAngularVelocity(&v3CurAngVel);
                if (v3CurAngVel.x * v3CurAngVel.x + v3CurAngVel.y * v3CurAngVel.y + v3CurAngVel.z * v3CurAngVel.z > 1.f)
                {
                    nlVec3Cross(v3MagnusForce, v3CurAngVel, v3CurLinVel);
                    v3MagnusForce.x *= 0.075f;
                    v3MagnusForce.y *= 0.075f;
                    v3MagnusForce.z *= 0.04f;
                    AddForceAtCentreOfMass(v3MagnusForce);
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x698 | 0x801353AC | size: 0x250
 */
ContactType PhysicsBall::Contact(PhysicsObject* other, dContact* contact, int param)
{
    nlVector3 pos;
    nlVector3 _pos;

    f32 temp_f0;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f2;
    f32 temp_f29;
    f32 temp_f30;
    f32 temp_f31;
    s32 objType;
    s32 i;
    dContact* c;

    objType = other->GetObjectType();
    GetPosition(&pos);

    if (objType == 0x11)
    {
        c = contact;
        for (i = 0; i < param; i++)
        {
            if ((c->geom.pos[2] <= pos.z) && (c->geom.normal[2] > 0.9f))
            {
                m_bIsSupportedByGround = 1;
                break;
            }
            c++;
        }
    }

    if (m_parentObject != NULL)
    {
        if (objType == 0x11)
        {
            GetPosition(&pos);
            if ((contact->geom.normal[2] > kZeroF[0]) && ((contact->geom.pos[2] + GetRadius()) < pos.z))
            {
                _pos = GetPosition();

                temp_f2 = contact->geom.normal[2];
                temp_f1 = contact->geom.depth;
                temp_f31 = temp_f2 * temp_f1;
                _pos.z += temp_f31;
                SetPosition(_pos, WORLD_COORDINATES);

                if (contact->geom.normal[2] > 0.95f)
                {
                    return NO_CONTACT;
                }

                temp_f30 = contact->geom.normal[1];
                temp_f29 = contact->geom.normal[0];
                temp_f1 = kZeroF[0];
                temp_f0 = temp_f30 * temp_f30;
                temp_f0 = (temp_f29 * temp_f29) + temp_f0;
                temp_f1 = temp_f1 + temp_f0;
                temp_f1_2 = nlRecipSqrt(temp_f1, true);
                contact->geom.normal[0] = (f32)(temp_f1_2 * temp_f29);
                contact->geom.normal[1] = (f32)(temp_f1_2 * temp_f30);
                temp_f0 = kZeroF[0];
                contact->geom.normal[2] = (f32)(temp_f1_2 * temp_f0);
                contact->geom.depth = (f32)(contact->geom.depth - temp_f31);
            }
        }
        return m_parentObject->Contact(other, contact, param);
    }

    if ((objType != 0x11) && (objType != 0xD) && (objType != 0xE) && (objType != 8))
    {
        cBall* ball;

        m_bUseMagnusEffect = 0;
        FakeBallWorld::InvalidateBallCache();
        g_pBall->m_bBallPathChangeCount = g_pBall->m_bBallPathChangeCount + 1;
        g_pBall->m_bBallDeflectCount = g_pBall->m_bBallDeflectCount + 1;

        ball = g_pBall;
        ball->m_unk_0xA6 = 0;
        ball->mpDamageTarget = NULL;
    }

    return TWO_WAY_CONTACT;
}

/**
 * Offset/Address/Size: 0x8E8 | 0x801355FC | size: 0x78
 */
void PhysicsBall::CloneBall(const PhysicsBall& other)
{
    CloneObject(other);

    u32* src = (u32*)&other.m_v3TiltForce;
    u32* dst = (u32*)&m_v3TiltForce;
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];

    m_bUseTiltForce = other.m_bUseTiltForce;
    m_bIsSupportedByGround = other.m_bIsSupportedByGround;
    m_bUseAngularVel = other.m_bUseAngularVel;
    m_bUseMagnusEffect = other.m_bUseMagnusEffect;
    m_fSpinTimer = other.m_fSpinTimer;
}

/**
 * Offset/Address/Size: 0x960 | 0x80135674 | size: 0x164
 */
void PhysicsBall::PostUpdate()
{
    nlVector3 linVel;
    nlVector3 pos;

    PhysicsObject::PostUpdate();
    GetLinearVelocity(&linVel);

    float l = (linVel.x * linVel.x) + (linVel.y * linVel.y) + (linVel.z * linVel.z);
    if (l > 2500.f)
    {
        const f32 f = 50.f / nlSqrt(l, true);
        nlVec3Set(linVel, f * linVel.x, f * linVel.y, f * linVel.z);
        SetLinearVelocity(linVel);
    }

    if ((GetPosition().z > 20.f) && (linVel.z > kZeroF[0]))
    {
        linVel.z *= 0.9f;
        SetLinearVelocity(linVel);
    }

    if (GetPosition().z < GetRadius())
    {
        m_bIsSupportedByGround = 1;
        GetPosition(&pos);
        pos.z = GetRadius();
        SetPosition(pos, WORLD_COORDINATES);

        linVel.z = linVel.z * -g_BallBounceGround;
        SetLinearVelocity(linVel);
    }
}

/**
 * Offset/Address/Size: 0xAC4 | 0x801357D8 | size: 0xAC
 */
void PhysicsBall::PreUpdate()
{
    nlVector3 vec;
    GetLinearVelocity(&vec);

    float l = (vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z);
    if (l > 2500.f)
    {
        float n = 50.f / nlSqrt(l, true);
        nlVec3Set(vec, n * vec.x, n * vec.y, n * vec.z);
        SetLinearVelocity(vec);
    }
    PhysicsObject::PreUpdate();
    AddResistanceForces();
}

/**
 * Offset/Address/Size: 0xB70 | 0x80135884 | size: 0xC
 */
void PhysicsBall::PreCollide()
{
    m_bIsSupportedByGround = false;
}

/**
 * Offset/Address/Size: 0xB7C | 0x80135890 | size: 0x114
 */
bool PhysicsBall::SetContactInfo(dContact* contact, PhysicsObject* other, bool param)
{
    if (m_parentObject != NULL)
    {
        return m_parentObject->SetContactInfo(contact, other, param);
    }

    if (param != 0)
    {
        SetDefaultContactInfo(contact);
    }

    if (other->GetObjectType() != 8)
    {
        if (other->GetObjectType() == 0x11)
        {
            contact->surface.bounce = (f32)g_BallBounceGround;
        }
        else if (other->GetObjectType() == 0x19)
        {
            contact->surface.bounce = (f32)g_BallBounceWall;
        }
        else
        {
            contact->surface.bounce = (f32)g_BallBounce;
        }

        contact->surface.bounce_vel = kZeroF[0];
        if (other->GetObjectType() == 0x19)
        {
            contact->surface.mu = (f32)g_BallFrictionWall;
        }
        else
        {
            contact->surface.mu = (f32)g_BallFriction;
        }
    }

    return true;
}

/**
 * Offset/Address/Size: 0xC90 | 0x801359A4 | size: 0x8
 */
float PhysicsBall::GetBallMaxVelocity()
{
    return 50.0;
}

/**
 * Offset/Address/Size: 0xC98 | 0x801359AC | size: 0x88
 */
PhysicsBall::PhysicsBall(CollisionSpace* space, PhysicsWorld* world, float radius)
    : PhysicsSphere(space, world, radius)
{
    m_bUseTiltForce = 0;
    m_bIsSupportedByGround = 0;
    m_bUseAngularVel = 0;
    m_bUseMagnusEffect = 0;
    m_fSpinTimer = kZeroF[0];

    SetCategory(0x20);
    SetCollide(0xaf);

    m_gravity = -14.f;

    m_v3TiltForce.x = kZeroF[0];
    m_v3TiltForce.y = kZeroF[0];
    m_v3TiltForce.z = kZeroF[0];
}
