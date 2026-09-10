#ifndef _FEMAPMENU_H_
#define _FEMAPMENU_H_

#include "Game/FE/tlInstance.h"

struct ItemEntry
{
    /* 0x00 */ int ItemID;
    /* 0x04 */ TLInstance* Icon;
    /* 0x08 */ bool Active;
    /* 0x09 */ char pad[3];
    /* 0x0C */ int Left;
    /* 0x10 */ int Right;
    /* 0x14 */ int Up;
    /* 0x18 */ int Down;
}; // total size: 0x1C

class FEMapMenu
{
public:
    FEMapMenu(TLInstance* pHighlighter, bool makeSounds);
    ~FEMapMenu();
    void AddItem(int itemID, TLInstance* pIcon, int leftID, int rightID, int upID, int downID, bool active);
    void UpdateAllItems();
    void Update(float dt);
    bool MoveRight(bool playSound);
    bool MoveLeft(bool playSound);
    bool MoveDown(bool playSound);
    bool MoveUp(bool playSound);
    int GetSelectedItem() const;
    bool IsSelectedItemActive() const;
    bool IsItemActive(int itemID) const;
    void SetSelectedItem(int itemID);
    void UpdateHighlighter();
    void SetItemActive(int itemID, bool active);
    void SetAllItemsActive();
    void ChangeItem(int itemID, TLInstance* pInstance);

    inline void SetCurrentSelectByID(int itemID);
    inline bool TryMoveRandom(int itemID, int soundIndex);

    /* 0x000 */ TLInstance* m_highlighter;
    /* 0x004 */ ItemEntry m_items[32];
    /* 0x384 */ signed char m_numItems;
    /* 0x385 */ signed char m_currentSelectIndex;
    /* 0x386 */ signed char m_numCyclesRemaining;
    /* 0x388 */ float m_randDeltaTime;
    /* 0x38C */ bool m_isRandomizing;
    /* 0x38D */ bool m_hasRandomizeJustFinished;
    /* 0x390 */ float m_disabledMultColor;
    /* 0x394 */ bool m_makeSounds;
}; // total size: 0x398

#endif // _FEMAPMENU_H_
