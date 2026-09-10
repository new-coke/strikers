#ifndef _FEFONTRESOURCE_H_
#define _FEFONTRESOURCE_H_

#include "NL/nlFont.h"
#include "Game/FE/feResourceManager.h"

class FEFontResource : public FEResourceHandle
{
public:
    void SetFontReference(nlFont* pFontReference);

    /* 0x14 */ nlFont* m_pFontReference;
};

#endif // _FEFONTRESOURCE_H_
