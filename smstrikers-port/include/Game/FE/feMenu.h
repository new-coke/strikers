#ifndef _FEMENU_H_
#define _FEMENU_H_

#include "NL/nlFunction1.h"

enum MenuAction
{
    ON_INVALID = -1,
    ON_APPLY = 0,
    ON_HIGHLIGHT = 1,
    ON_UNHIGHLIGHT = 2,
    NUM_ACTIONS = 3,
};

enum MenuResult
{
    RES_ERROR = 0,
    RES_OK = 1,
    RES_NOT_CHANGED = 2,
    RES_ITEM_DISABLED = 3,
    RES_NO_CALLBACK_FUNC = 4,
};

// #pragma push
// #pragma pack(1)
template <typename T>
struct MenuItem
{
    typedef void FnCallback(T*);
    typedef Function<FnCallback> Callback;

private:
    /*  0x00 */ Callback mCallbacks[3];
    /*  0x18 */ T* mType;
    /*  0x1C */ bool mDisabled;
    /*  0x1D */ bool mLocked;

public:
    MenuItem()
        : mType(0)
        , mDisabled(false)
        , mLocked(false) { };

    T* GetType() const
    {
        return mType;
    }

    void SetType(T* type)
    {
        mType = type;
    }

    bool IsValidCallback(MenuAction action)
    {
        return !mCallbacks[action].Empty();
    }

    MenuResult RunCallback(MenuAction action)
    {
        MenuResult result;
        bool valid = IsValidCallback(action);
        if (valid)
        {
            if (action == ON_APPLY && mDisabled)
            {
                result = RES_ITEM_DISABLED;
            }
            else
            {
                mCallbacks[action](mType);
                result = RES_OK;
            }
        }
        else
        {
            result = RES_NO_CALLBACK_FUNC;
        }
        return result;
    }

    void SetCallback(MenuAction action, const Callback& callback)
    {
        mCallbacks[action] = callback;
    }

    void SetDisabledFlag(bool disabled)
    {
        mDisabled = disabled;
    }

    bool IsDisabled() const
    {
        return mDisabled;
    }

    void SetLockedFlag(bool locked)
    {
        mLocked = locked;
    }

    bool IsLocked() const
    {
        return mLocked;
    }
}; // total size: 0x20

template <typename T>
class MenuList
{
public:
    MenuList()
        : mCurrentIndex(0)
        , mNumItemsAdded(0)
        , mFlags(0) { };
    virtual ~MenuList() { };

    MenuItem<T>* AddItem(T* type)
    {
        MenuItem<T>* item = &mMenuItems[mNumItemsAdded];
        mMenuItems[mNumItemsAdded].SetType(type);
        mNumItemsAdded++;
        return item;
    }

    void SetItem(int index)
    {
        mMenuItems[mCurrentIndex].RunCallback(ON_UNHIGHLIGHT);
        mCurrentIndex = index;
        mMenuItems[mCurrentIndex].RunCallback(ON_HIGHLIGHT);
    }

    MenuResult RunCallbackOnCurrent(MenuAction action)
    {
        return mMenuItems[mCurrentIndex].RunCallback(action);
    }

    MenuResult NextItem()
    {
        MenuResult result = RES_ERROR;
        int index = mCurrentIndex + 1;

        while (true)
        {
            if (TestFlag(1))
            {
                index %= mNumItemsAdded;
            }
            else if (index >= mNumItemsAdded)
            {
                result = RES_NOT_CHANGED;
                break;
            }

            if (TestFlag(2) && mMenuItems[index].IsDisabled())
            {
                index++;
                continue;
            }

            SetItem(index);
            result = RES_OK;
            break;
        }

        return result;
    }

    MenuResult PreviousItem()
    {
        int index = mCurrentIndex - 1;

        while (true)
        {
            if (TestFlag(1))
            {
                if (index < 0)
                {
                    index = mNumItemsAdded - 1;
                }
            }
            else if (index < 0)
            {
                return RES_NOT_CHANGED;
            }

            if (TestFlag(2) && mMenuItems[index].IsDisabled())
            {
                index--;
                continue;
            }

            SetItem(index);
            return RES_OK;
        }
    }

    int GetActiveItemIndex() const
    {
        return mCurrentIndex;
    }

    void SetActiveItemIndex(int index)
    {
        mCurrentIndex = index;
    }

    int GetNumItemsAdded() const
    {
        return mNumItemsAdded;
    }

    MenuItem<T>* GetMenuItem(int index = -1)
    {
        if (index == -1)
        {
            return &mMenuItems[mCurrentIndex];
        }
        return &mMenuItems[index];
    }

    bool TestFlag(int flag) const
    {
        return (mFlags & flag) != 0;
    }

    void SetFlag(int flag)
    {
        mFlags = flag;
    }

private:
    /* 0x004 */ MenuItem<T> mMenuItems[16];
    /* 0x204 */ int mCurrentIndex;
    /* 0x208 */ int mNumItemsAdded;
    /* 0x20C */ bool mWrapList;
    /* 0x210 */ int mFlags;
}; // total size: 0x214
// #pragma pop

#endif // _FESLIDEMENU_H_
