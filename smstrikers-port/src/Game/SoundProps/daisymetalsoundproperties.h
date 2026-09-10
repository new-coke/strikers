#ifndef _DAISYMETALSOUNDPROPERTIES_H_
#define _DAISYMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DAISYMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    DAISYMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DAISYMETALSoundPropAccessor gDAISYMETALSoundPropAccessor;
extern SoundPropAccessor* gpDAISYMETALSoundPropAccessor;

#endif // _DAISYMETALSOUNDPROPERTIES_H_
