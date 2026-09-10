#ifndef _NLFILEGC_H_
#define _NLFILEGC_H_

#include "NL/nlFile.h"
#include "NL/nlFunction.h"
#include "NL/nlArrayAllocator.h"
#include "dolphin/dvd.h"
#include "FILE_POS.h"

enum GCFileSystem
{
    eGC_UNKNOWN = 0,
    eGC_TDEV = 1,
    eGC_DVDOPEN = 2,
};

class GCFile;

void nlReadAsyncToVirtualMemory(nlFile* file, void* buffer, int size, ReadAsyncCallback callback, uintptr_t param, unsigned long chunkSize, void* userData);
void nlAsyncLoadFileToVirtualMemory(nlFile* file, int size, void* buffer, ReadAsyncCallback callback, uintptr_t alignment);
void nlCancelPendingAsyncReads(nlFile* pFile, void (*callback)(nlFile*, void*, unsigned int, uintptr_t, void (*)(nlFile*, void*, unsigned int, uintptr_t)));
bool nlAsyncReadsPending(nlFile* file);
void* nlLoadEntireFileToVirtualMemory(const char* fileName, int* size, unsigned int transferSize, void* target, eAllocType allocType);
void* nlReadToVirtualMemory(nlFile* file, void* buffer, unsigned int size, unsigned int chunkSize);
u32 nlGetFilePosition(nlFile* file);
void nlSeek(nlFile* file, unsigned int offset, unsigned long origin);
void nlReadAsync(nlFile* file, void* buffer, unsigned int size, ReadAsyncCallback callback, uintptr_t uParam);
void nlServiceFileSystem();
void nlInitFileSystem();
unsigned char GameCubeReadBlocking(GCFile* pFile, void* pBuffer, unsigned long uSize);
static unsigned char GameCubeReadAsync(GCFile* pFile, ReadAsyncCallback pFunc, void* pBuffer, unsigned long uSize, uintptr_t uParam);
void nlFlushFileCash();
nlFile* nlOpen(const char* fileName);
void nlRegHandleDVDMessageCB(const Function<void(int)>& cb);
void nlRegHandleDVDAllClearCB(const Function<void(int)>& cb);
void nlRegCheckForResetFromFSCB(const Function<FnVoidVoid>& cb);

class GCFile : public nlFile
{
public:
    GCFile()
    {
        PendingAsync.m_Count = 0;
        m_Position = 0;
    }
    virtual ~GCFile();
    virtual u32 FileSize(unsigned int*) = 0;
    virtual void Read(void* buffer, unsigned int size);
    virtual s32 GetReadStatus() = 0;
    virtual void ReadAsync(void*, unsigned long, unsigned long) = 0;
    virtual u32 GetDiscPosition() = 0;

    /* 0x04 */ Counter PendingAsync;
    /* 0x08 */ unsigned long m_Position;
};

struct CURRENT_READ
{
    /* 0x0 */ unsigned char* Buffer;
    /* 0x4 */ unsigned long Pos;
    /* 0x8 */ unsigned long Length;
    /* 0xC */ unsigned long AmountRead;
}; // total size: 0x10

class TDEVChunkFile : public GCFile
{
public:
    TDEVChunkFile(_FILE* fp)
        : m_pFile(fp)
    {
    }
    virtual ~TDEVChunkFile()
    {
        fclose(m_pFile);
        m_pFile = NULL;
    }

    static unsigned int FileSize(_FILE* pFile)
    {
        unsigned long uPos = ftell(pFile);
        fseek(pFile, 0, 2);
        unsigned long uSize = ftell(pFile);
        fseek(pFile, uPos, 0);
        return uSize;
    }

    virtual u32 FileSize(unsigned int* pSize)
    {
        unsigned long Size = TDEVChunkFile::FileSize(m_pFile);
        if (pSize != NULL)
        {
            *pSize = Size;
        }
        return Size;
    }

    virtual s32 GetReadStatus();
    virtual void ReadAsync(void* buffer, unsigned long length, unsigned long offset);

    virtual u32 GetDiscPosition()
    {
        return 0;
    }

    void* operator new(size_t)
    {
        return s_pAllocator->Allocate();
    }

    void operator delete(void* ptr)
    {
        s_pAllocator->DeleteEntry((TDEVChunkFile*)ptr);
    }

    static GCFile* Open(const char* FileName)
    {
        _FILE* pFile = fopen(FileName, "rb");

        if (pFile == NULL)
        {
            return NULL;
        }

        if (FileSize(pFile) == 0xFFFFFFFF)
        {
            return NULL;
        }

        GCFile* pGCFile = new TDEVChunkFile(pFile);
        while (pFile == NULL)
        {
        }

        return pGCFile;
    }

    static nlArrayAllocator<TDEVChunkFile>* s_pAllocator;

    /* 0x0C */ _FILE* m_pFile;
    /* 0x10 */ CURRENT_READ m_CurrentRead;
};

inline GCFile::~GCFile()
{
}

class DolphinFile : public GCFile
{
public:
    DolphinFile(s32 entryNum) { DVDFastOpen(entryNum, &m_fileInfo); }
    virtual ~DolphinFile()
    {
        DVDClose(&m_fileInfo);
    }

    virtual u32 FileSize(unsigned int* size)
    {
        u32 s = m_fileInfo.length;
        if (size != NULL)
        {
            *size = s;
        }
        return s;
    }

    virtual s32 GetReadStatus()
    {
        return DVDGetCommandBlockStatus(&m_fileInfo.cb);
    }

    virtual void ReadAsync(void* addr, unsigned long length, unsigned long offset)
    {
        DVDReadAsyncPrio(&m_fileInfo, addr, (s32)length, (s32)offset, 0, 2);
    }

    virtual u32 GetDiscPosition()
    {
        return m_fileInfo.startAddr;
    }

    void* operator new(size_t)
    {
        return s_pAllocator->Allocate();
    }

    void operator delete(void* ptr)
    {
        s_pAllocator->DeleteEntry((DolphinFile*)ptr);
    }

    static GCFile* Open(const char* FileName)
    {
        long FileEntrynum = DVDConvertPathToEntrynum(FileName);

        if (FileEntrynum == -1)
        {
            return NULL;
        }

        GCFile* pFile = new DolphinFile(FileEntrynum);
        while (pFile == NULL)
        {
        }

        return pFile;
    }

    static nlArrayAllocator<DolphinFile>* s_pAllocator;

    /* 0x0c */ DVDFileInfo m_fileInfo;
};

#endif // _NLFILEGC_H_
