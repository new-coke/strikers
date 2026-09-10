#ifndef PORT_STD_CMATH_H_
#define PORT_STD_CMATH_H_
// Port shim for MSL's <cmath.h>.
// The original supplies float overloads (fabs/sqrt/fmod/ceil/floor/abs) that
// the host C++ library already provides; we defer to the host and only pull
// them into the global namespace so unqualified calls keep resolving.
#include <math.h>
#ifdef __cplusplus
#include <cmath>
using std::fabs;
using std::sqrt;
using std::fmod;
using std::ceil;
using std::floor;
extern "C" inline float fabsf_(float x) { return ::fabsf(x); }
#endif
#endif // PORT_STD_CMATH_H_
