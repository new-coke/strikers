#ifndef _TLINSTANCE_H_
#define _TLINSTANCE_H_

#include "NL/nlColour.h"
#include "NL/nlMath.h"

#include "Game/FE/tlComponent.h"
#include "Game/FE/FEAudio.h"

enum eTimeLineAssetType
{
    TLAT_UNKNOWN = -1,
    TLAT_SLIDE = 0,
    TLAT_LAYER = 1,
    TLAT_IMAGE = 2,
    TLAT_TEXT = 3,
    TLAT_COMPONENT = 4,
    TLAT_GROUP = 5,
    TLAT_MAX_TYPES = 6,
};

class TLInstance
{
public:
    eTimeLineAssetType GetType() const;
    unsigned long GetHashID() const
    {
        return m_hash;
    }
    bool IsVisible() const;
    FELibObject* GetLibRefObject() const;

    void SetAssetColour(const nlColour& color);
    void SetAssetScale(float x, float y, float z);
    void SetAssetRotation(float x, float y, float z);
    void SetAssetPosition(float x, float y, float z);
    bool IsValidAtTime(float fCurrentTime);
    nlColour& GetColour() const;
    feVector3& GetScale() const;
    feVector3& GetRotation() const;
    feVector3& GetPosition() const;
    nlColour& GetAssetColour() const;
    feVector3& GetAssetScale() const;
    feVector3& GetAssetRotation() const;
    feVector3& GetAssetPosition() const;

    /* 0x00 */ TLInstance* m_next;
    /* 0x04 */ TLInstance* m_prev;
    /* 0x08 */ TLInstance* pChildren;
    /* 0x0C */ TLComponent* m_component;
    /* 0x10 */ f32 m_fStartTime;
    /* 0x14 */ f32 m_fDuration;
    /* 0x18 */ char m_szName[32];
    /* 0x38 */ unsigned long m_hash;
    /* 0x3C */ FELibObjectAttributes m_overloadedAttributes;
    /* 0x74 */ unsigned long m_overloadFlags;
    /* 0x78 */ eTimeLineAssetType m_type;
    /* 0x7C */ unsigned short m_priority;
    /* 0x7E */ bool m_bVisible;
}; // total size: 0x80

// PORT: TLInstance's accessors are declared above and defined inline in Game/FE/feImage.h, which a TU seeing only the declaration cannot emit.
#include "Game/FE/feImage.h"

#endif // _TLINSTANCE_H_
