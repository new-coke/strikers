#ifndef _MSL_COMMON_MBSTRING_H
#define _MSL_COMMON_MBSTRING_H

#include "wchar_io.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t wcstombs(char* s, const wchar_t* pwcs, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* _MSL_COMMON_MBSTRING_H */