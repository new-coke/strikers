#ifndef _HAMBROSWOODSOUNDPROPERTIES_H_
#define _HAMBROSWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class HAMBROSWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    HAMBROSWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern HAMBROSWOODSoundPropAccessor gHAMBROSWOODSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSWOODSoundPropAccessor;

#endif // _HAMBROSWOODSOUNDPROPERTIES_H_
