#ifndef _NLBUNDLEFILE_H_
#define _NLBUNDLEFILE_H_

#include "NL/nlWare.h"
#include "NL/nlFileGC.h"
#include "NL/nlString.h"

typedef void (*FileReadAsyncCallback)(void*, unsigned long, uintptr_t);

typedef struct
{
    /* 0x00 */ u32 nSectorSize;
    /* 0x04 */ u32 nNumFiles;
    /* 0x08 */ u32 nDirectoryOffsetInSectors;
    /* 0x0C */ u32 nDataOffsetInSectors;
} BundleFileHeader;

typedef struct
{
    /* 0x00 */ u32 m_hash;
    /* 0x04 */ u32 m_blockNumber;
    /* 0x08 */ u32 m_length;
} BundleFileDirectoryEntry, *BundleFileDirectoryEntryPtr;

static void cbFileReadAsyncCallback(nlFile* file, void* buffer, unsigned int arg, uintptr_t bundlePtr);

class BundleFile
{
public:
    void ReadFileAsync(unsigned long hash, void* buffer, unsigned long size, FileReadAsyncCallback callback, uintptr_t userParam);
    void ReadFileAsync(const char* filename, void* buffer, unsigned long size, FileReadAsyncCallback callback, uintptr_t userParam);
    void LoadFile(const char* filename, void* pBuffer);
    void ReadFileByIndex(unsigned long index, void* buffer, unsigned long size);
    void ReadFile(unsigned long hash, void* buffer, unsigned long size);
    void ReadFile(const char* filename, void* pBuffer, unsigned long size);
    bool GetFileInfoByIndex(unsigned long index, BundleFileDirectoryEntry* entry);
    bool GetFileInfo(unsigned long hash, BundleFileDirectoryEntry* entry, bool printError);
    bool GetFileInfo(const char* filename, BundleFileDirectoryEntry* entry, bool printError);
    void Close();
    bool Open(const char* filename);

    ~BundleFile();
    BundleFile();

    static inline u32 HashFilename(const char* filename)
    {
        char fixedName[256];
        unsigned long index = 0;
        for (; index < nlStrLen<char>(filename); index++)
        {
            fixedName[index] = nlToLower<char>(*(char*)&filename[index]);
            if (*(char*)&filename[index] == 0x5C)
            {
                fixedName[index] = '/';
            }
        }
        fixedName[index] = 0;
        return nlStringHash(fixedName);
    }

    inline void LoadFileByIndex(unsigned long nFileIndex, void* pBuffer)
    {
        BundleFileDirectoryEntry* pEntry = &m_pDirectory[nFileIndex];
        nlSeek(m_pFile, pEntry->m_blockNumber * m_pHeader->nSectorSize, 0);
        nlRead(m_pFile, pBuffer, pEntry->m_length);
    }

    inline void LoadFile(unsigned long nHashID, void* pBuffer)
    {
        u32 index = FindHashIndex(nHashID);
        LoadFileByIndex(index, pBuffer);
    }

    inline void ReadFileAsyncByIndex(unsigned long nFileIndex, void* pBuffer, unsigned long bytesToRead, FileReadAsyncCallback pCallback, uintptr_t userParam)
    {
        m_pReadCallback = pCallback;
        m_readUserParam = userParam;
        BundleFileDirectoryEntry* pEntry = &m_pDirectory[nFileIndex];
        nlSeek(m_pFile, pEntry->m_blockNumber * m_pHeader->nSectorSize, 0);
        nlReadAsync(m_pFile, pBuffer, bytesToRead, &cbFileReadAsyncCallback, (uintptr_t)this);
    }

    inline void LoadFileAsyncByIndex(unsigned long nFileIndex, void* pBuffer, FileReadAsyncCallback pCallback, uintptr_t userParam)
    {
        BundleFileDirectoryEntry* pEntry = &m_pDirectory[nFileIndex];
        ReadFileAsyncByIndex(nFileIndex, pBuffer, pEntry->m_length, pCallback, userParam);
    }

    inline void LoadFileAsync(unsigned long nHashID, void* pBuffer, FileReadAsyncCallback pCallback, uintptr_t userParam)
    {
        u32 index = FindHashIndex(nHashID);
        LoadFileAsyncByIndex(index, pBuffer, pCallback, userParam);
    }

    inline void LoadFileAsync(const char* filename, void* pBuffer, FileReadAsyncCallback pCallback, uintptr_t userParam)
    {
        u32 hash = HashFilename(filename);
        LoadFileAsync(hash, pBuffer, pCallback, userParam);
    }

    inline u32 GetFileIndex(unsigned long nHashID, bool bMissingFileFatal)
    {
        return FindHashIndex(nHashID, bMissingFileFatal);
    }

    inline u32 GetFileIndex(const char* filename, bool bMissingFileFatal)
    {
        u32 hash = HashFilename(filename);
        return FindHashIndex(hash, bMissingFileFatal);
    }

    inline unsigned long GetNumFiles() const
    {
        return m_pHeader->nNumFiles;
    }

public:
    /* 0x00 */ nlFile* m_pFile;
    /* 0x04 */ void (*m_pOpenCallback)(void*, unsigned long, uintptr_t);
    /* 0x08 */ unsigned long m_openUserParam;
    /* 0x0C */ FileReadAsyncCallback m_pReadCallback;
    /* 0x10 */ uintptr_t m_readUserParam;
    /* 0x14 */ BundleFileHeader* m_pHeader;
    /* 0x18 */ BundleFileDirectoryEntry* m_pDirectory;

    inline u32 FindHashIndex(u32 hash) const
    {
        for (u32 i = 0; i < m_pHeader->nNumFiles; i++)
        {
            if (hash == m_pDirectory[i].m_hash)
            {
                return i;
            }
        }
        nlPrintf("ERROR: Failed to find file with hash ID: %d\n", hash);
        return -1U;
    }

    inline u32 FindHashIndex(u32 hash, bool printError) const
    {
        for (u32 i = 0; i < m_pHeader->nNumFiles; i++)
        {
            if (hash == m_pDirectory[i].m_hash)
            {
                return i;
            }
        }
        if (printError)
        {
            nlPrintf("ERROR: Failed to find file with hash ID: %d\n", hash);
        }
        return -1U;
    }
};

#endif // _NLBUNDLEFILE_H_
