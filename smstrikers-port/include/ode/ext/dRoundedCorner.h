#ifndef _DROUNDEDCORNER_H_
#define _DROUNDEDCORNER_H_

#include "ode/collision.h"

dGeomID dCreateRoundedCorner(dxSpace* space, float param, bool flag1, bool flag2);
int dCollideRoundedCornerColumn(dxGeom* cor_GeomID, dxGeom* col_GeomID, int flags, dContactGeom* contact, int skip);
int dCollideRoundedCornerSphere(dxGeom* cornerGeomID, dxGeom* sphereGeomID, int flags, dContactGeom* contact, int skip);
void dGeomRoundedCornerSetParams(dxGeom* geomID, float param, bool flag1, bool flag2);
void dGeomRoundedCornerGetParams(dxGeom* geomID, float* param, bool* flag1, bool* flag2);

#endif // _DROUNDEDCORNER_H_
