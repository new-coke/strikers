#ifndef _NLFILE_H_
#define _NLFILE_H_

#include "types.h"
#include "file_io.h"

class nlFile;

typedef void (*ReadAsyncCallback)(nlFile*, void*, unsigned int, uintptr_t);
typedef void (*LoadAsyncCallback)(void*, unsigned long, void*);

enum eAllocType
{
    AllocateStart = 0,
    AllocateEnd = 1,
};

class Counter
{
public:
    long m_Count; // offset 0x0, size 0x4
}; // total size: 0x4

class nlFile
{
public:
    nlFile();
    virtual ~nlFile();

    /* 0x0C */ virtual u32 FileSize(unsigned int* size) = 0;
    /* 0x10 */ virtual void Read(void* buffer, unsigned int size) = 0;
}; // total size: 0xC

bool nlLoadEntireFileAsync(const char* filename, LoadAsyncCallback callback, void* user_data, unsigned int alignment, eAllocType type);
void nlLoadEntireFileAsyncCallback(nlFile* file, void* pBuffer, unsigned int uSize, uintptr_t uParam);
void* nlLoadEntireFile(const char* filename, unsigned long* outSize, unsigned int alignment, eAllocType type);
void nlClose(nlFile* file);
void nlRead(nlFile* file, void* buffer, unsigned int size);
unsigned int nlFileSize(nlFile* file, unsigned int* size);

#endif // _NLFILE_H_
