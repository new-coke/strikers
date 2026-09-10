#ifndef _MARIOMETALSOUNDPROPERTIES_H_
#define _MARIOMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class MARIOMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    MARIOMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern MARIOMETALSoundPropAccessor gMARIOMETALSoundPropAccessor;
extern SoundPropAccessor* gpMARIOMETALSoundPropAccessor;

#endif // _MARIOMETALSOUNDPROPERTIES_H_
