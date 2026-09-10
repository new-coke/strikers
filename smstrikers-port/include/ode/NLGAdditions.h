#ifndef _NLGADDITIONS_H_
#define _NLGADDITIONS_H_

#include "ode/common.h"
#include "ode/collision.h"
#include "ode/joint.h"

struct dxJointCharacter : public dxJoint
{
    float direction[4]; // offset 0x50, size 0x10
}; // total size: 0x60

void dGeomCollideAABBs(dxGeom* g1, dxGeom* g2, void* userData, dNearCallback* callback);
void dGeomMarkAABBAsValid(dxGeom* g);
void dGeomComputeAABB(dxGeom* g);
void dVector3Add(float* __restrict v, const float* __restrict x);
void dVectorScale(float* v, float scale);
void dVector4Set(float* v, float x, float y, float z, float w);
void dVector3Set(float* v, float x, float y, float z);
void dExtractColumn3(float* __restrict v, const float* __restrict M, int columnIndex);
void dInvertRigidTransformation(float* TInv, const float* R, const float* p);
void dMultiplyMatrix4Vector4(float* result, const float* T, const float* v);
void dMultiplyMatrix3Vector3(float* result, const float* R, const float* v, bool transposeMatrix);
void dGeomSetGFlags(dxGeom* g, int gflags);
int dGeomGetGFlags(dxGeom* g);
void dJointSetCharacterNoMotionDirection(dxJoint* id, float* dir);
dxJoint* dJointCreateCharacter(dxWorld* w, dxJointGroup* group);
void dClearCachedData();
void dWorldSetClearAccumulators(dxWorld* w, int doClear);
dxBody* dBodyGetNextBody(dxBody* b);
dxBody* dWorldGetFirstBody(dxWorld* w);
void dBodySetUpdateMode(dxBody* b, int updateLinear, int updateAngular);

#endif // _NLGADDITIONS_H_
