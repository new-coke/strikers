#ifndef _CHARACTEREFFECTS_H_
#define _CHARACTEREFFECTS_H_

#include "types.h"

#include "NL/gl/glState.h"

enum eEffectsTextureType
{
    eFXTex_Nothing = 0,
    eFXTex_Freeze = 1,
    eFXTex_Fire = 2,
    eFXTex_Star = 3,
    eFXTex_Electrocution = 4,
    eFXTex_Num = 5,
};

enum eGLBlend
{
    GLB_None = 0,
    GLB_Standard = 1,
    GLB_Additive = 2,
    GLB_ScaledAdditive = 3,
    GLB_Luminance = 4,
    GLB_Screen = 5,
    GLB_PassThru = 6,
    GLB_Subtractive = 7,
    GLB_Num = 8,
};

class EffectsTexturing
{
public:
    /* 0x0 */ unsigned long m_uTexture;
    /* 0x4 */ eGLBlend m_eBlendMode;
    /* 0x8 */ bool m_bEnviro;
    /* 0x9 */ bool m_bDetail;
}; // total size: 0xC

EffectsTexturing* fxGetTexturing(eEffectsTextureType type);

#endif // _CHARACTEREFFECTS_H_
