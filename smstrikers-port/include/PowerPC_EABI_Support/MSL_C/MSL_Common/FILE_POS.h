#ifndef _MSL_COMMON_FILE_POS_H
#define _MSL_COMMON_FILE_POS_H

#include "ansi_files.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int fseek(FILE* stream, unsigned long offset, int whence);
    int _fseek(FILE* file, fpos_t offset, int whence);
    long ftell(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif /* _MSL_COMMON_FILE_POS_H */
