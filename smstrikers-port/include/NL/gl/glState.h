#ifndef _GLSTATE_H_
#define _GLSTATE_H_

#include "dolphin/gx/GXEnum.h"
#include "NL/gl/glView.h"
#include "NL/glx/glxTexture.h"
#include "NL/gl/glStateBundle.h"

enum eGLState
{
    GLS_DepthTest = 0,
    GLS_DepthWrite = 1,
    GLS_DepthFunc = 2,
    GLS_AlphaTest = 3,
    GLS_AlphaTestRef = 4,
    GLS_AlphaBlend = 5,
    GLS_Culling = 6,
    GLS_ColourWrite = 7,
    GLS_SolidOffset = 8,
    GLS_FillMode = 9,
    GLS_Num = 10,
};

void glSetDefaultState(bool setRasterDefaults);
void glUnHandleizeTextureState(unsigned long long state);
unsigned long long glHandleizeTextureState();
void glUnHandleizeRasterState(unsigned long state);
unsigned long glHandleizeRasterState();
void glSetTextureStateDefaults();
void glSetRasterStateDefaults();
unsigned long glSetTextureState(unsigned long long& texture, eGLTextureState state, unsigned long value);
unsigned long glSetTextureState(eGLTextureState state, unsigned long value);
unsigned long glGetTextureState(unsigned long long texture, eGLTextureState texturestate);
unsigned long glGetTextureState(eGLTextureState texturestate);
u32 glSetRasterState(u32& raster, eGLState state, unsigned long value);
u32 glSetRasterState(eGLState state, unsigned long value);
u32 glGetRasterState(unsigned long raster, eGLState state);
u32 glGetRasterState(eGLState state);
u32 glGetTexture(const char* textureName);
u32 glGetProgram(const char* programName);
uintptr_t glSetCurrentMatrix(uintptr_t matrix);
u64 glSetCurrentTextureState(unsigned long long state);
u64 glGetCurrentTextureState();
u32 glSetCurrentRasterState(unsigned long raster = 0);
u32 glGetCurrentRasterState();
u32 glSetCurrentProgram(unsigned long program);
uintptr_t glSetCurrentTexture(uintptr_t texture, eGLTextureType type);
void glStateRestore(const glStateBundle& state);
void glStateSave(glStateBundle& state);
glStateBundle* gl_GetCurrentStateBundle();
void gl_StateStartup();

#endif // _GLSTATE_H_
