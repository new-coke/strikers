#include "Game/Physics/PhysicsFinitePlane.h"
#include "ode/ext/dFinitePlane.h"
#include "NL/nlMath.h"

/**
 * Offset/Address/Size: 0x0 | 0x801FFAE4 | size: 0x218
 */
PhysicsFinitePlane::PhysicsFinitePlane(CollisionSpace* collision_space, nlVector3& centre, nlVector3& v1, nlVector3& v2, bool isOneSided, float errorCorrectionDepth)
    : PhysicsObject(NULL)
{
    mErrorCorrectionDepth = errorCorrectionDepth;

    xMin = 0.f;
    xMax = 0.f;
    yMin = 0.f;
    yMax = 0.f;

    xMax = nlSqrt(v1.x * v1.x + v1.y * v1.y + v1.z * v1.z, true);
    yMax = nlSqrt(v2.x * v2.x + v2.y * v2.y + v2.z * v2.z, true);

    xMin = -xMax;
    yMin = -yMax;

    const float l = 1.f / xMax;
    nlVec3Set(v1, l * v1.x, l * v1.y, l * v1.z);

    const float l2 = 1.f / yMax;
    nlVec3Set(v2, l2 * v2.x, l2 * v2.y, l2 * v2.z);

    nlMatrix3 R;
    nlVector3 normal;
    nlVec3CrossProductAlt(normal, v1, v2);
    R.e[0] = v1.x;
    R.e[1] = v1.y;
    R.e[2] = v1.z;
    R.e[3] = v2.x;
    R.e[4] = v2.y;
    R.e[5] = v2.z;
    R.e[6] = normal.z;
    R.e[7] = normal.y;
    R.e[8] = normal.x;

    dSpaceID space = NULL;
    if (collision_space != NULL)
    {
        space = collision_space->m_spaceID;
    }

    m_geomID = dCreateFinitePlane(space, xMin, xMax, yMin, yMax, isOneSided, errorCorrectionDepth);
    dGeomSetData(m_geomID, this);
    SetRotation(R);
    SetPosition(centre, WORLD_COORDINATES);
    SetDefaultCollideBits();
}
