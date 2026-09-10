#ifndef _NLPRINT_H_
#define _NLPRINT_H_

#include "printf.h"

int nlVSNPrintf(char* buffer, unsigned long size, const char* format, __va_list args);
int nlSNPrintf(char* buffer, unsigned long size, const char* format, ...);

#endif // _NLPRINT_H_
