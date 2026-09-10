#ifndef _FESLIDEMENU_H_
#define _FESLIDEMENU_H_

#include "Game/FE/tlComponentInstance.h"
#include "NL/nlBind.h"
#include "NL/nlFunction.h"
#include "Game/FE/feMenu.h"

class SlideMenuItem
{
public:
    SlideMenuItem(TLComponentInstance* componentInstance, int userEnumType)
        : mSlideMenuHash((unsigned long)-1)
        , mComponentInstance(componentInstance)
        , mUserEnumType(userEnumType)
    {
    }
    virtual ~SlideMenuItem()
    {
    }

    TLComponentInstance* GetComponentInstance() const
    {
        return mComponentInstance;
    }

    int GetUserEnumType() const
    {
        return mUserEnumType;
    }

    void SetUserEnumType(int userEnumType)
    {
        mUserEnumType = userEnumType;
    }

    unsigned long GetHashID() const
    {
        return mSlideMenuHash;
    }

    void SetHashID(unsigned long hashID)
    {
        mSlideMenuHash = hashID;
    }

private:
    /* 0x4 */ unsigned long mSlideMenuHash;
    /* 0x8 */ TLComponentInstance* mComponentInstance;
    /* 0xC */ int mUserEnumType;
}; // total size: 0x10

class SlideMenuList : public MenuList<SlideMenuItem>
{
public:
    SlideMenuList()
    {
        mInputLocked = 0;
    }
    SlideMenuList(TLComponentInstance* comp)
    {
        mInputLocked = 0;
        mComponentInstance = comp;
    }
    virtual ~SlideMenuList()
    {
        int numItems = GetNumItemsAdded();
        if (numItems > 0)
        {
            for (int i = 0; i < numItems; i++)
            {
                delete GetMenuItem(i)->GetType();
            }
        }
    }
    virtual void Update(float dt)
    {
    }

    TLComponentInstance* GetComponentInstance() const
    {
        return mComponentInstance;
    }

    MenuItem<SlideMenuItem>* AddItem(unsigned long hashID, int userEnumType)
    {
        SlideMenuItem* item = new (nlMalloc(sizeof(SlideMenuItem), 8, true))
            SlideMenuItem(mComponentInstance, userEnumType);
        item->SetHashID(hashID);

        MenuItem<SlideMenuItem>* menuItem = MenuList<SlideMenuItem>::AddItem(item);
        MenuItem<SlideMenuItem>::Callback callback(
            Bind<void>(MemFun<SlideMenuList, void>(&SlideMenuList::SetSlide), this));
        menuItem->SetCallback(ON_HIGHLIGHT, callback);
        return menuItem;
    }

    void SetSlide()
    {
        SlideMenuItem* item = GetMenuItem()->GetType();
        item->GetComponentInstance()->SetActiveSlide(item->GetHashID());
    }

private:
    /* 0x214 */ unsigned char mInputLocked;
    /* 0x218 */ TLComponentInstance* mComponentInstance;
}; // total size: 0x21C

class FESlideMenu
{
public:
    struct MenuItem
    {
        MenuItem();
        ~MenuItem();

        /* 0x0 */ unsigned long ItemSlide;
        /* 0x4 */ Function<FnVoidVoid> ItemCBFuncs[2];
    }; // total size: 0x14

    void UpdatePresentation();
    bool PrevItem();
    bool NextItem();
    void SetSlideByIndex(unsigned char index);
    bool OnHighlight()
    {
        MenuItem* item = &m_menuItems[m_currentSlide];
        if (!item->ItemCBFuncs[ON_HIGHLIGHT])
        {
            return false;
        }
        item->ItemCBFuncs[ON_HIGHLIGHT]();
        return true;
    }
    void runCallBack()
    {
        MenuItem* item = &m_menuItems[m_currentSlide];
        if (item->ItemCBFuncs[ON_APPLY])
        {
            item->ItemCBFuncs[ON_APPLY]();
        }
    }
    bool ApplyFunction();
    MenuItem* AddMenuItem(const char* name);
    MenuItem* AddMenuItem(const char* name, const Function<FnVoidVoid>& func);
    ~FESlideMenu();
    FESlideMenu(TLComponentInstance* pWorkPres);

    /* 0x0,  */ MenuItem m_menuItems[16];
    /* 0x140 */ unsigned char m_size;
    /* 0x141 */ unsigned char m_currentSlide;
    /* 0x142 */ unsigned char m_doWrapAround;
    /* 0x144 */ TLComponentInstance* m_pMenuComp;
    /* 0x148 */ unsigned char m_lockInput;
    /* 0x14C */ void* m_callbackParam;
    /* 0x150 */ long mLastChosenSlide;
    /* 0x154 */ long mLastRandomSlide;
    /* 0x158 */ long mNumCyclesRemaining;
    /* 0x15C */ float mRandDeltaTime;
}; // total size: 0x160

#endif // _FESLIDEMENU_H_
