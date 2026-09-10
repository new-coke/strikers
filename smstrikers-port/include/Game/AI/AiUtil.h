#ifndef _AIUTIL_H_
#define _AIUTIL_H_

#include "NL/nlMath.h"

nlVector3 GetClosestPointOnSidelines(const nlVector3& v3Position);
char* GetPowerupName(int powerup);
void SortToMinOrMaxTotalSum(unsigned int* result, float (*data)[4], bool findMin);
nlVector3 GetClosestPointOnLineABFromPointC(const nlVector3& a, const nlVector3& b, const nlVector3& c);
float AIsgn(float fValue);
float NormalizeVal(float fromVal, float fromMin, float fromMax);
float NormalizeVal(float fromVal, const nlVector2& fromExtrema);
float InterpolateRangeClamped(const nlVector2& outputRange, const nlVector2& inputRange, float value);
float InterpolateRangeClamped(float fResultMin, float fResultMax, float fInputMin, float fInputMax, float fInput);
float Interpolate(float fMin, float fMax, float fPercent);
void RotateVector(nlVector3& result, const nlVector3& v, nlQuaternion& q);
void GetRotationBetweenVectors(nlQuaternion& quat, const nlVector3& v3Vec1, const nlVector3& v3Vec2);
void RotateVectorZAxis(nlVector3& v3Out, const nlVector3& v3In, unsigned short angle);
void GetWorldPoint(nlVector3& v3WorldPointOut, const nlVector3& v3LocalPointIn, const nlVector3& v3RefPosition, unsigned short aRefAngle);
void GetLocalPoint(nlVector3& v3LocalPointOut, const nlVector3& v3WorldPointIn, const nlVector3& v3RefPosition, unsigned short aRefAngle);
float GetClosingSpeed(const nlVector3& pos1, const nlVector3& vel1, const nlVector3& pos2, const nlVector3& vel2);
float GetClosingSpeed2D(const nlVector3& pos1, const nlVector3& vel1, const nlVector3& pos2, const nlVector3& vel2);
float Exp(float k);
bool TestCollision(float rp, const nlVector3& p1, const nlVector3& p2, float rq, const nlVector3& q1, const nlVector3& q2);
bool ClipPositionToSidelines(nlVector3& position, float margin);
void CalcInterceptXY(const nlVector3& pos1, float speed1, float speed2, const nlVector3& pos2, const nlVector3& vel, int& count, float* times);
float SeekSpeed(float fCurrent, float fDesired, float fSeekAccel, float fSeekDecel, float fDeltaT);
unsigned short SeekDirection(unsigned short aCurrent, unsigned short aDesired, float fSeekSpeed, float fFalloff, float fDeltaT);
bool IsPointInCone(const nlVector3& v3Point, const nlVector3& v3Pivot, const nlVector3& v3Plane1, const nlVector3& v3Plane2);
void MakePerpendicularPlane(const nlVector3& v3Position, const nlVector3& v3Normal, nlVector4& v4Plane, float fPlaneOffset);
void MakePerpendicularPlane(const nlVector3& v3Position, unsigned short aNormalAngle, nlVector4& v4Plane, float fPlaneOffset);

#endif // _AIUTIL_H_
