#ifndef _FONTMANAGER_H_
#define _FONTMANAGER_H_

#include "NL/nlSingleton.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlFont.h"

class FontManager : public nlSingleton<FontManager>
{
public:
    FontManager();
    virtual ~FontManager();
    nlFont* GetFontByHashID(unsigned long hashID);
    bool LoadFont(const char* bundlePath, const char* fontName, const char* fontFileName);

    /* 0x4 */ nlDLListSlotPool<nlFont*> m_fonts;
};

// class DLListContainerBase<nlFont*, BasicSlotPool<DLListEntry<nlFont*>>>
// {
// public:
// };

// class nlWalkDLRing<DLListEntry<nlFont*>, DLListContainerBase<nlFont*, BasicSlotPool<DLListEntry<nlFont*>>>>(DLListEntry<nlFont*>*, DLListContainerBase<nlFont*, BasicSlotPool<DLListEntry<nlFont*>>>*, void (DLListContainerBase<nlFont*, BasicSlotPool<DLListEntry<nlFont*>>>
// {
// public:
// };

// class nlWalkRing<DLListEntry<nlFont*>, DLListContainerBase<nlFont*, BasicSlotPool<DLListEntry<nlFont*>>>>(DLListEntry<nlFont*>*, DLListContainerBase<nlFont*, BasicSlotPool<DLListEntry<nlFont*>>>*, void (DLListContainerBase<nlFont*, BasicSlotPool<DLListEntry<nlFont*>>>
// {
// public:
// };

#endif // _FONTMANAGER_H_
