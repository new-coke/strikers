#ifndef _SUPERWOODSOUNDPROPERTIES_H_
#define _SUPERWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class SUPERWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    SUPERWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern SUPERWOODSoundPropAccessor gSUPERWOODSoundPropAccessor;
extern SoundPropAccessor* gpSUPERWOODSoundPropAccessor;

#endif // _SUPERWOODSOUNDPROPERTIES_H_
