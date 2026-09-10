#ifndef _DCOLUMN_H_
#define _DCOLUMN_H_

#include "ode/collision.h"

dGeomID dCreateColumn(dxSpace* space, float radius);
int dCollideColumnPlane(dxGeom* o1, dxGeom* o2, int flags, dContactGeom* contact, int skip);
int dCollideColumnColumn(dxGeom* o1, dxGeom* o2, int flags, dContactGeom* contact, int skip);
void dGeomColumnGetParams(dxGeom* geomID, float* radius);
void dGeomColumnSetParams(dxGeom* geomID, float radius);

#endif // _DCOLUMN_H_
