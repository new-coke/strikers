#ifndef PORT_MSL_FILE_IO_H
#define PORT_MSL_FILE_IO_H
// Port shim for MSL's <file_io.h>: host stdio plus the one MSL-only helper.
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
int __msl_strnicmp(const char* str1, const char* str2, int n);
#ifdef __cplusplus
}
#endif
#endif // PORT_MSL_FILE_IO_H
