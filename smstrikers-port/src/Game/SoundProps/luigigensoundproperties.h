#ifndef _LUIGIGENSOUNDPROPERTIES_H_
#define _LUIGIGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class LUIGISoundPropAccessor : public SoundPropAccessor
{
public:
    LUIGISoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern LUIGISoundPropAccessor gLUIGISoundPropAccessor;
extern SoundPropAccessor* gpLUIGISoundPropAccessor;

#endif // _LUIGIGENSOUNDPROPERTIES_H_
