#ifndef _DEPTHOFFIELD_H_
#define _DEPTHOFFIELD_H_

#include "types.h"

class DepthOfFieldManager
{
public:
    static DepthOfFieldManager instance;

    DepthOfFieldManager()
    {
        m_bOn = false;
        m_bDebugView = false;
        m_fDistanceFromCamera = 16.0f;
        m_fIntensity = 1.0f;
    }

    void Initialize();
    void TurnOn();
    void TurnOff();
    void Update();

    /* 0x0 */ unsigned char m_bOn;
    /* 0x1 */ unsigned char m_bDebugView;
    /* 0x4 */ float m_fDistanceFromCamera;
    /* 0x8 */ float m_fIntensity;
}; // total size: 0xC

#endif // _DEPTHOFFIELD_H_
