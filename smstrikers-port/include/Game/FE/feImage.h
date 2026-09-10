#ifndef _FEIMAGE_H_
#define _FEIMAGE_H_

#include "Game/FE/feLibObject.h"
#include "Game/FE/tlInstance.h"

class FETextureResource;

class FEImage : public FELibObject
{
public:
    /* 0x68 */ FETextureResource* m_pFeTextureResource;
}; // total size: 0x6C

// TLInstance's accessor bodies are visible only to this TU chain: retail
// tlSlide.o binds them UND, so they must not be inlinable from tlInstance.h,
// while retail feRender.o weak-emits them (DWARF attributes the bodies to a
// tlInstance implementation header this reconstruction folds in here).
inline eTimeLineAssetType TLInstance::GetType() const
{
    return m_type;
}

inline bool TLInstance::IsVisible() const
{
    return m_bVisible;
}

inline FELibObject* TLInstance::GetLibRefObject() const
{
    return (FELibObject*)m_component;
}

#endif // _FEIMAGE_H_
