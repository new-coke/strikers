#ifndef _FESOUNDKEYFRAMETRIGGER_H_
#define _FESOUNDKEYFRAMETRIGGER_H_

#include "Game/FE/tlSlide.h"

class SoundKeyframeTrigger
{
public:
    void Update(float previoustime, float currenttime);

    /*0x00*/ TLSlide* m_slide;
};

#endif // _FESOUNDKEYFRAMETRIGGER_H_
