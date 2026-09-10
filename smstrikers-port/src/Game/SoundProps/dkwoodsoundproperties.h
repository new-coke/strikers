#ifndef _DKWOODSOUNDPROPERTIES_H_
#define _DKWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DKWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    DKWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DKWOODSoundPropAccessor gDKWOODSoundPropAccessor;
extern SoundPropAccessor* gpDKWOODSoundPropAccessor;

#endif // _DKWOODSOUNDPROPERTIES_H_
