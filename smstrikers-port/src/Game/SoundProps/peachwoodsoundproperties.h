#ifndef _PEACHWOODSOUNDPROPERTIES_H_
#define _PEACHWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class PEACHWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    PEACHWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern PEACHWOODSoundPropAccessor gPEACHWOODSoundPropAccessor;
extern SoundPropAccessor* gpPEACHWOODSoundPropAccessor;

#endif // _PEACHWOODSOUNDPROPERTIES_H_
