#include "NL/gl/gl.h"
#include "port/aspect.h"
#include "NL/gl/glPlat.h"
#include "NL/gl/glView.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glStat.h"
#include "NL/gl/glState.h"
#include "NL/gl/glStruct.h"
#include "NL/gl/glTarget.h"
#include "NL/gl/glFont.h"
#include "NL/gl/glConstant.h"
#include "NL/glx/glxTexture.h"
#include "NL/glx/glxLoadModel.h"

#include "NL/nlString.h"

static u32 gl_clearColour = 0x00000040;
static s32 gl_frameCounter = 0;
static s32 gl_nDiscard = 0;
static s32 gl_state = 0;

/**
 * Offset/Address/Size: 0x0 | 0x801D733C | size: 0x20
 */
bool glEndLoadTextureBundle(void* data, unsigned long size)
{
    return glplatEndLoadTextureBundle(data, size);
}

/**
 * Offset/Address/Size: 0x20 | 0x801D735C | size: 0x20
 */
bool glBeginLoadTextureBundle(const char* filename, void (*callback)(void*, unsigned long, void*), void* param)
{
    return glplatBeginLoadTextureBundle(filename, callback, param);
}

/**
 * Offset/Address/Size: 0x40 | 0x801D737C | size: 0x20
 */
glModel* glEndLoadModel(void* data, unsigned long size, unsigned long* pNumModels)
{
    return glplatEndLoadModel(data, size, pNumModels);
}

/**
 * Offset/Address/Size: 0x60 | 0x801D739C | size: 0x20
 */
bool glBeginLoadModel(const char* filename, void (*callback)(void*, unsigned long, void*), void* param)
{
    return glplatBeginLoadModel(filename, callback, param);
}

/**
 * Offset/Address/Size: 0x80 | 0x801D73BC | size: 0x8
 */
u32* glGetClearColour()
{
    return &gl_clearColour;
}

/**
 * Offset/Address/Size: 0x88 | 0x801D73C4 | size: 0x8
 */
float glGetOrthographicHeight()
{
    return 480.f;
}

/**
 * Offset/Address/Size: 0x90 | 0x801D73CC | size: 0x8
 */
float glGetOrthographicWidth()
{
    // PORT: 480 * the display aspect, which is 640 at 4:3 exactly as shipped.
    return (float)PortLogicalFrameWidth();
}

/**
 * Offset/Address/Size: 0x98 | 0x801D73D4 | size: 0x20
 */
bool glLoadTextureBundle(const char* filename)
{
    return glplatLoadTextureBundle(filename);
}

/**
 * Offset/Address/Size: 0xB8 | 0x801D73F4 | size: 0x20
 */
glModel* glLoadModel(const char* filename, unsigned long* pNumModels)
{
    return glplatLoadModel(filename, pNumModels);
}

/**
 * Offset/Address/Size: 0xD8 | 0x801D7414 | size: 0x20
 */
void glFinish()
{
    glplatFinish();
}

/**
 * Offset/Address/Size: 0xF8 | 0x801D7434 | size: 0x14
 */
void glDiscardFrame(int nFrames)
{
    if (nFrames > (s32)gl_nDiscard)
    {
        gl_nDiscard = nFrames;
    }
}

/**
 * Offset/Address/Size: 0x10C | 0x801D7448 | size: 0x58
 */
void glSendFrame()
{
    if ((s32)gl_nDiscard > 0)
    {
        glplatAbortFrame();
        gl_nDiscard -= 1;
    }
    else
    {
        glplatSendFrame();
    }

    gl_ViewReset();
    gl_state = 0;
    gl_frameCounter += 1;
}

/**
 * Offset/Address/Size: 0x164 | 0x801D74A0 | size: 0x28
 */
void glEndFrame()
{
    glplatEndFrame();
    gl_state = 2;
}

/**
 * Offset/Address/Size: 0x18C | 0x801D74C8 | size: 0x60
 */
void glBeginFrame()
{
    s32 var_r31;

    glplatBeginFrame();
    var_r31 = 0;
    gl_state = 1;
    do
    {
        glViewSetProjectionMatrix((eGLView)var_r31, glGetIdentityMatrix());
        glViewSetViewMatrix((eGLView)var_r31, glGetIdentityMatrix());
        var_r31 += 1;
    } while (var_r31 < 0x22);
}

/**
 * Offset/Address/Size: 0x1EC | 0x801D7528 | size: 0x8
 */
bool glHasQuads()
{
    return true;
}

/**
 * Offset/Address/Size: 0x1F4 | 0x801D7530 | size: 0x8
 */
s32 glGetCurrentFrame()
{
    return gl_frameCounter;
}

/**
 * Offset/Address/Size: 0x1FC | 0x801D7538 | size: 0x20
 */
u32 glHash(const char* string)
{
    return nlStringHash(string);
}

/**
 * Offset/Address/Size: 0x21C | 0x801D7558 | size: 0xA8
 */
bool glStartup()
{
    s32 var_r31;

    gl_frameCounter = 0;
    gl_nDiscard = 0;
    gl_state = 0;

    if (glplatStartup(glGetScreenInfo()) == 0)
    {
        return false;
    }

    gl_StatStartup();
    gl_StateStartup();
    var_r31 = 0;
    do
    {
        glSetCurrentTexture(-1, (eGLTextureType)var_r31);
        var_r31 += 1;
    } while (var_r31 < 6);

    glSetCurrentProgram(-1);
    glSetRasterStateDefaults();
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetTextureStateDefaults();
    glSetCurrentTextureState(glHandleizeTextureState());
    gl_MatrixStartup();
    gl_TargetStartup();
    gl_ViewStartup();
    gl_FontStartup();
    gl_ConstantStartup();

    return glplatPostStartup();
}
