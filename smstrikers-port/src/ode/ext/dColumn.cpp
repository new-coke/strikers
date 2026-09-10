#include "ode/ext/dColumn.h"
#include "math.h"

static u8 lengthwiseAxis = 0x2;
s32 dColumnClassUser = -1;

void dGeomColumnSetParams(dxGeom* geomID, float radius)
{
    *(float*)dGeomGetClassData(geomID) = radius;
}

/**
 * Offset/Address/Size: 0x5A4 | 0x8021D3D0 | size: 0x34
 */
void dGeomColumnGetParams(dxGeom* geomID, float* radius)
{
    *radius = *(float*)dGeomGetClassData(geomID);
}

/**
 * Offset/Address/Size: 0x514 | 0x8021D340 | size: 0x90
 */
static void dColumnAABB(dxGeom* geomID, float* aabb)
{
    f32 radius;
    float* position;

    radius = *(float*)dGeomGetClassData(geomID);
    position = (float*)dGeomGetPosition(geomID);

    aabb[0] = position[0] - radius;
    aabb[1] = position[0] + radius;
    aabb[2] = position[1] - radius;
    aabb[3] = position[1] + radius;
    aabb[4] = -radius;
    aabb[5] = radius;
}

/**
 * Offset/Address/Size: 0x28C | 0x8021D0B8 | size: 0x288
 */
int dCollideColumnColumn(dxGeom* o1, dxGeom* o2, int flags, dContactGeom* contact, int skip)
{
    f32 radius1;
    f32 radius2;
    const float* pos1;
    const float* pos2;
    u8 ax1;
    u8 ax2;
    f32 distSquared;
    f32 x;

    radius1 = *(f32*)dGeomGetClassData(o1);
    radius2 = *(f32*)dGeomGetClassData(o2);
    pos1 = (const float*)dGeomGetPosition(o1);
    pos2 = (const float*)dGeomGetPosition(o2);
    ax1 = 0xFF;
    ax2 = 0xFF;
    if (lengthwiseAxis == 0)
    {
        ax1 = 1;
        ax2 = 2;
    }
    if (lengthwiseAxis == 1)
    {
        ax1 = 0;
        ax2 = 2;
    }
    if (lengthwiseAxis == 2)
    {
        ax1 = 0;
        ax2 = 1;
    }

    distSquared = (pos1[ax1] - pos2[ax1]) * (pos1[ax1] - pos2[ax1]) + (pos1[ax2] - pos2[ax2]) * (pos1[ax2] - pos2[ax2]);

    if (distSquared < ((radius1 + radius2) * (radius1 + radius2)))
    {
        if (distSquared > 0.0f)
        {
            f64 rsqrt0 = __frsqrte(distSquared);
            f64 rsqrt1 = 0.5 * rsqrt0 * -(((f64)distSquared * (rsqrt0 * rsqrt0)) - 3.0);
            f64 rsqrt2 = 0.5 * rsqrt1 * -(((f64)distSquared * (rsqrt1 * rsqrt1)) - 3.0);
            x = (f32)((f64)distSquared * (0.5 * rsqrt2 * -(((f64)distSquared * (rsqrt2 * rsqrt2)) - 3.0)));
        }
        else if (distSquared < 0.0)
        {
            x = *(f32*)&__float_nan;
        }
        else
        {
            f32 classify_val = distSquared;
            s32 exp_bits = *(s32*)&classify_val & 0x7F800000;
            s32 fp_class;
            switch (exp_bits)
            {
            case 0x7F800000:
                if (*(s32*)&classify_val & 0x7FFFFF)
                {
                    fp_class = 1;
                }
                else
                {
                    fp_class = 2;
                }
                break;
            case 0x0:
                if (*(s32*)&classify_val & 0x7FFFFF)
                {
                    fp_class = 5;
                }
                else
                {
                    fp_class = 3;
                }
                break;
            default:
                fp_class = 4;
                break;
            }
            if (fp_class == 1)
            {
                x = *(f32*)&__float_nan;
            }
            else
            {
                x = distSquared;
            }
        }

        contact->normal[ax1] = (pos1[ax1] - pos2[ax1]) / x;
        {
            f32 halfOverlap = ((x + radius2) - radius1) / 2.0f;
            contact->normal[ax2] = (pos1[ax2] - pos2[ax2]) / x;

            contact->normal[lengthwiseAxis] = 0.0f;
            contact->pos[ax1] = (contact->normal[ax1] * halfOverlap) + pos2[ax1];
            contact->pos[ax2] = (contact->normal[ax2] * halfOverlap) + pos2[ax2];
            contact->pos[lengthwiseAxis] = 0.0f;
        }
        contact->depth = radius2 - (x - radius2);
        contact->g1 = o1;
        contact->g2 = o2;
        return 1;
    }

    return 0;
}

/**
 * Offset/Address/Size: 0xE0 | 0x8021CF0C | size: 0x1AC
 */
int dCollideColumnPlane(dxGeom* o1, dxGeom* o2, int flags, dContactGeom* contact, int skip)
{
    dVector4 n;
    dReal radius;
    dReal d;
    const dReal* pos;
    u8 ax1;
    u8 ax2;
    u8 perpendicular;
    dReal distance;

    radius = *(dReal*)dGeomGetClassData(o1);
    dGeomPlaneGetParams(o2, n);
    d = n[3];
    pos = (const dReal*)dGeomGetPosition(o1);
    ax1 = 0xFF;
    ax2 = 0xFF;
    perpendicular = 0;
    if (lengthwiseAxis == 0)
    {
        ax1 = 1;
        ax2 = 2;
        if (n[0] < 0.001f)
        {
            perpendicular = 1;
        }
    }
    if (lengthwiseAxis == 1)
    {
        ax1 = 0;
        ax2 = 2;
        if (n[1] < 0.001f)
        {
            perpendicular = 1;
        }
    }
    if (lengthwiseAxis == 2)
    {
        ax1 = 0;
        ax2 = 1;
        if (n[2] < 0.001f)
        {
            perpendicular = 1;
        }
    }
    if (!perpendicular)
    {
        return 0;
    }
    distance = n[ax1] * pos[ax1] + n[ax2] * pos[ax2] - d;
    if (distance < radius)
    {
        contact->normal[0] = n[0];
        contact->normal[1] = n[1];
        contact->normal[2] = n[2];
        contact->pos[ax1] = pos[ax1] - contact->normal[ax1] * radius;
        contact->pos[ax2] = pos[ax2] - contact->normal[ax2] * radius;
        contact->pos[lengthwiseAxis] = 0.0f;
        contact->depth = distance - radius;
        contact->g1 = o1;
        contact->g2 = o2;
        return 1;
    }
    return 0;
}

/**
 * Offset/Address/Size: 0xB0 | 0x8021CEDC | size: 0x30
 */
static dColliderFn* dColumnColliderFn(int num)
{
    if (num == (s32)dColumnClassUser)
    {
        return &dCollideColumnColumn;
    }
    if (num == 4)
    {
        return &dCollideColumnPlane;
    }
    return NULL;
}

/**
 * Offset/Address/Size: 0x0 | 0x8021CE2C | size: 0xB0
 */
dGeomID dCreateColumn(dxSpace* space, float radius)
{
    dGeomClass geomClass;
    dGeomID geomID;

    if ((s32)dColumnClassUser == -1)
    {
        geomClass.bytes = 4;
        geomClass.collider = &dColumnColliderFn;
        geomClass.aabb = &dColumnAABB;
        geomClass.aabb_test = 0;
        geomClass.dtor = 0;
        dColumnClassUser = dCreateGeomClass(&geomClass);
    }
    geomID = dCreateGeom(dColumnClassUser);
    if (space != NULL)
    {
        dSpaceAdd(space, geomID);
    }
    *(float*)dGeomGetClassData(geomID) = radius;
    return geomID;
}
