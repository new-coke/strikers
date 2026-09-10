#ifndef _GLMODIFY_H_
#define _GLMODIFY_H_

#include "NL/gl/glUserData.h"

enum eGLModifier
{
    GLMod_Program,
    GLMod_DiffuseTex,
    GLMod_GlossTex,
    GLMod_TextureMask,
    GLMod_Num
};

void gl_ModifyAddMapping(eGLModifier modifier, unsigned long willBe);
void gl_ModifyAddMapping(eGLModifier modifier, unsigned long was, unsigned long willBe);
void gl_ModifyClearLastMapping();
void gl_ModifyClearMappings();
glModelPacket* gl_Modify(const glModelPacket* pPacket);
u32 gl_ModifyGetNum();

#endif // _GLMODIFY_H_
