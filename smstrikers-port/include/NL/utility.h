#ifndef _UTILITY_H_
#define _UTILITY_H_

#include "NL/nlMath.h"

void SolveQuadratic(float a, float b, float c, int& numRoots, float& x1, float& x2);
float SweepSpheres(float rp, const nlVector3& p1, const nlVector3& p2, float rq, const nlVector3& q1, const nlVector3& q2);

#endif // _UTILITY_H_
