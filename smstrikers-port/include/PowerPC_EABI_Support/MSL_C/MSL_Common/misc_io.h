#ifndef _MSL_COMMON_MISC_IO_H
#define _MSL_COMMON_MISC_IO_H

#include "PowerPC_EABI_Support/MSL_C/MSL_Common/ansi_files.h"

#ifdef __cplusplus
extern "C" {
#endif

void clearerr(FILE * file);
void __stdio_atexit(void);

#ifdef __cplusplus
}
#endif

#endif /* _MSL_COMMON_MISC_IO_H */
