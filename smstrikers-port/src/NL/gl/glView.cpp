#include "NL/gl/glView.h"
#include "NL/gl/glPlat.h"
#include "NL/gl/glStruct.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glRenderList.h"
#include "NL/nlMemory.h"
#include <string.h>

bool gl_ViewEnable[34] = { true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };

glView* views[34];

/**
 * Offset/Address/Size: 0x0 | 0x801DE4A4 | size: 0x18
 */
bool glViewSetEnable(eGLView view, bool enable)
{
    bool bPrev = gl_ViewEnable[view];
    gl_ViewEnable[view] = enable;
    return bPrev;
}

/**
 * Offset/Address/Size: 0x18 | 0x801DE4BC | size: 0x10
 */
bool glViewGetEnable(eGLView view)
{
    return gl_ViewEnable[view];
}

/**
 * Offset/Address/Size: 0x28 | 0x801DE4CC | size: 0x1C
 */
eGLTarget glViewSetFilterSource(eGLView view, eGLTarget target)
{
    eGLTarget prev;
    glView* v;

    v = views[view];
    prev = v->filterSource;
    v->filterSource = target;
    return prev;
}

/**
 * Offset/Address/Size: 0x44 | 0x801DE4E8 | size: 0x18
 */
eGLFilter glViewGetFilter(eGLView view)
{
    return views[view]->filter;
}

/**
 * Offset/Address/Size: 0x5C | 0x801DE500 | size: 0x1C
 */
eGLFilter glViewSetFilter(eGLView view, eGLFilter filter)
{
    eGLFilter prev = views[view]->filter;
    views[view]->filter = filter;
    return prev;
}

/**
 * Offset/Address/Size: 0x78 | 0x801DE51C | size: 0x1C
 */
eGLTarget glViewSetTarget(eGLView view, eGLTarget target)
{
    eGLTarget prev = views[view]->target;
    views[view]->target = target;
    return prev;
}

/**
 * Offset/Address/Size: 0x94 | 0x801DE538 | size: 0x20
 */
void glViewProjectPoint(eGLView view, const nlVector3& v3world, nlVector3& v3NDC)
{
    glplatViewProjectPoint(view, v3world, v3NDC);
}

/**
 * Offset/Address/Size: 0xB4 | 0x801DE558 | size: 0x94
 */
void glViewGetProjectionMatrix(eGLView view, nlMatrix4& m)
{
    m = views[view]->projm;
}

/**
 * Offset/Address/Size: 0x148 | 0x801DE5EC | size: 0x94
 */
void glViewGetViewMatrix(eGLView view, nlMatrix4& m)
{
    m = views[view]->viewm;
}

/**
 * Offset/Address/Size: 0x1DC | 0x801DE680 | size: 0x54
 */
nlMatrix4* glViewSetProjectionMatrix(eGLView view, uintptr_t matrix)
{
    glView* v = views[view];
    nlMatrix4* prev = v->projMatrix;
    v->projMatrix = (nlMatrix4*)matrix;
    v->bConcatDirty = 1;

    glGetMatrix(matrix, v->projm);

    return prev;
}

/**
 * Offset/Address/Size: 0x230 | 0x801DE6D4 | size: 0x18
 */
nlMatrix4* glViewGetProjectionMatrix(eGLView view)
{
    glView* v = views[view];
    return v->projMatrix;
}

/**
 * Offset/Address/Size: 0x248 | 0x801DE6EC | size: 0x54
 */
nlMatrix4* glViewSetViewMatrix(eGLView view, uintptr_t matrix)
{
    glView* v = views[view];
    nlMatrix4* prev = v->viewMatrix;
    v->viewMatrix = (nlMatrix4*)matrix;
    v->bConcatDirty = 1;

    glGetMatrix(matrix, v->viewm);

    return prev;
}

/**
 * Offset/Address/Size: 0x29C | 0x801DE740 | size: 0x18
 */
nlMatrix4* glViewGetViewMatrix(eGLView view)
{
    glView* v = views[view];
    return v->viewMatrix;
}

/**
 * Offset/Address/Size: 0x2B4 | 0x801DE758 | size: 0x24
 */
eGLViewSort glViewSetSortMode(eGLView view, eGLViewSort sort)
{
    glView* v = views[view];
    eGLViewSort prev = v->sortMode;
    v->sortMode = sort;
    v->renderList->sortMode = sort;
    return prev;
}

/**
 * Offset/Address/Size: 0x2D8 | 0x801DE77C | size: 0x1C
 */
bool glViewSetDepthClear(eGLView view, bool bClear)
{
    glView* v = views[view];
    bool bPrev = v->bClearDepth;
    v->bClearDepth = bClear;
    return bPrev;
}

/**
 * Offset/Address/Size: 0x2F4 | 0x801DE798 | size: 0x18
 */
bool glViewGetDepthClear(eGLView view)
{
    glView* v = views[view];
    return v->bClearDepth;
}

/**
 * Offset/Address/Size: 0x30C | 0x801DE7B0 | size: 0x164
 */
void gl_ViewStartup()
{
    s32 i;
    eGLView view;
    uintptr_t identity;            // PORT: a matrix handle
    u32 screenWidth;
    u32 screenHeight;
    glView* pView;
    GLRenderList* renderList;

    screenWidth = glGetScreenWidth();
    screenHeight = glGetScreenHeight();
    identity = glGetIdentityMatrix();

    for (i = 0; i < 0x22; i++)
    {
        view = (eGLView)i;
        gl_ViewEnable[i] = TRUE;

        pView = (glView*)nlMalloc(sizeof(glView), 8, FALSE);
        if (pView != NULL)
        {
            renderList = new (nlMalloc(sizeof(GLRenderList), 8, FALSE)) GLRenderList();
            pView->renderList = renderList;
            pView->preViewCallback = NULL;
            pView->postViewCallback = NULL;
            pView->bClearColour = FALSE;
            pView->bClearDepth = FALSE;
        }

        views[i] = pView;

        pView = views[view];
        pView->sortMode = GLVSort_Texture;
        renderList = pView->renderList;
        renderList->sortMode = GLVSort_Texture;

        pView = views[view];
        pView->viewMatrix = (nlMatrix4*)identity;
        pView->bConcatDirty = TRUE;
        glGetMatrix(identity, pView->viewm);

        pView = views[view];
        pView->projMatrix = (nlMatrix4*)identity;
        pView->bConcatDirty = TRUE;
        glGetMatrix(identity, pView->projm);

        pView = views[view];
        pView->target = GLTG_None;

        pView = views[view];
        pView->vpX = 0;
        pView->vpY = 0;
        pView->vpWidth = screenWidth;
        pView->vpHeight = screenHeight;

        pView = views[view];
        pView->filter = GLFilter_None;
        pView = views[view];
        pView->filterSource = GLTG_None;
        pView = views[view];
        pView->bClearColour = FALSE;
        pView = views[view];
        pView->bClearDepth = FALSE;

        views[i]->renderList->view = (eGLView)i;
    }
}

/**
 * Offset/Address/Size: 0x470 | 0x801DE914 | size: 0xA8
 */
#pragma opt_common_subs off
void gl_ViewIterate(eGLView view, glViewPacketCallback cb)
{
    if (views[view]->preViewCallback != NULL)
    {
        views[view]->preViewCallback(view, 1);
    }

    views[view]->renderList->Iterate(view, cb);

    if (views[view]->postViewCallback != NULL)
    {
        views[view]->postViewCallback(view, 0);
    }
}
#pragma opt_common_subs on

/**
 * Offset/Address/Size: 0x518 | 0x801DE9BC | size: 0x54
 */
void glViewCompact()
{
    for (int i = 0; i < 34; i++)
    {
        views[i]->renderList->Compact();
    }
}

/**
 * Offset/Address/Size: 0x56C | 0x801DEA10 | size: 0x68
 */
void gl_ViewReset()
{
    for (int i = 0; i < 0x22; i++)
    {
        views[i]->renderList->Clear();
        views[i]->bConcatDirty = 1;
    }
}

/**
 * Offset/Address/Size: 0x5D4 | 0x801DEA78 | size: 0x70
 */
void glViewAttachPacket(eGLView view, const glModelPacket* packet)
{
    glModel model;
    memset(&model, 0, sizeof(glModel));
    model.numPackets = 1;
    model.packets = (glModelPacket*)packet;

    glView* v = views[view];
    v->renderList->AttachModel(&model, 0);
}

/**
 * Offset/Address/Size: 0x644 | 0x801DEAE8 | size: 0x38
 */
void glViewAttachModel(eGLView view, const glModel* model)
{
    glView* v = views[view];
    v->renderList->AttachModel(model, 0);
}

/**
 * Offset/Address/Size: 0x67C | 0x801DEB20 | size: 0x40
 */
void glViewAttachModel(eGLView view, unsigned long arg, const glModel* model)
{
    glView* v = views[view];
    v->renderList->AttachModel(model, arg);
}

/**
 * Offset/Address/Size: 0x6BC | 0x801DEB60 | size: 0x18
 */
GLRenderList* gl_ViewGetRenderList(eGLView view)
{
    return views[view]->renderList;
}
