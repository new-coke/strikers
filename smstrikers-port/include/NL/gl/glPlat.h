#ifndef _GLPLAT_H_
#define _GLPLAT_H_

#include "NL/nlMath.h"
#include "NL/gl/glView.h"

#include "dolphin/gx/GXStruct.h"

enum eVideoMode
{
    VideoMode_NTSC = 0,
    VideoMode_PAL = 1,
    VideoMode_MPAL = 2,
    VideoMode_PAL60 = 3,
    VideoMode_Num = 4,
};

// struct gl_ScreenInfo // size: 0x28
// {
//     /* 0x00 */ u32 m_screenWidth;
//     /* 0x04 */ u32 m_screenHeight;
//     /* 0x08 */ u32 m_unk_0x08;
//     /* 0x0C */ u32 m_unk_0x0C;
//     /* 0x10 */ u32 m_unk_0x10;
//     /* 0x14 */ u32 m_unk_0x14;
//     /* 0x18 */ u32 m_unk_0x18;
//     /* 0x1C */ u32 m_unk_0x1C;
//     /* 0x20 */ float m_unk_0x20;
//     /* 0x24 */ u32 m_unk_0x24;
// };

struct gl_ScreenInfo
{
    /* 0x00 */ int ScreenWidth;
    /* 0x04 */ int ScreenHeight;
    /* 0x08 */ int ColourDepth[4];
    /* 0x18 */ int ZDepth;
    /* 0x1C */ int StencilDepth;
    /* 0x20 */ float PixelCentre;
    /* 0x24 */ bool FSAA;
}; // total size: 0x28

void glplatViewProjectPoint(eGLView view, const nlVector3& v3world, nlVector3& v3NDC);
void glplatEndFrame();
void glplatBeginFrame();
void glplatFinish();
void glplatAbortFrame();
void glplatSendFrame();
void glx_Fog(bool enable);
bool glx_GetFog();
bool glplatPostStartup();
bool glplatStartup(gl_ScreenInfo* screenInfo);
void glx_SetPal50Mode();
void glx_SetRGB60Mode();
void glx_SetInterlacedMode();
void glx_SetProgressiveMode();
u32 glx_GetResetCode();
void glx_SwitchVideoMode(_GXRenderModeObj* rmode, eVideoMode mode);
bool glplatPreStartup();
void virt_cb(unsigned long faultAddr, unsigned long mainAddr, unsigned long pageIndex, unsigned long elapsed, int wroteBack);
void glx_ClearXFB(void* cache);
u32 glx_GetTargetFPS();
u32 glx_GetScaledXFBWidth();
void glx_SetFog(int type);
#endif // _GLPLAT_H_
