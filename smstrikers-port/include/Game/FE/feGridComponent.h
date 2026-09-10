#ifndef _FEGRIDCOMPONENT_H_
#define _FEGRIDCOMPONENT_H_
#include "NL/nlString.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feMapMenu.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/tlComponentInstance.h"
#include "NL/nlMemory.h"

#include "Game/DB/Cup.h"

class TLSlide;

template <typename T>
class IGridComponent
{
public:
    virtual ~IGridComponent()
    {
        if (mInstanceTable != NULL)
        {
            delete[] mInstanceTable;
            mInstanceTable = NULL;
        }
        delete mMapMenu;
        mMapMenu = NULL;
    }
    virtual void BuildMapMenu() = 0;
    virtual void RebuildInstanceTable() = 0;
    virtual void Update(eFEINPUT_PAD) = 0;
    virtual void MoveHighlightToTarget(T target) = 0;
    virtual T GetSelectedItem() const = 0;

    IGridComponent(TLComponentInstance* parentcomponent, const char* highlitename, bool ismirrored);
    void RebindHighliteComponent(const char* highlitename);
    TLComponentInstance* GetParentComponent() const
    {
        return mParentComponent;
    }

    /* 0x00 */ // vtable
    /* 0x04 */ TLInstance** mInstanceTable;
    /* 0x08 */ FEMapMenu* mMapMenu;
    /* 0x0C */ bool mHighliteVisibilityAtAnimEnd;
    /* 0x10 */ TLComponentInstance* mParentComponent;
    /* 0x14 */ TLComponentInstance* mHighliteComponent;
    /* 0x18 */ bool mIsMirrored;
    /* 0x19 */ bool mHasChangedSinceLastUpdate;
}; // total size: 0x1C

template <typename T>
void IGridComponent<T>::RebindHighliteComponent(const char* highlitename)
{
    mHighliteComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        mParentComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash(highlitename)));
    FEMapMenu* menu = mMapMenu;
    menu->m_highlighter = (TLInstance*)mHighliteComponent;
    menu->UpdateHighlighter();
}

template <typename T>
IGridComponent<T>::IGridComponent(TLComponentInstance* parentcomponent, const char* highlitename, bool ismirrored)
{
    mInstanceTable = NULL;
    mMapMenu = NULL;
    mHighliteVisibilityAtAnimEnd = false;
    mParentComponent = parentcomponent;
    mHighliteComponent = NULL;
    mIsMirrored = ismirrored;
    mHasChangedSinceLastUpdate = false;

    mHighliteComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        parentcomponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash(highlitename)));

    mMapMenu = new (8, false) FEMapMenu((TLInstance*)mHighliteComponent, true);
}
#endif // _FEGRIDCOMPONENT_H_
