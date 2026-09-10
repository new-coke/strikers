#ifndef _FETEXTURERESOURCE_H_
#define _FETEXTURERESOURCE_H_

#include "types.h"
#include "Game/FE/feResourceManager.h"

class FETextureResource : public FEResourceHandle
{
public:
    FETextureResource();

    u32 GetTextureHandle() const
    {
        return m_glTextureHandle;
    }

    /* 0x14 */ u32 m_glTextureHandle;
};

#endif // _FETEXTURERESOURCE_H_
