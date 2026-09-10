#ifndef _VMATH_H_
#define _VMATH_H_

#include "NL/nlMath.h"

inline void nlVector3::Set(float _x, float _y, float _z)
{
    x = _x;
    y = _y;
    z = _z;
}

inline void nlMatrix4::SetColumn(int col, const nlVector3& v)
{
    e2[0][col] = v.x;
    e2[1][col] = v.y;
    e2[2][col] = v.z;
}

inline nlVector3& nlMatrix4::GetTranslation() const
{
    return *(nlVector3*)&m41;
}

inline void nlMatrix4::SetTranslation(const nlVector3& trans)
{
    m41 = trans.x;
    m42 = trans.y;
    m43 = trans.z;
    m44 = 1.0f;
}

inline void nlVecAdd(nlVector3& out, const nlVector3& a, const nlVector3& b)
{
    nlVec3Set(out, a.x + b.x, a.y + b.y, a.z + b.z);
}

#endif // _VMATH_H_
