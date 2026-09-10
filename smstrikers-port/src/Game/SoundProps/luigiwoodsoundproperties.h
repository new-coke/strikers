#ifndef _LUIGIWOODSOUNDPROPERTIES_H_
#define _LUIGIWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class LUIGIWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    LUIGIWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern LUIGIWOODSoundPropAccessor gLUIGIWOODSoundPropAccessor;
extern SoundPropAccessor* gpLUIGIWOODSoundPropAccessor;

#endif // _LUIGIWOODSOUNDPROPERTIES_H_
