#ifndef _TLSLIDE_H_
#define _TLSLIDE_H_

#include "types.h"

#include "Game/FE/tlInstance.h"
#include "Game/FE/feAnimation.h"

enum eTimeLinePlayMode
{
    TLPM_STOP_AT_END = 0,
    TLPM_LOOPING = 1,
};

class TLSlide
{
public:
    f32 GetCurrentTime() const
    {
        return m_time;
    }
    void Update(float time);
    void UpdateAsset(TLInstance* instance, float time);

    /* 0x00 */ TLSlide* m_next;
    /* 0x04 */ TLSlide* m_prev;
    /* 0x08 */ TLInstance* m_instances;
    /* 0x0C */ FEAnimation* m_animations;
    /* 0x10 */ f32 m_start;
    /* 0x14 */ f32 m_duration;
    /* 0x18 */ f32 m_time;
    /* 0x1C */ eTimeLinePlayMode m_uPlayMode;
    /* 0x20 */ char m_szName[32];
    /* 0x40 */ u32 m_hash;
};

#endif // _TLSLIDE_H_
