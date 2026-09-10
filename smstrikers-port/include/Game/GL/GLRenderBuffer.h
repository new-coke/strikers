#ifndef _GLRENDERBUFFER_H_
#define _GLRENDERBUFFER_H_

#include "NL/nlDLListContainer.h"

class GLDrawableData
{

public:
    GLDrawableData();
    virtual ~GLDrawableData();

    /* 0x4 */ bool m_bVisible;
}; // total size: 0x8

class GLRenderBuffer
{
public:
    GLRenderBuffer();
    ~GLRenderBuffer();

    /* 0x0 */ nlDLListContainer<GLDrawableData*> m_drawableData;
    /* 0x8 */ bool m_bEnabled;
    /* 0x9 */ bool m_bSending;
    /* 0xA */ bool m_bExclusive;
    /* 0xC */ u32 m_uAllocated;

    static bool m_bInstance;
}; // total size: 0x10

// class DLListContainerBase<GLDrawableData*, NewAdapter<DLListEntry<GLDrawableData*>>>
// {
// public:
// };

// class nlWalkDLRing<DLListEntry<GLDrawableData*>, DLListContainerBase<GLDrawableData*, NewAdapter<DLListEntry<GLDrawableData*>>>>(DLListEntry<GLDrawableData*>*, DLListContainerBase<GLDrawableData*, NewAdapter<DLListEntry<GLDrawableData*>>>*, void (DLListContainerBase<GLDrawableData*, NewAdapter<DLListEntry<GLDrawableData*>>>
// {
// public:
// };

// class nlWalkRing<DLListEntry<GLDrawableData*>, DLListContainerBase<GLDrawableData*, NewAdapter<DLListEntry<GLDrawableData*>>>>(DLListEntry<GLDrawableData*>*, DLListContainerBase<GLDrawableData*, NewAdapter<DLListEntry<GLDrawableData*>>>*, void (DLListContainerBase<GLDrawableData*, NewAdapter<DLListEntry<GLDrawableData*>>>
// {
// public:
// };

#endif // _GLRENDERBUFFER_H_
