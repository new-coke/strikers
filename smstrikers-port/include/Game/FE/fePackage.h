#ifndef _FEPACKAGE_H_
#define _FEPACKAGE_H_

#include "Game/FE/fePresentation.h"

class FELibObject;
class FEResourceHandle;
class TLComponent;

class FEPackage
{
public:
    void Update(float deltaTime);
    FEPresentation* GetPresentation() const;

    /* 0x00 */ TLComponent* m_pComponentList;
    /* 0x04 */ FEPresentation* m_pFEPresentation;
    /* 0x08 */ FEResourceHandle* m_pResourceList;
    /* 0x0C */ FELibObject* m_pFEObjectLibrary;
    /* 0x10 */ unsigned long m_uUniqueID;
    /* 0x14 */ unsigned long m_uResourceCount;
}; // total size: 0x18

#endif // _FEPACKAGE_H_
