#ifndef _ANIMTRIGGER_H_
#define _ANIMTRIGGER_H_

#include "NL/nlSlotPool.h"

class AnimTriggerCallbackInfo
{
public:
    static SlotPool<AnimTriggerCallbackInfo> m_AnimTriggerCallbackInfoSlotPool;

    /* 0x0 */ unsigned long m_uEventID;
    /* 0x4 */ float m_fIntensity;
}; // total size: 0x8

#endif // _ANIMTRIGGER_H_
