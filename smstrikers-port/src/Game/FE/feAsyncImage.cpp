#include "Game/FE/feAsyncImage.h"

#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "NL/nlMath.h"
#include "NL/nlFileGC.h"
#include "NL/nlBundleFile.h"

#include "NL/gl/gl.h"
#include "NL/gl/glTexture.h"

extern unsigned int nlDefaultSeed;

/**
 * Offset/Address/Size: 0x3E4 | 0x80093E50 | size: 0xC0
 */
AsyncImage::AsyncImage(const char* filename, const char* texturename)
{
    mBundleFile = NULL;
    mImageInstance = NULL;
    m_loadBuffer = NULL;
    mTextureSize = 0;
    mLoadState = LS_IDLE;

    mBundleFile = new (nlMalloc(sizeof(BundleFile), 0x20, 1)) BundleFile();
    mBundleFile->Open(filename);

    int hash;
    if (texturename != NULL)
    {
        hash = nlStringHash(texturename);
    }
    else
    {
        hash = nlRandom(0xFFFFFFFF, &nlDefaultSeed);
    }
    mTextureHandle = hash;
}

/**
 * Offset/Address/Size: 0x354 | 0x80093DC0 | size: 0x90
 */
AsyncImage::~AsyncImage()
{
    while (mLoadState == LS_ISSUED_LOAD)
    {
        nlServiceFileSystem();
    }

    mBundleFile->Close();
    delete mBundleFile;

    if (m_loadBuffer != NULL)
    {
        delete[] (m_loadBuffer);
    }
}

/**
 * Offset/Address/Size: 0x284 | 0x80093CF0 | size: 0xD0
 */
void AsyncImage::QueueLoad(const char* path, bool isblocking)
{
    BundleFileDirectoryEntry info;

    if (mLoadState != LS_ISSUED_LOAD)
    {
        mBundleFile->GetFileInfo(path, &info, true);

        if (m_loadBuffer == NULL)
        {
            m_loadBuffer = (u8*)nlMalloc(info.m_length, 0x20, true);
            mTextureSize = info.m_length;
        }

        mLoadState = LS_ISSUED_LOAD;

        if (isblocking)
        {
            mBundleFile->LoadFile(path, m_loadBuffer);
            mLoadState = LS_LOAD_COMPLETE;
            return;
        }
        mBundleFile->ReadFileAsync(path, m_loadBuffer, mTextureSize, &AsyncImage::TextureLoadComplete, (uintptr_t)this);
    }
}

/**
 * Offset/Address/Size: 0x150 | 0x80093BBC | size: 0x134
 */
bool AsyncImage::Update(bool autoswap)
{
    if (mImageInstance == NULL)
    {
        return false;
    }

    bool res = false;
    if (mLoadState == LS_LOAD_COMPLETE)
    {
        if (mTextureHandle != mImageInstance->m_pTextureResource->m_glTextureHandle && (glTextureLoad(mTextureHandle) == 0))
        {
            glTextureAdd(mTextureHandle, m_loadBuffer, mTextureSize);
            mImageInstance->m_pTextureResource->m_glTextureHandle = mTextureHandle;
        }
    }
    if ((mLoadState == LS_LOAD_COMPLETE) && autoswap)
    {
        bool shouldReplace;
        if (mLoadState != LS_LOAD_COMPLETE)
        {
            shouldReplace = false;
        }
        else
        {
            shouldReplace = false;
            if ((mImageInstance != NULL) && (mImageInstance->m_pTextureResource->m_bValid) && (glTextureLoad(mTextureHandle) != 0))
            {
                shouldReplace = true;
            }
        }

        if (shouldReplace)
        {
            glFinish();
            glTextureReplace(mImageInstance->m_pTextureResource->m_glTextureHandle, m_loadBuffer, mTextureSize);
            glDiscardFrame(1);
            res = true;
            mLoadState = LS_IDLE;
        }
    }
    return res;
}

/**
 * Offset/Address/Size: 0xE0 | 0x80093B4C | size: 0x70
 */
bool AsyncImage::CanSwapTextures() const
{
    bool res;

    if (mLoadState != LS_LOAD_COMPLETE)
    {
        res = 0;
    }
    else
    {
        res = 0;
        if ((mImageInstance != NULL) && (mImageInstance->m_pTextureResource->m_bValid) && (glTextureLoad(mTextureHandle) != 0))
        {
            res = 1;
        }
    }
    return res;
}

/**
 * Offset/Address/Size: 0x94 | 0x80093B00 | size: 0x4C
 */
void AsyncImage::SwapTextures()
{
    glFinish();
    glTextureReplace(mImageInstance->m_pTextureResource->m_glTextureHandle, m_loadBuffer, mTextureSize);
    glDiscardFrame(1);
}

/**
 * Offset/Address/Size: 0x64 | 0x80093AD0 | size: 0x30
 */
void AsyncImage::CopyFrom(AsyncImage* image)
{
    memcpy(m_loadBuffer, image->m_loadBuffer, image->mTextureSize);
}

/**
 * Offset/Address/Size: 0x40 | 0x80093AAC | size: 0x24
 */
void AsyncImage::CopyFrom(void* buffer, int size)
{
    memcpy(m_loadBuffer, buffer, size);
}

/**
 * Offset/Address/Size: 0x0 | 0x80093A6C | size: 0x40
 */
void AsyncImage::FreeLoadBuffer()
{
    if (m_loadBuffer != NULL)
    {
        delete[] m_loadBuffer;
        m_loadBuffer = NULL;
    }
}
