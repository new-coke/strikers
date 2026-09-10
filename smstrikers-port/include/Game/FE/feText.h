#ifndef _FETEXT_H_
#define _FETEXT_H_

#include "Game/FE/feLibObject.h"
#include "Game/FE/feFontResource.h"
#include "Game/FE/tlTextInstance.h"

class FEText : public FELibObject
{
public:
    /* 0x68 */ const FEFontResource* m_pFeFontResource;
    /* 0x6C */ FETextLibObjectAttributes m_TextAttributes;
}; // total size: 0x78

#endif // _FETEXT_H_
