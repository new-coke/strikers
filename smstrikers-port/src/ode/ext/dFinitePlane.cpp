#include "ode/ext/dFinitePlane.h"
#include "ode/NLGAdditions.h"

s32 dFinitePlaneClassUser = -1;

struct FinitePlane
{
    float a;
    float b;
    float c;
    float d;
    bool flag;
    float param;
};

void dGeomFinitePlaneSetParams(dxGeom* geomID, float a, float b, float c, float d, bool flag, float param)
{
    FinitePlane* planeData = (FinitePlane*)dGeomGetClassData(geomID);

    planeData->a = a;
    planeData->b = b;
    planeData->c = c;
    planeData->d = d;
    planeData->flag = flag;
    planeData->param = param;
}

void dGeomFinitePlaneGetParams(dxGeom* geomID, float* a, float* b, float* c, float* d, bool* flag, float* param)
{
    FinitePlane* planeData = (FinitePlane*)dGeomGetClassData(geomID);

    *a = planeData->a;
    *b = planeData->b;
    *c = planeData->c;
    *d = planeData->d;
    *flag = planeData->flag;
    *param = planeData->param;
}

/**
 * Offset/Address/Size: 0x314 | 0x8021D718 | size: 0x3B0
 */
void dFinitePlaneAABB(dxGeom* geomID, float* aabb)
{
    dVector3 p[4];
    dVector3 x;

    f32 plane_a;
    f32 plane_b;
    f32 plane_c;
    f32 plane_d;
    u32 plane_flag;
    f32 plane_param;
    float* rotation;
    float* position;
    FinitePlane* planeData;
    int i;

    planeData = (FinitePlane*)dGeomGetClassData(geomID);
    plane_a = planeData->a;
    plane_b = planeData->b;
    plane_c = planeData->c;
    plane_d = planeData->d;
    plane_flag = planeData->flag;
    plane_param = planeData->param;
    rotation = (float*)dGeomGetRotation(geomID);
    position = (float*)dGeomGetPosition(geomID);

    dVector3Set(x, plane_a, plane_c, 0.0f);
    dMultiplyMatrix3Vector3(p[0], rotation, x, 0);
    dVector3Add(p[0], position);

    dVector3Set(x, plane_a, plane_d, 0.0f);
    dMultiplyMatrix3Vector3(p[1], rotation, x, 0);
    dVector3Add(p[1], position);

    dVector3Set(x, plane_b, plane_d, 0.0f);
    dMultiplyMatrix3Vector3(p[2], rotation, x, 0);
    dVector3Add(p[2], position);

    dVector3Set(x, plane_b, plane_c, 0.0f);
    dMultiplyMatrix3Vector3(p[3], rotation, x, 0);
    dVector3Add(p[3], position);

    aabb[0] = p[0][0];
    aabb[1] = p[0][0];
    aabb[2] = p[0][1];
    aabb[3] = p[0][1];
    aabb[4] = p[0][2];
    aabb[5] = p[0][2];

    for (i = 1; i < 4; i++)
    {
        if (p[i][0] < aabb[0])
            aabb[0] = p[i][0];
        if (p[i][0] > aabb[1])
            aabb[1] = p[i][0];
        if (p[i][1] < aabb[2])
            aabb[2] = p[i][1];
        if (p[i][1] > aabb[3])
            aabb[3] = p[i][1];
        if (p[i][2] < aabb[4])
            aabb[4] = p[i][2];
        if (p[i][2] > aabb[5])
            aabb[5] = p[i][2];
    }

    if (plane_flag != 0)
    {
        f32 negParam = -plane_param;
        dVector3Set(x, plane_a, plane_c, negParam);
        dMultiplyMatrix3Vector3(p[0], rotation, x, 0);
        dVector3Add(p[0], position);

        dVector3Set(x, plane_a, plane_d, negParam);
        dMultiplyMatrix3Vector3(p[1], rotation, x, 0);
        dVector3Add(p[1], position);

        dVector3Set(x, plane_b, plane_d, negParam);
        dMultiplyMatrix3Vector3(p[2], rotation, x, 0);
        dVector3Add(p[2], position);

        dVector3Set(x, plane_b, plane_c, negParam);
        dMultiplyMatrix3Vector3(p[3], rotation, x, 0);
        dVector3Add(p[3], position);

        for (i = 1; i < 4; i++)
        {
            if (p[i][0] < aabb[0])
                aabb[0] = p[i][0];
            if (p[i][0] > aabb[1])
                aabb[1] = p[i][0];
            if (p[i][1] < aabb[2])
                aabb[2] = p[i][1];
            if (p[i][1] > aabb[3])
                aabb[3] = p[i][1];
            if (p[i][2] < aabb[4])
                aabb[4] = p[i][2];
            if (p[i][2] > aabb[5])
                aabb[5] = p[i][2];
        }
    }
}

/**
 * Offset/Address/Size: 0x100 | 0x8021D504 | size: 0x214
 */
int dCollideFinitePlaneSphere(dxGeom* planeGeomID, dxGeom* sphereGeomID, int flags, dContactGeom* contact, int skip)
{
    dMatrix4 TInv;
    dVector3 spherePos4;
    dVector3 planeLocalPosition;
    f32 plane_a;
    f32 plane_b;
    f32 plane_c;
    f32 plane_d;
    f32 radius;
    f32 plane_param;
    f32 dist;
    u32 plane_flag;
    float* plane_rot;
    float* plane_pos;
    float* sphere_pos;
    FinitePlane* planeData;

    planeData = (FinitePlane*)dGeomGetClassData(planeGeomID);
    plane_a = planeData->a;
    plane_b = planeData->b;
    plane_c = planeData->c;
    plane_d = planeData->d;
    plane_flag = planeData->flag;
    plane_param = planeData->param;
    radius = dGeomSphereGetRadius(sphereGeomID);
    plane_pos = (float*)dGeomGetPosition(planeGeomID);
    sphere_pos = (float*)dGeomGetPosition(sphereGeomID);
    plane_rot = (float*)dGeomGetRotation(planeGeomID);

    dInvertRigidTransformation(&TInv[0], plane_rot, plane_pos);
    dVector4Set(spherePos4, sphere_pos[0], sphere_pos[1], sphere_pos[2], 1.0f);
    dMultiplyMatrix4Vector4(&planeLocalPosition[0], &TInv[0], spherePos4);

    if (plane_flag != 0)
    {
        dist = planeLocalPosition[2];
    }
    else
    {
        dist = (f32)fabs(planeLocalPosition[2]);
    }

    if ((dist < radius) && (planeLocalPosition[0] > plane_a) && (planeLocalPosition[0] < plane_b) && (planeLocalPosition[1] > plane_c) && (planeLocalPosition[1] < plane_d))
    {
        dExtractColumn3(contact->normal, plane_rot, 2);
        if ((planeLocalPosition[2] > 0.0f) || (plane_flag != 0))
        {
            dVectorScale(contact->normal, -1.0f);
        }

        contact->pos[0] = (f32)((contact->normal[0] * radius) + sphere_pos[0]);
        contact->pos[1] = (f32)((contact->normal[1] * radius) + sphere_pos[1]);
        contact->pos[2] = (f32)((contact->normal[2] * radius) + sphere_pos[2]);
        contact->depth = (f32)(radius - dist);

        if ((plane_param != -1.0f) && (contact->depth > plane_param))
        {
            return 0;
        }

        contact->g1 = planeGeomID;
        contact->g2 = sphereGeomID;
        return 1;
    }

    return 0;
}

/**
 * Offset/Address/Size: 0xE8 | 0x8021D4EC | size: 0x18
 */
static dColliderFn* dFinitePlaneColliderFn(int num)
{
    if (num == 0)
    {
        return &dCollideFinitePlaneSphere;
    }
    return NULL;
}

/**
 * Offset/Address/Size: 0x0 | 0x8021D404 | size: 0xE8
 */
dGeomID dCreateFinitePlane(dSpaceID space, dReal a, dReal b, dReal c, dReal d, bool flag, float param)
{
    FinitePlane* planeData;
    dGeomClass geomClass;
    dGeomID geomID;

    if ((s32)dFinitePlaneClassUser == -1)
    {
        geomClass.bytes = 0x18;
        geomClass.collider = &dFinitePlaneColliderFn;
        geomClass.aabb = &dFinitePlaneAABB;
        geomClass.aabb_test = 0;
        geomClass.dtor = 0;
        dFinitePlaneClassUser = dCreateGeomClass(&geomClass);
    }

    geomID = dCreateGeom(dFinitePlaneClassUser);
    if (space != 0U)
    {
        dSpaceAdd(space, geomID);
    }

    planeData = (FinitePlane*)dGeomGetClassData(geomID);
    planeData->a = a;
    planeData->b = b;
    planeData->c = c;
    planeData->d = d;
    planeData->flag = flag;
    planeData->param = param;

    return geomID;
}
