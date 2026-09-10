#include "ode/ext/dRoundedCorner.h"
#include "ode/ext/dColumn.h"
#include "math.h"

s32 dRoundedCornerClassUser = -1;
extern s32 dColumnClassUser;

struct RoundedCornerData
{
    float param;
    bool flag1;
    bool flag2;
};

struct AABBDimensions
{
    float minX;
    float maxX;
    float minY;
    float maxY;
    float minZ;
    float maxZ;
};

void dGeomRoundedCornerSetParams(dxGeom* geomID, float param, bool flag1, bool flag2)
{
    RoundedCornerData* data = (RoundedCornerData*)dGeomGetClassData(geomID);

    data->param = param;
    data->flag1 = flag1;
    data->flag2 = flag2;
}

void dGeomRoundedCornerGetParams(dxGeom* geomID, float* param, bool* flag1, bool* flag2)
{
    RoundedCornerData* data = (RoundedCornerData*)dGeomGetClassData(geomID);

    *param = data->param;
    *flag1 = data->flag1;
    *flag2 = data->flag2;
}

/**
 * Offset/Address/Size: 0x520 | 0x8021DFE8 | size: 0xD4
 */
static void dRoundedCornerAABB(dxGeom* geomID, float* aabb)
{
    float radius;
    float minX;
    float minY;
    float maxX;
    float maxY;
    u32 extendPosX;
    u32 extendPosY;
    RoundedCornerData* data;
    float* position;

    data = (RoundedCornerData*)dGeomGetClassData(geomID);
    radius = data->param;
    extendPosX = data->flag1;
    extendPosY = data->flag2;
    position = (float*)dGeomGetPosition(geomID);
    minX = position[0];
    minY = position[1];
    maxX = minX;
    maxY = minY;
    if (extendPosX != 0)
    {
        maxX = minX + radius;
    }
    if (extendPosX == 0)
    {
        minX -= radius;
    }
    if (extendPosY != 0)
    {
        maxY += radius;
    }
    if (extendPosY == 0)
    {
        minY -= radius;
    }

    aabb[0] = minX;
    aabb[1] = maxX;
    aabb[2] = minY;
    aabb[3] = maxY;
    aabb[4] = -*(float*)&__float_max;
    aabb[5] = *(float*)&__float_max;
}

/**
 * Offset/Address/Size: 0x30C | 0x8021DDD4 | size: 0x214
 */
int dCollideRoundedCornerSphere(dxGeom* cornerGeomID, dxGeom* sphereGeomID, int flags, dContactGeom* contact, int skip)
{
    f32 classify_val;
    f32 s_radius;
    f32 c_param;
    f32 normal_x;
    f32 inner_radius;
    f32 delta_y;
    f32 normal_y;
    f32 delta_x;
    f32 dist;
    f64 rsqrt0;
    f64 rsqrt1;
    f64 rsqrt2;
    s32 exp_bits;
    s32 result;
    s32 fp_class;
    u32 c_flag1;
    u32 c_flag2;
    float* c_pos;
    RoundedCornerData* cornerData;
    float* s_pos;

    cornerData = (RoundedCornerData*)dGeomGetClassData(cornerGeomID);
    c_param = cornerData->param;
    c_flag1 = cornerData->flag1;
    c_flag2 = cornerData->flag2;
    s_radius = dGeomSphereGetRadius(sphereGeomID);
    c_pos = (float*)dGeomGetPosition(cornerGeomID);
    s_pos = (float*)dGeomGetPosition(sphereGeomID);
    delta_x = s_pos[0] - c_pos[0];
    delta_y = s_pos[1] - c_pos[1];

    {
        u32 x_check = (u32)(delta_x > 0.0f);
        u32 y_check = (u32)(delta_y > 0.0f);
        if (x_check != c_flag1 || y_check != c_flag2)
        {
            result = 0;
        }
        else
        {
            dist = (delta_x * delta_x) + (delta_y * delta_y);
            inner_radius = c_param - s_radius;
            if (dist >= (inner_radius * inner_radius))
            {
                if (dist > 0.0f)
                {
                    rsqrt0 = __frsqrte(dist);
                    rsqrt1 = 0.5 * rsqrt0 * -(((f64)dist * (rsqrt0 * rsqrt0)) - 3.0);
                    rsqrt2 = 0.5 * rsqrt1 * -(((f64)dist * (rsqrt1 * rsqrt1)) - 3.0);
                    dist = (f32)((f64)dist * (0.5 * rsqrt2 * -(((f64)dist * (rsqrt2 * rsqrt2)) - 3.0)));
                }
                else if (dist < 0.0)
                {
                    dist = *(float*)&__float_nan;
                }
                else
                {
                    classify_val = dist;
                    exp_bits = *(s32*)&classify_val & 0x7F800000;
                    switch (exp_bits)
                    { /* irregular */
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
                        dist = *(float*)&__float_nan;
                    }
                }

                normal_x = delta_x / dist;
                result = 1;
                normal_y = delta_y / dist;

                contact->pos[0] = (f32)((c_param * normal_x) + c_pos[0]);
                contact->pos[1] = (f32)((c_param * normal_y) + c_pos[1]);
                contact->pos[2] = s_pos[2];
                contact->normal[0] = normal_x;
                contact->normal[1] = normal_y;
                contact->normal[2] = 0.0f;
                contact->depth = (f32)(dist - inner_radius);
                contact->g1 = cornerGeomID;
                contact->g2 = sphereGeomID;
            }
            else
            {
                result = 0;
            }
        }
    }
    return result;
}

/**
 * Offset/Address/Size: 0x104 | 0x8021DBCC | size: 0x208
 */
int dCollideRoundedCornerColumn(dxGeom* cor_GeomID, dxGeom* col_GeomID, int flags, dContactGeom* contact, int skip)
{
    f32 col_radius;
    f32 classify_val;
    f32 cor_param;
    f32 normal_x;
    f32 inner_radius;
    f32 delta_y;
    f32 delta_x;
    f32 dist;
    f64 rsqrt0;
    f64 rsqrt1;
    f64 rsqrt2;
    s32 exp_bits;
    s32 result;
    s32 fp_class;
    u32 cor_flag1;
    u32 cor_flag2;
    float* cor_pos;
    RoundedCornerData* cornerData;
    float* col_pos;

    cornerData = (RoundedCornerData*)dGeomGetClassData(cor_GeomID);
    cor_param = cornerData->param;
    cor_flag1 = cornerData->flag1;
    cor_flag2 = cornerData->flag2;
    dGeomColumnGetParams(col_GeomID, &col_radius);
    cor_pos = (float*)dGeomGetPosition(cor_GeomID);
    col_pos = (float*)dGeomGetPosition(col_GeomID);
    delta_x = col_pos[0] - cor_pos[0];
    delta_y = col_pos[1] - cor_pos[1];
    inner_radius = col_radius;

    {
        u32 x_check = (u32)(delta_x > 0.0f);
        u32 y_check = (u32)(delta_y > 0.0f);
        if (x_check != cor_flag1 || y_check != cor_flag2)
        {
            result = 0;
        }
        else
        {
            dist = (delta_x * delta_x) + (delta_y * delta_y);
            inner_radius = cor_param - inner_radius;
            if (dist >= (inner_radius * inner_radius))
            {
                if (dist > 0.0f)
                {
                    rsqrt0 = __frsqrte(dist);
                    rsqrt1 = 0.5 * rsqrt0 * -(((f64)dist * (rsqrt0 * rsqrt0)) - 3.0);
                    rsqrt2 = 0.5 * rsqrt1 * -(((f64)dist * (rsqrt1 * rsqrt1)) - 3.0);
                    dist = (f32)((f64)dist * (0.5 * rsqrt2 * -(((f64)dist * (rsqrt2 * rsqrt2)) - 3.0)));
                }
                else if (dist < 0.0)
                {
                    dist = *(float*)&__float_nan;
                }
                else
                {
                    classify_val = dist;
                    exp_bits = *(s32*)&classify_val & 0x7F800000;
                    switch (exp_bits)
                    { /* irregular */
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
                        dist = *(float*)&__float_nan;
                    }
                }
                normal_x = delta_x / dist;
                contact->pos[0] = cor_pos[0] + cor_param * normal_x;
                delta_y = delta_y / dist;
                contact->pos[1] = cor_pos[1] + cor_param * delta_y;
                contact->pos[2] = col_pos[2];
                contact->normal[0] = normal_x;
                contact->normal[1] = delta_y;
                contact->normal[2] = 0.0f;
                contact->depth = dist - inner_radius;
                contact->g1 = cor_GeomID;
                contact->g2 = col_GeomID;
                result = 1;
            }
            else
            {
                result = 0;
            }
        }
    }
    return result;
}

/**
 * Offset/Address/Size: 0xD0 | 0x8021DB98 | size: 0x34
 */
static dColliderFn* dRoundedCornerColliderFn(int num)
{
    if (num == 0)
    {
        return &dCollideRoundedCornerSphere;
    }

    if (num == dColumnClassUser)
    {
        return &dCollideRoundedCornerColumn;
    }

    return NULL;
}

/**
 * Offset/Address/Size: 0x0 | 0x8021DAC8 | size: 0xD0
 */
dGeomID dCreateRoundedCorner(dxSpace* space, float param, bool flag1, bool flag2)
{
    RoundedCornerData* data;
    dGeomClass geomClass;
    dGeomID geomID;

    if ((s32)dRoundedCornerClassUser == -1)
    {
        geomClass.bytes = 8;
        geomClass.collider = &dRoundedCornerColliderFn;
        geomClass.aabb = &dRoundedCornerAABB;
        geomClass.aabb_test = 0;
        geomClass.dtor = 0;
        dRoundedCornerClassUser = dCreateGeomClass(&geomClass);
    }

    geomID = dCreateGeom(dRoundedCornerClassUser);
    if (space != 0U)
    {
        dSpaceAdd(space, geomID);
    }

    data = (RoundedCornerData*)dGeomGetClassData(geomID);
    data->param = param;
    data->flag1 = flag1;
    data->flag2 = flag2;

    return geomID;
}
