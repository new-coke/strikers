#ifndef _FEASYNCIMAGE_H_
#define _FEASYNCIMAGE_H_

#include "types.h"
#include "NL/nlBundleFile.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/tlImageInstance.h"
#include "Game/FE/feTextureResource.h"



enum LoadState
{
    LS_ISSUED_LOAD = 0,
    LS_LOAD_COMPLETE = 1,
    LS_IDLE = 2,
};

class AsyncImage
{
public:
    AsyncImage(const char* filename, const char* texturename);
    virtual ~AsyncImage();
    void QueueLoad(const char* path, bool isblocking);
    bool Update(bool autoswap);
    bool CanSwapTextures() const;
    void SwapTextures();
    void CopyFrom(AsyncImage* image);
    void CopyFrom(void* buffer, int size);
    void FreeLoadBuffer();

    static inline void TextureLoadComplete(void* buffer, unsigned long size, uintptr_t userData)
    {
        AsyncImage* self = (AsyncImage*)userData;
        self->mLoadState = LS_LOAD_COMPLETE;
    }

    /* 0x04 */ BundleFile* mBundleFile;
    /* 0x08 */ TLImageInstance* mImageInstance;
    /* 0x0C */ void* m_loadBuffer;
    /* 0x10 */ u32 mTextureHandle;
    /* 0x14 */ u32 mTextureSize;
    /* 0x18 */ LoadState mLoadState;
};

#endif // _FEASYNCIMAGE_H_
