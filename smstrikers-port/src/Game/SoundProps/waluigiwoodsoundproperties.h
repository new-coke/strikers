#ifndef _WALUIGIWOODSOUNDPROPERTIES_H_
#define _WALUIGIWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WALUIGIWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    WALUIGIWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WALUIGIWOODSoundPropAccessor gWALUIGIWOODSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIWOODSoundPropAccessor;

#endif // _WALUIGIWOODSOUNDPROPERTIES_H_
