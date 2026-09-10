#ifndef _GLCONSTANT_H_
#define _GLCONSTANT_H_

#include "NL/nlMath.h"

nlVector4 glConstantGet(const char* constantName);
bool glConstantGet(const char* constantName, nlVector4& result);
void glConstantSet(const char* constantName, const nlVector4& value);
void gl_ConstantMarkerBackup(int arg);
void gl_ConstantMarkerAdvance();
void gl_ConstantStartup();

#endif // _GLCONSTANT_H_
