#ifndef _BLINKER_H_
#define _BLINKER_H_

#include "NL/gl/glModel.h"
#include "Game/GL/GLMaterial.h"

enum eBlinkState
{
    Blink_Open = 0,
    Blink_HalfClosed = 1,
    Blink_Closed = 2,
    Blink_HalfOpen = 3,
    Blink_Num = 4,
};

class Blinker
{
public:
    void Blink(glModel* model);

    void Update(float fDeltaT);
    Blinker(const char* szBaseName, unsigned long model0Hash, GLMaterialList* mats0, GLMaterialList* mats1, unsigned long eyesHash);

    /* 0x00 */ u32 m_uModel0Hash;
    /* 0x04 */ GLMaterialList* m_pMats[2];
    /* 0x0C */ GLMaterialEntry* m_pEyes[2];
    /* 0x14 */ u32 m_uEyesHash;
    /* 0x18 */ unsigned long m_Textures[4];
    /* 0x28 */ f32 m_fTime;
    /* 0x2C */ eBlinkState m_State;
    /* 0x30 */ f32 m_fBlinkTimes[4];
    /* 0x40 */ bool m_bJustDoubleBlinked;
    /* 0x41 */ bool m_bValid;
}; // total size: 0x44

#endif // _BLINKER_H_
