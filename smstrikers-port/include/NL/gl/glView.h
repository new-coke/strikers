#ifndef _GLVIEW_H_
#define _GLVIEW_H_

#include "NL/nlMath.h"
#include "NL/gl/gl.h"

#include "NL/gl/glModel.h"

class GLRenderList;

typedef void (*glViewIterateCallback)(eGLView, unsigned long);
typedef void (*glViewPacketCallback)(eGLView, unsigned long, const glModelPacket*);

struct glView
{
    /* 0x00 */ eGLViewSort sortMode;
    /* 0x04 */ u32 vpX;
    /* 0x08 */ u32 vpY;
    /* 0x0C */ u32 vpWidth;
    /* 0x10 */ u32 vpHeight;
    /* 0x14 */ nlMatrix4* viewMatrix;
    /* 0x18 */ nlMatrix4* projMatrix;
    /* 0x1C */ eGLTarget target;
    /* 0x20 */ nlMatrix4 viewm;
    /* 0x60 */ nlMatrix4 projm;
    /* 0xA0 */ nlMatrix4 concatm; // projm * viewm, rebuilt when bConcatDirty
    /* 0xE0 */ bool bConcatDirty;
    /* 0xE4 */ eGLFilter filter;
    /* 0xE8 */ eGLTarget filterSource;
    /* 0xEC */ bool bClearColour;
    /* 0xED */ bool bClearDepth;
    /* 0xEE */ bool pad_0xEE;
    /* 0xEF */ bool pad_0xEF;
    /* 0xF0 */ GLRenderList* renderList;
    /* 0xF4 */ glViewIterateCallback preViewCallback;
    /* 0xF8 */ glViewIterateCallback postViewCallback;
};

bool glViewSetEnable(eGLView view, bool enable);
bool glViewGetEnable(eGLView view);
eGLTarget glViewSetFilterSource(eGLView view, eGLTarget target);
eGLFilter glViewGetFilter(eGLView view);
eGLFilter glViewSetFilter(eGLView view, eGLFilter filter);
eGLTarget glViewSetTarget(eGLView view, eGLTarget target);
void glViewProjectPoint(eGLView view, const nlVector3& v3world, nlVector3& v3NDC);
void glViewGetProjectionMatrix(eGLView view, nlMatrix4& m);
void glViewGetViewMatrix(eGLView view, nlMatrix4& m);
nlMatrix4* glViewSetProjectionMatrix(eGLView view, uintptr_t matrix);
nlMatrix4* glViewGetProjectionMatrix(eGLView view);
nlMatrix4* glViewSetViewMatrix(eGLView view, uintptr_t matrix);
nlMatrix4* glViewGetViewMatrix(eGLView view);
eGLViewSort glViewSetSortMode(eGLView view, eGLViewSort sort);
bool glViewSetDepthClear(eGLView view, bool bClear);
bool glViewGetDepthClear(eGLView view);
void gl_ViewStartup();
void gl_ViewIterate(eGLView view, glViewPacketCallback cb);
void glViewCompact();
void gl_ViewReset();
void glViewAttachPacket(eGLView view, const glModelPacket* packet);
void glViewAttachModel(eGLView view, const glModel* model);
void glViewAttachModel(eGLView view, unsigned long arg, const glModel* model);
GLRenderList* gl_ViewGetRenderList(eGLView view);

#endif // _GLVIEW_H_
