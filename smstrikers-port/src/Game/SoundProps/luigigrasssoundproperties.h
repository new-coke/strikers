#ifndef _LUIGIGRASSSOUNDPROPERTIES_H_
#define _LUIGIGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class LUIGIGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    LUIGIGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern LUIGIGRASSSoundPropAccessor gLUIGIGRASSSoundPropAccessor;
extern SoundPropAccessor* gpLUIGIGRASSSoundPropAccessor;

#endif // _LUIGIGRASSSOUNDPROPERTIES_H_
