#include "ode/NLGAdditions.h"

#include "NL/nlMath.h"
#include "ode/matrix.h"
#include "ode/odemath.h"
#include "objects.h"
#include "collision_kernel.h"
#include "collision_space_internal.h"

/**
 * Offset/Address/Size: 0x5C0 | 0x80224888 | size: 0x38
 */
void dBodySetUpdateMode(dxBody* b, int updateLinear, int updateAngular)
{
    b->flags &= ~0x60;
    if (updateLinear == 0)
    {
        b->flags |= 0x20;
    }
    if (updateAngular == 0)
    {
        b->flags |= 0x40;
    }
}

/**
 * Offset/Address/Size: 0x5B8 | 0x80224880 | size: 0x8
 */
dxBody* dWorldGetFirstBody(dxWorld* w)
{
    return w->firstbody;
}

/**
 * Offset/Address/Size: 0x5B0 | 0x80224878 | size: 0x8
 */
dxBody* dBodyGetNextBody(dxBody* b)
{
    return (dxBody*)b->next;
}

/**
 * Offset/Address/Size: 0x5A8 | 0x80224870 | size: 0x8
 */
void dWorldSetClearAccumulators(dxWorld* w, int doClear)
{
    w->clear_accumulators = doClear;
}

/**
 * Offset/Address/Size: 0x5A4 | 0x8022486C | size: 0x4
 */
void dClearCachedData()
{
}

/**
 * Offset/Address/Size: 0x590 | 0x80224858 | size: 0x14
 */
static void characterInit(dxJointCharacter* j)
{
    j->direction[0] = 0.f;
    j->direction[1] = 0.f;
    j->direction[2] = 0.f;
}

/**
 * Offset/Address/Size: 0x57C | 0x80224844 | size: 0x14
 */
static void characterGetInfo1(dxJointCharacter* j, dxJoint::Info1* info)
{
    info->m = 4;
    info->nub = 0;
}

/**
 * Offset/Address/Size: 0x41C | 0x802246E4 | size: 0x160
 */
static void characterGetInfo2(dxJointCharacter* j, dxJoint::Info2* info)
{
    info->J1a[0] = 1;
    info->c[0] = 0;
    info->lo[0] = -dInfinity;
    info->hi[0] = dInfinity;

    {
        int row = info->rowskip + 1;
        info->J1a[row] = 1;
    }
    info->c[1] = 0;
    info->lo[1] = -dInfinity;
    info->hi[1] = dInfinity;

    {
        int row = info->rowskip * 2 + 2;
        info->J1a[row] = 1;
    }
    info->c[2] = 0;
    info->lo[2] = -dInfinity;
    info->hi[2] = dInfinity;

    dReal* R = ((dxJoint*)j)->node[0].body->R;
    unsigned int jStart = info->rowskip * 3;

    dReal v0 = dDOT(R, j->direction);
    dReal v1 = dDOT(R + 4, j->direction);
    dReal v2 = dDOT(R + 8, j->direction);

    info->J1l[jStart] = v0;
    {
        unsigned int idx = jStart + 1;
        info->J1l[idx] = v1;
    }
    {
        unsigned int idx = jStart + 2;
        info->J1l[idx] = v2;
    }

    info->c[3] = 0;
    info->lo[3] = -dInfinity;
    info->hi[3] = dInfinity;
}

dxJoint::Vtable __dcharacter_vtable = {
    sizeof(dxJointCharacter),
    (dxJoint::init_fn*)characterInit,
    (dxJoint::getInfo1_fn*)characterGetInfo1,
    (dxJoint::getInfo2_fn*)characterGetInfo2,
    dJointTypeCharacter
};

/**
 * Offset/Address/Size: 0x3F4 | 0x802246BC | size: 0x28
 */
dxJoint* dJointCreateCharacter(dxWorld* w, dxJointGroup* group)
{
    return createJoint(w, group, &__dcharacter_vtable);
}

/**
 * Offset/Address/Size: 0x3D8 | 0x802246A0 | size: 0x1C
 */
void dJointSetCharacterNoMotionDirection(dxJoint* id, float* dir)
{
    dxJointCharacter* joint = (dxJointCharacter*)id;
    joint->direction[0] = dir[0];
    joint->direction[1] = dir[1];
    joint->direction[2] = dir[2];
}

static void dGeomGetPosition(dxGeom* g, dReal* out)
{
    out[0] = g->pos[0];
    out[1] = g->pos[1];
    out[2] = g->pos[2];
    out[3] = g->pos[3];
}

static void dGeomGetRotation(dxGeom* g, dReal* out)
{
    int i;

    for (i = 0; i < 12; i++)
    {
        out[i] = g->R[i];
    }
}

static bool dGeomIsPlaceable(dxGeom* g)
{
    return (g->gflags & GEOM_PLACEABLE) != 0;
}

/**
 * Offset/Address/Size: 0x3D0 | 0x80224698 | size: 0x8
 */
int dGeomGetGFlags(dxGeom* g)
{
    return g->gflags;
}

/**
 * Offset/Address/Size: 0x3C8 | 0x80224690 | size: 0x8
 */
void dGeomSetGFlags(dxGeom* g, int gflags)
{
    g->gflags = gflags;
}

static int dBodyGetFlags(dxBody* b)
{
    return b->flags;
}

static void dBodySetFlags(dxBody* b, int flags)
{
    b->flags = flags;
}

static void dBodyGetRotation(dxBody* b, dReal* out)
{
    int i;

    for (i = 0; i < 12; i++)
    {
        out[i] = b->R[i];
    }
}

static int dBodyGetAutoDisableStepsRemain(dxBody* b)
{
    return b->adis_stepsleft;
}

static void dBodySetAutoDisableStepsRemain(dxBody* b, int steps_remain)
{
    b->adis_stepsleft = steps_remain;
}

static dReal dBodyGetAutoDisableTimeRemain(dxBody* b)
{
    return b->adis_timeleft;
}

static void dBodySetAutoDisableTimeRemain(dxBody* b, dReal time_remain)
{
    b->adis_timeleft = time_remain;
}

static void dMassGetParameters(dMass* m, dReal& themass, dReal& cgx, dReal& cgy, dReal& cgz,
    dReal& I11, dReal& I22, dReal& I33, dReal& I12, dReal& I13,
    dReal& I23)
{
    themass = m->mass;
    cgx = m->c[0];
    cgy = m->c[1];
    cgz = m->c[2];
    I11 = m->I[0];
    I22 = m->I[5];
    I33 = m->I[10];
    I12 = m->I[1];
    I13 = m->I[2];
    I23 = m->I[6];
}

static void dWorldGetParameters(dxWorld* w, dReal* gravity, dReal& global_erp, dReal& global_cfm,
    dReal& adis_linear_threshold, dReal& adis_angular_threshold,
    dReal& adis_idle_time, int& adis_idle_steps, int& adis_flag,
    int& qs_num_iterations, dReal& qs_w, dReal& contact_max_vel,
    dReal& contact_min_depth, int& clear_accumulators)
{
    gravity[0] = w->gravity[0];
    gravity[1] = w->gravity[1];
    gravity[2] = w->gravity[2];
    global_erp = w->global_erp;
    global_cfm = w->global_cfm;
    adis_linear_threshold = w->adis.linear_threshold;
    adis_angular_threshold = w->adis.angular_threshold;
    adis_idle_time = w->adis.idle_time;
    adis_idle_steps = w->adis.idle_steps;
    adis_flag = w->adis_flag;
    qs_num_iterations = w->qs.num_iterations;
    qs_w = w->qs.w;
    contact_max_vel = w->contactp.max_vel;
    contact_min_depth = w->contactp.min_depth;
    clear_accumulators = w->clear_accumulators;
}

static void dWorldSetParameters(dxWorld* w, dReal* gravity, dReal global_erp, dReal global_cfm,
    dReal adis_linear_threshold, dReal adis_angular_threshold,
    dReal adis_idle_time, int adis_idle_steps, int adis_flag,
    int qs_num_iterations, dReal qs_w, dReal contact_max_vel,
    dReal contact_min_depth, int clear_accumulators)
{
    w->gravity[0] = gravity[0];
    w->gravity[1] = gravity[1];
    w->gravity[2] = gravity[2];
    w->global_erp = global_erp;
    w->global_cfm = global_cfm;
    w->adis.linear_threshold = adis_linear_threshold;
    w->adis.angular_threshold = adis_angular_threshold;
    w->adis.idle_time = adis_idle_time;
    w->adis.idle_steps = adis_idle_steps;
    w->adis_flag = adis_flag;
    w->qs.num_iterations = qs_num_iterations;
    w->qs.w = qs_w;
    w->contactp.max_vel = contact_max_vel;
    w->contactp.min_depth = contact_min_depth;
    w->clear_accumulators = clear_accumulators;
}

/**
 * Offset/Address/Size: 0x340 | 0x80224608 | size: 0x88
 */
void dMultiplyMatrix3Vector3(float* result, const float* R, const float* v, bool transposeMatrix)
{
    if (!transposeMatrix)
    {
        dMultiply0(result, R, v, 3, 3, 1);
    }
    if (transposeMatrix)
    {
        dMultiply1(result, R, v, 3, 3, 1);
    }
}

/**
 * Offset/Address/Size: 0x314 | 0x802245DC | size: 0x2C
 */
void dMultiplyMatrix4Vector4(float* result, const float* T, const float* v)
{
    dMultiply0(result, T, v, 4, 4, 1);
}

static void dInvertRigidTransformation(dReal* TInv, const dReal* T)
{
    dVector3 p;

    dExtractColumn3(p, T, 3);
    dInvertRigidTransformation(TInv, T, p);
}

/**
 * Offset/Address/Size: 0x238 | 0x80224500 | size: 0xDC
 */
void dInvertRigidTransformation(dReal* TInv, const dReal* R, const dReal* p)
{
    dVector3 temp;

    TInv[0] = R[0];
    TInv[1] = R[4];
    TInv[2] = R[8];
    TInv[4] = R[1];
    TInv[5] = R[5];
    TInv[6] = R[9];
    TInv[8] = R[2];
    TInv[9] = R[6];
    TInv[10] = R[10];

    dMultiply0(temp, TInv, p, 3, 3, 1);
    dVectorScale(temp, REAL(-1.0));

    TInv[3] = temp[0];
    TInv[7] = temp[1];
    TInv[11] = temp[2];
    TInv[12] = REAL(0.0);
    TInv[13] = REAL(0.0);
    TInv[14] = REAL(0.0);
    TInv[15] = REAL(1.0);
}

/**
 * Offset/Address/Size: 0x214 | 0x802244DC | size: 0x24
 */
void dExtractColumn3(float* __restrict v, const float* __restrict M, int columnIndex)
{
    v[0] = M[columnIndex];
    v[1] = M[columnIndex + 4];
    v[2] = M[columnIndex + 8];
}

static void dExtractColumn4(dReal* v, const dReal* M, int columnIndex)
{
    v[0] = M[columnIndex];
    v[1] = M[columnIndex + 4];
    v[2] = M[columnIndex + 8];
    v[3] = M[columnIndex + 12];
}

static void dSetColumn3(dReal* M, const dReal* v, int columnIndex)
{
    M[columnIndex] = v[0];
    M[columnIndex + 4] = v[1];
    M[columnIndex + 8] = v[2];
}

static void dSetColumn4(dReal* M, const dReal* v, int columnIndex)
{
    M[columnIndex] = v[0];
    M[columnIndex + 4] = v[1];
    M[columnIndex + 8] = v[2];
    M[columnIndex + 12] = v[3];
}

static void dConstructMatrix3(dReal* R, const dReal* v1, const dReal* v2, const dReal* v3)
{
    dSetColumn3(R, v1, 0);
    dSetColumn3(R, v2, 1);
    dSetColumn3(R, v3, 2);
}

static void dConstructMatrix4(dReal* T, const dReal* v1, const dReal* v2, const dReal* v3,
    const dReal* p)
{
    dSetColumn3(T, v1, 0);
    dSetColumn3(T, v2, 1);
    dSetColumn3(T, v3, 2);
    dSetColumn3(T, p, 3);
    T[12] = REAL(0.0);
    T[13] = REAL(0.0);
    T[14] = REAL(0.0);
    T[15] = REAL(1.0);
}

/**
 * Offset/Address/Size: 0x204 | 0x802244CC | size: 0x10
 */
void dVector3Set(float* v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

/**
 * Offset/Address/Size: 0x1F0 | 0x802244B8 | size: 0x14
 */
void dVector4Set(float* v, float x, float y, float z, float w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

/**
 * Offset/Address/Size: 0x1C8 | 0x80224490 | size: 0x28
 */
void dVectorScale(float* v, float scale)
{
    v[0] *= scale;
    v[1] *= scale;
    v[2] *= scale;
}

/**
 * Offset/Address/Size: 0x194 | 0x8022445C | size: 0x34
 */
void dVector3Add(float* __restrict v, const float* __restrict x)
{
    v[0] += x[0];
    v[1] += x[1];
    v[2] += x[2];
}

static void dGeomSetPosition(dxGeom* g, const dReal* p)
{
    dReal* pos = g->pos;

    pos[0] = p[0];
    pos[1] = p[1];
    pos[2] = p[2];
    g->gflags |= GEOM_DIRTY | GEOM_AABB_BAD;
}

/**
 * Offset/Address/Size: 0x150 | 0x80224418 | size: 0x44
 */
void dGeomComputeAABB(dxGeom* g)
{
    g->computeAABB();
    g->gflags &= ~(GEOM_DIRTY | GEOM_AABB_BAD);
}

/**
 * Offset/Address/Size: 0x140 | 0x80224408 | size: 0x10
 */
void dGeomMarkAABBAsValid(dxGeom* g)
{
    g->gflags &= ~(GEOM_DIRTY | GEOM_AABB_BAD);
}

/**
 * Offset/Address/Size: 0x0 | 0x802242C8 | size: 0x140
 */
void dGeomCollideAABBs(dxGeom* g1, dxGeom* g2, void* userData, dNearCallback* callback)
{
    collideAABBs(g1, g2, userData, callback);
}
