#ifndef _NLDEBUGFILE_H_
#define _NLDEBUGFILE_H_

void nlCloseFileDebug(void* fp);
void nlFlushFileDebug(void* fp);
void nlWriteLineDebug(void* file, const char* buffer, bool flush);
void* nlOpenFileDebug(const char* fileName, bool bBinary, bool bAppend);

#endif // _NLDEBUGFILE_H_
