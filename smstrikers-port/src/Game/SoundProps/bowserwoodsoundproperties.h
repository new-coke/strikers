#ifndef _BOWSERWOODSOUNDPROPERTIES_H_
#define _BOWSERWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BOWSERWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    BOWSERWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BOWSERWOODSoundPropAccessor gBOWSERWOODSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERWOODSoundPropAccessor;

#endif // _BOWSERWOODSOUNDPROPERTIES_H_
