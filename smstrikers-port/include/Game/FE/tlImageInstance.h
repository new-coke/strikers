#ifndef _TLIMAGEINSTANCE_H_
#define _TLIMAGEINSTANCE_H_

#include "Game/FE/tlInstance.h"

class FETextureResource;

class TLImageInstance : public TLInstance
{
public:
    /* 0x80 */ FETextureResource* m_pTextureResource;
}; // total size: 0x84

#endif // _TLIMAGEINSTANCE_H_
