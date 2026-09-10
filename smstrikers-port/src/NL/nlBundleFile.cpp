#include "NL/nlBundleFile.h"
#include "port/endian.h"
#include "dolphin/os.h"
#include <stdlib.h>
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include <string.h>

/**
 * Offset/Address/Size: 0x0 | 0x801E85CC | size: 0xD4
 */
void BundleFile::ReadFileAsync(unsigned long hash, void* buffer, unsigned long size, FileReadAsyncCallback callback, uintptr_t userParam)
{
    u32 index = FindHashIndex(hash);
    m_pReadCallback = callback;
    m_readUserParam = userParam;
    BundleFileDirectoryEntry* entry = &m_pDirectory[index];
    nlSeek(m_pFile, entry->m_blockNumber * m_pHeader->nSectorSize, 0);
    nlReadAsync(m_pFile, buffer, size, &cbFileReadAsyncCallback, (uintptr_t)this);
}

/**
 * Offset/Address/Size: 0xD4 | 0x801E86A0 | size: 0x138
 */
void BundleFile::ReadFileAsync(const char* filename, void* buffer, unsigned long size, FileReadAsyncCallback callback, uintptr_t userParam)
{
    const u32 index = FindHashIndex(HashFilename(filename));
    m_pReadCallback = callback;
    m_readUserParam = userParam;
    BundleFileDirectoryEntry* entry = &m_pDirectory[index];
    nlSeek(m_pFile, entry->m_blockNumber * m_pHeader->nSectorSize, 0);
    nlReadAsync(m_pFile, buffer, size, &cbFileReadAsyncCallback, (uintptr_t)this);
}

/**
 * Offset/Address/Size: 0x20C | 0x801E87D8 | size: 0x118
 */
void BundleFile::LoadFile(const char* filename, void* pBuffer)
{
    LoadFile(HashFilename(filename), pBuffer);
}

/**
 * Offset/Address/Size: 0x324 | 0x801E88F0 | size: 0x74
 */
void BundleFile::ReadFileByIndex(unsigned long index, void* buffer, unsigned long size)
{
    BundleFileDirectoryEntry* entry = &m_pDirectory[index];
    nlSeek(m_pFile, entry->m_blockNumber * m_pHeader->nSectorSize, 0);
    nlRead(m_pFile, buffer, entry->m_length);
}

/**
 * Offset/Address/Size: 0x398 | 0x801E8964 | size: 0xC4
 */
void BundleFile::ReadFile(unsigned long hash, void* buffer, unsigned long size)
{
    u32 index = FindHashIndex(hash);
    BundleFileDirectoryEntry* entry = &m_pDirectory[index];
    nlSeek(m_pFile, entry->m_blockNumber * m_pHeader->nSectorSize, 0);
    nlRead(m_pFile, buffer, entry->m_length);
}

/**
 * Offset/Address/Size: 0x45C | 0x801E8A28 | size: 0x118
 */
void BundleFile::ReadFile(const char* filename, void* pBuffer, unsigned long size)
{
    LoadFile(HashFilename(filename), pBuffer);
}

/**
 * Offset/Address/Size: 0x574 | 0x801E8B40 | size: 0x50
 */
bool BundleFile::GetFileInfoByIndex(unsigned long index, BundleFileDirectoryEntry* entry)
{
    if (index < (u32)m_pHeader->nNumFiles)
    {
        memcpy((void*)entry, (void*)&m_pDirectory[index], sizeof(BundleFileDirectoryEntry));
        return 1;
    }
    return 0;
}

/**
 * Offset/Address/Size: 0x5C4 | 0x801E8B90 | size: 0xE8
 */
bool BundleFile::GetFileInfo(unsigned long hash, BundleFileDirectoryEntry* entry, bool printError)
{
    u32 index = FindHashIndex(hash, printError);

    if ((index == -1U) && (printError == 0))
    {
        return 0;
    }

    if (index < (u32)m_pHeader->nNumFiles)
    {
        memcpy((void*)entry, &m_pDirectory[index], sizeof(BundleFileDirectoryEntry));
        return 1;
    }

    return 0;
}

/**
 * Offset/Address/Size: 0x6AC | 0x801E8C78 | size: 0x13C
 */
bool BundleFile::GetFileInfo(const char* filename, BundleFileDirectoryEntry* entry, bool printError)
{
    u32 index = GetFileIndex(filename, printError);

    if ((index == -1U) && (printError == 0))
    {
        return 0;
    }

    if (index < (u32)m_pHeader->nNumFiles)
    {
        memcpy((void*)entry, &m_pDirectory[index], sizeof(BundleFileDirectoryEntry));
        return 1;
    }

    return 0;
}

/**
 * Offset/Address/Size: 0x7E8 | 0x801E8DB4 | size: 0x58
 */
void BundleFile::Close()
{
    if ((uintptr_t)m_pFile != NULL)
    {
        nlClose(m_pFile);
        m_pFile = NULL;
    }

    if ((uintptr_t)m_pDirectory != NULL)
    {
        delete[] m_pDirectory;
        m_pDirectory = NULL;
    }
}

/**
 * Offset/Address/Size: 0x840 | 0x801E8E0C | size: 0xA8
 */
bool BundleFile::Open(const char* filename)
{
    m_pFile = nlOpen(filename);
    if ((void*)m_pFile == NULL)
    {
        return 0;
    }
    nlRead(m_pFile, m_pHeader, 0x10);
    // PORT: four big-endian u32s, all four used before anything else runs.
    port_be32_array(m_pHeader, 4);

    nlSeek(m_pFile, m_pHeader->nDirectoryOffsetInSectors * m_pHeader->nSectorSize, 0);
    m_pDirectory = (BundleFileDirectoryEntry*)nlMalloc(m_pHeader->nNumFiles * 0xC, 0x20, 0);
    nlRead(m_pFile, m_pDirectory, m_pHeader->nNumFiles * 0xC);
    // PORT: three u32s per entry; every lookup below reads them directly.
    port_be32_array(m_pDirectory, m_pHeader->nNumFiles * 3);

    if (getenv("STRIKERS_LOG_BUNDLES") != NULL)
        OSReport("[port] bundle '%s': %lu files, sector %lu, dir@%lu\n",
                 filename, (unsigned long)m_pHeader->nNumFiles,
                 (unsigned long)m_pHeader->nSectorSize,
                 (unsigned long)m_pHeader->nDirectoryOffsetInSectors);
    return 1;
}

/**
 * Offset/Address/Size: 0x8E8 | 0x801E8EB4 | size: 0x8C
 */
BundleFile::~BundleFile()
{
    if (m_pFile != NULL)
    {
        nlClose(m_pFile);
        m_pFile = NULL;
    }

    if (m_pDirectory != 0U)
    {
        delete[] m_pDirectory;
        m_pDirectory = NULL;
    }

    delete m_pHeader;
    m_pHeader = NULL;
}

/**
 * Offset/Address/Size: 0x974 | 0x801E8F40 | size: 0x70
 */
BundleFile::BundleFile()
{
    m_pFile = 0;
    m_pOpenCallback = 0;
    m_openUserParam = 0;
    m_pReadCallback = NULL;
    m_readUserParam = 0;
    m_pHeader = 0;
    m_pDirectory = 0;
    m_pHeader = (BundleFileHeader*)nlMalloc(sizeof(BundleFileHeader), 0x20, 0);
    memset(m_pHeader, 0, sizeof(BundleFileHeader));
}

/**
 * Offset/Address/Size: 0x9E4 | 0x801E8FB0 | size: 0x50
 */
static void cbFileReadAsyncCallback(nlFile* file, void* buffer, unsigned int arg, uintptr_t bundlePtr)
{
    BundleFile* bundleFile = (BundleFile*)bundlePtr;
    bundleFile->m_pReadCallback(buffer, arg, bundleFile->m_readUserParam);
    bundleFile->m_pReadCallback = NULL;
    bundleFile->m_readUserParam = 0;
}
