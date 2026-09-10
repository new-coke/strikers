#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "NL/nlMemory.h"

struct AsyncFileLoadData
{
    /* 0x00 */ nlFile* file;
    /* 0x04 */ void* alloc_data;
    /* 0x08 */ unsigned long datasize;
    /* 0x0C */ LoadAsyncCallback callback;
    /* 0x10 */ void* user_data;

    AsyncFileLoadData()
        : file(nullptr)
        , alloc_data(nullptr)
        , datasize(0)
        , callback(nullptr)
        , user_data(nullptr)
    {
    }

    // Constructor with const parameters
    AsyncFileLoadData(nlFile* const f, void* const alloc, const unsigned long size, LoadAsyncCallback const cb, void* const user)
        : file(f)
        , alloc_data(alloc)
        , datasize(size)
        , callback(cb)
        , user_data(user)
    {
    }
};

/**
 * Offset/Address/Size: 0x0 | 0x801CEA30 | size: 0xF8
 */
bool nlLoadEntireFileAsync(const char* filename, LoadAsyncCallback callback, void* user_data, unsigned int alignment, eAllocType type)
{
    unsigned int filesize;
    unsigned long datasize;
    nlFile* file;
    AsyncFileLoadData* _asyncData;
    void* alloc_data;

    file = nlOpen(filename);
    if (file == NULL)
    {
        return 0;
    }
    datasize = file->FileSize(&filesize);
    if (datasize != 0)
    {
        if (type == 1)
        {
            alloc_data = nlMalloc(filesize, alignment, 1);
        }
        else if (type == 0)
        {
            alloc_data = nlMalloc(filesize, alignment, 0);
        }
        else
        {
            alloc_data = user_data;
            user_data = NULL;
        }

        _asyncData = new (nlMalloc(sizeof(AsyncFileLoadData), 8, 1)) AsyncFileLoadData(file, alloc_data, datasize, callback, user_data);
        nlReadAsync(file, alloc_data, datasize, nlLoadEntireFileAsyncCallback, (uintptr_t)_asyncData);
    }
    return 1;
}

/**
 * Offset/Address/Size: 0xF8 | 0x801CEB28 | size: 0x68
 */
void nlLoadEntireFileAsyncCallback(nlFile* file, void* pBuffer, unsigned int uSize, uintptr_t uParam)
{
    AsyncFileLoadData* p = (AsyncFileLoadData*)uParam;
    p->callback(p->alloc_data, (unsigned long)p->datasize, p->user_data);
    delete p->file;
    delete (p);
}

/**
 * Offset/Address/Size: 0x160 | 0x801CEB90 | size: 0xDC
 */
void* nlLoadEntireFile(const char* filename, unsigned long* outSize, unsigned int alignment, eAllocType type)
{
    unsigned int datasize;
    nlFile* file;
    void* alloc_data = NULL;
    unsigned int filesize;

    file = nlOpen(filename);
    if (file != NULL)
    {
        datasize = file->FileSize(&filesize);
        if (datasize != 0)
        {
            if (type == AllocateEnd)
            {
                alloc_data = operator new(filesize, alignment, true);
            }
            else
            {
                alloc_data = operator new(filesize, alignment, false);
            }
            file->Read(alloc_data, datasize);
        }

        delete file;

        if (outSize != NULL)
        {
            *outSize = datasize;
        }
    }
    return alloc_data;
}

/**
 * Offset/Address/Size: 0x23C | 0x801CEC6C | size: 0x38
 */
void nlClose(nlFile* file)
{
    delete file;
}

/**
 * Offset/Address/Size: 0x274 | 0x801CECA4 | size: 0x2C
 */
void nlRead(nlFile* file, void* buffer, unsigned int size)
{
    file->Read(buffer, size);
}

/**
 * Offset/Address/Size: 0x2A0 | 0x801CECD0 | size: 0x2C
 */
unsigned int nlFileSize(nlFile* file, unsigned int* size)
{
    return file->FileSize(size);
}

/**
 * Offset/Address/Size: 0x2CC | 0x801CECFC | size: 0x48
 */
nlFile::~nlFile()
{
}

/**
 * Offset/Address/Size: 0x314 | 0x801CED44 | size: 0x10
 */
nlFile::nlFile()
{
}
