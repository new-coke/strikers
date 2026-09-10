#ifndef _TLCOMPONENTINSTANCE_H_
#define _TLCOMPONENTINSTANCE_H_

#include "types.h"

#include "Game/FE/tlInstance.h"
#include "Game/FE/tlSlide.h"

class TLComponentInstance : public TLInstance
{
public:
    TLSlide* GetActiveSlide();
    void SetActiveSlide(TLSlide* slide);
    void SetActiveSlide(unsigned long hash);
    void SetActiveSlide(const char* name);
    void Update(float dt);

    /* 0x80 */ f32 m_fCurrentTime;
}; // total size: 0x84

#endif // _TLCOMPONENTINSTANCE_H_
