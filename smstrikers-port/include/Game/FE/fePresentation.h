#ifndef _FEPRESENTATION_H_
#define _FEPRESENTATION_H_

#include "types.h"

#include "Game/FE/tlSlide.h"

// class TLInstance;
// class FEAnimation;

// class TLSlide
// {
// public:
//     void Update(float deltaTime);

//     /* 0x00 */ TLSlide* m_next;
//     /* 0x04 */ char pad0[0x4];
//     /* 0x08 */ TLInstance* unk8;
//     /* 0x0C */ FEAnimation* unkC;
//     /* 0x10 */ f32 unk10;
//     /* 0x14 */ f32 unk14;
//     /* 0x18 */ f32 unk18;
//     /* 0x1C */ s32 unk1C;
//     /* 0x20 */ char pad20[0x20];
//     /* 0x40 */ u32 m_hash;
// };

class FEPresentation
{
public:
    void Update(float deltaTime);
    TLSlide* GetActiveSlide()
    {
        return m_currentSlide;
    }
    void SetActiveSlide(TLSlide* slide);
    void SetActiveSlide(unsigned long hash);
    void SetActiveSlide(const char* slideName);

    /* 0x0 */ TLSlide* m_slides;
    /* 0x4 */ TLSlide* m_currentSlide;
    /* 0x8 */ f32 m_fadeDuration;
};

#endif // _FEPRESENTATION_H_
