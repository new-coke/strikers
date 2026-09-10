#include "Game/Physics/PhysicsObject.h"
#include "NL/nlMath.h"

float PhysicsObject::DefaultGravity = -9.8f;


// PORT: what an object with neither geom nor body reads as.
static const float port_no_geom_pos[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static const float port_no_geom_rot[12] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
};

/**
 * Offset/Address/Size: 0x0 | 0x801FFCFC | size: 0x70
 */
void ConvertDMat3ToNLMat4(const float* src, nlMatrix4* dest)
{
    dest->e2[0][0] = src[0];
    dest->e2[1][0] = src[1];
    dest->e2[2][0] = src[2];
    dest->e2[0][1] = src[4];
    dest->e2[1][1] = src[5];
    dest->e2[2][1] = src[6];
    dest->e2[0][2] = src[8];
    dest->e2[1][2] = src[9];
    dest->e2[2][2] = src[10];
    dest->e2[3][0] = 0.0f;
    dest->e2[3][1] = 0.0f;
    dest->e2[3][2] = 0.0f;
    dest->e2[3][3] = 1.0f;
    dest->e2[0][3] = 0.0f;
    dest->e2[1][3] = 0.0f;
    dest->e2[2][3] = 0.0f;
}

/**
 * Offset/Address/Size: 0x70 | 0x801FFD6C | size: 0x1A8
 */
void PhysicsObject::CloneObject(const PhysicsObject& obj)
{
    dMass m2, m1;
    nlMatrix4 m4Rot;
    nlVector3 v3Pos;
    nlVector3 v3LinearVel;
    nlVector3 v3AngularVelocity;
    f32 mass;

    obj.GetPosition(&v3Pos);
    SetPosition(v3Pos, WORLD_COORDINATES);

    obj.GetRotation(&m4Rot);

    dMatrix3 _rot;
    ConvertNLMat4ToDMat3_Transposed(m4Rot, _rot);

    if ((m_geomID == NULL) && (m_bodyID != NULL))
    {
        dBodySetRotation(m_bodyID, _rot);
    }
    else
    {
        // PORT: skip when there is no geom either.
        if (m_geomID != NULL)
        {
            dGeomSetRotation(m_geomID, _rot);
        }
    }

    obj.GetLinearVelocity(&v3LinearVel);
    dBodySetLinearVel(m_bodyID, v3LinearVel.x, v3LinearVel.y, v3LinearVel.z);
    obj.GetAngularVelocity(&v3AngularVelocity);
    dBodySetAngularVel(m_bodyID, v3AngularVelocity.x, v3AngularVelocity.y, v3AngularVelocity.z);
    m_gravity = obj.m_gravity;

    if (obj.m_bodyID != NULL)
    {
        // dMass m1;
        dMassSetZero(&m1);
        dBodyGetMass(obj.m_bodyID, &m1);
        mass = m1.mass;
    }
    else
    {
        mass = -1.f;
    }

    if (m_bodyID != NULL)
    {
        // dMass m2;
        dMassSetZero(&m2);
        dBodyGetMass(m_bodyID, &m2);
        dMassAdjust(&m2, mass);
        dBodySetMass(m_bodyID, &m2);
    }
}

/**
 * Offset/Address/Size: 0x218 | 0x801FFF14 | size: 0x44
 */
ContactType PhysicsObject::Contact(PhysicsObject* obj1, dContact* contact, int param, PhysicsObject* obj2)
{
    if (m_parentObject != NULL)
    {
        return m_parentObject->Contact(obj1, contact, param, this);
    }
    return TWO_WAY_CONTACT;
}

/**
 * Offset/Address/Size: 0x25C | 0x801FFF58 | size: 0x30
 */
ContactType PhysicsObject::Contact(PhysicsObject* obj1, dContact* contact, int param)
{
    return Contact(obj1, contact, param, NULL);
}

/**
 * Offset/Address/Size: 0x28C | 0x801FFF88 | size: 0x4C
 */
void PhysicsObject::MakeStatic()
{
    if (m_bodyID != NULL)
    {
        dBodyDestroy(m_bodyID);
        m_bodyID = NULL;
    }
    dGeomSetBody(m_geomID, NULL);
}

/**
 * Offset/Address/Size: 0x2D8 | 0x801FFFD4 | size: 0x6C
 */
void PhysicsObject::SetMass(float mass)
{
    dMass m;
    if (m_bodyID != nullptr)
    {
        dMassSetZero(&m);
        dBodyGetMass(m_bodyID, &m);
        dMassAdjust(&m, mass);
        dBodySetMass(m_bodyID, &m);
    }
}

/**
 * Offset/Address/Size: 0x344 | 0x80200040 | size: 0x50
 */
void PhysicsObject::Reconnect(dSpaceID space)
{
    dSpaceAdd(space, m_geomID);
    dGeomSetBody(m_geomID, m_bodyID);
    if (m_bodyID != 0)
    {
        dBodyEnable(m_bodyID);
    }
}

/**
 * Offset/Address/Size: 0x394 | 0x80200090 | size: 0x68
 */
dSpaceID PhysicsObject::Disconnect()
{
    dSpaceID space;
    space = dGeomGetSpace(m_geomID);
    if (space != nullptr)
    {
        dSpaceRemove(space, m_geomID);
    }
    dGeomSetBody(m_geomID, 0);
    if (m_bodyID != nullptr)
    {
        dBodyDisable(m_bodyID);
    }
    return space;
}
/**
 * Offset/Address/Size: 0x3FC | 0x802000F8 | size: 0x24
 */
void PhysicsObject::EnableCollisions()
{
    dGeomEnable(m_geomID);
}

/**
 * Offset/Address/Size: 0x420 | 0x8020011C | size: 0x24
 */
void PhysicsObject::DisableCollisions()
{
    dGeomDisable(m_geomID);
}

/**
 * Offset/Address/Size: 0x444 | 0x80200140 | size: 0xD4
 */
void PhysicsObject::SetWorldMatrix(const nlMatrix4& in)
{
    dMatrix3 mat;
    ConvertNLMat4ToDMat3_Transposed(in, mat);

    if ((m_geomID == 0) && (m_bodyID != 0))
    {
        dBodySetRotation(m_bodyID, mat);
    }
    else
    {
        // PORT: skip when there is no geom either.
        if (m_geomID != NULL)
        {
            dGeomSetRotation(m_geomID, mat);
        }
    }

    nlVector3* pos = (nlVector3*)&(in.e2[3][0]);
    SetPosition(*pos, WORLD_COORDINATES);
}

/**
 * Offset/Address/Size: 0x518 | 0x80200214 | size: 0x78
 */
bool PhysicsObject::SetContactInfo(dContact* contact, PhysicsObject* otherObject, bool first)
{
    if (m_parentObject != NULL)
    {
        return m_parentObject->SetContactInfo(contact, otherObject, first);
    }

    if (first != 0)
    {
        (contact->surface).mode = 0x14;
        (contact->surface).soft_cfm = 0.0001f;
        (contact->surface).mu = 0.9f;
        (contact->surface).mu2 = 0.0f;
        (contact->surface).bounce = 0.2f;
        (contact->surface).bounce_vel = 0.1f;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x590 | 0x8020028C | size: 0x34
 */
void PhysicsObject::SetDefaultContactInfo(dContact* contact)
{
    (contact->surface).mode = 0x14;
    (contact->surface).soft_cfm = 0.0001f;
    (contact->surface).mu = 0.9f;
    (contact->surface).mu2 = 0.0f;
    (contact->surface).bounce = .2f;
    (contact->surface).bounce_vel = .1f;
}

/**
 * Offset/Address/Size: 0x5C4 | 0x802002C0 | size: 0x50
 */
void PhysicsObject::ZeroForceAccumulators()
{
    dBodySetForce(m_bodyID, 0.f, 0.f, 0.f);
    dBodySetTorque(m_bodyID, 0.f, 0.f, 0.f);
}

/**
 * Offset/Address/Size: 0x614 | 0x80200310 | size: 0x30
 */
void PhysicsObject::AddForceAtCentreOfMass(const nlVector3& force)
{
    dBodyAddForce(m_bodyID, force.x, force.y, force.z);
}

/**
 * Offset/Address/Size: 0x644 | 0x80200340 | size: 0x1AC
 */
void PhysicsObject::GetAngularVelocity(nlVector3* vel) const
{
    PhysicsObject* parent2;
    PhysicsObject* parent4;
    PhysicsObject* parent6;
    PhysicsObject* parent;
    PhysicsObject* parent3;
    PhysicsObject* parent5;
    PhysicsObject* parent7;

    parent = this->m_parentObject;
    if (parent != NULL)
    {
        parent2 = parent->m_parentObject;
        if (parent2 != NULL)
        {
            parent3 = parent2->m_parentObject;
            if (parent3 != NULL)
            {
                parent4 = parent3->m_parentObject;
                if (parent4 != NULL)
                {
                    parent5 = parent4->m_parentObject;
                    if (parent5 != NULL)
                    {
                        parent6 = parent5->m_parentObject;
                        if (parent6 != NULL)
                        {
                            parent7 = parent6->m_parentObject;
                            if (parent7 != NULL)
                            {
                                if (parent7->m_parentObject != NULL)
                                {
                                    parent7->m_parentObject->GetAngularVelocity(vel);
                                    return;
                                }
                                const float* velFloats = dBodyGetAngularVel(parent7->m_bodyID);
                                vel->Set(velFloats[0], velFloats[1], velFloats[2]);
                                return;
                            }
                            const float* velFloats = dBodyGetAngularVel(parent6->m_bodyID);
                            nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                            return;
                        }
                        const float* velFloats = dBodyGetAngularVel(parent5->m_bodyID);
                        nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                        return;
                    }
                    const float* velFloats = dBodyGetAngularVel(parent4->m_bodyID);
                    nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                    return;
                }
                const float* velFloats = dBodyGetAngularVel(parent3->m_bodyID);
                nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                return;
            }
            const float* velFloats = dBodyGetAngularVel(parent2->m_bodyID);
            nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
            return;
        }
        const float* velFloats = dBodyGetAngularVel(parent->m_bodyID);
        nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
        return;
    }
    const float* velFloats = dBodyGetAngularVel(this->m_bodyID);
    nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
}

/**
 * Offset/Address/Size: 0x7F0 | 0x802004EC | size: 0x30
 */
void PhysicsObject::SetAngularVelocity(const nlVector3& velocity)
{
    dBodySetAngularVel(m_bodyID, (float)velocity.x, (float)velocity.y, (float)velocity.z);
}

/**
 * Offset/Address/Size: 0x820 | 0x8020051C | size: 0x34
 */
nlVector3& PhysicsObject::GetLinearVelocity()
{
    GetLinearVelocity(&m_linearVelocity);
    return m_linearVelocity;
}

/**
 * Offset/Address/Size: 0x854 | 0x80200550 | size: 0x1AC
 */
void PhysicsObject::GetLinearVelocity(nlVector3* vel) const
{
    PhysicsObject* parent2;
    PhysicsObject* parent4;
    PhysicsObject* parent6;

    PhysicsObject* parent;
    PhysicsObject* parent3;
    PhysicsObject* parent5;
    PhysicsObject* parent7;

    parent = m_parentObject;
    if (parent != NULL)
    {
        parent2 = parent->m_parentObject;
        if (parent2 != NULL)
        {
            parent3 = parent2->m_parentObject;
            if (parent3 != NULL)
            {
                parent4 = parent3->m_parentObject;
                if (parent4 != NULL)
                {
                    parent5 = parent4->m_parentObject;
                    if (parent5 != NULL)
                    {
                        parent6 = parent5->m_parentObject;
                        if (parent6 != NULL)
                        {
                            parent7 = parent6->m_parentObject;
                            if (parent7 != NULL)
                            {
                                if ((uintptr_t)parent7->m_parentObject != 0U)
                                {
                                    parent7->m_parentObject->GetLinearVelocity(vel);
                                    return;
                                }
                                float* velFloats = (float*)dBodyGetLinearVel(parent7->m_bodyID);
                                vel->Set(velFloats[0], velFloats[1], velFloats[2]);
                                return;
                            }
                            float* velFloats = (float*)dBodyGetLinearVel(parent6->m_bodyID);
                            nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                            return;
                        }
                        float* velFloats = (float*)dBodyGetLinearVel(parent5->m_bodyID);
                        nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                        return;
                    }
                    float* velFloats = (float*)dBodyGetLinearVel(parent4->m_bodyID);
                    nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                    return;
                }
                float* velFloats = (float*)dBodyGetLinearVel(parent3->m_bodyID);
                nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
                return;
            }
            float* velFloats = (float*)dBodyGetLinearVel(parent2->m_bodyID);
            nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
            return;
        }
        float* velFloats = (float*)dBodyGetLinearVel(parent->m_bodyID);
        nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
        return;
    }
    float* velFloats = (float*)dBodyGetLinearVel(m_bodyID);
    nlVec3Set(*vel, velFloats[0], velFloats[1], velFloats[2]);
}

/**
 * Offset/Address/Size: 0xA00 | 0x802006FC | size: 0x30
 */
void PhysicsObject::SetLinearVelocity(const nlVector3& velocity)
{
    dBodySetLinearVel(m_bodyID, velocity.x, velocity.y, velocity.z);
}

/**
 * Offset/Address/Size: 0xA30 | 0x8020072C | size: 0x1E0
 */
void PhysicsObject::GetRotation(nlMatrix4* m_out) const
{
    const float* dRot;
    if ((m_geomID == 0) && (m_bodyID != 0))
    {
        dRot = dBodyGetRotation(m_bodyID);
    }
    else
    {
        dRot = dGeomGetRotation(m_geomID);
    }

    PhysicsObject* parentObj = m_parentObject;

    m_out->e2[0][0] = dRot[0];
    m_out->e2[1][0] = dRot[1];
    m_out->e2[2][0] = dRot[2];
    m_out->e2[0][1] = dRot[4];
    m_out->e2[1][1] = dRot[5];
    m_out->e2[2][1] = dRot[6];
    m_out->e2[0][2] = dRot[8];
    m_out->e2[1][2] = dRot[9];
    m_out->e2[2][2] = dRot[10];
    m_out->e2[3][0] = 0.f;
    m_out->e2[3][1] = 0.f;
    m_out->e2[3][2] = 0.f;
    m_out->e2[3][3] = 1.f;
    m_out->e2[0][3] = 0.f;
    m_out->e2[1][3] = 0.f;
    m_out->e2[2][3] = 0.f;

    if (parentObj == NULL)
        return;

    if ((parentObj->m_geomID == 0) && (parentObj->m_bodyID != 0))
    {
        dRot = dBodyGetRotation(parentObj->m_bodyID);
    }
    else
    {
        dRot = dGeomGetRotation(parentObj->m_geomID);
    }

    nlMatrix4 parentRot;
    parentRot.SetColumn(0, *(nlVector3*)&dRot[0]);
    parentRot.SetColumn(1, *(nlVector3*)&dRot[4]);
    parentRot.SetColumn(2, *(nlVector3*)&dRot[8]);
    parentRot.e2[3][0] = 0.f;
    parentRot.e2[3][1] = 0.f;
    parentRot.e2[3][2] = 0.f;
    parentRot.e2[3][3] = 1.f;
    parentRot.e2[0][3] = 0.f;
    parentRot.e2[1][3] = 0.f;
    parentRot.e2[2][3] = 0.f;

    PhysicsObject* parent_parentObj = parentObj->m_parentObject;
    if (parent_parentObj != NULL)
    {
        if ((parent_parentObj->m_geomID == 0) && (parent_parentObj->m_bodyID != 0))
        {
            dRot = dBodyGetRotation(parent_parentObj->m_bodyID);
        }
        else
        {
            dRot = dGeomGetRotation(parent_parentObj->m_geomID);
        }
        nlMatrix4 rot_tmp1;
        ConvertDMat3ToNLMat4(dRot, &rot_tmp1);

        PhysicsObject* parent_parent_parentObj = parent_parentObj->m_parentObject;
        if (parent_parent_parentObj != NULL)
        {
            nlMatrix4 rot_tmp2;
            parent_parent_parentObj->GetRotation(&rot_tmp2);
            nlMultMatrices(rot_tmp1, rot_tmp1, rot_tmp2);
        }

        nlMultMatrices(parentRot, parentRot, rot_tmp1);
    }

    nlMultMatrices(*m_out, *m_out, parentRot);
}

/**
 * Offset/Address/Size: 0xC10 | 0x8020090C | size: 0xAC
 */
void PhysicsObject::SetRotation(const nlMatrix4& m4_in)
{
    dMatrix3 dRot;
    ConvertNLMat4ToDMat3_Transposed(m4_in, dRot);

    if ((m_geomID == NULL) && (m_bodyID != NULL))
    {
        dBodySetRotation(m_bodyID, dRot);
        return;
    }
    // PORT: skip when there is no geom either.
    if (m_geomID != NULL)
    {
        dGeomSetRotation(m_geomID, dRot);
    }
}

/**
 * Offset/Address/Size: 0xCBC | 0x802009B8 | size: 0x94
 */
void PhysicsObject::SetRotation(const nlMatrix3& m3_in)
{
    float dRot[12];
    ConvertNLMat3ToDMat3(m3_in, dRot);

    if ((m_geomID == NULL) && (m_bodyID != NULL))
    {
        dBodySetRotation(m_bodyID, dRot);
        return;
    }
    // PORT: skip when there is no geom either.
    if (m_geomID != NULL)
    {
        dGeomSetRotation(m_geomID, dRot);
    }
}

/**
 * Offset/Address/Size: 0xD50 | 0x80200A4C | size: 0x34
 */
nlVector3& PhysicsObject::GetPosition()
{
    GetPosition(&m_position);
    return m_position;
}

/**
 * Offset/Address/Size: 0xD84 | 0x80200A80 | size: 0x32C
 */
void PhysicsObject::GetPosition(nlVector3* position) const
{
    nlMatrix4 parentRot;
    nlMatrix4 rot_1;
    nlMatrix4 rot_2;
    nlVector3 parentPos;
    nlVector3 pos_1;
    nlVector3 pos_2;

    PhysicsObject* parentObj;

    const float* _retFloats;

    // PORT: neither id set reached dGeomGetPosition(NULL).
    if (m_geomID != NULL)
    {
        _retFloats = dGeomGetPosition(m_geomID);
    }
    else if (m_bodyID != NULL)
    {
        _retFloats = dBodyGetPosition(m_bodyID);
    }
    else
    {
        _retFloats = port_no_geom_pos;
    }

    nlVec3Set(*position, _retFloats[0], _retFloats[1], _retFloats[2]);

    parentObj = m_parentObject;
    if (parentObj != NULL)
    {
        // PORT: neither id set reached dGeomGetPosition(NULL).
        if (parentObj->m_geomID != NULL)
        {
            _retFloats = dGeomGetPosition(parentObj->m_geomID);
        }
        else if (parentObj->m_bodyID != NULL)
        {
            _retFloats = dBodyGetPosition(parentObj->m_bodyID);
        }
        else
        {
            _retFloats = port_no_geom_pos;
        }

        nlVec3Set(parentPos, _retFloats[0], _retFloats[1], _retFloats[2]);

        PhysicsObject* parent_parentObj = parentObj->m_parentObject;
        if (parent_parentObj != NULL)
        {
            // PORT: neither id set reached dGeomGetPosition(NULL).
            if (parent_parentObj->m_geomID != NULL)
            {
                _retFloats = dGeomGetPosition(parent_parentObj->m_geomID);
            }
            else if (parent_parentObj->m_bodyID != NULL)
            {
                _retFloats = dBodyGetPosition(parent_parentObj->m_bodyID);
            }
            else
            {
                _retFloats = port_no_geom_pos;
            }
            pos_1.Set(_retFloats[0], _retFloats[1], _retFloats[2]);
            if (parent_parentObj->m_parentObject != NULL)
            {
                nlMatrix4 rot_tmp2;
                parent_parentObj->m_parentObject->GetPosition(&pos_2);
                parent_parentObj->m_parentObject->GetRotation(&rot_tmp2);
                nlMultPosVectorMatrix(pos_1, pos_1, rot_tmp2);
                nlVecAdd(pos_1, pos_2, pos_1);
            }

            parent_parentObj = parentObj->m_parentObject;

            // PORT: neither id set reached dGeomGetRotation(NULL).
            if (parent_parentObj->m_geomID != NULL)
            {
                _retFloats = dGeomGetRotation(parent_parentObj->m_geomID);
            }
            else if (parent_parentObj->m_bodyID != NULL)
            {
                _retFloats = dBodyGetRotation(parent_parentObj->m_bodyID);
            }
            else
            {
                _retFloats = port_no_geom_rot;
            }
            ConvertDMat3ToNLMat4(_retFloats, &rot_1);

            if (parent_parentObj->m_parentObject != NULL)
            {
                nlMatrix4 rot_tmp2;
                parent_parentObj->m_parentObject->GetRotation(&rot_tmp2);
                nlMultMatrices(rot_1, rot_1, rot_tmp2);
            }
            nlMultPosVectorMatrix(parentPos, parentPos, rot_1);
            parentPos.Set(pos_1.x + parentPos.x, pos_1.y + parentPos.y, pos_1.z + parentPos.z);
        }

        parentObj = m_parentObject;
        // PORT: neither id set reached dGeomGetRotation(NULL).
        if (parentObj->m_geomID != NULL)
        {
            _retFloats = dGeomGetRotation(parentObj->m_geomID);
        }
        else if (parentObj->m_bodyID != NULL)
        {
            _retFloats = dBodyGetRotation(parentObj->m_bodyID);
        }
        else
        {
            _retFloats = port_no_geom_rot;
        }

        parentRot.SetColumn(0, *(nlVector3*)&_retFloats[0]);
        parentRot.SetColumn(1, *(nlVector3*)&_retFloats[4]);
        parentRot.SetColumn(2, *(nlVector3*)&_retFloats[8]);
        parentRot.e2[3][0] = 0.f;
        parentRot.e2[3][1] = 0.f;
        parentRot.e2[3][2] = 0.f;
        parentRot.e2[3][3] = 1.f;
        parentRot.e2[0][3] = 0.f;
        parentRot.e2[1][3] = 0.f;
        parentRot.e2[2][3] = 0.f;

        PhysicsObject* grandParent = parentObj->m_parentObject;
        if (grandParent != NULL)
        {
            // PORT: neither id set reached dGeomGetRotation(NULL).
            if (grandParent->m_geomID != NULL)
            {
                _retFloats = dGeomGetRotation(grandParent->m_geomID);
            }
            else if (grandParent->m_bodyID != NULL)
            {
                _retFloats = dBodyGetRotation(grandParent->m_bodyID);
            }
            else
            {
                _retFloats = port_no_geom_rot;
            }
            ConvertDMat3ToNLMat4(_retFloats, &rot_2);

            if (grandParent->m_parentObject != NULL)
            {
                nlMatrix4 rot_tmp2;
                grandParent->m_parentObject->GetRotation(&rot_tmp2);
                nlMultMatrices(rot_2, rot_2, rot_tmp2);
            }
            nlMultMatrices(parentRot, parentRot, rot_2);
        }

        nlMultPosVectorMatrix(*position, *position, parentRot);
        nlVec3Add(*position, parentPos, *position);
    }
}

/**
 * Offset/Address/Size: 0x10B0 | 0x80200DAC | size: 0x118
 */
void PhysicsObject::SetPosition(const nlVector3& pos, PhysicsObject::CoordinateType type)
{
    nlMatrix4 rot;     // sp54
    nlMatrix4 inv_rot; // sp14
    nlVector3 _pos;    // sp8...sp10

    if ((m_parentObject != NULL) && (type == 0))
    {
        m_parentObject->GetRotation(&rot);
        nlVector3& p = m_parentObject->GetPosition();

        rot.e2[3][0] = p.x;
        rot.e2[3][1] = p.y;
        rot.e2[3][2] = p.z;
        rot.e2[3][3] = 1.0f;

        nlInvertRotTransMatrix(inv_rot, rot);
        nlMultPosVectorMatrix(_pos, pos, inv_rot);
        if ((m_geomID == NULL) && (m_bodyID != NULL))
        {
            dBodySetPosition(m_bodyID, _pos.x, _pos.y, _pos.z);
            return;
        }
        // PORT: skip when there is no geom either.
        if (m_geomID != NULL)
        {
            dGeomSetPosition(m_geomID, _pos.x, _pos.y, _pos.z);
        }
        return;
    }

    if ((m_geomID == NULL) && (m_bodyID != NULL))
    {
        dBodySetPosition(m_bodyID, pos.x, pos.y, pos.z);
        return;
    }
    // PORT: skip when there is no geom either.
    if (m_geomID != NULL)
    {
        dGeomSetPosition(m_geomID, pos.x, pos.y, pos.z);
    }
}

/**
 * Offset/Address/Size: 0x11C8 | 0x80200EC4 | size: 0x20
 */
void PhysicsObject::PostUpdate()
{
    CheckForNaN();
}

/**
 * Offset/Address/Size: 0x11E8 | 0x80200EE4 | size: 0x78
 */
void PhysicsObject::PreUpdate()
{
    dMass m;
    if ((m_bodyID != 0) && (m_parentObject == 0))
    {
        dMassSetZero(&m);
        dBodyGetMass(m_bodyID, &m);
        dBodyAddForce(m_bodyID, 0.f, 0.f, m_gravity * m.mass);
    }
    CheckForNaN();
}

/**
 * Offset/Address/Size: 0x1260 | 0x80200F5C | size: 0x48
 */
void PhysicsObject::CheckForNaN()
{
    dBodyGetForce(m_bodyID);
    dBodyGetTorque(m_bodyID);
    GetPosition();
    GetLinearVelocity();
}

/**
 * Offset/Address/Size: 0x12A8 | 0x80200FA4 | size: 0x24
 */
void PhysicsObject::SetCategory(unsigned int categoryBits)
{
    dGeomSetCategoryBits(m_geomID, categoryBits);
}

/**
 * Offset/Address/Size: 0x12CC | 0x80200FC8 | size: 0x24
 */
void PhysicsObject::SetCollide(unsigned int collideBits)
{
    dGeomSetCollideBits(m_geomID, collideBits);
}

/**
 * Offset/Address/Size: 0x12F0 | 0x80200FEC | size: 0x40
 */
void PhysicsObject::SetDefaultCollideBits()
{
    dGeomSetCollideBits(m_geomID, 2);
    dGeomSetCategoryBits(m_geomID, 2);
}

/**
 * Offset/Address/Size: 0x1330 | 0x8020102C | size: 0x88
 */
PhysicsObject::~PhysicsObject()
{
    if (m_bodyID != nullptr)
    {
        dBodyDestroy(m_bodyID);
        m_bodyID = nullptr;
    }

    if (m_geomID != nullptr)
    {
        dGeomDestroy(m_geomID);
        m_geomID = nullptr;
    }
}

/**
 * Offset/Address/Size: 0x13B8 | 0x802010B4 | size: 0x7C
 */
PhysicsObject::PhysicsObject(PhysicsWorld* world)
{
    m_bodyID = NULL;
    m_geomID = NULL;
    m_parentObject = NULL;
    m_gravity = DefaultGravity;

    if (world != NULL)
    {
        m_bodyID = dBodyCreate(world->m_World);
        dBodySetData(m_bodyID, this);
        dBodySetGravityMode(m_bodyID, 0);
    }
}

#include "NL/vmath.h"
