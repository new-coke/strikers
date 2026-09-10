#ifndef _TRANSLIGHT_H_
#define _TRANSLIGHT_H_

#include "NL/nlMath.h"
#include "NL/gl/glDraw2.h"
#include "NL/gl/glLightUserData.h"

#include "Game/Sys/simpleparser.h"
#include "Game/Transitions/ScreenTransitionManager.h"

class TransitionLight
{
public:
    TransitionLight();
    void ApplyLight(float t);
    void LoadFromParser(SimpleParser* parser);
    void AttachToModel(glModel* pModel);
    void DetachFromModel(glModel* pModel);

    /* 0x0, */ float m_fStartScale;                          // size 0x4
    /* 0x4, */ float m_fEndScale;                            // size 0x4
    /* 0x8, */ unsigned long m_nDirType;                     // size 0x4
    /* 0xC, */ unsigned long m_nDirLights;                   // size 0x4
    /* 0x10 */ GLDirectionalLightUserData m_Directionals[3]; // size 0x54
    /* 0x64 */ unsigned long m_nPointType;                   // size 0x4
    /* 0x68 */ unsigned long m_nPointLights;                 // size 0x4
    /* 0x6C */ GLLightUserData m_PointLights[3];             // size 0x78
    /* 0xE4 */ unsigned long m_nAmbientType;                 // size 0x4
    /* 0xE8 */ nlFloatColour m_ScaledAmbient;                // size 0x10
    /* 0xF8 */ nlFloatColour m_Ambient;                      // size 0x10
}; // total size: 0x108

#endif // _TRANSLIGHT_H_
