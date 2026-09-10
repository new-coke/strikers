#ifndef _DFINITEPLANE_H_
#define _DFINITEPLANE_H_

#include "ode/collision.h"

dGeomID dCreateFinitePlane(dSpaceID space, dReal a, dReal b, dReal c, dReal d, bool flag, float param);
int dCollideFinitePlaneSphere(dxGeom* planeGeomID, dxGeom* sphereGeomID, int flags, dContactGeom* contact, int skip);
void dFinitePlaneAABB(dxGeom* geomID, float* aabb);
void dGeomFinitePlaneSetParams(dxGeom* geomID, float a, float b, float c, float d, bool flag, float param);
void dGeomFinitePlaneGetParams(dxGeom* geomID, float* a, float* b, float* c, float* d, bool* flag, float* param);

#endif // _DFINITEPLANE_H_
