#ifndef _GL_H_
#define _GL_H_

#include "types.h"

// GL View system enums
enum eGLView
{
    GLV_ShadowTexture = 0,
    GLV_GrabTexture = 1,
    GLV_Skybox = 2,
    GLV_Shadowed = 3,
    GLV_Shadow0 = 4,
    GLV_ShadowBlend0 = 5,
    GLV_WorldShadowed = 6,
    GLV_Unshadowed = 7,
    GLV_BigBlackPolygon = 8,
    GLV_Warble = 9,
    GLV_WarbleBlend = 10,
    GLV_Characters = 11,
    GLV_CoPlanar0 = 12,
    GLV_CoPlanar = 13,
    GLV_Shadow1 = 14,
    GLV_ShadowBlend1 = 15,
    GLV_UnsortedPerspective = 16,
    GLV_DepthOfField = 17,
    GLV_LingeringParticles = 18,
    GLV_Particles = 19,
    GLV_InvisiblePlane = 20,
    GLV_ElectricFence = 21,
    GLV_CameraSpace = 22,
    GLV_ScreenBlur = 23,
    GLV_ScreenBlur2 = 24,
    GLV_ScreenGrab = 25,
    GLV_FrontEnd = 26,
    GLV_UnsortedOrtho = 27,
    GLV_Transitions3D = 28,
    GLV_Transitions = 29,
    GLV_Anark3D_BG = 30,
    GLV_Anark = 31,
    GLV_Anark3D_FG = 32,
    GLV_Debug = 33,
    GLV_Num = 34
};

enum eGLTarget
{
    GLTG_None = 0,
    GLTG_Main = 1,
    GLTG_Staging = 2,
    GLTG_GlowConv = 3,
    GLTG_Num = 4
};

enum eGLFilter
{
    GLFilter_None = 0,
    GLFilter_GaussianX = 1,
    GLFilter_GaussianY = 2,
    GLFilter_Box = 3,
    GLFilter_Glow = 4,
    GLFilter_LargeBox = 5,
    GLFilter_Blt = 6,
    GLFilter_Blt_Luminance = 7,
    GLFilter_Num = 8
};

enum eGLViewSort
{
    GLVSort_Texture = 0,
    GLVSort_TransformedDepth = 1,
    GLVSort_TransformedMatrixDepth = 2,
    GLVSort_None = 3,
    GLVSort_Reverse = 4,
    GLVSort_Num = 5
};

struct glModel;

// General GL functions
bool glEndLoadTextureBundle(void* data, unsigned long size);
bool glBeginLoadTextureBundle(const char* filename, void (*callback)(void*, unsigned long, void*), void* param);
glModel* glEndLoadModel(void* data, unsigned long size, unsigned long* pNumModels);
bool glBeginLoadModel(const char* filename, void (*callback)(void*, unsigned long, void*), void* param);
u32* glGetClearColour();
float glGetOrthographicHeight();
float glGetOrthographicWidth();
bool glLoadTextureBundle(const char* filename);
glModel* glLoadModel(const char* filename, unsigned long* pNumModels);
void glFinish();
void glDiscardFrame(int nFrames);
void glSendFrame();
void glEndFrame();
void glBeginFrame();
bool glHasQuads();
s32 glGetCurrentFrame();
u32 glHash(const char* string);
bool glStartup();

#endif // _GL_H_
